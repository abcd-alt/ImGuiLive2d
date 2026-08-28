LOCAL_PATH := $(call my-dir)
MAIN_LOCAL_PATH := $(call my-dir)


include $(CLEAR_VARS)
LOCAL_MODULE            := libdobby
LOCAL_SRC_FILES         := Dobby/libraries/$(TARGET_ARCH_ABI)/libdobby.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/Dobby/
include $(PREBUILT_STATIC_LIBRARY)

#Prebuilt Live2D Cubism Core static library
include $(CLEAR_VARS)
LOCAL_MODULE := Live2DCubismCore
LOCAL_SRC_FILES := Live2D/Core/lib/android/$(TARGET_ARCH_ABI)/libLive2DCubismCore.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/Live2D/Core/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := Tool
LOCAL_CFLAGS := -Wno-error=format-security -fvisibility=hidden -ffunction-sections -fdata-sections -w
LOCAL_CFLAGS += -fno-exceptions -fpermissive -frtti
LOCAL_CPPFLAGS := -Wno-error=format-security -fvisibility=hidden -ffunction-sections -fdata-sections -w -Werror -s -std=c++17
LOCAL_CPPFLAGS += -DCSM_TARGET_ANDROID_ES2
LOCAL_CPPFLAGS += -Wno-error=c++11-narrowing -fms-extensions -fno-exceptions -fpermissive
LOCAL_LDFLAGS += -Wl,--gc-sections,--strip-all, -llog
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(MAIN_LOCAL_PATH)
LOCAL_C_INCLUDES := $(MAIN_LOCAL_PATH)/ \
                    $(MAIN_LOCAL_PATH)/ImGui \
                    $(MAIN_LOCAL_PATH)/ImGui/backends \
                    $(MAIN_LOCAL_PATH)/Live2D \
                    $(MAIN_LOCAL_PATH)/Live2D/Framework \
                    $(MAIN_LOCAL_PATH)/Live2D/Framework/Rendering \
                    $(MAIN_LOCAL_PATH)/Live2D/Framework/Rendering/OpenGL \
                    $(MAIN_LOCAL_PATH)/Live2D/Core/include

LOCAL_SRC_FILES 		:=main.cpp \
        Il2cpp/xdl/xdl.c \
        Il2cpp/xdl/xdl_iterate.c \
        Il2cpp/xdl/xdl_linker.c \
        Il2cpp/xdl/xdl_lzma.c \
        Il2cpp/xdl/include/xdl.h \
        Il2cpp/xdl/xdl_util.c \
        Il2cpp/Il2Cpp.cpp\
        Helper/Tools.cpp\
        Helper/fake_dlfcn.cpp \
        Helper/ElfImg.cpp \
        base64/base64.cpp \
        Helper/plthook_elf.cpp  \
        Helper/android_native_app_glue.c \
        imgui/imgui.cpp    \
        imgui/imgui_draw.cpp\
        imgui/imgui_demo.cpp \
        imgui/imgui_tables.cpp \
        imgui/imgui_widgets.cpp \
        imgui/backends/imgui_impl_android.cpp \
        imgui/backends/imgui_impl_opengl3.cpp \
        Substrate/hde64.c \
        Substrate/SubstrateHook.cpp \
        Substrate/SubstrateDebug.cpp \
        Substrate/And64InlineHook.cpp \
        KittyMemory/KittyMemory.cpp \
        KittyMemory/MemoryPatch.cpp \
        KittyMemory/MemoryBackup.cpp \
        KittyMemory/KittyUtils.cpp \
        Live2D/LAppDefine.cpp \
        Live2D/LAppPal.cpp \
        Live2D/LAppAllocator.cpp \
        Live2D/Live2DManager.cpp \
        Live2D/Live2DModel.cpp \
        Live2D/TextureLoader.cpp \
        \
        Live2D/Framework/CubismFramework.cpp \
        Live2D/Framework/CubismCdiJson.cpp \
        Live2D/Framework/CubismDefaultParameterId.cpp \
        Live2D/Framework/CubismModelSettingJson.cpp \
        Live2D/Framework/Effect/CubismBreath.cpp \
        Live2D/Framework/Effect/CubismEyeBlink.cpp \
        Live2D/Framework/Effect/CubismLook.cpp \
        Live2D/Framework/Effect/CubismPose.cpp \
        Live2D/Framework/Id/CubismId.cpp \
        Live2D/Framework/Id/CubismIdManager.cpp \
        Live2D/Framework/Math/CubismMath.cpp \                     Live2D/Framework/Math/CubismMatrix44.cpp \
        Live2D/Framework/Math/CubismModelMatrix.cpp \
        Live2D/Framework/Math/CubismTargetPoint.cpp \
        Live2D/Framework/Math/CubismVector2.cpp \
        Live2D/Framework/Math/CubismViewMatrix.cpp \
        Live2D/Framework/Model/CubismMoc.cpp \
        Live2D/Framework/Model/CubismModel.cpp \
        Live2D/Framework/Model/CubismModelMultiplyAndScreenColor.cpp \
        Live2D/Framework/Model/CubismModelUserData.cpp \
        Live2D/Framework/Model/CubismModelUserDataJson.cpp \
        Live2D/Framework/Model/CubismUserModel.cpp \                  Live2D/Framework/Motion/ACubismMotion.cpp \
        Live2D/Framework/Motion/CubismBreathUpdater.cpp \
        Live2D/Framework/Motion/CubismExpressionMotion.cpp \
        Live2D/Framework/Motion/CubismExpressionMotionManager.cpp \
        Live2D/Framework/Motion/CubismExpressionUpdater.cpp \
        Live2D/Framework/Motion/CubismEyeBlinkUpdater.cpp \
        Live2D/Framework/Motion/CubismLipSyncUpdater.cpp \
        Live2D/Framework/Motion/CubismLookUpdater.cpp \
        Live2D/Framework/Motion/CubismMotion.cpp \
        Live2D/Framework/Motion/CubismMotionJson.cpp \
        Live2D/Framework/Motion/CubismMotionManager.cpp \
        Live2D/Framework/Motion/CubismMotionQueueEntry.cpp \
        Live2D/Framework/Motion/CubismMotionQueueManager.cpp \
        Live2D/Framework/Motion/CubismPhysicsUpdater.cpp \
        Live2D/Framework/Motion/CubismPoseUpdater.cpp \
        Live2D/Framework/Motion/CubismUpdateScheduler.cpp \
        Live2D/Framework/Motion/ICubismUpdater.cpp \
        Live2D/Framework/Motion/IParameterProvider.cpp \
        Live2D/Framework/Physics/CubismPhysics.cpp \
        Live2D/Framework/Physics/CubismPhysicsJson.cpp \
        Live2D/Framework/Rendering/CubismRenderer.cpp \
        Live2D/Framework/Rendering/csmBlendMode.cpp \
        Live2D/Framework/Rendering/OpenGL/CubismOffscreenManager_OpenGLES2.cpp \
        Live2D/Framework/Rendering/OpenGL/CubismOffscreenRenderTarget_OpenGLES2.cpp \
        Live2D/Framework/Rendering/OpenGL/CubismRenderTarget_OpenGLES2.cpp \
        Live2D/Framework/Rendering/OpenGL/CubismRenderer_OpenGLES2.cpp \
        Live2D/Framework/Rendering/OpenGL/CubismShader_OpenGLES2.cpp \
        Live2D/Framework/Type/csmRectF.cpp \
        Live2D/Framework/Type/csmString.cpp \
        Live2D/Framework/Utils/CubismDebug.cpp \
        Live2D/Framework/Utils/CubismJson.cpp \
        Live2D/Framework/Utils/CubismString.cpp        
        


LOCAL_CPP_FEATURES                      := exceptions
LOCAL_LDLIBS                            := -llog -landroid -lEGL -lGLESv2 -lGLESv3 -lGLESv1_CM -lz
LOCAL_STATIC_LIBRARIES := libdobby Live2DCubismCore
include $(BUILD_SHARED_LIBRARY)
