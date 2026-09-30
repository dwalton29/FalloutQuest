#include "fo3-interaction-hud-runtime.h"
#include "fo3-door-prompt-q1840.h"
#include "fo3-interaction-hud-q1840.h"
#include "fo3-interaction-hud-q1850.h"
#include "fo3-interaction-hud-q1880.h"
#include <array>
#include <string>
#include "fo3-loading-vr-q1780.h"
#include "fo3-loading-vanilla-q1760.h"
#include "fo3-loading-comfort-q1720.h"
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include "fo3-collision-overlay.h"
#include "fo3-megaton-scene.h"
#include "fo3-environment-q1000.h"
#include "fo3-weather-sky-q1330.h"
#include "fo3-pc-sky-q1660.h"
#include "fo3-weather-sky-color-q1390.h"
#include "fo3-visual-depth-q1010.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>
#include "fo3-fog-ab-q1450.h"
#include "fo3-render-stage-q1560.h"
#include "fo3-legacy-colour-domain-q1570.h"
#include "fo3-pplighting-domain-q1470.h"

// Q16.14: NativeActivity event bridge for long synchronous Fallout asset work.
// External linkage lets the renderer/collision TUs service Android without
// moving or rewriting any of their authored scene-building behaviour.
static android_app* gFo3AndroidAppQ1860 = nullptr;

void SetFo3AndroidAppQ1860(android_app* app) {
    gFo3AndroidAppQ1860 = app;
}

void PumpFo3AndroidEventsQ1860() {
    android_app* app = gFo3AndroidAppQ1860;
    if (!app) return;

    android_poll_source* source = nullptr;
    int events = 0;
    int processed = 0;
    while (ALooper_pollOnce(0, nullptr, &events,
                            reinterpret_cast<void**>(&source)) >= 0) {
        if (source) {
            source->process(app, source);
            ++processed;
        }
        if (app->destroyRequested) break;
        source = nullptr;
    }

    static uint64_t pumpCalls = 0u;
    static uint64_t processedEvents = 0u;
    ++pumpCalls;
    processedEvents += static_cast<uint64_t>(processed);
    if (processed > 0) {
        __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                            "Q16.14 ANDROID EVENT PUMP: call=%llu processed=%d totalEvents=%llu destroy=%d",
                            static_cast<unsigned long long>(pumpCalls), processed,
                            static_cast<unsigned long long>(processedEvents),
                            app->destroyRequested ? 1 : 0);
    }
}

namespace {

constexpr const char* TAG = "FalloutQuest";
constexpr float PI = 3.14159265358979323846f;
constexpr float SNAP_TURN_RADIANS = 30.0f * PI / 180.0f;

// Q7.15: Fallout 3's own default movement settings, expressed in the same
// 70 game-units-per-metre scale used by the FalloutQuest world renderer.
constexpr float FO3_UNITS_PER_METRE_Q715 = 70.0f;
constexpr float FO3_MOVE_BASE_SPEED_Q715 = 77.0f;
constexpr float FO3_MOVE_RUN_MULT_Q715 = 4.0f;
constexpr float FO3_MOVE_NO_WEAPON_MULT_Q715 = 1.1f;
constexpr float MOVE_SPEED_METRES_PER_SECOND =
    (FO3_MOVE_BASE_SPEED_Q715 * FO3_MOVE_RUN_MULT_Q715 *
     FO3_MOVE_NO_WEAPON_MULT_Q715) / FO3_UNITS_PER_METRE_Q715;

#define FQ_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define FQ_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

bool CheckXr(XrResult result, const char* call) {
    if (XR_FAILED(result)) {
        FQ_LOGE("%s failed: XrResult %d", call, static_cast<int>(result));
        return false;
    }
    return true;
}

float Deadzone(float value, float threshold = 0.18f) {
    const float magnitude = std::fabs(value);
    if (magnitude <= threshold) return 0.0f;
    const float scaled = (magnitude - threshold) / (1.0f - threshold);
    return std::copysign(std::min(scaled, 1.0f), value);
}

void RadialMoveDeadzoneQ715(float rawX, float rawY, float& outX, float& outY,
                            float threshold = 0.18f) {
    const float magnitude = std::sqrt(rawX * rawX + rawY * rawY);
    if (magnitude <= threshold) {
        outX = 0.0f;
        outY = 0.0f;
        return;
    }
    const float clampedMagnitude = std::min(magnitude, 1.0f);
    const float scaledMagnitude = (clampedMagnitude - threshold) / (1.0f - threshold);
    const float invMagnitude = 1.0f / std::max(magnitude, 1e-6f);
    outX = rawX * invMagnitude * scaledMagnitude;
    outY = rawY * invMagnitude * scaledMagnitude;
}

struct Mat4 {
    float m[16]{};
};

Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out.m[c * 4 + r] =
                a.m[0 * 4 + r] * b.m[c * 4 + 0] +
                a.m[1 * 4 + r] * b.m[c * 4 + 1] +
                a.m[2 * 4 + r] * b.m[c * 4 + 2] +
                a.m[3 * 4 + r] * b.m[c * 4 + 3];
        }
    }
    return out;
}

XrQuaternionf QuaternionMultiply(const XrQuaternionf& a, const XrQuaternionf& b) {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

XrQuaternionf YawQuaternion(float yaw) {
    const float half = yaw * 0.5f;
    return {0.0f, std::sin(half), 0.0f, std::cos(half)};
}

XrVector3f RotateYaw(const XrVector3f& p, float yaw) {
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    return {
        c * p.x + s * p.z,
        p.y,
        -s * p.x + c * p.z,
    };
}

float YawFromQuaternion(const XrQuaternionf& q) {
    return std::atan2(2.0f * (q.w * q.y + q.x * q.z),
                      1.0f - 2.0f * (q.y * q.y + q.z * q.z));
}

Mat4 MatrixFromPose(const XrPosef& pose) {
    const float x = pose.orientation.x;
    const float y = pose.orientation.y;
    const float z = pose.orientation.z;
    const float w = pose.orientation.w;

    Mat4 m{};
    m.m[0] = 1.0f - 2.0f * (y * y + z * z);
    m.m[1] = 2.0f * (x * y + z * w);
    m.m[2] = 2.0f * (x * z - y * w);
    m.m[3] = 0.0f;

    m.m[4] = 2.0f * (x * y - z * w);
    m.m[5] = 1.0f - 2.0f * (x * x + z * z);
    m.m[6] = 2.0f * (y * z + x * w);
    m.m[7] = 0.0f;

    m.m[8] = 2.0f * (x * z + y * w);
    m.m[9] = 2.0f * (y * z - x * w);
    m.m[10] = 1.0f - 2.0f * (x * x + y * y);
    m.m[11] = 0.0f;

    m.m[12] = pose.position.x;
    m.m[13] = pose.position.y;
    m.m[14] = pose.position.z;
    m.m[15] = 1.0f;
    return m;
}

Mat4 ViewFromPose(const XrPosef& pose) {
    const float x = -pose.orientation.x;
    const float y = -pose.orientation.y;
    const float z = -pose.orientation.z;
    const float w = pose.orientation.w;

    Mat4 v{};
    v.m[0] = 1.0f - 2.0f * (y * y + z * z);
    v.m[1] = 2.0f * (x * y + z * w);
    v.m[2] = 2.0f * (x * z - y * w);
    v.m[3] = 0.0f;

    v.m[4] = 2.0f * (x * y - z * w);
    v.m[5] = 1.0f - 2.0f * (x * x + z * z);
    v.m[6] = 2.0f * (y * z + x * w);
    v.m[7] = 0.0f;

    v.m[8] = 2.0f * (x * z + y * w);
    v.m[9] = 2.0f * (y * z - x * w);
    v.m[10] = 1.0f - 2.0f * (x * x + y * y);
    v.m[11] = 0.0f;

    const float px = pose.position.x;
    const float py = pose.position.y;
    const float pz = pose.position.z;
    v.m[12] = -(v.m[0] * px + v.m[4] * py + v.m[8] * pz);
    v.m[13] = -(v.m[1] * px + v.m[5] * py + v.m[9] * pz);
    v.m[14] = -(v.m[2] * px + v.m[6] * py + v.m[10] * pz);
    v.m[15] = 1.0f;
    return v;
}

Mat4 ProjectionFromFov(const XrFovf& fov, float nearZ, float farZ) {
    const float tanLeft = std::tan(fov.angleLeft);
    const float tanRight = std::tan(fov.angleRight);
    const float tanDown = std::tan(fov.angleDown);
    const float tanUp = std::tan(fov.angleUp);
    const float tanWidth = tanRight - tanLeft;
    const float tanHeight = tanUp - tanDown;

    Mat4 p{};
    p.m[0] = 2.0f / tanWidth;
    p.m[5] = 2.0f / tanHeight;
    p.m[8] = (tanRight + tanLeft) / tanWidth;
    p.m[9] = (tanUp + tanDown) / tanHeight;
    p.m[10] = -(farZ + nearZ) / (farZ - nearZ);
    p.m[11] = -1.0f;
    p.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    return p;
}

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        FQ_LOGE("Shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateProgram() {
    static const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        uniform mat4 uMvp;
        void main() {
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";

    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;
        uniform vec3 uColor;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(uColor, 1.0);
        }
    )";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        FQ_LOGE("Program link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

struct EyeSwapchain {
    XrSwapchain handle{XR_NULL_HANDLE};
    int32_t width{0};
    int32_t height{0};
    std::vector<XrSwapchainImageOpenGLESKHR> images;
};

class FalloutQuestXr {
public:
    explicit FalloutQuestXr(android_app* app) : app_(app) {
        SetFo3AndroidAppQ1860(app);
    }


    bool Initialize() {
        FQ_LOGI("Q7.1 BOOT STEP: InitializeLoader");
        if (!InitializeLoader()) return false;
        FQ_LOGI("Q7.1 BOOT STEP: CreateInstance");
        if (!CreateInstance()) return false;
        FQ_LOGI("Q7.1 BOOT STEP: CreateSystem");
        if (!CreateSystem()) return false;
        FQ_LOGI("Q7.1 BOOT STEP: CreateEgl");
        if (!CreateEgl()) return false;
        FQ_LOGI("Q7.1 BOOT STEP: CreateSession");
        if (!CreateSession()) return false;
        FQ_LOGI("Q7.1 BOOT STEP: CreateInputActions");
        if (!CreateInputActions()) return false;
        FQ_LOGI("Q7.1 BOOT STEP: CreateReferenceSpace");
        if (!CreateReferenceSpace()) return false;
        FQ_LOGI("Q7.1 BOOT STEP: CreateSwapchains");
        if (!CreateSwapchains()) return false;
        FQ_LOGI("Q7.1 BOOT STEP: CreateSceneRenderer");
        if (!CreateSceneRenderer()) return false;

        FQ_LOGI("Q7.15 MOVEMENT: base=%.1f runMult=%.2f noWeaponMult=%.2f unitsPerMetre=%.1f fullStick=%.3fm/s radialAnalog=1",
                FO3_MOVE_BASE_SPEED_Q715, FO3_MOVE_RUN_MULT_Q715,
                FO3_MOVE_NO_WEAPON_MULT_Q715, FO3_UNITS_PER_METRE_Q715,
                MOVE_SPEED_METRES_PER_SECOND);
        FQ_LOGI("Q7.1 READY: Q6K runtime + isolated right-trigger door probe");
        return true;
    }

    void Run() {
        while (!exitRequested_ && !app_->destroyRequested) {
            ProcessAndroidEvents();
            ProcessXrEvents();
            if (sessionRunning_) RenderFrame();
        }
    }

    void Shutdown() {
        if (sessionRunning_) {
            xrEndSession(session_);
            sessionRunning_ = false;
        }

        if (program_) glDeleteProgram(program_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (vao_) glDeleteVertexArrays(1, &vao_);
        if (framebuffer_) glDeleteFramebuffers(1, &framebuffer_);

        for (XrSpace space : handSpaces_) {
            if (space != XR_NULL_HANDLE) xrDestroySpace(space);
        }
        for (auto& eye : eyes_) {
            if (eye.handle != XR_NULL_HANDLE) xrDestroySwapchain(eye.handle);
        }
        if (localSpace_ != XR_NULL_HANDLE) xrDestroySpace(localSpace_);
        if (session_ != XR_NULL_HANDLE) xrDestroySession(session_);
        if (actionSet_ != XR_NULL_HANDLE) xrDestroyActionSet(actionSet_);
        if (instance_ != XR_NULL_HANDLE) xrDestroyInstance(instance_);

        if (eglDisplay_ != EGL_NO_DISPLAY) {
            eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (eglContext_ != EGL_NO_CONTEXT) eglDestroyContext(eglDisplay_, eglContext_);
            if (eglSurface_ != EGL_NO_SURFACE) eglDestroySurface(eglDisplay_, eglSurface_);
            eglTerminate(eglDisplay_);
        }
    }

private:
    bool InitializeLoader() {
        PFN_xrVoidFunction fn = nullptr;
        if (!CheckXr(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", &fn),
                     "xrGetInstanceProcAddr(xrInitializeLoaderKHR)")) return false;
        auto initializeLoader = reinterpret_cast<PFN_xrInitializeLoaderKHR>(fn);
        if (!initializeLoader) return false;

        XrLoaderInitInfoAndroidKHR info{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        info.applicationVM = app_->activity->vm;
        info.applicationContext = app_->activity->clazz;
        return CheckXr(initializeLoader(reinterpret_cast<XrLoaderInitInfoBaseHeaderKHR*>(&info)),
                       "xrInitializeLoaderKHR");
    }

    bool CreateInstance() {
        const char* extensions[] = {
            XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
            XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
        };

        XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
        androidInfo.applicationVM = app_->activity->vm;
        androidInfo.applicationActivity = app_->activity->clazz;

        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        info.next = &androidInfo;
        std::strncpy(info.applicationInfo.applicationName, "FalloutQuest", XR_MAX_APPLICATION_NAME_SIZE - 1);
        info.applicationInfo.applicationVersion = 4;
        std::strncpy(info.applicationInfo.engineName, "FalloutQuestNative", XR_MAX_ENGINE_NAME_SIZE - 1);
        info.applicationInfo.engineVersion = 1;
        info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
        info.enabledExtensionCount = 2;
        info.enabledExtensionNames = extensions;

        if (!CheckXr(xrCreateInstance(&info, &instance_), "xrCreateInstance")) return false;

        XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
        if (CheckXr(xrGetInstanceProperties(instance_, &props), "xrGetInstanceProperties")) {
            FQ_LOGI("OpenXR runtime: %s", props.runtimeName);
        }
        return true;
    }

    bool CreateSystem() {
        XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
        info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        return CheckXr(xrGetSystem(instance_, &info, &systemId_), "xrGetSystem");
    }

    bool CreateEgl() {
        PFN_xrVoidFunction fn = nullptr;
        if (!CheckXr(xrGetInstanceProcAddr(instance_, "xrGetOpenGLESGraphicsRequirementsKHR", &fn),
                     "xrGetInstanceProcAddr(xrGetOpenGLESGraphicsRequirementsKHR)")) return false;
        auto getRequirements = reinterpret_cast<PFN_xrGetOpenGLESGraphicsRequirementsKHR>(fn);
        XrGraphicsRequirementsOpenGLESKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        if (!CheckXr(getRequirements(instance_, systemId_, &requirements),
                     "xrGetOpenGLESGraphicsRequirementsKHR")) return false;

        eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eglDisplay_ == EGL_NO_DISPLAY) return false;
        EGLint major = 0, minor = 0;
        if (!eglInitialize(eglDisplay_, &major, &minor)) return false;
        eglBindAPI(EGL_OPENGL_ES_API);

        const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        EGLint configCount = 0;
        if (!eglChooseConfig(eglDisplay_, configAttribs, &eglConfig_, 1, &configCount) || configCount < 1) return false;

        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        eglContext_ = eglCreateContext(eglDisplay_, eglConfig_, EGL_NO_CONTEXT, contextAttribs);
        if (eglContext_ == EGL_NO_CONTEXT) return false;

        const EGLint pbufferAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
        eglSurface_ = eglCreatePbufferSurface(eglDisplay_, eglConfig_, pbufferAttribs);
        if (eglSurface_ == EGL_NO_SURFACE) return false;
        if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) return false;

        FQ_LOGI("OpenGL ES renderer: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        return true;
    }

    bool CreateSession() {
        XrGraphicsBindingOpenGLESAndroidKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
        binding.display = eglDisplay_;
        binding.config = eglConfig_;
        binding.context = eglContext_;

        XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
        info.next = &binding;
        info.systemId = systemId_;
        return CheckXr(xrCreateSession(instance_, &info, &session_), "xrCreateSession");
    }

    bool CreateAction(const char* name, const char* localizedName, XrActionType type,
                      uint32_t subactionCount, const XrPath* subactionPaths, XrAction* action) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
        info.actionType = type;
        std::strncpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
        std::strncpy(info.localizedActionName, localizedName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        info.countSubactionPaths = subactionCount;
        info.subactionPaths = subactionPaths;
        return CheckXr(xrCreateAction(actionSet_, &info, action), "xrCreateAction");
    }

    bool Path(const char* text, XrPath* path) {
        return CheckXr(xrStringToPath(instance_, text, path), text);
    }

    bool CreateInputActions() {
        if (!Path("/user/hand/left", &handPaths_[0]) ||
            !Path("/user/hand/right", &handPaths_[1])) return false;

        XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strncpy(setInfo.actionSetName, "gameplay", XR_MAX_ACTION_NAME_SIZE - 1);
        std::strncpy(setInfo.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        setInfo.priority = 0;
        if (!CheckXr(xrCreateActionSet(instance_, &setInfo, &actionSet_), "xrCreateActionSet")) return false;

        if (!CreateAction("hand_pose", "Hand Pose", XR_ACTION_TYPE_POSE_INPUT,
                          2, handPaths_.data(), &poseAction_)) return false;
        if (!CreateAction("move_x", "Move X", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &moveXAction_)) return false;
        if (!CreateAction("move_y", "Move Y", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &moveYAction_)) return false;
        if (!CreateAction("turn_x", "Turn X", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[1], &turnXAction_)) return false;
        if (!CreateAction("activate_q160", "Activate", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[1], &activateAction_)) return false;
        if (!CreateAction("time_advance", "Advance Time", XR_ACTION_TYPE_FLOAT_INPUT,
                          1, &handPaths_[0], &timeAdvanceAction_)) return false;
        if (!CreateAction("fog_toggle", "Fog Toggle", XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[0], &fogToggleAction_)) return false;
        if (!CreateAction("pplighting_domain_toggle", "PPLighting Domain Toggle",
                          XR_ACTION_TYPE_BOOLEAN_INPUT,
                          1, &handPaths_[0], &ppDomainToggleAction_)) return false;

        XrPath profile = XR_NULL_PATH;
        XrPath leftAim = XR_NULL_PATH;
        XrPath rightAim = XR_NULL_PATH;
        XrPath leftStickX = XR_NULL_PATH;
        XrPath leftStickY = XR_NULL_PATH;
        XrPath rightStickX = XR_NULL_PATH;
        XrPath rightA = XR_NULL_PATH;
        XrPath leftTimeTriggerValue = XR_NULL_PATH;
        XrPath leftFogX = XR_NULL_PATH;
        XrPath leftPpDomainY = XR_NULL_PATH;
        if (!Path("/interaction_profiles/oculus/touch_controller", &profile) ||
            !Path("/user/hand/left/input/aim/pose", &leftAim) ||
            !Path("/user/hand/right/input/aim/pose", &rightAim) ||
            !Path("/user/hand/left/input/thumbstick/x", &leftStickX) ||
            !Path("/user/hand/left/input/thumbstick/y", &leftStickY) ||
            !Path("/user/hand/right/input/thumbstick/x", &rightStickX) ||
            !Path("/user/hand/right/input/a/click", &rightA) ||
            !Path("/user/hand/left/input/trigger/value", &leftTimeTriggerValue) ||
            !Path("/user/hand/left/input/x/click", &leftFogX) ||
            !Path("/user/hand/left/input/y/click", &leftPpDomainY)) return false;

        const std::array<XrActionSuggestedBinding, 9> bindings{{
            {poseAction_, leftAim},
            {poseAction_, rightAim},
            {moveXAction_, leftStickX},
            {moveYAction_, leftStickY},
            {turnXAction_, rightStickX},
            {activateAction_, rightA},
            {timeAdvanceAction_, leftTimeTriggerValue},
            {fogToggleAction_, leftFogX},
            {ppDomainToggleAction_, leftPpDomainY},
        }};
        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile = profile;
        suggested.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
        suggested.suggestedBindings = bindings.data();
        if (!CheckXr(xrSuggestInteractionProfileBindings(instance_, &suggested),
                     "xrSuggestInteractionProfileBindings(Touch)")) return false;

        for (uint32_t hand = 0; hand < 2; ++hand) {
            XrActionSpaceCreateInfo spaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            spaceInfo.action = poseAction_;
            spaceInfo.subactionPath = handPaths_[hand];
            spaceInfo.poseInActionSpace.orientation.w = 1.0f;
            if (!CheckXr(xrCreateActionSpace(session_, &spaceInfo, &handSpaces_[hand]),
                         "xrCreateActionSpace")) return false;
        }

        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attach.countActionSets = 1;
        attach.actionSets = &actionSet_;
        if (!CheckXr(xrAttachSessionActionSets(session_, &attach), "xrAttachSessionActionSets")) return false;

        FQ_LOGI("Q16.1 Touch bindings attached: Q6K controls unchanged + right A/click activate");
        return true;
    }

    bool CreateReferenceSpace() {
        XrReferenceSpaceCreateInfo info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        info.poseInReferenceSpace.orientation.w = 1.0f;
        return CheckXr(xrCreateReferenceSpace(session_, &info, &localSpace_), "xrCreateReferenceSpace");
    }

    bool CreateSwapchains() {
        uint32_t viewCount = 0;
        if (!CheckXr(xrEnumerateViewConfigurationViews(instance_, systemId_,
                        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr),
                     "xrEnumerateViewConfigurationViews(count)")) return false;
        if (viewCount != 2) return false;

        std::array<XrViewConfigurationView, 2> configViews{
            XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW},
            XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW}
        };
        if (!CheckXr(xrEnumerateViewConfigurationViews(instance_, systemId_,
                        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount,
                        configViews.data()), "xrEnumerateViewConfigurationViews")) return false;

        uint32_t formatCount = 0;
        if (!CheckXr(xrEnumerateSwapchainFormats(session_, 0, &formatCount, nullptr),
                     "xrEnumerateSwapchainFormats(count)")) return false;
        std::vector<int64_t> formats(formatCount);
        if (!CheckXr(xrEnumerateSwapchainFormats(session_, formatCount, &formatCount, formats.data()),
                     "xrEnumerateSwapchainFormats")) return false;

        int64_t chosenFormat = formats.empty() ? 0 : formats.front();
        for (int64_t format : formats) {
            if (format == GL_SRGB8_ALPHA8) {
                chosenFormat = format;
                break;
            }
            if (format == GL_RGBA8) chosenFormat = format;
        }
        if (!chosenFormat) return false;

        for (uint32_t i = 0; i < 2; ++i) {
            EyeSwapchain& eye = eyes_[i];
            eye.width = static_cast<int32_t>(configViews[i].recommendedImageRectWidth);
            eye.height = static_cast<int32_t>(configViews[i].recommendedImageRectHeight);

            XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            info.format = chosenFormat;
            info.sampleCount = 1;
            info.width = eye.width;
            info.height = eye.height;
            info.faceCount = 1;
            info.arraySize = 1;
            info.mipCount = 1;
            if (!CheckXr(xrCreateSwapchain(session_, &info, &eye.handle), "xrCreateSwapchain")) return false;

            uint32_t imageCount = 0;
            if (!CheckXr(xrEnumerateSwapchainImages(eye.handle, 0, &imageCount, nullptr),
                         "xrEnumerateSwapchainImages(count)")) return false;
            eye.images.resize(imageCount);
            for (auto& image : eye.images) image = {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR};
            if (!CheckXr(xrEnumerateSwapchainImages(
                            eye.handle, imageCount, &imageCount,
                            reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())),
                         "xrEnumerateSwapchainImages")) return false;
        }

        uint32_t blendCount = 0;
        if (CheckXr(xrEnumerateEnvironmentBlendModes(instance_, systemId_,
                        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &blendCount, nullptr),
                    "xrEnumerateEnvironmentBlendModes(count)") && blendCount > 0) {
            std::vector<XrEnvironmentBlendMode> modes(blendCount);
            if (CheckXr(xrEnumerateEnvironmentBlendModes(instance_, systemId_,
                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, blendCount, &blendCount, modes.data()),
                        "xrEnumerateEnvironmentBlendModes")) {
                blendMode_ = modes.front();
                for (auto mode : modes) if (mode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) blendMode_ = mode;
            }
        }
        return true;
    }

    bool CreateSceneRenderer() {
        program_ = CreateProgram();
        if (!program_) return false;

        std::vector<float> vertices{
            -0.55f, -0.45f, -2.0f,  0.55f, -0.45f, -2.0f,  0.00f, 0.55f, -2.0f,
             0.65f, -0.30f, -3.0f,  1.35f, -0.30f, -3.0f,  1.00f, 0.40f, -3.0f,
            -1.35f, -0.20f, -4.0f, -0.65f, -0.20f, -4.0f, -1.00f, 0.45f, -4.0f,
        };

        gridStartVertex_ = static_cast<GLint>(vertices.size() / 3);
        constexpr float floorY = -1.55f;
        constexpr int gridHalf = 10;
        for (int i = -gridHalf; i <= gridHalf; ++i) {
            const float f = static_cast<float>(i);
            vertices.insert(vertices.end(), {f, floorY, -10.0f, f, floorY, 10.0f});
            vertices.insert(vertices.end(), {-10.0f, floorY, f, 10.0f, floorY, f});
        }
        gridVertexCount_ = static_cast<GLsizei>(vertices.size() / 3 - gridStartVertex_);

        controllerStartVertex_ = static_cast<GLint>(vertices.size() / 3);
        const std::array<float, 18> controllerVertices{
            -0.035f, -0.025f, 0.02f,
             0.035f, -0.025f, 0.02f,
             0.000f,  0.020f, -0.22f,
            -0.025f,  0.020f, 0.02f,
             0.025f,  0.020f, 0.02f,
             0.000f, -0.025f, -0.22f,
        };
        vertices.insert(vertices.end(), controllerVertices.begin(), controllerVertices.end());
        controllerVertexCount_ = 6;

        // Q16.1: vector text "Q16.1" in left-controller local space.
        // +Y places it physically above the hand; +Z keeps it just in front of
        // the controller body. Lines avoid any font/texture dependency.
        versionStartVertex_ = static_cast<GLint>(vertices.size() / 3);
        auto q1600Line = [&](float x0, float y0, float x1, float y1) {
            constexpr float z = 0.035f;
            vertices.insert(vertices.end(), {x0, y0, z, x1, y1, z});
        };
        constexpr float q1600Y = 0.072f;
        constexpr float q1600W = 0.022f;
        constexpr float q1600H = 0.040f;
        constexpr float q1600Gap = 0.007f;
        float q1600X = -0.078f;

        // Q
        q1600Line(q1600X, q1600Y, q1600X + q1600W, q1600Y);
        q1600Line(q1600X, q1600Y + q1600H, q1600X + q1600W, q1600Y + q1600H);
        q1600Line(q1600X, q1600Y, q1600X, q1600Y + q1600H);
        q1600Line(q1600X + q1600W, q1600Y, q1600X + q1600W, q1600Y + q1600H);
        q1600Line(q1600X + q1600W * 0.52f, q1600Y + q1600H * 0.20f,
                  q1600X + q1600W * 1.15f, q1600Y - 0.006f);
        q1600X += q1600W + q1600Gap;

        // Seven-segment helper: A B C D E F G -> bits 0..6.
        auto q1600Digit = [&](float x, unsigned mask) {
            const float y = q1600Y;
            const float m = y + q1600H * 0.5f;
            const float t = y + q1600H;
            if (mask & (1u << 0)) q1600Line(x, t, x + q1600W, t);
            if (mask & (1u << 1)) q1600Line(x + q1600W, t, x + q1600W, m);
            if (mask & (1u << 2)) q1600Line(x + q1600W, m, x + q1600W, y);
            if (mask & (1u << 3)) q1600Line(x, y, x + q1600W, y);
            if (mask & (1u << 4)) q1600Line(x, m, x, y);
            if (mask & (1u << 5)) q1600Line(x, t, x, m);
            if (mask & (1u << 6)) q1600Line(x, m, x + q1600W, m);
        };
        constexpr unsigned q1600Digit1 = 0x06u; // B C
        constexpr unsigned q1600Digit5 = 0x7Du; // Q16.1: digit 6 = A F G E D C

        q1600Digit(q1600X, q1600Digit1);
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, q1600Digit5);
        q1600X += q1600W + q1600Gap;

        // Decimal point.
        q1600Line(q1600X + 0.002f, q1600Y,
                  q1600X + 0.002f, q1600Y + 0.0045f);
        q1600X += 0.006f + q1600Gap;

        q1600Digit(q1600X, 0x5Bu); // Q16.27: 2 = A B G E D
        q1600X += q1600W + q1600Gap;
        q1600Digit(q1600X, 0x07u); // Q16.27: 7 = A B C

        versionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - versionStartVertex_);

        interactionStartVertex_ = static_cast<GLint>(vertices.size() / 3);
        auto q1700Line = [&](float x0, float y0, float x1, float y1) {
            constexpr float z = 0.045f;
            vertices.insert(vertices.end(), {x0, y0, z, x1, y1, z});
        };
        constexpr float q1700Y = 0.078f;
        constexpr float q1700W = 0.018f;
        constexpr float q1700H = 0.032f;
        constexpr float q1700Gap = 0.005f;
        float q1700X = -0.073f;
        auto q1700Advance = [&]() { q1700X += q1700W + q1700Gap; };

        // Q16.1 vanilla Info widget: Xbox A button glyph on the LEFT.
        // The UI XML resolves &xbuttona; through text_box.xml. FalloutQuest has
        // no Gamebryo UI runtime, so reproduce the authored circular controller
        // glyph around the same A strokes in the HUD's 320:80 (~4:1) layout.
        {
            constexpr int kButtonSegments = 24;
            const float cx = q1700X + q1700W * 0.5f;
            const float cy = q1700Y + q1700H * 0.5f;
            const float rx = q1700W * 0.79f;
            const float ry = q1700H * 0.58f;
            for (int i = 0; i < kButtonSegments; ++i) {
                const float a0 = (6.28318530718f * static_cast<float>(i)) /
                                 static_cast<float>(kButtonSegments);
                const float a1 = (6.28318530718f * static_cast<float>(i + 1)) /
                                 static_cast<float>(kButtonSegments);
                q1700Line(cx + std::cos(a0) * rx, cy + std::sin(a0) * ry,
                          cx + std::cos(a1) * rx, cy + std::sin(a1) * ry);
            }
        }

        // A
        q1700Line(q1700X, q1700Y, q1700X + q1700W * 0.5f, q1700Y + q1700H);
        q1700Line(q1700X + q1700W * 0.5f, q1700Y + q1700H, q1700X + q1700W, q1700Y);
        q1700Line(q1700X + q1700W * 0.22f, q1700Y + q1700H * 0.48f,
                  q1700X + q1700W * 0.78f, q1700Y + q1700H * 0.48f);
        q1700Advance();
        q1700X += q1700Gap * 1.2f;

        // O
        q1700Line(q1700X, q1700Y, q1700X + q1700W, q1700Y);
        q1700Line(q1700X, q1700Y + q1700H, q1700X + q1700W, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y, q1700X, q1700Y + q1700H);
        q1700Line(q1700X + q1700W, q1700Y, q1700X + q1700W, q1700Y + q1700H);
        q1700Advance();
        // P
        q1700Line(q1700X, q1700Y, q1700X, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y + q1700H, q1700X + q1700W, q1700Y + q1700H);
        q1700Line(q1700X + q1700W, q1700Y + q1700H,
                  q1700X + q1700W, q1700Y + q1700H * 0.5f);
        q1700Line(q1700X, q1700Y + q1700H * 0.5f,
                  q1700X + q1700W, q1700Y + q1700H * 0.5f);
        q1700Advance();
        // E
        q1700Line(q1700X, q1700Y, q1700X, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y + q1700H, q1700X + q1700W, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y + q1700H * 0.5f,
                  q1700X + q1700W * 0.82f, q1700Y + q1700H * 0.5f);
        q1700Line(q1700X, q1700Y, q1700X + q1700W, q1700Y);
        q1700Advance();
        // N
        q1700Line(q1700X, q1700Y, q1700X, q1700Y + q1700H);
        q1700Line(q1700X + q1700W, q1700Y, q1700X + q1700W, q1700Y + q1700H);
        q1700Line(q1700X, q1700Y + q1700H, q1700X + q1700W, q1700Y);

        interactionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - interactionStartVertex_);

        // Q16.5: vanilla HUDMainMenu Info hierarchy, adapted to right-forearm VR.
        // Authored desktop proportions are 320x80 with a 75x75 A button on the
        // left. Scale that relationship to a compact ~15 cm wide arm HUD.
        interactionStartVertex_ = static_cast<GLint>(vertices.size() / 3);
        auto q1750Line = [&](float x0, float y0, float x1, float y1) {
            constexpr float z = 0.068f; // lifted off the controller/forearm plane
            vertices.insert(vertices.end(), {x0, y0, z, x1, y1, z});
        };

        constexpr float q1750CharW = 0.0165f;
        constexpr float q1750CharH = 0.0250f;
        constexpr float q1750CharGap = 0.0042f;
        constexpr float q1750BottomY = 0.097f;
        constexpr float q1750TopY = q1750BottomY + q1750CharH + 0.0090f;
        constexpr float q1750TextX = -0.025f;

        // The real text_box.xml button is 75x75 inside an 80px-high Info widget.
        // Preserve that near-full-height relationship beside the two text lines.
        constexpr float q1750ButtonCx = -0.087f;
        constexpr float q1750ButtonCy = q1750BottomY + q1750CharH + 0.00325f;
        constexpr float q1750ButtonRx = 0.0335f;
        constexpr float q1750ButtonRy = 0.0335f;
        constexpr int q1750ButtonSegments = 32;
        for (int i = 0; i < q1750ButtonSegments; ++i) {
            const float a0 = (6.28318530718f * static_cast<float>(i)) /
                             static_cast<float>(q1750ButtonSegments);
            const float a1 = (6.28318530718f * static_cast<float>(i + 1)) /
                             static_cast<float>(q1750ButtonSegments);
            q1750Line(q1750ButtonCx + std::cos(a0) * q1750ButtonRx,
                      q1750ButtonCy + std::sin(a0) * q1750ButtonRy,
                      q1750ButtonCx + std::cos(a1) * q1750ButtonRx,
                      q1750ButtonCy + std::sin(a1) * q1750ButtonRy);
        }

        // A glyph centered inside the authored button footprint.
        {
            constexpr float aw = 0.0175f;
            constexpr float ah = 0.0255f;
            const float ax = q1750ButtonCx - aw * 0.5f;
            const float ay = q1750ButtonCy - ah * 0.5f;
            q1750Line(ax, ay, ax + aw * 0.5f, ay + ah);
            q1750Line(ax + aw * 0.5f, ay + ah, ax + aw, ay);
            q1750Line(ax + aw * 0.22f, ay + ah * 0.48f,
                      ax + aw * 0.78f, ay + ah * 0.48f);
        }

        auto q1750Glyph = [&](char c, float x, float y) {
            const float w = q1750CharW;
            const float h = q1750CharH;
            const float m = y + h * 0.5f;
            switch (c) {
                case 'O':
                    q1750Line(x, y, x + w, y);
                    q1750Line(x, y + h, x + w, y + h);
                    q1750Line(x, y, x, y + h);
                    q1750Line(x + w, y, x + w, y + h);
                    break;
                case 'P':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x, y + h, x + w, y + h);
                    q1750Line(x + w, y + h, x + w, m);
                    q1750Line(x, m, x + w, m);
                    break;
                case 'E':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x, y + h, x + w, y + h);
                    q1750Line(x, m, x + w * 0.82f, m);
                    q1750Line(x, y, x + w, y);
                    break;
                case 'N':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x + w, y, x + w, y + h);
                    q1750Line(x, y + h, x + w, y);
                    break;
                case 'D':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x, y + h, x + w * 0.72f, y + h);
                    q1750Line(x, y, x + w * 0.72f, y);
                    q1750Line(x + w * 0.72f, y, x + w, y + h * 0.25f);
                    q1750Line(x + w, y + h * 0.25f, x + w, y + h * 0.75f);
                    q1750Line(x + w, y + h * 0.75f, x + w * 0.72f, y + h);
                    break;
                case 'R':
                    q1750Line(x, y, x, y + h);
                    q1750Line(x, y + h, x + w, y + h);
                    q1750Line(x + w, y + h, x + w, m);
                    q1750Line(x, m, x + w, m);
                    q1750Line(x + w * 0.52f, m, x + w, y);
                    break;
                default:
                    break;
            }
        };
        auto q1750Text = [&](const char* s, float x, float y) {
            float pen = x;
            for (const char* p = s; *p; ++p) {
                q1750Glyph(*p, pen, y);
                pen += q1750CharW + q1750CharGap;
            }
        };

        // Fallout 3 feeds the Info string as a verb/object hierarchy. For the
        // current DOOR-only interaction slice that is the familiar Open / Door.
        q1750Text("OPEN", q1750TextX, q1750TopY);
        q1750Text("DOOR", q1750TextX, q1750BottomY);

        interactionVertexCount_ =
            static_cast<GLsizei>(vertices.size() / 3 - interactionStartVertex_);
        FQ_LOGI("Q16.27 BUILD LABEL: text=Q16.27 anchor=left-hand vertices=%d alwaysVisible=1",
                static_cast<int>(versionVertexCount_));

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
        glGenBuffers(1, &vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                     vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);

        mvpLocation_ = glGetUniformLocation(program_, "uMvp");
        colorLocation_ = glGetUniformLocation(program_, "uColor");
        glGenFramebuffers(1, &framebuffer_);
        return true;
    }

    void ProcessAndroidEvents() {
        android_poll_source* source = nullptr;
        int events = 0;
        while (ALooper_pollOnce(sessionRunning_ ? 0 : 10, nullptr, &events,
                                reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(app_, source);
            if (app_->destroyRequested) {
                exitRequested_ = true;
                return;
            }
        }
    }

    void ProcessXrEvents() {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(instance_, &event) == XR_SUCCESS) {
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* changed = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                sessionState_ = changed->state;
                FQ_LOGI("OpenXR session state -> %d", static_cast<int>(sessionState_));

                if (sessionState_ == XR_SESSION_STATE_READY && !sessionRunning_) {
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (CheckXr(xrBeginSession(session_, &begin), "xrBeginSession")) {
                        sessionRunning_ = true;
                        lastFrameTime_ = 0;
                    }
                } else if (sessionState_ == XR_SESSION_STATE_STOPPING && sessionRunning_) {
                    CheckXr(xrEndSession(session_), "xrEndSession");
                    sessionRunning_ = false;
                } else if (sessionState_ == XR_SESSION_STATE_EXITING ||
                           sessionState_ == XR_SESSION_STATE_LOSS_PENDING) {
                    exitRequested_ = true;
                }
            } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                exitRequested_ = true;
            }
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
    }

    float ReadFloatAction(XrAction action, XrPath subaction) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subaction;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        if (!CheckXr(xrGetActionStateFloat(session_, &getInfo, &state), "xrGetActionStateFloat")) return 0.0f;
        return state.isActive ? state.currentState : 0.0f;
    }

    bool ReadBooleanActionQ1450(XrAction action, XrPath subaction) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subaction;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (!CheckXr(xrGetActionStateBoolean(session_, &getInfo, &state),
                     "xrGetActionStateBoolean")) return false;
        return state.isActive && state.currentState;
    }

    bool ReadBoolActionQ1699(XrAction action, XrPath subaction) {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subaction;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (!CheckXr(xrGetActionStateBoolean(session_, &getInfo, &state),
                     "xrGetActionStateBoolean(Q16.1 A)")) return false;
        return state.isActive && state.currentState;
    }

    void SyncInput(XrTime time) {
        XrActiveActionSet active{actionSet_, XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &active;
        if (!CheckXr(xrSyncActions(session_, &sync), "xrSyncActions")) return;

        const float rawMoveX = ReadFloatAction(moveXAction_, handPaths_[0]);
        const float rawMoveY = ReadFloatAction(moveYAction_, handPaths_[0]);
        RadialMoveDeadzoneQ715(rawMoveX, rawMoveY, moveX_, moveY_);
        turnX_ = Deadzone(ReadFloatAction(turnXAction_, handPaths_[1]));
        timeAdvanceValue_ = ReadFloatAction(timeAdvanceAction_, handPaths_[0]);
        const bool q1450FogPressed = ReadBooleanActionQ1450(fogToggleAction_, handPaths_[0]);
        if (q1450FogPressed && !fogTogglePressed_) {
            CycleFo3RenderStageQ1560();
            FQ_LOGI("Q15.6 RENDER STAGE MODE: mode=%s source=LEFT_X stages=RAW_FOG_HDR_FINAL authoredLightingOnly=1 tangentABRetired=1",
                    GetFo3RenderStageNameQ1560());
        }
        fogTogglePressed_ = q1450FogPressed;
        const bool q1470DomainPressed =
            ReadBooleanActionQ1450(ppDomainToggleAction_, handPaths_[0]);
        if (q1470DomainPressed && !ppDomainTogglePressed_) {
            ToggleFo3LegacyColourDomainQ1570();
            FQ_LOGI("Q15.7 LEGACY DOMAIN MODE: mode=%s source=LEFT_Y scope=statics+LAND ambientScale=0.65 authoredSunlightRGB=1",
                    GetFo3LegacyColourDomainNameQ1570());
        }
        ppDomainTogglePressed_ = q1470DomainPressed;
        activateDownQ1700_ = ReadBoolActionQ1699(activateAction_, handPaths_[1]);

        for (uint32_t hand = 0; hand < 2; ++hand) {
            XrActionStateGetInfo poseInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            poseInfo.action = poseAction_;
            poseInfo.subactionPath = handPaths_[hand];
            XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
            if (!CheckXr(xrGetActionStatePose(session_, &poseInfo, &poseState), "xrGetActionStatePose") ||
                !poseState.isActive) {
                handPoseValid_[hand] = false;
                continue;
            }

            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
            if (!CheckXr(xrLocateSpace(handSpaces_[hand], localSpace_, time, &location), "xrLocateSpace(hand)")) {
                handPoseValid_[hand] = false;
                continue;
            }
            const XrSpaceLocationFlags required = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                                  XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            handPoseValid_[hand] = (location.locationFlags & required) == required;
            if (handPoseValid_[hand]) handLocalPoses_[hand] = location.pose;
        }
    }

    void UpdatePlayer(const XrView& headView, XrTime frameTime) {
        double dt = 1.0 / 72.0;
        if (lastFrameTime_ != 0 && frameTime > lastFrameTime_) {
            dt = static_cast<double>(frameTime - lastFrameTime_) / 1'000'000'000.0;
            dt = std::clamp(dt, 0.0, 0.05);
        }
        lastFrameTime_ = frameTime;

        if (!snapTurnLatched_ && std::fabs(turnX_) > 0.65f) {
            const float deltaYaw = turnX_ > 0.0f ? -SNAP_TURN_RADIANS : SNAP_TURN_RADIANS;
            const XrVector3f before = RotateYaw(headView.pose.position, playerYaw_);
            const XrVector3f after = RotateYaw(headView.pose.position, playerYaw_ + deltaYaw);
            playerPosition_.x += before.x - after.x;
            playerPosition_.z += before.z - after.z;
            playerYaw_ += deltaYaw;
            snapTurnLatched_ = true;
            FQ_LOGI("Snap turn: %s yaw %.1f degrees pivot-preserved=1",
                    deltaYaw < 0.0f ? "right" : "left",
                    playerYaw_ * 180.0f / PI);
        } else if (snapTurnLatched_ && std::fabs(turnX_) < 0.30f) {
            snapTurnLatched_ = false;
        }

        const float headYaw = YawFromQuaternion(headView.pose.orientation) + playerYaw_;
        const float forward = moveY_;
        const float strafe = moveX_;
        const float c = std::cos(headYaw);
        const float s = std::sin(headYaw);
        const float velocity = MOVE_SPEED_METRES_PER_SECOND * static_cast<float>(dt);

        const float requestedDx = (strafe * c - forward * s) * velocity;
        const float requestedDz = (-strafe * s - forward * c) * velocity;
        const float desiredPlayerX = playerPosition_.x + requestedDx;
        const float desiredPlayerZ = playerPosition_.z + requestedDz;

        const XrVector3f headOffset = RotateYaw(headView.pose.position, playerYaw_);
        const float currentCenterX = headOffset.x + playerPosition_.x;
        const float currentCenterZ = headOffset.z + playerPosition_.z;
        const float desiredCenterX = headOffset.x + desiredPlayerX;
        const float desiredCenterZ = headOffset.z + desiredPlayerZ;

        float resolvedCenterX = desiredCenterX;
        float resolvedCenterZ = desiredCenterZ;
        float resolvedPlayerY = playerPosition_.y;
        if (ResolveFo3PlayerMotionQ6G(currentCenterX, currentCenterZ,
                                     desiredCenterX, desiredCenterZ,
                                     playerPosition_.y,
                                     &resolvedCenterX, &resolvedCenterZ,
                                     &resolvedPlayerY)) {
            playerPosition_.x = resolvedCenterX - headOffset.x;
            playerPosition_.z = resolvedCenterZ - headOffset.z;
            playerPosition_.y = resolvedPlayerY;
        } else {
            playerPosition_.x = desiredPlayerX;
            playerPosition_.z = desiredPlayerZ;
        }
    }

    XrPosef ToVirtualPose(const XrPosef& localPose) const {
        const XrVector3f rotated = RotateYaw(localPose.position, playerYaw_);
        XrPosef result{};
        result.orientation = QuaternionMultiply(YawQuaternion(playerYaw_), localPose.orientation);
        result.position = {
            rotated.x + playerPosition_.x,
            rotated.y + playerPosition_.y,
            rotated.z + playerPosition_.z,
        };
        return result;
    }

    void ProbeDoorQ71() {
        if (activateValue_ > 0.75f && !activateLatched_) {
            activateLatched_ = true;
            if (!handPoseValid_[1]) {
                FQ_LOGI("Q7.1 DOOR MISS: right-hand aim pose unavailable at trigger edge");
                return;
            }
            const XrPosef hand = ToVirtualPose(handLocalPoses_[1]);
            const Mat4 handMatrix = MatrixFromPose(hand);
            const float dx = -handMatrix.m[8];
            const float dy = -handMatrix.m[9];
            const float dz = -handMatrix.m[10];
            ProbeMegatonPlayerHouseDoorQ71(hand.position.x, hand.position.y, hand.position.z,
                                           dx, dy, dz);
        } else if (activateValue_ < 0.25f) {
            activateLatched_ = false;
        }
    }

    void UpdateDoorAimQ1700() {
        if (IsFo3LoadingVisibleQ1700() || !handPoseValid_[1]) {
            if (doorAimActiveQ1700_) {
                FQ_LOGI("Q16.13 HUD TARGET HIDE: sourceDoor=%08X reason=%s promptStorage=fixed-retained",
                        doorAimSourceQ1840_,
                        IsFo3LoadingVisibleQ1700() ? "loading" : "invalid-hand-pose");
            }
            doorAimActiveQ1700_ = false;
            doorAimSourceQ1840_ = 0u;
            doorPromptValidQ1850_ = false;
            return;
        }
        const XrPosef hand = ToVirtualPose(handLocalPoses_[1]);
        const Mat4 handMatrix = MatrixFromPose(hand);
        Fo3DoorAimQ1700 aim{};
        const bool q1850WasActive = doorAimActiveQ1700_;
        const bool q1850NextActive = QueryFo3DoorAimQ1700(
            hand.position.x, hand.position.y, hand.position.z,
            -handMatrix.m[8], -handMatrix.m[9], -handMatrix.m[10], &aim) && aim.valid;

        if (q1850NextActive) {
            // Keep the authored destination/yaw cache for the post-A facing fix.
            doorAimYawQ1800_ = aim.rz;
            doorAimDestinationQ1800_ = aim.destinationDoorRef;

            if (doorAimSourceQ1840_ != aim.sourceDoorRef || !doorPromptValidQ1850_) {
                std::array<char, 512> q1850Prompt{};
                if (GetFo3DoorPromptQ1840(aim.sourceDoorRef,
                                          q1850Prompt.data(),
                                          q1850Prompt.size())) {
                    // Fixed-size copy only when the target changes. On aim loss
                    // these bytes are deliberately retained and never mutated.
                    std::memcpy(doorPromptQ1850_.data(),
                                q1850Prompt.data(),
                                doorPromptQ1850_.size());
                    doorPromptValidQ1850_ = true;
                } else {
                    doorPromptValidQ1850_ = false;
                    FQ_LOGE("Q16.13 HUD PROMPT MISS: sourceDoor=%08X destinationDoor=%08X",
                            aim.sourceDoorRef, aim.destinationDoorRef);
                }
                doorAimSourceQ1840_ = aim.sourceDoorRef;
            }

            if (!q1850WasActive) {
                FQ_LOGI("Q16.13 HUD TARGET SHOW: sourceDoor=%08X destinationDoor=%08X text=%s promptStorage=fixed",
                        aim.sourceDoorRef, aim.destinationDoorRef,
                        doorPromptValidQ1850_ ? doorPromptQ1850_.data() : "<missing>");
            }
        } else {
            if (q1850WasActive) {
                FQ_LOGI("Q16.13 HUD TARGET HIDE: sourceDoor=%08X reason=ray-miss promptStorage=fixed-retained",
                        doorAimSourceQ1840_);
            }
            // Aim loss is now three scalar writes. Do NOT clear/memset/free the
            // prompt buffer, and do not touch HUD GL resources.
            doorAimSourceQ1840_ = 0u;
            doorPromptValidQ1850_ = false;
        }
        doorAimActiveQ1700_ = q1850NextActive;
    }

    void ApplyAuthoredDoorFacingQ1810(const XrView& view) {
        if (!IsFo3LoadingVisibleQ1700()) return;

        const uint64_t generation = GetFo3LoadingGenerationQ1700();
        if (generation == 0u || generation == doorFacingGenerationQ1810_) return;

        // Consume this generation first so a malformed/missing cached target
        // cannot repeatedly alter the player's heading every loading frame.
        doorFacingGenerationQ1810_ = generation;
        if (doorAimDestinationQ1800_ == 0u) {
            FQ_LOGI("Q16.27 AUTHORED DOOR FACING SKIP: generation=%llu reason=no-cached-destination",
                    static_cast<unsigned long long>(generation));
            return;
        }

        playerPosition_ = {0.0f, 0.0f, 0.0f};
        const float physicalHeadYaw = YawFromQuaternion(view.pose.orientation);
        const float rawPlayerYaw = doorAimYawQ1800_ - physicalHeadYaw;
        playerYaw_ = std::atan2(std::sin(rawPlayerYaw), std::cos(rawPlayerYaw));
        lastFrameTime_ = 0;

        FQ_LOGI("Q16.27 AUTHORED DOOR FACING: destinationDoor=%08X XTELrz=%.4f physicalHeadYaw=%.4f playerYaw=%.4f degrees=%.1f generation=%llu",
                doorAimDestinationQ1800_, doorAimYawQ1800_,
                physicalHeadYaw, playerYaw_,
                playerYaw_ * 180.0f / PI,
                static_cast<unsigned long long>(generation));
    }

    void RenderFrame() {
        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        if (!CheckXr(xrWaitFrame(session_, &waitInfo, &frameState), "xrWaitFrame")) return;
        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        if (!CheckXr(xrBeginFrame(session_, &beginInfo), "xrBeginFrame")) return;
        PrepareFo3LoadingScreensQ1700();

        std::array<XrCompositionLayerProjectionView, 2> projectionViews{
            XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
            XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
        };
        XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        const XrCompositionLayerBaseHeader* layers[1]{};
        uint32_t layerCount = 0;

        if (frameState.shouldRender) {
            std::array<XrView, 2> views{XrView{XR_TYPE_VIEW}, XrView{XR_TYPE_VIEW}};
            XrViewState viewState{XR_TYPE_VIEW_STATE};
            XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
            locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locateInfo.displayTime = frameState.predictedDisplayTime;
            locateInfo.space = localSpace_;

            uint32_t viewCount = 0;
            if (CheckXr(xrLocateViews(session_, &locateInfo, &viewState,
                                     static_cast<uint32_t>(views.size()), &viewCount, views.data()),
                        "xrLocateViews") && viewCount == 2 &&
                (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
                (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {

                SyncInput(frameState.predictedDisplayTime);
                UpdateFo3TimeOfDayQ1400(timeAdvanceValue_,
                    static_cast<int64_t>(frameState.predictedDisplayTime));
                if (fo3todq1400::gRuntime.ready) {
                    UpdateFo3FogPowerQ1410(
                        fo3todq1400::gTestHour,
                        fo3todq1400::gRuntime.climate.sunriseBegin,
                        fo3todq1400::gRuntime.climate.sunriseEnd,
                        fo3todq1400::gRuntime.climate.sunsetBegin,
                        fo3todq1400::gRuntime.climate.sunsetEnd);
                }
                if (!IsFo3LoadingVisibleQ1700()) {
                    UpdatePlayer(views[0], frameState.predictedDisplayTime);
                const XrPosef q1890VirtualHead = ToVirtualPose(views[0].pose);
                UpdateFo3ExteriorStreamingQ1890(q1890VirtualHead.position.x,
                                                q1890VirtualHead.position.z);
                } else {
                    lastFrameTime_ = 0;
                }
                UpdateDoorAimQ1700();
                ApplyAuthoredDoorFacingQ1810(views[0]);
                if (activateDownQ1700_ && !activateLatched_) {
                    activateLatched_ = true;
                    if (handPoseValid_[1]) {
                        const XrPosef handQ1700 = ToVirtualPose(handLocalPoses_[1]);
                        const Mat4 handMatrixQ1700 = MatrixFromPose(handQ1700);
                        if (ActivateFo3DoorQ1700(
                                handQ1700.position.x, handQ1700.position.y, handQ1700.position.z,
                                -handMatrixQ1700.m[8], -handMatrixQ1700.m[9], -handMatrixQ1700.m[10])) {
                            playerPosition_ = {0.0f, 0.0f, 0.0f};
                            playerYaw_ = 0.0f;
                            lastFrameTime_ = 0;
                            FQ_LOGI("Q16.1 player origin reset for queued authored XTEL");
                        }
                    }
                } else if (!activateDownQ1700_) {
                    activateLatched_ = false;
                }

                for (uint32_t eye = 0; eye < 2; ++eye) {
                    RenderEye(eye, views[eye], projectionViews[eye]);
                }

                projectionLayer.space = localSpace_;
                projectionLayer.viewCount = 2;
                projectionLayer.views = projectionViews.data();
                layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer);
                layerCount = 1;

                if ((frameCounter_++ % 180) == 0) {
                    FQ_LOGI("Q7.1 live: player %.2f %.2f %.2f, collision=%d, hands L=%d R=%d, move %.2f %.2f trigger=%.2f",
                            playerPosition_.x, playerPosition_.y, playerPosition_.z,
                            IsFo3PlayerCollisionReadyQ6G() ? 1 : 0,
                            handPoseValid_[0] ? 1 : 0, handPoseValid_[1] ? 1 : 0,
                            moveX_, moveY_, activateValue_);
                }
            }
        }

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = blendMode_;
        endInfo.layerCount = layerCount;
        endInfo.layers = layerCount ? layers : nullptr;
        const bool q1700FrameSubmitted =
            CheckXr(xrEndFrame(session_, &endInfo), "xrEndFrame");
        if (q1700FrameSubmitted) MarkFo3LoadingFramePresentedQ1700();
    }

    void SetMvpAndColor(const Mat4& mvp, float r, float g, float b) {
        glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, mvp.m);
        glUniform3f(colorLocation_, r, g, b);
    }

    void RenderEye(uint32_t eyeIndex, const XrView& view,
                   XrCompositionLayerProjectionView& projectionView) {
        EyeSwapchain& eye = eyes_[eyeIndex];
        uint32_t imageIndex = 0;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        if (!CheckXr(xrAcquireSwapchainImage(eye.handle, &acquireInfo, &imageIndex),
                     "xrAcquireSwapchainImage")) return;
        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        if (!CheckXr(xrWaitSwapchainImage(eye.handle, &waitInfo), "xrWaitSwapchainImage")) return;

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               eye.images[imageIndex].image, 0);
        Q1280BeginEyePostQ1280(framebuffer_, eye.width, eye.height);
        glViewport(0, 0, eye.width, eye.height);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            glDisable(GL_DEPTH_TEST);
            const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
            if (q1000Env.valid) {
                glClearColor(q1000Env.horizon[0], q1000Env.horizon[1], q1000Env.horizon[2], 1.0f);
            } else {
                glClearColor(0.018f, 0.028f, 0.020f, 1.0f);
            }
            glClear(GL_COLOR_BUFFER_BIT);

            constexpr float Q1850_FO3_BLOCK_LOAD_DISTANCE_UNITS = 125000.0f;
            constexpr float Q1850_FAR_CLIP_METRES =
                Q1850_FO3_BLOCK_LOAD_DISTANCE_UNITS / FO3_UNITS_PER_METRE_Q715;
            const Mat4 projection = ProjectionFromFov(
                view.fov, 0.04f, Q1850_FAR_CLIP_METRES);
            static bool q1850FarClipLogged = false;
            if (!q1850FarClipLogged) {
                q1850FarClipLogged = true;
                FQ_LOGI("Q18.5 FAR CLIP: near=0.04m far=%.3fm source=Fallout.ini/fBlockLoadDistance gameUnits=%.1f unitsPerMetre=%.1f",
                        Q1850_FAR_CLIP_METRES,
                        Q1850_FO3_BLOCK_LOAD_DISTANCE_UNITS,
                        FO3_UNITS_PER_METRE_Q715);
            }
            const XrPosef virtualEyePose = ToVirtualPose(view.pose);
            UpdateFo3VisualEyeQ1010(virtualEyePose.position.x, virtualEyePose.position.y, virtualEyePose.position.z);
            const Mat4 camera = ViewFromPose(virtualEyePose);
            Mat4 skyView = camera;
            skyView.m[12] = 0.0f;
            skyView.m[13] = 0.0f;
            skyView.m[14] = 0.0f;
            const Mat4 skyMvp = Multiply(projection, skyView);
            RenderFo3PcSkyQ1660(skyMvp.m);
            const Mat4 viewProjection = Multiply(projection, camera);

            glUseProgram(program_);
            glBindVertexArray(vao_);

            SetMvpAndColor(viewProjection, 0.95f, 0.72f, 0.12f);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glUniform3f(colorLocation_, 0.30f, 0.80f, 0.42f);
            glDrawArrays(GL_TRIANGLES, 3, 3);
            glUniform3f(colorLocation_, 0.58f, 0.72f, 0.34f);
            glDrawArrays(GL_TRIANGLES, 6, 3);

            glUniform3f(colorLocation_, 0.13f, 0.30f, 0.17f);
            glDrawArrays(GL_LINES, gridStartVertex_, gridVertexCount_);

            for (uint32_t hand = 0; hand < 2; ++hand) {
                if (!handPoseValid_[hand]) continue;
                const XrPosef virtualHandPose = ToVirtualPose(handLocalPoses_[hand]);
                const Mat4 model = MatrixFromPose(virtualHandPose);
                const Mat4 mvp = Multiply(viewProjection, model);
                if (hand == 0) {
                    SetMvpAndColor(mvp, 0.25f, 0.68f, 1.0f);
                    glDrawArrays(GL_TRIANGLES, controllerStartVertex_, controllerVertexCount_);

                    // A bright floating build ID above the left hand. Force the
                    // label through depth only for these line vertices, then put
                    // the normal world/controller depth function straight back.
                    glDepthFunc(GL_ALWAYS);
                    glLineWidth(2.0f);
                    SetMvpAndColor(mvp, 1.0f, 0.86f, 0.10f);
                    glDrawArrays(GL_LINES, versionStartVertex_, versionVertexCount_);
                    glDepthFunc(GL_LEQUAL);
                } else {
                    SetMvpAndColor(mvp, 1.0f, 0.48f, 0.18f);
                    glDrawArrays(GL_TRIANGLES, controllerStartVertex_, controllerVertexCount_);
                    if (doorAimActiveQ1700_) {
                        if (doorPromptValidQ1850_) {
                            RenderFo3InteractionHudQ1880(mvp.m, doorPromptQ1850_.data());
                        }

                    }
                    if (false) {
                        static bool q1780HudDrawLogged = false;
                        if (!q1780HudDrawLogged) {
                            q1780HudDrawLogged = true;
                            __android_log_print(ANDROID_LOG_INFO, "FalloutQuest",
                                                "Q16.8 HUD DRAW: start=%d count=%d layout=A+OPEN/DOOR scale=1.22 HUDMainRGB=(26,255,128)",
                                                interactionStartVertex_, interactionVertexCount_);
                        }
                        // hud_main_menu.xml -> Info -> text_box.xml semantics:
                        // HUDMain system colour, glow=true, line_alpha=255,
                        // xbuttona on the left; Q16.5 mounts the 320:80 layout above the right forearm.
                        const GLboolean q1710BlendWasEnabled = glIsEnabled(GL_BLEND);
                        GLint q1710BlendSrcRgb = GL_ONE;
                        GLint q1710BlendDstRgb = GL_ZERO;
                        GLint q1710BlendSrcAlpha = GL_ONE;
                        GLint q1710BlendDstAlpha = GL_ZERO;
                        glGetIntegerv(GL_BLEND_SRC_RGB, &q1710BlendSrcRgb);
                        glGetIntegerv(GL_BLEND_DST_RGB, &q1710BlendDstRgb);
                        glGetIntegerv(GL_BLEND_SRC_ALPHA, &q1710BlendSrcAlpha);
                        glGetIntegerv(GL_BLEND_DST_ALPHA, &q1710BlendDstAlpha);

                        glDepthFunc(GL_ALWAYS);
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_ONE, GL_ONE);

                        // Soft Gamebryo-style glow using the RGB-only line shader.
                        glLineWidth(7.0f);
                        SetMvpAndColor(mvp,
                                       (26.0f / 255.0f) * 0.10f,
                                       0.10f,
                                       (128.0f / 255.0f) * 0.10f);
                        glDrawArrays(GL_LINES, interactionStartVertex_, interactionVertexCount_);
                        glLineWidth(4.0f);
                        SetMvpAndColor(mvp,
                                       (26.0f / 255.0f) * 0.22f,
                                       0.22f,
                                       (128.0f / 255.0f) * 0.22f);
                        glDrawArrays(GL_LINES, interactionStartVertex_, interactionVertexCount_);

                        // Full-alpha vanilla HUDMain core: RGB(26,255,128).
                        glBlendFunc(GL_ONE, GL_ZERO);
                        glLineWidth(2.0f);
                        SetMvpAndColor(mvp, 26.0f / 255.0f, 1.0f, 128.0f / 255.0f);
                        glDrawArrays(GL_LINES, interactionStartVertex_, interactionVertexCount_);

                        glBlendFuncSeparate(q1710BlendSrcRgb, q1710BlendDstRgb,
                                            q1710BlendSrcAlpha, q1710BlendDstAlpha);
                        if (!q1710BlendWasEnabled) glDisable(GL_BLEND);
                        glLineWidth(1.0f);
                        glDepthFunc(GL_LEQUAL);
                    }
                }
            }
            Q1350CompositeEyePostQ1350(eyeIndex, framebuffer_, eye.width, eye.height);
            if (IsFo3LoadingVisibleQ1700()) {
                RenderFo3LoadingScreenQ1780(framebuffer_, eye.width, eye.height);
            }
            glFlush();
        } else {
            FQ_LOGE("Incomplete eye framebuffer");
        }

        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        CheckXr(xrReleaseSwapchainImage(eye.handle, &releaseInfo), "xrReleaseSwapchainImage");

        projectionView.pose = view.pose;
        projectionView.fov = view.fov;
        projectionView.subImage.swapchain = eye.handle;
        projectionView.subImage.imageRect.offset = {0, 0};
        projectionView.subImage.imageRect.extent = {eye.width, eye.height};
        projectionView.subImage.imageArrayIndex = 0;
    }

private:
    android_app* app_{};
    XrInstance instance_{XR_NULL_HANDLE};
    XrSystemId systemId_{XR_NULL_SYSTEM_ID};
    XrSession session_{XR_NULL_HANDLE};
    XrSpace localSpace_{XR_NULL_HANDLE};
    XrSessionState sessionState_{XR_SESSION_STATE_UNKNOWN};
    bool sessionRunning_{false};
    bool exitRequested_{false};
    XrEnvironmentBlendMode blendMode_{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
    std::array<EyeSwapchain, 2> eyes_{};

    XrActionSet actionSet_{XR_NULL_HANDLE};
    XrAction poseAction_{XR_NULL_HANDLE};
    XrAction moveXAction_{XR_NULL_HANDLE};
    XrAction moveYAction_{XR_NULL_HANDLE};
    XrAction turnXAction_{XR_NULL_HANDLE};
    XrAction activateAction_{XR_NULL_HANDLE};
    XrAction timeAdvanceAction_{XR_NULL_HANDLE};
    XrAction fogToggleAction_{XR_NULL_HANDLE};
    XrAction ppDomainToggleAction_{XR_NULL_HANDLE};
    std::array<XrPath, 2> handPaths_{XR_NULL_PATH, XR_NULL_PATH};
    std::array<XrSpace, 2> handSpaces_{XR_NULL_HANDLE, XR_NULL_HANDLE};
    std::array<XrPosef, 2> handLocalPoses_{};
    std::array<bool, 2> handPoseValid_{false, false};
    float moveX_{0.0f};
    float moveY_{0.0f};
    float turnX_{0.0f};
    float activateValue_{0.0f}; // legacy Q7.1 method only; no longer sampled
    bool activateDownQ1700_{false};
    float timeAdvanceValue_{0.0f};
    bool fogTogglePressed_{false};
    bool ppDomainTogglePressed_{false};
    bool activateLatched_{false};

    XrVector3f playerPosition_{0.0f, 0.0f, 0.0f};
    float playerYaw_{0.0f};
    bool snapTurnLatched_{false};
    XrTime lastFrameTime_{0};

    EGLDisplay eglDisplay_{EGL_NO_DISPLAY};
    EGLConfig eglConfig_{nullptr};
    EGLContext eglContext_{EGL_NO_CONTEXT};
    EGLSurface eglSurface_{EGL_NO_SURFACE};

    GLuint program_{0};
    GLuint vao_{0};
    GLuint vbo_{0};
    GLuint framebuffer_{0};
    GLint mvpLocation_{-1};
    GLint colorLocation_{-1};
    GLint gridStartVertex_{0};
    GLsizei gridVertexCount_{0};
    GLint controllerStartVertex_{0};
    GLsizei controllerVertexCount_{0};
    GLint versionStartVertex_{0};
    GLsizei versionVertexCount_{0};
    GLint interactionStartVertex_{0};
    GLsizei interactionVertexCount_{0};
    bool doorAimActiveQ1700_{false};
    float doorAimYawQ1800_{0.0f};
    uint32_t doorAimDestinationQ1800_{0u};
    uint32_t doorAimSourceQ1840_{0u};
    std::array<char, 512> doorPromptQ1850_{};
    bool doorPromptValidQ1850_{false};
    uint64_t doorFacingGenerationQ1810_{0u};

    uint64_t frameCounter_{0};
};

} // namespace

extern "C" void android_main(struct android_app* app) {
    app_dummy();
    FQ_LOGI("FalloutQuest Q7.1 native OpenXR process booting");
    FalloutQuestXr xr(app);
    if (!xr.Initialize()) {
        FQ_LOGE("FalloutQuest Q7.1 initialization failed");
        return;
    }
    xr.Run();
    xr.Shutdown();
    FQ_LOGI("FalloutQuest Q7.1 process stopped");
}

// Q15.6 RENDER STAGE MODE: build-guard marker

#if 0
                            playerPosition_ = {0.0f, 0.0f, 0.0f};
                            const float q1800PhysicalHeadYaw =
                                YawFromQuaternion(views[0].pose.orientation);
                            const float q1800RawPlayerYaw =
                                doorAimYawQ1800_ - q1800PhysicalHeadYaw;
                            playerYaw_ = std::atan2(std::sin(q1800RawPlayerYaw),
                                                   std::cos(q1800RawPlayerYaw));
                            lastFrameTime_ = 0;
                            FQ_LOGI("Q16.27 AUTHORED DOOR FACING: destinationDoor=%08X XTELrz=%.4f physicalHeadYaw=%.4f playerYaw=%.4f degrees=%.1f",
                                    doorAimDestinationQ1800_, doorAimYawQ1800_,
                                    q1800PhysicalHeadYaw, playerYaw_,
                                    playerYaw_ * 180.0f / PI);
#endif

