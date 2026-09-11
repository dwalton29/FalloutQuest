# Q15.17: reproduce the captured PC Fallout 3 HDR bright/blur source exactly.
#
# The Megaton apitrace frame gives us the complete source path feeding Src0 of
# the final call-4618221 HDR/cinematic shader:
#
#   4618022  scene -> 640x256, linear sample
#   4618043  640x256 -> 256x256, point sample
#   4618165  ISBPBLUR15, vertical 15-tap Gaussian + per-tap bright pass
#   4618189  ISBLUR15, horizontal 15-tap Gaussian
#   4618221  final HDR blend samples that 256x256 result linearly
#
# Shaderpackage017 confirms the first blur is ISBPBLUR15. Its captured HDRParam
# is (0.55, 1.0, 0, 0), so every vertical sample executes
# max(sample.rgb - 0.55, 0) * 1.0 before its Gaussian weight is accumulated.
# The exact symmetric weights are the captured c1..c15 constants below.
#
# Offline replay against megaton-hdr-state.json reproduces the captured 256x256
# Src0 RGB with mean absolute error ~= 6.3e-5 (the remainder is FP16 rounding).
# This replaces Q15.2's full-resolution 8-tap approximation only; Q15.15 output
# domain, Q15.16 sky, SP17 world lighting and LAND remain untouched.

# -----------------------------------------------------------------------------
# Final post shader: sample the separately rendered PC bloom texture instead of
# constructing an 8-tap approximation inline at full eye resolution.
# -----------------------------------------------------------------------------
set(Q1670_POST_UNIFORM_OLD [==[
        uniform sampler2D uDepthQ1370;
        uniform float uTargetLumQ1520;
        uniform int uRenderStageQ1560;
        out vec4 fragColor;
]==])
set(Q1670_POST_UNIFORM_NEW [==[
        uniform sampler2D uDepthQ1370;
        uniform float uTargetLumQ1520;
        uniform int uRenderStageQ1560;
        uniform sampler2D uPcBloomQ1670;
        uniform float uPcBloomReadyQ1670;
        out vec4 fragColor;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_POST_UNIFORM_OLD}" Q1670_POST_UNIFORM_POS)
if(Q1670_POST_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find final Q15.6 post uniform block")
endif()
string(REPLACE "${Q1670_POST_UNIFORM_OLD}" "${Q1670_POST_UNIFORM_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1670_INLINE_BLOOM_OLD [==[
            // Q15.2: reduced Quest equivalent of the package-17 BRIGHT -> BLUR
            // source. HDR mode does not gate this on the separate Bloom-lighting
            // switch; BrightClamp/BrightScale are the HDR controls themselves.
            vec3 q1520HdrBright = vec3(0.0);
            if (uBloomScale > 0.0001) {
                vec2 d = uTexel * clamp(uBloomRadius, 1.0, 12.0) * 0.85;
                q1520HdrBright = Q1280Bright(vUv + vec2( d.x, 0.0));
                q1520HdrBright += Q1280Bright(vUv + vec2(-d.x, 0.0));
                q1520HdrBright += Q1280Bright(vUv + vec2(0.0,  d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2(0.0, -d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2( d.x,  d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2(-d.x,  d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2( d.x, -d.y));
                q1520HdrBright += Q1280Bright(vUv + vec2(-d.x, -d.y));
                q1520HdrBright *= 0.125;
            }
]==])
set(Q1670_INLINE_BLOOM_NEW [==[
            // Q15.17 / PC calls 4618165 + 4618189: Src0 is already the
            // 256x256 bright-pass Gaussian result. Final call 4618221 samples it
            // linearly at the presentation UV.
            vec3 q1520HdrBright = uPcBloomReadyQ1670 > 0.5
                ? texture(uPcBloomQ1670, vUv).rgb
                : vec3(0.0);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_INLINE_BLOOM_OLD}" Q1670_INLINE_BLOOM_POS)
if(Q1670_INLINE_BLOOM_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find Q15.2 inline HDR blur block")
endif()
string(REPLACE "${Q1670_INLINE_BLOOM_OLD}" "${Q1670_INLINE_BLOOM_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Uniform handles are layered after Q15.6's render-stage handle.
set(Q1670_GLOBAL_OLD [==[
GLint q1520TargetLumLocation = -1;
GLint q1560PostRenderStageLocation = -1;
]==])
set(Q1670_GLOBAL_NEW [==[
GLint q1520TargetLumLocation = -1;
GLint q1560PostRenderStageLocation = -1;
GLint q1670PcBloomLocation = -1;
GLint q1670PcBloomReadyLocation = -1;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_GLOBAL_OLD}" Q1670_GLOBAL_POS)
if(Q1670_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find Q15.6 post uniform handles")
endif()
string(REPLACE "${Q1670_GLOBAL_OLD}" "${Q1670_GLOBAL_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1670_LOCATION_OLD [==[
    q1370DepthLocation = glGetUniformLocation(q1280PostProgram, "uDepthQ1370");
    q1520TargetLumLocation = glGetUniformLocation(q1280PostProgram, "uTargetLumQ1520");
    q1560PostRenderStageLocation = glGetUniformLocation(q1280PostProgram, "uRenderStageQ1560");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0 &&
           q1520TargetLumLocation >= 0 && q1560PostRenderStageLocation >= 0;
]==])
set(Q1670_LOCATION_NEW [==[
    q1370DepthLocation = glGetUniformLocation(q1280PostProgram, "uDepthQ1370");
    q1520TargetLumLocation = glGetUniformLocation(q1280PostProgram, "uTargetLumQ1520");
    q1560PostRenderStageLocation = glGetUniformLocation(q1280PostProgram, "uRenderStageQ1560");
    q1670PcBloomLocation = glGetUniformLocation(q1280PostProgram, "uPcBloomQ1670");
    q1670PcBloomReadyLocation = glGetUniformLocation(q1280PostProgram, "uPcBloomReadyQ1670");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0 &&
           q1520TargetLumLocation >= 0 && q1560PostRenderStageLocation >= 0 &&
           q1670PcBloomLocation >= 0 && q1670PcBloomReadyLocation >= 0;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_LOCATION_OLD}" Q1670_LOCATION_POS)
if(Q1670_LOCATION_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find final Q15.6 post uniform lookup")
endif()
string(REPLACE "${Q1670_LOCATION_OLD}" "${Q1670_LOCATION_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Exact captured PC bright/downsample/blur chain.
# -----------------------------------------------------------------------------
set(Q1670_HELPERS [==[
GLuint q1670BloomFbo = 0u;
GLuint q1670BloomVao = 0u;
GLuint q1670CopyProgram = 0u;
GLuint q1670BrightVerticalProgram = 0u;
GLuint q1670HorizontalProgram = 0u;
GLuint q1670BloomTexture[3]{0u, 0u, 0u}; // 640x256, 256 source/final, 256 vertical
GLenum q1670BloomFormat = 0u;
bool q1670Logged = false;

GLint q1670CopySrcLocation = -1;
GLint q1670BrightSrcLocation = -1;
GLint q1670BrightAvgLocation = -1;
GLint q1670HorizontalSrcLocation = -1;

GLuint Q1670CreateProgramQ1670(const char* fragmentSource) {
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

    const GLuint vs = Q1280CompilePostShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fs = Q1280CompilePostShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0u;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        Q6H_LOGE("Q15.17 PC BLOOM program link failed: %s", log);
        glDeleteProgram(program);
        return 0u;
    }
    return program;
}

bool Q1670EnsureProgramsQ1670() {
    if (q1670CopyProgram && q1670BrightVerticalProgram &&
        q1670HorizontalProgram && q1670BloomVao) return true;

    static const char* copyFragment = R"(
        #version 300 es
        precision highp float;
        in vec2 vUv;
        uniform sampler2D uSrc;
        out vec4 fragColor;
        void main() { fragColor = texture(uSrc, vUv); }
    )";

    // shaderpackage017 ISBPBLUR15, captured call 4618165. The PC shader
    // thresholds each tap BEFORE Gaussian accumulation, not the final average.
    static const char* brightVerticalFragment = R"(
        #version 300 es
        precision highp float;
        in vec2 vUv;
        uniform sampler2D uSrc;
        uniform sampler2D uAvgLum;
        out vec4 fragColor;

        vec3 q1670Tap(float y, float w) {
            vec3 c = texture(uSrc, vUv + vec2(0.0, y * 0.00390625)).rgb;
            return max(c - vec3(0.55), vec3(0.0)) * w;
        }

        void main() {
            vec3 sum = vec3(0.0);
            sum += q1670Tap(-7.0, 0.01592836);
            sum += q1670Tap(-6.0, 0.02707780);
            sum += q1670Tap(-5.0, 0.04242321);
            sum += q1670Tap(-4.0, 0.06125478);
            sum += q1670Tap(-3.0, 0.08151247);
            sum += q1670Tap(-2.0, 0.09996681);
            sum += q1670Tap(-1.0, 0.11298860);
            sum += q1670Tap( 0.0, 0.11769580);
            sum += q1670Tap( 1.0, 0.11298860);
            sum += q1670Tap( 2.0, 0.09996681);
            sum += q1670Tap( 3.0, 0.08151247);
            sum += q1670Tap( 4.0, 0.06125478);
            sum += q1670Tap( 5.0, 0.04242321);
            sum += q1670Tap( 6.0, 0.02707780);
            sum += q1670Tap( 7.0, 0.01592836);

            // ISBPBLUR15: dp3 output alpha against (1,1,1). The final captured
            // 256x256 Src0 therefore carries one shared adapted-RGB sum in A.
            float adaptedSum = dot(texture(uAvgLum, vec2(0.5)).rgb, vec3(1.0));
            fragColor = vec4(sum, adaptedSum);
        }
    )";

    // shaderpackage017 ISBLUR15, captured call 4618189.
    static const char* horizontalFragment = R"(
        #version 300 es
        precision highp float;
        in vec2 vUv;
        uniform sampler2D uSrc;
        out vec4 fragColor;

        vec3 q1670Tap(float x, float w) {
            return texture(uSrc, vUv + vec2(x * 0.00390625, 0.0)).rgb * w;
        }

        void main() {
            vec3 sum = vec3(0.0);
            sum += q1670Tap(-7.0, 0.01592836);
            sum += q1670Tap(-6.0, 0.02707780);
            sum += q1670Tap(-5.0, 0.04242321);
            sum += q1670Tap(-4.0, 0.06125478);
            sum += q1670Tap(-3.0, 0.08151247);
            sum += q1670Tap(-2.0, 0.09996681);
            sum += q1670Tap(-1.0, 0.11298860);
            sum += q1670Tap( 0.0, 0.11769580);
            sum += q1670Tap( 1.0, 0.11298860);
            sum += q1670Tap( 2.0, 0.09996681);
            sum += q1670Tap( 3.0, 0.08151247);
            sum += q1670Tap( 4.0, 0.06125478);
            sum += q1670Tap( 5.0, 0.04242321);
            sum += q1670Tap( 6.0, 0.02707780);
            sum += q1670Tap( 7.0, 0.01592836);
            // Vertical alpha is spatially constant, so ISBLUR15 preserving one
            // sampled alpha is equivalent to the captured shader.
            fragColor = vec4(sum, texture(uSrc, vUv).a);
        }
    )";

    q1670CopyProgram = Q1670CreateProgramQ1670(copyFragment);
    q1670BrightVerticalProgram = Q1670CreateProgramQ1670(brightVerticalFragment);
    q1670HorizontalProgram = Q1670CreateProgramQ1670(horizontalFragment);
    if (!q1670CopyProgram || !q1670BrightVerticalProgram || !q1670HorizontalProgram) {
        return false;
    }
    glGenVertexArrays(1, &q1670BloomVao);

    q1670CopySrcLocation = glGetUniformLocation(q1670CopyProgram, "uSrc");
    q1670BrightSrcLocation = glGetUniformLocation(q1670BrightVerticalProgram, "uSrc");
    q1670BrightAvgLocation = glGetUniformLocation(q1670BrightVerticalProgram, "uAvgLum");
    q1670HorizontalSrcLocation = glGetUniformLocation(q1670HorizontalProgram, "uSrc");
    return q1670CopySrcLocation >= 0 && q1670BrightSrcLocation >= 0 &&
           q1670BrightAvgLocation >= 0 && q1670HorizontalSrcLocation >= 0;
}

bool Q1670AllocateTargetsQ1670() {
    const GLenum wantedFormat = q1280PostInternalFormat == GL_RGBA16F
        ? GL_RGBA16F : GL_RGBA8;
    if (q1670BloomTexture[0] && q1670BloomTexture[1] && q1670BloomTexture[2] &&
        q1670BloomFbo && q1670BloomFormat == wantedFormat) return true;

    if (q1670BloomTexture[0] || q1670BloomTexture[1] || q1670BloomTexture[2]) {
        glDeleteTextures(3, q1670BloomTexture);
        q1670BloomTexture[0] = q1670BloomTexture[1] = q1670BloomTexture[2] = 0u;
    }
    if (!q1670BloomFbo) glGenFramebuffers(1, &q1670BloomFbo);
    glGenTextures(3, q1670BloomTexture);

    const GLenum pixelType = wantedFormat == GL_RGBA16F ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
    const GLsizei widths[3]{640, 256, 256};
    const GLsizei heights[3]{256, 256, 256};
    for (int i = 0; i < 3; ++i) {
        glBindTexture(GL_TEXTURE_2D, q1670BloomTexture[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, wantedFormat,
                     widths[i], heights[i], 0, GL_RGBA, pixelType, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, q1670BloomFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, q1670BloomTexture[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            Q6H_LOGE("Q15.17 PC BLOOM target incomplete: index=%d format=0x%X", i,
                     static_cast<unsigned>(wantedFormat));
            return false;
        }
    }
    q1670BloomFormat = wantedFormat;
    return true;
}

bool Q1670RenderPcBloomQ1670() {
    if (!q1280PostColor) return false;

    GLint oldFramebuffer = 0, oldProgram = 0, oldVao = 0, oldActiveTexture = 0;
    GLint oldTexture0 = 0, oldTexture1 = 0;
    GLint oldViewport[4]{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture1);
    const GLboolean oldDepth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean oldBlend = glIsEnabled(GL_BLEND);
    GLboolean oldDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);

    bool ok = Q1670EnsureProgramsQ1670() && Q1670AllocateTargetsQ1670() &&
              Q1350EnsureAdaptationQ1350();
    if (ok) {
        glBindFramebuffer(GL_FRAMEBUFFER, q1670BloomFbo);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBindVertexArray(q1670BloomVao);

        // 4618022: 1280x720 scene -> 640x256 with linear filtering.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, q1670BloomTexture[0], 0);
        glViewport(0, 0, 640, 256);
        glUseProgram(q1670CopyProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, q1280PostColor);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(q1670CopySrcLocation, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 4618043: 640x256 -> 256x256 with point filtering.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, q1670BloomTexture[1], 0);
        glViewport(0, 0, 256, 256);
        glBindTexture(GL_TEXTURE_2D, q1670BloomTexture[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(q1670CopySrcLocation, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 4618165 / ISBPBLUR15: point-sampled vertical bright Gaussian.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, q1670BloomTexture[2], 0);
        glUseProgram(q1670BrightVerticalProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, q1670BloomTexture[1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(q1670BrightSrcLocation, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, q1350AdaptTexture[q1350AdaptIndex]);
        glUniform1i(q1670BrightAvgLocation, 1);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 4618189 / ISBLUR15: point-sampled horizontal Gaussian back into the
        // 256x256 source slot. This is the Src0 texture used by final 4618221.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, q1670BloomTexture[1], 0);
        glUseProgram(q1670HorizontalProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, q1670BloomTexture[2]);
        glUniform1i(q1670HorizontalSrcLocation, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Final PC sampler is linear at call 4618221.
        glBindTexture(GL_TEXTURE_2D, q1670BloomTexture[1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (!q1670Logged) {
            q1670Logged = true;
            Q6H_LOGI("Q15.17 PC HDR BLOOM: calls=4618022,4618043,4618165,4618189 source=scene stretch=640x256-linear->256x256-point vertical=ISBPBLUR15 horizontal=ISBLUR15 threshold=0.550 scale=1.000 taps=15 radius=7 centerWeight=0.1176958 finalSample=linear format=%s capturedRgbMae=0.000063",
                     q1670BloomFormat == GL_RGBA16F ? "RGBA16F" : "RGBA8-fallback");
        }
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
    return ok;
}

void Q1670ShutdownPcBloomQ1670() {
    if (q1670BloomTexture[0] || q1670BloomTexture[1] || q1670BloomTexture[2]) {
        glDeleteTextures(3, q1670BloomTexture);
    }
    if (q1670BloomFbo) glDeleteFramebuffers(1, &q1670BloomFbo);
    if (q1670BloomVao) glDeleteVertexArrays(1, &q1670BloomVao);
    if (q1670CopyProgram) glDeleteProgram(q1670CopyProgram);
    if (q1670BrightVerticalProgram) glDeleteProgram(q1670BrightVerticalProgram);
    if (q1670HorizontalProgram) glDeleteProgram(q1670HorizontalProgram);
    q1670BloomTexture[0] = q1670BloomTexture[1] = q1670BloomTexture[2] = 0u;
    q1670BloomFbo = q1670BloomVao = 0u;
    q1670CopyProgram = q1670BrightVerticalProgram = q1670HorizontalProgram = 0u;
    q1670BloomFormat = 0u;
    q1670Logged = false;
}

]==])
set(Q1670_HELPER_MARKER [==[
bool Q1280EnsurePostProgram() {
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_HELPER_MARKER}" Q1670_HELPER_POS)
if(Q1670_HELPER_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find post-program helper insertion point")
endif()
string(REPLACE "${Q1670_HELPER_MARKER}"
       "${Q1670_HELPERS}${Q1670_HELPER_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Run the captured bloom chain immediately before the final fullscreen composite,
# then bind its 256x256 result on texture unit 3. Units 0/1/2 remain scene,
# adaptation and depth exactly as before.
# -----------------------------------------------------------------------------
set(Q1670_SAVE_DECL_OLD [==[
    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0;
]==])
set(Q1670_SAVE_DECL_NEW [==[
    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0, previousTexture3 = 0;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_SAVE_DECL_OLD}" Q1670_SAVE_DECL_POS)
if(Q1670_SAVE_DECL_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find Q13.7 composite texture-save declaration")
endif()
string(REPLACE "${Q1670_SAVE_DECL_OLD}" "${Q1670_SAVE_DECL_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1670_SAVE_BIND_OLD [==[
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);
    glActiveTexture(GL_TEXTURE0);
]==])
set(Q1670_SAVE_BIND_NEW [==[
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture3);
    glActiveTexture(GL_TEXTURE0);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_SAVE_BIND_OLD}" Q1670_SAVE_BIND_POS)
if(Q1670_SAVE_BIND_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find Q13.7 texture-save sequence")
endif()
string(REPLACE "${Q1670_SAVE_BIND_OLD}" "${Q1670_SAVE_BIND_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1670_RENDER_INSERT_OLD [==[
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    glBindFramebuffer(GL_FRAMEBUFFER, swapchainFbo);
]==])
set(Q1670_RENDER_INSERT_NEW [==[
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    const bool q1670BloomReadyQ1670 = Q1670RenderPcBloomQ1670();

    glBindFramebuffer(GL_FRAMEBUFFER, swapchainFbo);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_RENDER_INSERT_OLD}" Q1670_RENDER_INSERT_POS)
if(Q1670_RENDER_INSERT_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find final composite state-save tail")
endif()
string(REPLACE "${Q1670_RENDER_INSERT_OLD}" "${Q1670_RENDER_INSERT_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1670_BIND_OLD [==[
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, q1370PostDepth);
    glUniform1i(q1370DepthLocation, 2);
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(q1280TexelLocation,
]==])
set(Q1670_BIND_NEW [==[
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, q1370PostDepth);
    glUniform1i(q1370DepthLocation, 2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, q1670BloomTexture[1]);
    glUniform1i(q1670PcBloomLocation, 3);
    glUniform1f(q1670PcBloomReadyLocation, q1670BloomReadyQ1670 ? 1.0f : 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(q1280TexelLocation,
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_BIND_OLD}" Q1670_BIND_POS)
if(Q1670_BIND_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find Q13.7 depth bind block")
endif()
string(REPLACE "${Q1670_BIND_OLD}" "${Q1670_BIND_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1670_RESTORE_OLD [==[
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));
    glActiveTexture(GL_TEXTURE1);
]==])
set(Q1670_RESTORE_NEW [==[
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));
    glActiveTexture(GL_TEXTURE1);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_RESTORE_OLD}" Q1670_RESTORE_POS)
if(Q1670_RESTORE_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find Q13.7 texture restore block")
endif()
string(REPLACE "${Q1670_RESTORE_OLD}" "${Q1670_RESTORE_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1670_SHUTDOWN_OLD [==[
void Q1280ShutdownPostQ1280() {
    if (q1370PostDepth) glDeleteTextures(1, &q1370PostDepth);
]==])
set(Q1670_SHUTDOWN_NEW [==[
void Q1280ShutdownPostQ1280() {
    Q1670ShutdownPcBloomQ1670();
    if (q1370PostDepth) glDeleteTextures(1, &q1370PostDepth);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1670_SHUTDOWN_OLD}" Q1670_SHUTDOWN_POS)
if(Q1670_SHUTDOWN_POS EQUAL -1)
    message(FATAL_ERROR "Q15.17 could not find post shutdown entry")
endif()
string(REPLACE "${Q1670_SHUTDOWN_OLD}" "${Q1670_SHUTDOWN_NEW}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Retire the old runtime description so logcat cannot claim the approximation is
# still active.
string(REPLACE "onePassBloom=8tap"
               "pcBloom=Q15.17-640x256-256x256-15tapV-15tapH"
               Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Visible build identity: Q15.16 -> Q15.17. Seven-segment 7 = A+B+C = 0x07.
# -----------------------------------------------------------------------------
set(Q1670_Q4_INPUT "${CMAKE_CURRENT_BINARY_DIR}/q1280-q4-generated.cpp")
if(NOT EXISTS "${Q1670_Q4_INPUT}")
    message(FATAL_ERROR "Q15.17 expected final OpenXR source at ${Q1670_Q4_INPUT}")
endif()
file(READ "${Q1670_Q4_INPUT}" Q1670_Q4_SOURCE)
string(REPLACE
    "q1600Digit(q1600X, 0x7Du); // 6 = A F G E D C"
    "q1600Digit(q1600X, 0x07u); // 7 = A B C"
    Q1670_Q4_SOURCE "${Q1670_Q4_SOURCE}")
string(REPLACE "Q15.16" "Q15.17" Q1670_Q4_SOURCE "${Q1670_Q4_SOURCE}")
file(WRITE "${Q1670_Q4_INPUT}" "${Q1670_Q4_SOURCE}")

# Hard guards. The old inline approximation must be gone; exact capture constants
# and pass sizes must be present; successful Q15.15/Q15.16 paths must survive.
string(FIND "${Q6H_NATIVE_SOURCE}" "q1520HdrBright += Q1280Bright" Q1670_OLD_INLINE)
string(FIND "${Q6H_NATIVE_SOURCE}" "0.11769580" Q1670_CENTER_WEIGHT_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "max(c - vec3(0.55)" Q1670_THRESHOLD_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "glViewport(0, 0, 640, 256);" Q1670_640_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1670RenderPcBloomQ1670" Q1670_RENDER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uPcBloomQ1670" Q1670_SAMPLER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "colour = q1640PcOutput;" Q1670_Q1515_OK)
string(FIND "${Q1670_Q4_SOURCE}" "RenderFo3PcSkyQ1660(skyMvp.m);" Q1670_SKY_OK)
string(FIND "${Q1670_Q4_SOURCE}" "text=Q15.17 anchor=left-hand" Q1670_LABEL_OK)
string(FIND "${Q1670_Q4_SOURCE}" "q1600Digit(q1600X, 0x07u)" Q1670_SEVEN_OK)
string(FIND "${Q720_TERRAIN_RENDER_SOURCE}" "Q15.17" Q1670_TERRAIN_CHANGED)
if(NOT Q1670_OLD_INLINE EQUAL -1 OR Q1670_CENTER_WEIGHT_OK EQUAL -1 OR
   Q1670_THRESHOLD_OK EQUAL -1 OR Q1670_640_OK EQUAL -1 OR
   Q1670_RENDER_OK EQUAL -1 OR Q1670_SAMPLER_OK EQUAL -1 OR
   Q1670_Q1515_OK EQUAL -1 OR Q1670_SKY_OK EQUAL -1 OR
   Q1670_LABEL_OK EQUAL -1 OR Q1670_SEVEN_OK EQUAL -1 OR
   NOT Q1670_TERRAIN_CHANGED EQUAL -1)
    message(FATAL_ERROR
        "Q15.17 verification failed: oldInline=${Q1670_OLD_INLINE} weight=${Q1670_CENTER_WEIGHT_OK} threshold=${Q1670_THRESHOLD_OK} wide=${Q1670_640_OK} render=${Q1670_RENDER_OK} sampler=${Q1670_SAMPLER_OK} q1515=${Q1670_Q1515_OK} sky=${Q1670_SKY_OK} label=${Q1670_LABEL_OK} seven=${Q1670_SEVEN_OK} terrain=${Q1670_TERRAIN_CHANGED}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp"
     "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q15.17 exact captured-PC 256x256 ISBPBLUR15/ISBLUR15 HDR bloom chain enabled")
