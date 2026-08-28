/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <CubismFramework.hpp>

/**
 * @brief  Sample App side definitions.
 *
 * This is a simplified version of the Live2D sample's LAppDefine,
 * adapted for the Android ImGui integration.
 */
namespace LAppDefine {

using namespace Csm;

//-- View --
extern const csmFloat32 ViewScale;
extern const csmFloat32 ViewMaxX;
extern const csmFloat32 ViewMaxY;
extern const csmFloat32 ViewMinX;
extern const csmFloat32 ViewMinY;

//-- Model --
extern const csmChar* ResourcesPath;

//-- Motion --
extern const csmChar* MotionGroupIdle;
extern const csmChar* MotionGroupTapBody;

//-- HitArea --
extern const csmChar* HitAreaNameHead;
extern const csmChar* HitAreaNameBody;

//-- Priority --
extern const csmInt32 PriorityNone;
extern const csmInt32 PriorityIdle;
extern const csmInt32 PriorityNormal;
extern const csmInt32 PriorityForce;

//-- Logging --
extern const csmInt32 CubismLoggingLevel;

//-- Debug --
extern const csmBool DebugLogEnable;
extern const csmBool DebugTouchLogEnable;

} // namespace LAppDefine
