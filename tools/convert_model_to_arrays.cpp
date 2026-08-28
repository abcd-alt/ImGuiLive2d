// ============================================================
// convert_model_to_arrays.cpp
// ============================================================
// 把 Live2D 模型目录下的所有文件（.moc3 / .json / .png / .wav /
// .motion3.json 等）编译成 C 数组，生成一个 .h 头文件，供 Android
// NDK 项目从内存直接加载，无需把文件放进 assets 目录。
//
// 这是 convert_model_to_arrays.py 的 C++ 交互式版本。
//
// 编译 (Linux/macOS):
//     g++ -std=c++17 -O2 -o convert_model_to_arrays convert_model_to_arrays.cpp
//
// 编译 (Windows, 需要 VS 或 MinGW):
//     g++ -std=c++17 -O2 -o convert_model_to_arrays.exe convert_model_to_arrays.cpp
//
// 运行:
//     ./convert_model_to_arrays
//     程序会交互式询问: 模型根目录 / 模型名 / 输出头文件路径
// ============================================================

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

// ------------------------------------------------------------------
// 工具函数
// ------------------------------------------------------------------

// 把任意路径/文件名转成合法的 C 标识符片段。
// 例如: "Haru/Haru.2048/texture_00.png" -> "Haru_Haru_2048_texture_00_png"
static std::string SanitizeIdentifier(const std::string& name)
{
    std::string s;
    bool lastUnderscore = false;
    for (char c : name)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            s += c;
            lastUnderscore = false;
        }
        else
        {
            if (!lastUnderscore && !s.empty())
            {
                s += '_';
                lastUnderscore = true;
            }
        }
    }
    // 去掉末尾下划线
    while (!s.empty() && s.back() == '_')
        s.pop_back();
    return s;
}

// 把字节串转成 16 字节一行的 C 数组初始化文本。
static std::string BytesToCArray(const std::vector<unsigned char>& data)
{
    std::ostringstream oss;
    const size_t perLine = 16;
    for (size_t i = 0; i < data.size(); i += perLine)
    {
        oss << "    ";
        size_t end = std::min(i + perLine, data.size());
        for (size_t j = i; j < end; ++j)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "0x%02x", data[j]);
            oss << buf;
            if (j + 1 < end)  // 行内元素之间用 ", " 分隔
                oss << ", ";
        }
        oss << ",\n";  // 每行末尾一个逗号 (trailing comma, C 合法)
    }
    return oss.str();
}

// 读取整个文件到字节向量。失败返回 false。
static bool ReadFileBytes(const std::string& path, std::vector<unsigned char>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    if (len < 0)
        return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(len));
    if (len > 0)
        f.read(reinterpret_cast<char*>(out.data()), len);
    return f.good() || f.eof();
}

// ------------------------------------------------------------------
// 目录遍历
// ------------------------------------------------------------------

struct FileEntry
{
    std::string relPath;  // 相对模型根目录的路径，使用 '/' 分隔
    std::string absPath;  // 绝对路径
};

// 递归收集目录下所有文件。excludeAbs: 需要排除的绝对路径（输出头文件本身）。
static void CollectFilesRecursive(const std::string& dir,
                                  const std::string& baseDir,
                                  const std::string& excludeAbs,
                                  std::vector<FileEntry>& out)
{
#if defined(_WIN32)
    // Windows 使用 _findfirst / _findnext
    std::string pattern = dir + "/*";
    struct _finddata_t fd;
    intptr_t handle = _findfirst(pattern.c_str(), &fd);
    if (handle == -1)
        return;
    do
    {
        std::string name = fd.name;
        if (name == "." || name == "..")
            continue;
        std::string full = dir + "/" + name;
        if (fd.attrib & _A_SUBDIR)
        {
            CollectFilesRecursive(full, baseDir, excludeAbs, out);
        }
        else
        {
            if (!excludeAbs.empty() && full == excludeAbs)
                continue;
            std::string rel = full.substr(baseDir.size() + 1);
            std::replace(rel.begin(), rel.end(), '\\', '/');
            out.push_back({rel, full});
        }
    } while (_findnext(handle, &fd) == 0);
    _findclose(handle);
#else
    // POSIX 使用 opendir / readdir
    DIR* d = opendir(dir.c_str());
    if (!d)
        return;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr)
    {
        std::string name = ent->d_name;
        if (name == "." || name == "..")
            continue;
        std::string full = dir + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            CollectFilesRecursive(full, baseDir, excludeAbs, out);
        }
        else if (S_ISREG(st.st_mode))
        {
            if (!excludeAbs.empty() && full == excludeAbs)
                continue;
            std::string rel = full.substr(baseDir.size() + 1);
            out.push_back({rel, full});
        }
    }
    closedir(d);
#endif
}

// ------------------------------------------------------------------
// 头文件生成
// ------------------------------------------------------------------

static std::string GenerateHeader(const std::string& modelName,
                                  const std::vector<FileEntry>& files)
{
    std::string modelId = SanitizeIdentifier(modelName);
    std::string modelIdLower = modelId;
    std::transform(modelIdLower.begin(), modelIdLower.end(), modelIdLower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string findFunc = "Find" + modelId + "EmbeddedFile";
    std::string tableName = "g_" + modelIdLower + "_embeddedFiles";
    std::string countName = "g_" + modelIdLower + "_embeddedFileCount";

    std::ostringstream out;
    out << "#pragma once\n\n";
    out << "/**\n";
    out << " * Auto-generated Live2D model data header.\n";
    out << " *\n";
    out << " * All model files for '" << modelName << "' are embedded as C arrays\n";
    out << " * for in-memory loading. No assets directory required at runtime.\n";
    out << " *\n";
    out << " * Do not edit manually -- regenerate with convert_model_to_arrays.cpp.\n";
    out << " */\n\n";
    out << "#include <cstring>\n";
    out << "#include <cstddef>\n\n";

    std::vector<std::string> entryLines;
    entryLines.reserve(files.size());

    for (const auto& fe : files)
    {
        std::vector<unsigned char> data;
        if (!ReadFileBytes(fe.absPath, data))
        {
            std::cerr << "  [WARN] 无法读取文件，跳过: " << fe.relPath << "\n";
            continue;
        }

        // 变量名: g_<model>_<sanitized rel path>，例如 g_haru_Haru_moc3
        // 若相对路径以 "<模型名>/" 开头，则去掉该前缀，避免变量名重复模型名。
        std::string relForVar = fe.relPath;
        std::string prefix = modelName + "/";
        if (relForVar.compare(0, prefix.size(), prefix) == 0)
            relForVar = relForVar.substr(prefix.size());

        std::string varName = "g_" + modelIdLower + "_" + SanitizeIdentifier(relForVar);
        std::string sizeName = varName + "_size";

        out << "// Source: " << fe.relPath << "\n";
        out << "static const unsigned char " << varName << "[] = {\n";
        out << BytesToCArray(data);
        out << "};\n";
        out << "static const unsigned int " << sizeName << " = " << data.size() << ";\n\n";

        entryLines.push_back("    { \"" + fe.relPath + "\", " + varName + ", " + sizeName + " },");
    }

    // 查找表
    out << "// ============================================================\n";
    out << "// Lookup Table\n";
    out << "// ============================================================\n\n";
    out << "struct Live2DEmbeddedFile {\n";
    out << "    const char* path;           // Lookup path (e.g., \"" << (files.empty() ? std::string("Haru/Haru.model3.json") : files[0].relPath) << "\")\n";
    out << "    const unsigned char* data;  // Pointer to embedded data\n";
    out << "    unsigned int size;           // Size in bytes\n";
    out << "};\n\n";
    out << "static const Live2DEmbeddedFile " << tableName << "[] = {\n";
    for (const auto& line : entryLines)
        out << line << "\n";
    out << "};\n\n";
    out << "static const int " << countName << " = " << entryLines.size() << ";\n\n";

    // 查找函数
    out << "/**\n";
    out << " * @brief  Find an embedded file by path.\n";
    out << " *\n";
    out << " * @param[in]  path     The lookup path (e.g., \"" << (files.empty() ? std::string("Haru/Haru.model3.json") : files[0].relPath) << "\").\n";
    out << " * @param[out] outSize  Receives the file size if found.\n";
    out << " *\n";
    out << " * @return  Pointer to the embedded data, or nullptr if not found.\n";
    out << " */\n";
    out << "static const unsigned char* " << findFunc << "(const char* path, unsigned int* outSize)\n";
    out << "{\n";
    out << "    if (!path) return nullptr;\n\n";
    out << "    // Skip \"live2d/\" prefix if present.\n";
    out << "    const char* searchPath = path;\n";
    out << "    const char* prefix = \"live2d/\";\n";
    out << "    if (strncmp(path, prefix, 7) == 0)\n";
    out << "    {\n";
    out << "        searchPath = path + 7;\n";
    out << "    }\n\n";
    out << "    for (int i = 0; i < " << countName << "; ++i)\n";
    out << "    {\n";
    out << "        if (strcmp(searchPath, " << tableName << "[i].path) == 0)\n";
    out << "        {\n";
    out << "            if (outSize) *outSize = " << tableName << "[i].size;\n";
    out << "            return " << tableName << "[i].data;\n";
    out << "        }\n";
    out << "    }\n";
    out << "    return nullptr;\n";
    out << "}\n\n";

    return out.str();
}

// ------------------------------------------------------------------
// 交互式输入辅助
// ------------------------------------------------------------------

static std::string Trim(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string Prompt(const std::string& question, const std::string& defaultValue)
{
    std::cout << question;
    if (!defaultValue.empty())
        std::cout << " [" << defaultValue << "]";
    std::cout << ": ";
    std::cout.flush();

    std::string line;
    if (!std::getline(std::cin, line))
        return defaultValue;
    line = Trim(line);
    if (line.empty())
        return defaultValue;
    return line;
}

// ------------------------------------------------------------------
// 主程序
// ------------------------------------------------------------------

int main()
{
    std::cout << "============================================================\n";
    std::cout << "  Live2D Model -> C Array Converter (C++ Interactive)\n";
    std::cout << "============================================================\n\n";

    // 1. 模型根目录
    std::string modelRoot;
    while (modelRoot.empty())
    {
        modelRoot = Prompt("模型根目录 (包含模型文件夹的父目录, 如 ./Resources)", "");
#if defined(_WIN32)
        struct _stat st;
        if (_stat(modelRoot.c_str(), &st) != 0 || !(st.st_mode & _S_IFDIR))
#else
        struct stat st;
        if (stat(modelRoot.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
#endif
        {
            std::cerr << "  [错误] 目录不存在: " << modelRoot << "\n\n";
            modelRoot.clear();
        }
    }

    // 2. 模型名
    std::string modelName = Prompt("模型名 (如 Haru)", "");
    if (modelName.empty())
    {
        std::cerr << "  [错误] 模型名不能为空。\n";
        return 1;
    }

    // 3. 输出头文件路径
    std::string outPath = Prompt("输出头文件路径", "Live2DModelData_" + SanitizeIdentifier(modelName) + ".h");

    // 收集文件
    std::cout << "\n正在扫描目录: " << modelRoot << " ...\n";
    std::vector<FileEntry> files;
    CollectFilesRecursive(modelRoot, modelRoot, outPath, files);
    std::sort(files.begin(), files.end(),
              [](const FileEntry& a, const FileEntry& b) { return a.relPath < b.relPath; });

    if (files.empty())
    {
        std::cerr << "  [错误] 目录下没有找到任何文件: " << modelRoot << "\n";
        return 1;
    }

    std::cout << "找到 " << files.size() << " 个文件:\n";
    for (const auto& fe : files)
        std::cout << "  - " << fe.relPath << "\n";

    // 确认
    std::string confirm = Prompt("\n确认生成头文件?", "y");
    if (confirm != "y" && confirm != "Y" && confirm != "yes" && confirm != "YES")
    {
        std::cout << "已取消。\n";
        return 0;
    }

    // 生成
    std::cout << "\n正在生成头文件...\n";
    std::string header = GenerateHeader(modelName, files);

    std::ofstream f(outPath, std::ios::binary);
    if (!f)
    {
        std::cerr << "  [错误] 无法写入输出文件: " << outPath << "\n";
        return 1;
    }
    f << header;
    f.close();

    // 统计总大小
    unsigned long long total = 0;
    for (const auto& fe : files)
    {
        std::vector<unsigned char> tmp;
        if (ReadFileBytes(fe.absPath, tmp))
            total += tmp.size();
    }

    std::cout << "\n完成!\n";
    std::cout << "  模型名    : " << modelName << "\n";
    std::cout << "  文件数    : " << files.size() << "\n";
    std::cout << "  总大小    : " << total << " 字节 (" << (total / 1024.0 / 1024.0) << " MB)\n";
    std::cout << "  输出      : " << outPath << "\n\n";
    std::cout << "后续步骤:\n";
    std::cout << "  1. 在 LAppPal.cpp (或你的文件加载处) include 这个头文件。\n";
    std::cout << "  2. 在 LAppPal::LoadFileAsBytes() 中调用查找函数, 例如:\n";
    std::cout << "       const unsigned char* d = Find" << SanitizeIdentifier(modelName) << "EmbeddedFile(filePath, &size);\n";
    std::cout << "  3. 用 ndk-build 重新编译。\n";

    return 0;
}
