/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "LAppPal.hpp"
#include "LAppDefine.hpp"
#include "Shader/Live2DShaders.h"
#include "Model/Live2DModelData_Hiyori.h"

#include <android/log.h>
#include <Helper/android_native_app_glue.h>
#include <android/asset_manager.h>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <unistd.h>
#include <string>

// Tag for Android log output.
static const char* const kLogTag = "Live2D";

// Stored android_app pointer.
static struct android_app* s_App = nullptr;

// Time tracking for delta time calculation.
static struct timespec s_LastTime;
static Csm::csmBool s_TimeInitialized = false;

// File-based crash log.
static FILE* s_FileLog = nullptr;
static char s_FileLogPath[512] = {0};

// --------------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------------

/**
 * @brief  Get a timestamp string for logging.
 */
static const char* GetTimestamp()
{
    static char buf[64];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03ld",
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
             ts.tv_nsec / 1000000);
    return buf;
}

/**
 * @brief  Write a formatted message to the file log.
 */
static void WriteToFileLog(const char* format, va_list args)
{
    if (!s_FileLog)
    {
        return;
    }

    // Write timestamp prefix.
    fprintf(s_FileLog, "[%s] ", GetTimestamp());

    // Write the formatted message.
    vfprintf(s_FileLog, format, args);
    fprintf(s_FileLog, "\n");
    fflush(s_FileLog);
}

static Csm::csmByte* TryLoadFromAssets(AAssetManager* assetManager,
                                        const char* assetPath,
                                        Csm::csmSizeInt* outSize)
{
    AAsset* asset = AAssetManager_open(assetManager, assetPath, AASSET_MODE_BUFFER);
    if (!asset)
    {
        return nullptr;
    }

    const off_t length = AAsset_getLength(asset);
    if (length <= 0)
    {
        AAsset_close(asset);
        return nullptr;
    }

    Csm::csmByte* buffer = static_cast<Csm::csmByte*>(malloc(length));
    if (!buffer)
    {
        AAsset_close(asset);
        return nullptr;
    }

    const off_t bytesRead = AAsset_read(asset, buffer, length);
    AAsset_close(asset);

    if (bytesRead != length)
    {
        free(buffer);
        return nullptr;
    }

    *outSize = static_cast<Csm::csmSizeInt>(length);
    return buffer;
}

static float GetMonotonicSeconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<float>(ts.tv_sec) + static_cast<float>(ts.tv_nsec) * 1.0e-9f;
}

// --------------------------------------------------------------------------
// LAppPal implementation
// --------------------------------------------------------------------------

void LAppPal::InitFileLog(const char* logFilePath)
{
    if (!logFilePath)
    {
        return;
    }

    // Close existing log if any.
    if (s_FileLog)
    {
        fclose(s_FileLog);
        s_FileLog = nullptr;
    }

    strncpy(s_FileLogPath, logFilePath, sizeof(s_FileLogPath) - 1);

    // Open in write mode (truncate existing).
    s_FileLog = fopen(logFilePath, "w");
    if (s_FileLog)
    {
        fprintf(s_FileLog, "========================================\n");
        fprintf(s_FileLog, "  Live2D ImGui Integration - Crash Log\n");
        fprintf(s_FileLog, "  Started: %s\n", GetTimestamp());
        fprintf(s_FileLog, "========================================\n");
        fflush(s_FileLog);

        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "File log initialized: %s", logFilePath);
    }
    else
    {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "FAILED to open file log: %s (errno=%d)", logFilePath, errno);
    }
}

void LAppPal::FileLog(const char* format, ...)
{
    if (!s_FileLog)
    {
        return;
    }

    va_list args;
    va_start(args, format);
    WriteToFileLog(format, args);
    va_end(args);
}

void LAppPal::FlushFileLog()
{
    if (s_FileLog)
    {
        fflush(s_FileLog);
        // Also try fsync to force write to disk.
        fsync(fileno(s_FileLog));
    }
}

void LAppPal::SetAndroidApp(struct android_app* app)
{
    s_App = app;

    // Initialize time tracking on first set.
    if (!s_TimeInitialized)
    {
        clock_gettime(CLOCK_MONOTONIC, &s_LastTime);
        s_TimeInitialized = true;
    }
}

struct android_app* LAppPal::GetAndroidApp()
{
    return s_App;
}

Csm::csmByte* LAppPal::LoadFileAsBytes(const Csm::csmChar* filePath, Csm::csmSizeInt* outSize)
{
    if (!filePath)
    {
        *outSize = 0;
        return nullptr;
    }

    FileLog("[LoadFileAsBytes] Requesting: %s", filePath);

    Csm::csmByte* result = nullptr;

    // 1. Try loading from embedded model data (in-memory, fastest).
    {
        unsigned int embeddedSize = 0;
        const unsigned char* embeddedData = FindHiyoriEmbeddedFile(filePath, &embeddedSize);
        if (embeddedData && embeddedSize > 0)
        {
            result = static_cast<Csm::csmByte*>(malloc(embeddedSize));
            if (result)
            {
                memcpy(result, embeddedData, embeddedSize);
                *outSize = static_cast<Csm::csmSizeInt>(embeddedSize);
                FileLog("[LoadFileAsBytes] OK (embedded memory): %s (%u bytes)", filePath, embeddedSize);
                return result;
            }
            else
            {
                FileLog("[LoadFileAsBytes] FAIL (malloc failed for %u bytes): %s", embeddedSize, filePath);
            }
        }
    }

    // 2. Try loading from embedded shader arrays.
    {
        const char* shaderSource = FindShaderByName(filePath);
        if (shaderSource)
        {
            const size_t len = strlen(shaderSource);
            result = static_cast<Csm::csmByte*>(malloc(len));
            if (result)
            {
                memcpy(result, shaderSource, len);
                *outSize = static_cast<Csm::csmSizeInt>(len);
                FileLog("[LoadFileAsBytes] OK (embedded shader): %s (%d bytes)", filePath, *outSize);
                return result;
            }
        }
    }

    // 3. Fallback: try loading from Android assets.
    if (s_App && s_App->activity && s_App->activity->assetManager)
    {
        AAssetManager* assetManager = s_App->activity->assetManager;

        // 3a. Try "live2d/<filePath>".
        {
            std::string fullPath = std::string(LAppDefine::ResourcesPath) + filePath;
            result = TryLoadFromAssets(assetManager, fullPath.c_str(), outSize);
            if (result)
            {
                FileLog("[LoadFileAsBytes] OK (assets): %s (%d bytes)", fullPath.c_str(), *outSize);
                return result;
            }
        }

        // 3b. Try as-is.
        {
            result = TryLoadFromAssets(assetManager, filePath, outSize);
            if (result)
            {
                FileLog("[LoadFileAsBytes] OK (assets absolute): %s (%d bytes)", filePath, *outSize);
                return result;
            }
        }
    }
    else
    {
        FileLog("[LoadFileAsBytes] SKIP assets (s_App or assetManager is null)");
    }

    // 4. All attempts failed.
    FileLog("[LoadFileAsBytes] FAIL (all methods): %s", filePath);
    PrintLogLn("[LAppPal] WARNING: Could not load file: %s", filePath);
    *outSize = 0;
    return nullptr;
}

void LAppPal::ReleaseBytes(Csm::csmByte* byteData)
{
    if (byteData)
    {
        free(byteData);
    }
}

Csm::csmFloat32 LAppPal::GetDeltaTime()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    const float nowSec = static_cast<float>(now.tv_sec) + static_cast<float>(now.tv_nsec) * 1.0e-9f;
    const float lastSec = static_cast<float>(s_LastTime.tv_sec) + static_cast<float>(s_LastTime.tv_nsec) * 1.0e-9f;

    float delta = nowSec - lastSec;

    if (delta > 0.1f)
    {
        delta = 0.016f;
    }
    if (delta < 0.0f)
    {
        delta = 0.0f;
    }

    return delta;
}

void LAppPal::UpdateTime()
{
    clock_gettime(CLOCK_MONOTONIC, &s_LastTime);
}

Csm::csmFloat32 LAppPal::GetSystemTime()
{
    return GetMonotonicSeconds();
}

void LAppPal::PrintLogLn(const Csm::csmChar* format, ...)
{
    // Print to Android logcat.
    va_list args1;
    va_start(args1, format);
    __android_log_vprint(ANDROID_LOG_INFO, kLogTag, format, args1);
    va_end(args1);

    // Also write to file log.
    if (s_FileLog)
    {
        va_list args2;
        va_start(args2, format);
        WriteToFileLog(format, args2);
        va_end(args2);
    }
}
