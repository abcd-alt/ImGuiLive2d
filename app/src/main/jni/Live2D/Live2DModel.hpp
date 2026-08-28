/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <CubismFramework.hpp>
#include <Model/CubismUserModel.hpp>
#include <ICubismModelSetting.hpp>
#include <Math/CubismMatrix44.hpp>
#include <Type/csmVector.hpp>
#include <Type/csmMap.hpp>
#include <Id/CubismId.hpp>
#include <GLES2/gl2.h>

/**
 * @brief  Simplified Live2D model class.
 *
 * Extends CubismUserModel to provide a complete model lifecycle:
 * loading assets, updating per frame, drawing, and handling motions/hit tests.
 */
class Live2DModel : public Csm::CubismUserModel
{
public:
    /**
     * @brief  Constructor.
     */
    Live2DModel();

    /**
     * @brief  Destructor.
     */
    virtual ~Live2DModel();

    /**
     * @brief  Load model assets from the given directory and model3.json file.
     *
     * @param[in]  dir       Directory within assets (relative to "live2d/") containing model files.
     * @param[in]  fileName  Name of the model3.json file.
     */
    void LoadAssets(const Csm::csmChar* dir, const Csm::csmChar* fileName);

    /**
     * @brief  Update the model for one frame.
     *
     * Updates motion, breath, eye blink, physics, pose, and user data.
     */
    void Update();

    /**
     * @brief  Draw the model using the given projection matrix.
     *
     * @param[in]  matrix  Projection matrix to apply.
     */
    void Draw(Csm::CubismMatrix44& matrix);

    /**
     * @brief  Perform a hit test at the given position.
     *
     * @param[in]  hitAreaName  Name of the hit area to test (e.g. "Head", "Body").
     * @param[in]  x  X coordinate in logical view space.
     * @param[in]  y  Y coordinate in logical view space.
     *
     * @return  true if the position hits the specified area.
     */
    Csm::csmBool HitTest(const Csm::csmChar* hitAreaName, Csm::csmFloat32 x, Csm::csmFloat32 y);

    /**
     * @brief  Preload all motions in the specified group.
     *
     * @param[in]  group  Name of the motion group to preload.
     */
    void PreloadMotionGroup(const Csm::csmChar* group);

    /**
     * @brief  Release all motions in the specified group.
     *
     * @param[in]  group  Name of the motion group to release.
     */
    void ReleaseMotionGroup(const Csm::csmChar* group) const;

    /**
     * @brief  Release all loaded motions.
     */
    void ReleaseMotions();

    /**
     * @brief  Release all loaded expressions.
     */
    void ReleaseExpressions();
    
    /**
    * @brief 通过参数名设置模型参数值（用于视线跟踪等）    
    * @param  name  参数名称（例如 "ParamAngleX"）
    * @param  value 目标值
    */
    void SetParameterValue(const Csm::csmChar* name, float value);

    /**
     * @brief  Start a specific motion in the given group.
     *
     * @param[in]  group     Name of the motion group.
     * @param[in]  no        Index of the motion within the group.
     * @param[in]  priority  Priority of the motion.
     *
     * @return  Motion queue entry handle, or 0 if the motion could not start.
     */
    Csm::CubismMotionQueueEntryHandle StartMotion(const Csm::csmChar* group,
                                                    const Csm::csmInt32 no,
                                                    const Csm::csmInt32 priority);

    /**
     * @brief  Start a random motion from the given group.
     *
     * @param[in]  group     Name of the motion group.
     * @param[in]  priority  Priority of the motion.
     *
     * @return  Motion queue entry handle, or 0 if the motion could not start.
     */
    Csm::CubismMotionQueueEntryHandle StartRandomMotion(const Csm::csmChar* group,
                                                          const Csm::csmInt32 priority);

    /**
     * @brief  Set an expression by name.
     *
     * @param[in]  expressionID  Name of the expression.
     */
    void SetExpression(const Csm::csmChar* expressionID);

    /**
     * @brief  Set a random expression.
     */
    void SetRandomExpression();

    /**
     * @brief  Get the model's directory path (relative to "live2d/").
     *
     * @return  Directory path string.
     */
    const Csm::csmString& GetModelDir() const;

protected:
    /**
     * @brief  Override: Create a buffer by loading a file from assets.
     */
    virtual Csm::csmByte* CreateBuffer(const Csm::csmChar* path, Csm::csmSizeInt* size);

    /**
     * @brief  Override: Release a buffer.
     */
    virtual void DeleteBuffer(Csm::csmByte* buffer, const Csm::csmChar* path = "");

private:
    /**
     * @brief  Set up the model from the model setting JSON.
     *
     * Loads the MOC, expressions, physics, pose, user data, display info,
     * and sets up eye blink and lip sync parameters.
     *
     * @param[in]  setting  The model setting.
     */
    void SetupModel(Csm::ICubismModelSetting* setting);

    /**
     * @brief  Load textures referenced by the model and bind them to the renderer.
     */
    void SetupTextures();

    /**
     * @brief  Build a full asset path from the model directory and a relative filename.
     *
     * @param[in]  filename  Relative filename within the model directory.
     *
     * @return  Full relative path (e.g., "modelDir/filename").
     */
    Csm::csmString GetAssetPath(const Csm::csmChar* filename) const;

    Csm::csmString                          _modelDir;        ///< Model directory name.
    Csm::csmString                          _modelHomeDir;    ///< Full home directory path.
    Csm::csmString                          _modelSettingFileName; ///< model3.json filename.
    Csm::csmMap<Csm::csmString, Csm::ACubismMotion*> _motions; ///< Loaded motions (key: group_no).
    Csm::csmMap<Csm::csmString, Csm::ACubismMotion*> _expressions; ///< Loaded expressions.
    Csm::csmVector<GLuint>                  _textures;        ///< OpenGL texture IDs.
    Csm::csmInt32                           _textureCount;    ///< Number of textures loaded.

    // On-demand motion loading support.
    Csm::csmInt32                           _motionGroupCount; ///< Number of motion groups.
    char                                    _motionGroupNames[16][64]; ///< Cached motion group names.
};
