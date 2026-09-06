#include "fo3-chair-material-mesh.h"
#include "fo3-texture-bsa.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr const char* Q5I_TAG = "FalloutQuest";
constexpr float FO3_UNITS_PER_METRE = 70.0f;
constexpr float FLOOR_Y = -1.55f;
constexpr float CHAIR_DISTANCE_METRES = -2.20f;

#define Q5I_LOGI(...) __android_log_print(ANDROID_LOG_INFO, Q5I_TAG, __VA_ARGS__)
#define Q5I_LOGW(...) __android_log_print(ANDROID_LOG_WARN, Q5I_TAG, __VA_ARGS__)
#define Q5I_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, Q5I_TAG, __VA_ARGS__)

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Normalize(const Vec3& v) {
    const float length = std::sqrt(std::max(0.0f, Dot(v, v)));
    if (length < 1e-6f) return {0.0f, 1.0f, 0.0f};
    return {v.x / length, v.y / length, v.z / length};
}

Vec3 GamePositionToOpenXr(float x, float y, float z) {
    // Gamebryo/Fallout is Z-up; OpenXR is Y-up. Preserve handedness.
    return {x / FO3_UNITS_PER_METRE,
            z / FO3_UNITS_PER_METRE,
            -y / FO3_UNITS_PER_METRE};
}

Vec3 GameDirectionToOpenXr(float x, float y, float z) {
    return Normalize({x, z, -y});
}

GLuint CompileQ5IShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Q5I_LOGE("Q5I shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateQ5IProgram() {
    static const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec3 aTangent;
        layout(location = 3) in vec3 aBitangent;
        layout(location = 4) in vec2 aUv;

        uniform mat4 uMvp;

        out vec3 vNormal;
        out vec3 vTangent;
        out vec3 vBitangent;
        out vec2 vUv;

        void main() {
            vNormal = aNormal;
            vTangent = aTangent;
            vBitangent = aBitangent;
            vUv = aUv;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";

    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;

        in vec3 vNormal;
        in vec3 vTangent;
        in vec3 vBitangent;
        in vec2 vUv;

        uniform sampler2D uDiffuse;
        uniform sampler2D uNormalGloss;
        uniform float uGlossiness;
        uniform float uNormalStrength;

        out vec4 fragColor;

        void main() {
            vec4 diffuseTexel = texture(uDiffuse, vUv);
            vec4 normalGloss = texture(uNormalGloss, vUv);

            // Fallout 3 stores tangent-space XYZ in RGB and gloss/specular mask
            // in the alpha channel of the _N texture.
            vec3 tangentNormal = normalGloss.rgb * 2.0 - 1.0;
            tangentNormal.xy *= uNormalStrength;
            tangentNormal = normalize(tangentNormal);

            // We invert V when sampling DDS in GLES. Flip tangent-space Y to
            // keep the original Gamebryo tangent basis consistent with that UV flip.
            tangentNormal.y = -tangentNormal.y;

            vec3 N = normalize(vNormal);
            vec3 T = normalize(vTangent - N * dot(N, vTangent));
            vec3 B = normalize(vBitangent - N * dot(N, vBitangent));
            mat3 tbn = mat3(T, B, N);
            vec3 mappedNormal = normalize(tbn * tangentNormal);

            vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.40));
            float diffuseLight = max(dot(mappedNormal, lightDirection), 0.0);
            float ambient = 0.32;

            // Debug room has no full Fallout lighting system yet. Use a stable
            // approximate view vector so the original gloss alpha is visible.
            vec3 viewDirection = normalize(vec3(0.0, 0.15, 1.0));
            vec3 halfVector = normalize(lightDirection + viewDirection);
            float exponent = clamp(uGlossiness, 2.0, 96.0);
            float specular = pow(max(dot(mappedNormal, halfVector), 0.0), exponent)
                           * normalGloss.a * 0.35;

            vec3 lit = diffuseTexel.rgb * (ambient + 0.68 * diffuseLight)
                     + vec3(specular);
            fragColor = vec4(lit, diffuseTexel.a);
        }
    )";

    GLuint vs = CompileQ5IShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileQ5IShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

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
        Q5I_LOGE("Q5I shader link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

GLuint gChairProgram = 0;
GLuint gChairVao = 0;
GLuint gChairVbo = 0;
GLuint gDiffuseTexture = 0;
GLuint gNormalTexture = 0;
GLsizei gChairVertexCount = 0;
GLint gMvpLocation = -1;
GLint gDiffuseLocation = -1;
GLint gNormalLocation = -1;
GLint gGlossinessLocation = -1;
GLint gNormalStrengthLocation = -1;
GLuint gDepthRenderbuffer = 0;
GLsizei gDepthWidth = 0;
GLsizei gDepthHeight = 0;
bool gChairReady = false;
bool gRealDiffuseLoaded = false;
bool gRealNormalLoaded = false;
bool gLoggedFirstDraw = false;
float gGlossiness = 10.0f;

bool UploadTexture(const std::string& path,
                   const std::vector<uint8_t>& fallbackPixel,
                   GLuint& outTexture,
                   bool& outReal,
                   const char* label) {
    Fo3RgbaTexture texture;
    outReal = LoadFalloutTextureRgba(path, texture);
    if (!outReal) {
        Q5I_LOGW("Q5I %s FALLBACK: failed to load %s", label, path.c_str());
        texture.width = 1;
        texture.height = 1;
        texture.rgba = fallbackPixel;
        texture.sourcePath = "<fallback>";
        texture.format = "fallback";
    }

    glGenTextures(1, &outTexture);
    glBindTexture(GL_TEXTURE_2D, outTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 texture.width, texture.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (glGetError() != GL_NO_ERROR) {
        Q5I_LOGE("Q5I FAILED: GLES %s texture upload failed", label);
        return false;
    }

    Q5I_LOGI("Q5I GPU %s: %s %dx%d format=%s source=%s",
              label, outReal ? "REAL" : "FALLBACK",
              texture.width, texture.height,
              texture.format.c_str(), texture.sourcePath.c_str());
    return true;
}

bool InitializeChairRenderer() {
    if (gChairReady) return true;

    Fo3MaterialChairMesh mesh;
    if (!LoadMegatonChairMaterialMesh(mesh) || mesh.positions.empty() ||
        mesh.normals.empty() || mesh.tangents.empty() || mesh.bitangents.empty() ||
        mesh.texcoords.empty() || mesh.indices.empty()) {
        Q5I_LOGE("Q5I FAILED: material-aware chair mesh unavailable");
        return false;
    }

    const size_t vertexCount = mesh.positions.size() / 3u;
    if (mesh.normals.size() / 3u != vertexCount ||
        mesh.tangents.size() / 3u != vertexCount ||
        mesh.bitangents.size() / 3u != vertexCount ||
        mesh.texcoords.size() / 2u != vertexCount) {
        Q5I_LOGE("Q5I FAILED: material vertex attributes are not aligned");
        return false;
    }

    std::vector<Vec3> worldPositions(vertexCount);
    std::vector<Vec3> worldNormals(vertexCount);
    std::vector<Vec3> worldTangents(vertexCount);
    std::vector<Vec3> worldBitangents(vertexCount);

    Vec3 minimum{1e30f, 1e30f, 1e30f};
    Vec3 maximum{-1e30f, -1e30f, -1e30f};

    for (size_t i = 0; i < vertexCount; ++i) {
        Vec3 p = GamePositionToOpenXr(mesh.positions[i * 3u + 0u],
                                      mesh.positions[i * 3u + 1u],
                                      mesh.positions[i * 3u + 2u]);
        worldPositions[i] = p;
        worldNormals[i] = GameDirectionToOpenXr(mesh.normals[i * 3u + 0u],
                                                 mesh.normals[i * 3u + 1u],
                                                 mesh.normals[i * 3u + 2u]);
        worldTangents[i] = GameDirectionToOpenXr(mesh.tangents[i * 3u + 0u],
                                                  mesh.tangents[i * 3u + 1u],
                                                  mesh.tangents[i * 3u + 2u]);
        worldBitangents[i] = GameDirectionToOpenXr(mesh.bitangents[i * 3u + 0u],
                                                    mesh.bitangents[i * 3u + 1u],
                                                    mesh.bitangents[i * 3u + 2u]);

        minimum.x = std::min(minimum.x, p.x);
        minimum.y = std::min(minimum.y, p.y);
        minimum.z = std::min(minimum.z, p.z);
        maximum.x = std::max(maximum.x, p.x);
        maximum.y = std::max(maximum.y, p.y);
        maximum.z = std::max(maximum.z, p.z);
    }

    // NIF hierarchy now determines the model-space geometry. ESM placement is
    // the next milestone, so for this debug room only we still ground/centre
    // the finished model and place it in front of the player.
    const float centerX = (minimum.x + maximum.x) * 0.5f;
    const float centerZ = (minimum.z + maximum.z) * 0.5f;
    const Vec3 offset{-centerX,
                      FLOOR_Y - minimum.y,
                      CHAIR_DISTANCE_METRES - centerZ};
    for (Vec3& p : worldPositions) {
        p.x += offset.x;
        p.y += offset.y;
        p.z += offset.z;
    }

    std::vector<float> expanded;
    constexpr size_t FLOATS_PER_VERTEX = 14u;
    expanded.reserve(mesh.indices.size() * FLOATS_PER_VERTEX);

    for (uint32_t index : mesh.indices) {
        if (index >= vertexCount) {
            Q5I_LOGE("Q5I FAILED: triangle index exceeded vertex count");
            return false;
        }

        const Vec3& p = worldPositions[index];
        const Vec3& n = worldNormals[index];
        const Vec3& t = worldTangents[index];
        const Vec3& b = worldBitangents[index];
        const float u = mesh.texcoords[static_cast<size_t>(index) * 2u + 0u];
        const float v = 1.0f - mesh.texcoords[static_cast<size_t>(index) * 2u + 1u];

        expanded.push_back(p.x);
        expanded.push_back(p.y);
        expanded.push_back(p.z);
        expanded.push_back(n.x);
        expanded.push_back(n.y);
        expanded.push_back(n.z);
        expanded.push_back(t.x);
        expanded.push_back(t.y);
        expanded.push_back(t.z);
        expanded.push_back(b.x);
        expanded.push_back(b.y);
        expanded.push_back(b.z);
        expanded.push_back(u);
        expanded.push_back(v);
    }

    gChairProgram = CreateQ5IProgram();
    if (!gChairProgram) return false;

    gMvpLocation = glGetUniformLocation(gChairProgram, "uMvp");
    gDiffuseLocation = glGetUniformLocation(gChairProgram, "uDiffuse");
    gNormalLocation = glGetUniformLocation(gChairProgram, "uNormalGloss");
    gGlossinessLocation = glGetUniformLocation(gChairProgram, "uGlossiness");
    gNormalStrengthLocation = glGetUniformLocation(gChairProgram, "uNormalStrength");

    if (!UploadTexture(mesh.diffuseTexturePath,
                       {199u, 143u, 64u, 255u},
                       gDiffuseTexture, gRealDiffuseLoaded, "DIFFUSE")) return false;
    if (!UploadTexture(mesh.normalTexturePath,
                       {128u, 128u, 255u, 0u},
                       gNormalTexture, gRealNormalLoaded, "NORMAL/GLOSS")) return false;

    gGlossiness = std::max(2.0f, mesh.glossiness);

    glGenVertexArrays(1, &gChairVao);
    glBindVertexArray(gChairVao);
    glGenBuffers(1, &gChairVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gChairVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(expanded.size() * sizeof(float)),
                 expanded.data(), GL_STATIC_DRAW);

    constexpr GLsizei stride = static_cast<GLsizei>(FLOATS_PER_VERTEX * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(9 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(12 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glBindVertexArray(0);

    gChairVertexCount = static_cast<GLsizei>(expanded.size() / FLOATS_PER_VERTEX);
    gChairReady = glGetError() == GL_NO_ERROR;

    const float width = maximum.x - minimum.x;
    const float height = maximum.y - minimum.y;
    const float depth = maximum.z - minimum.z;
    if (gChairReady) {
        Q5I_LOGI("Q5I READY: chair03.nif vertices=%d triangles=%d size=%.2fm x %.2fm x %.2fm diffuse=%s normal=%s gloss=%.2f nifTransforms=REAL",
                  gChairVertexCount, gChairVertexCount / 3,
                  width, height, depth,
                  gRealDiffuseLoaded ? "REAL" : "FALLBACK",
                  gRealNormalLoaded ? "REAL" : "FALLBACK",
                  gGlossiness);
    } else {
        Q5I_LOGE("Q5I FAILED: GLES material mesh upload returned error");
    }
    return gChairReady;
}

void RenderInjectedChair() {
    if (!gChairReady || !gChairProgram || !gDiffuseTexture || !gNormalTexture ||
        gChairVertexCount <= 0) return;

    GLint mainProgram = 0;
    GLint mainVao = 0;
    GLint previousActiveTexture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &mainProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &mainVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    if (mainProgram == 0) return;

    const GLint sourceMvp = glGetUniformLocation(static_cast<GLuint>(mainProgram), "uMvp");
    if (sourceMvp < 0) return;

    GLfloat mvp[16]{};
    glGetUniformfv(static_cast<GLuint>(mainProgram), sourceMvp, mvp);

    GLint previousTexture0 = 0;
    GLint previousTexture1 = 0;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);

    glUseProgram(gChairProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp);
    glUniform1i(gDiffuseLocation, 0);
    glUniform1i(gNormalLocation, 1);
    glUniform1f(gGlossinessLocation, gGlossiness);
    glUniform1f(gNormalStrengthLocation, gRealNormalLoaded ? 1.0f : 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gDiffuseTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormalTexture);
    glBindVertexArray(gChairVao);
    glDrawArrays(GL_TRIANGLES, 0, gChairVertexCount);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindVertexArray(static_cast<GLuint>(mainVao));
    glUseProgram(static_cast<GLuint>(mainProgram));

    if (!gLoggedFirstDraw) {
        gLoggedFirstDraw = true;
        Q5I_LOGI("Q5I VISIBLE: Fallout 3 diffuse + RGB normal/gloss material submitted in both-eye OpenXR path normal=%s transforms=REAL",
                  gRealNormalLoaded ? "REAL" : "FALLBACK");
    }
}

void Q5IGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    glGenFramebuffers(n, framebuffers);
    InitializeChairRenderer();
}

void Q5IDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    glDeleteFramebuffers(n, framebuffers);
    if (gChairVbo) glDeleteBuffers(1, &gChairVbo);
    if (gChairVao) glDeleteVertexArrays(1, &gChairVao);
    if (gDiffuseTexture) glDeleteTextures(1, &gDiffuseTexture);
    if (gNormalTexture) glDeleteTextures(1, &gNormalTexture);
    if (gChairProgram) glDeleteProgram(gChairProgram);
    if (gDepthRenderbuffer) glDeleteRenderbuffers(1, &gDepthRenderbuffer);
    gChairVbo = 0;
    gChairVao = 0;
    gDiffuseTexture = 0;
    gNormalTexture = 0;
    gChairProgram = 0;
    gDepthRenderbuffer = 0;
    gChairReady = false;
    gRealDiffuseLoaded = false;
    gRealNormalLoaded = false;
    gLoggedFirstDraw = false;
}

void Q5IViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    glViewport(x, y, width, height);
    if (width <= 0 || height <= 0) return;

    if (!gDepthRenderbuffer) glGenRenderbuffers(1, &gDepthRenderbuffer);
    GLint previousRenderbuffer = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, gDepthRenderbuffer);
    if (gDepthWidth != width || gDepthHeight != height) {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        gDepthWidth = width;
        gDepthHeight = height;
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, gDepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(previousRenderbuffer));
}

void Q5IDisable(GLenum cap) {
    if (cap == GL_DEPTH_TEST) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        return;
    }
    glDisable(cap);
}

void Q5IClear(GLbitfield mask) {
    glClear(mask | GL_DEPTH_BUFFER_BIT);
}

void Q5IDrawArrays(GLenum mode, GLint first, GLsizei count) {
    // Replace Q4's three placeholder triangles with the material-correct chair.
    if (mode == GL_TRIANGLES && count == 3 && (first == 0 || first == 3)) {
        return;
    }
    if (mode == GL_TRIANGLES && count == 3 && first == 6) {
        RenderInjectedChair();
        return;
    }
    glDrawArrays(mode, first, count);
}

} // namespace

#define glGenFramebuffers Q5IGenFramebuffers
#define glDeleteFramebuffers Q5IDeleteFramebuffers
#define glViewport Q5IViewport
#define glDisable Q5IDisable
#define glClear Q5IClear
#define glDrawArrays Q5IDrawArrays
#include "q4-native.cpp"
#undef glDrawArrays
#undef glClear
#undef glDisable
#undef glViewport
#undef glDeleteFramebuffers
#undef glGenFramebuffers
