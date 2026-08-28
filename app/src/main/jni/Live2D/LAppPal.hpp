/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <CubismFramework.hpp>
#include <android/asset_manager.h>

struct android_app;

/**
 * @brief  Platform abstraction layer for the Android native app.
 *
 * Provides file loading via AAssetManager, time management, and logging
 * through Android's log system. Adapted for android_native_app_glue.
 */
class LAppPal
{
public:
    /**
     * @brief  Store a reference to the android_app struct.
     *
     * Must be called once at startup before any other LAppPal function.
     *
     * @param[in]  app  Pointer to the android_app struct.
     */
    static void SetAndroidApp(struct android_app* app);

    /**
     * @brief  Get the stored android_app pointer.
     *
     * @return  Pointer to the android_app struct, or nullptr if not set.
     */
    static struct android_app* GetAndroidApp();

    /**
     * @brief  Load a file from assets as a byte buffer.
     *
     * Resolution order:
     *   1. Try loading from embedded model data (in-memory arrays).
     *   2. Try loading from embedded shader arrays.
     *   3. Fallback: try loading from Android assets.
     *
     * @param[in]   filePath  Relative file path.
     * @param[out]  outSize   Number of bytes loaded.
     *
     * @return  Pointer to the allocated buffer, or nullptr on failure.
     *          Caller must free with ReleaseBytes().
     */
    static Csm::csmByte* LoadFileAsBytes(const Csm::csmChar* filePath, Csm::csmSizeInt* outSize);

    /**
     * @brief  Release a byte buffer allocated by LoadFileAsBytes.
     *
     * @param[in]  byteData  Pointer to the buffer to free.
     */
    static void ReleaseBytes(Csm::csmByte* byteData);

    /**
     * @brief  Get the elapsed time since the last call to UpdateTime().
     *
     * @return  Delta time in seconds.
     */
    static Csm::csmFloat32 GetDeltaTime();

    /**
     * @brief  Update the internal time tracking.
     *
     * Call this once per frame to keep GetDeltaTime() accurate.
     */
    static void UpdateTime();

    /**
     * @brief  Get the current system time in seconds (monotonic clock).
     *
     * @return  System time in seconds.
     */
    static Csm::csmFloat32 GetSystemTime();

    /**
     * @brief  Print a log message via Android's log system AND to the crash log file.
     *
     * @param[in]  format  printf-style format string.
     * @param[in]  ...     Variable arguments.
     */
    static void PrintLogLn(const Csm::csmChar* format, ...);

    /**
     * @brief  Initialize the file-based crash log.
     *
     * Opens the log file at the specified path for writing.
     * All subsequent PrintLogLn calls will also write to this file.
     *
     * @param[in]  logFilePath  Full path to the log file (e.g., "/storage/emulated/0/日志输出.log").
     */
    static void InitFileLog(const char* logFilePath);

    /**
     * @brief  Write a message directly to the file log (with timestamp).
     *
     * This bypasses Android logcat and only writes to the file.
     * Useful for critical crash-point markers.
     *
     * @param[in]  format  printf-style format string.
     * @param[in]  ...     Variable arguments.
     */
    static void FileLog(const char* format, ...);

    /**
     * @brief  Flush the file log to disk immediately.
     *
     * Call this after critical operations to ensure logs are written
     * even if the app crashes immediately after.
     */
    static void FlushFileLog();
};
