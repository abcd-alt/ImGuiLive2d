/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <GLES2/gl2.h>

/**
 * @brief  Utility class for loading textures from Android assets or memory.
 *
 * Uses stb_image for PNG/JPEG decoding and creates OpenGL ES textures.
 */
class TextureLoader
{
public:
    /**
     * @brief  Load a texture from an asset file.
     *
     * The file path is relative to the assets root directory.
     * Supports PNG and JPEG formats via stb_image.
     *
     * @param[in]  assetPath  Path to the image file within the assets directory.
     *
     * @return  OpenGL texture ID, or 0 on failure.
     */
    static GLuint LoadTextureFromAssets(const char* assetPath);

    /**
     * @brief  Load a texture from an in-memory image buffer.
     *
     * @param[in]  data  Pointer to the encoded image data (PNG/JPEG).
     * @param[in]  size  Size of the data in bytes.
     *
     * @return  OpenGL texture ID, or 0 on failure.
     */
    static GLuint LoadTextureFromMemory(const unsigned char* data, int size);

    /**
     * @brief  Release a previously loaded texture.
     *
     * @param[in]  texId  OpenGL texture ID to release.
     */
    static void ReleaseTexture(GLuint texId);
};
