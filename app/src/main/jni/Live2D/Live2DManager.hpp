/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <CubismFramework.hpp>
#include <Type/csmVector.hpp>
#include <Math/CubismMatrix44.hpp>

class Live2DModel;
class LAppAllocator;

/**
 * @brief  Manages the Cubism Framework lifecycle and loaded models.
 *
 * Provides a simple API for initializing the framework, loading models,
 * and performing per-frame update/draw operations.
 */
class Live2DManager
{
public:
    /**
     * @brief  Get the singleton instance.
     *
     * @return  Reference to the singleton Live2DManager.
     */
    static Live2DManager& GetInstance();

    /**
     * @brief  Delete the singleton instance (releases all resources).
     */
    static void DeleteInstance();

    /**
     * @brief  Initialize the Cubism Framework.
     *
     * Must be called once before any other Live2D operations.
     * Sets up the allocator, logging, and file loading functions.
     */
    void Init();

    /**
     * @brief  Load a model from the given directory and model3.json file.
     *
     * @param[in]  modelDir   Directory within the "live2d/" assets path.
     * @param[in]  fileName   Name of the model3.json file.
     *
     * @return  true if the model was loaded successfully.
     */
    Csm::csmBool LoadModel(const Csm::csmChar* modelDir, const Csm::csmChar* fileName);

    /**
     * @brief  Update all loaded models for one frame.
     */
    void Update();

    /**
     * @brief  Draw all loaded models.
     *
     * @param[in]  projectionMatrix  The projection matrix to use for rendering.
     */
    void Draw(Csm::CubismMatrix44& projectionMatrix);

    /**
     * @brief  Shut down the Cubism Framework and release all resources.
     */
    void Shutdown();

    /**
     * @brief  Get the first loaded model (convenience for single-model usage).
     *
     * @return  Pointer to the first model, or nullptr if none loaded.
     */
    Live2DModel* GetModel();

    /**
     * @brief  Get a model by index.
     *
     * @param[in]  index  Index of the model.
     *
     * @return  Pointer to the model, or nullptr if index is out of range.
     */
    Live2DModel* GetModel(Csm::csmUint32 index);

    /**
     * @brief  Get the number of loaded models.
     *
     * @return  Number of models.
     */
    Csm::csmUint32 GetModelCount() const;

    /**
     * @brief  Check if the framework has been initialized.
     *
     * @return  true if initialized.
     */
    Csm::csmBool IsInitialized() const;

private:
    /**
     * @brief  Constructor (private for singleton).
     */
    Live2DManager();

    /**
     * @brief  Destructor (private for singleton).
     */
    ~Live2DManager();

    /**
     * @brief  Copy prevention.
     */
    Live2DManager(const Live2DManager&);
    Live2DManager& operator=(const Live2DManager&);

    static Live2DManager*           s_instance;   ///< Singleton instance.
    LAppAllocator*                  _allocator;   ///< Memory allocator for the framework.
    Csm::csmVector<Live2DModel*>    _models;      ///< Loaded models.
    Csm::csmBool                    _initialized;  ///< Framework initialization flag.
};
