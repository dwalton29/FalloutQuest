# Q10.5: Quest-safe exterior sun shadows.
# One cached 1024x1024 directional shadow map is shared by both eyes. Fallout
# statics and LAND cast/receive; the map is rebuilt only after scene changes or
# meaningful player translation. No movement/collision source is modified.

# -----------------------------------------------------------------------------
# LAND: receive the shared shadow map and expose an opaque caster draw.
# -----------------------------------------------------------------------------
string(REPLACE
    "        out vec3 vPosition;"
    "        out vec3 vPosition;\n        out vec4 vShadowCoord;\n        uniform mat4 uLightMvp;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            vPosition = aPosition;"
    "            vPosition = aPosition;\n            vShadowCoord = uLightMvp * vec4(aPosition, 1.0);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        in vec3 vPosition;"
    "        in vec3 vPosition;\n        in vec4 vShadowCoord;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "        uniform vec4 uLocalLightColorFalloff[8];"
    "        uniform vec4 uLocalLightColorFalloff[8];\n        uniform sampler2D uShadowMap;\n        uniform vec2 uShadowTexelSize;\n        uniform float uShadowsEnabled;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1050_TERRAIN_SHADOW_FN [=[
        float Q1050ShadowVisibility(vec4 shadowCoord, vec3 N, vec3 L) {
            if (uShadowsEnabled < 0.5 || shadowCoord.w <= 0.0) return 1.0;
            vec3 projected = shadowCoord.xyz / shadowCoord.w;
            projected = projected * 0.5 + 0.5;
            if (projected.x <= 0.0 || projected.x >= 1.0 ||
                projected.y <= 0.0 || projected.y >= 1.0 ||
                projected.z <= 0.0 || projected.z >= 1.0) return 1.0;
            float ndotl = max(dot(N, L), 0.0);
            float bias = max(0.00015, 0.00065 * (1.0 - ndotl));
            vec2 halfTexel = uShadowTexelSize * 0.5;
            float visible = 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2(-halfTexel.x, -halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2( halfTexel.x, -halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2(-halfTexel.x,  halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2( halfTexel.x,  halfTexel.y)).r ? 1.0 : 0.0;
            return visible * 0.25;
        }
]=])
string(REPLACE
    "        out vec4 fragColor;\n        void main() {"
    "        out vec4 fragColor;\n${Q1050_TERRAIN_SHADOW_FN}        void main() {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "            vec3 lit = albedo * (uAmbientColor + uSunlightColor * lambert);"
    "            float q1050SunVisibility = Q1050ShadowVisibility(vShadowCoord, N, lightDirection);\n            vec3 lit = albedo * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

string(REPLACE
    "GLint q1010TerrainLightColorFalloffLocation = -1;"
    "GLint q1010TerrainLightColorFalloffLocation = -1;\nGLint q1050TerrainLightMvpLocation = -1;\nGLint q1050TerrainShadowMapLocation = -1;\nGLint q1050TerrainShadowTexelLocation = -1;\nGLint q1050TerrainShadowsEnabledLocation = -1;\nGLuint q1050TerrainShadowTexture = 0;\nfloat q1050TerrainLightMvp[16]{};\nbool q1050TerrainShadowEnabled = false;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1010TerrainLightColorFalloffLocation = -1;"
    "    q1010TerrainLightColorFalloffLocation = -1;\n    q1050TerrainLightMvpLocation = -1;\n    q1050TerrainShadowMapLocation = -1;\n    q1050TerrainShadowTexelLocation = -1;\n    q1050TerrainShadowsEnabledLocation = -1;\n    q1050TerrainShadowTexture = 0;\n    q1050TerrainShadowEnabled = false;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    q1010TerrainLightColorFalloffLocation = glGetUniformLocation(q76bProgram, \"uLocalLightColorFalloff[0]\");"
    "    q1010TerrainLightColorFalloffLocation = glGetUniformLocation(q76bProgram, \"uLocalLightColorFalloff[0]\");\n    q1050TerrainLightMvpLocation = glGetUniformLocation(q76bProgram, \"uLightMvp\");\n    q1050TerrainShadowMapLocation = glGetUniformLocation(q76bProgram, \"uShadowMap\");\n    q1050TerrainShadowTexelLocation = glGetUniformLocation(q76bProgram, \"uShadowTexelSize\");\n    q1050TerrainShadowsEnabledLocation = glGetUniformLocation(q76bProgram, \"uShadowsEnabled\");"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

set(Q1050_TERRAIN_RECEIVER_UNIFORMS [=[
    if (q1050TerrainLightMvpLocation >= 0) glUniformMatrix4fv(q1050TerrainLightMvpLocation, 1, GL_FALSE, q1050TerrainLightMvp);
    if (q1050TerrainShadowMapLocation >= 0) glUniform1i(q1050TerrainShadowMapLocation, 3);
    if (q1050TerrainShadowTexelLocation >= 0) glUniform2f(q1050TerrainShadowTexelLocation, 1.0f / 1024.0f, 1.0f / 1024.0f);
    if (q1050TerrainShadowsEnabledLocation >= 0) glUniform1f(q1050TerrainShadowsEnabledLocation, q1050TerrainShadowEnabled && q1050TerrainShadowTexture ? 1.0f : 0.0f);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, q1050TerrainShadowTexture);
    glActiveTexture(GL_TEXTURE0);
]=])
string(REPLACE
    "${Q1010_TERRAIN_UNIFORMS}    for (const Q711TerrainBatch& batch : q711TerrainBatches) {"
    "${Q1010_TERRAIN_UNIFORMS}${Q1050_TERRAIN_RECEIVER_UNIFORMS}    for (const Q711TerrainBatch& batch : q711TerrainBatches) {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Preserve texture unit 3 around the ordinary LAND pass.
string(REPLACE
    "    GLint previousTexture0 = 0;"
    "    GLint previousTexture0 = 0;\n    GLint previousTexture3 = 0;"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    glActiveTexture(GL_TEXTURE0);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);"
    "    glActiveTexture(GL_TEXTURE0);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);\n    glActiveTexture(GL_TEXTURE3);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture3);\n    glActiveTexture(GL_TEXTURE0);"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
string(REPLACE
    "    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));\n    glActiveTexture(static_cast<GLenum>(previousActiveTexture));"
    "    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));\n    glActiveTexture(GL_TEXTURE3);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));\n    glActiveTexture(static_cast<GLenum>(previousActiveTexture));"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")

# Public bridge: the static renderer owns the depth texture/light matrix, while
# the terrain translation unit owns its VAOs.
set(Q1050_TERRAIN_BRIDGE [=[
void SetFo3TerrainShadowQ1050(GLuint depthTexture, const float* lightMvp, bool enabled) {
    q1050TerrainShadowTexture = depthTexture;
    q1050TerrainShadowEnabled = enabled && depthTexture != 0u && lightMvp != nullptr;
    if (lightMvp) std::copy(lightMvp, lightMvp + 16, q1050TerrainLightMvp);
}

void RenderFo3TerrainShadowQ1050(const float* lightMvp,
                                 GLuint program,
                                 GLint mvpLocation,
                                 GLint alphaTestLocation) {
    if (!q76bReady || !lightMvp || !program || mvpLocation < 0 || q711TerrainBatches.empty()) return;
    glUseProgram(program);
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, lightMvp);
    if (alphaTestLocation >= 0) glUniform1f(alphaTestLocation, 0.0f);
    for (const Q711TerrainBatch& batch : q711TerrainBatches) {
        if (batch.alphaLayer || !batch.vao || batch.vertexCount <= 0) continue;
        glBindVertexArray(batch.vao);
        glDrawArrays(GL_TRIANGLES, 0, batch.vertexCount);
    }
}

]=])
string(REPLACE
    "void RenderFo3TerrainQ76(const float* mvp) {"
    "${Q1050_TERRAIN_BRIDGE}void RenderFo3TerrainQ76(const float* mvp) {"
    Q720_TERRAIN_RENDER_SOURCE "${Q720_TERRAIN_RENDER_SOURCE}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-terrain-render-q720.cpp"
     "${Q720_TERRAIN_RENDER_SOURCE}")

# -----------------------------------------------------------------------------
# Static renderer: directional depth pass + shadow receive.
# -----------------------------------------------------------------------------
string(REPLACE
    "#include \"fo3-visual-depth-q1010.h\""
    "#include \"fo3-visual-depth-q1010.h\"\n\nvoid SetFo3TerrainShadowQ1050(GLuint depthTexture, const float* lightMvp, bool enabled);\nvoid RenderFo3TerrainShadowQ1050(const float* lightMvp, GLuint program, GLint mvpLocation, GLint alphaTestLocation);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Receiver shader inputs.
string(REPLACE
    "        out vec4 vColor;"
    "        out vec4 vColor;\n        out vec4 vShadowCoord;\n        uniform mat4 uLightMvp;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            vColor = aColor;"
    "            vColor = aColor;\n            vShadowCoord = uLightMvp * vec4(aPosition, 1.0);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        in vec4 vColor;"
    "        in vec4 vColor;\n        in vec4 vShadowCoord;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "        uniform float uGlowEnabled;"
    "        uniform float uGlowEnabled;\n        uniform sampler2D uShadowMap;\n        uniform vec2 uShadowTexelSize;\n        uniform float uShadowsEnabled;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1050_STATIC_SHADOW_FN [=[
        float Q1050ShadowVisibility(vec4 shadowCoord, vec3 N, vec3 L) {
            if (uShadowsEnabled < 0.5 || shadowCoord.w <= 0.0) return 1.0;
            vec3 projected = shadowCoord.xyz / shadowCoord.w;
            projected = projected * 0.5 + 0.5;
            if (projected.x <= 0.0 || projected.x >= 1.0 ||
                projected.y <= 0.0 || projected.y >= 1.0 ||
                projected.z <= 0.0 || projected.z >= 1.0) return 1.0;
            float ndotl = max(dot(N, L), 0.0);
            float bias = max(0.00015, 0.00065 * (1.0 - ndotl));
            vec2 halfTexel = uShadowTexelSize * 0.5;
            float visible = 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2(-halfTexel.x, -halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2( halfTexel.x, -halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2(-halfTexel.x,  halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2( halfTexel.x,  halfTexel.y)).r ? 1.0 : 0.0;
            return visible * 0.25;
        }
]=])
string(REPLACE
    "        out vec4 fragColor;\n        void main() {"
    "        out vec4 fragColor;\n${Q1050_STATIC_SHADOW_FN}        void main() {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "            vec3 lit = uNoLighting > 0.5\n                ? baseColor\n                : baseColor * (uAmbientColor + uSunlightColor * lambert) + uSunlightColor * specularContribution;"
    "            float q1050SunVisibility = Q1050ShadowVisibility(vShadowCoord, mappedNormal, lightDirection);\n            vec3 lit = uNoLighting > 0.5\n                ? baseColor\n                : baseColor * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility) + uSunlightColor * specularContribution * q1050SunVisibility;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Main-program shadow handles/state.
string(REPLACE
    "GLint gGlowEnabledLocationQ1020 = -1;"
    "GLint gGlowEnabledLocationQ1020 = -1;\nGLint gLightMvpLocationQ1050 = -1;\nGLint gShadowMapLocationQ1050 = -1;\nGLint gShadowTexelLocationQ1050 = -1;\nGLint gShadowsEnabledLocationQ1050 = -1;\nGLuint gShadowFboQ1050 = 0;\nGLuint gShadowDepthQ1050 = 0;\nGLuint gShadowProgramQ1050 = 0;\nGLint gShadowMvpLocationQ1050 = -1;\nGLint gShadowDiffuseLocationQ1050 = -1;\nGLint gShadowAlphaTestLocationQ1050 = -1;\nGLint gShadowAlphaThresholdLocationQ1050 = -1;\nfloat gLightMvpQ1050[16]{};\nfloat gShadowLastEyeQ1050[3]{1e30f, 1e30f, 1e30f};\nfloat gShadowLastSunQ1050[3]{0.0f, 0.0f, 0.0f};\nbool gShadowReadyQ1050 = false;\nbool gShadowDirtyQ1050 = true;\nconstexpr GLsizei Q1050_SHADOW_SIZE = 1024;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, \"uGlowEnabled\");"
    "    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, \"uGlowEnabled\");\n    gLightMvpLocationQ1050 = glGetUniformLocation(gProgram, \"uLightMvp\");\n    gShadowMapLocationQ1050 = glGetUniformLocation(gProgram, \"uShadowMap\");\n    gShadowTexelLocationQ1050 = glGetUniformLocation(gProgram, \"uShadowTexelSize\");\n    gShadowsEnabledLocationQ1050 = glGetUniformLocation(gProgram, \"uShadowsEnabled\");"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Q10.3 has its own program-only uniform lookup path. Extend that too.
string(REPLACE
    "    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, \"uGlowEnabled\");\n\n    // Only the transform"
    "    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, \"uGlowEnabled\");\n    gLightMvpLocationQ1050 = glGetUniformLocation(gProgram, \"uLightMvp\");\n    gShadowMapLocationQ1050 = glGetUniformLocation(gProgram, \"uShadowMap\");\n    gShadowTexelLocationQ1050 = glGetUniformLocation(gProgram, \"uShadowTexelSize\");\n    gShadowsEnabledLocationQ1050 = glGetUniformLocation(gProgram, \"uShadowsEnabled\");\n\n    // Only the transform"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Shadow-map math/resources/caster pass. Insert where the shader compiler and
# GpuObject type are already available.
set(Q1050_SHADOW_RUNTIME [=[
struct Q1050V3 { float x, y, z; };

Q1050V3 Q1050Sub(Q1050V3 a, Q1050V3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
float Q1050Dot(Q1050V3 a, Q1050V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Q1050V3 Q1050Cross(Q1050V3 a, Q1050V3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
Q1050V3 Q1050Norm(Q1050V3 v) {
    const float l = std::sqrt(Q1050Dot(v,v));
    if (l < 1e-6f) return {0.0f,1.0f,0.0f};
    return {v.x/l,v.y/l,v.z/l};
}
void Q1050Mul(const float a[16], const float b[16], float out[16]) {
    float r[16]{};
    for (int c=0;c<4;++c) for (int row=0;row<4;++row) {
        for (int k=0;k<4;++k) r[c*4+row] += a[k*4+row] * b[c*4+k];
    }
    std::copy(r,r+16,out);
}
void Q1050LookAt(Q1050V3 eye, Q1050V3 center, Q1050V3 up, float out[16]) {
    const Q1050V3 f = Q1050Norm(Q1050Sub(center,eye));
    Q1050V3 s = Q1050Norm(Q1050Cross(f,up));
    if (std::fabs(Q1050Dot(s,s)) < 1e-5f) s = {1.0f,0.0f,0.0f};
    const Q1050V3 u = Q1050Cross(s,f);
    const float m[16]{
        s.x,u.x,-f.x,0.0f,
        s.y,u.y,-f.y,0.0f,
        s.z,u.z,-f.z,0.0f,
        -Q1050Dot(s,eye),-Q1050Dot(u,eye),Q1050Dot(f,eye),1.0f
    };
    std::copy(m,m+16,out);
}
void Q1050Ortho(float halfSpan, float nearZ, float farZ, float out[16]) {
    std::fill(out,out+16,0.0f);
    out[0] = 1.0f/halfSpan;
    out[5] = 1.0f/halfSpan;
    out[10] = -2.0f/(farZ-nearZ);
    out[14] = -(farZ+nearZ)/(farZ-nearZ);
    out[15] = 1.0f;
}
void Q1050BuildLightMvp(float out[16]) {
    const Fo3EnvironmentQ1000& env = GetFo3EnvironmentQ1000();
    Q1050V3 sun = Q1050Norm({env.sunDirection[0],env.sunDirection[1],env.sunDirection[2]});
    Q1050V3 center{gFo3EyePositionQ1010[0], gFo3EyePositionQ1010[1] + 4.0f, gFo3EyePositionQ1010[2]};
    Q1050V3 eye{center.x + sun.x*80.0f, center.y + sun.y*80.0f, center.z + sun.z*80.0f};
    Q1050V3 up = std::fabs(sun.y) > 0.92f ? Q1050V3{0.0f,0.0f,1.0f} : Q1050V3{0.0f,1.0f,0.0f};
    float view[16], projection[16];
    Q1050LookAt(eye,center,up,view);
    Q1050Ortho(58.0f,1.0f,180.0f,projection);
    Q1050Mul(projection,view,out);
}

bool Q1050CreateShadowResources() {
    if (gShadowFboQ1050 && gShadowDepthQ1050 && gShadowProgramQ1050) return true;

    static const char* vsSource = R"(
        #version 300 es
        layout(location=0) in vec3 aPosition;
        layout(location=4) in vec2 aUv;
        uniform mat4 uMvp;
        out vec2 vUv;
        void main(){ vUv=aUv; gl_Position=uMvp*vec4(aPosition,1.0); }
    )";
    static const char* fsSource = R"(
        #version 300 es
        precision mediump float;
        in vec2 vUv;
        uniform sampler2D uDiffuse;
        uniform float uAlphaTest;
        uniform float uAlphaThreshold;
        void main(){
            if (uAlphaTest > 0.5 && texture(uDiffuse,vUv).a < uAlphaThreshold) discard;
        }
    )";
    GLuint vs = CompileQ6HShader(GL_VERTEX_SHADER,vsSource);
    GLuint fs = CompileQ6HShader(GL_FRAGMENT_SHADER,fsSource);
    if (!vs || !fs) return false;
    gShadowProgramQ1050 = glCreateProgram();
    glAttachShader(gShadowProgramQ1050,vs);
    glAttachShader(gShadowProgramQ1050,fs);
    glLinkProgram(gShadowProgramQ1050);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint linked=GL_FALSE; glGetProgramiv(gShadowProgramQ1050,GL_LINK_STATUS,&linked);
    if (linked != GL_TRUE) {
        char log[1024]{}; glGetProgramInfoLog(gShadowProgramQ1050,sizeof(log),nullptr,log);
        Q6H_LOGE("Q10.5 SHADOW FAILED: stage=program log=%s",log);
        glDeleteProgram(gShadowProgramQ1050); gShadowProgramQ1050=0; return false;
    }
    gShadowMvpLocationQ1050=glGetUniformLocation(gShadowProgramQ1050,"uMvp");
    gShadowDiffuseLocationQ1050=glGetUniformLocation(gShadowProgramQ1050,"uDiffuse");
    gShadowAlphaTestLocationQ1050=glGetUniformLocation(gShadowProgramQ1050,"uAlphaTest");
    gShadowAlphaThresholdLocationQ1050=glGetUniformLocation(gShadowProgramQ1050,"uAlphaThreshold");

    GLint previousFbo=0, previousTex=0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previousFbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousTex);
    glGenTextures(1,&gShadowDepthQ1050);
    glBindTexture(GL_TEXTURE_2D,gShadowDepthQ1050);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,Q1050_SHADOW_SIZE,Q1050_SHADOW_SIZE,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    glGenFramebuffers(1,&gShadowFboQ1050);
    glBindFramebuffer(GL_FRAMEBUFFER,gShadowFboQ1050);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,gShadowDepthQ1050,0);
    const GLenum none=GL_NONE;
    glDrawBuffers(1,&none);
    glReadBuffer(GL_NONE);
    const GLenum status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER,static_cast<GLuint>(previousFbo));
    glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previousTex));
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Q6H_LOGE("Q10.5 SHADOW FAILED: stage=fbo status=0x%X",status);
        return false;
    }
    Q6H_LOGI("Q10.5 SHADOW RESOURCES READY: size=%dx%d format=DEPTH24 footprint=116m stereoShared=1",Q1050_SHADOW_SIZE,Q1050_SHADOW_SIZE);
    return true;
}

bool Q1050UpdateSunShadow() {
    if (!gSceneReady || gObjects.empty()) return false;
    const Fo3EnvironmentQ1000& env=GetFo3EnvironmentQ1000();
    if (!env.valid || !Q1050CreateShadowResources()) return false;
    const float dx=gFo3EyePositionQ1010[0]-gShadowLastEyeQ1050[0];
    const float dy=gFo3EyePositionQ1010[1]-gShadowLastEyeQ1050[1];
    const float dz=gFo3EyePositionQ1010[2]-gShadowLastEyeQ1050[2];
    const float sdx=env.sunDirection[0]-gShadowLastSunQ1050[0];
    const float sdy=env.sunDirection[1]-gShadowLastSunQ1050[1];
    const float sdz=env.sunDirection[2]-gShadowLastSunQ1050[2];
    if (gShadowReadyQ1050 && !gShadowDirtyQ1050 &&
        dx*dx+dy*dy+dz*dz < 0.0625f && sdx*sdx+sdy*sdy+sdz*sdz < 1e-6f) return true;

    Q1050BuildLightMvp(gLightMvpQ1050);
    GLint previousFbo=0, previousProgram=0, previousVao=0, previousViewport[4]{}, previousActiveTex=0, previousTex0=0;
    GLint previousDepthFunc=GL_LESS, previousCullFace=GL_BACK;
    GLboolean previousDepthMask=GL_TRUE;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previousFbo);
    glGetIntegerv(GL_CURRENT_PROGRAM,&previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previousVao);
    glGetIntegerv(GL_VIEWPORT,previousViewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&previousActiveTex);
    glGetIntegerv(GL_DEPTH_FUNC,&previousDepthFunc);
    glGetIntegerv(GL_CULL_FACE_MODE,&previousCullFace);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&previousDepthMask);
    const GLboolean depthWas=glIsEnabled(GL_DEPTH_TEST), blendWas=glIsEnabled(GL_BLEND), cullWas=glIsEnabled(GL_CULL_FACE), polyWas=glIsEnabled(GL_POLYGON_OFFSET_FILL);
    glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousTex0);

    glBindFramebuffer(GL_FRAMEBUFFER,gShadowFboQ1050);
    glViewport(0,0,Q1050_SHADOW_SIZE,Q1050_SHADOW_SIZE);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1.5f,3.0f);
    glClearDepthf(1.0f); glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(gShadowProgramQ1050);
    glUniformMatrix4fv(gShadowMvpLocationQ1050,1,GL_FALSE,gLightMvpQ1050);
    glUniform1i(gShadowDiffuseLocationQ1050,0);

    size_t staticCasters=0;
    for (const GpuObject& object:gObjects) {
        if (object.alphaBlend || !object.vao || object.vertexCount<=0) continue;
        glUniform1f(gShadowAlphaTestLocationQ1050,object.alphaTest?1.0f:0.0f);
        glUniform1f(gShadowAlphaThresholdLocationQ1050,object.alphaThreshold);
        glBindTexture(GL_TEXTURE_2D,object.diffuse);
        glBindVertexArray(object.vao);
        glDrawArrays(GL_TRIANGLES,0,object.vertexCount);
        ++staticCasters;
    }
    RenderFo3TerrainShadowQ1050(gLightMvpQ1050,gShadowProgramQ1050,gShadowMvpLocationQ1050,gShadowAlphaTestLocationQ1050);

    glBindFramebuffer(GL_FRAMEBUFFER,static_cast<GLuint>(previousFbo));
    glViewport(previousViewport[0],previousViewport[1],previousViewport[2],previousViewport[3]);
    glUseProgram(static_cast<GLuint>(previousProgram));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previousTex0));
    glActiveTexture(static_cast<GLenum>(previousActiveTex));
    glDepthFunc(static_cast<GLenum>(previousDepthFunc)); glDepthMask(previousDepthMask);
    glCullFace(static_cast<GLenum>(previousCullFace));
    if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullWas) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (polyWas) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL);

    std::copy(gFo3EyePositionQ1010,gFo3EyePositionQ1010+3,gShadowLastEyeQ1050);
    std::copy(env.sunDirection,env.sunDirection+3,gShadowLastSunQ1050);
    gShadowReadyQ1050=true; gShadowDirtyQ1050=false;
    SetFo3TerrainShadowQ1050(gShadowDepthQ1050,gLightMvpQ1050,true);
    static size_t updates=0; ++updates;
    if (updates<=8 || updates%40==0) Q6H_LOGI("Q10.5 SHADOW MAP: update=%zu staticCasters=%zu eye=(%.2f %.2f %.2f) footprint=116m",updates,staticCasters,gFo3EyePositionQ1010[0],gFo3EyePositionQ1010[1],gFo3EyePositionQ1010[2]);
    return true;
}

]=])
string(REPLACE
    "bool InitializeScene() {"
    "${Q1050_SHADOW_RUNTIME}bool InitializeScene() {"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Mark the cache dirty whenever a scene swap replaces geometry.
string(REPLACE
    "    gSceneReady = !gObjects.empty();\n    gLoggedFirstDraw = false;"
    "    gSceneReady = !gObjects.empty();\n    gLoggedFirstDraw = false;\n    gShadowDirtyQ1050 = true;\n    gShadowReadyQ1050 = false;\n    SetFo3TerrainShadowQ1050(gShadowDepthQ1050,gLightMvpQ1050,false);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Static receive uniforms + texture unit 3.
set(Q1050_STATIC_RECEIVER_UNIFORMS [=[
    const bool q1050ShadowActive = Q1050UpdateSunShadow();
    if (gLightMvpLocationQ1050 >= 0) glUniformMatrix4fv(gLightMvpLocationQ1050,1,GL_FALSE,gLightMvpQ1050);
    if (gShadowMapLocationQ1050 >= 0) glUniform1i(gShadowMapLocationQ1050,3);
    if (gShadowTexelLocationQ1050 >= 0) glUniform2f(gShadowTexelLocationQ1050,1.0f/Q1050_SHADOW_SIZE,1.0f/Q1050_SHADOW_SIZE);
    if (gShadowsEnabledLocationQ1050 >= 0) glUniform1f(gShadowsEnabledLocationQ1050,q1050ShadowActive?1.0f:0.0f);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,q1050ShadowActive?gShadowDepthQ1050:0u);
    glActiveTexture(GL_TEXTURE0);
]=])
string(REPLACE
    "${Q1010_STATIC_UNIFORMS}    // Opaque and alpha-tested cutouts first. Alpha testing still writes depth,"
    "${Q1010_STATIC_UNIFORMS}${Q1050_STATIC_RECEIVER_UNIFORMS}    // Opaque and alpha-tested cutouts first. Alpha testing still writes depth,"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Preserve texture unit 3 in the static renderer.
string(REPLACE
    "    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0;"
    "    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0, previousTexture3 = 0;"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    glActiveTexture(GL_TEXTURE2);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);"
    "    glActiveTexture(GL_TEXTURE2);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);\n    glActiveTexture(GL_TEXTURE3);\n    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture3);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")
string(REPLACE
    "    glActiveTexture(GL_TEXTURE2);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));"
    "    glActiveTexture(GL_TEXTURE3);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));\n    glActiveTexture(GL_TEXTURE2);\n    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

# Release shadow resources with the renderer/context.
string(REPLACE
    "    if (gProgram) glDeleteProgram(gProgram);"
    "    if (gShadowProgramQ1050) glDeleteProgram(gShadowProgramQ1050);\n    if (gShadowDepthQ1050) glDeleteTextures(1,&gShadowDepthQ1050);\n    if (gShadowFboQ1050) glDeleteFramebuffers(1,&gShadowFboQ1050);\n    gShadowProgramQ1050=0; gShadowDepthQ1050=0; gShadowFboQ1050=0;\n    gShadowReadyQ1050=false; gShadowDirtyQ1050=true;\n    SetFo3TerrainShadowQ1050(0,nullptr,false);\n    if (gProgram) glDeleteProgram(gProgram);"
    Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
