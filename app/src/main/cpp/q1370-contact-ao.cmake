# Q13.7: depth-backed contact ambient occlusion for scene grounding.
#
# Q12.8 renders the complete eye into an HDR colour texture, but Q6H's depth
# path is a renderbuffer. That is sufficient for depth testing, but impossible
# to sample during the final-frame pass. Q13.7 keeps the exact same depth-test
# semantics while replacing the HDR target's depth attachment with a sampleable
# DEPTH_COMPONENT24 texture. A conservative deterministic 8-tap depth-only AO
# then darkens local contact/crease regions before Q13.6 exposure/cinematic work.
#
# This is renderer-side grounding, not an invented ESM colour/light override.
# Sky (depth=1) is excluded and high-luminance/emissive pixels are protected.

# -----------------------------------------------------------------------------
# Shared depth texture + post shader uniform.
# -----------------------------------------------------------------------------
set(Q1370_OLD_GLOBAL [==[
GLuint q1280PostColor = 0u;
GLuint q1280PostProgram = 0u;
]==])
set(Q1370_NEW_GLOBAL [==[
GLuint q1280PostColor = 0u;
GLuint q1370PostDepth = 0u;
GLuint q1280PostProgram = 0u;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_GLOBAL}" Q1370_GLOBAL_POS)
if(Q1370_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q12.8 post colour global")
endif()
string(REPLACE "${Q1370_OLD_GLOBAL}" "${Q1370_NEW_GLOBAL}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1370_OLD_LOCATION_GLOBAL [==[
GLint q1350ExposureLocation = -1;
]==])
set(Q1370_NEW_LOCATION_GLOBAL [==[
GLint q1350ExposureLocation = -1;
GLint q1370DepthLocation = -1;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_LOCATION_GLOBAL}" Q1370_LOC_GLOBAL_POS)
if(Q1370_LOC_GLOBAL_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 exposure uniform global")
endif()
string(REPLACE "${Q1370_OLD_LOCATION_GLOBAL}" "${Q1370_NEW_LOCATION_GLOBAL}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Add sampleable depth to the final post shader.
set(Q1370_OLD_SHADER_UNIFORM [==[
        uniform sampler2D uExposureQ1350;
        out vec4 fragColor;
]==])
set(Q1370_NEW_SHADER_UNIFORM [==[
        uniform sampler2D uExposureQ1350;
        uniform sampler2D uDepthQ1370;
        out vec4 fragColor;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_SHADER_UNIFORM}" Q1370_SHADER_UNIFORM_POS)
if(Q1370_SHADER_UNIFORM_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 post sampler block")
endif()
string(REPLACE "${Q1370_OLD_SHADER_UNIFORM}" "${Q1370_NEW_SHADER_UNIFORM}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Depth-only contact AO helpers. Projection values match the current authored
# OpenXR eye path: ProjectionFromFov(..., 0.04f, 100.0f).
set(Q1370_AO_HELPERS [==[

        float Q1370LinearDepth(float depth01) {
            const float nearZ = 0.04;
            const float farZ = 100.0;
            float z = depth01 * 2.0 - 1.0;
            return (2.0 * nearZ * farZ) /
                   max(farZ + nearZ - z * (farZ - nearZ), 0.0001);
        }

        float Q1370ContactAo(vec2 uv, float centerRaw) {
            if (centerRaw >= 0.99995) return 1.0;
            float center = Q1370LinearDepth(centerRaw);

            // Screen-space contact radius narrows with distance. This is not a
            // large SSAO halo pass: it is deliberately aimed at object/ground,
            // wall/floor and clutter/structure contact regions.
            float distanceFade = clamp(center / 24.0, 0.0, 1.0);
            float radiusPx = mix(7.0, 2.5, distanceFade);
            float bias = max(0.012, center * 0.0015);
            float range = max(0.16, center * 0.028);

            const vec2 dirs[8] = vec2[8](
                vec2( 1.000,  0.000), vec2(-1.000,  0.000),
                vec2( 0.000,  1.000), vec2( 0.000, -1.000),
                vec2( 0.707,  0.707), vec2(-0.707,  0.707),
                vec2( 0.707, -0.707), vec2(-0.707, -0.707));

            float occ = 0.0;
            for (int i = 0; i < 8; ++i) {
                // Alternate inner/outer ring without noise so VR is temporally
                // stable and both eyes use the same deterministic kernel.
                float ring = (i < 4) ? 0.55 : 1.0;
                vec2 sampleUv = clamp(uv + dirs[i] * uTexel * radiusPx * ring,
                                      vec2(0.0), vec2(1.0));
                float neighbourRaw = texture(uDepthQ1370, sampleUv).r;
                if (neighbourRaw >= 0.99995) continue;
                float neighbour = Q1370LinearDepth(neighbourRaw);
                float delta = center - neighbour;
                float nearOccluder = smoothstep(bias, range, delta);
                float haloReject = 1.0 - smoothstep(range, range * 3.0, delta);
                occ += nearOccluder * haloReject;
            }

            float normalized = occ * 0.125;
            return 1.0 - normalized * 0.22;
        }
]==])
set(Q1370_HELPER_MARKER [==[
        void main() {
            vec3 colour = texture(uScene, vUv).rgb;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_HELPER_MARKER}" Q1370_HELPER_POS)
if(Q1370_HELPER_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q12.8 post main entry")
endif()
string(REPLACE "${Q1370_HELPER_MARKER}"
       "${Q1370_AO_HELPERS}\n${Q1370_HELPER_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Apply AO after bloom but before exposure/display-domain cinematic controls.
# Protect very bright/emissive output from generic screen-space darkening.
set(Q1370_AO_APPLY [==[
            float contactAoQ1370 = Q1370ContactAo(vUv, texture(uDepthQ1370, vUv).r);
            float aoLumQ1370 = Q1280Lum(max(colour, vec3(0.0)));
            float aoMaterialMaskQ1370 = 1.0 - smoothstep(0.70, 1.35, aoLumQ1370);
            colour *= mix(1.0, contactAoQ1370, aoMaterialMaskQ1370);

]==])
set(Q1370_APPLY_MARKER [==[
            // Q13.5 exposure is packed across RG in a 1x1 RGBA8 history texture.
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_APPLY_MARKER}" Q1370_APPLY_POS)
if(Q1370_APPLY_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 exposure entry")
endif()
string(REPLACE "${Q1370_APPLY_MARKER}"
       "${Q1370_AO_APPLY}${Q1370_APPLY_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Fetch depth sampler location when the post program links.
set(Q1370_OLD_LOCATION [==[
    q1350ExposureLocation = glGetUniformLocation(q1280PostProgram, "uExposureQ1350");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 && q1350ExposureLocation >= 0;
]==])
set(Q1370_NEW_LOCATION [==[
    q1350ExposureLocation = glGetUniformLocation(q1280PostProgram, "uExposureQ1350");
    q1370DepthLocation = glGetUniformLocation(q1280PostProgram, "uDepthQ1370");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_LOCATION}" Q1370_LOCATION_POS)
if(Q1370_LOCATION_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 post uniform lookup tail")
endif()
string(REPLACE "${Q1370_OLD_LOCATION}" "${Q1370_NEW_LOCATION}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Allocate DEPTH_COMPONENT24 alongside the HDR colour target.
# -----------------------------------------------------------------------------
set(Q1370_OLD_ALLOC_GEN [==[
    if (!q1280PostFbo) glGenFramebuffers(1, &q1280PostFbo);
    if (!q1280PostColor) glGenTextures(1, &q1280PostColor);

    if (q1280PostWidth == width && q1280PostHeight == height && q1280PostColor) return true;
]==])
set(Q1370_NEW_ALLOC_GEN [==[
    if (!q1280PostFbo) glGenFramebuffers(1, &q1280PostFbo);
    if (!q1280PostColor) glGenTextures(1, &q1280PostColor);
    if (!q1370PostDepth) glGenTextures(1, &q1370PostDepth);

    if (q1280PostWidth == width && q1280PostHeight == height &&
        q1280PostColor && q1370PostDepth) return true;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_ALLOC_GEN}" Q1370_ALLOC_GEN_POS)
if(Q1370_ALLOC_GEN_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q12.8 target allocation header")
endif()
string(REPLACE "${Q1370_OLD_ALLOC_GEN}" "${Q1370_NEW_ALLOC_GEN}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1370_DEPTH_ALLOC [==[

    glBindTexture(GL_TEXTURE_2D, q1370PostDepth);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, q1280PostFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, q1370PostDepth, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
]==])
set(Q1370_FINAL_STATUS_MARKER [==[
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Q6H_LOGE("Q12.8 POST target incomplete: status=0x%X size=%dx%d",
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_FINAL_STATUS_MARKER}" Q1370_STATUS_POS)
if(Q1370_STATUS_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q12.8 final FBO status check")
endif()
string(REPLACE "${Q1370_FINAL_STATUS_MARKER}"
       "${Q1370_DEPTH_ALLOC}\n${Q1370_FINAL_STATUS_MARKER}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q6HViewport normally attaches its legacy depth renderbuffer after glViewport.
# For the HDR FBO only, keep Q13.7's sampleable depth texture attached instead.
set(Q1370_OLD_VIEWPORT [==[
void Q6HViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    glViewport(x, y, width, height);
    if (width <= 0 || height <= 0) return;
    if (!gDepthRenderbuffer) glGenRenderbuffers(1, &gDepthRenderbuffer);
]==])
set(Q1370_NEW_VIEWPORT [==[
void Q6HViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    glViewport(x, y, width, height);
    if (width <= 0 || height <= 0) return;
    GLint q1370CurrentFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &q1370CurrentFbo);
    if (q1280PostFbo != 0u && q1370PostDepth != 0u &&
        q1370CurrentFbo == static_cast<GLint>(q1280PostFbo)) {
        // Q13.7 depth texture was already attached during target allocation.
        return;
    }
    if (!gDepthRenderbuffer) glGenRenderbuffers(1, &gDepthRenderbuffer);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_VIEWPORT}" Q1370_VIEWPORT_POS)
if(Q1370_VIEWPORT_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q6H depth viewport wrapper")
endif()
string(REPLACE "${Q1370_OLD_VIEWPORT}" "${Q1370_NEW_VIEWPORT}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# -----------------------------------------------------------------------------
# Bind depth on texture unit 2 during final composite, preserving caller state.
# -----------------------------------------------------------------------------
set(Q1370_OLD_SAVE [==[
    GLint previousTexture0 = 0, previousTexture1 = 0;
]==])
set(Q1370_NEW_SAVE [==[
    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_SAVE}" Q1370_SAVE_POS)
if(Q1370_SAVE_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 composite texture save variables")
endif()
string(REPLACE "${Q1370_OLD_SAVE}" "${Q1370_NEW_SAVE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1370_OLD_SAVE_BINDINGS [==[
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);
    glActiveTexture(GL_TEXTURE0);
]==])
set(Q1370_NEW_SAVE_BINDINGS [==[
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);
    glActiveTexture(GL_TEXTURE0);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_SAVE_BINDINGS}" Q1370_SAVE_BINDINGS_POS)
if(Q1370_SAVE_BINDINGS_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 texture binding save")
endif()
string(REPLACE "${Q1370_OLD_SAVE_BINDINGS}" "${Q1370_NEW_SAVE_BINDINGS}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1370_OLD_EXPOSURE_BIND [==[
        glUniform1i(q1350ExposureLocation, 1);
        glActiveTexture(GL_TEXTURE0);
    }
    glUniform2f(q1280TexelLocation,
]==])
set(Q1370_NEW_EXPOSURE_BIND [==[
        glUniform1i(q1350ExposureLocation, 1);
        glActiveTexture(GL_TEXTURE0);
    }
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, q1370PostDepth);
    glUniform1i(q1370DepthLocation, 2);
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(q1280TexelLocation,
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_EXPOSURE_BIND}" Q1370_BIND_POS)
if(Q1370_BIND_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 exposure bind tail")
endif()
string(REPLACE "${Q1370_OLD_EXPOSURE_BIND}" "${Q1370_NEW_EXPOSURE_BIND}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1370_OLD_RESTORE [==[
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
]==])
set(Q1370_NEW_RESTORE [==[
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_RESTORE}" Q1370_RESTORE_POS)
if(Q1370_RESTORE_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 texture restore")
endif()
string(REPLACE "${Q1370_OLD_RESTORE}" "${Q1370_NEW_RESTORE}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Cleanup sampleable depth with the existing post resources.
set(Q1370_OLD_SHUTDOWN [==[
void Q1280ShutdownPostQ1280() {
    if (q1350AdaptTexture[0] || q1350AdaptTexture[1]) glDeleteTextures(2, q1350AdaptTexture);
]==])
set(Q1370_NEW_SHUTDOWN [==[
void Q1280ShutdownPostQ1280() {
    if (q1370PostDepth) glDeleteTextures(1, &q1370PostDepth);
    q1370PostDepth = 0u;
    if (q1350AdaptTexture[0] || q1350AdaptTexture[1]) glDeleteTextures(2, q1350AdaptTexture);
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_SHUTDOWN}" Q1370_SHUTDOWN_POS)
if(Q1370_SHUTDOWN_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q13.5 post shutdown")
endif()
string(REPLACE "${Q1370_OLD_SHUTDOWN}" "${Q1370_NEW_SHUTDOWN}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Runtime audit. Keep separate from authored ImageSpace diagnostics so it is
# obvious that this is a renderer grounding layer rather than ESM data.
set(Q1370_OLD_GPU_LOG [==[
        q1280PostLoggedGpu = true;
        Q6H_LOGI("Q12.8 POST GPU READY: size=%dx%d format=%s onePassBloom=8tap stereoSequential=1",
]==])
set(Q1370_NEW_GPU_LOG [==[
        q1280PostLoggedGpu = true;
        Q6H_LOGI("Q13.7 CONTACT AO READY: size=%dx%d depth=DEPTH_COMPONENT24 sampleable=1 taps=8 maxDarken=0.220 radiusPx=2.5..7.0 skyExcluded=1 emissiveProtected=1 stereoSequential=1",
                 width, height);
        Q6H_LOGI("Q12.8 POST GPU READY: size=%dx%d format=%s onePassBloom=8tap stereoSequential=1",
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1370_OLD_GPU_LOG}" Q1370_GPU_LOG_POS)
if(Q1370_GPU_LOG_POS EQUAL -1)
    message(FATAL_ERROR "Q13.7 could not find Q12.8 GPU-ready log")
endif()
string(REPLACE "${Q1370_OLD_GPU_LOG}" "${Q1370_NEW_GPU_LOG}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Drift guards: depth texture, sampler and AO function must all survive into the
# final generated native source.
string(FIND "${Q6H_NATIVE_SOURCE}" "GL_DEPTH_COMPONENT24" Q1370_DEPTH_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "uDepthQ1370" Q1370_SAMPLER_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q1370ContactAo" Q1370_AO_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q13.7 CONTACT AO READY" Q1370_LOG_OK)
if(Q1370_DEPTH_OK EQUAL -1 OR Q1370_SAMPLER_OK EQUAL -1 OR
   Q1370_AO_OK EQUAL -1 OR Q1370_LOG_OK EQUAL -1)
    message(FATAL_ERROR "Q13.7 contact AO verification failed: depth=${Q1370_DEPTH_OK} sampler=${Q1370_SAMPLER_OK} ao=${Q1370_AO_OK} log=${Q1370_LOG_OK}")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q13.7 sampleable depth and conservative contact AO enabled")
