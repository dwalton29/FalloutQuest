
float GetFo3FogPower();

#include "rendering/terrain/fo3-terrain.h"
#include "rendering/terrain/fo3-terrain-bounds.h"
#include "rendering/exact-indexed-mesh.h"
#include "rendering/terrain/terrain-paint-coverage.h"
#include "rendering/terrain/terrain-layer-fusion.h"
#include "rendering/terrain/terrain-layer-options.h"
#include "rendering/terrain/fo3-terrain-v237-shaders.h"
#include "rendering/environment/fo3-environment.h"
#include "rendering/environment/fo3-weather-byte-staging.h"
#include "rendering/environment/fo3-visual-depth.h"
#include "fo3-texture-bsa.h"
#include "rendering/loading-diagnostics.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <unordered_set>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr const char* Q76B_TAG = "FalloutQuest";
constexpr int Q76B_LAND_SIDE = 33;
constexpr int Q76B_LAND_QUADS = 32;
constexpr float Q76B_CELL_SIZE = 4096.0f;
constexpr float Q76B_VERTEX_SPACING = Q76B_CELL_SIZE / static_cast<float>(Q76B_LAND_QUADS);
constexpr size_t Q76B_HEIGHT_COUNT = static_cast<size_t>(Q76B_LAND_SIDE * Q76B_LAND_SIDE);
constexpr float Q711_TEXTURE_REPEAT_GAME_UNITS = 512.0f;
constexpr size_t Q713_MAX_REAL_TEXTURES = 64u;
constexpr float Q718_MAX_SEAM_STITCH_DELTA = 64.0f;
constexpr float Q718_ALPHA_EPS = 0.001f;

#define Q76B_LOGI(...) __android_log_print(ANDROID_LOG_INFO, Q76B_TAG, __VA_ARGS__)
#define Q76B_LOGW(...) __android_log_print(ANDROID_LOG_WARN, Q76B_TAG, __VA_ARGS__)
#define Q76B_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, Q76B_TAG, __VA_ARGS__)

struct Q76BV3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Q711TerrainBatch : fo3terrain::Bounds {
    int32_t gridX = 0;
    int32_t gridY = 0;
    std::string texturePath;
    std::string normalPath;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;
    GLuint normalTexture = 0;
    GLsizei vertexCount = 0;
    // Vertices and uint32 indices share one GL buffer and one lifetime.
    // Zero means the original array path; indexed data starts after vertices.
    size_t indexOffset = 0;
    int paintCount = 0;
    std::array<GLuint,3> paintDiffuse = {};
    std::array<GLuint,3> paintNormal = {};
    std::array<bool,3> paintSpecular = {};
    int layerOrder = -1;
    bool alphaLayer = false;
    bool realTexture = false;
    bool realNormal = false;
    bool specularEnabled = false;
    bool ownsTexture = false;
    bool ownsNormal = false;
};

struct Q711CpuBatch {
    int32_t gridX = 0;
    int32_t gridY = 0;
    std::string texturePath;
    std::string normalPath;
    bool specularEnabled = false;
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    size_t vertexFloats = 12u;
    std::vector<fo3terrain::PaintMaterial> paintMaterials;
    int layerOrder = -1;
    bool alphaLayer = false;
};

void Q237DrawTerrainBatch(const Q711TerrainBatch& batch) {
    if (batch.indexOffset)
        glDrawElements(GL_TRIANGLES, batch.vertexCount, GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(batch.indexOffset));
    else glDrawArrays(GL_TRIANGLES, 0, batch.vertexCount);
}

void Q237PrepareTerrainIndices(Q711CpuBatch& batch) {
    if(!batch.indices.empty())return;
    if(batch.vertexFloats==15u)fqmesh::IndexExact<15>(batch.vertices,batch.indices);
    else fqmesh::IndexExact<12>(batch.vertices, batch.indices);
}

GLuint q76bProgram = 0;
GLint q76bMvpLocation = -1;
GLint q76bSamplerLocation = -1;
GLint q76bUseTextureLocation = -1;
GLint q238PaintCountLocation = -1, q238RoundModeLocation = -1;
std::array<GLint,3> q238PaintDiffuseLocation = {}, q238PaintNormalLocation = {}, q238PaintFlagsLocation = {};
int q238TerrainRoundMode = 1; // 1=FP16, 2=RGBA8; supplied by the active target.
GLint q1060TerrainNormalSamplerLocation = -1;
GLint q1060TerrainUseNormalLocation = -1;
GLint q1060TerrainSpecularLocation = -1;
GLint q1000TerrainAmbientLocation = -1;
GLint q1000TerrainSunlightLocation = -1;
GLint q1000TerrainSunDirectionLocation = -1;
GLint q1010TerrainEyeLocation = -1;
GLint q1010TerrainFogColorLocation = -1;
GLint q1010TerrainFogNearLocation = -1;
GLint q1010TerrainFogFarLocation = -1;
GLint q1410TerrainFogPowerLocation = -1;
GLint q1532TerrainFogNearVertexLocation = -1;
GLint q1532TerrainFogFarVertexLocation = -1;
GLint q1532TerrainFogPowerVertexLocation = -1;
GLint q1010TerrainLightCountLocation = -1;
GLint q1010TerrainLightPosRadiusLocation = -1;
GLint q1010TerrainLightColorFalloffLocation = -1;
GLint q1050TerrainLightMvpLocation = -1;
GLint q1050TerrainShadowMapLocation = -1;
GLint q1050TerrainShadowTexelLocation = -1;
GLint q1050TerrainShadowsEnabledLocation = -1;
GLint q2090TerrainReflectionClipEnabledLocation = -1;
GLint q2090TerrainReflectionPlaneYLocation = -1;
bool q2090TerrainReflectionClipEnabled = false;
float q2090TerrainReflectionPlaneY = 0.0f;
// LAND's vertex shader passes aPosition directly to vPosition, and the
// original reflection fragment clip discards positions below the plane.
// An entire batch strictly below that plane therefore contributes no pixel.
// This plane rejection is separate from the conservative homogeneous clip test.
bool Q228EntireTerrainBatchBelowReflectionPlane(const Q711TerrainBatch& batch) {
    return batch.boundsValid && q2090TerrainReflectionClipEnabled &&
           std::isfinite(batch.maxY) && std::isfinite(q2090TerrainReflectionPlaneY) &&
           batch.maxY < q2090TerrainReflectionPlaneY - 0.001f;
}
GLuint q1050TerrainShadowTexture = 0;
float q1050TerrainLightMvp[16]{};
bool q1050TerrainShadowEnabled = false;
std::vector<Q711TerrainBatch> q711TerrainBatches;
// Q229: preserve original LAND base-then-painted composition, but classify
// published terrain batches once instead of scanning all batches for every
// mirror/main eye. Every structural mutation below invalidates the pointers.
uint64_t q229TerrainBatchRevision=1u,q229TerrainOrderRevision=0u;
std::vector<const Q711TerrainBatch*> q229TerrainBaseBatches,q229TerrainPaintBatches;
void Q229TouchTerrainBatches(){++q229TerrainBatchRevision;}
void Q229PrepareTerrainDrawLists(){
    if(q229TerrainBatchRevision==q229TerrainOrderRevision)return;
    q229TerrainBaseBatches.clear();q229TerrainPaintBatches.clear();
    q229TerrainBaseBatches.reserve(q711TerrainBatches.size());
    q229TerrainPaintBatches.reserve(q711TerrainBatches.size());
    for(const auto& batch:q711TerrainBatches){
        if(!batch.vao||batch.vertexCount<=0)continue;
        if(batch.alphaLayer){
            if(batch.realTexture)q229TerrainPaintBatches.push_back(&batch);
        }else q229TerrainBaseBatches.push_back(&batch);
    }
    q229TerrainOrderRevision=q229TerrainBatchRevision;
}
std::unordered_map<std::string, GLuint> q1910TerrainTextureCache;
std::unordered_map<std::string, GLuint> q1810TerrainNormalTextureCache;
Q711TerrainBatch Q238GpuMaterial(const Q711CpuBatch& cpu) {
    Q711TerrainBatch gpu;
    gpu.gridX=cpu.gridX;gpu.gridY=cpu.gridY;
    gpu.texturePath=cpu.texturePath;gpu.normalPath=cpu.normalPath;
    gpu.alphaLayer=cpu.alphaLayer;gpu.layerOrder=cpu.layerOrder;
    gpu.specularEnabled=cpu.specularEnabled;
    auto lookup=[](const auto& cache,const std::string& path){
        const auto found=cache.find(path);return found==cache.end()?0u:found->second;
    };
    gpu.texture=lookup(q1910TerrainTextureCache,cpu.texturePath);
    gpu.normalTexture=lookup(q1810TerrainNormalTextureCache,cpu.normalPath);
    gpu.realTexture=gpu.texture!=0u;gpu.realNormal=gpu.normalTexture!=0u;
    gpu.paintCount=static_cast<int>(cpu.paintMaterials.size());
    for(size_t i=0;i<cpu.paintMaterials.size()&&i<3u;++i){
        gpu.paintDiffuse[i]=lookup(q1910TerrainTextureCache,cpu.paintMaterials[i].texturePath);
        gpu.paintNormal[i]=lookup(q1810TerrainNormalTextureCache,cpu.paintMaterials[i].normalPath);
        gpu.paintSpecular[i]=cpu.paintMaterials[i].specularEnabled;
    }
    static_cast<fo3terrain::Bounds&>(gpu)=fo3terrain::BuildBounds(cpu.vertices.data(),cpu.vertices.size(),cpu.vertexFloats);
    return gpu;
}
void Q238BindPaint(const Q711TerrainBatch& batch) {
    glUniform1i(q238PaintCountLocation,batch.paintCount);
    if(!batch.paintCount)return;
    constexpr unsigned diffuseUnits[3]={2,5,7},normalUnits[3]={4,6,8};
    for(int i=0;i<batch.paintCount;++i){
        glUniform1i(q238PaintDiffuseLocation[i],static_cast<GLint>(diffuseUnits[i]));
        glUniform1i(q238PaintNormalLocation[i],static_cast<GLint>(normalUnits[i]));
        glUniform2f(q238PaintFlagsLocation[i],batch.paintNormal[i]?1.0f:0.0f,batch.paintSpecular[i]?1.0f:0.0f);
        glActiveTexture(GL_TEXTURE0+diffuseUnits[i]);glBindTexture(GL_TEXTURE_2D,batch.paintDiffuse[i]);
        glActiveTexture(GL_TEXTURE0+normalUnits[i]);glBindTexture(GL_TEXTURE_2D,batch.paintNormal[i]);
    }
    glActiveTexture(GL_TEXTURE0);
}
constexpr size_t Q1910_TERRAIN_TEXTURE_CACHE_LIMIT = 256u;
constexpr size_t Q1810_TERRAIN_NORMAL_CACHE_LIMIT = 256u;

bool q76bReady = false;
bool q76bLoggedVisible = false;
uint32_t q76bWorldspace = 0;
size_t q76bTerrainCells = 0;
size_t q76bTriangles = 0;
size_t q718AlphaTriangles = 0;
size_t q711RealTextureBatches = 0;
size_t q711FallbackBatches = 0;
size_t q713TextureUploadFailures = 0;
size_t q713TextureLimitHits = 0;
size_t q718AlphaBatches = 0;
size_t q718AlphaQuads = 0;
size_t q718AlphaLayersSeen = 0;
size_t q718AlphaLayersResolved = 0;
size_t q718AlphaLayersUnresolved = 0;
size_t q720HiddenBaseQuads = 0;
size_t q720HiddenAlphaLayers = 0;

uint64_t Q711GridKey(int32_t x, int32_t y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) |
           static_cast<uint32_t>(y);
}


Q76BV3 Q76BSub(const Q76BV3& a, const Q76BV3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Q76BV3 Q76BNormalize(Q76BV3 v) {
    const float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-6f) return {0.0f, 1.0f, 0.0f};
    return {v.x / len, v.y / len, v.z / len};
}

Q76BV3 Q76BCross(const Q76BV3& a, const Q76BV3& b) {
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x,
    };
}

void Q718PushTriangle(std::vector<float>& out,
                      const Q76BV3& a, float au, float av,
                      const Q76BV3& b, float bu, float bv,
                      const Q76BV3& c, float cu, float cv,
                      float alphaA = 1.0f,
                      float alphaB = 1.0f,
                      float alphaC = 1.0f,
                      Q76BV3 authoredA = {},
                      Q76BV3 authoredB = {},
                      Q76BV3 authoredC = {},
                      Q76BV3 colorA = {1.0f, 1.0f, 1.0f},
                      Q76BV3 colorB = {1.0f, 1.0f, 1.0f},
                      Q76BV3 colorC = {1.0f, 1.0f, 1.0f}) {
    Q76BV3 n = Q76BNormalize(Q76BCross(Q76BSub(b, a), Q76BSub(c, a)));
    if (n.y < 0.0f) n = {-n.x, -n.y, -n.z};
    const Q76BV3 points[3]{a, b, c};
    const Q76BV3 authoredNormals[3]{authoredA, authoredB, authoredC};
    const Q76BV3 authoredColors[3]{colorA, colorB, colorC};
    const float uv[6]{au, av, bu, bv, cu, cv};
    const float alpha[3]{alphaA, alphaB, alphaC};
    for (int i = 0; i < 3; ++i) {
        const Q76BV3& p = points[i];
        Q76BV3 vertexNormal = authoredNormals[i];
        const float authoredLength = std::sqrt(vertexNormal.x*vertexNormal.x +
                                               vertexNormal.y*vertexNormal.y +
                                               vertexNormal.z*vertexNormal.z);
        if (authoredLength < 1e-5f) vertexNormal = n;
        else vertexNormal = Q76BNormalize(vertexNormal);
        out.insert(out.end(), {
            p.x, p.y, p.z,
            vertexNormal.x, vertexNormal.y, vertexNormal.z,
            uv[i * 2], uv[i * 2 + 1],
            std::clamp(alpha[i], 0.0f, 1.0f),
            std::clamp(authoredColors[i].x, 0.0f, 1.0f),
            std::clamp(authoredColors[i].y, 0.0f, 1.0f),
            std::clamp(authoredColors[i].z, 0.0f, 1.0f)
        });
    }
}

GLuint Q76BCompile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Q76B_LOGE("Q7.20 terrain shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Q76BCreateProgram() {
    static const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;
        layout(location = 3) in float aAlpha;
        layout(location = 4) in vec3 aVertexColor;
        layout(location = 5) in vec3 aPaintAlpha;
        uniform mat4 uMvp;
        uniform highp float uFogNearVertexQ1532;
        uniform highp float uFogFarVertexQ1532;
        uniform highp float uFogPowerVertexQ1532;
        out vec3 vNormal;
        out highp float vFogFactorQ1532;
        out vec2 vTexCoord;
        out float vAlpha;
        out vec3 vPaintAlpha;
        out vec3 vVertexColor;
        out vec3 vPosition;
        out vec4 vShadowCoord;
        uniform mat4 uLightMvp;
        void main() {
            vNormal = aNormal;
            vTexCoord = aTexCoord;
            vAlpha = aAlpha;
            vPaintAlpha = aPaintAlpha;
            vVertexColor = aVertexColor;
            vPosition = aPosition;
            vShadowCoord = uLightMvp * vec4(aPosition, 1.0);
            vec4 q1532Clip = uMvp * vec4(aPosition, 1.0);
            gl_Position = q1532Clip;
            float q1532FogDistance = length(q1532Clip.xyz);
            float q1532FogT = 1.0 - clamp(
                (uFogFarVertexQ1532 - q1532FogDistance) /
                (uFogFarVertexQ1532 - uFogNearVertexQ1532), 0.0, 1.0);
            vFogFactorQ1532 = pow(q1532FogT, uFogPowerVertexQ1532);
        }
    )";
    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;
        in vec3 vNormal;
        in vec2 vTexCoord;
        in float vAlpha;
        in vec3 vPaintAlpha;
        in vec3 vVertexColor;
        in vec3 vPosition;
        in highp float vFogFactorQ1532;
        in vec4 vShadowCoord;
        uniform sampler2D uDiffuse;
        uniform sampler2D uNormalGloss;
        uniform int uUseNormal;
        uniform float uTerrainSpecularEnabled;
        uniform int uUseTexture;
        uniform vec3 uAmbientColor;
        uniform vec3 uSunlightColor;
        uniform vec3 uSunDirection;
        uniform vec3 uEyePosition;
        uniform vec3 uFogColor;
        uniform float uFogNear;
        uniform float uFogFar;
        uniform float uFogPower;
        uniform int uLocalLightCount;
        uniform vec4 uLocalLightPosRadius[8];
        uniform vec4 uLocalLightColorFalloff[8];
        uniform sampler2D uShadowMap;
        uniform vec2 uShadowTexelSize;
        uniform float uShadowsEnabled;
        uniform float uWaterReflectionClipEnabledQ2090;
        uniform float uWaterReflectionPlaneYQ2090;
        uniform int uPaintCount;
        uniform int uRoundMode;
        uniform sampler2D uPaintDiffuse0;
        uniform sampler2D uPaintNormal0;
        uniform vec2 uPaintFlags0;
        uniform sampler2D uPaintDiffuse1;
        uniform sampler2D uPaintNormal1;
        uniform vec2 uPaintFlags1;
        uniform sampler2D uPaintDiffuse2;
        uniform sampler2D uPaintNormal2;
        uniform vec2 uPaintFlags2;
        out highp vec4 fragColor;
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
// Q239: geometric lighting terms are shared by the original LAND materials;
        // normals, shadow/specular evaluation and layer blending remain per material.
        struct Q239SharedLighting {
            vec3 sunDirection;
            vec3 localDirection[8];
            float localAttenuation[8];
        };
        Q239SharedLighting Q239PrepareSharedLighting() {
            Q239SharedLighting shared;
            shared.sunDirection = normalize(uSunDirection);
            for (int i = 0; i < 8; ++i) {
                if (i >= uLocalLightCount) break;
                vec3 toLight = uLocalLightPosRadius[i].xyz - vPosition;
                float distanceToLight = length(toLight);
                float radius = max(uLocalLightPosRadius[i].w, 0.001);
                if (distanceToLight >= radius) {
                    shared.localAttenuation[i] = -1.0;
                    continue;
                }
                shared.localDirection[i] = toLight / max(distanceToLight, 0.001);
                float edge = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
                shared.localAttenuation[i] =
                    pow(edge, max(uLocalLightColorFalloff[i].w, 0.25));
            }
            return shared;
        }
        vec3 Q238ShadeLayer(sampler2D diffuseMap, sampler2D normalMap,
                             int useTexture, int useNormal, float specularEnabled,
                             Q239SharedLighting shared) {
            vec3 N = normalize(vNormal);
            vec4 normalGloss = vec4(0.5, 0.5, 1.0, 0.0);
            if (useNormal != 0) {
                normalGloss = texture(normalMap, vTexCoord);
                vec3 tangentNormal = normalize(normalGloss.xyz * 2.0 - 1.0);
                vec3 authoredU = vec3(1.0, 0.0, 0.0);
                vec3 T = authoredU - N * dot(N, authoredU);
                if (dot(T, T) < 0.000001) T = vec3(0.0, 0.0, -1.0);
                T = normalize(T);
                // Increasing LAND V follows game +Y, which is renderer -Z.
                vec3 B = normalize(cross(N, T));
                N = normalize(mat3(T, B, N) * tangentNormal);
            }

#ifdef FQ239_UNSHARED_LIGHTS
            vec3 lightDirection = normalize(uSunDirection);
#else
            vec3 lightDirection = shared.sunDirection;
#endif
            float lambert = max(dot(N, lightDirection), 0.0);
            vec3 earthFallback = vec3(0.34, 0.27, 0.18);
            vec3 albedo = (useTexture != 0 ? texture(diffuseMap, vTexCoord).rgb : earthFallback) * vVertexColor;
            float q1050SunVisibility = Q1050ShadowVisibility(vShadowCoord, N, lightDirection);
            vec3 q1570LandWorldDiffuse =
                albedo * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);
            vec3 lit = q1570LandWorldDiffuse;
            if (useNormal != 0 && specularEnabled > 0.5) {
                vec3 V = normalize(uEyePosition - vPosition);
                vec3 H = normalize(lightDirection + V);
                float specular = pow(max(dot(N, H), 0.0), 18.0) * normalGloss.a * 0.24;
                lit += uSunlightColor * specular * q1050SunVisibility;
            }
#ifdef FQ239_UNSHARED_LIGHTS
            for (int i = 0; i < 8; ++i) {
                if (i >= uLocalLightCount) break;
                vec3 toLight = uLocalLightPosRadius[i].xyz - vPosition;
                float distanceToLight = length(toLight);
                float radius = max(uLocalLightPosRadius[i].w, 0.001);
                if (distanceToLight >= radius) continue;
                vec3 L = toLight / max(distanceToLight, 0.001);
                float localLambert = max(dot(N, L), 0.0);
                float edge = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
                float attenuation = pow(edge, max(uLocalLightColorFalloff[i].w, 0.25));
                lit += albedo * uLocalLightColorFalloff[i].rgb * localLambert * attenuation;
            }
#else
            for (int i = 0; i < 8; ++i) {
                if (i >= uLocalLightCount) break;
                if (shared.localAttenuation[i] < 0.0) continue;
                float localLambert = max(dot(N, shared.localDirection[i]), 0.0);
                lit += albedo * uLocalLightColorFalloff[i].rgb *
                       localLambert * shared.localAttenuation[i];
            }
#endif
            // PC SLS2100.vso computes LAND fog per vertex from transformed
            // position, FogParam=(far, far-near, power, _), then SLS2124.pso
            // blends against the interpolated factor. Use the already matching
            // vertex result instead of recomputing a different world-distance fog.
            float fogFactor = vFogFactorQ1532;
            lit = mix(lit, uFogColor, fogFactor);
            return max(lit, vec3(0.0));
        }
        highp vec4 Q238Round(highp vec4 value) {
            if (uRoundMode == 2) return floor(clamp(value, 0.0, 1.0) * 255.0 + 0.5) / 255.0;
            return vec4(unpackHalf2x16(packHalf2x16(value.xy)),
                        unpackHalf2x16(packHalf2x16(value.zw)));
        }
        highp vec4 Q238Over(highp vec4 previous, vec3 lit, float alpha) {
            highp float a = clamp(alpha, 0.0, 1.0);
            // Fixed-point targets clamp fragment source values before blend.
            if (uRoundMode == 2) lit = clamp(lit, 0.0, 1.0);
            // Match both RGB and the original SRC_ALPHA alpha-channel blend.
            return Q238Round(vec4(vec3(lit) * a + previous.rgb * (1.0-a),
                                   a*a + previous.a * (1.0-a)));
        }
        void main() {
            if (uWaterReflectionClipEnabledQ2090 > 0.5 &&
                vPosition.y < uWaterReflectionPlaneYQ2090) discard;
            Q239SharedLighting shared = Q239PrepareSharedLighting();
            vec3 base = Q238ShadeLayer(uDiffuse, uNormalGloss, uUseTexture,
                                      uUseNormal, uTerrainSpecularEnabled, shared);
            if (uPaintCount == 0) {
                fragColor = vec4(base, clamp(vAlpha, 0.0, 1.0));
                return;
            }
            highp vec4 result = Q238Round(vec4(base, 1.0));
            if (uPaintCount > 0 && vPaintAlpha.x != 0.0) result = Q238Over(result,
                Q238ShadeLayer(uPaintDiffuse0, uPaintNormal0, 1, int(uPaintFlags0.x), uPaintFlags0.y, shared),
                vPaintAlpha.x);
            if (uPaintCount > 1 && vPaintAlpha.y != 0.0) result = Q238Over(result,
                Q238ShadeLayer(uPaintDiffuse1, uPaintNormal1, 1, int(uPaintFlags1.x), uPaintFlags1.y, shared),
                vPaintAlpha.y);
            if (uPaintCount > 2 && vPaintAlpha.z != 0.0) result = Q238Over(result,
                Q238ShadeLayer(uPaintDiffuse2, uPaintNormal2, 1, int(uPaintFlags2.x), uPaintFlags2.y, shared),
                vPaintAlpha.z);
            fragColor = result;
        }
    )";

    const bool q239Legacy = fo3terrain::LegacyLayers();
    const bool q239Unshared = !q239Legacy && fo3terrain::UnsharedLighting();
    std::string q239FragmentWithDefine;
    const char* q239Fragment = q239Legacy ? fo3terrain::kV237FragmentShader : fragmentSource;
    if (q239Unshared) {
        q239FragmentWithDefine = fragmentSource;
        const size_t version = q239FragmentWithDefine.find("#version");
        const size_t newline = version == std::string::npos ? std::string::npos
            : q239FragmentWithDefine.find('\n', version);
        if (newline != std::string::npos)
            q239FragmentWithDefine.insert(newline + 1, "#define FQ239_UNSHARED_LIGHTS 1\n");
        q239Fragment = q239FragmentWithDefine.c_str();
    }
    Q76B_LOGI("Q239 TERRAIN SHADER: legacyV237=%d unsharedLights=%d",
              q239Legacy ? 1 : 0, q239Unshared ? 1 : 0);
    GLuint vs = Q76BCompile(GL_VERTEX_SHADER,
                            q239Legacy ? fo3terrain::kV237VertexShader : vertexSource);
    GLuint fs = Q76BCompile(GL_FRAGMENT_SHADER, q239Fragment);
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
        Q76B_LOGE("Q7.20 terrain shader link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

Q76BV3 Q76BToVr(float gameX, float gameY, float gameZ,
                 float arrivalX, float arrivalY, float arrivalZ,
                 float sceneForward, float floorY, float unitsPerMetre) {
    return {
        (gameX - arrivalX) / unitsPerMetre,
        floorY + (gameZ - arrivalZ) / unitsPerMetre,
        sceneForward - (gameY - arrivalY) / unitsPerMetre,
    };
}

void Q1070ApplyTerrainAnisotropy() {
    static bool checked = false;
    static float level = 1.0f;
    static bool logged = false;
    if (!checked) {
        checked = true;
        const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (extensions && std::strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {
            constexpr GLenum GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT_Q1070 = 0x84FF;
            GLfloat driverMax = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT_Q1070, &driverMax);
            level = std::max(1.0f, std::min(4.0f, static_cast<float>(driverMax)));
        }
    }
    if (level > 1.0f) {
        constexpr GLenum GL_TEXTURE_MAX_ANISOTROPY_EXT_Q1070 = 0x84FE;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT_Q1070, level);
    }
    if (!logged) {
        logged = true;
        Q76B_LOGI("Q10.7 TERRAIN FILTER: anisotropy=%.1fx trilinear=1 source=Quest-GLES-extension", level);
    }
}

GLuint Q711UploadTexture(const std::string& path,
                          const Fo3RgbaTexture* predecoded = nullptr) {
    if (path.empty()) return 0;
    Fo3RgbaTexture fallback;
    if (!predecoded && !LoadFalloutTextureRgba(path, fallback)) {
        Q76B_LOGW("Q7.20 TERRAIN DDS MISS: path=%s", path.c_str());
        return 0;
    }
    const Fo3RgbaTexture& image = predecoded ? *predecoded : fallback;
    if (image.width <= 0 || image.height <= 0 || image.rgba.empty()) {
        Q76B_LOGW("Q7.20 TERRAIN DDS MISS: path=%s", path.c_str());
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Q1070ApplyTerrainAnisotropy();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 image.width, image.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        Q76B_LOGW("Q7.20 TERRAIN DDS GPU MISS: path=%s glError=0x%X", path.c_str(), error);
        if (texture) glDeleteTextures(1, &texture);
        return 0;
    }

    Q76B_LOGI("Q7.20 TERRAIN DDS READY: path=%s resolved=%s size=%dx%d format=%s",
              path.c_str(), image.sourcePath.c_str(), image.width, image.height,
              image.format.c_str());
    return texture;
}

int Q711QuadrantForQuad(int x, int y) {
    const int top = y >= 16 ? 2 : 0;
    const int right = x >= 16 ? 1 : 0;
    return top + right;
}

} // namespace

void SetFo3TerrainWaterReflectionClipQ2090(bool enabled, float planeY) {
    q2090TerrainReflectionClipEnabled = enabled;
    q2090TerrainReflectionPlaneY = planeY;
}
void SetFo3TerrainColorFormatQ238(bool halfFloat) { q238TerrainRoundMode=halfFloat?1:2; }

void ShutdownFo3TerrainRenderQ76() {
    // Q20 may own a worker or partially staged incoming strip. Stop that work
    // before deleting the legacy/cell-owned terrain GL objects.
    ResetFo3TerrainStreamingQ2000();
    for (Q711TerrainBatch& batch : q711TerrainBatches) {
        if (batch.vbo) glDeleteBuffers(1, &batch.vbo);
        if (batch.vao) glDeleteVertexArrays(1, &batch.vao);
        // Q16.19: texture is process-cache owned; recenter retires only window geometry.
        if (batch.normalTexture && batch.ownsNormal) glDeleteTextures(1, &batch.normalTexture);
        batch = {};
    }
    q711TerrainBatches.clear();
    Q229TouchTerrainBatches();
    if (q76bProgram) glDeleteProgram(q76bProgram);
    q76bProgram = 0;
    q76bMvpLocation = -1;
    q76bSamplerLocation = -1;
    q76bUseTextureLocation = -1;
    q1060TerrainNormalSamplerLocation = -1;
    q1060TerrainUseNormalLocation = -1;
    q1060TerrainSpecularLocation = -1;
    q1000TerrainAmbientLocation = -1;
    q1000TerrainSunlightLocation = -1;
    q1000TerrainSunDirectionLocation = -1;
    q1010TerrainEyeLocation = -1;
    q1010TerrainFogColorLocation = -1;
    q1010TerrainFogNearLocation = -1;
    q1010TerrainFogFarLocation = -1;
    q1410TerrainFogPowerLocation = -1;
    q1532TerrainFogNearVertexLocation = -1;
    q1532TerrainFogFarVertexLocation = -1;
    q1532TerrainFogPowerVertexLocation = -1;
    q1010TerrainLightCountLocation = -1;
    q1010TerrainLightPosRadiusLocation = -1;
    q1010TerrainLightColorFalloffLocation = -1;
    q1050TerrainLightMvpLocation = -1;
    q1050TerrainShadowMapLocation = -1;
    q1050TerrainShadowTexelLocation = -1;
    q1050TerrainShadowsEnabledLocation = -1;
    q1050TerrainShadowTexture = 0;
    q1050TerrainShadowEnabled = false;
    q76bReady = false;
    q76bLoggedVisible = false;
    q76bWorldspace = 0;
    q76bTerrainCells = 0;
    q76bTriangles = 0;
    q718AlphaTriangles = 0;
    q711RealTextureBatches = 0;
    q711FallbackBatches = 0;
    q713TextureUploadFailures = 0;
    q713TextureLimitHits = 0;
    q718AlphaBatches = 0;
    q718AlphaQuads = 0;
    q718AlphaLayersSeen = 0;
    q718AlphaLayersResolved = 0;
    q718AlphaLayersUnresolved = 0;
    q720HiddenBaseQuads = 0;
    q720HiddenAlphaLayers = 0;
}

bool InitializeFo3TerrainRenderQ76(uint32_t worldspaceFormId,
                                   float arrivalX, float arrivalY, float arrivalZ,
                                   float sceneForward, float floorY,
                                   float unitsPerMetre) {
    // Q22.2: independent phase timing includes original LAND, CPU geometry,
    // thread-parallel texture decode, and GL publication without hiding cells.
    auto q2220Last=fqloadprofile::NowUs();
    const uint64_t q2220Started=q2220Last;
    auto q2220Mark=[&](const char* stage,size_t count=0u){
        const uint64_t now=fqloadprofile::NowUs();
        fqloadprofile::Log("TERRAIN_INIT_STEP worldspace=%08X stage=%s stepMs=%.1f totalMs=%.1f count=%zu",
            worldspaceFormId,stage,(now-q2220Last)/1000.0,
            (now-q2220Started)/1000.0,count);
        q2220Last=now;
    };
    ShutdownFo3TerrainRenderQ76();
    q2220Mark("shutdown-previous-terrain");
    if (worldspaceFormId == 0u || unitsPerMetre <= 1e-4f) return false;

    if (!LoadFo3TerrainQ76(worldspaceFormId, arrivalX, arrivalY, arrivalZ)) {
        Q76B_LOGE("Q7.20 TERRAIN GPU FAILED: worldspace=%08X reason=LAND-load", worldspaceFormId);
        return false;
    }
    const std::vector<Fo3TerrainCellQ76>& terrain = GetFo3TerrainQ76();
    if (terrain.empty()) return false;
    q2220Mark("load-authored-land",terrain.size());

    std::vector<std::vector<float>> stitchedHeights;
    stitchedHeights.reserve(terrain.size());
    std::unordered_map<uint64_t, size_t> cellByGrid;
    for (size_t i = 0; i < terrain.size(); ++i) {
        stitchedHeights.push_back(terrain[i].heights);
        if (terrain[i].heights.size() == Q76B_HEIGHT_COUNT) {
            cellByGrid[Q711GridKey(terrain[i].gridX, terrain[i].gridY)] = i;
        }
    }

    size_t sharedEdges = 0u;
    size_t adjustedVertices = 0u;
    size_t preservedLargeVertices = 0u;
    size_t largeEdgeLogs = 0u;
    float maxSeamDelta = 0.0f;
    for (size_t i = 0; i < terrain.size(); ++i) {
        if (stitchedHeights[i].size() != Q76B_HEIGHT_COUNT) continue;
        const Fo3TerrainCellQ76& cell = terrain[i];

        const auto stitchEdge = [&](size_t j, bool east) {
            if (j >= terrain.size() || stitchedHeights[j].size() != Q76B_HEIGHT_COUNT) return;
            ++sharedEdges;
            float edgeMax = 0.0f;
            size_t edgePreserved = 0u;
            for (int p = 0; p < Q76B_LAND_SIDE; ++p) {
                const size_t a = east
                    ? static_cast<size_t>(p * Q76B_LAND_SIDE + 32)
                    : static_cast<size_t>(32 * Q76B_LAND_SIDE + p);
                const size_t b = east
                    ? static_cast<size_t>(p * Q76B_LAND_SIDE)
                    : static_cast<size_t>(p);
                const float delta = std::fabs(stitchedHeights[i][a] - stitchedHeights[j][b]);
                edgeMax = std::max(edgeMax, delta);
                maxSeamDelta = std::max(maxSeamDelta, delta);
                if (delta <= Q718_MAX_SEAM_STITCH_DELTA) {
                    if (delta > 0.001f) ++adjustedVertices;
                    const float average = (stitchedHeights[i][a] + stitchedHeights[j][b]) * 0.5f;
                    stitchedHeights[i][a] = average;
                    stitchedHeights[j][b] = average;
                } else {
                    ++edgePreserved;
                    ++preservedLargeVertices;
                }
            }
            if (edgePreserved > 0u && largeEdgeLogs < 20u) {
                ++largeEdgeLogs;
                Q76B_LOGW("Q7.20 LAND SEAM PRESERVE: grid=(%d,%d) neighbor=(%d,%d) dir=%s maxDelta=%.1f preservedVertices=%zu stitchLimit=%.1f",
                          cell.gridX, cell.gridY,
                          terrain[j].gridX, terrain[j].gridY,
                          east ? "E" : "N", edgeMax,
                          edgePreserved, Q718_MAX_SEAM_STITCH_DELTA);
            }
        };

        const auto eastIt = cellByGrid.find(Q711GridKey(cell.gridX + 1, cell.gridY));
        if (eastIt != cellByGrid.end()) stitchEdge(eastIt->second, true);
        const auto northIt = cellByGrid.find(Q711GridKey(cell.gridX, cell.gridY + 1));
        if (northIt != cellByGrid.end()) stitchEdge(northIt->second, false);
    }
    q2220Mark("stitch-authored-heights",sharedEdges);
    Q76B_LOGI("Q7.20 LAND SEAMS: cells=%zu sharedEdges=%zu adjustedVertices=%zu preservedLargeVertices=%zu maxHeightDelta=%.1f stitchLimit=%.1f",
              terrain.size(), sharedEdges, adjustedVertices,
              preservedLargeVertices, maxSeamDelta,
              Q718_MAX_SEAM_STITCH_DELTA);

    std::vector<Q711CpuBatch> cpuBatches;
    std::unordered_map<std::string, size_t> batchByTexture;
    int32_t q2000BatchGridX = 0;
    int32_t q2000BatchGridY = 0;
    auto getBatch = [&](const std::string& texturePath,
                        bool alphaLayer,
                        int layerOrder) -> Q711CpuBatch& {
        const std::string key =
            std::to_string(q2000BatchGridX) + "," +
            std::to_string(q2000BatchGridY) + "|" +
            (alphaLayer ? "A" : "B") +
            std::to_string(layerOrder) + "|" +
            (texturePath.empty() ? std::string("<fallback>") : texturePath);
        const auto found = batchByTexture.find(key);
        if (found != batchByTexture.end()) return cpuBatches[found->second];
        const size_t index = cpuBatches.size();
        batchByTexture.emplace(key, index);
        Q711CpuBatch batch;
        batch.gridX = q2000BatchGridX;
        batch.gridY = q2000BatchGridY;
        batch.texturePath = texturePath;
        batch.normalPath = ResolveFo3TerrainNormalTextureQ1060(texturePath);
        batch.specularEnabled = IsFo3TerrainSpecularEnabledQ1060(texturePath);
        batch.alphaLayer = alphaLayer;
        batch.layerOrder = layerOrder;
        cpuBatches.push_back(std::move(batch));
        return cpuBatches.back();
    };

    size_t acceptedCells = 0u;
    size_t texturedQuads = 0u;
    size_t fallbackQuads = 0u;
    for (size_t cellIndex = 0; cellIndex < terrain.size(); ++cellIndex) {
        const Fo3TerrainCellQ76& cell = terrain[cellIndex];
        if (stitchedHeights[cellIndex].size() != Q76B_HEIGHT_COUNT) continue;
        ++acceptedCells;
        q2000BatchGridX = cell.gridX;
        q2000BatchGridY = cell.gridY;
        const float originX = static_cast<float>(cell.gridX) * Q76B_CELL_SIZE;
        const float originY = static_cast<float>(cell.gridY) * Q76B_CELL_SIZE;

        auto gameHeight = [&](int x, int y) -> float {
            return stitchedHeights[cellIndex][static_cast<size_t>(y * Q76B_LAND_SIDE + x)];
        };
        auto point = [&](int x, int y) -> Q76BV3 {
            const float gameX = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
            const float gameY = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
            return Q76BToVr(gameX, gameY, gameHeight(x, y),
                            arrivalX, arrivalY, arrivalZ,
                            sceneForward, floorY, unitsPerMetre);
        };
        const bool hasAuthoredNormals = cell.normals.size() == Q76B_HEIGHT_COUNT * 3u;
        auto authoredNormal = [&](int x, int y) -> Q76BV3 {
            if (!hasAuthoredNormals) return {};
            const size_t index = static_cast<size_t>(y * Q76B_LAND_SIDE + x) * 3u;
            // Fallout LAND uses game X/Y/Z with Z up; renderer uses X/Y/-Z world axes.
            return Q76BNormalize({cell.normals[index + 0u],
                                  cell.normals[index + 2u],
                                 -cell.normals[index + 1u]});
        };

        const std::vector<float>& childColorsQ1260 = GetFo3TerrainVertexColorsQ1060(cell.landFormId);
        const bool hasChildColorsQ1260 = childColorsQ1260.size() == Q76B_HEIGHT_COUNT * 3u;
        const uint32_t repairedParentLandForVclrQ1260 = GetFo3MaterialParentLandQ1270(cell.landFormId);
        const std::vector<float>& parentColorsQ1260 =
            GetFo3TerrainVertexColorsQ1060(repairedParentLandForVclrQ1260);
        const bool hasParentColorsQ1260 = parentColorsQ1260.size() == Q76B_HEIGHT_COUNT * 3u;
        const std::vector<float>& authoredColorsQ1060 =
            hasChildColorsQ1260 ? childColorsQ1260 :
            (hasParentColorsQ1260 ? parentColorsQ1260 : childColorsQ1260);
        const bool hasAuthoredColorsQ1060 = authoredColorsQ1060.size() == Q76B_HEIGHT_COUNT * 3u;
        if (repairedParentLandForVclrQ1260 != 0u) {
            Q76B_LOGI("Q12.6 REPAIRED LAND VCLR: grid=(%d,%d) child=%d parent=%d source=%s",
                      cell.gridX, cell.gridY,
                      hasChildColorsQ1260 ? 1 : 0,
                      hasParentColorsQ1260 ? 1 : 0,
                      hasChildColorsQ1260 ? "child" :
                      (hasParentColorsQ1260 ? "parent" : "none"));
        }
        auto authoredColorQ1060 = [&](int x, int y) -> Q76BV3 {
            if (!hasAuthoredColorsQ1060) return {1.0f, 1.0f, 1.0f};
            const size_t index = static_cast<size_t>(y * Q76B_LAND_SIDE + x) * 3u;
            return {authoredColorsQ1060[index + 0u],
                    authoredColorsQ1060[index + 1u],
                    authoredColorsQ1060[index + 2u]};
        };

        std::string quadrantTextures[4];
        for (int quadrant = 0; quadrant < 4; ++quadrant) {
            quadrantTextures[quadrant] = ResolveFo3TerrainBaseTextureQ711(cell.landFormId, quadrant);
        }

        const uint32_t repairedParentLandQ1250 = GetFo3MaterialParentLandQ1270(cell.landFormId);
        if (repairedParentLandQ1250 != 0u) {
            uint32_t childBaseMaskQ1250 = 0u;
            uint32_t parentBaseMaskQ1250 = 0u;
            std::string parentQuadrantTexturesQ1250[4];
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                if (!quadrantTextures[quadrant].empty()) childBaseMaskQ1250 |= (1u << quadrant);
                parentQuadrantTexturesQ1250[quadrant] =
                    ResolveFo3TerrainBaseTextureQ711(repairedParentLandQ1250, quadrant);
                if (!parentQuadrantTexturesQ1250[quadrant].empty()) parentBaseMaskQ1250 |= (1u << quadrant);
            }

            const std::vector<Q718TerrainAlphaLayer>& childAlphaQ1250 =
                GetFo3TerrainAlphaLayersQ718(cell.landFormId);
            const std::vector<Q718TerrainAlphaLayer>& parentAlphaQ1250 =
                GetFo3TerrainAlphaLayersQ718(repairedParentLandQ1250);
            size_t childAlphaVerticesQ1250 = 0u;
            size_t parentAlphaVerticesQ1250 = 0u;
            for (const Q718TerrainAlphaLayer& layer : childAlphaQ1250) {
                childAlphaVerticesQ1250 += layer.authoredVertices;
            }
            for (const Q718TerrainAlphaLayer& layer : parentAlphaQ1250) {
                parentAlphaVerticesQ1250 += layer.authoredVertices;
            }

            Q76B_LOGI("Q12.5 REPAIRED LAND MATERIAL: grid=(%d,%d) childLAND=%08X parentLAND=%08X childBaseMask=0x%X parentBaseMask=0x%X childAlphaLayers=%zu parentAlphaLayers=%zu childAlphaVertices=%zu parentAlphaVertices=%zu",
                      cell.gridX, cell.gridY, cell.landFormId, repairedParentLandQ1250,
                      childBaseMaskQ1250, parentBaseMaskQ1250,
                      childAlphaQ1250.size(), parentAlphaQ1250.size(),
                      childAlphaVerticesQ1250, parentAlphaVerticesQ1250);
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                Q76B_LOGI("Q12.5 REPAIRED LAND QUAD: grid=(%d,%d) q=%d child=%s parent=%s",
                          cell.gridX, cell.gridY, quadrant,
                          quadrantTextures[quadrant].empty() ? "<none>" : quadrantTextures[quadrant].c_str(),
                          parentQuadrantTexturesQ1250[quadrant].empty() ? "<none>" : parentQuadrantTexturesQ1250[quadrant].c_str());
            }

            uint32_t inheritedBaseMaskQ1260 = 0u;
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                if (quadrantTextures[quadrant].empty() &&
                    !parentQuadrantTexturesQ1250[quadrant].empty()) {
                    quadrantTextures[quadrant] = parentQuadrantTexturesQ1250[quadrant];
                    inheritedBaseMaskQ1260 |= (1u << quadrant);
                }
            }
            Q76B_LOGI("Q12.6 REPAIRED LAND BASE: grid=(%d,%d) childLAND=%08X parentLAND=%08X inheritedMask=0x%X",
                      cell.gridX, cell.gridY, cell.landFormId,
                      repairedParentLandQ1250, inheritedBaseMaskQ1260);
        }

        for (int y = 0; y < Q76B_LAND_QUADS; ++y) {
            for (int x = 0; x < Q76B_LAND_QUADS; ++x) {
                const int quadrant = Q711QuadrantForQuad(x, y);
                if (IsFo3TerrainQuadrantHiddenQ720(cell.gridX, cell.gridY, quadrant)) {
                    ++q720HiddenBaseQuads;
                    continue;
                }
                Q711CpuBatch& batch = getBatch(quadrantTextures[quadrant], false, -1);
                if (quadrantTextures[quadrant].empty()) ++fallbackQuads;
                else ++texturedQuads;

                const float gameX0 = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
                const float gameY0 = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
                const float gameX1 = gameX0 + Q76B_VERTEX_SPACING;
                const float gameY1 = gameY0 + Q76B_VERTEX_SPACING;
                const float u0 = gameX0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float v0 = gameY0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float u1 = gameX1 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                const float v1 = gameY1 / Q711_TEXTURE_REPEAT_GAME_UNITS;

                const Q76BV3 p00 = point(x, y);
                const Q76BV3 p10 = point(x + 1, y);
                const Q76BV3 p01 = point(x, y + 1);
                const Q76BV3 p11 = point(x + 1, y + 1);
                Q718PushTriangle(batch.vertices, p00, u0, v0, p10, u1, v0, p11, u1, v1, 1.0f, 1.0f, 1.0f, authoredNormal(x, y), authoredNormal(x + 1, y), authoredNormal(x + 1, y + 1), authoredColorQ1060(x, y), authoredColorQ1060(x + 1, y), authoredColorQ1060(x + 1, y + 1));
                Q718PushTriangle(batch.vertices, p00, u0, v0, p11, u1, v1, p01, u0, v1, 1.0f, 1.0f, 1.0f, authoredNormal(x, y), authoredNormal(x + 1, y + 1), authoredNormal(x, y + 1), authoredColorQ1060(x, y), authoredColorQ1060(x + 1, y + 1), authoredColorQ1060(x, y + 1));
            }
        }

        const std::vector<Q718TerrainAlphaLayer>& childAlphaLayersQ1260 =
            GetFo3TerrainAlphaLayersQ718(cell.landFormId);
        std::vector<Q718TerrainAlphaLayer> mergedAlphaLayersQ1260;
        const std::vector<Q718TerrainAlphaLayer>* alphaLayersQ1260 = &childAlphaLayersQ1260;
        const uint32_t repairedParentLandForAlphaQ1260 = GetFo3MaterialParentLandQ1270(cell.landFormId);
        if (repairedParentLandForAlphaQ1260 != 0u) {
            const std::vector<Q718TerrainAlphaLayer>& parentAlphaLayersQ1260 =
                GetFo3TerrainAlphaLayersQ718(repairedParentLandForAlphaQ1260);
            mergedAlphaLayersQ1260.reserve(parentAlphaLayersQ1260.size() + childAlphaLayersQ1260.size());
            mergedAlphaLayersQ1260.insert(mergedAlphaLayersQ1260.end(),
                                          parentAlphaLayersQ1260.begin(),
                                          parentAlphaLayersQ1260.end());
            constexpr uint32_t Q1260_CHILD_LAYER_BIAS = 1024u;
            for (const Q718TerrainAlphaLayer& childLayerQ1260 : childAlphaLayersQ1260) {
                Q718TerrainAlphaLayer localLayerQ1260 = childLayerQ1260;
                const uint32_t biasedOrderQ1260 =
                    static_cast<uint32_t>(localLayerQ1260.layer) + Q1260_CHILD_LAYER_BIAS;
                localLayerQ1260.layer = static_cast<uint16_t>(
                    std::min<uint32_t>(biasedOrderQ1260, 65535u));
                mergedAlphaLayersQ1260.push_back(std::move(localLayerQ1260));
            }
            alphaLayersQ1260 = &mergedAlphaLayersQ1260;
            Q76B_LOGI("Q12.6 REPAIRED LAND ALPHA: grid=(%d,%d) childLAND=%08X parentLAND=%08X parentLayers=%zu childLayers=%zu mergedLayers=%zu childOrderBias=%u",
                      cell.gridX, cell.gridY, cell.landFormId,
                      repairedParentLandForAlphaQ1260,
                      parentAlphaLayersQ1260.size(), childAlphaLayersQ1260.size(),
                      mergedAlphaLayersQ1260.size(), Q1260_CHILD_LAYER_BIAS);
        }
        const std::vector<Q718TerrainAlphaLayer>& alphaLayers = *alphaLayersQ1260;
        for (const Q718TerrainAlphaLayer& layer : alphaLayers) {
            if (layer.quadrant > 3u || layer.authoredVertices == 0u) continue;
            if (IsFo3TerrainQuadrantHiddenQ720(cell.gridX, cell.gridY,
                                               static_cast<int>(layer.quadrant))) {
                ++q720HiddenAlphaLayers;
                continue;
            }
            ++q718AlphaLayersSeen;
            const std::string texturePath = ResolveFo3TerrainAlphaTextureQ718(layer);
            if (texturePath.empty()) {
                ++q718AlphaLayersUnresolved;
                continue;
            }
            ++q718AlphaLayersResolved;

            const int xStart = (layer.quadrant & 1u) ? 16 : 0;
            const int yStart = (layer.quadrant & 2u) ? 16 : 0;
            for (int localY = 0; localY < 16; ++localY) {
                for (int localX = 0; localX < 16; ++localX) {
                    const size_t i00 = static_cast<size_t>(localY * 17 + localX);
                    const size_t i10 = i00 + 1u;
                    const size_t i01 = i00 + 17u;
                    const size_t i11 = i01 + 1u;
                    const float a00 = layer.opacity[i00];
                    const float a10 = layer.opacity[i10];
                    const float a01 = layer.opacity[i01];
                    const float a11 = layer.opacity[i11];
                    if (std::max({a00, a10, a01, a11}) <= Q718_ALPHA_EPS) continue;

                    const int x = xStart + localX;
                    const int y = yStart + localY;
                    const float gameX0 = originX + static_cast<float>(x) * Q76B_VERTEX_SPACING;
                    const float gameY0 = originY + static_cast<float>(y) * Q76B_VERTEX_SPACING;
                    const float gameX1 = gameX0 + Q76B_VERTEX_SPACING;
                    const float gameY1 = gameY0 + Q76B_VERTEX_SPACING;
                    const float u0 = gameX0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                    const float v0 = gameY0 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                    const float u1 = gameX1 / Q711_TEXTURE_REPEAT_GAME_UNITS;
                    const float v1 = gameY1 / Q711_TEXTURE_REPEAT_GAME_UNITS;

                    const Q76BV3 p00 = point(x, y);
                    const Q76BV3 p10 = point(x + 1, y);
                    const Q76BV3 p01 = point(x, y + 1);
                    const Q76BV3 p11 = point(x + 1, y + 1);
                    Q711CpuBatch& batch = getBatch(texturePath, true, static_cast<int>(layer.layer));
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p10, u1, v0, p11, u1, v1,
                                     a00, a10, a11,
                                     authoredNormal(x, y), authoredNormal(x + 1, y),
                                     authoredNormal(x + 1, y + 1),
                                     authoredColorQ1060(x, y), authoredColorQ1060(x + 1, y),
                                     authoredColorQ1060(x + 1, y + 1));
                    Q718PushTriangle(batch.vertices,
                                     p00, u0, v0, p11, u1, v1, p01, u0, v1,
                                     a00, a11, a01,
                                     authoredNormal(x, y), authoredNormal(x + 1, y + 1),
                                     authoredNormal(x, y + 1),
                                     authoredColorQ1060(x, y), authoredColorQ1060(x + 1, y + 1),
                                     authoredColorQ1060(x, y + 1));
                    ++q718AlphaQuads;
                }
            }
        }
    }

    if (acceptedCells == 0u || cpuBatches.empty()) return false;

    q2220Mark("build-authored-geometry",cpuBatches.size());
    std::stable_sort(cpuBatches.begin(), cpuBatches.end(),
                     [](const Q711CpuBatch& a, const Q711CpuBatch& b) {
        if (a.alphaLayer != b.alphaLayer) return !a.alphaLayer;
        if (a.alphaLayer && a.layerOrder != b.layerOrder) return a.layerOrder < b.layerOrder;
        return false;
    });

    // Predecode immutable Fallout DDS assets on three CPU threads, not in
    // serial between GL uploads. The bounded result cache lives only for this
    // admission; absent/oversize images use the unchanged synchronous fallback.
    constexpr size_t Q2220_MAX_STAGED_DDS=96u;
    constexpr size_t Q2220_MAX_STAGED_BYTES=96u*1024u*1024u;
    std::vector<std::string> q2220Paths;
    std::unordered_set<std::string> q2220Seen;
    q2220Paths.reserve(96u);
    for(const Q711CpuBatch& batch:cpuBatches){
        for(const std::string* path : {&batch.texturePath,&batch.normalPath}){
            if(path->empty() || !q2220Seen.insert(*path).second)continue;
            if(q1910TerrainTextureCache.find(*path)!=q1910TerrainTextureCache.end() ||
               q1810TerrainNormalTextureCache.find(*path)!=q1810TerrainNormalTextureCache.end())continue;
            if(q2220Paths.size()>=Q2220_MAX_STAGED_DDS)break;
            q2220Paths.push_back(*path);
        }
    }
    std::vector<Fo3RgbaTexture> q2220Images(q2220Paths.size());
    std::atomic<size_t> q2220Next{0u},q2220DecodedBytes{0u};
    auto q2220Decode=[&](){
        while(true){
            const size_t i=q2220Next.fetch_add(1u,std::memory_order_relaxed);
            if(i>=q2220Paths.size())return;
            Fo3RgbaTexture image;
            if(!LoadFalloutTextureRgba(q2220Paths[i],image) ||
               image.width<=0 || image.height<=0 || image.rgba.empty())continue;
            const size_t bytes=image.rgba.size();
            const size_t reserved=q2220DecodedBytes.fetch_add(bytes,std::memory_order_relaxed);
            if(bytes>Q2220_MAX_STAGED_BYTES ||
               reserved>Q2220_MAX_STAGED_BYTES-bytes){
                q2220DecodedBytes.fetch_sub(bytes,std::memory_order_relaxed);
                continue;
            }
            q2220Images[i]=std::move(image); // Per-slot writes; no shared vector mutation.
        }
    };
    std::vector<std::thread> q2220Workers;
    try {
        for(size_t i=0u;i<2u && i<q2220Paths.size();++i)
            q2220Workers.emplace_back(q2220Decode);
    } catch (...) {
        // Worker launch is opportunistic, never a reason to lose LAND.
    }
    q2220Decode();
    for(auto& worker:q2220Workers)if(worker.joinable())worker.join();
    std::unordered_map<std::string,Fo3RgbaTexture> q2220Predecoded;
    q2220Predecoded.reserve(q2220Paths.size());
    for(size_t i=0u;i<q2220Paths.size();++i){
        if(q2220Images[i].rgba.empty())continue;
        q2220Predecoded.emplace(q2220Paths[i],std::move(q2220Images[i]));
    }
    q2220Mark("parallel-dds-decode",q2220Predecoded.size());

    q76bProgram = Q76BCreateProgram();
    if (!q76bProgram) {
        Q76B_LOGE("Q10.7 TERRAIN FAILED: stage=shader-program");
        return false;
    }
    q76bMvpLocation = glGetUniformLocation(q76bProgram, "uMvp");
    q76bSamplerLocation = glGetUniformLocation(q76bProgram, "uDiffuse");
    q238PaintCountLocation=glGetUniformLocation(q76bProgram,"uPaintCount");
    q238RoundModeLocation=glGetUniformLocation(q76bProgram,"uRoundMode");
    for(int i=0;i<3;++i){
        const std::string suffix=std::to_string(i);
        q238PaintDiffuseLocation[i]=glGetUniformLocation(q76bProgram,("uPaintDiffuse"+suffix).c_str());
        q238PaintNormalLocation[i]=glGetUniformLocation(q76bProgram,("uPaintNormal"+suffix).c_str());
        q238PaintFlagsLocation[i]=glGetUniformLocation(q76bProgram,("uPaintFlags"+suffix).c_str());
    }
    q76bUseTextureLocation = glGetUniformLocation(q76bProgram, "uUseTexture");
    q1060TerrainNormalSamplerLocation = glGetUniformLocation(q76bProgram, "uNormalGloss");
    q1060TerrainUseNormalLocation = glGetUniformLocation(q76bProgram, "uUseNormal");
    q1060TerrainSpecularLocation = glGetUniformLocation(q76bProgram, "uTerrainSpecularEnabled");
    q1000TerrainAmbientLocation = glGetUniformLocation(q76bProgram, "uAmbientColor");
    q1000TerrainSunlightLocation = glGetUniformLocation(q76bProgram, "uSunlightColor");
    q1000TerrainSunDirectionLocation = glGetUniformLocation(q76bProgram, "uSunDirection");
    q1010TerrainEyeLocation = glGetUniformLocation(q76bProgram, "uEyePosition");
    q1010TerrainFogColorLocation = glGetUniformLocation(q76bProgram, "uFogColor");
    q1010TerrainFogNearLocation = glGetUniformLocation(q76bProgram, "uFogNear");
    q1010TerrainFogFarLocation = glGetUniformLocation(q76bProgram, "uFogFar");
    q1410TerrainFogPowerLocation = glGetUniformLocation(q76bProgram, "uFogPower");
    q1532TerrainFogNearVertexLocation = glGetUniformLocation(q76bProgram, "uFogNearVertexQ1532");
    q1532TerrainFogFarVertexLocation = glGetUniformLocation(q76bProgram, "uFogFarVertexQ1532");
    q1532TerrainFogPowerVertexLocation = glGetUniformLocation(q76bProgram, "uFogPowerVertexQ1532");
    q1010TerrainLightCountLocation = glGetUniformLocation(q76bProgram, "uLocalLightCount");
    q1010TerrainLightPosRadiusLocation = glGetUniformLocation(q76bProgram, "uLocalLightPosRadius[0]");
    q1010TerrainLightColorFalloffLocation = glGetUniformLocation(q76bProgram, "uLocalLightColorFalloff[0]");
    q1050TerrainLightMvpLocation = glGetUniformLocation(q76bProgram, "uLightMvp");
    q1050TerrainShadowMapLocation = glGetUniformLocation(q76bProgram, "uShadowMap");
    q1050TerrainShadowTexelLocation = glGetUniformLocation(q76bProgram, "uShadowTexelSize");
    q1050TerrainShadowsEnabledLocation = glGetUniformLocation(q76bProgram, "uShadowsEnabled");
    q2090TerrainReflectionClipEnabledLocation =
        glGetUniformLocation(q76bProgram, "uWaterReflectionClipEnabledQ2090");
    q2090TerrainReflectionPlaneYLocation =
        glGetUniformLocation(q76bProgram, "uWaterReflectionPlaneYQ2090");
    if (q76bMvpLocation < 0 || q76bSamplerLocation < 0 || q76bUseTextureLocation < 0 || q1000TerrainAmbientLocation < 0 || q1000TerrainSunlightLocation < 0 || q1000TerrainSunDirectionLocation < 0) {
        ShutdownFo3TerrainRenderQ76();
        return false;
    }

    q2220Mark("compile-terrain-shader");
    size_t textureUploads = 0u;
    size_t baseVertices = 0u;
    size_t alphaVertices = 0u;
    auto& uploadedByPath = q1910TerrainTextureCache;
    auto& uploadedNormalsQ1060 = q1810TerrainNormalTextureCache;
    size_t q1060NormalUploads = 0u;
    size_t q1810NormalCacheHits = 0u;
    size_t q1060NormalMisses = 0u;
    for (size_t cpuIndex = 0; cpuIndex < cpuBatches.size(); ++cpuIndex) {
        Q711CpuBatch& cpu = cpuBatches[cpuIndex];
        if (cpu.vertices.empty()) continue;
        Q711TerrainBatch gpu;
        gpu.gridX = cpu.gridX;
        gpu.gridY = cpu.gridY;
        gpu.texturePath = cpu.texturePath;
        gpu.normalPath = cpu.normalPath;
        gpu.specularEnabled = cpu.specularEnabled;
        gpu.alphaLayer = cpu.alphaLayer;
        gpu.layerOrder = cpu.layerOrder;

        static_cast<fo3terrain::Bounds&>(gpu)=fo3terrain::BuildBounds(cpu.vertices.data(),cpu.vertices.size());

        if (!cpu.texturePath.empty()) {
            const auto cached = uploadedByPath.find(cpu.texturePath);
            if (cached != uploadedByPath.end()) {
                gpu.texture = cached->second;
                gpu.realTexture = gpu.texture != 0u;
            } else if (textureUploads < Q713_MAX_REAL_TEXTURES &&
                       q1910TerrainTextureCache.size() < Q1910_TERRAIN_TEXTURE_CACHE_LIMIT) {
                auto prepared=q2220Predecoded.find(cpu.texturePath);
                gpu.texture = Q711UploadTexture(cpu.texturePath,
                    prepared!=q2220Predecoded.end() ? &prepared->second : nullptr);
                gpu.realTexture = gpu.texture != 0u;
                if (gpu.realTexture) {
                    gpu.ownsTexture = false; // Q16.19 shared process-lifetime terrain texture cache
                    uploadedByPath[cpu.texturePath] = gpu.texture;
                    ++textureUploads;
                } else {
                    ++q713TextureUploadFailures;
                }
            } else {
                ++q713TextureLimitHits;
                if (q713TextureLimitHits <= 8u) {
                    Q76B_LOGW("Q7.20 TERRAIN TEXTURE LIMIT: path=%s successfulUploads=%zu cap=%zu",
                              cpu.texturePath.c_str(), textureUploads, Q713_MAX_REAL_TEXTURES);
                }
            }
        }

        if (!cpu.normalPath.empty()) {
            const auto cachedNormal = uploadedNormalsQ1060.find(cpu.normalPath);
            if (cachedNormal != uploadedNormalsQ1060.end()) {
                gpu.normalTexture = cachedNormal->second;
                gpu.realNormal = gpu.normalTexture != 0u;
                if (gpu.realNormal) ++q1810NormalCacheHits;
            } else if (q1060NormalUploads < Q713_MAX_REAL_TEXTURES &&
                       uploadedNormalsQ1060.size() < Q1810_TERRAIN_NORMAL_CACHE_LIMIT) {
                auto prepared=q2220Predecoded.find(cpu.normalPath);
                gpu.normalTexture = Q711UploadTexture(cpu.normalPath,
                    prepared!=q2220Predecoded.end() ? &prepared->second : nullptr);
                gpu.realNormal = gpu.normalTexture != 0u;
                if (gpu.realNormal) {
                    // Q18.1: normals are immutable Fallout assets just like the
                    // diffuse LAND textures. Keep them process-lifetime so a
                    // 7x7 recenter does not reopen/decode the same DDS files.
                    gpu.ownsNormal = false;
                    uploadedNormalsQ1060[cpu.normalPath] = gpu.normalTexture;
                    ++q1060NormalUploads;
                } else {
                    ++q1060NormalMisses;
                }
            } else {
                ++q1060NormalMisses;
            }
        }

        // Never draw an unresolved alpha layer as the brown fallback. Base LAND
        // still uses the fallback so there is always opaque geometry under paint.
        if (gpu.alphaLayer && !gpu.realTexture) continue;
        if (!gpu.alphaLayer && !gpu.realTexture) ++q711FallbackBatches;
        if (gpu.realTexture) ++q711RealTextureBatches;
        if (gpu.alphaLayer) ++q718AlphaBatches;

    }
    const size_t q237Covered = fo3terrain::RemoveCoveredPaint(cpuBatches, [&](const Q711CpuBatch& batch) {
        const auto texture = uploadedByPath.find(batch.texturePath);
        return texture != uploadedByPath.end() && texture->second != 0u;
    });
    fo3terrain::FusionStats q238Fusion;
    if(!fo3terrain::LegacyLayers())q238Fusion=fo3terrain::FuseLayers(cpuBatches,[&](const Q711CpuBatch& batch){
        const auto texture=uploadedByPath.find(batch.texturePath);
        return texture!=uploadedByPath.end()&&texture->second!=0u;
    });
    Q76B_LOGI("Q238 TERRAIN FUSION: path=cold baseTriangles=%zu foldedPaintTriangles=%zu groups=%zu fallbackStacks=%zu legacy=%d",
              q238Fusion.triangles,q238Fusion.paintTriangles,q238Fusion.groups,q238Fusion.fallback,fo3terrain::LegacyLayers());
    size_t q237Corners = 0u, q237Unique = 0u, q237IndexedBatches = 0u;
    q711RealTextureBatches=q711FallbackBatches=q718AlphaBatches=0u;
    for (Q711CpuBatch& cpu : cpuBatches) {
        if (cpu.vertices.empty()) continue;
        Q711TerrainBatch gpu=Q238GpuMaterial(cpu);
        if(gpu.alphaLayer&&!gpu.realTexture)continue;
        if(gpu.realTexture)++q711RealTextureBatches;else if(!gpu.alphaLayer)++q711FallbackBatches;
        if(gpu.alphaLayer)++q718AlphaBatches;

        glGenVertexArrays(1, &gpu.vao);
        glBindVertexArray(gpu.vao);
        glGenBuffers(1, &gpu.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
        Q237PrepareTerrainIndices(cpu);
        const size_t vertexBytes = cpu.vertices.size() * sizeof(float);
        const size_t indexBytes = cpu.indices.size() * sizeof(uint32_t);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertexBytes + indexBytes),
                     nullptr, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertexBytes), cpu.vertices.data());
        if (indexBytes) {
            gpu.indexOffset = vertexBytes;
            glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(vertexBytes),
                            static_cast<GLsizeiptr>(indexBytes), cpu.indices.data());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.vbo);
        }
        const GLsizei stride = static_cast<GLsizei>(cpu.vertexFloats * sizeof(float));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(8 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(9 * sizeof(float)));
        glEnableVertexAttribArray(4);
        if(cpu.vertexFloats==15u){
            glVertexAttribPointer(5,3,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<const void*>(12u*sizeof(float)));
            glEnableVertexAttribArray(5);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        gpu.vertexCount = static_cast<GLsizei>(indexBytes ? cpu.indices.size() : cpu.vertices.size() / cpu.vertexFloats);
        q237Corners += static_cast<size_t>(gpu.vertexCount);
        q237Unique += cpu.vertices.size() / cpu.vertexFloats;
        q237IndexedBatches += indexBytes != 0u;
        if (gpu.alphaLayer) alphaVertices += static_cast<size_t>(gpu.vertexCount);
        else baseVertices += static_cast<size_t>(gpu.vertexCount);
        q711TerrainBatches.push_back(std::move(gpu));
        Q229TouchTerrainBatches();
    }
    Q76B_LOGI("Q237 TERRAIN INDEX: path=cold indexedBatches=%zu drawCorners=%zu uniqueVertices=%zu coveredPaintTriangles=%zu baseDepth=retained",
              q237IndexedBatches, q237Corners, q237Unique, q237Covered);

    q2220Mark("gpu-textures-and-geometry",q711TerrainBatches.size());
    // All decoded pixels are now in GL-owned resources. Drop the temporary CPU
    // staging memory before terrain becomes playable.
    q2220Predecoded.clear();
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR || q711TerrainBatches.empty()) {
        Q76B_LOGE("Q10.7 TERRAIN FAILED: stage=gpu-upload worldspace=%08X glError=0x%X batches=%zu", worldspaceFormId, error, q711TerrainBatches.size());
        Q76B_LOGE("Q7.20 TERRAIN GPU FAILED: worldspace=%08X glError=0x%X batches=%zu",
                  worldspaceFormId, error, q711TerrainBatches.size());
        ShutdownFo3TerrainRenderQ76();
        return false;
    }

    q76bTriangles = baseVertices / 3u;
    q718AlphaTriangles = alphaVertices / 3u;
    q76bTerrainCells = acceptedCells;
    q76bWorldspace = worldspaceFormId;
    q76bReady = true;
    size_t q1070VclrCells = 0u;
    for (const Fo3TerrainCellQ76& c : terrain) {
        if (GetFo3TerrainVertexColorsQ1060(c.landFormId).size() == Q76B_HEIGHT_COUNT * 3u) ++q1070VclrCells;
    }
    Q76B_LOGI("Q18.1 TERRAIN MATERIAL ACTIVE: cells=%zu VCLR=%zu normalUploads=%zu normalCacheHits=%zu normalCacheEntries=%zu normalMisses=%zu strideFloats=12 TX01=1",
              acceptedCells, q1070VclrCells, q1060NormalUploads,
              q1810NormalCacheHits, uploadedNormalsQ1060.size(),
              q1060NormalMisses);
    Q76B_LOGI("Q7.20 LAND HIDE APPLIED: worldspace=%08X hiddenBaseQuads=%zu hiddenAlphaLayers=%zu", q76bWorldspace, q720HiddenBaseQuads, q720HiddenAlphaLayers);
    q2220Mark("finalise-renderer",q711TerrainBatches.size());
    Q76B_LOGI("Q7.20 TERRAIN GPU READY: worldspace=%08X cells=%zu baseTriangles=%zu alphaTriangles=%zu batches=%zu alphaBatches=%zu uniqueTextureUploads=%zu realTextureBatches=%zu fallbackBatches=%zu texturedQuads=%zu fallbackQuads=%zu alphaLayersSeen=%zu alphaLayersResolved=%zu alphaLayersUnresolved=%zu alphaQuads=%zu uploadFailures=%zu limitHits=%zu alphaLayers=1 seamGuard=1",
              q76bWorldspace, q76bTerrainCells, q76bTriangles, q718AlphaTriangles,
              q711TerrainBatches.size(), q718AlphaBatches, textureUploads,
              q711RealTextureBatches, q711FallbackBatches,
              texturedQuads, fallbackQuads,
              q718AlphaLayersSeen, q718AlphaLayersResolved,
              q718AlphaLayersUnresolved, q718AlphaQuads,
              q713TextureUploadFailures, q713TextureLimitHits);
    return true;
}

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
        Q237DrawTerrainBatch(batch);
    }
}

void RenderFo3TerrainQ76(const float* mvp) {
    if (!q76bReady || !mvp || !q76bProgram || q711TerrainBatches.empty()) return;

    const auto q2018TerrainStarted = std::chrono::steady_clock::now();
    // Q231 uses actual homogeneous shader clipping and complete publication bounds.
    // The old Q20.18 plane-extraction path remains unused.
    Q229PrepareTerrainDrawLists();
    size_t q2018BaseTested = 0u;
    size_t q2018BaseDrawn = 0u;
    size_t q228MirrorUnderwaterSkipped = 0u;
    size_t q2018AlphaTested = 0u;
    size_t q2018AlphaDrawn = 0u;

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousActiveTexture = 0;
    GLint previousTexture0 = 0;
    GLint previousTexture1 = 0;
    GLint previousTexture3 = 0;
    GLint previousDepthFunc = GL_LESS;
    GLint previousBlendSrcRgb = GL_ONE;
    GLint previousBlendDstRgb = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE;
    GLint previousBlendDstAlpha = GL_ZERO;
    GLboolean previousDepthMask = GL_TRUE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture3);
    glActiveTexture(GL_TEXTURE0);
    constexpr unsigned q238Units[6]={2,4,5,6,7,8};
    std::array<GLint,6> q238PreviousTextures{};
    for(size_t i=0;i<6u;++i){glActiveTexture(GL_TEXTURE0+q238Units[i]);glGetIntegerv(GL_TEXTURE_BINDING_2D,&q238PreviousTextures[i]);}
    glActiveTexture(GL_TEXTURE0);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glUseProgram(q76bProgram);
    glUniform1i(q238RoundModeLocation,q238TerrainRoundMode);
    glUniformMatrix4fv(q76bMvpLocation, 1, GL_FALSE, mvp);
    if (q2090TerrainReflectionClipEnabledLocation >= 0) {
        glUniform1f(q2090TerrainReflectionClipEnabledLocation,
                    q2090TerrainReflectionClipEnabled ? 1.0f : 0.0f);
    }
    if (q2090TerrainReflectionPlaneYLocation >= 0) {
        glUniform1f(q2090TerrainReflectionPlaneYLocation,
                    q2090TerrainReflectionPlaneY);
    }
    glUniform1i(q76bSamplerLocation, 0);
    if (q1060TerrainNormalSamplerLocation >= 0) glUniform1i(q1060TerrainNormalSamplerLocation, 1);
    const Fo3Environment& q1000Env = GetFo3RenderEnvironment();
    float q203dPcLightDirection[3]{0.35f, 0.85f, 0.40f};
    const bool q203dPcLightReady =
        GetFo3PcLightDirection(q203dPcLightDirection);

    // Q20.3 PC LAND parity: the captured SLS2124 constants prove that WTHR
    // AmbientColor and PSLightColor chroma are supplied in raw normalized byte
    // space, not after sRGB->linear conversion. Recover only the scalar already
    // applied by Q14 from its linear sunlight result; this deliberately changes
    // colour-domain interpretation without inventing a new intensity curve.
    float q2030RawAmbient[3]{0.48f, 0.48f, 0.48f};
    float q2030RawSunlight[3]{0.52f, 0.52f, 0.52f};
    float q2030LinearSunlight[3]{0.52f, 0.52f, 0.52f};
    float q2030RawFog[3]{0.0f, 0.0f, 0.0f};
    RefreshFo3RawWeatherLighting();
    const bool q2030RawLightingReady =
        GetFo3RawWeatherLighting(q2030RawAmbient, q2030RawSunlight) &&
        GetFo3LinearWeatherSunlight(q2030LinearSunlight);
    const bool q2030RawFogReady = GetFo3RawWeatherFog(q2030RawFog);

    float q2030SunScale = 1.0f;
    if (q1000Env.valid && q2030RawLightingReady) {
        float numerator = 0.0f;
        float denominator = 0.0f;
        for (int channel = 0; channel < 3; ++channel) {
            numerator += q1000Env.sunlight[channel] * q2030LinearSunlight[channel];
            denominator += q2030LinearSunlight[channel] * q2030LinearSunlight[channel];
        }
        if (denominator > 1.0e-8f) {
            q2030SunScale = std::clamp(numerator / denominator, 0.0f, 8.0f);
        }
        float q2030PcSunlight[3]{
            q2030RawSunlight[0] * q2030SunScale,
            q2030RawSunlight[1] * q2030SunScale,
            q2030RawSunlight[2] * q2030SunScale,
        };
        glUniform3fv(q1000TerrainAmbientLocation, 1, q2030RawAmbient);
        glUniform3fv(q1000TerrainSunlightLocation, 1, q2030PcSunlight);
        glUniform3fv(q1000TerrainSunDirectionLocation, 1,
                     q203dPcLightReady ? q203dPcLightDirection : q1000Env.sunDirection);
    } else if (q1000Env.valid) {
        glUniform3fv(q1000TerrainAmbientLocation, 1, q1000Env.ambient);
        glUniform3fv(q1000TerrainSunlightLocation, 1, q1000Env.sunlight);
        glUniform3fv(q1000TerrainSunDirectionLocation, 1,
                     q203dPcLightReady ? q203dPcLightDirection : q1000Env.sunDirection);
    } else {
        glUniform3f(q1000TerrainAmbientLocation, 0.48f, 0.48f, 0.48f);
        glUniform3f(q1000TerrainSunlightLocation, 0.52f, 0.52f, 0.52f);
        glUniform3f(q1000TerrainSunDirectionLocation, 0.35f, 0.85f, 0.40f);
    }

    static bool q2030PcLandLogged = false;
    if (!q2030PcLandLogged && q1000Env.valid && q2030RawLightingReady) {
        q2030PcLandLogged = true;
        Q76B_LOGI(
            "Q20.3 PC LAND CORE: ambientRaw=(%.6f %.6f %.6f) sunlightRaw=(%.6f %.6f %.6f) recoveredSunScale=%.6f fogRaw=(%.6f %.6f %.6f) shader=SLS2100+SLS2124 fog=vertex-PC direction=PC_CAPTURE_Q20.3D textureSRGB=unchanged",
            q2030RawAmbient[0], q2030RawAmbient[1], q2030RawAmbient[2],
            q2030RawSunlight[0], q2030RawSunlight[1], q2030RawSunlight[2],
            q2030SunScale,
            q2030RawFog[0], q2030RawFog[1], q2030RawFog[2]);
    }

    glUniform3fv(q1010TerrainEyeLocation, 1, gFo3EyePosition);
    glUniform1i(q1010TerrainLightCountLocation, gFo3SelectedLightCount);
    glUniform4fv(q1010TerrainLightPosRadiusLocation, FO3_SHADER_LIGHTS,
                 gFo3SelectedLightPosRadius);
    glUniform4fv(q1010TerrainLightColorFalloffLocation, FO3_SHADER_LIGHTS,
                 gFo3SelectedLightColorFalloff);
    if (q1410TerrainFogPowerLocation >= 0) glUniform1f(q1410TerrainFogPowerLocation, GetFo3FogPower());
    const float q1532TerrainFogNear =
        (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f)
            ? std::max(0.0f, q1000Env.fogNear / 70.0f)
            : 10000.0f;
    const float q1532TerrainFogFar =
        (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f)
            ? std::max(0.1f, q1000Env.fogFar / 70.0f)
            : 10001.0f;
    if (q1532TerrainFogNearVertexLocation >= 0)
        glUniform1f(q1532TerrainFogNearVertexLocation, q1532TerrainFogNear);
    if (q1532TerrainFogFarVertexLocation >= 0)
        glUniform1f(q1532TerrainFogFarVertexLocation, q1532TerrainFogFar);
    if (q1532TerrainFogPowerVertexLocation >= 0)
        glUniform1f(q1532TerrainFogPowerVertexLocation, GetFo3FogPower());
    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {
        if (q2030RawFogReady) {
            glUniform3fv(q1010TerrainFogColorLocation, 1, q2030RawFog);
        } else {
            glUniform3fv(q1010TerrainFogColorLocation, 1, q1000Env.fog);
        }
        glUniform1f(q1010TerrainFogNearLocation, std::max(0.0f, q1000Env.fogNear / 70.0f));
        glUniform1f(q1010TerrainFogFarLocation, std::max(0.1f, q1000Env.fogFar / 70.0f));
    } else {
        glUniform3f(q1010TerrainFogColorLocation, 0.0f, 0.0f, 0.0f);
        glUniform1f(q1010TerrainFogNearLocation, 10000.0f);
        glUniform1f(q1010TerrainFogFarLocation, 10001.0f);
    }

    if (q1050TerrainLightMvpLocation >= 0) glUniformMatrix4fv(q1050TerrainLightMvpLocation, 1, GL_FALSE, q1050TerrainLightMvp);
    if (q1050TerrainShadowMapLocation >= 0) glUniform1i(q1050TerrainShadowMapLocation, 3);
    if (q1050TerrainShadowTexelLocation >= 0) glUniform2f(q1050TerrainShadowTexelLocation, 1.0f / 1024.0f, 1.0f / 1024.0f);
    if (q1050TerrainShadowsEnabledLocation >= 0) glUniform1f(q1050TerrainShadowsEnabledLocation, 0.0f);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, q1050TerrainShadowTexture);
    glActiveTexture(GL_TEXTURE0);
    for (const Q711TerrainBatch* batchPtr : q229TerrainBaseBatches) {
        const Q711TerrainBatch& batch=*batchPtr;
        ++q2018BaseTested;
        if (Q228EntireTerrainBatchBelowReflectionPlane(batch)) {
            ++q228MirrorUnderwaterSkipped;
            continue;
        }
        if(!fo3terrain::MayIntersectClip(batch,mvp))continue;
        ++q2018BaseDrawn;
        glUniform1i(q76bUseTextureLocation, batch.realTexture ? 1 : 0);
        if (q1060TerrainUseNormalLocation >= 0) glUniform1i(q1060TerrainUseNormalLocation, batch.realNormal ? 1 : 0);
        if (q1060TerrainSpecularLocation >= 0) glUniform1f(q1060TerrainSpecularLocation, batch.specularEnabled ? 1.0f : 0.0f);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, batch.realNormal ? batch.normalTexture : 0u);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.realTexture ? batch.texture : 0u);
        glBindVertexArray(batch.vao);
        Q238BindPaint(batch);
        Q237DrawTerrainBatch(batch);
    }

    // Painted ATXT/VTXT layers lie exactly on the base LAND mesh. LEQUAL lets
    // the overlay pass the base depth value; disabling depth writes keeps later
    // authored paint layers compositing in layer order without z-fighting.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    for (const Q711TerrainBatch* batchPtr : q229TerrainPaintBatches) {
        const Q711TerrainBatch& batch=*batchPtr;
        ++q2018AlphaTested;
        if (Q228EntireTerrainBatchBelowReflectionPlane(batch)) {
            ++q228MirrorUnderwaterSkipped;
            continue;
        }
        if(!fo3terrain::MayIntersectClip(batch,mvp))continue;
        ++q2018AlphaDrawn;
        glUniform1i(q76bUseTextureLocation, 1);
        if (q1060TerrainUseNormalLocation >= 0) glUniform1i(q1060TerrainUseNormalLocation, batch.realNormal ? 1 : 0);
        if (q1060TerrainSpecularLocation >= 0) glUniform1f(q1060TerrainSpecularLocation, batch.specularEnabled ? 1.0f : 0.0f);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, batch.realNormal ? batch.normalTexture : 0u);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.texture);
        glBindVertexArray(batch.vao);
        Q238BindPaint(batch);
        Q237DrawTerrainBatch(batch);
    }

    glDepthMask(previousDepthMask);
    glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    glBlendFuncSeparate(static_cast<GLenum>(previousBlendSrcRgb),
                        static_cast<GLenum>(previousBlendDstRgb),
                        static_cast<GLenum>(previousBlendSrcAlpha),
                        static_cast<GLenum>(previousBlendDstAlpha));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glUniform1i(q238PaintCountLocation,0);
    for(size_t i=0;i<6u;++i){glActiveTexture(GL_TEXTURE0+q238Units[i]);glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(q238PreviousTextures[i]));}
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glUseProgram(static_cast<GLuint>(previousProgram));
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);

    const uint64_t q2018TerrainUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2018TerrainStarted).count());
    static uint64_t q2018TerrainPasses = 0u;
    ++q2018TerrainPasses;
    if ((q2018TerrainPasses % 240u) == 1u ||
        q2018TerrainUs >= 12000u) {
        Q76B_LOGI("Q231 TERRAIN CLIP: renderUs=%llu baseTested=%zu baseDrawn=%zu baseRejected=%zu alphaTested=%zu alphaDrawn=%zu alphaRejected=%zu totalBatches=%zu reflection=%d mirrorUnderwaterSkipped=%zu culling=homogeneous-padded bounds=cold+streamed unknown=draw",
                  static_cast<unsigned long long>(q2018TerrainUs),
                  q2018BaseTested, q2018BaseDrawn,
                  q2018BaseTested - q2018BaseDrawn,
                  q2018AlphaTested, q2018AlphaDrawn,
                  q2018AlphaTested - q2018AlphaDrawn,
                  q711TerrainBatches.size(),
                  q2090TerrainReflectionClipEnabled ? 1 : 0,
                  q228MirrorUnderwaterSkipped);
    }

    if (!q76bLoggedVisible) {
        q76bLoggedVisible = true;
        Q76B_LOGI("Q7.20 TERRAIN VISIBLE: worldspace=%08X cells=%zu baseTriangles=%zu alphaTriangles=%zu batches=%zu alphaBatches=%zu textured=%zu fallback=%zu alphaLayersResolved=%zu alphaQuads=%zu uploadFailures=%zu limitHits=%zu stereoRenderLayer=1",
                  q76bWorldspace, q76bTerrainCells, q76bTriangles,
                  q718AlphaTriangles, q711TerrainBatches.size(), q718AlphaBatches,
                  q711RealTextureBatches, q711FallbackBatches,
                  q718AlphaLayersResolved, q718AlphaQuads,
                  q713TextureUploadFailures, q713TextureLimitHits);
    }
}
