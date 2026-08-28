/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "Live2DModel.hpp"
#include "LAppDefine.hpp"
#include "LAppPal.hpp"
#include "TextureLoader.hpp"

#include <CubismFramework.hpp>
#include <Id/CubismIdManager.hpp>
#include <CubismModelSettingJson.hpp>
#include <Model/CubismMoc.hpp>
#include <Model/CubismModel.hpp>
#include <Motion/CubismMotion.hpp>
#include <Motion/CubismMotionManager.hpp>
#include <Motion/CubismExpressionMotion.hpp>
#include <Motion/CubismExpressionMotionManager.hpp>
#include <Effect/CubismEyeBlink.hpp>
#include <Effect/CubismBreath.hpp>
#include <Effect/CubismPose.hpp>
#include <Physics/CubismPhysics.hpp>
#include <Model/CubismModelUserData.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Math/CubismModelMatrix.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <Utils/CubismJson.hpp>
#include <Type/csmString.hpp>

#include <GLES2/gl2.h>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace Csm;
using namespace Csm::Rendering;

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

/**
 * @brief  Build a motion map key from group name and index.
 */
static csmString MakeMotionKey(const csmChar* group, csmInt32 index)
{
    csmString key(group);
    key+= "_";
    // Convert index to string.
    csmChar buf[16];
    snprintf(buf, sizeof(buf), "%d", index);
    key+= buf;
    return key;
}

// --------------------------------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------------------------------

Live2DModel::Live2DModel()
    : CubismUserModel()
    , _textureCount(0)
    , _motionGroupCount(0)
{
    memset(_motionGroupNames, 0, sizeof(_motionGroupNames));
}

Live2DModel::~Live2DModel()
{
    // Release renderer textures.
    CubismRenderer_OpenGLES2* renderer = GetRenderer<CubismRenderer_OpenGLES2>();
    if (renderer)
    {
        for (csmUint32 i = 0; i < _textures.GetSize(); ++i)
        {
            TextureLoader::ReleaseTexture(_textures[i]);
        }
        _textures.Clear();
    }

    // Release motions.
    ReleaseMotions();

    // Release expressions.
    ReleaseExpressions();

    // The renderer is destroyed by the parent class destructor.
}

// --------------------------------------------------------------------------
// Buffer overrides (used by CubismUserModel's loading methods)
// --------------------------------------------------------------------------

csmByte* Live2DModel::CreateBuffer(const csmChar* path, csmSizeInt* size)
{
    // Build the full path: modelHomeDir + "/" + path
    csmString fullPath = _modelHomeDir;
    fullPath+= "/";
    fullPath+= path;

    if (LAppDefine::DebugLogEnable)
    {
        LAppPal::PrintLogLn("[Live2DModel] Load: %s", fullPath.GetRawString());
    }

    return LAppPal::LoadFileAsBytes(fullPath.GetRawString(), size);
}

void Live2DModel::DeleteBuffer(csmByte* buffer, const csmChar* path)
{
    LAppPal::ReleaseBytes(buffer);
}

// --------------------------------------------------------------------------
// Asset path helper
// --------------------------------------------------------------------------

csmString Live2DModel::GetAssetPath(const csmChar* filename) const
{
    csmString path(_modelHomeDir);
    path+= "/";
    path+= filename;
    return path;
}

// --------------------------------------------------------------------------
// LoadAssets
// --------------------------------------------------------------------------

void Live2DModel::LoadAssets(const csmChar* dir, const csmChar* fileName)
{
    LAppPal::FileLog("[Live2DModel::LoadAssets] START dir=%s, file=%s", dir, fileName);
    LAppPal::FlushFileLog();

    _modelDir = dir;
    _modelHomeDir = dir;
    _modelSettingFileName = fileName;

    // Build the full path to model3.json within the live2d resources.
    csmString settingPath;
    settingPath+= dir;
    settingPath+= "/";
    settingPath+= fileName;
    LAppPal::FileLog("[Live2DModel::LoadAssets] settingPath=%s", settingPath.GetRawString());

    // Load model3.json.
    LAppPal::FileLog("[Live2DModel::LoadAssets] Loading model3.json...");
    LAppPal::FlushFileLog();
    csmSizeInt size = 0;
    csmByte* buffer = LAppPal::LoadFileAsBytes(settingPath.GetRawString(), &size);
    if (!buffer)
    {
        LAppPal::FileLog("[Live2DModel::LoadAssets] FATAL: Failed to load model3.json: %s", settingPath.GetRawString());
        LAppPal::FlushFileLog();
        LAppPal::PrintLogLn("[Live2DModel] ERROR: Failed to load model3.json: %s", settingPath.GetRawString());
        return;
    }
    LAppPal::FileLog("[Live2DModel::LoadAssets] model3.json loaded: %d bytes", size);

    // Parse the model setting JSON.
    LAppPal::FileLog("[Live2DModel::LoadAssets] Parsing model3.json...");
    LAppPal::FlushFileLog();
    CubismModelSettingJson* setting = CSM_NEW CubismModelSettingJson(buffer, size);
    LAppPal::FileLog("[Live2DModel::LoadAssets] model3.json parsed OK");
    LAppPal::ReleaseBytes(buffer);
    buffer = nullptr;

    // Update model home directory from the model setting's directory.
    _modelHomeDir = _modelDir;

    // Set up all model components.
    LAppPal::FileLog("[Live2DModel::LoadAssets] Calling SetupModel...");
    LAppPal::FlushFileLog();
    SetupModel(setting);
    LAppPal::FileLog("[Live2DModel::LoadAssets] SetupModel OK");

    // Import expressions.
    if (setting->GetExpressionCount() > 0)
    {
        for (csmInt32 i = 0; i < setting->GetExpressionCount(); ++i)
        {
            const csmChar* name = setting->GetExpressionName(i);
            const csmChar* expressionFile = setting->GetExpressionFileName(i);

            csmString path = GetAssetPath(expressionFile);
            csmSizeInt expSize = 0;
            csmByte* expBuffer = LAppPal::LoadFileAsBytes(path.GetRawString(), &expSize);
            if (expBuffer)
            {
                ACubismMotion* motion = LoadExpression(expBuffer, expSize, name);
                if (_expressions[name])
                {
                    ACubismMotion::Delete(_expressions[name]);
                }
                _expressions[name] = motion;
                LAppPal::ReleaseBytes(expBuffer);
            }
        }
    }

    // NOTE: Motion preloading is DISABLED due to Cubism Core v6 / Framework v5 incompatibility.
    // Motions will be loaded on-demand when played (see StartMotion).
    // This avoids crashes in CubismMotion::Parse() caused by version mismatch.
    LAppPal::FileLog("[LoadAssets] Motion preloading DISABLED (on-demand loading enabled)");
    _motionGroupCount = setting->GetMotionGroupCount();
    for (csmInt32 i = 0; i < _motionGroupCount && i < 16; ++i)
    {
        const csmChar* group = setting->GetMotionGroupName(i);
        if (group)
        {
            strncpy(_motionGroupNames[i], group, sizeof(_motionGroupNames[i]) - 1);
            _motionGroupNames[i][sizeof(_motionGroupNames[i]) - 1] = '\0';
        }
    }
    LAppPal::FileLog("[LoadAssets] Stored %d motion group names for on-demand loading", _motionGroupCount);

    // Create the renderer BEFORE setting up textures (textures need to bind to renderer).
    LAppPal::FileLog("[LoadAssets] Calling CreateRenderer(2048, 2048)...");
    LAppPal::FlushFileLog();
    CreateRenderer(2048, 2048);
    LAppPal::FileLog("[LoadAssets] Renderer created OK, renderer=%p", (void*)GetRenderer<CubismRenderer_OpenGLES2>());
    LAppPal::FlushFileLog();

    // Set up textures.
    SetupTextures();

    // Clean up.
    CSM_DELETE(setting);
}

// --------------------------------------------------------------------------
// SetupModel
// --------------------------------------------------------------------------

void Live2DModel::SetupModel(ICubismModelSetting* setting)
{
    LAppPal::FileLog("[SetupModel] START");
    LAppPal::FlushFileLog();

    _updating = true;
    _initialized = false;

    // Load MOC file.
    csmBool mocConsistency = false;
    const csmChar* mocFile = setting->GetModelFileName();
    LAppPal::FileLog("[SetupModel] MOC file: %s", mocFile ? mocFile : "(null)");
    if (mocFile && strlen(mocFile) > 0)
    {
        csmString path = GetAssetPath(mocFile);
        LAppPal::FileLog("[SetupModel] Loading MOC: %s", path.GetRawString());
        LAppPal::FlushFileLog();
        csmSizeInt size = 0;
        csmByte* buffer = LAppPal::LoadFileAsBytes(path.GetRawString(), &size);
        if (buffer)
        {
            LAppPal::FileLog("[SetupModel] MOC loaded: %d bytes, calling LoadModel...", size);
            LAppPal::FlushFileLog();
            LoadModel(buffer, size, mocConsistency);
            LAppPal::FileLog("[SetupModel] LoadModel returned, _model=%p", (void*)_model);
            LAppPal::ReleaseBytes(buffer);
        }
        else
        {
            LAppPal::FileLog("[SetupModel] FATAL: MOC load failed: %s", path.GetRawString());
        }
    }

    if (!_model)
    {
        LAppPal::FileLog("[SetupModel] FATAL: _model is null after LoadModel!");
        LAppPal::FlushFileLog();
        LAppPal::PrintLogLn("[Live2DModel] ERROR: Failed to load model (MOC).");
        return;
    }
    LAppPal::FileLog("[SetupModel] Model created OK");

    // Apply layout.
    csmMap<csmString, csmFloat32> layout;
    if (setting->GetLayoutMap(layout))
    {
        _modelMatrix->SetupFromLayout(layout);
        LAppPal::FileLog("[SetupModel] Layout applied");
    }

    // Load physics.
    const csmChar* physicsFile = setting->GetPhysicsFileName();
    LAppPal::FileLog("[SetupModel] Physics file: %s", physicsFile ? physicsFile : "(none)");
    if (physicsFile && strlen(physicsFile) > 0)
    {
        csmString path = GetAssetPath(physicsFile);
        csmSizeInt size = 0;
        csmByte* buffer = LAppPal::LoadFileAsBytes(path.GetRawString(), &size);
        if (buffer)
        {
            LAppPal::FileLog("[SetupModel] Loading physics: %d bytes", size);
            LoadPhysics(buffer, size);
            LAppPal::ReleaseBytes(buffer);
        }
    }

    // Load pose.
    const csmChar* poseFile = setting->GetPoseFileName();
    LAppPal::FileLog("[SetupModel] Pose file: %s", poseFile ? poseFile : "(none)");
    if (poseFile && strlen(poseFile) > 0)
    {
        csmString path = GetAssetPath(poseFile);
        csmSizeInt size = 0;
        csmByte* buffer = LAppPal::LoadFileAsBytes(path.GetRawString(), &size);
        if (buffer)
        {
            LAppPal::FileLog("[SetupModel] Loading pose: %d bytes", size);
            LoadPose(buffer, size);
            LAppPal::ReleaseBytes(buffer);
        }
    }

    // Load user data.
    const csmChar* userDataFile = setting->GetUserDataFile();
    LAppPal::FileLog("[SetupModel] UserData file: %s", userDataFile ? userDataFile : "(none)");
    if (userDataFile && strlen(userDataFile) > 0)
    {
        csmString path = GetAssetPath(userDataFile);
        csmSizeInt size = 0;
        csmByte* buffer = LAppPal::LoadFileAsBytes(path.GetRawString(), &size);
        if (buffer)
        {
            LAppPal::FileLog("[SetupModel] Loading userdata: %d bytes", size);
            LoadUserData(buffer, size);
            LAppPal::ReleaseBytes(buffer);
        }
    }

    // Set up eye blink parameters.
    if (_eyeBlink)
    {
        CubismEyeBlink::Delete(_eyeBlink);
        _eyeBlink = nullptr;
    }
    _eyeBlink = CubismEyeBlink::Create(setting);
    LAppPal::FileLog("[SetupModel] EyeBlink created");

    // Set up breath.
    if (_breath)
    {
        CubismBreath::Delete(_breath);
        _breath = nullptr;
    }
    _breath = CubismBreath::Create();
    LAppPal::FileLog("[SetupModel] Breath created");

    _updating = false;
    _initialized = true;

    LAppPal::FileLog("[SetupModel] COMPLETE");
    LAppPal::FlushFileLog();
}

// --------------------------------------------------------------------------
// SetupTextures
// --------------------------------------------------------------------------

void Live2DModel::SetupTextures()
{
    LAppPal::FileLog("[SetupTextures] START");
    LAppPal::FlushFileLog();

    // Re-load model3.json to get texture paths.
    csmString settingPath;
    settingPath+= _modelDir;
    settingPath+= "/";
    settingPath+= _modelSettingFileName;

    csmSizeInt settingSize = 0;
    csmByte* settingBuffer = LAppPal::LoadFileAsBytes(settingPath.GetRawString(), &settingSize);
    if (!settingBuffer)
    {
        LAppPal::FileLog("[SetupTextures] FATAL: Could not load model3.json for textures");
        LAppPal::FlushFileLog();
        LAppPal::PrintLogLn("[Live2DModel] WARNING: Could not load model3.json for texture loading: %s",
                            settingPath.GetRawString());
        return;
    }

    CubismModelSettingJson* modelSetting = CSM_NEW CubismModelSettingJson(settingBuffer, settingSize);
    LAppPal::ReleaseBytes(settingBuffer);

    // Build texture base path: modelHomeDir + "/"
    // Note: texture filenames in model3.json already include subdirectory
    // (e.g., "Haru.2048/texture_00.png"), so we don't use GetTextureDirectory().
    csmString texBasePath;
    texBasePath+= _modelHomeDir;
    texBasePath+= "/";
    LAppPal::FileLog("[SetupTextures] texBasePath=%s, texCount=%d", texBasePath.GetRawString(), modelSetting->GetTextureCount());

    CubismRenderer_OpenGLES2* renderer = GetRenderer<CubismRenderer_OpenGLES2>();
    LAppPal::FileLog("[SetupTextures] renderer=%p", (void*)renderer);

    for (csmInt32 i = 0; i < modelSetting->GetTextureCount(); ++i)
    {
        const csmChar* texFile = modelSetting->GetTextureFileName(i);
        csmString texPath = texBasePath;
        texPath+= texFile;

        LAppPal::FileLog("[SetupTextures] Loading texture %d: %s", i, texPath.GetRawString());
        LAppPal::FlushFileLog();
        GLuint texId = TextureLoader::LoadTextureFromAssets(texPath.GetRawString());
        LAppPal::FileLog("[SetupTextures] Texture %d loaded: texId=%u", i, texId);
        _textures.PushBack(texId);

        if (renderer)
        {
            LAppPal::FileLog("[SetupTextures] Binding texture %d -> texId=%u", i, texId);
            renderer->BindTexture(i, texId);
        }
        else
        {
            LAppPal::FileLog("[SetupTextures] WARNING: renderer is null, cannot bind texture %d", i);
        }

        if (LAppDefine::DebugLogEnable)
        {
            LAppPal::PrintLogLn("[Live2DModel] Texture %d: %s (GL id=%u)", i, texPath.GetRawString(), texId);
        }
    }

    _textureCount = static_cast<csmInt32>(_textures.GetSize());

    CSM_DELETE(modelSetting);
}

// --------------------------------------------------------------------------
// Update
// --------------------------------------------------------------------------

void Live2DModel::Update()
{
    if (!_model || !_initialized)
    {
        return;
    }

    const csmFloat32 deltaTimeSeconds = LAppPal::GetDeltaTime();
    const csmFloat32 userTimeSeconds  = LAppPal::GetSystemTime();

    // Update motion.
    csmBool motionUpdated = _motionManager->UpdateMotion(_model, deltaTimeSeconds);

    // Update model parameters if motion did not update them.
    if (!motionUpdated)
    {
        // Default: do nothing special; the model keeps its current parameters.
    }

    // Update breath.
    if (_breath)
    {
        _breath->UpdateParameters(_model, deltaTimeSeconds);
    }

    // Update eye blink.
    if (_eyeBlink)
    {
        _eyeBlink->UpdateParameters(_model, deltaTimeSeconds);
    }

    // Update physics.
    if (_physics)
    {
        _physics->Evaluate(_model, deltaTimeSeconds);
    }

    // Update pose.
    if (_pose)
    {
        _pose->UpdateParameters(_model, deltaTimeSeconds);
    }

    // Update expression.
    _expressionManager->UpdateMotion(_model, deltaTimeSeconds);

    // Update the model itself (save parameters, etc.).
    _model->Update();
}

// --------------------------------------------------------------------------
// Draw
// --------------------------------------------------------------------------

void Live2DModel::Draw(CubismMatrix44& matrix)
{
    if (!_model || !GetRenderer<CubismRenderer_OpenGLES2>())
    {
        return;
    }

    // Apply the model matrix.
    matrix.MultiplyByMatrix(_modelMatrix);

    // Set the MVP matrix on the renderer.
    GetRenderer<CubismRenderer_OpenGLES2>()->SetMvpMatrix(&matrix);

    // Draw the model.
    GetRenderer<CubismRenderer_OpenGLES2>()->DrawModel();
}

// --------------------------------------------------------------------------
// HitTest
// --------------------------------------------------------------------------

csmBool Live2DModel::HitTest(const csmChar* hitAreaName, csmFloat32 x, csmFloat32 y)
{
    if (!_model || !_initialized)
    {
        return false;
    }

    // Re-load model3.json for hit area info.
    // For efficiency, this could be cached, but for the simplified version we re-parse.
    csmString settingPath;
    settingPath+= _modelDir;
    settingPath+= "/";
    settingPath+= _modelSettingFileName;

    csmSizeInt size = 0;
    csmByte* buffer = LAppPal::LoadFileAsBytes(settingPath.GetRawString(), &size);
    if (!buffer)
    {
        return false;
    }

    CubismModelSettingJson* modelSetting = CSM_NEW CubismModelSettingJson(buffer, size);
    LAppPal::ReleaseBytes(buffer);

    // Search for the hit area by name.
    for (csmInt32 i = 0; i < modelSetting->GetHitAreasCount(); ++i)
    {
        if (strcmp(modelSetting->GetHitAreaName(i), hitAreaName) == 0)
        {
            CubismIdHandle drawableId = modelSetting->GetHitAreaId(i);
            CSM_DELETE(modelSetting);
            return IsHit(drawableId, x, y);
        }
    }

    CSM_DELETE(modelSetting);
    return false;
}

// --------------------------------------------------------------------------
// Motion management
// --------------------------------------------------------------------------

void Live2DModel::PreloadMotionGroup(const csmChar* group)
{
    LAppPal::FileLog("[PreloadMotionGroup] group=%s", group);
    LAppPal::FlushFileLog();

    // Re-load model3.json to get motion file names.
    csmString settingPath;
    settingPath+= _modelDir;
    settingPath+= "/";
    settingPath+= _modelSettingFileName;

    csmSizeInt size = 0;
    csmByte* buffer = LAppPal::LoadFileAsBytes(settingPath.GetRawString(), &size);
    if (!buffer)
    {
        LAppPal::FileLog("[PreloadMotionGroup] FATAL: could not load model3.json");
        LAppPal::FlushFileLog();
        return;
    }

    CubismModelSettingJson* modelSetting = CSM_NEW CubismModelSettingJson(buffer, size);
    LAppPal::ReleaseBytes(buffer);

    const csmInt32 count = modelSetting->GetMotionCount(group);
    LAppPal::FileLog("[PreloadMotionGroup] motion count=%d", count);

    for (csmInt32 i = 0; i < count; ++i)
    {
        LAppPal::FileLog("[PreloadMotionGroup] motion %d/%d...", i, count);
        LAppPal::FlushFileLog();

        const csmChar* motionFile = modelSetting->GetMotionFileName(group, i);
        if (!motionFile || strlen(motionFile) == 0)
        {
            LAppPal::FileLog("[PreloadMotionGroup] motion %d: empty filename, skip", i);
            continue;
        }

        csmString path = GetAssetPath(motionFile);
        csmSizeInt size = 0;
        csmByte* buffer = LAppPal::LoadFileAsBytes(path.GetRawString(), &size);
        if (!buffer)
        {
            LAppPal::FileLog("[PreloadMotionGroup] motion %d: load failed, skip", i);
            continue;
        }
        LAppPal::FileLog("[PreloadMotionGroup] motion %d: file loaded (%d bytes)", i, size);

        csmString key = MakeMotionKey(group, i);
        LAppPal::FileLog("[PreloadMotionGroup] motion %d: key=%s", i, key.GetRawString());

        // Release existing motion if any.
        if (_motions[key])
        {
            ACubismMotion::Delete(_motions[key]);
        }

        // Get fade times from model setting.
        csmFloat32 fadeInTime = modelSetting->GetMotionFadeInTimeValue(group, i);
        csmFloat32 fadeOutTime = modelSetting->GetMotionFadeOutTimeValue(group, i);
        LAppPal::FileLog("[PreloadMotionGroup] motion %d: calling LoadMotion...", i);
        LAppPal::FlushFileLog();

        ACubismMotion* motion = LoadMotion(buffer, size, key.GetRawString(),
                                            nullptr, nullptr,
                                            modelSetting, group, i);
        LAppPal::FileLog("[PreloadMotionGroup] motion %d: LoadMotion returned %p", i, (void*)motion);

        if (motion)
        {
            if (fadeInTime >= 0.0f)
            {
                motion->SetFadeInTime(fadeInTime);
            }
            if (fadeOutTime >= 0.0f)
            {
                motion->SetFadeOutTime(fadeOutTime);
            }
        }

        _motions[key] = motion;
        LAppPal::ReleaseBytes(buffer);
        LAppPal::FileLog("[PreloadMotionGroup] motion %d: done", i);
        LAppPal::FlushFileLog();
    }

    LAppPal::FileLog("[PreloadMotionGroup] deleting modelSetting...");
    CSM_DELETE(modelSetting);
    LAppPal::FileLog("[PreloadMotionGroup] COMPLETE");
    LAppPal::FlushFileLog();
}

void Live2DModel::ReleaseMotionGroup(const csmChar* group) const
{
    // Iterate all motions and release those matching the group prefix.
    // Since csmMap doesn't have a remove-by-prefix, we'll just delete matching entries.
    // This is a simplified approach.
    (void)group; // Suppress unused warning in simplified version.
}

void Live2DModel::ReleaseMotions()
{
    for (csmMap<csmString, ACubismMotion*>::const_iterator iter = _motions.Begin(); iter != _motions.End(); ++iter)
    {
        ACubismMotion::Delete((*iter).Second);
    }
    _motions.Clear();
}

void Live2DModel::ReleaseExpressions()
{
    for (csmMap<csmString, ACubismMotion*>::const_iterator iter = _expressions.Begin(); iter != _expressions.End(); ++iter)
    {
        ACubismMotion::Delete((*iter).Second);
    }
    _expressions.Clear();
}

CubismMotionQueueEntryHandle Live2DModel::StartMotion(const csmChar* group,
                                                        const csmInt32 no,
                                                        const csmInt32 priority)
{
    if (priority == LAppDefine::PriorityForce)
    {
        _motionManager->SetReservePriority(priority);
    }
    else if (!_motionManager->ReserveMotion(priority))
    {
        if (LAppDefine::DebugLogEnable)
        {
            LAppPal::PrintLogLn("[Live2DModel] Cannot start motion: priority too low.");
        }
        return InvalidMotionQueueEntryHandleValue;
    }

    csmString key = MakeMotionKey(group, no);
    ACubismMotion* motion = _motions[key];

    // On-demand motion loading: if motion not preloaded, load it now.
    if (!motion)
    {
        LAppPal::FileLog("[StartMotion] Motion not cached, loading on-demand: %s", key.GetRawString());
        LAppPal::FlushFileLog();

        // Load model3.json to get the motion filename.
        csmString settingPath;
        settingPath+= _modelDir;
        settingPath+= "/";
        settingPath+= _modelSettingFileName;

        csmSizeInt settingSize = 0;
        csmByte* settingBuffer = LAppPal::LoadFileAsBytes(settingPath.GetRawString(), &settingSize);
        if (settingBuffer)
        {
            CubismModelSettingJson* modelSetting = CSM_NEW CubismModelSettingJson(settingBuffer, settingSize);
            LAppPal::ReleaseBytes(settingBuffer);

            const csmChar* motionFile = modelSetting->GetMotionFileName(group, no);
            if (motionFile && strlen(motionFile) > 0)
            {
                csmString path = GetAssetPath(motionFile);
                csmSizeInt motionSize = 0;
                csmByte* motionBuffer = LAppPal::LoadFileAsBytes(path.GetRawString(), &motionSize);
                if (motionBuffer)
                {
                    motion = LoadMotion(motionBuffer, motionSize, key.GetRawString());
                    if (motion)
                    {
                        // Get fade times.
                        csmFloat32 fadeInTime = modelSetting->GetMotionFadeInTimeValue(group, no);
                        csmFloat32 fadeOutTime = modelSetting->GetMotionFadeOutTimeValue(group, no);
                        if (fadeInTime >= 0.0f) motion->SetFadeInTime(fadeInTime);
                        if (fadeOutTime >= 0.0f) motion->SetFadeOutTime(fadeOutTime);

                        _motions[key] = motion;
                        LAppPal::FileLog("[StartMotion] Motion loaded on-demand OK: %s", key.GetRawString());
                    }
                    LAppPal::ReleaseBytes(motionBuffer);
                }
            }
            CSM_DELETE(modelSetting);
        }
    }

    if (!motion)
    {
        if (LAppDefine::DebugLogEnable)
        {
            LAppPal::PrintLogLn("[Live2DModel] Motion not found: %s", key.GetRawString());
        }
        _motionManager->SetReservePriority(LAppDefine::PriorityNone);
        return InvalidMotionQueueEntryHandleValue;
    }

    // Set finished motion handler.
    auto finishedHandler = [](ACubismMotion* self) {
        // Motion finished callback.
        (void)self;
    };

    if (LAppDefine::DebugLogEnable)
    {
        LAppPal::PrintLogLn("[Live2DModel] Start motion: %s (priority=%d)", key.GetRawString(), priority);
    }

    return _motionManager->StartMotionPriority(motion, false, priority);
}

CubismMotionQueueEntryHandle Live2DModel::StartRandomMotion(const csmChar* group,
                                                              const csmInt32 priority)
{
    // Get motion count for this group.
    csmString settingPath;
    settingPath+= _modelDir;
    settingPath+= "/";
    settingPath+= _modelSettingFileName;

    csmSizeInt size = 0;
    csmByte* buffer = LAppPal::LoadFileAsBytes(settingPath.GetRawString(), &size);
    if (!buffer)
    {
        return InvalidMotionQueueEntryHandleValue;
    }

    CubismModelSettingJson* modelSetting = CSM_NEW CubismModelSettingJson(buffer, size);
    LAppPal::ReleaseBytes(buffer);

    const csmInt32 count = modelSetting->GetMotionCount(group);
    CSM_DELETE(modelSetting);

    if (count <= 0)
    {
        return InvalidMotionQueueEntryHandleValue;
    }

    // Pick a random motion.
    const csmInt32 no = rand() % count;
    return StartMotion(group, no, priority);
}

// --------------------------------------------------------------------------
// Expression
// --------------------------------------------------------------------------

void Live2DModel::SetExpression(const csmChar* expressionID)
{
    ACubismMotion* motion = _expressions[expressionID];
    if (motion)
    {
        if (LAppDefine::DebugLogEnable)
        {
            LAppPal::PrintLogLn("[Live2DModel] Set expression: %s", expressionID);
        }
        // ExpressionMotionManager inherits from CubismMotionQueueManager,
        // which provides StartMotion (not StartMotionPriority).
        _expressionManager->StartMotion(motion, false);
    }
}

void Live2DModel::SetRandomExpression()
{
    if (_expressions.GetSize() == 0)
    {
        return;
    }
    const csmInt32 no = rand() % _expressions.GetSize();
    // csmMap iterator uses operator* returning csmPair, not operator->.
    csmUint32 idx = 0;
    for (csmMap<csmString, ACubismMotion*>::const_iterator it = _expressions.Begin();
         it != _expressions.End(); ++it, ++idx)
    {
        if (idx == static_cast<csmUint32>(no))
        {
            SetExpression((*it).First.GetRawString());
            return;
        }
    }
}

void Live2DModel::SetParameterValue(const Csm::csmChar* name, float value)
{
    if (!_model) return;
    // 将字符串转换为 CubismIdHandle
    Csm::CubismIdHandle parameterId = Csm::CubismFramework::GetIdManager()->GetId(name);
    if (parameterId)
    {
        // 设置参数值，权重 1.0 表示直接覆盖
        _model->SetParameterValue(parameterId, value, 1.0f);
    }
}
// --------------------------------------------------------------------------
// Getters
// --------------------------------------------------------------------------

const csmString& Live2DModel::GetModelDir() const
{
    return _modelDir;
}
