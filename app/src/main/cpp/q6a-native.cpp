#include "fo3-megaton-scene.h"
#include "fo3-static-nif.h"
#include "fo3-texture-bsa.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const char* Q6A_TAG = "FalloutQuest";
constexpr float FO3_UNITS_PER_METRE = 70.0f;
constexpr float FLOOR_Y = -1.55f;
constexpr float SCENE_FORWARD = -3.20f;
constexpr size_t MAX_SCENE_OBJECTS = 72u;
constexpr size_t MAX_MODEL_ATTEMPTS = 800u;
constexpr float MAX_MODEL_EXTENT_UNITS = 3000.0f;

#define Q6A_LOGI(...) __android_log_print(ANDROID_LOG_INFO, Q6A_TAG, __VA_ARGS__)
#define Q6A_LOGW(...) __android_log_print(ANDROID_LOG_WARN, Q6A_TAG, __VA_ARGS__)
#define Q6A_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, Q6A_TAG, __VA_ARGS__)

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 Normalize(Vec3 v) {
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length < 1e-7f) return {0.0f, 1.0f, 0.0f};
    v.x /= length;
    v.y /= length;
    v.z /= length;
    return v;
}

Vec3 RotateX(Vec3 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {v.x, c * v.y - s * v.z, s * v.y + c * v.z};
}

Vec3 RotateY(Vec3 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}

Vec3 RotateZ(Vec3 v, float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    return {c * v.x - s * v.y, s * v.x + c * v.y, v.z};
}

Vec3 ApplyEsmRotation(Vec3 v, const Fo3WorldPlacement& p) {
    v = RotateX(v, p.rx);
    v = RotateY(v, p.ry);
    v = RotateZ(v, p.rz);
    return v;
}

Vec3 GameDirectionToOpenXr(Vec3 v) {
    return Normalize({v.x, v.z, -v.y});
}

struct CpuObject {
    Fo3WorldPlacement placement;
    Fo3StaticNifMesh mesh;
    std::vector<Vec3> positionsGame;
    std::vector<Vec3> normalsGame;
    std::vector<Vec3> tangentsGame;
    std::vector<Vec3> bitangentsGame;
};

struct GpuObject {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint diffuse = 0;
    GLuint normal = 0;
    GLsizei vertexCount = 0;
    float glossiness = 10.0f;
    bool realDiffuse = false;
    bool realNormal = false;
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string editorId;
    std::string modelPath;
};

struct CachedGpuTexture {
    GLuint id = 0;
    bool real = false;
};

GLuint gProgram = 0;
GLint gMvpLocation = -1;
GLint gDiffuseLocation = -1;
GLint gNormalLocation = -1;
GLint gGlossinessLocation = -1;
GLint gNormalStrengthLocation = -1;
std::vector<GpuObject> gObjects;
std::unordered_map<std::string, CachedGpuTexture> gTextureCache;
GLuint gDepthRenderbuffer = 0;
GLsizei gDepthWidth = 0;
GLsizei gDepthHeight = 0;
bool gSceneReady = false;
bool gLoggedFirstDraw = false;

std::string TextureCacheKey(const std::string& path, const char* label) {
    if (path.empty()) return std::string("<fallback>:") + label;
    std::string key = path;
    for (char& ch : key) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return key;
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
        Q6A_LOGE("Q6A shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateProgram() {
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
            vec3 tangentNormal = normalGloss.rgb * 2.0 - 1.0;
            tangentNormal.xy *= uNormalStrength;
            tangentNormal = normalize(tangentNormal);
            tangentNormal.y = -tangentNormal.y;

            vec3 N = normalize(vNormal);
            vec3 T = normalize(vTangent - N * dot(N, vTangent));
            vec3 B = normalize(vBitangent - N * dot(N, vBitangent));
            vec3 mappedNormal = normalize(mat3(T, B, N) * tangentNormal);

            vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.40));
            float lambert = max(dot(mappedNormal, lightDirection), 0.0);
            vec3 viewDirection = normalize(vec3(0.0, 0.15, 1.0));
            vec3 halfVector = normalize(lightDirection + viewDirection);
            float exponent = clamp(uGlossiness, 2.0, 96.0);
            float specular = pow(max(dot(mappedNormal, halfVector), 0.0), exponent)
                           * normalGloss.a * 0.32;
            vec3 lit = diffuseTexel.rgb * (0.34 + 0.66 * lambert) + vec3(specular);
            fragColor = vec4(lit, diffuseTexel.a);
        }
    )";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
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
        Q6A_LOGE("Q6A shader link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool UploadTexture(const std::string& path,
                   const std::vector<uint8_t>& fallback,
                   GLuint& textureId, bool& real, const char* label,
                   uint32_t refFormId) {
    const std::string cacheKey = TextureCacheKey(path, label);
    const auto cached = gTextureCache.find(cacheKey);
    if (cached != gTextureCache.end()) {
        textureId = cached->second.id;
        real = cached->second.real;
        Q6A_LOGI("Q6A GPU %s CACHE HIT: ref=%08X key=%s real=%d",
                 label, refFormId, cacheKey.c_str(), real ? 1 : 0);
        return true;
    }

    Fo3RgbaTexture texture;
    real = !path.empty() && LoadFalloutTextureRgba(path, texture);
    if (!real) {
        texture.width = 1;
        texture.height = 1;
        texture.rgba = fallback;
        texture.sourcePath = "<fallback>";
        texture.format = "fallback";
    }

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (glGetError() != GL_NO_ERROR) {
        if (textureId) glDeleteTextures(1, &textureId);
        textureId = 0;
        return false;
    }

    gTextureCache.emplace(cacheKey, CachedGpuTexture{textureId, real});
    Q6A_LOGI("Q6A GPU %s CACHE MISS: ref=%08X source=%s %dx%d format=%s uniqueTextures=%zu",
             label, refFormId, real ? texture.sourcePath.c_str() : "<fallback>",
             texture.width, texture.height, texture.format.c_str(), gTextureCache.size());
    return true;
}

float MaxModelExtent(const Fo3StaticNifMesh& mesh) {
    if (mesh.positions.size() < 3u) return 1e30f;
    Vec3 minimum{1e30f, 1e30f, 1e30f};
    Vec3 maximum{-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i + 2u < mesh.positions.size(); i += 3u) {
        minimum.x = std::min(minimum.x, mesh.positions[i]);
        minimum.y = std::min(minimum.y, mesh.positions[i + 1u]);
        minimum.z = std::min(minimum.z, mesh.positions[i + 2u]);
        maximum.x = std::max(maximum.x, mesh.positions[i]);
        maximum.y = std::max(maximum.y, mesh.positions[i + 1u]);
        maximum.z = std::max(maximum.z, mesh.positions[i + 2u]);
    }
    return std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z});
}

bool BuildCpuObject(const Fo3WorldPlacement& placement, CpuObject& out) {
    Fo3StaticNifMesh mesh;
    if (!LoadFo3StaticNif(placement.modelPath, mesh)) return false;

    const float extent = MaxModelExtent(mesh) * placement.scale;
    if (!(extent >= 1.0f && extent <= MAX_MODEL_EXTENT_UNITS)) {
        Q6A_LOGW("Q6A SKIP SIZE: ref=%08X model=%s extent=%.1f",
                 placement.refFormId, placement.modelPath.c_str(), extent);
        return false;
    }

    const size_t vertexCount = mesh.positions.size() / 3u;
    if (mesh.normals.size() / 3u != vertexCount ||
        mesh.tangents.size() / 3u != vertexCount ||
        mesh.bitangents.size() / 3u != vertexCount ||
        mesh.texcoords.size() / 2u != vertexCount) return false;

    out = {};
    out.placement = placement;
    out.mesh = std::move(mesh);
    out.positionsGame.resize(vertexCount);
    out.normalsGame.resize(vertexCount);
    out.tangentsGame.resize(vertexCount);
    out.bitangentsGame.resize(vertexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        Vec3 position{
            out.mesh.positions[i * 3u] * placement.scale,
            out.mesh.positions[i * 3u + 1u] * placement.scale,
            out.mesh.positions[i * 3u + 2u] * placement.scale,
        };
        position = ApplyEsmRotation(position, placement);
        position.x += placement.x;
        position.y += placement.y;
        position.z += placement.z;
        out.positionsGame[i] = position;

        out.normalsGame[i] = Normalize(ApplyEsmRotation({
            out.mesh.normals[i * 3u], out.mesh.normals[i * 3u + 1u], out.mesh.normals[i * 3u + 2u]}, placement));
        out.tangentsGame[i] = Normalize(ApplyEsmRotation({
            out.mesh.tangents[i * 3u], out.mesh.tangents[i * 3u + 1u], out.mesh.tangents[i * 3u + 2u]}, placement));
        out.bitangentsGame[i] = Normalize(ApplyEsmRotation({
            out.mesh.bitangents[i * 3u], out.mesh.bitangents[i * 3u + 1u], out.mesh.bitangents[i * 3u + 2u]}, placement));
    }

    Q6A_LOGI("Q6A CPU OBJECT: ref=%08X base=%08X EDID=%s model=%s vertices=%zu triangles=%zu ESM=(%.1f %.1f %.1f / %.3f %.3f %.3f / %.3f)",
             placement.refFormId, placement.baseFormId,
             placement.editorId.empty() ? "<none>" : placement.editorId.c_str(),
             placement.modelPath.c_str(), vertexCount, out.mesh.indices.size() / 3u,
             placement.x, placement.y, placement.z,
             placement.rx, placement.ry, placement.rz, placement.scale);
    return true;
}

bool UploadCpuObject(CpuObject& cpu, float centerX, float centerY, float floorZ,
                     GpuObject& gpu) {
    constexpr size_t FLOATS_PER_VERTEX = 14u;
    const size_t vertexCount = cpu.positionsGame.size();
    std::vector<float> expanded;
    expanded.reserve(cpu.mesh.indices.size() * FLOATS_PER_VERTEX);

    for (uint32_t index : cpu.mesh.indices) {
        if (index >= vertexCount) return false;
        const Vec3 gameP = cpu.positionsGame[index];
        const Vec3 p{
            (gameP.x - centerX) / FO3_UNITS_PER_METRE,
            FLOOR_Y + (gameP.z - floorZ) / FO3_UNITS_PER_METRE,
            SCENE_FORWARD - (gameP.y - centerY) / FO3_UNITS_PER_METRE,
        };
        const Vec3 n = GameDirectionToOpenXr(cpu.normalsGame[index]);
        const Vec3 t = GameDirectionToOpenXr(cpu.tangentsGame[index]);
        const Vec3 b = GameDirectionToOpenXr(cpu.bitangentsGame[index]);
        const float u = cpu.mesh.texcoords[static_cast<size_t>(index) * 2u];
        const float v = 1.0f - cpu.mesh.texcoords[static_cast<size_t>(index) * 2u + 1u];

        expanded.insert(expanded.end(), {
            p.x, p.y, p.z,
            n.x, n.y, n.z,
            t.x, t.y, t.z,
            b.x, b.y, b.z,
            u, v,
        });
    }
    if (expanded.empty()) return false;

    gpu = {};
    gpu.glossiness = std::max(2.0f, cpu.mesh.glossiness);
    gpu.refFormId = cpu.placement.refFormId;
    gpu.baseFormId = cpu.placement.baseFormId;
    gpu.editorId = cpu.placement.editorId;
    gpu.modelPath = cpu.placement.modelPath;

    if (!UploadTexture(cpu.mesh.diffuseTexturePath, {190u, 170u, 135u, 255u},
                       gpu.diffuse, gpu.realDiffuse, "DIFFUSE", gpu.refFormId)) return false;
    if (!UploadTexture(cpu.mesh.normalTexturePath, {128u, 128u, 255u, 0u},
                       gpu.normal, gpu.realNormal, "NORMAL", gpu.refFormId)) return false;

    glGenVertexArrays(1, &gpu.vao);
    glBindVertexArray(gpu.vao);
    glGenBuffers(1, &gpu.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
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

    gpu.vertexCount = static_cast<GLsizei>(expanded.size() / FLOATS_PER_VERTEX);
    if (glGetError() != GL_NO_ERROR) return false;

    Q6A_LOGI("Q6A GPU OBJECT READY: ref=%08X EDID=%s model=%s triangles=%d diffuse=%s normal=%s",
             gpu.refFormId, gpu.editorId.empty() ? "<none>" : gpu.editorId.c_str(),
             gpu.modelPath.c_str(), gpu.vertexCount / 3,
             gpu.realDiffuse ? "REAL" : "FALLBACK",
             gpu.realNormal ? "REAL" : "FALLBACK");
    return true;
}

bool InitializeScene() {
    if (gSceneReady) return true;

    std::vector<Fo3WorldPlacement> placements;
    if (!LoadMegatonPlayerHousePlacements(placements)) {
        Q6A_LOGE("Q6A FAILED: ESM returned no MegatonPlayerHouse model placements");
        return false;
    }

    float centroidX = 0.0f, centroidY = 0.0f, centroidZ = 0.0f;
    for (const auto& p : placements) {
        centroidX += p.x;
        centroidY += p.y;
        centroidZ += p.z;
    }
    centroidX /= static_cast<float>(placements.size());
    centroidY /= static_cast<float>(placements.size());
    centroidZ /= static_cast<float>(placements.size());

    std::stable_sort(placements.begin(), placements.end(),
                     [&](const Fo3WorldPlacement& a, const Fo3WorldPlacement& b) {
        const float adx = a.x - centroidX, ady = a.y - centroidY, adz = a.z - centroidZ;
        const float bdx = b.x - centroidX, bdy = b.y - centroidY, bdz = b.z - centroidZ;
        return adx * adx + ady * ady + adz * adz < bdx * bdx + bdy * bdy + bdz * bdz;
    });

    std::vector<CpuObject> selected;
    selected.reserve(MAX_SCENE_OBJECTS);
    size_t attempts = 0;
    for (const Fo3WorldPlacement& placement : placements) {
        if (selected.size() >= MAX_SCENE_OBJECTS || attempts >= MAX_MODEL_ATTEMPTS) break;
        ++attempts;
        CpuObject object;
        if (BuildCpuObject(placement, object)) selected.push_back(std::move(object));
    }

    if (selected.size() < 2u) {
        Q6A_LOGE("Q6A FAILED: only %zu supported ESM-driven objects after %zu attempts",
                 selected.size(), attempts);
        return false;
    }

    Vec3 minimum{1e30f, 1e30f, 1e30f};
    Vec3 maximum{-1e30f, -1e30f, -1e30f};
    for (const CpuObject& object : selected) {
        for (const Vec3& p : object.positionsGame) {
            minimum.x = std::min(minimum.x, p.x);
            minimum.y = std::min(minimum.y, p.y);
            minimum.z = std::min(minimum.z, p.z);
            maximum.x = std::max(maximum.x, p.x);
            maximum.y = std::max(maximum.y, p.y);
            maximum.z = std::max(maximum.z, p.z);
        }
    }

    const float centerX = (minimum.x + maximum.x) * 0.5f;
    const float centerY = (minimum.y + maximum.y) * 0.5f;
    const float floorZ = minimum.z;
    Q6A_LOGI("Q6A SCENE FRAME: selected=%zu attempts=%zu gameBounds=[%.1f %.1f %.1f]-[%.1f %.1f %.1f] sizeMetres=(%.2f %.2f %.2f) globalOffsetOnly=1",
             selected.size(), attempts,
             minimum.x, minimum.y, minimum.z,
             maximum.x, maximum.y, maximum.z,
             (maximum.x - minimum.x) / FO3_UNITS_PER_METRE,
             (maximum.y - minimum.y) / FO3_UNITS_PER_METRE,
             (maximum.z - minimum.z) / FO3_UNITS_PER_METRE);

    gProgram = CreateProgram();
    if (!gProgram) return false;
    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    gDiffuseLocation = glGetUniformLocation(gProgram, "uDiffuse");
    gNormalLocation = glGetUniformLocation(gProgram, "uNormalGloss");
    gGlossinessLocation = glGetUniformLocation(gProgram, "uGlossiness");
    gNormalStrengthLocation = glGetUniformLocation(gProgram, "uNormalStrength");

    gObjects.reserve(selected.size());
    for (CpuObject& cpu : selected) {
        GpuObject gpu;
        if (UploadCpuObject(cpu, centerX, centerY, floorZ, gpu)) {
            gObjects.push_back(std::move(gpu));
        }
    }

    gSceneReady = gObjects.size() >= 2u;
    if (gSceneReady) {
        size_t realDiffuse = 0, realNormal = 0;
        size_t triangles = 0;
        for (const GpuObject& object : gObjects) {
            if (object.realDiffuse) ++realDiffuse;
            if (object.realNormal) ++realNormal;
            triangles += static_cast<size_t>(object.vertexCount / 3);
        }
        Q6A_LOGI("Q6A READY: ESM->REFR->BASE->MODL->BSA->NIF objects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu uniqueTextures=%zu cell=MegatonPlayerHouse",
                 gObjects.size(), triangles, realDiffuse, realNormal, gTextureCache.size());
    } else {
        Q6A_LOGE("Q6A FAILED: GPU scene objects=%zu", gObjects.size());
    }
    return gSceneReady;
}

void RenderScene() {
    if (!gSceneReady || !gProgram || gObjects.empty()) return;

    GLint mainProgram = 0, mainVao = 0, previousActiveTexture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &mainProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &mainVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    if (mainProgram == 0) return;

    const GLint sourceMvp = glGetUniformLocation(static_cast<GLuint>(mainProgram), "uMvp");
    if (sourceMvp < 0) return;
    GLfloat mvp[16]{};
    glGetUniformfv(static_cast<GLuint>(mainProgram), sourceMvp, mvp);

    GLint previousTexture0 = 0, previousTexture1 = 0;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp);
    glUniform1i(gDiffuseLocation, 0);
    glUniform1i(gNormalLocation, 1);

    for (const GpuObject& object : gObjects) {
        glUniform1f(gGlossinessLocation, object.glossiness);
        glUniform1f(gNormalStrengthLocation, object.realNormal ? 1.0f : 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, object.diffuse);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, object.normal);
        glBindVertexArray(object.vao);
        glDrawArrays(GL_TRIANGLES, 0, object.vertexCount);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindVertexArray(static_cast<GLuint>(mainVao));
    glUseProgram(static_cast<GLuint>(mainProgram));

    if (!gLoggedFirstDraw) {
        gLoggedFirstDraw = true;
        Q6A_LOGI("Q6A VISIBLE: %zu Bethesda-placed Megaton objects submitted to both-eye OpenXR render path using ESM transforms",
                 gObjects.size());
    }
}

void Q6AGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    glGenFramebuffers(n, framebuffers);
    InitializeScene();
}

void Q6ADeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    glDeleteFramebuffers(n, framebuffers);
    for (GpuObject& object : gObjects) {
        if (object.vbo) glDeleteBuffers(1, &object.vbo);
        if (object.vao) glDeleteVertexArrays(1, &object.vao);
    }
    gObjects.clear();
    for (const auto& entry : gTextureCache) {
        const GLuint id = entry.second.id;
        if (id) glDeleteTextures(1, &id);
    }
    gTextureCache.clear();
    if (gProgram) glDeleteProgram(gProgram);
    if (gDepthRenderbuffer) glDeleteRenderbuffers(1, &gDepthRenderbuffer);
    gProgram = 0;
    gDepthRenderbuffer = 0;
    gSceneReady = false;
    gLoggedFirstDraw = false;
}

void Q6AViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    glViewport(x, y, width, height);
    if (width <= 0 || height <= 0) return;
    if (!gDepthRenderbuffer) glGenRenderbuffers(1, &gDepthRenderbuffer);
    GLint previous = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous);
    glBindRenderbuffer(GL_RENDERBUFFER, gDepthRenderbuffer);
    if (gDepthWidth != width || gDepthHeight != height) {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        gDepthWidth = width;
        gDepthHeight = height;
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, gDepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(previous));
}

void Q6ADisable(GLenum cap) {
    if (cap == GL_DEPTH_TEST) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        return;
    }
    glDisable(cap);
}

void Q6AClear(GLbitfield mask) {
    glClear(mask | GL_DEPTH_BUFFER_BIT);
}

void Q6ADrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (mode == GL_TRIANGLES && count == 3 && (first == 0 || first == 3)) return;
    if (mode == GL_TRIANGLES && count == 3 && first == 6) {
        RenderScene();
        return;
    }
    glDrawArrays(mode, first, count);
}

} // namespace

#define glGenFramebuffers Q6AGenFramebuffers
#define glDeleteFramebuffers Q6ADeleteFramebuffers
#define glViewport Q6AViewport
#define glDisable Q6ADisable
#define glClear Q6AClear
#define glDrawArrays Q6ADrawArrays
#include "q4-native.cpp"
#undef glDrawArrays
#undef glClear
#undef glDisable
#undef glViewport
#undef glDeleteFramebuffers
#undef glGenFramebuffers
