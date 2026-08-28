/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "LAppDefine.hpp"

namespace LAppDefine {

// View
const csmFloat32 ViewScale     = 1.0f;
const csmFloat32 ViewMaxX      = 1.0f;
const csmFloat32 ViewMaxY      = 1.0f;
const csmFloat32 ViewMinX      = -1.0f;
const csmFloat32 ViewMinY      = -1.0f;

// Model
// Path within the assets directory where Live2D model files are placed.
const csmChar* ResourcesPath   = "live2d/";

// Motion groups
const csmChar* MotionGroupIdle      = "Idle";
const csmChar* MotionGroupTapBody   = "TapBody";

// Hit area names
const csmChar* HitAreaNameHead  = "Head";
const csmChar* HitAreaNameBody  = "Body";

// Priority constants
const csmInt32 PriorityNone     = 0;
const csmInt32 PriorityIdle     = 1;
const csmInt32 PriorityNormal   = 2;
const csmInt32 PriorityForce    = 3;

// Logging level for the CubismFramework
const csmInt32 CubismLoggingLevel = CSM_LOG_LEVEL_VERBOSE;

// Debug flags
const csmBool DebugLogEnable      = true;
const csmBool DebugTouchLogEnable = true;

} // namespace LAppDefine
