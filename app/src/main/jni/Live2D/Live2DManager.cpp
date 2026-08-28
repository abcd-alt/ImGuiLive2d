/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "Live2DManager.hpp"
#include "Live2DModel.hpp"
#include "LAppDefine.hpp"
#include "LAppPal.hpp"
#include "LAppAllocator.hpp"

#include <CubismFramework.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>

#include <cstring>

using namespace Csm;
using namespace Csm::Rendering;

// --------------------------------------------------------------------------
// Static members
// --------------------------------------------------------------------------

Live2DManager* Live2DManager::s_instance = nullptr;

// --------------------------------------------------------------------------
// Callbacks for the Cubism Framework
// --------------------------------------------------------------------------

/**
 * @brief  Log callback for the Cubism Framework.
 *
 * Routes framework log messages to the Android log system via LAppPal.
 */
static void CubismLogFunction(const csmChar* message)
{
    LAppPal::PrintLogLn("[Cubism] %s", message);
}

/**
 * @brief  File load callback for the Cubism Framework.
 *
 * Routes file loading to LAppPal::LoadFileAsBytes.
 */
static csmByte* CubismLoadFileFunction(const std::string filePath, csmSizeInt* outSize)
{
    return LAppPal::LoadFileAsBytes(filePath.c_str(), outSize);
}

/**
 * @brief  File release callback for the Cubism Framework.
 *
 * Routes memory release to LAppPal::ReleaseBytes.
 */
static void CubismReleaseBytesFunction(csmByte* byteData)
{
    LAppPal::ReleaseBytes(byteData);
}

// --------------------------------------------------------------------------
// Singleton
// --------------------------------------------------------------------------

Live2DManager& Live2DManager::GetInstance()
{
    if (!s_instance)
    {
        s_instance = new Live2DManager();
    }
    return *s_instance;
}

void Live2DManager::DeleteInstance()
{
    if (s_instance)
    {
        s_instance->Shutdown();
        delete s_instance;
        s_instance = nullptr;
    }
}

// --------------------------------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------------------------------

Live2DManager::Live2DManager()
    : _allocator(nullptr)
    , _initialized(false)
{
}

Live2DManager::~Live2DManager()
{
    Shutdown();
}

// --------------------------------------------------------------------------
// Init
// --------------------------------------------------------------------------

void Live2DManager::Init()
{
    if (_initialized)
    {
        LAppPal::FileLog("[Live2DManager::Init] Already initialized, skipping");
        return;
    }

    LAppPal::FileLog("[Live2DManager::Init] Creating allocator...");
    LAppPal::FlushFileLog();

    // Create the allocator.
    _allocator = new LAppAllocator();
    LAppPal::FileLog("[Live2DManager::Init] Allocator created: %p", (void*)_allocator);

    // Set up framework options.
    // NOTE: Must be a static variable! CubismFramework::StartUp() stores a POINTER
    // to this option, so it must outlive Init(). A local variable would be destroyed
    // when Init() returns, leaving a dangling pointer that crashes later when the
    // framework reads LoadFileFunction / ReleaseBytesFunction from it.
    static CubismFramework::Option option;
    option.LogFunction    = CubismLogFunction;
    option.LoggingLevel   = static_cast<CubismFramework::Option::LogLevel>(LAppDefine::CubismLoggingLevel);
    option.LoadFileFunction   = CubismLoadFileFunction;
    option.ReleaseBytesFunction = CubismReleaseBytesFunction;

    LAppPal::FileLog("[Live2DManager::Init] Calling CubismFramework::StartUp...");
    LAppPal::FlushFileLog();

    // Start up the framework.
    const csmBool started = CubismFramework::StartUp(_allocator, &option);
    if (!started)
    {
        LAppPal::FileLog("[Live2DManager::Init] FATAL: CubismFramework::StartUp() FAILED");
        LAppPal::FlushFileLog();
        LAppPal::PrintLogLn("[Live2DManager] ERROR: CubismFramework::StartUp() failed.");
        return;
    }
    LAppPal::FileLog("[Live2DManager::Init] StartUp OK");

    LAppPal::FileLog("[Live2DManager::Init] Calling CubismFramework::Initialize...");
    LAppPal::FlushFileLog();

    // Initialize the framework.
    CubismFramework::Initialize();
    LAppPal::FileLog("[Live2DManager::Init] Initialize OK");

    _initialized = true;
    LAppPal::FileLog("[Live2DManager::Init] COMPLETE");
    LAppPal::FlushFileLog();
}

// --------------------------------------------------------------------------
// LoadModel
// --------------------------------------------------------------------------

csmBool Live2DManager::LoadModel(const csmChar* modelDir, const csmChar* fileName)
{
    LAppPal::FileLog("[Live2DManager::LoadModel] dir=%s, file=%s", modelDir, fileName);
    LAppPal::FlushFileLog();

    if (!_initialized)
    {
        LAppPal::FileLog("[Live2DManager::LoadModel] FATAL: Framework not initialized");
        LAppPal::FlushFileLog();
        LAppPal::PrintLogLn("[Live2DManager] ERROR: Framework not initialized. Call Init() first.");
        return false;
    }

    LAppPal::FileLog("[Live2DManager::LoadModel] Creating Live2DModel...");
    LAppPal::FlushFileLog();
    Live2DModel* model = new Live2DModel();

    LAppPal::FileLog("[Live2DManager::LoadModel] Calling model->LoadAssets()...");
    LAppPal::FlushFileLog();
    model->LoadAssets(modelDir, fileName);
    LAppPal::FileLog("[Live2DManager::LoadModel] LoadAssets() returned OK");

    _models.PushBack(model);
    LAppPal::FileLog("[Live2DManager::LoadModel] Model added. Total models: %d", static_cast<csmInt32>(_models.GetSize()));
    LAppPal::FlushFileLog();

    return true;
}

// --------------------------------------------------------------------------
// Update
// --------------------------------------------------------------------------

void Live2DManager::Update()
{
    for (csmUint32 i = 0; i < _models.GetSize(); ++i)
    {
        _models[i]->Update();
    }

    // Update the time tracking for next frame's delta time.
    LAppPal::UpdateTime();
}

// --------------------------------------------------------------------------
// Draw
// --------------------------------------------------------------------------

void Live2DManager::Draw(CubismMatrix44& projectionMatrix)
{
    for (csmUint32 i = 0; i < _models.GetSize(); ++i)
    {
        _models[i]->Draw(projectionMatrix);
    }
}

// --------------------------------------------------------------------------
// Shutdown
// --------------------------------------------------------------------------

void Live2DManager::Shutdown()
{
    if (!_initialized)
    {
        return;
    }

    if (LAppDefine::DebugLogEnable)
    {
        LAppPal::PrintLogLn("[Live2DManager] Shutting down...");
    }

    // Release all models.
    for (csmUint32 i = 0; i < _models.GetSize(); ++i)
    {
        delete _models[i];
    }
    _models.Clear();

    // Release the renderer's static resources.
    CubismRenderer::StaticRelease();

    // Dispose and clean up the framework.
    CubismFramework::Dispose();
    CubismFramework::CleanUp();

    // Release the allocator.
    delete _allocator;
    _allocator = nullptr;

    _initialized = false;

    if (LAppDefine::DebugLogEnable)
    {
        LAppPal::PrintLogLn("[Live2DManager] Shutdown complete.");
    }
}

// --------------------------------------------------------------------------
// Model accessors
// --------------------------------------------------------------------------

Live2DModel* Live2DManager::GetModel()
{
    if (_models.GetSize() == 0)
    {
        return nullptr;
    }
    return _models[0];
}

Live2DModel* Live2DManager::GetModel(csmUint32 index)
{
    if (index >= _models.GetSize())
    {
        return nullptr;
    }
    return _models[index];
}

csmUint32 Live2DManager::GetModelCount() const
{
    return _models.GetSize();
}

csmBool Live2DManager::IsInitialized() const
{
    return _initialized;
}
