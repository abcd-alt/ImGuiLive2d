#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "Helper/Includes.h"
#include "Il2cpp/Il2Cpp.h"
#include "Helper/obfuscate.h"
#include "字体/字体.h"
#include "imgui/stb_image.cpp"
#define STB_IMAGE_IMPLEMENTATION
#include "配置/stb_image.h"
#include "Dobby/dobby.h"
#include "Helper/CPU.h"
#include "Unity/Vector3.hpp"
#include "Unity/Vector2.hpp"
#include "Unity/Rect.h"
#include "Unity/Quaternion.h"
#include "Unity/Color.h"
#include "Utils.h"
void ShowMenu(){
    ImGui::Begin("引言");
    ImGui::Text("此代码基于CoisiniLuaR开源的 \n ImGui对接live2d进行二次开发，\n 想要学习的可以前往下方视频地址下载基础 \n 源码【ImGui对接Live2D已开源-哔哩哔哩】\n https://b23.tv/mTJBrON");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("开源 bilibili： Debug小白");
    ImGui::Text("注：源码免费开源给大家，如果您是买的证明您被骗了");
    ImGui::Text("这个版本对角色动画和交互进行优化，\n 有能力的大佬可以继续优化");
    ImGui::End();
}
bool initImGui = false;
int screenWidth = 2400, screenHeight = 1080, glWidth, glHeight, gWidth, gHeight,px,py;


#define libName "libil2cpp.so"
#define PI 3.14159265358979323846f

// Live2D Integration
#include "Live2D/LAppPal.hpp"
#include "Live2D/Live2DManager.hpp"
#include "Live2D/Live2DModel.hpp"
#include "Live2D/LAppDefine.hpp"
#include <Math/CubismMatrix44.hpp>
#include <cstdlib>   // rand()
#include <cmath>     // exp / fabs / sinf


// ===================== 全局变量 =====================
static bool  g_Live2DEnabled    = true;
static bool  g_Live2DInited     = false;
static GLuint g_Live2DFBO       = 0;
static GLuint g_Live2DTexture   = 0;
static const int g_Live2DRenderSize = 2048;  // 必须和 CreateRenderer(2048,2048) 一致
static float g_Live2DZoom       = 1.11f;
// 视线追踪模式
enum EyeTrackMode {
    EYE_TRACK_OFF,
    EYE_TRACK_TOUCH      // 跟随触摸/鼠标
};
static EyeTrackMode g_EyeTrackMode = EYE_TRACK_TOUCH;  // 默认触摸跟随

// ---- 模型显示区域位置记录（触摸跟随的坐标基准） ----
static ImVec2 g_PreviewCenter = ImVec2(0.5f, 0.5f);   // 模型显示区域中心（归一化 0~1）
static ImVec2 g_PreviewSize   = ImVec2(480.0f, 560.0f);// 模型显示区域尺寸（像素，兜底值）
static bool   g_PreviewVisible = false;               // 预览窗口当前是否可见

// ---- 视角跟随优化：平滑跟踪（头/眼分离速度） ----
static float g_HeadX = 0.0f, g_HeadY = 0.0f;   // 当前头部角度（缓动）
static float g_EyeX  = 0.0f, g_EyeY  = 0.0f;   // 当前眼球位置（缓动）
static float g_HeadSpeed   = 5.0f;             // 头部转动速度（慢）
static float g_EyeSpeed    = 12.0f;            // 眼球移动速度（快）
static float g_ReturnSpeed = 3.0f;             // 无目标时回正速度
static float g_HeadGain    = 50.0f;            // 头部角度增益 ±50°
static float g_EyeGain     = 1.0f;             // 眼球位移增益

// ---- 扩展 Live2D 功能 ----
static bool  g_BlinkEnabled   = true;          // 自动眨眼
static float g_BlinkTimer     = 0.0f;          // 眨眼计时
static float g_BlinkInterval  = 2.5f;          // 平均眨眼间隔(秒)
static float g_BlinkPhase     = 0.0f;          // 当前闭眼程度 0~1
static bool  g_BreathEnabled  = true;          // 呼吸(身体起伏)
static float g_BreathTimer    = 0.0f;

// ---- 更多扩展功能 ----
static bool  g_MouthTalk      = false;         // 模拟说话（嘴巴开合，按住触发）
static bool  g_MouthTalking   = false;         // 当前是否在说话
static float g_MouthPhase     = 0.0f;          // 嘴巴相位
static float g_PressTimer     = 0.0f;          // 本次按下的持续时间（用于区分点击/长按说话）
static const float g_LongPressMs = 0.35f;      // 长按判定的阈值（秒）：超过则视为"说话"而非"点击"


// ============================================================
// GL 状态工具：hook 环境必须完整保存/恢复游戏 GL 状态
// ============================================================

// 渲染前清理上下文（防止游戏残留的 VAO/shader/纹理单元污染 Cubism）
static void Live2D_ResetGLState()
{
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);          // 关键：游戏可能留在别的 unit 上
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

struct GLStateBackup {
    GLint program, vao, fbo, viewport[4], scissorBox[4];
    GLint arrayBuffer, elemBuffer;
    GLint activeTexture, boundTex[8];
    GLboolean blend, depthTest, cullFace, scissor, stencil, depthMask;
    GLint blendSrcRGB, blendDstRGB, blendSrcA, blendDstA;
    GLint unpackAlignment, packAlignment;      // ← 新增
};

static void SaveGLState(GLStateBackup& s)
{
    glGetIntegerv(GL_CURRENT_PROGRAM, &s.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &s.vao);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s.fbo);
    glGetIntegerv(GL_VIEWPORT, s.viewport);
    glGetIntegerv(GL_SCISSOR_BOX, s.scissorBox);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s.arrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &s.elemBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &s.activeTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &s.unpackAlignment);
    glGetIntegerv(GL_PACK_ALIGNMENT, &s.packAlignment);
    
    for (int i = 0; i < 8; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &s.boundTex[i]);
    }
    glActiveTexture(s.activeTexture);

    s.blend     = glIsEnabled(GL_BLEND);
    s.depthTest = glIsEnabled(GL_DEPTH_TEST);
    s.cullFace  = glIsEnabled(GL_CULL_FACE);
    s.scissor   = glIsEnabled(GL_SCISSOR_TEST);
    s.stencil   = glIsEnabled(GL_STENCIL_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &s.depthMask);
    glGetIntegerv(GL_BLEND_SRC_RGB, &s.blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &s.blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &s.blendSrcA);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &s.blendDstA);
}

static void RestoreGLState(const GLStateBackup& s)
{
    glBindVertexArray(0);
    glUseProgram(s.program);
    glBindFramebuffer(GL_FRAMEBUFFER, s.fbo);
    glViewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
    glScissor(s.scissorBox[0], s.scissorBox[1], s.scissorBox[2], s.scissorBox[3]);

    for (int i = 0; i < 8; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, s.boundTex[i]);
    }
    glActiveTexture(s.activeTexture);

    glBindVertexArray(s.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s.arrayBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.elemBuffer);    
    
    if (s.blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (s.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (s.cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (s.scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (s.stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    glDepthMask(s.depthMask);
    glBlendFuncSeparate(s.blendSrcRGB, s.blendDstRGB, s.blendSrcA, s.blendDstA);
    glPixelStorei(GL_UNPACK_ALIGNMENT, s.unpackAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, s.packAlignment);
}

// ===================== FBO 创建/销毁 =====================
static void CreateLive2DRenderTarget()
{
    if (g_Live2DFBO != 0)
    {
        glDeleteFramebuffers(1, &g_Live2DFBO);
        glDeleteTextures(1, &g_Live2DTexture);
        g_Live2DFBO = 0;
        g_Live2DTexture = 0;
    }

    Live2D_ResetGLState();

    glGenTextures(1, &g_Live2DTexture);
    glBindTexture(GL_TEXTURE_2D, g_Live2DTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_Live2DRenderSize, g_Live2DRenderSize,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glGenFramebuffers(1, &g_Live2DFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_Live2DFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_Live2DTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        LOGD("Live2D FBO incomplete!");

    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
}

// ===================== Live2D 初始化 =====================
static void InitLive2D()
{
    GLStateBackup backup;
    SaveGLState(backup);              // 先快照，初始化和贴图上传也会改状态

    Live2D_ResetGLState();            // 清理干净再让 Cubism 上传贴图
    Live2DManager::GetInstance().Init();
    Live2DManager::GetInstance().LoadModel("Hiyori", "Hiyori.model3.json");
    CreateLive2DRenderTarget();
    g_Live2DInited = true;

    RestoreGLState(backup);           // 还给游戏
}

// ===================== Live2D ImGui 窗口 =====================
static void ShowLive2DWindows()
{
    // 控制面板
    {
        ImGui::Begin("Live2D 控制");
        ImGui::Checkbox("显示 Live2D", &g_Live2DEnabled);

        if (Live2DManager::GetInstance().IsInitialized())
        {
            ImGui::Text("状态：已初始化");
            ImGui::Text("模型加载: %d", Live2DManager::GetInstance().GetModelCount());

            if (ImGui::Button("播放怠速运动"))
            {
                Live2DModel* model = Live2DManager::GetInstance().GetModel();
                if (model) model->StartRandomMotion(LAppDefine::MotionGroupIdle, LAppDefine::PriorityIdle);
            }
            if (ImGui::Button("播放轻击体运动"))
            {
                Live2DModel* model = Live2DManager::GetInstance().GetModel();
                if (model) model->StartRandomMotion(LAppDefine::MotionGroupTapBody, LAppDefine::PriorityNormal);
            }
            if (ImGui::Button("随机表情"))
            {
                Live2DModel* model = Live2DManager::GetInstance().GetModel();
                if (model) model->SetRandomExpression();
            }
           const char* items[] = { "关闭", "触摸点跟随" };
           int current = (int)g_EyeTrackMode;
           if (ImGui::Combo("视线跟随模式", &current, items, IM_ARRAYSIZE(items))) {
               g_EyeTrackMode = (EyeTrackMode)current;
           }

           // 跟踪手感参数
           ImGui::Separator();
           ImGui::Text("跟踪手感:");
           ImGui::SliderFloat("头部速度", &g_HeadSpeed, 1.0f, 20.0f, "%.1f");
           ImGui::SliderFloat("眼球速度", &g_EyeSpeed, 1.0f, 30.0f, "%.1f");
           ImGui::SliderFloat("回正速度", &g_ReturnSpeed, 0.5f, 10.0f, "%.1f");
           ImGui::SliderFloat("头部幅度", &g_HeadGain, 10.0f, 80.0f, "%.0f°");

           // 扩展功能开关
           ImGui::Separator();
           ImGui::Text("扩展动画:");
           ImGui::Checkbox("自动眨眼", &g_BlinkEnabled);
           ImGui::Checkbox("呼吸起伏", &g_BreathEnabled);
           ImGui::Checkbox("按住说话", &g_MouthTalk);

            ImGui::SliderFloat("缩放", &g_Live2DZoom, 0.5f, 3.0f, "%.2fx");
                        
        }
        else
        {
            ImGui::Text("状态: 未初始化/初始化失败");
        }
        ImGui::End();
    }

    // 预览窗口
    if (g_Live2DEnabled && g_Live2DTexture != 0)
    {
        static bool show_live2d_preview = true;
        ImGui::SetNextWindowSize(ImVec2(480.0f, 560.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Live2D Preview", &show_live2d_preview);
        
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float img_size = avail.x < avail.y ? avail.x : avail.y;
        if (img_size < 16.0f) img_size = 16.0f;

        // 可交互的模型显示区：短按播放轻击体运动；长按（按住超过阈值）模拟说话，两者互斥
        ImGui::PushID("live2d_preview_img");
        bool hovered = ImGui::InvisibleButton("live2d_img_btn", ImVec2(img_size, img_size), 0);
        if (hovered) {
            ImGui::SetTooltip("点击：轻击体运动\n长按(>0.35s)：模拟说话");
        }

        // 绘制图片到按钮区域
        ImVec2 btnMin = ImGui::GetItemRectMin();
        ImVec2 btnMax = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)g_Live2DTexture,
                                             btnMin, btnMax,
                                             ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        // ===== 记录模型显示区域中心/尺寸（触摸跟随的坐标基准）=====
        // 触摸跟随要把"触摸点相对模型区域"的方向映射为视线方向：
        // 无论窗口在屏幕左/中/右，触摸点在模型左边就往左看、右边就往右看
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.DisplaySize.x > 0 && io.DisplaySize.y > 0) {
                ImVec2 btnCenter = ImVec2((btnMin.x + btnMax.x) * 0.5f,
                                          (btnMin.y + btnMax.y) * 0.5f);
                g_PreviewCenter = ImVec2(btnCenter.x / io.DisplaySize.x,
                                         btnCenter.y / io.DisplaySize.y);
                g_PreviewSize   = ImVec2(btnMax.x - btnMin.x,
                                         btnMax.y - btnMin.y);
            }
            g_PreviewVisible = true;
        }

        // ===== 互斥交互：短按=点击，长按=说话 =====
        ImGuiIO& pio = ImGui::GetIO();
        bool isItemActive = ImGui::IsItemActive();

        // 记录按住持续时间，并在松开瞬间保存"本次按了多久"用于判定点击/长按
        static float s_releasedDuration = 0.0f;  // 最近一次松开时按住的总时长
        if (isItemActive) {
            g_PressTimer += pio.DeltaTime;       // 按住累计
        } else {
            if (g_PressTimer > 0.0f)             // 松开瞬间记住本次时长
                s_releasedDuration = g_PressTimer;
            g_PressTimer = 0.0f;                 // 清零
        }

        // 长按判定：按住超过阈值 → 说话；未超过 → 松手时算"点击"
        bool longPressed = isItemActive && g_PressTimer >= g_LongPressMs;

        // 1) 说话（长按期间持续触发，由 g_MouthTalk 开关控制）
        g_MouthTalking = g_MouthTalk && longPressed;

        // 2) 点击（仅当"松手前的按住时长"是短按时才播放轻击体运动）
        //    - 松开瞬间：用刚保存的 s_releasedDuration 判断
        //    - 这样长按说话松开时不会误触发点击
        if (ImGui::IsItemClicked(0) &&
            s_releasedDuration <= g_LongPressMs) {
            Live2DModel* model = Live2DManager::GetInstance().GetModel();
            if (model) model->StartRandomMotion(LAppDefine::MotionGroupTapBody, LAppDefine::PriorityNormal);
        }
        ImGui::PopID();

        ImGui::End();
    }
}

// 更新模型的视线参数（平滑缓动 + 头/眼分离）
static void UpdateEyeTracking()
{
    if (!g_Live2DEnabled || !g_Live2DInited) return;
    Live2DModel* model = Live2DManager::GetInstance().GetModel();
    if (!model) return;

    ImGuiIO& io = ImGui::GetIO();
    float dt = LAppPal::GetDeltaTime();
    if (dt <= 0.0f) dt = 0.016f;

    // ===== 1. 计算目标 (targetX, targetY)，范围 -1~1 =====
    float targetX = 0.0f, targetY = 0.0f;
    bool hasTarget = false;

    switch (g_EyeTrackMode) {
        case EYE_TRACK_OFF:
            hasTarget = false;
            break;

        case EYE_TRACK_TOUCH: {
            // 触摸跟随：按下时，视线朝向"触摸点相对模型显示区域"的方向
            bool isTouching = io.MouseDown[0];
            if (isTouching && io.MousePos.x >= 0 && io.MousePos.y >= 0) {
                if (g_PreviewVisible && g_PreviewSize.x > 1.0f && g_PreviewSize.y > 1.0f) {
                    // 触摸点相对模型区域中心的偏移（以模型区域半宽/半高归一化 → -1~1）
                    float halfW = g_PreviewSize.x * 0.5f;
                    float halfH = g_PreviewSize.y * 0.5f;
                    float centerX = g_PreviewCenter.x * io.DisplaySize.x;
                    float centerY = g_PreviewCenter.y * io.DisplaySize.y;
                    targetX = (io.MousePos.x - centerX) / halfW;
                    targetY = -((io.MousePos.y - centerY) / halfH);
                    // 防止手指离模型太远时角度过大
                    if (targetX >  1.0f) targetX =  1.0f;
                    if (targetX < -1.0f) targetX = -1.0f;
                    if (targetY >  1.0f) targetY =  1.0f;
                    if (targetY < -1.0f) targetY = -1.0f;
                } else {
                    // 兜底：预览窗口不可见时退回全局归一化
                    targetX = (io.MousePos.x / io.DisplaySize.x) * 2.0f - 1.0f;
                    targetY = -((io.MousePos.y / io.DisplaySize.y) * 2.0f - 1.0f);
                }
                hasTarget = true;
            }
            break;
        }
    }

    // ===== 2. 缓动跟踪：头慢、眼快，分离运动 =====
    float lerpHead = 1.0f - exp(-g_HeadSpeed * dt);
    float lerpEye  = 1.0f - exp(-g_EyeSpeed  * dt);

    if (hasTarget) {
        // 有目标：朝目标平滑移动（头和眼速度不同，更自然）
        g_HeadX += (targetX - g_HeadX) * lerpHead;
        g_HeadY += (targetY - g_HeadY) * lerpHead;
        g_EyeX  += (targetX - g_EyeX)  * lerpEye;
        g_EyeY  += (targetY - g_EyeY)  * lerpEye;
    } else {
        // 无目标：缓慢回正到 (0,0)
        float lerpReturn = 1.0f - exp(-g_ReturnSpeed * dt);
        g_HeadX += (0.0f - g_HeadX) * lerpReturn;
        g_HeadY += (0.0f - g_HeadY) * lerpReturn;
        g_EyeX  += (0.0f - g_EyeX)  * lerpReturn;
        g_EyeY  += (0.0f - g_EyeY)  * lerpReturn;

        // 接近中心时归零防抖
        if (fabs(g_HeadX) < 0.002f && fabs(g_HeadY) < 0.002f) {
            g_HeadX = g_HeadY = 0.0f;
            g_EyeX  = g_EyeY  = 0.0f;
        }
    }

    // 轻微空闲摆动（让角色不呆板）
    static float idleTime = 0.0f;
    idleTime += dt;
    float idleSway = sinf(idleTime * 0.6f) * 0.04f; // 微弱头部摆动

    // ===== 3. 眨眼逻辑 =====
    if (g_BlinkEnabled) {
        g_BlinkTimer += dt;
        // 随机眨眼间隔
        if (g_BlinkTimer >= g_BlinkInterval) {
            g_BlinkTimer = 0.0f;
            g_BlinkInterval = 2.0f + (float)(rand() % 300) / 100.0f; // 2~5s
            g_BlinkPhase = 1.0f; // 触发一次眨眼
        }
        // 眨眼阶段：快闭慢开
        if (g_BlinkPhase > 0.0f) {
            g_BlinkPhase -= dt * 10.0f; // 眨眼速度
            if (g_BlinkPhase < 0.0f) g_BlinkPhase = 0.0f;
        }
    } else {
        g_BlinkPhase = 0.0f;
    }
    // 眨眼程度：0=全开，1=全闭（用 sin 曲线，先闭后开）
    float blink = g_BlinkPhase > 0.0f ? sinf(g_BlinkPhase * PI) : 0.0f;
    if (blink < 0.0f) blink = 0.0f;

    // ===== 4. 呼吸起伏 =====
    float breath = 0.0f;
    if (g_BreathEnabled) {
        g_BreathTimer += dt;
        breath = sinf(g_BreathTimer * 1.8f) * 0.05f; // 身体轻微起伏
    }

    // ===== 4.5 说话（嘴巴开合）=====
    float mouth = 0.0f;
    if (g_MouthTalking) {
        g_MouthPhase += dt * 12.0f;   // 说话频率
        // 模拟说话时嘴巴随机开合（更像真人说话）
        mouth = fabs(sinf(g_MouthPhase)) * 0.6f + (float)(rand() % 40) / 100.0f * 0.4f;
        if (mouth > 1.0f) mouth = 1.0f;
    } else {
        g_MouthPhase = 0.0f;
        mouth = 0.0f;
    }

    // ===== 5. 应用到模型参数（头/眼分离 + 扩展动画） =====
    float angleX = (g_HeadX + idleSway) * g_HeadGain;
    float angleY = g_HeadY * g_HeadGain;
    float eyeX   = g_EyeX * g_EyeGain;
    float eyeY   = g_EyeY * g_EyeGain;

    model->SetParameterValue("ParamAngleX", angleX);
    model->SetParameterValue("ParamAngleY", angleY);
    model->SetParameterValue("ParamEyeBallX", eyeX);
    model->SetParameterValue("ParamEyeBallY", eyeY);

    // 眨眼参数（不同模型参数名可能不同，尽量都设置，无副作用）
    model->SetParameterValue("ParamEyeLOpen", 1.0f - blink);
    model->SetParameterValue("ParamEyeROpen", 1.0f - blink);

    // 呼吸：身体摆动（部分模型有，无副作用）
    model->SetParameterValue("ParamBodyAngleZ", breath * 30.0f);
    model->SetParameterValue("ParamBreath", breath * 8.0f);

    // 说话：嘴巴开合（不同模型参数名可能不同，尽量都设置）
    model->SetParameterValue("ParamMouthOpenY", mouth);
    model->SetParameterValue("ParamMouthForm", mouth * 0.5f);
}

// ===================== Live2D 渲染到 FBO =====================
static void RenderLive2DToFBO()
{
    if (!g_Live2DEnabled || !g_Live2DInited) return;
    if (!Live2DManager::GetInstance().IsInitialized()) return;
    if (Live2DManager::GetInstance().GetModelCount() == 0 || g_Live2DFBO == 0) return;

    static GLStateBackup backup;
    SaveGLState(backup);            // 1. 完整快照

    Live2D_ResetGLState();          // 2. 清理后给 Live2D 用

    glBindFramebuffer(GL_FRAMEBUFFER, g_Live2DFBO);
    glViewport(0, 0, g_Live2DRenderSize, g_Live2DRenderSize);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    Csm::CubismMatrix44 projection;
    projection.Scale(g_Live2DZoom, g_Live2DZoom);
    Live2DManager::GetInstance().Draw(projection);

    RestoreGLState(backup);         // 3. 原样还给游戏
}

EGLBoolean (*old_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);
    if (glWidth <= 0 || glHeight <= 0)
        return hook_eglSwapBuffers(dpy, surface);

    if (!initImGui) {
        initImGui = true;
        px = glWidth / 2;
        py = glHeight / 2;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        ImGuiStyle *style = &ImGui::GetStyle();
        io.IniFilename = NULL;
        ImGui_ImplOpenGL3_Init("#version 300 es");
        io.Fonts->AddFontFromMemoryTTF((void*)字体_data, 字体_size, 20.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
        ImGui::StyleColorsClassic();
        style->GrabMinSize = 13.0f;
        style->ScrollbarRounding = 2.0f;
        style->ScrollbarSize = 13.0f;
        style->WindowRounding = 8.0f;
        style->WindowTitleAlign = ImVec2(0.5, 0.5);
        ImGui::GetStyle().ScaleAllSizes(2.0f);
        InitLive2D();          // ← Live2D 初始化（含状态保存/恢复）
    }

    ImGuiIO &io = ImGui::GetIO();
    // 更新 Live2D 动画（纯 CPU 计算，不碰 GL）
    if (g_Live2DEnabled && g_Live2DInited && Live2DManager::GetInstance().IsInitialized())
        Live2DManager::GetInstance().Update();
    // 视线跟随 / 眨眼 / 呼吸 / 说话：必须在动画更新后覆盖，保证控制值生效
    UpdateEyeTracking();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(glWidth, glHeight);
    ImGui::NewFrame();

    ShowMenu();

    ShowLive2DWindows();

    ImGui::Render();
    RenderLive2DToFBO();       // ← 含完整 GL 状态保存/恢复

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return old_eglSwapBuffers(dpy, surface);
}

void (*old_Input)(void *thiz, void *ex_ab, void *ex_ac);
void hook_Input(void *thiz, void *ex_ab, void *ex_ac) {
    old_Input(thiz, ex_ab, ex_ac);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *) thiz, {(float)screenWidth / (float) glWidth, (float) screenHeight / (float) glHeight});
    return;
}

int (*old_getWidth)(ANativeWindow* window);
int hook_getWidth(ANativeWindow* window) {
    screenWidth = old_getWidth(window);
    return old_getWidth(window);
}

int (*old_getHeight)(ANativeWindow* window);
int hook_getHeight(ANativeWindow* window) {
    screenHeight = old_getHeight(window);
    return old_getHeight(window);
}

void *main_thread(void *) {
     do {
        sleep(1);
    } while (!isLibraryLoaded(libName));

    auto p_eglSwapBuffers = (uintptr_t)dlsym(RTLD_NEXT, "eglSwapBuffers");
    DobbyHook((void *)p_eglSwapBuffers, (void *)hook_eglSwapBuffers, (void **)&old_eglSwapBuffers);
    void *sym_input = DobbySymbolResolver("/system/lib/libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");
    if (NULL != sym_input) {
        DobbyHook((void *)sym_input, (void *) hook_Input, (void **)&old_Input);
    }
    DobbyHook((void *) dlsym(dlopen("libandroid.so", 4), "ANativeWindow_getWidth"), (void *) hook_getWidth, (void **) &old_getWidth);
    DobbyHook((void *) dlsym(dlopen("libandroid.so", 4), "ANativeWindow_getHeight"), (void *) hook_getHeight, (void **) &old_getHeight);

    Il2CppAttach();


    return 0;
}
__attribute__((constructor)) void _init() {
    pthread_t t;
    pthread_create(&t, 0, main_thread, 0);
}
