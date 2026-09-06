#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr const char* TAG = "FalloutQuest";

#define FQ_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define FQ_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

bool CheckXr(XrResult result, const char* call) {
    if (XR_FAILED(result)) {
        FQ_LOGE("%s failed: XrResult %d", call, static_cast<int>(result));
        return false;
    }
    return true;
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

Mat4 ViewFromPose(const XrPosef& pose) {
    // The OpenXR view pose maps view-local coordinates into the reference space.
    // Build its inverse: conjugate rotation followed by inverse translation.
    const float x = -pose.orientation.x;
    const float y = -pose.orientation.y;
    const float z = -pose.orientation.z;
    const float w =  pose.orientation.w;

    Mat4 v{};
    v.m[0]  = 1.0f - 2.0f * (y * y + z * z);
    v.m[1]  = 2.0f * (x * y + z * w);
    v.m[2]  = 2.0f * (x * z - y * w);
    v.m[3]  = 0.0f;

    v.m[4]  = 2.0f * (x * y - z * w);
    v.m[5]  = 1.0f - 2.0f * (x * x + z * z);
    v.m[6]  = 2.0f * (y * z + x * w);
    v.m[7]  = 0.0f;

    v.m[8]  = 2.0f * (x * z + y * w);
    v.m[9]  = 2.0f * (y * z - x * w);
    v.m[10] = 1.0f - 2.0f * (x * x + y * y);
    v.m[11] = 0.0f;

    const float px = pose.position.x;
    const float py = pose.position.y;
    const float pz = pose.position.z;
    v.m[12] = -(v.m[0] * px + v.m[4] * py + v.m[8]  * pz);
    v.m[13] = -(v.m[1] * px + v.m[5] * py + v.m[9]  * pz);
    v.m[14] = -(v.m[2] * px + v.m[6] * py + v.m[10] * pz);
    v.m[15] = 1.0f;
    return v;
}

Mat4 ProjectionFromFov(const XrFovf& fov, float nearZ, float farZ) {
    const float tanLeft  = std::tan(fov.angleLeft);
    const float tanRight = std::tan(fov.angleRight);
    const float tanDown  = std::tan(fov.angleDown);
    const float tanUp    = std::tan(fov.angleUp);

    const float tanWidth  = tanRight - tanLeft;
    const float tanHeight = tanUp - tanDown;

    Mat4 p{};
    p.m[0]  = 2.0f / tanWidth;
    p.m[5]  = 2.0f / tanHeight;
    p.m[8]  = (tanRight + tanLeft) / tanWidth;
    p.m[9]  = (tanUp + tanDown) / tanHeight;
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
    explicit FalloutQuestXr(android_app* app) : app_(app) {}

    bool Initialize() {
        if (!InitializeLoader()) return false;
        if (!CreateInstance()) return false;
        if (!CreateSystem()) return false;
        if (!CreateEgl()) return false;
        if (!CreateSession()) return false;
        if (!CreateReferenceSpace()) return false;
        if (!CreateSwapchains()) return false;
        if (!CreateSceneRenderer()) return false;

        FQ_LOGI("Q3 OPENXR READY: instance + session + stereo swapchains + GLES renderer initialized");
        return true;
    }

    void Run() {
        while (!exitRequested_ && !app_->destroyRequested) {
            ProcessAndroidEvents();
            ProcessXrEvents();
            if (sessionRunning_) {
                RenderFrame();
            }
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

        for (auto& eye : eyes_) {
            if (eye.handle != XR_NULL_HANDLE) xrDestroySwapchain(eye.handle);
        }
        if (localSpace_ != XR_NULL_HANDLE) xrDestroySpace(localSpace_);
        if (session_ != XR_NULL_HANDLE) xrDestroySession(session_);
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
                     "xrGetInstanceProcAddr(xrInitializeLoaderKHR)")) {
            return false;
        }

        auto initializeLoader = reinterpret_cast<PFN_xrInitializeLoaderKHR>(fn);
        if (!initializeLoader) {
            FQ_LOGE("xrInitializeLoaderKHR unavailable");
            return false;
        }

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
        std::strncpy(info.applicationInfo.applicationName, "FalloutQuest",
                     XR_MAX_APPLICATION_NAME_SIZE - 1);
        info.applicationInfo.applicationVersion = 2;
        std::strncpy(info.applicationInfo.engineName, "FalloutQuestNative",
                     XR_MAX_ENGINE_NAME_SIZE - 1);
        info.applicationInfo.engineVersion = 1;
        // Keep the requested API baseline conservative even though we compile against 1.1 headers.
        info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
        info.enabledExtensionCount = 2;
        info.enabledExtensionNames = extensions;

        if (!CheckXr(xrCreateInstance(&info, &instance_), "xrCreateInstance")) return false;

        XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
        if (CheckXr(xrGetInstanceProperties(instance_, &props), "xrGetInstanceProperties")) {
            FQ_LOGI("OpenXR runtime: %s (%u.%u.%u)", props.runtimeName,
                    XR_VERSION_MAJOR(props.runtimeVersion),
                    XR_VERSION_MINOR(props.runtimeVersion),
                    XR_VERSION_PATCH(props.runtimeVersion));
        }
        return true;
    }

    bool CreateSystem() {
        XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
        info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        if (!CheckXr(xrGetSystem(instance_, &info, &systemId_), "xrGetSystem")) return false;

        XrSystemProperties props{XR_TYPE_SYSTEM_PROPERTIES};
        if (CheckXr(xrGetSystemProperties(instance_, systemId_, &props), "xrGetSystemProperties")) {
            FQ_LOGI("XR system: %s, max swapchain %ux%u", props.systemName,
                    props.graphicsProperties.maxSwapchainImageWidth,
                    props.graphicsProperties.maxSwapchainImageHeight);
        }
        return true;
    }

    bool CreateEgl() {
        PFN_xrVoidFunction fn = nullptr;
        if (!CheckXr(xrGetInstanceProcAddr(instance_, "xrGetOpenGLESGraphicsRequirementsKHR", &fn),
                     "xrGetInstanceProcAddr(xrGetOpenGLESGraphicsRequirementsKHR)")) {
            return false;
        }
        auto getRequirements = reinterpret_cast<PFN_xrGetOpenGLESGraphicsRequirementsKHR>(fn);

        XrGraphicsRequirementsOpenGLESKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        if (!CheckXr(getRequirements(instance_, systemId_, &requirements),
                     "xrGetOpenGLESGraphicsRequirementsKHR")) {
            return false;
        }

        eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eglDisplay_ == EGL_NO_DISPLAY) {
            FQ_LOGE("eglGetDisplay failed");
            return false;
        }

        EGLint major = 0, minor = 0;
        if (!eglInitialize(eglDisplay_, &major, &minor)) {
            FQ_LOGE("eglInitialize failed: 0x%x", eglGetError());
            return false;
        }
        eglBindAPI(EGL_OPENGL_ES_API);

        const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };

        EGLint configCount = 0;
        if (!eglChooseConfig(eglDisplay_, configAttribs, &eglConfig_, 1, &configCount) || configCount < 1) {
            FQ_LOGE("No suitable EGL config: 0x%x", eglGetError());
            return false;
        }

        const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };
        eglContext_ = eglCreateContext(eglDisplay_, eglConfig_, EGL_NO_CONTEXT, contextAttribs);
        if (eglContext_ == EGL_NO_CONTEXT) {
            FQ_LOGE("eglCreateContext failed: 0x%x", eglGetError());
            return false;
        }

        const EGLint pbufferAttribs[] = {
            EGL_WIDTH, 16,
            EGL_HEIGHT, 16,
            EGL_NONE
        };
        eglSurface_ = eglCreatePbufferSurface(eglDisplay_, eglConfig_, pbufferAttribs);
        if (eglSurface_ == EGL_NO_SURFACE) {
            FQ_LOGE("eglCreatePbufferSurface failed: 0x%x", eglGetError());
            return false;
        }

        if (!eglMakeCurrent(eglDisplay_, eglSurface_, eglSurface_, eglContext_)) {
            FQ_LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
            return false;
        }

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
                     "xrEnumerateViewConfigurationViews(count)")) {
            return false;
        }
        if (viewCount != 2) {
            FQ_LOGE("Expected two stereo views, got %u", viewCount);
            return false;
        }

        std::array<XrViewConfigurationView, 2> configViews{
            XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW},
            XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW}
        };
        if (!CheckXr(xrEnumerateViewConfigurationViews(instance_, systemId_,
                        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount,
                        configViews.data()),
                     "xrEnumerateViewConfigurationViews")) {
            return false;
        }

        uint32_t formatCount = 0;
        if (!CheckXr(xrEnumerateSwapchainFormats(session_, 0, &formatCount, nullptr),
                     "xrEnumerateSwapchainFormats(count)")) {
            return false;
        }
        std::vector<int64_t> formats(formatCount);
        if (!CheckXr(xrEnumerateSwapchainFormats(session_, formatCount, &formatCount, formats.data()),
                     "xrEnumerateSwapchainFormats")) {
            return false;
        }

        int64_t chosenFormat = formats.empty() ? 0 : formats.front();
        for (const int64_t format : formats) {
            if (format == GL_SRGB8_ALPHA8) {
                chosenFormat = format;
                break;
            }
            if (format == GL_RGBA8) chosenFormat = format;
        }

        if (!chosenFormat) {
            FQ_LOGE("No OpenGL ES swapchain format available");
            return false;
        }

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

            if (!CheckXr(xrCreateSwapchain(session_, &info, &eye.handle), "xrCreateSwapchain")) {
                return false;
            }

            uint32_t imageCount = 0;
            if (!CheckXr(xrEnumerateSwapchainImages(eye.handle, 0, &imageCount, nullptr),
                         "xrEnumerateSwapchainImages(count)")) {
                return false;
            }

            eye.images.resize(imageCount);
            for (auto& image : eye.images) {
                image = {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR};
            }
            if (!CheckXr(xrEnumerateSwapchainImages(
                            eye.handle, imageCount, &imageCount,
                            reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())),
                         "xrEnumerateSwapchainImages")) {
                return false;
            }

            FQ_LOGI("Eye %u swapchain: %dx%d, %u images", i, eye.width, eye.height, imageCount);
        }

        uint32_t blendCount = 0;
        if (CheckXr(xrEnumerateEnvironmentBlendModes(instance_, systemId_,
                        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &blendCount, nullptr),
                    "xrEnumerateEnvironmentBlendModes(count)") && blendCount > 0) {
            std::vector<XrEnvironmentBlendMode> modes(blendCount);
            if (CheckXr(xrEnumerateEnvironmentBlendModes(instance_, systemId_,
                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, blendCount, &blendCount,
                            modes.data()),
                        "xrEnumerateEnvironmentBlendModes")) {
                blendMode_ = modes.front();
                for (auto mode : modes) {
                    if (mode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE) blendMode_ = mode;
                }
            }
        }

        return true;
    }

    bool CreateSceneRenderer() {
        program_ = CreateProgram();
        if (!program_) return false;

        // Three world-locked triangles at different positions/depths. Moving your head
        // should produce obvious parallax before Fallout assets enter the project.
        constexpr float vertices[] = {
            -0.55f, -0.45f, -2.0f,
             0.55f, -0.45f, -2.0f,
             0.00f,  0.55f, -2.0f,

             0.65f, -0.30f, -3.0f,
             1.35f, -0.30f, -3.0f,
             1.00f,  0.40f, -3.0f,

            -1.35f, -0.20f, -4.0f,
            -0.65f, -0.20f, -4.0f,
            -1.00f,  0.45f, -4.0f,
        };

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);
        glGenBuffers(1, &vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
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
                        FQ_LOGI("Q3 STEREO SESSION STARTED");
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

    void RenderFrame() {
        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        if (!CheckXr(xrWaitFrame(session_, &waitInfo, &frameState), "xrWaitFrame")) return;

        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        if (!CheckXr(xrBeginFrame(session_, &beginInfo), "xrBeginFrame")) return;

        std::array<XrCompositionLayerProjectionView, 2> projectionViews{
            XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
            XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}
        };
        XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        const XrCompositionLayerBaseHeader* layers[1]{};
        uint32_t layerCount = 0;

        if (frameState.shouldRender) {
            std::array<XrView, 2> views{
                XrView{XR_TYPE_VIEW},
                XrView{XR_TYPE_VIEW}
            };
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

                for (uint32_t eyeIndex = 0; eyeIndex < 2; ++eyeIndex) {
                    RenderEye(eyeIndex, views[eyeIndex], projectionViews[eyeIndex]);
                }

                projectionLayer.space = localSpace_;
                projectionLayer.viewCount = 2;
                projectionLayer.views = projectionViews.data();
                layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer);
                layerCount = 1;

                if ((frameCounter_++ % 120) == 0) {
                    const auto& p = views[0].pose.position;
                    FQ_LOGI("Head tracking live: %.3f %.3f %.3f", p.x, p.y, p.z);
                }
            }
        }

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = blendMode_;
        endInfo.layerCount = layerCount;
        endInfo.layers = layerCount ? layers : nullptr;
        CheckXr(xrEndFrame(session_, &endInfo), "xrEndFrame");
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
        glViewport(0, 0, eye.width, eye.height);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            FQ_LOGE("Incomplete eye framebuffer");
        } else {
            glDisable(GL_DEPTH_TEST);
            glClearColor(0.025f, 0.045f, 0.030f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            const Mat4 projection = ProjectionFromFov(view.fov, 0.05f, 100.0f);
            const Mat4 camera = ViewFromPose(view.pose);
            const Mat4 mvp = Multiply(projection, camera);

            glUseProgram(program_);
            glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, mvp.m);
            glBindVertexArray(vao_);

            glUniform3f(colorLocation_, 0.95f, 0.72f, 0.12f);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glUniform3f(colorLocation_, 0.30f, 0.80f, 0.42f);
            glDrawArrays(GL_TRIANGLES, 3, 3);
            glUniform3f(colorLocation_, 0.58f, 0.72f, 0.34f);
            glDrawArrays(GL_TRIANGLES, 6, 3);
            glFlush();
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
    uint64_t frameCounter_{0};
};

} // namespace

extern "C" void android_main(struct android_app* app) {
    app_dummy();
    FQ_LOGI("FalloutQuest native OpenXR process booting");

    FalloutQuestXr xr(app);
    if (!xr.Initialize()) {
        FQ_LOGE("FalloutQuest OpenXR initialization failed");
        return;
    }

    xr.Run();
    xr.Shutdown();
    FQ_LOGI("FalloutQuest OpenXR process stopped");
}
