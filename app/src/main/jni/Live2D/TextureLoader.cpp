/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "TextureLoader.hpp"
#include "LAppPal.hpp"
#include "LAppDefine.hpp"

#include <android/asset_manager.h>
#include <Helper/android_native_app_glue.h>
#include <GLES2/gl2.h>
#include <cstdlib>
#include <cstring>

// stb_image configuration: only include the implementation in this translation unit.
//#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --------------------------------------------------------------------------
// Internal helper: create an OpenGL texture from raw RGBA pixel data.
// --------------------------------------------------------------------------

static GLuint CreateGLTexture(unsigned char* pixels, int width, int height, int channels)
{
    GLuint texId = 0;
    glGenTextures(1, &texId);
    if (texId == 0)
    {
        LAppPal::PrintLogLn("[TextureLoader] ERROR: glGenTextures failed.");
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texId);

    // Set texture parameters.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Determine the GL format based on the number of channels.
    GLenum format;
    switch (channels)
    {
        case 1: format = GL_LUMINANCE;     break;
        case 2: format = GL_LUMINANCE_ALPHA; break;
        case 3: format = GL_RGB;           break;
        case 4: format = GL_RGBA;          break;
        default:
            LAppPal::PrintLogLn("[TextureLoader] ERROR: Unsupported channel count: %d", channels);
            glDeleteTextures(1, &texId);
            return 0;
    }

    // Upload the texture data.
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Unbind.
    glBindTexture(GL_TEXTURE_2D, 0);

    if (LAppDefine::DebugLogEnable)
    {
        LAppPal::PrintLogLn("[TextureLoader] Created texture %u (%dx%d, %d channels)", texId, width, height, channels);
    }

    return texId;
}

// --------------------------------------------------------------------------
// TextureLoader implementation
// --------------------------------------------------------------------------

GLuint TextureLoader::LoadTextureFromAssets(const char* assetPath)
{
    if (!assetPath || strlen(assetPath) == 0)
    {
        return 0;
    }

    // Load the file via LAppPal.
    Csm::csmSizeInt fileSize = 0;
    Csm::csmByte* fileData = LAppPal::LoadFileAsBytes(assetPath, &fileSize);
    if (!fileData || fileSize == 0)
    {
        LAppPal::PrintLogLn("[TextureLoader] ERROR: Failed to load texture file: %s", assetPath);
        return 0;
    }

    GLuint texId = LoadTextureFromMemory(fileData, static_cast<int>(fileSize));

    // Release the file buffer.
    LAppPal::ReleaseBytes(fileData);

    return texId;
}

GLuint TextureLoader::LoadTextureFromMemory(const unsigned char* data, int size)
{
    if (!data || size <= 0)
    {
        return 0;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    // Decode the image using stb_image.
    // Force RGBA output for consistent texture format.
    unsigned char* pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (!pixels)
    {
        LAppPal::PrintLogLn("[TextureLoader] ERROR: stb_image failed to decode image: %s", stbi_failure_reason());
        return 0;
    }

    // Create the OpenGL texture.
    GLuint texId = CreateGLTexture(pixels, width, height, 4);

    // Free the decoded pixel data.
    stbi_image_free(pixels);

    return texId;
}

void TextureLoader::ReleaseTexture(GLuint texId)
{
    if (texId != 0)
    {
        glDeleteTextures(1, &texId);
    }
}
