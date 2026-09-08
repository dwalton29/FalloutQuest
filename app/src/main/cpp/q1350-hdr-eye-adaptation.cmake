# Q13.5: Quest-safe Fallout 3 HDR eye adaptation / exposure bridge.
#
# Q13.4 fixed the proven ordering bug by moving cinematic controls out of raw
# linear HDR. Q13.5 adds the missing temporal HDR adaptation stage without a
# per-frame GPU->CPU stall. A tiny 1x1 GPU history texture stores exposure; once
# per stereo frame (eye 0) a 4x4 log-luminance probe of the current HDR eye scene
# updates that history. Both eyes then sample the exact same exposure.
#
# GECK documents Eye Adapt Speed as a 0..1 chase control, with 0 behaving as
# immediate adaptation and 1 holding the adapted value. We therefore use it as
# the previous-frame retention factor. Target LUM is used conservatively as the
# desired scene-luminance reference, with exposure clamped to 0.5..2.0 because
# Fallout 3's exact legacy Target LUM transfer equation is not documented well
# enough to justify unbounded reconstruction.

# -----------------------------------------------------------------------------
# Add shared exposure state beside the Q12.8 post state.
# -----------------------------------------------------------------------------
set(Q1350_OLD_GLOBAL [==[
GLint q1280BloomAlphaLocation = -1;
]==])
set(Q1350_NEW_GLOBAL [==[
GLint q1280BloomAlphaLocation = -1;
GLint q1350ExposureLocation = -1;

GLuint q1350AdaptProgram = 0u;
GLuint q1350AdaptVao = 0u;
GLuint q1350AdaptFbo = 0u;
GLuint q1350AdaptTexture[2]{0u, 0u};
int q1350AdaptIndex = 0;
uint64_t q1350AdaptFrame = 0u;
bool q1350AdaptLoggedReady = false;

GLint q1350AdaptSceneLocation = -1;
GLint q1350AdaptPrevLocation = -1;
GLint q1350AdaptTargetLocation = -1;
GLint q1350AdaptSpeedLocation = -1;
GLint q1350AdaptFirstLocation = -1;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_OLD_GLOBAL}" Q1350_GLOBAL_POS)
if(Q1350_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 post global block")
endif()
string(REPLACE "${Q1350_OLD_GLOBAL}" "${Q1350_NEW_GLOBAL}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Add one exposure sampler to Q13.4's post shader and multiply the linear HDR
# frame after bloom but before the display-domain cinematic controls.
# -----------------------------------------------------------------------------
set(Q1350_OLD_SHADER_UNIFORM [==[
        uniform float uBloomAlpha;
        out vec4 fragColor;
]==])
set(Q1350_NEW_SHADER_UNIFORM [==[
        uniform float uBloomAlpha;
        uniform sampler2D uExposureQ1350;
        out vec4 fragColor;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_OLD_SHADER_UNIFORM}" Q1350_SHADER_UNIFORM_POS)
if(Q1350_SHADER_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 post shader uniforms")
endif()
string(REPLACE "${Q1350_OLD_SHADER_UNIFORM}" "${Q1350_NEW_SHADER_UNIFORM}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1350_OLD_CINEMATIC_ENTRY [==[
            vec3 cinematic = Q1340LinearToSrgb(colour);
]==])
set(Q1350_NEW_CINEMATIC_ENTRY [==[
            // Q13.5 exposure is packed across RG in a 1x1 RGBA8 history texture.
            // R carries the high byte and G the fractional byte of exposure/4.
            vec2 packedExposureQ1350 = texture(uExposureQ1350, vec2(0.5)).rg;
            float exposureQ1350 = clamp(
                (packedExposureQ1350.r + packedExposureQ1350.g / 255.0) * 4.0,
                0.5, 2.0);
            colour *= exposureQ1350;

            vec3 cinematic = Q1340LinearToSrgb(colour);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_OLD_CINEMATIC_ENTRY}" Q1350_CINEMATIC_POS)
if(Q1350_CINEMATIC_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q13.4 cinematic entry")
endif()
string(REPLACE "${Q1350_OLD_CINEMATIC_ENTRY}" "${Q1350_NEW_CINEMATIC_ENTRY}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Fetch the new sampler location when the Q12.8 post program links.
set(Q1350_OLD_LOCATION [==[
    q1280BloomAlphaLocation = glGetUniformLocation(q1280PostProgram, "uBloomAlpha");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0;
]==])
set(Q1350_NEW_LOCATION [==[
    q1280BloomAlphaLocation = glGetUniformLocation(q1280PostProgram, "uBloomAlpha");
    q1350ExposureLocation = glGetUniformLocation(q1280PostProgram, "uExposureQ1350");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 && q1350ExposureLocation >= 0;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_OLD_LOCATION}" Q1350_LOCATION_POS)
if(Q1350_LOCATION_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 post uniform lookup tail")
endif()
string(REPLACE "${Q1350_OLD_LOCATION}" "${Q1350_NEW_LOCATION}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# GPU adaptation pass. It uses a 4x4 fixed log-luminance probe, which is tiny
# enough for Quest and much less sensitive to the sun disc or a single black
# corner than a straight arithmetic mean. History remains GPU resident.
# -----------------------------------------------------------------------------
set(Q1350_ADAPT_HELPERS [==[
bool Q1350EnsureAdaptationQ1350() {
    if (q1350AdaptProgram && q1350AdaptVao && q1350AdaptFbo &&
        q1350AdaptTexture[0] && q1350AdaptTexture[1]) return true;

    static const char* adaptVertex = R"(
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

    static const char* adaptFragment = R"(
        #version 300 es
        precision highp float;
        uniform sampler2D uScene;
        uniform sampler2D uPrev;
        uniform float uTargetLum;
        uniform float uEyeAdaptSpeed;
        uniform int uFirstFrame;
        out vec4 fragColor;

        float Q1350Lum(vec3 c) {
            return dot(max(c, vec3(0.0)), vec3(0.2126, 0.7152, 0.0722));
        }

        float Q1350UnpackExposure(vec2 rg) {
            return (rg.r + rg.g / 255.0) * 4.0;
        }

        vec2 Q1350PackExposure(float exposure) {
            float normalized = clamp(exposure / 4.0, 0.0, 1.0);
            float scaled = normalized * 255.0;
            float highByte = floor(scaled);
            float lowByte = fract(scaled);
            return vec2(highByte / 255.0, lowByte);
        }

        void main() {
            float sumLog = 0.0;
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    vec2 uv = (vec2(float(x), float(y)) + vec2(0.5)) / 4.0;
                    float lum = max(Q1350Lum(texture(uScene, uv).rgb), 0.01);
                    sumLog += log(lum);
                }
            }
            float sceneLum = exp(sumLog / 16.0);
            float desiredExposure = clamp(uTargetLum / max(sceneLum, 0.02), 0.5, 2.0);
            float previousExposure = clamp(Q1350UnpackExposure(texture(uPrev, vec2(0.5)).rg),
                                           0.5, 2.0);
            float retention = clamp(uEyeAdaptSpeed, 0.0, 0.9995);
            float adaptedExposure = (uFirstFrame != 0)
                ? desiredExposure
                : mix(desiredExposure, previousExposure, retention);

            // RG = high precision exposure; B = scene luminance / 4; A = desired
            // exposure / 4. B/A exist only for infrequent diagnostic readback.
            fragColor = vec4(Q1350PackExposure(adaptedExposure),
                             clamp(sceneLum / 4.0, 0.0, 1.0),
                             clamp(desiredExposure / 4.0, 0.0, 1.0));
        }
    )";

    const GLuint vs = Q1280CompilePostShader(GL_VERTEX_SHADER, adaptVertex);
    const GLuint fs = Q1280CompilePostShader(GL_FRAGMENT_SHADER, adaptFragment);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    q1350AdaptProgram = glCreateProgram();
    glAttachShader(q1350AdaptProgram, vs);
    glAttachShader(q1350AdaptProgram, fs);
    glLinkProgram(q1350AdaptProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(q1350AdaptProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(q1350AdaptProgram, sizeof(log), nullptr, log);
        Q6H_LOGE("Q13.5 HDR ADAPT program link failed: %s", log);
        glDeleteProgram(q1350AdaptProgram);
        q1350AdaptProgram = 0u;
        return false;
    }

    glGenVertexArrays(1, &q1350AdaptVao);
    glGenFramebuffers(1, &q1350AdaptFbo);
    glGenTextures(2, q1350AdaptTexture);

    // Initial packed exposure = 1.0. Pack(exposure/4 = .25) -> bytes 63,191.
    const uint8_t initialPixel[4]{63u, 191u, 64u, 64u};
    for (GLuint tex : q1350AdaptTexture) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, initialPixel);
    }

    q1350AdaptSceneLocation = glGetUniformLocation(q1350AdaptProgram, "uScene");
    q1350AdaptPrevLocation = glGetUniformLocation(q1350AdaptProgram, "uPrev");
    q1350AdaptTargetLocation = glGetUniformLocation(q1350AdaptProgram, "uTargetLum");
    q1350AdaptSpeedLocation = glGetUniformLocation(q1350AdaptProgram, "uEyeAdaptSpeed");
    q1350AdaptFirstLocation = glGetUniformLocation(q1350AdaptProgram, "uFirstFrame");

    if (q1350AdaptSceneLocation < 0 || q1350AdaptPrevLocation < 0 ||
        q1350AdaptTargetLocation < 0 || q1350AdaptSpeedLocation < 0 ||
        q1350AdaptFirstLocation < 0) {
        Q6H_LOGE("Q13.5 HDR ADAPT missing shader uniforms");
        return false;
    }

    q1350AdaptIndex = 0;
    q1350AdaptFrame = 0u;
    if (!q1350AdaptLoggedReady) {
        q1350AdaptLoggedReady = true;
        Q6H_LOGI("Q13.5 HDR ADAPT READY: probe=4x4-log-average history=RGBA8-packed16 gpuOnly=1 update=eye0-once-per-stereo-frame exposureClamp=0.500..2.000 semantics=eyeAdapt-retention");
    }
    return true;
}

void Q1350UpdateExposureQ1350() {
    if (!q1280PostColor || !Q1350EnsureAdaptationQ1350()) return;

    GLint oldFramebuffer = 0, oldProgram = 0, oldVao = 0, oldActiveTexture = 0;
    GLint oldTexture0 = 0, oldTexture1 = 0;
    GLint oldViewport[4]{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    const GLboolean oldDepth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLboolean oldDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);

    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture1);

    const int nextIndex = 1 - q1350AdaptIndex;
    glBindFramebuffer(GL_FRAMEBUFFER, q1350AdaptFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, q1350AdaptTexture[nextIndex], 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Q6H_LOGE("Q13.5 HDR ADAPT FBO incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFramebuffer));
        return;
    }

    glViewport(0, 0, 1, 1);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glUseProgram(q1350AdaptProgram);
    glBindVertexArray(q1350AdaptVao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, q1280PostColor);
    glUniform1i(q1350AdaptSceneLocation, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, q1350AdaptTexture[q1350AdaptIndex]);
    glUniform1i(q1350AdaptPrevLocation, 1);

    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    const float targetLum = image.valid ? std::clamp(image.hdrTargetLum, 0.05f, 4.0f) : 1.0f;
    const float eyeSpeed = image.valid ? std::clamp(image.hdrEyeAdaptSpeed, 0.0f, 0.9995f) : 0.5f;
    glUniform1f(q1350AdaptTargetLocation, targetLum);
    glUniform1f(q1350AdaptSpeedLocation, eyeSpeed);
    glUniform1i(q1350AdaptFirstLocation, q1350AdaptFrame == 0u ? 1 : 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    q1350AdaptIndex = nextIndex;
    ++q1350AdaptFrame;

    // Diagnostic-only 1x1 readback: first update and then roughly every two
    // seconds at 72 Hz. Rendering itself never depends on this CPU readback.
    if (q1350AdaptFrame == 1u || (q1350AdaptFrame % 144u) == 0u) {
        uint8_t pixel[4]{};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        const float packed = static_cast<float>(pixel[0]) / 255.0f +
                             (static_cast<float>(pixel[1]) / 255.0f) / 255.0f;
        const float exposure = packed * 4.0f;
        const float sceneLum = (static_cast<float>(pixel[2]) / 255.0f) * 4.0f;
        const float desiredExposure = (static_cast<float>(pixel[3]) / 255.0f) * 4.0f;
        Q6H_LOGI("Q13.5 HDR EXPOSURE: sceneLum=%.4f targetLum=%.3f desiredExposure=%.4f adaptedExposure=%.4f eyeAdaptSpeed=%.3f update=%llu stereoShared=1 gpuHistory=1 mapping=target-over-logAverage clamp=0.5..2.0",
                 sceneLum, targetLum, desiredExposure, exposure, eyeSpeed,
                 static_cast<unsigned long long>(q1350AdaptFrame));
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture1));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture0));
    glActiveTexture(static_cast<GLenum>(oldActiveTexture));
    glBindVertexArray(static_cast<GLuint>(oldVao));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glDepthMask(oldDepthMask);
    if (oldDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (oldBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFramebuffer));
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

]==])
set(Q1350_HELPER_MARKER [==[
bool Q1280EnsurePostProgram() {
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_HELPER_MARKER}" Q1350_HELPER_POS)
if(Q1350_HELPER_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 post program entry")
endif()
string(REPLACE "${Q1350_HELPER_MARKER}"
       "${Q1350_ADAPT_HELPERS}${Q1350_HELPER_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Bind the shared exposure history alongside uScene during final composite.
# Preserve texture-unit 1 state because older material code also uses it.
# -----------------------------------------------------------------------------
set(Q1350_OLD_SAVE_TEX [==[
    GLint previousTexture0 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
]==])
set(Q1350_NEW_SAVE_TEX [==[
    GLint previousTexture0 = 0, previousTexture1 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);
    glActiveTexture(GL_TEXTURE0);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_OLD_SAVE_TEX}" Q1350_SAVE_TEX_POS)
if(Q1350_SAVE_TEX_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 composite texture-state save")
endif()
string(REPLACE "${Q1350_OLD_SAVE_TEX}" "${Q1350_NEW_SAVE_TEX}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1350_OLD_BIND [==[
    glBindTexture(GL_TEXTURE_2D, q1280PostColor);
    glUniform1i(q1280SceneLocation, 0);
    glUniform2f(q1280TexelLocation,
]==])
set(Q1350_NEW_BIND [==[
    glBindTexture(GL_TEXTURE_2D, q1280PostColor);
    glUniform1i(q1280SceneLocation, 0);
    if (Q1350EnsureAdaptationQ1350()) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, q1350AdaptTexture[q1350AdaptIndex]);
        glUniform1i(q1350ExposureLocation, 1);
        glActiveTexture(GL_TEXTURE0);
    }
    glUniform2f(q1280TexelLocation,
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_OLD_BIND}" Q1350_BIND_POS)
if(Q1350_BIND_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 scene bind")
endif()
string(REPLACE "${Q1350_OLD_BIND}" "${Q1350_NEW_BIND}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1350_OLD_RESTORE_TEX [==[
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
]==])
set(Q1350_NEW_RESTORE_TEX [==[
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_OLD_RESTORE_TEX}" Q1350_RESTORE_TEX_POS)
if(Q1350_RESTORE_TEX_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 texture-state restore")
endif()
string(REPLACE "${Q1350_OLD_RESTORE_TEX}" "${Q1350_NEW_RESTORE_TEX}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Composite wrapper updates exposure only from eye 0, then both eyes use the
# shared history texture. RenderFrame is authored sequentially eye 0 -> eye 1.
set(Q1350_COMPOSITE_WRAPPER [==[
void Q1350CompositeEyePostQ1350(uint32_t eyeIndex, GLuint swapchainFbo,
                                GLsizei width, GLsizei height) {
    if (eyeIndex == 0u) Q1350UpdateExposureQ1350();
    Q1280CompositeEyePostQ1280(swapchainFbo, width, height);
}

]==])
set(Q1350_SHUTDOWN_MARKER [==[
void Q1280ShutdownPostQ1280() {
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_SHUTDOWN_MARKER}" Q1350_SHUTDOWN_POS)
if(Q1350_SHUTDOWN_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 shutdown")
endif()
string(REPLACE "${Q1350_SHUTDOWN_MARKER}"
       "${Q1350_COMPOSITE_WRAPPER}${Q1350_SHUTDOWN_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1350_OLD_SHUTDOWN [==[
void Q1280ShutdownPostQ1280() {
    if (q1280PostColor) glDeleteTextures(1, &q1280PostColor);
]==])
set(Q1350_NEW_SHUTDOWN [==[
void Q1280ShutdownPostQ1280() {
    if (q1350AdaptTexture[0] || q1350AdaptTexture[1]) glDeleteTextures(2, q1350AdaptTexture);
    if (q1350AdaptFbo) glDeleteFramebuffers(1, &q1350AdaptFbo);
    if (q1350AdaptVao) glDeleteVertexArrays(1, &q1350AdaptVao);
    if (q1350AdaptProgram) glDeleteProgram(q1350AdaptProgram);
    q1350AdaptTexture[0] = q1350AdaptTexture[1] = 0u;
    q1350AdaptFbo = q1350AdaptVao = q1350AdaptProgram = 0u;
    q1350AdaptFrame = 0u;
    q1350AdaptIndex = 0;

    if (q1280PostColor) glDeleteTextures(1, &q1280PostColor);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1350_OLD_SHUTDOWN}" Q1350_SHUTDOWN_BODY_POS)
if(Q1350_SHUTDOWN_BODY_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find Q12.8 shutdown body")
endif()
string(REPLACE "${Q1350_OLD_SHUTDOWN}" "${Q1350_NEW_SHUTDOWN}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Patch the final Q12.8/Q13.3 eye source so the wrapper receives eyeIndex.
# -----------------------------------------------------------------------------
set(Q1350_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1350_Q4_INPUT}")
    message(FATAL_ERROR "Q13.5 expected final Q12.8 eye source at ${Q1350_Q4_INPUT}")
endif()
file(READ "${Q1350_Q4_INPUT}" Q1350_Q4_SOURCE)
set(Q1350_OLD_COMPOSITE_CALL [==[
            Q1280CompositeEyePostQ1280(framebuffer_, eye.width, eye.height);
]==])
set(Q1350_NEW_COMPOSITE_CALL [==[
            Q1350CompositeEyePostQ1350(eyeIndex, framebuffer_, eye.width, eye.height);
]==])
string(FIND "${Q1350_Q4_SOURCE}" "${Q1350_OLD_COMPOSITE_CALL}" Q1350_CALL_POS)
if(Q1350_CALL_POS EQUAL -1)
    message(FATAL_ERROR "Q13.5 could not find final Q12.8 composite call")
endif()
string(REPLACE "${Q1350_OLD_COMPOSITE_CALL}" "${Q1350_NEW_COMPOSITE_CALL}"
       Q1350_Q4_SOURCE "${Q1350_Q4_SOURCE}")
file(WRITE "${Q1350_Q4_INPUT}" "${Q1350_Q4_SOURCE}")

# Drift guards: hard-fail rather than silently ship a non-adapting post pass.
string(FIND "${Q6H_NATIVE_SOURCE}" "Q13.5 HDR ADAPT READY" Q1350_READY_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uExposureQ1350" Q1350_EXPOSURE_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1350CompositeEyePostQ1350" Q1350_WRAPPER_OK)
string(FIND "${Q1350_Q4_SOURCE}" "Q1350CompositeEyePostQ1350(eyeIndex" Q1350_Q4_OK)
if(Q1350_READY_OK EQUAL -1 OR Q1350_EXPOSURE_OK EQUAL -1 OR
   Q1350_WRAPPER_OK EQUAL -1 OR Q1350_Q4_OK EQUAL -1)
    message(FATAL_ERROR "Q13.5 HDR adaptation verification failed: ready=${Q1350_READY_OK} exposure=${Q1350_EXPOSURE_OK} wrapper=${Q1350_WRAPPER_OK} q4=${Q1350_Q4_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q13.5 shared stereo HDR eye adaptation enabled")
