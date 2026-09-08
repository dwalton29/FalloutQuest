# Q12.8: authored Fallout 3 ImageSpace post-processing.
#
# The scene/material/terrain renderers stay untouched. Each OpenXR eye renders
# into one reusable off-screen colour buffer, then a single fullscreen pass
# applies the active CELL -> XCIM -> IMGS cinematic/HDR-bloom parameters before
# presenting to the existing Quest swapchain. This creates a reusable final-frame
# stage for later IMAD/weather/time-of-day effects without baking colour grading
# into every material shader.

# -----------------------------------------------------------------------------
# Runtime ImageSpace parser is shared by scene transition + eye composite.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-environment-q1000.h\""
    "#include \"fo3-environment-q1000.h\"\n#include \"fo3-imagespace-q1280.h\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1280_OLD_TRANSITION_COMPLETE [==[
    CompleteFo3CellTransitionQ74(request.cellFormId);
]==])
set(Q1280_NEW_TRANSITION_COMPLETE [==[
    CompleteFo3CellTransitionQ74(request.cellFormId);
    LoadFo3ImageSpaceQ1280(request.cellFormId, request.worldspaceFormId);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1280_OLD_TRANSITION_COMPLETE}" Q1280_TRANSITION_POS)
if(Q1280_TRANSITION_POS EQUAL -1)
    message(FATAL_ERROR "Q12.8 could not find transition completion hook")
endif()
string(REPLACE "${Q1280_OLD_TRANSITION_COMPLETE}" "${Q1280_NEW_TRANSITION_COMPLETE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Post-process GPU stage. It lives before the Q4 include so the OpenXR eye class
# can call it directly. RGBA16F is preferred when the Quest driver exposes a
# colour-buffer float extension; RGBA8 is a complete fallback, so a post effect
# can never make the eye framebuffer unusable.
# -----------------------------------------------------------------------------
set(Q1280_POST_HELPERS [==[
GLuint q1280PostFbo = 0u;
GLuint q1280PostColor = 0u;
GLuint q1280PostProgram = 0u;
GLuint q1280PostVao = 0u;
GLsizei q1280PostWidth = 0;
GLsizei q1280PostHeight = 0;
GLenum q1280PostInternalFormat = GL_RGBA8;
bool q1280PostActive = false;
bool q1280PostLoggedGpu = false;

GLint q1280SceneLocation = -1;
GLint q1280TexelLocation = -1;
GLint q1280FlagsLocation = -1;
GLint q1280SaturationLocation = -1;
GLint q1280ContrastAvgLocation = -1;
GLint q1280ContrastLocation = -1;
GLint q1280BrightnessLocation = -1;
GLint q1280TintColorLocation = -1;
GLint q1280TintValueLocation = -1;
GLint q1280BloomRadiusLocation = -1;
GLint q1280BloomScaleLocation = -1;
GLint q1280BloomThresholdLocation = -1;
GLint q1280BloomAlphaLocation = -1;

GLuint Q1280CompilePostShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Q6H_LOGE("Q12.8 POST shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0u;
    }
    return shader;
}

bool Q1280EnsurePostProgram() {
    if (q1280PostProgram && q1280PostVao) return true;

    static const char* vertexSource = R"(
        #version 300 es
        precision highp float;
        out vec2 vUv;
        void main() {
            vec2 p;
            if (gl_VertexID == 0) p = vec2(-1.0, -1.0);
            else if (gl_VertexID == 1) p = vec2(3.0, -1.0);
            else p = vec2(-1.0, 3.0);
            vUv = p * 0.5 + 0.5;
            gl_Position = vec4(p, 0.0, 1.0);
        }
    )";

    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;
        in vec2 vUv;
        uniform sampler2D uScene;
        uniform vec2 uTexel;
        uniform int uFlags;
        uniform float uSaturation;
        uniform float uContrastAvg;
        uniform float uContrast;
        uniform float uBrightness;
        uniform vec3 uTintColor;
        uniform float uTintValue;
        uniform float uBloomRadius;
        uniform float uBloomScale;
        uniform float uBloomThreshold;
        uniform float uBloomAlpha;
        out vec4 fragColor;

        float Q1280Lum(vec3 c) {
            return dot(c, vec3(0.2126, 0.7152, 0.0722));
        }

        vec3 Q1280Bright(vec2 uv) {
            vec3 c = texture(uScene, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
            float lum = Q1280Lum(c);
            float threshold = clamp(uBloomThreshold, 0.05, 0.98);
            float mask = smoothstep(threshold, min(threshold + 0.30, 1.0), lum);
            return c * mask;
        }

        void main() {
            vec3 colour = texture(uScene, vUv).rgb;

            // Quest-safe one-pass bloom approximation. The authored IMGS blur
            // radius chooses the sample spread; Bright Scale/Clamp and exterior
            // bloom alpha remain the authored strength controls.
            if (uBloomAlpha > 0.0001 && uBloomScale > 0.0001) {
                vec2 d = uTexel * clamp(uBloomRadius, 1.0, 12.0) * 0.85;
                vec3 bloom = Q1280Bright(vUv + vec2( d.x, 0.0));
                bloom += Q1280Bright(vUv + vec2(-d.x, 0.0));
                bloom += Q1280Bright(vUv + vec2(0.0,  d.y));
                bloom += Q1280Bright(vUv + vec2(0.0, -d.y));
                bloom += Q1280Bright(vUv + vec2( d.x,  d.y));
                bloom += Q1280Bright(vUv + vec2(-d.x,  d.y));
                bloom += Q1280Bright(vUv + vec2( d.x, -d.y));
                bloom += Q1280Bright(vUv + vec2(-d.x, -d.y));
                colour += (bloom * 0.125) * uBloomScale * uBloomAlpha;
            }

            // IMGS flags: 0x01 saturation, 0x02 contrast, 0x04 tint,
            // 0x08 brightness. Values remain data-driven from Fallout3.esm.
            if ((uFlags & 1) != 0) {
                float lum = Q1280Lum(colour);
                colour = mix(vec3(lum), colour, uSaturation);
            }
            if ((uFlags & 2) != 0) {
                colour = (colour - vec3(uContrastAvg)) * uContrast + vec3(uContrastAvg);
            }
            if ((uFlags & 4) != 0) {
                float lum = Q1280Lum(max(colour, vec3(0.0)));
                vec3 tinted = lum * max(uTintColor, vec3(0.0));
                colour = mix(colour, tinted, clamp(uTintValue, 0.0, 1.0));
            }
            if ((uFlags & 8) != 0) {
                colour *= uBrightness;
            }

            // Preserve the renderer's established exposure while allowing HDR
            // bloom/emission to roll into the display range without NaN/negative
            // output. Eye adaptation is intentionally deferred until game time.
            colour = max(colour, vec3(0.0));
            fragColor = vec4(colour, 1.0);
        }
    )";

    const GLuint vs = Q1280CompilePostShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fs = Q1280CompilePostShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    q1280PostProgram = glCreateProgram();
    glAttachShader(q1280PostProgram, vs);
    glAttachShader(q1280PostProgram, fs);
    glLinkProgram(q1280PostProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(q1280PostProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(q1280PostProgram, sizeof(log), nullptr, log);
        Q6H_LOGE("Q12.8 POST program link failed: %s", log);
        glDeleteProgram(q1280PostProgram);
        q1280PostProgram = 0u;
        return false;
    }

    glGenVertexArrays(1, &q1280PostVao);
    q1280SceneLocation = glGetUniformLocation(q1280PostProgram, "uScene");
    q1280TexelLocation = glGetUniformLocation(q1280PostProgram, "uTexel");
    q1280FlagsLocation = glGetUniformLocation(q1280PostProgram, "uFlags");
    q1280SaturationLocation = glGetUniformLocation(q1280PostProgram, "uSaturation");
    q1280ContrastAvgLocation = glGetUniformLocation(q1280PostProgram, "uContrastAvg");
    q1280ContrastLocation = glGetUniformLocation(q1280PostProgram, "uContrast");
    q1280BrightnessLocation = glGetUniformLocation(q1280PostProgram, "uBrightness");
    q1280TintColorLocation = glGetUniformLocation(q1280PostProgram, "uTintColor");
    q1280TintValueLocation = glGetUniformLocation(q1280PostProgram, "uTintValue");
    q1280BloomRadiusLocation = glGetUniformLocation(q1280PostProgram, "uBloomRadius");
    q1280BloomScaleLocation = glGetUniformLocation(q1280PostProgram, "uBloomScale");
    q1280BloomThresholdLocation = glGetUniformLocation(q1280PostProgram, "uBloomThreshold");
    q1280BloomAlphaLocation = glGetUniformLocation(q1280PostProgram, "uBloomAlpha");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0;
}

bool Q1280DriverSupportsHalfFloatTarget() {
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (!extensions) return false;
    return std::strstr(extensions, "GL_EXT_color_buffer_half_float") != nullptr ||
           std::strstr(extensions, "GL_EXT_color_buffer_float") != nullptr;
}

bool Q1280AllocatePostTarget(GLsizei width, GLsizei height) {
    if (width <= 0 || height <= 0) return false;
    if (!q1280PostFbo) glGenFramebuffers(1, &q1280PostFbo);
    if (!q1280PostColor) glGenTextures(1, &q1280PostColor);

    if (q1280PostWidth == width && q1280PostHeight == height && q1280PostColor) return true;

    glBindTexture(GL_TEXTURE_2D, q1280PostColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    const bool preferHalf = Q1280DriverSupportsHalfFloatTarget();
    q1280PostInternalFormat = preferHalf ? GL_RGBA16F : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, q1280PostInternalFormat,
                 width, height, 0, GL_RGBA,
                 preferHalf ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, q1280PostFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, q1280PostColor, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE && preferHalf) {
        q1280PostInternalFormat = GL_RGBA8;
        glBindTexture(GL_TEXTURE_2D, q1280PostColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, q1280PostFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, q1280PostColor, 0);
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    }
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Q6H_LOGE("Q12.8 POST target incomplete: status=0x%X size=%dx%d",
                 status, width, height);
        return false;
    }

    q1280PostWidth = width;
    q1280PostHeight = height;
    if (!q1280PostLoggedGpu) {
        q1280PostLoggedGpu = true;
        Q6H_LOGI("Q12.8 POST GPU READY: size=%dx%d format=%s onePassBloom=8tap stereoSequential=1",
                 width, height,
                 q1280PostInternalFormat == GL_RGBA16F ? "RGBA16F" : "RGBA8");
    }
    return true;
}

bool Q1280BeginEyePostQ1280(GLuint swapchainFbo, GLsizei width, GLsizei height) {
    q1280PostActive = false;
    if (!Q1280EnsurePostProgram() || !Q1280AllocatePostTarget(width, height)) {
        glBindFramebuffer(GL_FRAMEBUFFER, swapchainFbo);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, q1280PostFbo);
    q1280PostActive = true;
    return true;
}

void Q1280CompositeEyePostQ1280(GLuint swapchainFbo, GLsizei width, GLsizei height) {
    if (!q1280PostActive || !q1280PostProgram || !q1280PostColor) return;

    GLint previousProgram = 0, previousVao = 0, previousActiveTexture = 0;
    GLint previousTexture0 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    glBindFramebuffer(GL_FRAMEBUFFER, swapchainFbo);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glUseProgram(q1280PostProgram);
    glBindVertexArray(q1280PostVao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, q1280PostColor);
    glUniform1i(q1280SceneLocation, 0);
    glUniform2f(q1280TexelLocation,
                1.0f / static_cast<float>(std::max<GLsizei>(width, 1)),
                1.0f / static_cast<float>(std::max<GLsizei>(height, 1)));

    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    const int flags = image.valid ? static_cast<int>(image.cinematicFlags) : 0;
    glUniform1i(q1280FlagsLocation, flags);
    glUniform1f(q1280SaturationLocation, image.valid ? image.cinematicSaturation : 1.0f);
    glUniform1f(q1280ContrastAvgLocation, image.valid ? image.cinematicContrastAvgLum : 0.5f);
    glUniform1f(q1280ContrastLocation, image.valid ? image.cinematicContrast : 1.0f);
    glUniform1f(q1280BrightnessLocation, image.valid ? image.cinematicBrightness : 1.0f);
    glUniform3fv(q1280TintColorLocation, 1,
                 image.valid ? image.cinematicTint : Fo3ImageSpaceQ1280{}.cinematicTint);
    glUniform1f(q1280TintValueLocation, image.valid ? image.cinematicTintValue : 0.0f);

    const float authoredRadius = image.valid
        ? std::max(image.hdrBlurRadius, image.bloomBlurRadius) : 1.0f;
    const float bloomAlpha = image.valid
        ? std::clamp(image.bloomAlphaExterior, 0.0f, 1.0f) : 0.0f;
    glUniform1f(q1280BloomRadiusLocation, authoredRadius);
    glUniform1f(q1280BloomScaleLocation,
                image.valid ? std::clamp(image.hdrBrightScale, 0.0f, 4.0f) : 0.0f);
    glUniform1f(q1280BloomThresholdLocation,
                image.valid ? std::clamp(image.hdrBrightClamp, 0.05f, 0.98f) : 0.98f);
    // Full Bethesda bloom is multi-pass; keep the first Quest pass conservative
    // while still scaling from the authored exterior alpha.
    glUniform1f(q1280BloomAlphaLocation, bloomAlpha * 0.35f);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    static uint32_t lastLoggedImageSpace = 0xFFFFFFFFu;
    const uint32_t currentImageSpace = image.valid ? image.imageSpaceFormId : 0u;
    if (lastLoggedImageSpace != currentImageSpace) {
        lastLoggedImageSpace = currentImageSpace;
        Q6H_LOGI("Q12.8 POST ACTIVE: IMGS=%08X flags=0x%02X saturation=%.3f contrast=%.3f brightness=%.3f tintValue=%.3f bloomExterior=%.3f bloomApplied=%.3f eyeAdapt=deferred",
                 currentImageSpace, image.valid ? image.cinematicFlags : 0u,
                 image.valid ? image.cinematicSaturation : 1.0f,
                 image.valid ? image.cinematicContrast : 1.0f,
                 image.valid ? image.cinematicBrightness : 1.0f,
                 image.valid ? image.cinematicTintValue : 0.0f,
                 image.valid ? image.bloomAlphaExterior : 0.0f,
                 bloomAlpha * 0.35f);
    }

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    glDepthMask(previousDepthMask);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    q1280PostActive = false;
}

void Q1280ShutdownPostQ1280() {
    if (q1280PostColor) glDeleteTextures(1, &q1280PostColor);
    if (q1280PostFbo) glDeleteFramebuffers(1, &q1280PostFbo);
    if (q1280PostVao) glDeleteVertexArrays(1, &q1280PostVao);
    if (q1280PostProgram) glDeleteProgram(q1280PostProgram);
    q1280PostColor = 0u;
    q1280PostFbo = 0u;
    q1280PostVao = 0u;
    q1280PostProgram = 0u;
    q1280PostWidth = 0;
    q1280PostHeight = 0;
    q1280PostActive = false;
}

]==])

set(Q1280_DELETE_MARKER [==[
void Q6HDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1280_DELETE_MARKER}" Q1280_DELETE_POS)
if(Q1280_DELETE_POS EQUAL -1)
    message(FATAL_ERROR "Q12.8 could not find Q6H framebuffer cleanup")
endif()
string(REPLACE "${Q1280_DELETE_MARKER}"
       "${Q1280_POST_HELPERS}${Q1280_DELETE_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(REPLACE
    "void Q6HDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {\n    glDeleteFramebuffers(n, framebuffers);"
    "void Q6HDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {\n    Q1280ShutdownPostQ1280();\n    glDeleteFramebuffers(n, framebuffers);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Patch the actual Q10.0-authored OpenXR eye source: keep the swapchain attached
# to its FBO, switch rendering to the post target, then composite back immediately
# before the existing glFlush/release.
# -----------------------------------------------------------------------------
set(Q1280_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1000-q4-generated.cpp")
if(NOT EXISTS "${Q1280_Q4_INPUT}")
    message(FATAL_ERROR "Q12.8 expected generated Q10.0 eye source at ${Q1280_Q4_INPUT}")
endif()
file(READ "${Q1280_Q4_INPUT}" Q1280_Q4_SOURCE)

set(Q1280_OLD_EYE_TARGET [==[
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               eye.images[imageIndex].image, 0);
        glViewport(0, 0, eye.width, eye.height);
]==])
set(Q1280_NEW_EYE_TARGET [==[
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               eye.images[imageIndex].image, 0);
        Q1280BeginEyePostQ1280(framebuffer_, eye.width, eye.height);
        glViewport(0, 0, eye.width, eye.height);
]==])
string(FIND "${Q1280_Q4_SOURCE}" "${Q1280_OLD_EYE_TARGET}" Q1280_EYE_TARGET_POS)
if(Q1280_EYE_TARGET_POS EQUAL -1)
    message(FATAL_ERROR "Q12.8 could not find OpenXR eye framebuffer target")
endif()
string(REPLACE "${Q1280_OLD_EYE_TARGET}" "${Q1280_NEW_EYE_TARGET}"
       Q1280_Q4_SOURCE "${Q1280_Q4_SOURCE}")

set(Q1280_OLD_FLUSH [==[
            glFlush();
]==])
set(Q1280_NEW_FLUSH [==[
            Q1280CompositeEyePostQ1280(framebuffer_, eye.width, eye.height);
            glFlush();
]==])
string(FIND "${Q1280_Q4_SOURCE}" "${Q1280_OLD_FLUSH}" Q1280_FLUSH_POS)
if(Q1280_FLUSH_POS EQUAL -1)
    message(FATAL_ERROR "Q12.8 could not find OpenXR eye flush")
endif()
string(REPLACE "${Q1280_OLD_FLUSH}" "${Q1280_NEW_FLUSH}"
       Q1280_Q4_SOURCE "${Q1280_Q4_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp" "${Q1280_Q4_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/q1000-q4-generated.cpp\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp\""
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Drift guards: all four pieces must exist together or fail the build rather
# than accidentally shipping a direct-render eye with half a post pipeline.
string(FIND "${Q6H_NATIVE_SOURCE}" "Q12.8 POST GPU READY" Q1280_GPU_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "LoadFo3ImageSpaceQ1280(request.cellFormId" Q1280_LOAD_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "q1280-q4-generated.cpp" Q1280_INCLUDE_OK)
string(FIND "${Q1280_Q4_SOURCE}" "Q1280CompositeEyePostQ1280" Q1280_COMPOSITE_OK)
if(Q1280_GPU_OK EQUAL -1 OR Q1280_LOAD_OK EQUAL -1 OR
   Q1280_INCLUDE_OK EQUAL -1 OR Q1280_COMPOSITE_OK EQUAL -1)
    message(FATAL_ERROR "Q12.8 ImageSpace post patch drifted: gpu=${Q1280_GPU_OK} load=${Q1280_LOAD_OK} include=${Q1280_INCLUDE_OK} composite=${Q1280_COMPOSITE_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q12.8 authored CELL/IMGS final-frame post-processing enabled")
