#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
convert_model_to_arrays.py
==========================
把 Live2D 模型目录下的所有文件（.moc3 / .json / .png / .wav / .motion3.json 等）
编译成 C 数组，生成一个 .h 头文件，供 Android NDK 项目从内存直接加载，
无需把文件放进 assets 目录。

用法:
    python3 convert_model_to_arrays.py <模型根目录> <模型名> [输出头文件路径]

示例 (模型根目录 = 包含模型文件夹的父目录):
    python3 convert_model_to_arrays.py ./Resources Haru \
        ../app/src/main/jni/Live2D/Model/Live2DModelData_Haru.h

    ./Resources/
    └── Haru/
        ├── Haru.model3.json
        ├── Haru.moc3
        ├── Haru.2048/texture_00.png
        ├── expressions/F01.exp3.json
        └── motions/haru_g_idle.motion3.json

    生成的查找路径会带 "Haru/" 前缀，例如 "Haru/Haru.model3.json"，
    与 Live2D 运行时使用的路径一致。

生成的头文件包含:
    - 每个文件一个 static const unsigned char 数组
    - 每个文件一个 _size 常量
    - 查找表 Live2DEmbeddedFile g_<model>_embeddedFiles[]
    - 查找函数 Find<Model>EmbeddedFile(path, &size)

注意:
    - 生成的数组是 static 的，请确保头文件只被一个 .cpp 包含（否则每个编译单元
      都会复制一份，导致 .so 体积膨胀）。
    - 若模型文件很大（如 2048 纹理），编译时间会略长，属正常现象。
"""

import os
import re
import sys


def sanitize_identifier(name: str) -> str:
    """把任意路径/文件名转成合法的 C 标识符片段。

    规则: 非字母数字字符 -> 下划线，连续下划线合并，首尾下划线去除。
    例如: "Haru/Haru.2048/texture_00.png" -> "Haru_Haru_2048_texture_00_png"
    """
    s = re.sub(r'[^A-Za-z0-9]+', '_', name)
    s = re.sub(r'_+', '_', s)
    s = s.strip('_')
    return s


def bytes_to_c_array(data: bytes, indent: str = '    ') -> str:
    """把字节串转成 16 字节一行的 C 数组初始化文本。"""
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        hexes = ', '.join('0x%02x' % b for b in chunk)
        lines.append(indent + hexes + ',')
    return '\n'.join(lines)


def collect_files(model_dir: str, exclude: str = None) -> list:
    """递归收集模型目录下所有文件，返回按路径排序的 (相对路径, 绝对路径) 列表。

    exclude: 需要排除的绝对路径（通常是输出头文件本身，防止被递归收集）。
    """
    files = []
    for root, _dirs, names in os.walk(model_dir):
        for name in names:
            full = os.path.join(root, name)
            if exclude and os.path.abspath(full) == os.path.abspath(exclude):
                continue
            rel = os.path.relpath(full, model_dir).replace(os.sep, '/')
            files.append((rel, full))
    files.sort(key=lambda x: x[0])
    return files


def generate_header(model_name: str, files: list) -> str:
    """生成完整的头文件内容。"""
    model_id = sanitize_identifier(model_name)          # 例如 "Haru" -> "Haru"
    model_id_lower = model_id.lower()                   # 例如 "Haru" -> "haru"
    find_func = 'Find%sEmbeddedFile' % model_id         # 例如 "FindHaruEmbeddedFile"
    table_name = 'g_%s_embeddedFiles' % model_id_lower  # 例如 "g_haru_embeddedFiles"
    count_name = 'g_%s_embeddedFileCount' % model_id_lower

    out = []
    out.append('#pragma once')
    out.append('')
    out.append('/**')
    out.append(' * Auto-generated Live2D model data header.')
    out.append(' *')
    out.append(" * All model files for '%s' are embedded as C arrays" % model_name)
    out.append(' * for in-memory loading. No assets directory required at runtime.')
    out.append(' *')
    out.append(' * Do not edit manually -- regenerate with convert_model_to_arrays.py.')
    out.append(' */')
    out.append('')
    out.append('#include <cstring>')
    out.append('#include <cstddef>')
    out.append('')

    entries = []

    for rel, full in files:
        with open(full, 'rb') as f:
            data = f.read()

        # 变量名: g_<model>_<sanitized rel path>，例如 g_haru_Haru_moc3
        # 若相对路径以 "<模型名>/" 开头，则去掉该前缀，避免变量名重复模型名
        # （例如 "Haru/Haru.moc3" -> "Haru.moc3" -> g_haru_Haru_moc3）。
        rel_for_var = rel
        prefix = model_name + '/'
        if rel_for_var.startswith(prefix):
            rel_for_var = rel_for_var[len(prefix):]
        var_name = 'g_%s_%s' % (model_id_lower, sanitize_identifier(rel_for_var))
        size_name = var_name + '_size'

        out.append('// Source: %s' % rel)
        out.append('static const unsigned char %s[] = {' % var_name)
        out.append(bytes_to_c_array(data))
        out.append('};')
        out.append('static const unsigned int %s = %d;' % (size_name, len(data)))
        out.append('')

        entries.append((rel, var_name, size_name))

    # 查找表
    out.append('// ============================================================')
    out.append('// Lookup Table')
    out.append('// ============================================================')
    out.append('')
    out.append('struct Live2DEmbeddedFile {')
    out.append('    const char* path;           // Lookup path (e.g., "%s")' % (entries[0][0] if entries else 'Haru/Haru.model3.json'))
    out.append('    const unsigned char* data;  // Pointer to embedded data')
    out.append('    unsigned int size;           // Size in bytes')
    out.append('};')
    out.append('')
    out.append('static const Live2DEmbeddedFile %s[] = {' % table_name)
    for rel, var_name, size_name in entries:
        out.append('    { "%s", %s, %s },' % (rel, var_name, size_name))
    out.append('};')
    out.append('')
    out.append('static const int %s = %d;' % (count_name, len(entries)))
    out.append('')

    # 查找函数
    out.append('/**')
    out.append(' * @brief  Find an embedded file by path.')
    out.append(' *')
    out.append(' * @param[in]  path     The lookup path (e.g., "%s").' % (entries[0][0] if entries else 'Haru/Haru.model3.json'))
    out.append(' * @param[out] outSize  Receives the file size if found.')
    out.append(' *')
    out.append(' * @return  Pointer to the embedded data, or nullptr if not found.')
    out.append(' */')
    out.append('static const unsigned char* %s(const char* path, unsigned int* outSize)' % find_func)
    out.append('{')
    out.append('    if (!path) return nullptr;')
    out.append('')
    out.append('    // Skip "live2d/" prefix if present.')
    out.append('    const char* searchPath = path;')
    out.append('    const char* prefix = "live2d/";')
    out.append('    if (strncmp(path, prefix, 7) == 0)')
    out.append('    {')
    out.append('        searchPath = path + 7;')
    out.append('    }')
    out.append('')
    out.append('    for (int i = 0; i < %s; ++i)' % count_name)
    out.append('    {')
    out.append('        if (strcmp(searchPath, %s[i].path) == 0)' % table_name)
    out.append('        {')
    out.append('            if (outSize) *outSize = %s[i].size;' % table_name)
    out.append('            return %s[i].data;' % table_name)
    out.append('        }')
    out.append('    }')
    out.append('    return nullptr;')
    out.append('}')
    out.append('')

    return '\n'.join(out)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    model_dir = sys.argv[1]
    model_name = sys.argv[2]
    out_path = sys.argv[3] if len(sys.argv) > 3 else 'Live2DModelData_%s.h' % sanitize_identifier(model_name)

    if not os.path.isdir(model_dir):
        print('ERROR: model directory not found: %s' % model_dir)
        sys.exit(1)

    files = collect_files(model_dir, exclude=out_path)
    if not files:
        print('ERROR: no files found in: %s' % model_dir)
        sys.exit(1)

    header = generate_header(model_name, files)

    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(header)

    total = sum(os.path.getsize(full) for _rel, full in files)
    print('Done!')
    print('  Model name : %s' % model_name)
    print('  Files      : %d' % len(files))
    print('  Total size : %d bytes (%.2f MB)' % (total, total / 1024.0 / 1024.0))
    print('  Output     : %s' % out_path)
    print('')
    print('Next steps:')
    print('  1. Include this header in your LAppPal.cpp (or wherever you load files).')
    print('  2. Add a lookup call in LAppPal::LoadFileAsBytes(), e.g.:')
    print('       const unsigned char* d = Find%sEmbeddedFile(filePath, &size);' % sanitize_identifier(model_name))
    print('  3. Rebuild with ndk-build.')


if __name__ == '__main__':
    main()
