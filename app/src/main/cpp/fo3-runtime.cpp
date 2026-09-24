void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius);
void SetNextFo3CollisionExteriorModeQ1931(bool exterior);
#include <atomic>
#include <memory>
#include <thread>
#include "fo3-terrain-q76.h"
extern void SetFo3TerrainSelectionOverrideQ1890(bool enabled, float gameX, float gameY);
#include "fo3-authored-door-query-q1870.h"
extern void PumpFo3AndroidEventsQ1860();
#include "fo3-door-prompt-q1840.h"
#include <chrono>
#include "fo3-authored-door-query-q1730.h"
#include "fo3-cell-traversal-q1700.h"
#include "fo3-loading-state-q1700.h"
#include "fo3-loading-screen-q1700.h"
#include "fo3-megaton-scene.h"
#include "fo3-static-nif.h"
#include "fo3-bsa-reader.h"
#include "fo3-texture-bsa.h"
#include "fo3-collision-overlay.h"
#include "fo3-transition-q74.h"
#include "fo3-environment-q1000.h"
#include "fo3-imagespace-q1280.h"
#include "fo3-weather-imad-q1300.h"
#include "fo3-weather-light-q1320.h"
#include "fo3-external-emittance-q1380.h"
#include "fo3-authored-color-q1390.h"
#include "fo3-megaton-cell-environment-q1410.h"
#define LoadFo3ImageSpaceQ1280 LoadFo3ImageSpaceBaseQ1410
#include "fo3-time-of-day-q1400.h"
#include "fo3-fog-ab-q1450.h"
#include "fo3-render-stage-q1560.h"
#include "fo3-legacy-colour-domain-q1570.h"
#include "fo3-pplighting-domain-q1470.h"
#define FO3_Q1480_DEFINE_REFRESH 1
#include "fo3-weather-byte-staging-q1480.h"
#undef FO3_Q1480_DEFINE_REFRESH
#undef LoadFo3ImageSpaceQ1280
#include "fo3-visual-depth-q1010.h"

void SetFo3TerrainShadowQ1050(GLuint depthTexture, const float* lightMvp, bool enabled);
void RenderFo3TerrainShadowQ1050(const float* lightMvp, GLuint program, GLint mvpLocation, GLint alphaTestLocation);

bool QueueFo3MegatonEntryQ1000();
bool QueueFo3MegatonEntryQ1040();
bool QueueFo3MegatonEntryQ1860();

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
struct GpuObject;
bool Q1970ShouldRenderFullDetail(const GpuObject& object);
void Q1970ProbeNativeLod(float gameX, float gameY);
extern const float Q1890_EXTERIOR_CELL_SIZE;
extern bool gExteriorStreamingActiveQ1890;
extern bool gExteriorStreamBusyQ1890;
extern uint32_t gExteriorWorldspaceQ1890;
extern uint32_t gExteriorPersistentCellQ1890;
extern float gExteriorOriginXQ1890;
extern float gExteriorOriginYQ1890;
extern float gExteriorOriginZQ1890;
extern int32_t gExteriorWindowGridXQ1890;
extern int32_t gExteriorWindowGridYQ1890;
extern uint64_t gExteriorWindowGenerationQ1890;
extern bool gQ1920LatestGridValid;
extern int32_t gQ1920LatestGridX;
extern int32_t gQ1920LatestGridY;
void Q1990EnsureNativeLodForCell(int32_t cellX, int32_t cellY,
                                 float centerX, float centerY, float floorZ);


constexpr const char* Q6H_TAG = "FalloutQuest";
constexpr float FO3_UNITS_PER_METRE = 70.0f;
constexpr float FLOOR_Y = -1.55f;
constexpr float SCENE_FORWARD = 0.00f;
constexpr size_t MAX_SCENE_OBJECTS = 1200u;
constexpr size_t MAX_MODEL_ATTEMPTS = 2000u;
constexpr float MAX_MODEL_EXTENT_UNITS = 20000.0f;

#define Q6H_LOGI(...) __android_log_print(ANDROID_LOG_INFO, Q6H_TAG, __VA_ARGS__)
#define Q6H_LOGW(...) __android_log_print(ANDROID_LOG_WARN, Q6H_TAG, __VA_ARGS__)
#define Q6H_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, Q6H_TAG, __VA_ARGS__)

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
    GLuint glow = 0;
    GLsizei vertexCount = 0;
    float glossiness = 10.0f;
    float materialAlpha = 1.0f;
    float alphaThreshold = 0.5f;
    bool alphaBlend = false;
    bool alphaTest = false;
    uint8_t alphaSourceBlend = 6u;
    uint8_t alphaDestBlend = 7u;
    bool zBufferTestQ1200 = true;
    bool zBufferWriteQ1200 = true;
    bool realDiffuse = false;
    bool realNormal = false;
    bool realGlow = false;
    bool noLighting = false;
    bool noLightingFalloff = false;
    float noLightingFalloffParams[4]{0.0f, 1.0f, 1.0f, 1.0f};
    bool stencilDrawModePresent = false;
    uint8_t stencilDrawMode = 0u;
    bool decalQ1170 = false;
    bool useVertexColor = false;
    bool useVertexAlpha = false;
    bool specularEnabled = false;
    float specularColor[3]{1.0f, 1.0f, 1.0f};
    float emissiveColor[3]{0.0f, 0.0f, 0.0f};
    float emissiveMult = 1.0f;
    bool externalEmittanceFlagQ1380 = false;
    bool externalEmittanceEnabledQ1380 = false;
    bool externalEmittanceRegionQ1380 = false;
    float externalEmittanceColorQ1380[3]{0.0f, 0.0f, 0.0f};
    uint32_t externalEmittanceFormIdQ1380 = 0u;
    uint32_t refFormId = 0;
    uint32_t baseFormId = 0;
    std::string editorId;
    std::string modelPath;
    int32_t q1970GridX = 0;
    int32_t q1970GridY = 0;
    bool q1990NativeLod = false;
    std::string baseRecordType;
    Fo3DoorTeleport teleport;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
    std::vector<Vec3> q1580NormalSamples;
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
GLint gMaterialAlphaLocation = -1;
GLint gAlphaTestLocation = -1;
GLint gAlphaThresholdLocation = -1;
GLint gGlowLocationQ1020 = -1;
GLint gNoLightingLocationQ1020 = -1;
GLint gNoLightingFalloffLocationQ1160 = -1;
GLint gNoLightingFalloffParamsLocationQ1160 = -1;
GLint gUseVertexColorLocationQ1020 = -1;
GLint gUseVertexAlphaLocationQ1020 = -1;
GLint gSpecularEnabledLocationQ1020 = -1;
GLint gSpecularColorLocationQ1020 = -1;
GLint gEmissiveColorLocationQ1020 = -1;
GLint gEmissiveMultLocationQ1020 = -1;
GLint gGlowEnabledLocationQ1020 = -1;
GLint gExternalEmittanceEnabledLocationQ1380 = -1;
GLint gExternalEmittanceColorLocationQ1380 = -1;
GLint gNativeLodClipEnabledLocationQ1810 = -1;
GLint gNativeLodClipBoundsLocationQ1810 = -1;
GLint gLightMvpLocationQ1050 = -1;
GLint gShadowMapLocationQ1050 = -1;
GLint gShadowTexelLocationQ1050 = -1;
GLint gShadowsEnabledLocationQ1050 = -1;
GLuint gShadowFboQ1050 = 0;
GLuint gShadowDepthQ1050 = 0;
GLuint gShadowProgramQ1050 = 0;
GLint gShadowMvpLocationQ1050 = -1;
GLint gShadowDiffuseLocationQ1050 = -1;
GLint gShadowAlphaTestLocationQ1050 = -1;
GLint gShadowAlphaThresholdLocationQ1050 = -1;
float gLightMvpQ1050[16]{};
float gShadowLastEyeQ1050[3]{1e30f, 1e30f, 1e30f};
float gShadowLastSunQ1050[3]{0.0f, 0.0f, 0.0f};
bool gShadowReadyQ1050 = false;
bool gShadowDirtyQ1050 = true;
constexpr GLsizei Q1050_SHADOW_SIZE = 1024;
GLint gAmbientColorLocationQ1000 = -1;
GLint gSunlightColorLocationQ1000 = -1;
GLint gSunDirectionLocationQ1000 = -1;
GLint gEyePositionLocationQ1010 = -1;
GLint gFogColorLocationQ1010 = -1;
GLint gFogNearLocationQ1010 = -1;
GLint gFogFarLocationQ1010 = -1;
GLint gFogPowerLocationQ1410 = -1;
GLint gFogEnabledLocationQ1450 = -1;
GLint gRenderStageLocationQ1560 = -1;
GLint gLegacyColourDomainLocationQ1570 = -1;
GLint gLegacyAmbientLocationQ1570 = -1;
GLint gLegacySunlightLocationQ1570 = -1;
GLint gFogNearVertexLocationQ1532 = -1;
GLint gFogFarVertexLocationQ1532 = -1;
GLint gFogPowerVertexLocationQ1532 = -1;
GLint gSunDirectionVertexLocationQ1540 = -1;
GLint gEyePositionVertexLocationQ1630 = -1;
GLint gPpDiffuseDomainLocationQ1470 = -1;
GLint gLocalLightCountLocationQ1010 = -1;
GLint gLocalLightPosRadiusLocationQ1010 = -1;
GLint gLocalLightColorFalloffLocationQ1010 = -1;
std::vector<GpuObject> gObjects;
std::unordered_map<std::string, CachedGpuTexture> gTextureCache;
GLuint gDepthRenderbuffer = 0;
GLsizei gDepthWidth = 0;
GLsizei gDepthHeight = 0;
bool gSceneReady = false;
bool gLoggedFirstDraw = false;
uint32_t gCurrentCellFormId = 0x00002DBDu;
float gSceneCenterXQ1730 = 0.0f;
float gSceneCenterYQ1730 = 0.0f;
float gSceneFloorZQ1730 = 0.0f;
size_t gQ1380ExternalFlagShapes = 0u;
size_t gQ1380ExternalResolvedShapes = 0u;
size_t gQ1380ExternalFixedShapes = 0u;
size_t gQ1380ExternalRegionShapes = 0u;

std::string TextureCacheKey(const std::string& path, const char* label) {
    if (path.empty()) return std::string("<fallback>:") + label;
    std::string key = path;
    for (char& ch : key) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    while (!key.empty() && key.front() == '\\') key.erase(key.begin());
    if (key.rfind("data\\", 0) == 0) key = key.substr(5);
    if (key.rfind("textures\\", 0) == 0) key = key.substr(9);
    return std::string("textures\\") + key;
}

GLuint CompileQ6HShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Q6H_LOGE("Q6H shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateQ6HProgram() {
    static const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec3 aTangent;
        layout(location = 3) in vec3 aBitangent;
        layout(location = 4) in vec2 aUv;
        layout(location = 5) in vec4 aColor;
        uniform mat4 uMvp;
        uniform highp float uFogNearVertexQ1532;
        uniform highp float uFogFarVertexQ1532;
        uniform highp float uFogPowerVertexQ1532;
        uniform highp vec3 uSunDirectionVertexQ1540;
        uniform highp vec3 uEyePositionVertexQ1630;
        out vec3 vNormal;
        out highp float vFogFactorQ1532;
        out highp vec3 vSp17LightEncodedQ1540;
        out highp vec3 vSp17HalfQ1630;
        out vec3 vTangent;
        out vec3 vBitangent;
        out vec2 vUv;
        out vec3 vPosition;
        out vec4 vColor;
        out vec4 vShadowCoord;
        uniform mat4 uLightMvp;
        void main() {
            vNormal = aNormal;
            vTangent = aTangent;
            vBitangent = aBitangent;
            vUv = aUv;
            // SLS1011.vso: LightData is dotted with the authored T/B/N rows in
            // the vertex stage, then encoded for interpolation to the pixel stage.
            vec3 q1540LightData = normalize(uSunDirectionVertexQ1540);
            vec3 q1540LightTangent = vec3(
                dot(aTangent, q1540LightData),
                dot(aBitangent, q1540LightData),
                dot(aNormal, q1540LightData));
            // PC SLS vertex shader: dp3 T/B/N then rsq-normalize before oT1.
            float q1610LightLen2 = dot(q1540LightTangent, q1540LightTangent);
            vSp17LightEncodedQ1540 = q1540LightTangent *
                inversesqrt(max(q1610LightLen2, 1.0e-12));

            // PC SLS: normalize(EyePosition - vertex), add LightData, normalize,
            // project the half vector into T/B/N, then normalize before oT3.
            vec3 q1630ViewWorld = normalize(uEyePositionVertexQ1630 - aPosition);
            vec3 q1630HalfWorld = normalize(q1630ViewWorld + q1540LightData);
            vec3 q1630HalfTangent = vec3(
                dot(aTangent, q1630HalfWorld),
                dot(aBitangent, q1630HalfWorld),
                dot(aNormal, q1630HalfWorld));
            float q1630HalfLen2 = dot(q1630HalfTangent, q1630HalfTangent);
            vSp17HalfQ1630 = q1630HalfTangent *
                inversesqrt(max(q1630HalfLen2, 1.0e-12));
            vPosition = aPosition;
            vColor = aColor;
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
        in vec3 vTangent;
        in vec3 vBitangent;
        in vec2 vUv;
        in vec3 vPosition;
        in highp float vFogFactorQ1532;
        in highp vec3 vSp17LightEncodedQ1540;
        in highp vec3 vSp17HalfQ1630;
        in vec4 vColor;
        in vec4 vShadowCoord;
        uniform sampler2D uDiffuse;
        uniform sampler2D uNormalGloss;
        uniform sampler2D uGlow;
        uniform float uGlossiness;
        uniform float uNormalStrength;
        uniform float uMaterialAlpha;
        uniform float uAlphaTest;
        uniform float uAlphaThreshold;
        uniform float uNoLighting;
        uniform float uNoLightingFalloff;
        uniform vec4 uNoLightingFalloffParams;
        uniform float uUseVertexColor;
        uniform float uUseVertexAlpha;
        uniform float uSpecularEnabled;
        uniform vec3 uSpecularColor;
        uniform vec3 uEmissiveColor;
        uniform float uEmissiveMult;
        uniform float uGlowEnabled;
        uniform float uExternalEmittanceEnabledQ1380;
        uniform vec3 uExternalEmittanceColorQ1380;
        uniform sampler2D uShadowMap;
        uniform vec2 uShadowTexelSize;
        uniform float uShadowsEnabled;
        uniform vec3 uAmbientColor;
        uniform vec3 uSunlightColor;
        uniform vec3 uSunDirection;
        uniform vec3 uEyePosition;
        uniform vec3 uFogColor;
        uniform float uFogNear;
        uniform float uFogFar;
        uniform float uFogPower;
        uniform float uQ1450FogEnabled;
        uniform int uRenderStageQ1560;
        uniform float uLegacyColourDomainQ1570;
        uniform vec3 uLegacyAmbientQ1570;
        uniform vec3 uLegacySunlightQ1570;
        uniform float uQ1470LegacyPpDiffuseDomain;
        uniform float uNativeLodClipEnabledQ1810;
        uniform vec4 uNativeLodClipBoundsQ1810;
        uniform int uLocalLightCount;
        uniform vec4 uLocalLightPosRadius[8];
        uniform vec4 uLocalLightColorFalloff[8];
        out vec4 fragColor;
        float Q1470LinearToSrgb1(float value) {
            float c = max(value, 0.0);
            return c <= 0.0031308
                ? c * 12.92
                : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
        }
        vec3 Q1470LinearToSrgb(vec3 value) {
            return vec3(Q1470LinearToSrgb1(value.r),
                        Q1470LinearToSrgb1(value.g),
                        Q1470LinearToSrgb1(value.b));
        }
        float Q1470SrgbToLinear1(float value) {
            float c = max(value, 0.0);
            return c <= 0.04045
                ? c / 12.92
                : pow((c + 0.055) / 1.055, 2.4);
        }
        vec3 Q1470SrgbToLinear(vec3 value) {
            return vec3(Q1470SrgbToLinear1(value.r),
                        Q1470SrgbToLinear1(value.g),
                        Q1470SrgbToLinear1(value.b));
        }

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
        void main() {
            // Q18.1: Bethesda Level4 object/terrain meshes cover coarse 4x4
            // macroblocks. Clip only the part overlapped by the live 3x3
            // detailed cells; the remainder of the same authored LOD shape
            // stays visible beyond the near-world boundary.
            if (uNativeLodClipEnabledQ1810 > 0.5 &&
                vPosition.x >= uNativeLodClipBoundsQ1810.x &&
                vPosition.x <= uNativeLodClipBoundsQ1810.y &&
                vPosition.z >= uNativeLodClipBoundsQ1810.z &&
                vPosition.z <= uNativeLodClipBoundsQ1810.w) {
                discard;
            }
            vec4 diffuseTexel = texture(uDiffuse, vUv);
            vec3 baseColor = diffuseTexel.rgb * mix(vec3(1.0), vColor.rgb, uUseVertexColor);
            float alpha = diffuseTexel.a * uMaterialAlpha * mix(1.0, vColor.a, uUseVertexAlpha);
            if (uNoLightingFalloff > 0.5) {
                vec3 viewDirectionQ1160 = normalize(vPosition - uEyePosition);
                float viewAngleQ1160 = abs(dot(normalize(vNormal), viewDirectionQ1160));
                float falloffSpanQ1190 = uNoLightingFalloffParams.y - uNoLightingFalloffParams.x;
                float falloffTQ1160 = abs(falloffSpanQ1190) > 0.00001
                    ? clamp((viewAngleQ1160 - uNoLightingFalloffParams.x) / falloffSpanQ1190, 0.0, 1.0)
                    : 0.0;
                float startOpacityQ1160 = clamp(uNoLightingFalloffParams.z, 0.0, 1.0);
                float stopOpacityQ1160 = clamp(uNoLightingFalloffParams.w, 0.0, 1.0);
                alpha *= mix(startOpacityQ1160, stopOpacityQ1160, falloffTQ1160);
            }
            if (uAlphaTest > 0.5 && alpha < uAlphaThreshold) discard;

            vec4 normalGloss = texture(uNormalGloss, vUv);
            vec3 tangentNormal = normalGloss.rgb * 2.0 - 1.0;
            tangentNormal.xy *= uNormalStrength;
            tangentNormal = normalize(tangentNormal);
            // Q11.3: no tangent-space Y inversion; UVs are no longer double-flipped.

            vec3 N = normalize(vNormal);
            vec3 T = normalize(vTangent - N * dot(N, vTangent));
            vec3 B = normalize(vBitangent - N * dot(N, vBitangent));
            vec3 mappedNormal = normalize(mat3(T, B, N) * tangentNormal);

            vec3 lightDirection = normalize(uSunDirection);
            float q1540QuestLambert = max(dot(mappedNormal, lightDirection), 0.0);

            vec3 q1540Sp17Normal = normalize(normalGloss.rgb * 2.0 - 1.0);
            vec3 q1540Sp17Light = vSp17LightEncodedQ1540;
            // PC SLS pixel shader: dp3 + saturate. The interpolated light is
            // intentionally not renormalized in the pixel stage.
            float q1540Sp17Lambert =
                clamp(dot(q1540Sp17Normal, q1540Sp17Light), 0.0, 1.0);

            // Q15.6: Q15.4 tangent A/B retired; LEFT X is render-stage only.
            // Q15.11: PC SP17 diffuse is NormalMap dot tangent-space LightData.
            float lambert = q1540Sp17Lambert;
            // Q15.13: instruction-for-instruction SP17 specular structure from
            // the captured Megaton shader. No synthetic 0.32 attenuation and no
            // material RGB multiplier exist in this PC permutation.
            vec3 q1630Sp17Half = normalize(vSp17HalfQ1630);
            float q1630RawNdotL = dot(q1540Sp17Normal, q1540Sp17Light);
            float q1630NdotH = clamp(dot(q1540Sp17Normal, q1630Sp17Half), 0.0, 1.0);
            float q1630Exponent = clamp(uGlossiness, 0.0, 128.0);
            float q1630SpecBase = normalGloss.a * pow(q1630NdotH, q1630Exponent);
            float q1630SpecLow = q1630SpecBase * clamp(q1630RawNdotL + 0.5, 0.0, 1.0);
            float q1630Spec = q1630RawNdotL <= 0.2 ? q1630SpecLow : q1630SpecBase;
            vec3 q1630SpecularRgb = clamp(uSunlightColor * q1630Spec, 0.0, 1.0)
                                  * uSpecularEnabled;
            float q1050SunVisibility = Q1050ShadowVisibility(vShadowCoord, mappedNormal, lightDirection);
            vec3 q1630Sp17Lighting = max(
                uAmbientColor + uSunlightColor * lambert, vec3(0.0));
            vec3 q1470WorldDiffuse = baseColor * q1630Sp17Lighting;
            if (uLegacyColourDomainQ1570 > 0.5 && uNoLighting <= 0.5) {
                vec3 q1570BaseEncoded = Q1470LinearToSrgb(baseColor);
                vec3 q1570EncodedDiffuse = q1570BaseEncoded *
                    (uLegacyAmbientQ1570 + uLegacySunlightQ1570 * lambert);
                q1470WorldDiffuse = Q1470SrgbToLinear(q1570EncodedDiffuse);
            }
            vec3 lit = uNoLighting > 0.5
                ? baseColor
                : q1470WorldDiffuse + q1630SpecularRgb;
            for (int i = 0; i < 8; ++i) {
                if (uNoLighting > 0.5 || i >= uLocalLightCount) break;
                vec3 toLight = uLocalLightPosRadius[i].xyz - vPosition;
                float distanceToLight = length(toLight);
                float radius = max(uLocalLightPosRadius[i].w, 0.001);
                if (distanceToLight >= radius) continue;
                vec3 L = toLight / max(distanceToLight, 0.001);
                float localLambert = max(dot(mappedNormal, L), 0.0);
                float edge = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
                float attenuation = pow(edge, max(uLocalLightColorFalloff[i].w, 0.25));
                vec3 localColor = uLocalLightColorFalloff[i].rgb;
                lit += baseColor * localColor * localLambert * attenuation;
            }
            if (uExternalEmittanceEnabledQ1380 > 0.5) {
                vec3 externalMaskQ1380 = uGlowEnabled > 0.5
                    ? texture(uGlow, vUv).rgb
                    : baseColor;
                if (uNoLighting > 0.5) {
                    // NoLighting FX/statics are already self-lit. External
                    // emittance is their authored surface colour, not another
                    // additive light layered on top of the texture.
                    lit = externalMaskQ1380 * uExternalEmittanceColorQ1380;
                } else {
                    // PP-lit geometry keeps ambient/sun/local lighting while its
                    // emitted component comes from the reference's XEMI source.
                    lit += externalMaskQ1380 * uExternalEmittanceColorQ1380;
                }
            } else if (uNoLighting < 0.5) {
                vec3 emissiveMask = uGlowEnabled > 0.5
                    ? texture(uGlow, vUv).rgb
                    : baseColor;
                lit += emissiveMask * uEmissiveColor * uEmissiveMult;
            }
            float fogDistance = length(vPosition - uEyePosition);
            float fogT = clamp((fogDistance - uFogNear) /
                               max(uFogFar - uFogNear, 0.01), 0.0, 1.0);
            float q1532QuestFragmentFog = pow(fogT, max(uFogPower, 0.01));
            float fogFactor = uRenderStageQ1560 >= 1
                ? q1532QuestFragmentFog
                : 0.0;
            lit = mix(lit, uFogColor, fogFactor);
            fragColor = vec4(max(lit, vec3(0.0)), alpha);
        }
    )";

    GLuint vs = CompileQ6HShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileQ6HShader(GL_FRAGMENT_SHADER, fragmentSource);
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
        Q6H_LOGE("Q6H shader link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void Q1070ApplyStaticAnisotropy() {
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
        Q6H_LOGI("Q10.7 STATIC FILTER: anisotropy=%.1fx trilinear=1", level);
    }
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
        if (!gExteriorStreamingActiveQ1890) Q6H_LOGI("Q6H GPU %s CACHE HIT: ref=%08X key=%s real=%d",
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

    const bool q1230TextureTarget =
        cacheKey.find("megatonsignscrap01") != std::string::npos ||
        cacheKey.find("megatonsignscrap02") != std::string::npos ||
        cacheKey.find("megatonsignscrap03") != std::string::npos ||
        cacheKey.find("metalscrapshing") != std::string::npos ||
        cacheKey.find("metalscrapdoor03") != std::string::npos ||
        cacheKey.find("fxwhite.dds") != std::string::npos ||
        cacheKey.find("fxsoftglowspot01.dds") != std::string::npos;
    if (q1230TextureTarget && texture.rgba.size() >= 4u) {
        uint8_t q1230Min[4]{255u, 255u, 255u, 255u};
        uint8_t q1230Max[4]{0u, 0u, 0u, 0u};
        uint64_t q1230Sum[4]{0u, 0u, 0u, 0u};
        size_t q1230AlphaZero = 0u;
        size_t q1230AlphaMid = 0u;
        size_t q1230AlphaFull = 0u;
        const size_t q1230Pixels = texture.rgba.size() / 4u;
        for (size_t q1230I = 0u; q1230I < q1230Pixels; ++q1230I) {
            const size_t q1230Base = q1230I * 4u;
            for (size_t q1230C = 0u; q1230C < 4u; ++q1230C) {
                const uint8_t q1230V = texture.rgba[q1230Base + q1230C];
                if (q1230V < q1230Min[q1230C]) q1230Min[q1230C] = q1230V;
                if (q1230V > q1230Max[q1230C]) q1230Max[q1230C] = q1230V;
                q1230Sum[q1230C] += q1230V;
            }
            const uint8_t q1230A = texture.rgba[q1230Base + 3u];
            if (q1230A == 0u) ++q1230AlphaZero;
            else if (q1230A == 255u) ++q1230AlphaFull;
            else ++q1230AlphaMid;
        }
        Q6H_LOGI("Q12.3 TEX: key=%s real=%d source=%s size=%dx%d format=%s min=(%u %u %u %u) max=(%u %u %u %u) avg=(%u %u %u %u) alpha0=%zu alphaMid=%zu alpha255=%zu pixels=%zu",
                 cacheKey.c_str(), real ? 1 : 0, texture.sourcePath.c_str(),
                 texture.width, texture.height, texture.format.c_str(),
                 static_cast<unsigned>(q1230Min[0]), static_cast<unsigned>(q1230Min[1]),
                 static_cast<unsigned>(q1230Min[2]), static_cast<unsigned>(q1230Min[3]),
                 static_cast<unsigned>(q1230Max[0]), static_cast<unsigned>(q1230Max[1]),
                 static_cast<unsigned>(q1230Max[2]), static_cast<unsigned>(q1230Max[3]),
                 static_cast<unsigned>(q1230Sum[0] / q1230Pixels),
                 static_cast<unsigned>(q1230Sum[1] / q1230Pixels),
                 static_cast<unsigned>(q1230Sum[2] / q1230Pixels),
                 static_cast<unsigned>(q1230Sum[3] / q1230Pixels),
                 q1230AlphaZero, q1230AlphaMid, q1230AlphaFull, q1230Pixels);
    }

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Q1070ApplyStaticAnisotropy();
    // Q15.12: test the unresolved PC sampler-input colour-space state.
    // Only authored diffuse/BaseMap colour receives hardware sRGB -> linear
    // decoding. Normal/gloss and other data textures remain linear GL_RGBA8.
    const GLenum q1620InternalFormat =
        std::string(label) == "DIFFUSE" ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    glTexImage2D(GL_TEXTURE_2D, 0, q1620InternalFormat,
                 texture.width, texture.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (glGetError() != GL_NO_ERROR) {
        if (textureId) glDeleteTextures(1, &textureId);
        textureId = 0;
        return false;
    }

    gTextureCache.emplace(cacheKey, CachedGpuTexture{textureId, real});
    if (!gExteriorStreamingActiveQ1890) Q6H_LOGI("Q6H GPU %s CACHE MISS: ref=%08X source=%s %dx%d format=%s uniqueTextures=%zu",
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

bool BuildCpuObjects(const Fo3WorldPlacement& placement, std::vector<CpuObject>& outs) {
    outs.clear();
    static std::unordered_map<std::string, std::vector<Fo3StaticNifMesh>> modelCache;

    auto cached = modelCache.find(placement.modelPath);
    if (cached == modelCache.end()) {
        std::vector<Fo3StaticNifMesh> meshes;
        if (!LoadFo3StaticNifMeshes(placement.modelPath, meshes)) return false;
        cached = modelCache.emplace(placement.modelPath, std::move(meshes)).first;
        Q6H_LOGI("Q6H MODEL CACHE MISS: model=%s shapes=%zu uniqueModels=%zu",
                 placement.modelPath.c_str(), cached->second.size(), modelCache.size());
    }

    size_t shapeIndex = 0;
    for (const Fo3StaticNifMesh& cachedMesh : cached->second) {
        Fo3StaticNifMesh mesh = cachedMesh;
        const float extent = MaxModelExtent(mesh) * placement.scale;
        if (!(extent >= 1.0f && extent <= MAX_MODEL_EXTENT_UNITS)) {
            Q6H_LOGW("Q7.16 STATIC SKIP SIZE: ref=%08X model=%s shape=%zu extent=%.1f",
                     placement.refFormId, placement.modelPath.c_str(), shapeIndex++, extent);
            continue;
        }

        const size_t vertexCount = mesh.positions.size() / 3u;
        if (mesh.normals.size() / 3u != vertexCount ||
            mesh.tangents.size() / 3u != vertexCount ||
            mesh.bitangents.size() / 3u != vertexCount ||
            mesh.texcoords.size() / 2u != vertexCount) {
            ++shapeIndex;
            continue;
        }

        CpuObject out;
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

        outs.push_back(std::move(out));
        ++shapeIndex;
    }
    return !outs.empty();
}

bool ResolveDoorTeleportCachedQ1698(uint32_t refFormId, Fo3DoorTeleport& out) {
    static std::unordered_map<uint32_t, Fo3DoorTeleport> cache;
    const auto cached = cache.find(refFormId);
    if (cached != cache.end()) {
        out = cached->second;
        return out.valid;
    }
    Fo3DoorTeleport resolved;
    ResolveFo3DoorTeleportQ1700(refFormId, &resolved);
    cache.emplace(refFormId, resolved);
    out = resolved;
    return out.valid;
}

bool UploadCpuObject(CpuObject& cpu, float centerX, float centerY, float floorZ,
                     GpuObject& gpu) {
    constexpr size_t FLOATS_PER_VERTEX = 18u;
    const size_t vertexCount = cpu.positionsGame.size();
    std::vector<float> expanded;
    expanded.reserve(cpu.mesh.indices.size() * FLOATS_PER_VERTEX);
    Vec3 objectMinimum{1e30f, 1e30f, 1e30f};
    Vec3 objectMaximum{-1e30f, -1e30f, -1e30f};

    for (uint32_t index : cpu.mesh.indices) {
        if (index >= vertexCount) return false;
        const Vec3 gameP = cpu.positionsGame[index];
        const Vec3 p{
            (gameP.x - centerX) / FO3_UNITS_PER_METRE,
            FLOOR_Y + (gameP.z - floorZ) / FO3_UNITS_PER_METRE,
            SCENE_FORWARD - (gameP.y - centerY) / FO3_UNITS_PER_METRE,
        };
        objectMinimum.x = std::min(objectMinimum.x, p.x);
        objectMinimum.y = std::min(objectMinimum.y, p.y);
        objectMinimum.z = std::min(objectMinimum.z, p.z);
        objectMaximum.x = std::max(objectMaximum.x, p.x);
        objectMaximum.y = std::max(objectMaximum.y, p.y);
        objectMaximum.z = std::max(objectMaximum.z, p.z);
        const Vec3 n = GameDirectionToOpenXr(cpu.normalsGame[index]);
        const Vec3 t = GameDirectionToOpenXr(cpu.tangentsGame[index]);
        const Vec3 b = GameDirectionToOpenXr(cpu.bitangentsGame[index]);
        const float u = cpu.mesh.texcoords[static_cast<size_t>(index) * 2u];
        const float v = cpu.mesh.texcoords[static_cast<size_t>(index) * 2u + 1u];
        float cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;
        if (cpu.mesh.vertexColors.size() == vertexCount * 4u) {
            const size_t ci = static_cast<size_t>(index) * 4u;
            cr = cpu.mesh.vertexColors[ci + 0u];
            cg = cpu.mesh.vertexColors[ci + 1u];
            cb = cpu.mesh.vertexColors[ci + 2u];
            ca = cpu.mesh.vertexColors[ci + 3u];
        }

        expanded.insert(expanded.end(), {
            p.x, p.y, p.z,
            n.x, n.y, n.z,
            t.x, t.y, t.z,
            b.x, b.y, b.z,
            u, v,
            cr, cg, cb, ca,
        });
    }
    if (expanded.empty()) return false;

    gpu = {};
    gpu.glossiness = std::max(2.0f, cpu.mesh.glossiness);
    gpu.materialAlpha = std::clamp(cpu.mesh.alpha, 0.0f, 1.0f);
    gpu.alphaThreshold = std::clamp(cpu.mesh.alphaThreshold, 0.0f, 1.0f);
    gpu.alphaBlend = cpu.mesh.alphaBlend || gpu.materialAlpha < 0.999f;
    gpu.alphaTest = cpu.mesh.alphaTest;
    gpu.alphaSourceBlend = cpu.mesh.alphaSourceBlend;
    gpu.alphaDestBlend = cpu.mesh.alphaDestBlend;
    gpu.zBufferTestQ1200 = (cpu.mesh.shaderFlags1 & 0x80000000u) != 0u;
    gpu.zBufferWriteQ1200 = (cpu.mesh.shaderFlags2 & 0x00000001u) != 0u;
    if (!gExteriorStreamingActiveQ1890) Q6H_LOGI("Q12.0 Z STATE: ref=%08X model=%s test=%d write=%d alphaBlend=%d flags1=%08X flags2=%08X",
             gpu.refFormId, cpu.placement.modelPath.c_str(),
             gpu.zBufferTestQ1200 ? 1 : 0, gpu.zBufferWriteQ1200 ? 1 : 0,
             gpu.alphaBlend ? 1 : 0, cpu.mesh.shaderFlags1, cpu.mesh.shaderFlags2);
    if (gpu.alphaBlend) {
        Q6H_LOGI("Q11.5 BLEND STATE: ref=%08X model=%s src=%u dst=%u noLighting=%d",
                 gpu.refFormId, cpu.placement.modelPath.c_str(),
                 static_cast<unsigned>(gpu.alphaSourceBlend),
                 static_cast<unsigned>(gpu.alphaDestBlend),
                 cpu.mesh.noLighting ? 1 : 0);
    }
    gpu.noLighting = cpu.mesh.noLighting;
    gpu.noLightingFalloff =
        cpu.mesh.noLightingFalloff && !cpu.mesh.diffuseTexturePath.empty();
    if (cpu.mesh.noLightingFalloff && cpu.mesh.diffuseTexturePath.empty()) {
        Q6H_LOGI("Q11.9 NOLIGHT FALLOFF SKIP: ref=%08X model=%s reason=textureless",
                 gpu.refFormId, cpu.placement.modelPath.c_str());
    }
    for (int q1160i = 0; q1160i < 4; ++q1160i) gpu.noLightingFalloffParams[q1160i] = cpu.mesh.noLightingFalloffParams[q1160i];
    gpu.stencilDrawModePresent = cpu.mesh.stencilDrawModePresent;
    gpu.stencilDrawMode = cpu.mesh.stencilDrawMode;
    gpu.decalQ1170 = (cpu.mesh.shaderFlags1 & 0x04000000u) != 0u;
    if (gpu.decalQ1170) {
        Q6H_LOGI("Q11.7 DECAL BIAS: ref=%08X model=%s shaderFlags1=%08X",
                 gpu.refFormId, cpu.placement.modelPath.c_str(), cpu.mesh.shaderFlags1);
    }
    if (gpu.noLightingFalloff) {
        Q6H_LOGI("Q11.6 NOLIGHT FALLOFF: ref=%08X model=%s angle=(%.4f %.4f) opacity=(%.4f %.4f)",
                 gpu.refFormId, cpu.placement.modelPath.c_str(),
                 gpu.noLightingFalloffParams[0], gpu.noLightingFalloffParams[1],
                 gpu.noLightingFalloffParams[2], gpu.noLightingFalloffParams[3]);
    }
    if (gpu.stencilDrawModePresent) {
        Q6H_LOGI("Q11.6 STENCIL DRAW: ref=%08X model=%s mode=%u",
                 gpu.refFormId, cpu.placement.modelPath.c_str(),
                 static_cast<unsigned>(gpu.stencilDrawMode));
    }
    const bool q1120HasVertexColorStream =
        cpu.mesh.vertexColors.size() == vertexCount * 4u;
    gpu.useVertexColor =
        q1120HasVertexColorStream &&
        (cpu.mesh.noLighting || (cpu.mesh.shaderFlags2 & 0x00000020u) != 0u);
    const bool q1140TexturelessNoLightingOverlay =
        q1120HasVertexColorStream && cpu.mesh.noLighting &&
        cpu.mesh.diffuseTexturePath.empty() && cpu.mesh.alphaBlend;
    gpu.useVertexAlpha =
        gpu.useVertexColor &&
        (((cpu.mesh.shaderFlags1 & 0x00000008u) != 0u) ||
         q1140TexturelessNoLightingOverlay);
    if (q1140TexturelessNoLightingOverlay) {
        float q1140MinAlpha = 1.0f;
        float q1140MaxAlpha = 0.0f;
        for (size_t q1140i = 3u; q1140i < cpu.mesh.vertexColors.size(); q1140i += 4u) {
            q1140MinAlpha = std::min(q1140MinAlpha, cpu.mesh.vertexColors[q1140i]);
            q1140MaxAlpha = std::max(q1140MaxAlpha, cpu.mesh.vertexColors[q1140i]);
        }
        Q6H_LOGI("Q11.4 OVERLAY ALPHA: ref=%08X model=%s min=%.3f max=%.3f applied=1",
                 gpu.refFormId, cpu.placement.modelPath.c_str(),
                 q1140MinAlpha, q1140MaxAlpha);
    }
    if (q1120HasVertexColorStream && !cpu.mesh.noLighting &&
        (cpu.mesh.shaderFlags2 & 0x00000020u) == 0u) {
        if (!gExteriorStreamingActiveQ1890) Q6H_LOGI("Q11.2 VCOLOR GATE: ref=%08X model=%s shaderFlags2=%08X stream=1 applied=0",
                 gpu.refFormId, cpu.placement.modelPath.c_str(),
                 cpu.mesh.shaderFlags2);
    }
    gpu.specularEnabled = !gpu.noLighting && (cpu.mesh.shaderFlags1 & 0x00000001u) != 0u;
    for (int i = 0; i < 3; ++i) { gpu.specularColor[i] = cpu.mesh.specularColor[i]; gpu.emissiveColor[i] = cpu.mesh.emissiveColor[i]; }
    gpu.emissiveMult = std::max(0.0f, cpu.mesh.emissiveMult);
    gpu.externalEmittanceFlagQ1380 =
        (cpu.mesh.shaderFlags1 & fo3emittanceq1380::EXTERNAL_EMITTANCE_SHADER_FLAG) != 0u;
    if (gpu.externalEmittanceFlagQ1380) {
        ++gQ1380ExternalFlagShapes;
        Fo3ExternalEmittanceQ1380 q1380;
        if (ResolveFo3ExternalEmittanceQ1390(0x00000A74u,
                                             cpu.placement.refFormId, q1380)) {
            gpu.externalEmittanceEnabledQ1380 = true;
            gpu.externalEmittanceRegionQ1380 = q1380.regionDriven;
            gpu.externalEmittanceFormIdQ1380 = q1380.emittanceFormId;
            for (int i = 0; i < 3; ++i) {
                gpu.externalEmittanceColorQ1380[i] = q1380.color[i];
            }
            ++gQ1380ExternalResolvedShapes;
            if (q1380.regionDriven) ++gQ1380ExternalRegionShapes;
            else ++gQ1380ExternalFixedShapes;
        }
    }
    gpu.refFormId = cpu.placement.refFormId;
    gpu.baseFormId = cpu.placement.baseFormId;
    gpu.editorId = cpu.placement.editorId;
    gpu.modelPath = cpu.placement.modelPath;
    gpu.q1970GridX = cpu.placement.hasExteriorGrid
        ? cpu.placement.gridX
        : static_cast<int32_t>(std::floor(cpu.placement.x / Q1890_EXTERIOR_CELL_SIZE));
    gpu.q1970GridY = cpu.placement.hasExteriorGrid
        ? cpu.placement.gridY
        : static_cast<int32_t>(std::floor(cpu.placement.y / Q1890_EXTERIOR_CELL_SIZE));
    gpu.baseRecordType = cpu.placement.baseRecordType;
    if (gpu.baseRecordType == "DOOR") {
        ResolveDoorTeleportCachedQ1698(gpu.refFormId, gpu.teleport);
        if (gpu.teleport.valid) {
            CacheFo3DoorPromptQ1840(gpu.refFormId,
                                    cpu.placement.baseFormId,
                                    gpu.teleport);
        }
    }
    gpu.minX = objectMinimum.x; gpu.maxX = objectMaximum.x;
    gpu.minY = objectMinimum.y; gpu.maxY = objectMaximum.y;
    gpu.minZ = objectMinimum.z; gpu.maxZ = objectMaximum.z;

    constexpr size_t Q1580_MAX_NORMAL_SAMPLES = 64u;
    const size_t q1580IndexCount = cpu.mesh.indices.size();
    const size_t q1580Wanted = std::min(Q1580_MAX_NORMAL_SAMPLES, q1580IndexCount);
    gpu.q1580NormalSamples.reserve(q1580Wanted);
    for (size_t q1580Sample = 0; q1580Sample < q1580Wanted; ++q1580Sample) {
        const size_t q1580StreamPos =
            (q1580Sample * q1580IndexCount) / std::max<size_t>(q1580Wanted, 1u);
        if (q1580StreamPos >= q1580IndexCount) continue;
        const uint32_t q1580Index = cpu.mesh.indices[q1580StreamPos];
        if (q1580Index >= cpu.normalsGame.size()) continue;
        gpu.q1580NormalSamples.push_back(
            GameDirectionToOpenXr(cpu.normalsGame[q1580Index]));
    }

    std::string q1230ModelLower = cpu.placement.modelPath;
    for (char& q1230Ch : q1230ModelLower) {
        if (q1230Ch == '/') q1230Ch = '\\';
        q1230Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(q1230Ch)));
    }
    const bool q1230MeshTarget =
        q1230ModelLower.find("megatonbrasslanternsign") != std::string::npos ||
        q1230ModelLower.find("megatonchurchofatom") != std::string::npos;
    if (q1230MeshTarget) {
        Q6H_LOGI("Q12.3 MESH: ref=%08X EDID=%s model=%s verts=%zu tris=%zu diffuse=%s normal=%s glow=%s noLighting=%d flags1=%08X flags2=%08X alphaBlend=%d src=%u dst=%u alphaTest=%d threshold=%.4f matAlpha=%.4f useVC=%d useVA=%d specEnabled=%d spec=(%.4f %.4f %.4f) emissive=(%.4f %.4f %.4f) emissiveMult=%.4f gloss=%.4f envScale=%.4f",
                 gpu.refFormId,
                 cpu.placement.editorId.empty() ? "<none>" : cpu.placement.editorId.c_str(),
                 cpu.placement.modelPath.c_str(), vertexCount, cpu.mesh.indices.size() / 3u,
                 cpu.mesh.diffuseTexturePath.empty() ? "<none>" : cpu.mesh.diffuseTexturePath.c_str(),
                 cpu.mesh.normalTexturePath.empty() ? "<none>" : cpu.mesh.normalTexturePath.c_str(),
                 cpu.mesh.glowTexturePath.empty() ? "<none>" : cpu.mesh.glowTexturePath.c_str(),
                 cpu.mesh.noLighting ? 1 : 0, cpu.mesh.shaderFlags1, cpu.mesh.shaderFlags2,
                 cpu.mesh.alphaBlend ? 1 : 0,
                 static_cast<unsigned>(cpu.mesh.alphaSourceBlend),
                 static_cast<unsigned>(cpu.mesh.alphaDestBlend),
                 cpu.mesh.alphaTest ? 1 : 0, cpu.mesh.alphaThreshold, cpu.mesh.alpha,
                 gpu.useVertexColor ? 1 : 0, gpu.useVertexAlpha ? 1 : 0,
                 gpu.specularEnabled ? 1 : 0,
                 cpu.mesh.specularColor[0], cpu.mesh.specularColor[1], cpu.mesh.specularColor[2],
                 cpu.mesh.emissiveColor[0], cpu.mesh.emissiveColor[1], cpu.mesh.emissiveColor[2],
                 cpu.mesh.emissiveMult, cpu.mesh.glossiness, cpu.mesh.environmentMapScale);

        if (cpu.mesh.texcoords.size() >= 2u) {
            float q1230MinU = cpu.mesh.texcoords[0], q1230MaxU = cpu.mesh.texcoords[0];
            float q1230MinV = cpu.mesh.texcoords[1], q1230MaxV = cpu.mesh.texcoords[1];
            double q1230SumU = 0.0, q1230SumV = 0.0;
            const size_t q1230UvCount = cpu.mesh.texcoords.size() / 2u;
            for (size_t q1230I = 0u; q1230I < q1230UvCount; ++q1230I) {
                const float q1230U = cpu.mesh.texcoords[q1230I * 2u];
                const float q1230V = cpu.mesh.texcoords[q1230I * 2u + 1u];
                if (q1230U < q1230MinU) q1230MinU = q1230U;
                if (q1230U > q1230MaxU) q1230MaxU = q1230U;
                if (q1230V < q1230MinV) q1230MinV = q1230V;
                if (q1230V > q1230MaxV) q1230MaxV = q1230V;
                q1230SumU += q1230U;
                q1230SumV += q1230V;
            }
            Q6H_LOGI("Q12.3 UV: ref=%08X model=%s tris=%zu count=%zu min=(%.5f %.5f) max=(%.5f %.5f) avg=(%.5f %.5f)",
                     gpu.refFormId, cpu.placement.modelPath.c_str(), cpu.mesh.indices.size() / 3u,
                     q1230UvCount, q1230MinU, q1230MinV, q1230MaxU, q1230MaxV,
                     static_cast<float>(q1230SumU / static_cast<double>(q1230UvCount)),
                     static_cast<float>(q1230SumV / static_cast<double>(q1230UvCount)));
        }

        if (cpu.mesh.vertexColors.size() >= 4u) {
            float q1230MinC[4]{cpu.mesh.vertexColors[0], cpu.mesh.vertexColors[1],
                               cpu.mesh.vertexColors[2], cpu.mesh.vertexColors[3]};
            float q1230MaxC[4]{q1230MinC[0], q1230MinC[1], q1230MinC[2], q1230MinC[3]};
            double q1230SumC[4]{0.0, 0.0, 0.0, 0.0};
            const size_t q1230ColorCount = cpu.mesh.vertexColors.size() / 4u;
            for (size_t q1230I = 0u; q1230I < q1230ColorCount; ++q1230I) {
                for (size_t q1230C = 0u; q1230C < 4u; ++q1230C) {
                    const float q1230V = cpu.mesh.vertexColors[q1230I * 4u + q1230C];
                    if (q1230V < q1230MinC[q1230C]) q1230MinC[q1230C] = q1230V;
                    if (q1230V > q1230MaxC[q1230C]) q1230MaxC[q1230C] = q1230V;
                    q1230SumC[q1230C] += q1230V;
                }
            }
            Q6H_LOGI("Q12.3 VCOLOR: ref=%08X model=%s tris=%zu count=%zu min=(%.4f %.4f %.4f %.4f) max=(%.4f %.4f %.4f %.4f) avg=(%.4f %.4f %.4f %.4f)",
                     gpu.refFormId, cpu.placement.modelPath.c_str(), cpu.mesh.indices.size() / 3u,
                     q1230ColorCount,
                     q1230MinC[0], q1230MinC[1], q1230MinC[2], q1230MinC[3],
                     q1230MaxC[0], q1230MaxC[1], q1230MaxC[2], q1230MaxC[3],
                     static_cast<float>(q1230SumC[0] / static_cast<double>(q1230ColorCount)),
                     static_cast<float>(q1230SumC[1] / static_cast<double>(q1230ColorCount)),
                     static_cast<float>(q1230SumC[2] / static_cast<double>(q1230ColorCount)),
                     static_cast<float>(q1230SumC[3] / static_cast<double>(q1230ColorCount)));
        } else {
            Q6H_LOGI("Q12.3 VCOLOR: ref=%08X model=%s tris=%zu count=0",
                     gpu.refFormId, cpu.placement.modelPath.c_str(), cpu.mesh.indices.size() / 3u);
        }
    }

    const bool q1110NoLightingVertexColorOnly =
        cpu.mesh.noLighting &&
        cpu.mesh.diffuseTexturePath.empty() &&
        cpu.mesh.vertexColors.size() == vertexCount * 4u;
    if (q1110NoLightingVertexColorOnly) {
        if (!UploadTexture(cpu.mesh.diffuseTexturePath, {255u, 255u, 255u, 255u},
                           gpu.diffuse, gpu.realDiffuse,
                           "DIFFUSE_NOLIGHT_VCOLOR", gpu.refFormId)) return false;
        Q6H_LOGI("Q11.1 NOLIGHT VCOLOR: ref=%08X model=%s vertices=%zu diffuse=<white-neutral>",
                 gpu.refFormId, cpu.placement.modelPath.c_str(), vertexCount);
    } else {
        if (!UploadTexture(cpu.mesh.diffuseTexturePath, {190u, 170u, 135u, 255u},
                           gpu.diffuse, gpu.realDiffuse, "DIFFUSE", gpu.refFormId)) return false;
    }
    if (!UploadTexture(cpu.mesh.normalTexturePath, {128u, 128u, 255u, 0u},
                       gpu.normal, gpu.realNormal, "NORMAL", gpu.refFormId)) return false;
    if (!UploadTexture(cpu.mesh.glowTexturePath, {0u, 0u, 0u, 255u},
                       gpu.glow, gpu.realGlow, "GLOW", gpu.refFormId)) return false;

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
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(14 * sizeof(float)));
    glEnableVertexAttribArray(5);
    glBindVertexArray(0);

    gpu.vertexCount = static_cast<GLsizei>(expanded.size() / FLOATS_PER_VERTEX);
    if (glGetError() != GL_NO_ERROR) return false;

    if (!gExteriorStreamingActiveQ1890) Q6H_LOGI("Q6H GPU OBJECT READY: ref=%08X EDID=%s model=%s triangles=%d diffuse=%s normal=%s alphaBlend=%d alphaTest=%d alpha=%.2f threshold=%.2f",
             gpu.refFormId, gpu.editorId.empty() ? "<none>" : gpu.editorId.c_str(),
             gpu.modelPath.c_str(), gpu.vertexCount / 3,
             gpu.realDiffuse ? "REAL" : "FALLBACK",
             gpu.realNormal ? "REAL" : "FALLBACK",
             gpu.alphaBlend ? 1 : 0, gpu.alphaTest ? 1 : 0,
             gpu.materialAlpha, gpu.alphaThreshold);
    return true;
}

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
        if (gExteriorWorldspaceQ1890 == 0x0000003Cu) {
            const int32_t q1970ShadowGridX = gQ1920LatestGridValid
                ? gQ1920LatestGridX : gExteriorWindowGridXQ1890;
            const int32_t q1970ShadowGridY = gQ1920LatestGridValid
                ? gQ1920LatestGridY : gExteriorWindowGridYQ1890;
            if (std::abs(object.q1970GridX - q1970ShadowGridX) > 1 ||
                std::abs(object.q1970GridY - q1970ShadowGridY) > 1) continue;
        }
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

bool InitializeScene() {
    if (gSceneReady) return true;

    std::vector<Fo3WorldPlacement> placements;
    if (!LoadMegatonPlayerHousePlacements(placements)) {
        Q6H_LOGE("Q6H FAILED: ESM returned no MegatonPlayerHouse model placements");
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

    Q6H_LOGI("Q6H CELL SOURCE: cell=000151E3 modelPlacements=%zu centroid=(%.1f %.1f %.1f) fullCell=1",
             placements.size(), centroidX, centroidY, centroidZ);

    std::vector<CpuObject> selected;
    selected.reserve(placements.size() * 2u);
    size_t attempts = 0;
    size_t selectedPlacements = 0;
    size_t filteredMarkersEffects = 0;
    size_t unsupportedPlacements = 0;
    std::unordered_map<std::string, size_t> attemptedTypes;
    std::unordered_map<std::string, size_t> renderedTypes;

    for (const Fo3WorldPlacement& placement : placements) {
        ++attempts;
        const std::string type = placement.baseRecordType.empty() ? "<none>" : placement.baseRecordType;
        ++attemptedTypes[type];

        if (placement.editorId == "XMarker" ||
            placement.modelPath.rfind("Effects\\", 0) == 0 ||
            placement.modelPath.rfind("effects\\", 0) == 0) {
            ++filteredMarkersEffects;
            continue;
        }

        std::vector<CpuObject> parts;
        if (BuildCpuObjects(placement, parts)) {
            ++selectedPlacements;
            ++renderedTypes[type];
            for (CpuObject& part : parts) selected.push_back(std::move(part));
        } else {
            ++unsupportedPlacements;
        }
    }

    Q6H_LOGI("Q7.16 STATIC COVERAGE: esmModelPlacements=%zu attempted=%zu renderedPlacements=%zu drawShapes=%zu filteredMarkersEffects=%zu unsupported=%zu fullCell=1",
             placements.size(), attempts, selectedPlacements, selected.size(),
             filteredMarkersEffects, unsupportedPlacements);
    for (const auto& entry : attemptedTypes) {
        const auto rendered = renderedTypes.find(entry.first);
        const size_t renderedCount = rendered == renderedTypes.end() ? 0u : rendered->second;
        Q6H_LOGI("Q6H CELL TYPE: type=%s attempted=%zu rendered=%zu",
                 entry.first.c_str(), entry.second, renderedCount);
    }

    if (selected.size() < 2u) {
        Q6H_LOGE("Q6H FAILED: only %zu supported ESM-driven objects after %zu attempts",
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

    float structureMinX = 1e30f, structureMinY = 1e30f;
    float structureMaxX = -1e30f, structureMaxY = -1e30f;
    size_t structureShapes = 0;
    for (const CpuObject& object : selected) {
        const std::string& path = object.placement.modelPath;
        const bool architecture =
            path.find("Architecture\\Megaton\\interior\\ShackInteriors") != std::string::npos ||
            path.find("architecture\\megaton\\interior\\shackinteriors") != std::string::npos;
        if (!architecture) continue;
        structureMinX = std::min(structureMinX, object.placement.x);
        structureMaxX = std::max(structureMaxX, object.placement.x);
        structureMinY = std::min(structureMinY, object.placement.y);
        structureMaxY = std::max(structureMaxY, object.placement.y);
        ++structureShapes;
    }

    Fo3CellArrival q6kArrival;
    const bool q6kArrivalReady = LoadMegatonPlayerHouseArrival(q6kArrival) && q6kArrival.valid;
    const float fallbackCenterX = structureShapes > 0u
        ? (structureMinX + structureMaxX) * 0.5f
        : (minimum.x + maximum.x) * 0.5f;
    const float fallbackCenterY = structureShapes > 0u
        ? (structureMinY + structureMaxY) * 0.5f
        : (minimum.y + maximum.y) * 0.5f;

    const float centerX = q6kArrivalReady ? q6kArrival.x : fallbackCenterX;
    const float centerY = q6kArrivalReady ? q6kArrival.y : fallbackCenterY;
    const float floorZ = q6kArrivalReady ? q6kArrival.z : minimum.z;
    gSceneCenterXQ1730 = centerX;
    gSceneCenterYQ1730 = centerY;
    gSceneFloorZQ1730 = floorZ;
    Q6H_LOGI("Q6H SPAWN SEED: source=%s center=(%.2f %.2f) floorReferenceZ=%.2f structureShapes=%zu VRseed=(0,0) collisionValidation=pending",
             q6kArrivalReady ? "Fallout3.esm/XTEL" : "structure-fallback",
             centerX, centerY, floorZ, structureShapes);
    Q6H_LOGI("Q6H SCENE FRAME: selected=%zu attempts=%zu gameBounds=[%.1f %.1f %.1f]-[%.1f %.1f %.1f] sizeMetres=(%.2f %.2f %.2f) globalOffsetOnly=1",
             selected.size(), attempts,
             minimum.x, minimum.y, minimum.z,
             maximum.x, maximum.y, maximum.z,
             (maximum.x - minimum.x) / FO3_UNITS_PER_METRE,
             (maximum.y - minimum.y) / FO3_UNITS_PER_METRE,
             (maximum.z - minimum.z) / FO3_UNITS_PER_METRE);

    std::vector<Fo3WorldPlacement> q6hCollisionPlacements;
    q6hCollisionPlacements.reserve(selected.size());
    for (const CpuObject& object : selected) {
        q6hCollisionPlacements.push_back(object.placement);
    }
    const bool collisionReady = InitializeFo3CollisionOverlay(q6hCollisionPlacements,
                                                               centerX, centerY, floorZ,
                                                               SCENE_FORWARD, FLOOR_Y,
                                                               FO3_UNITS_PER_METRE);
    Q6H_LOGI("Q6H COLLISION WORLD: requestedRefs=%zu ready=%d safeSpawnValidation=firstFrame",
             q6hCollisionPlacements.size(), collisionReady ? 1 : 0);

    gProgram = CreateQ6HProgram();
    if (!gProgram) return false;
    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    gDiffuseLocation = glGetUniformLocation(gProgram, "uDiffuse");
    gNormalLocation = glGetUniformLocation(gProgram, "uNormalGloss");
    gGlossinessLocation = glGetUniformLocation(gProgram, "uGlossiness");
    gNormalStrengthLocation = glGetUniformLocation(gProgram, "uNormalStrength");
    gMaterialAlphaLocation = glGetUniformLocation(gProgram, "uMaterialAlpha");
    gAlphaTestLocation = glGetUniformLocation(gProgram, "uAlphaTest");
    gAlphaThresholdLocation = glGetUniformLocation(gProgram, "uAlphaThreshold");
    gGlowLocationQ1020 = glGetUniformLocation(gProgram, "uGlow");
    gNoLightingLocationQ1020 = glGetUniformLocation(gProgram, "uNoLighting");
    gNoLightingFalloffLocationQ1160 = glGetUniformLocation(gProgram, "uNoLightingFalloff");
    gNoLightingFalloffParamsLocationQ1160 = glGetUniformLocation(gProgram, "uNoLightingFalloffParams");
    gUseVertexColorLocationQ1020 = glGetUniformLocation(gProgram, "uUseVertexColor");
    gUseVertexAlphaLocationQ1020 = glGetUniformLocation(gProgram, "uUseVertexAlpha");
    gSpecularEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uSpecularEnabled");
    gSpecularColorLocationQ1020 = glGetUniformLocation(gProgram, "uSpecularColor");
    gEmissiveColorLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveColor");
    gEmissiveMultLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveMult");
    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uGlowEnabled");
    gExternalEmittanceEnabledLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceEnabledQ1380");
    gExternalEmittanceColorLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceColorQ1380");
    gNativeLodClipEnabledLocationQ1810 =
        glGetUniformLocation(gProgram, "uNativeLodClipEnabledQ1810");
    gNativeLodClipBoundsLocationQ1810 =
        glGetUniformLocation(gProgram, "uNativeLodClipBoundsQ1810");
    gLightMvpLocationQ1050 = glGetUniformLocation(gProgram, "uLightMvp");
    gShadowMapLocationQ1050 = glGetUniformLocation(gProgram, "uShadowMap");
    gShadowTexelLocationQ1050 = glGetUniformLocation(gProgram, "uShadowTexelSize");
    gShadowsEnabledLocationQ1050 = glGetUniformLocation(gProgram, "uShadowsEnabled");
    gAmbientColorLocationQ1000 = glGetUniformLocation(gProgram, "uAmbientColor");
    gSunlightColorLocationQ1000 = glGetUniformLocation(gProgram, "uSunlightColor");
    gSunDirectionLocationQ1000 = glGetUniformLocation(gProgram, "uSunDirection");
    gEyePositionLocationQ1010 = glGetUniformLocation(gProgram, "uEyePosition");
    gFogColorLocationQ1010 = glGetUniformLocation(gProgram, "uFogColor");
    gFogNearLocationQ1010 = glGetUniformLocation(gProgram, "uFogNear");
    gFogFarLocationQ1010 = glGetUniformLocation(gProgram, "uFogFar");
    gFogPowerLocationQ1410 = glGetUniformLocation(gProgram, "uFogPower");
    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, "uQ1450FogEnabled");
    gRenderStageLocationQ1560 = glGetUniformLocation(gProgram, "uRenderStageQ1560");
    gLegacyColourDomainLocationQ1570 = glGetUniformLocation(gProgram, "uLegacyColourDomainQ1570");
    gLegacyAmbientLocationQ1570 = glGetUniformLocation(gProgram, "uLegacyAmbientQ1570");
    gLegacySunlightLocationQ1570 = glGetUniformLocation(gProgram, "uLegacySunlightQ1570");
    gFogNearVertexLocationQ1532 = glGetUniformLocation(gProgram, "uFogNearVertexQ1532");
    gFogFarVertexLocationQ1532 = glGetUniformLocation(gProgram, "uFogFarVertexQ1532");
    gFogPowerVertexLocationQ1532 = glGetUniformLocation(gProgram, "uFogPowerVertexQ1532");
    gSunDirectionVertexLocationQ1540 = glGetUniformLocation(gProgram, "uSunDirectionVertexQ1540");
    gEyePositionVertexLocationQ1630 = glGetUniformLocation(gProgram, "uEyePositionVertexQ1630");
    gPpDiffuseDomainLocationQ1470 = glGetUniformLocation(gProgram, "uQ1470LegacyPpDiffuseDomain");
    gLocalLightCountLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightCount");
    gLocalLightPosRadiusLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightPosRadius[0]");
    gLocalLightColorFalloffLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightColorFalloff[0]");

    LoadFo3ExternalEmittanceQ1380(0x00000A74u);
    gObjects.reserve(selected.size());
    size_t q1860GpuPumpCount = 0u;
    for (CpuObject& cpu : selected) {
        PumpFo3AndroidEventsQ1860();
        ++q1860GpuPumpCount;
        GpuObject gpu;
        if (UploadCpuObject(cpu, centerX, centerY, floorZ, gpu)) {
            gObjects.push_back(std::move(gpu));
        }
    }

    Q6H_LOGI("Q13.8 EMITTANCE GPU: nifFlagShapes=%zu resolvedShapes=%zu fixedLIGHShapes=%zu regionDayShapes=%zu shaderFlag=0x%08X dayEndpoint=1 authoredOnly=1 effectsFolderStillDeferred=1",
             gQ1380ExternalFlagShapes, gQ1380ExternalResolvedShapes,
             gQ1380ExternalFixedShapes, gQ1380ExternalRegionShapes,
             fo3emittanceq1380::EXTERNAL_EMITTANCE_SHADER_FLAG);
    gSceneReady = gObjects.size() >= 2u;
    if (gSceneReady) {
        size_t realDiffuse = 0, realNormal = 0;
        size_t triangles = 0, alphaBlend = 0, alphaTest = 0;
        size_t q1020NoLighting = 0, q1020VertexColor = 0, q1020Glow = 0,
               q1020Specular = 0, q1020Emissive = 0;
        for (const GpuObject& object : gObjects) {
            if (object.realDiffuse) ++realDiffuse;
            if (object.realNormal) ++realNormal;
            if (object.alphaBlend) ++alphaBlend;
            if (object.alphaTest) ++alphaTest;
            if (object.noLighting) ++q1020NoLighting;
            if (object.useVertexColor) ++q1020VertexColor;
            if (object.realGlow) ++q1020Glow;
            if (object.specularEnabled) ++q1020Specular;
            if (object.emissiveMult > 0.0f && (object.emissiveColor[0] != 0.0f || object.emissiveColor[1] != 0.0f || object.emissiveColor[2] != 0.0f)) ++q1020Emissive;
            triangles += static_cast<size_t>(object.vertexCount / 3);
        }
        Q6H_LOGI("Q10.2 MATERIAL READY: drawShapes=%zu noLighting=%zu vertexColor=%zu glowMaps=%zu specular=%zu emissive=%zu", gObjects.size(), q1020NoLighting, q1020VertexColor, q1020Glow, q1020Specular, q1020Emissive);
        Q6H_LOGI("Q13.8 EMITTANCE GPU: nifFlagShapes=%zu resolvedShapes=%zu fixedLIGHShapes=%zu regionDayShapes=%zu shaderFlag=0x%08X dayEndpoint=1 authoredOnly=1 effectsFolderStillDeferred=1",
                 gQ1380ExternalFlagShapes, gQ1380ExternalResolvedShapes,
                 gQ1380ExternalFixedShapes, gQ1380ExternalRegionShapes,
                 fo3emittanceq1380::EXTERNAL_EMITTANCE_SHADER_FLAG);

        Q6H_LOGI("Q6H READY: ESM->REFR->BASE->MODL->BSA->NIF objects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu uniqueTextures=%zu alphaBlend=%zu alphaTest=%zu cell=MegatonPlayerHouse",
                 gObjects.size(), triangles, realDiffuse, realNormal, gTextureCache.size(), alphaBlend, alphaTest);
    } else {
        Q6H_LOGE("Q6H FAILED: GPU scene objects=%zu", gObjects.size());
    }
    return gSceneReady;
}

bool Q74ShouldSkipPlacement(const Fo3WorldPlacement& placement) {
    std::string edid = placement.editorId;
    std::string model = placement.modelPath;
    for (char& ch : edid) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    for (char& ch : model) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (placement.baseRecordType == "IDLM") return true;
    if (model.rfind("effects\\", 0) == 0) return true;
    if (edid == "xmarker" || edid == "xmarkerheading") return true;
    if (edid.find("marker") != std::string::npos) return true;
    if (edid == "counterlean" || edid == "barkeep") return true;
    return false;
}

void Q74DeleteGpuObjects(std::vector<GpuObject>& objects) {
    for (GpuObject& object : objects) {
        if (object.vbo) glDeleteBuffers(1, &object.vbo);
        if (object.vao) glDeleteVertexArrays(1, &object.vao);
        object.vbo = 0;
        object.vao = 0;
    }
    objects.clear();
}

bool ProcessQ74TransitionRequest() {
    Fo3CellTransitionRequestQ74 request;
    if (!ConsumeFo3CellTransitionRequestQ74(request) || !request.valid) return false;

    Q6H_LOGI("Q16.11 MATURE SCENE SWAP BEGIN: door=%08X cell=%08X worldspace=%08X XTEL=(%.2f %.2f %.2f)",
             request.destinationDoorRef, request.cellFormId, request.worldspaceFormId,
             request.x, request.y, request.z);

    std::vector<Fo3WorldPlacement> placements;
    if (!LoadFo3CellPlacementsQ74(request.cellFormId, placements)) {
        Q6H_LOGE("Q7.4 SCENE SWAP FAILED: cell=%08X reason=cell-loader oldSceneRetained=1",
                 request.cellFormId);
        return false;
    }

    std::vector<CpuObject> selected;
    selected.reserve(placements.size() * 2u);
    size_t skipped = 0u;
    size_t unsupported = 0u;
    size_t q1860CpuPumpCount = 0u;
    for (const Fo3WorldPlacement& placement : placements) {
        PumpFo3AndroidEventsQ1860();
        ++q1860CpuPumpCount;
        if (Q74ShouldSkipPlacement(placement)) {
            ++skipped;
            continue;
        }
        std::vector<CpuObject> parts;
        if (!BuildCpuObjects(placement, parts)) {
            ++unsupported;
            continue;
        }
        for (CpuObject& part : parts) selected.push_back(std::move(part));
    }
    if (selected.size() < 2u) {
        Q6H_LOGE("Q7.4 SCENE SWAP FAILED: cell=%08X drawShapes=%zu skipped=%zu unsupported=%zu oldSceneRetained=1",
                 request.cellFormId, selected.size(), skipped, unsupported);
        return false;
    }

    std::vector<GpuObject> replacement;
    replacement.reserve(selected.size());
    size_t q1860GpuPumpCount = 0u;
    for (CpuObject& cpu : selected) {
        PumpFo3AndroidEventsQ1860();
        ++q1860GpuPumpCount;
        GpuObject gpu;
        if (UploadCpuObject(cpu, request.x, request.y, request.z, gpu)) {
            replacement.push_back(std::move(gpu));
        } else {
            if (gpu.vbo) glDeleteBuffers(1, &gpu.vbo);
            if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
        }
    }
    if (replacement.size() < 2u) {
        Q74DeleteGpuObjects(replacement);
        Q6H_LOGE("Q7.4 SCENE SWAP FAILED: cell=%08X gpuObjects=%zu oldSceneRetained=1",
                 request.cellFormId, replacement.size());
        return false;
    }

    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(selected.size());
    for (const CpuObject& cpu : selected) collisionPlacements.push_back(cpu.placement);

    Q6H_LOGI("Q16.14 MATURE LOAD PUMP: phase=cpu-gpu-complete cpuBoundaries=%zu gpuBoundaries=%zu collisionPlacements=%zu",
             q1860CpuPumpCount, q1860GpuPumpCount, collisionPlacements.size());
    PumpFo3AndroidEventsQ1860();
    // Q16.27: initial Capital Wasteland visuals are 7x7, but initial physics is
    // deliberately local 3x3. Other worldspaces keep their established policy.
    std::vector<Fo3WorldPlacement> q1960InitialCollisionPlacements;
    const std::vector<Fo3WorldPlacement>* q1960CollisionSource = &collisionPlacements;
    size_t q1960OutsideInitialCollisionWindow = 0u;
    if (request.worldspaceFormId == 0x0000003Cu) {
        Q1970ProbeNativeLod(request.x, request.y);
        Q1990EnsureNativeLodForCell(
            static_cast<int32_t>(std::floor(request.x / Q1890_EXTERIOR_CELL_SIZE)),
            static_cast<int32_t>(std::floor(request.y / Q1890_EXTERIOR_CELL_SIZE)),
            request.x, request.y, request.z);
        constexpr float Q1960_CELL_SIZE = 4096.0f;
        constexpr int Q1960_INITIAL_COLLISION_RADIUS = 1;
        const int32_t q1960TargetGridX = static_cast<int32_t>(
            std::floor(request.x / Q1960_CELL_SIZE));
        const int32_t q1960TargetGridY = static_cast<int32_t>(
            std::floor(request.y / Q1960_CELL_SIZE));
        q1960InitialCollisionPlacements.reserve(collisionPlacements.size());
        for (const Fo3WorldPlacement& placement : collisionPlacements) {
            const int32_t q1960PlacementGridX = static_cast<int32_t>(
                std::floor(placement.x / Q1960_CELL_SIZE));
            const int32_t q1960PlacementGridY = static_cast<int32_t>(
                std::floor(placement.y / Q1960_CELL_SIZE));
            if (std::abs(q1960PlacementGridX - q1960TargetGridX) >
                    Q1960_INITIAL_COLLISION_RADIUS ||
                std::abs(q1960PlacementGridY - q1960TargetGridY) >
                    Q1960_INITIAL_COLLISION_RADIUS) {
                ++q1960OutsideInitialCollisionWindow;
                continue;
            }
            q1960InitialCollisionPlacements.push_back(placement);
        }
        q1960CollisionSource = &q1960InitialCollisionPlacements;
        SetNextFo3CollisionExteriorModeQ1931(true);
        Q6H_LOGI("Q16.27 INITIAL COLLISION WINDOW: worldspace=%08X targetGrid=(%d,%d) visualCollisionCandidates=%zu localCollisionPlacements=%zu outside3x3=%zu radius=1",
                 request.worldspaceFormId,
                 q1960TargetGridX, q1960TargetGridY,
                 collisionPlacements.size(), q1960InitialCollisionPlacements.size(),
                 q1960OutsideInitialCollisionWindow);
    }
    const bool collisionReady = InitializeFo3CollisionOverlay(*q1960CollisionSource,
                                                               request.x, request.y, request.z,
                                                               SCENE_FORWARD, FLOOR_Y,
                                                               FO3_UNITS_PER_METRE);
    PumpFo3AndroidEventsQ1860();

    const size_t oldObjects = gObjects.size();
    Q74DeleteGpuObjects(gObjects);
    gObjects = std::move(replacement);
    gSceneReady = !gObjects.empty();
    gSceneCenterXQ1730 = request.x;
    gSceneCenterYQ1730 = request.y;
    gSceneFloorZQ1730 = request.z;
    gLoggedFirstDraw = false;
    gShadowDirtyQ1050 = true;
    gShadowReadyQ1050 = false;
    SetFo3TerrainShadowQ1050(gShadowDepthQ1050,gLightMvpQ1050,false);

    size_t triangles = 0u;
    size_t realDiffuse = 0u;
    size_t realNormal = 0u;
    for (const GpuObject& object : gObjects) {
        triangles += static_cast<size_t>(object.vertexCount / 3);
        if (object.realDiffuse) ++realDiffuse;
        if (object.realNormal) ++realNormal;
    }

    CompleteFo3CellTransitionQ74(request.cellFormId);
    gCurrentCellFormId = request.cellFormId;
    PrimeFo3AuthoredDoorAnchorsQ1870(gCurrentCellFormId);

    if (request.worldspaceFormId != 0u) {
        gExteriorStreamingActiveQ1890 = true;
        gExteriorWorldspaceQ1890 = request.worldspaceFormId;
        gExteriorPersistentCellQ1890 = request.cellFormId;
        gExteriorOriginXQ1890 = request.x;
        gExteriorOriginYQ1890 = request.y;
        gExteriorOriginZQ1890 = request.z;
        gExteriorWindowGridXQ1890 = static_cast<int32_t>(
            std::floor(request.x / Q1890_EXTERIOR_CELL_SIZE));
        gExteriorWindowGridYQ1890 = static_cast<int32_t>(
            std::floor(request.y / Q1890_EXTERIOR_CELL_SIZE));
        gExteriorWindowGenerationQ1890 = 0u;
        Q6H_LOGI("Q16.27 STREAM CONTEXT: active=1 worldspace=%08X persistent=%08X grid=(%d,%d) originXTEL=(%.2f %.2f %.2f) residentRadius=2 activeRadius=1 collisionRadius=1 terrainRadius=3 rebuildMode=5x5-resident+3x3-active+cached-3x3-collision+7x7-terrain+Level4-LOD",
                 gExteriorWorldspaceQ1890, gExteriorPersistentCellQ1890,
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 gExteriorOriginXQ1890, gExteriorOriginYQ1890,
                 gExteriorOriginZQ1890);
    } else {
        gExteriorStreamingActiveQ1890 = false;
        gExteriorWorldspaceQ1890 = 0u;
        gExteriorPersistentCellQ1890 = 0u;
        Q6H_LOGI("Q16.27 STREAM CONTEXT: active=0 reason=interior");
    }
    LoadFo3CellEnvironmentQ1410(request.cellFormId, request.worldspaceFormId,
                                request.x, request.y);
    // Rebuild Q14.0 from the corrected region weather/XCIM on the first frame.
    fo3todq1400::gRuntime = {};
    fo3todq1400::gAppliedOnce = false;
    fo3todq1400::gLastPredictedNs = 0;
    fo3todq1400::gLastLoggedHour = -1;
    fo3todq1400::gLastLoggedPhase.clear();
    Q6H_LOGI("Q7.4 SCENE SWAP READY: cell=%08X worldspace=%08X oldObjects=%zu newObjects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu skippedMarkersEffects=%zu unsupported=%zu collisionReady=%d XTELorigin=1 orientationPreserved=1",
             request.cellFormId, request.worldspaceFormId,
             oldObjects, gObjects.size(), triangles, realDiffuse, realNormal,
             skipped, unsupported, collisionReady ? 1 : 0);
    return true;
}

GLenum Q1150BlendFactor(uint8_t mode, bool source) {
    switch (mode) {
        case 0u: return GL_ONE;
        case 1u: return GL_ZERO;
        case 2u: return GL_SRC_COLOR;
        case 3u: return GL_ONE_MINUS_SRC_COLOR;
        case 4u: return GL_DST_COLOR;
        case 5u: return GL_ONE_MINUS_DST_COLOR;
        case 6u: return GL_SRC_ALPHA;
        case 7u: return GL_ONE_MINUS_SRC_ALPHA;
        case 8u: return GL_DST_ALPHA;
        case 9u: return GL_ONE_MINUS_DST_ALPHA;
        case 10u: return GL_SRC_ALPHA_SATURATE;
        default: return source ? GL_SRC_ALPHA : GL_ONE_MINUS_SRC_ALPHA;
    }
}

struct Q1580NdotLStats {
    size_t samples = 0u;
    size_t zero = 0u;
    size_t below10 = 0u;
    size_t below25 = 0u;
    double sum = 0.0;
    float maximum = 0.0f;
};

void Q1580AddNdotL(Q1580NdotLStats& stats, const Vec3& normal, const Vec3& light) {
    const float ndotl = std::max(normal.x * light.x + normal.y * light.y + normal.z * light.z, 0.0f);
    ++stats.samples;
    if (ndotl <= 0.0001f) ++stats.zero;
    if (ndotl < 0.10f) ++stats.below10;
    if (ndotl < 0.25f) ++stats.below25;
    stats.sum += ndotl;
    stats.maximum = std::max(stats.maximum, ndotl);
}

float Q1580Mean(const Q1580NdotLStats& stats) {
    return stats.samples ? static_cast<float>(stats.sum / static_cast<double>(stats.samples)) : 0.0f;
}
float Q1580Pct(size_t part, size_t whole) {
    return whole ? 100.0f * static_cast<float>(part) / static_cast<float>(whole) : 0.0f;
}
bool Q1580MegatonArchitecture(const GpuObject& object) {
    return object.modelPath.find("megaton") != std::string::npos ||
           object.modelPath.find("Megaton") != std::string::npos;
}

void Q1580LogLightTrace(const Fo3EnvironmentQ1000& env) {
    static bool q1580Logged = false;
    if (q1580Logged || !env.valid || env.worldspaceFormId != 0x00000A74u ||
        !fo3todq1400::gRuntime.ready || gObjects.empty()) return;

    const Vec3 uploaded = Normalize({env.sunDirection[0], env.sunDirection[1], env.sunDirection[2]});
    const Vec3 converted = GameDirectionToOpenXr({env.sunDirection[0], env.sunDirection[1], env.sunDirection[2]});
    const Vec3 inverted{-uploaded.x, -uploaded.y, -uploaded.z};

    Q1580NdotLStats allA, allB, allInv, archA, archB, archInv;
    size_t litObjects = 0u, archObjects = 0u, details = 0u;
    for (const GpuObject& object : gObjects) {
        if (object.noLighting || object.q1580NormalSamples.empty()) continue;
        ++litObjects;
        const bool arch = Q1580MegatonArchitecture(object);
        if (arch) ++archObjects;
        Q1580NdotLStats objA, objB, objInv;
        for (const Vec3& normal : object.q1580NormalSamples) {
            Q1580AddNdotL(allA, normal, uploaded);
            Q1580AddNdotL(allB, normal, converted);
            Q1580AddNdotL(allInv, normal, inverted);
            Q1580AddNdotL(objA, normal, uploaded);
            Q1580AddNdotL(objB, normal, converted);
            Q1580AddNdotL(objInv, normal, inverted);
            if (arch) {
                Q1580AddNdotL(archA, normal, uploaded);
                Q1580AddNdotL(archB, normal, converted);
                Q1580AddNdotL(archInv, normal, inverted);
            }
        }
        if (arch && details < 12u) {
            ++details;
            Q6H_LOGI("Q15.8 LIGHT TRACE OBJECT: ref=%08X base=%08X model=%s samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) converted(mean=%.3f zero=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f)",
                     object.refFormId, object.baseFormId, object.modelPath.c_str(), objA.samples,
                     Q1580Mean(objA), Q1580Pct(objA.zero,objA.samples), Q1580Pct(objA.below10,objA.samples), Q1580Pct(objA.below25,objA.samples), objA.maximum,
                     Q1580Mean(objB), Q1580Pct(objB.zero,objB.samples), objB.maximum,
                     Q1580Mean(objInv), Q1580Pct(objInv.zero,objInv.samples), objInv.maximum);
        }
    }

    Q6H_LOGI("Q15.8 LIGHT TRACE HEADER: world=%08X climate=%08X weather=%08X EDID=%s hour=%.2f uploadedSun=(%.6f %.6f %.6f) convertedCandidate=(%.6f %.6f %.6f) pcSP17LightDataUnaligned=(-0.684054 0.286799 0.670684) ambient=(%.6f %.6f %.6f) sunlight=(%.6f %.6f %.6f)",
             env.worldspaceFormId, env.climateFormId, env.weatherFormId,
             env.weatherEditorId.empty()?"<none>":env.weatherEditorId.c_str(), GetFo3TestHourQ1400(),
             uploaded.x, uploaded.y, uploaded.z, converted.x, converted.y, converted.z,
             env.ambient[0], env.ambient[1], env.ambient[2], env.sunlight[0], env.sunlight[1], env.sunlight[2]);
    Q6H_LOGI("Q15.8 LIGHT TRACE SUMMARY: litObjects=%zu samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) converted(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f)",
             litObjects, allA.samples,
             Q1580Mean(allA),Q1580Pct(allA.zero,allA.samples),Q1580Pct(allA.below10,allA.samples),Q1580Pct(allA.below25,allA.samples),allA.maximum,
             Q1580Mean(allB),Q1580Pct(allB.zero,allB.samples),Q1580Pct(allB.below10,allB.samples),Q1580Pct(allB.below25,allB.samples),allB.maximum,
             Q1580Mean(allInv),Q1580Pct(allInv.zero,allInv.samples),allInv.maximum);
    Q6H_LOGI("Q15.8 LIGHT TRACE ARCH: architectureObjects=%zu samples=%zu uploaded(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) converted(mean=%.3f zero=%.1f%% lt10=%.1f%% lt25=%.1f%% max=%.3f) inverted(mean=%.3f zero=%.1f%% max=%.3f) renderChanged=0",
             archObjects, archA.samples,
             Q1580Mean(archA),Q1580Pct(archA.zero,archA.samples),Q1580Pct(archA.below10,archA.samples),Q1580Pct(archA.below25,archA.samples),archA.maximum,
             Q1580Mean(archB),Q1580Pct(archB.zero,archB.samples),Q1580Pct(archB.below10,archB.samples),Q1580Pct(archB.below25,archB.samples),archB.maximum,
             Q1580Mean(archInv),Q1580Pct(archInv.zero,archInv.samples),archInv.maximum);
    q1580Logged = true;
}

bool Q1590GetPcSp17LightConstants(float ambient[3], float sunlight[3],
                                  float& baseSunDimmer, float& effectiveSunScale) {
    using namespace fo3todq1400;
    if (!gRuntime.ready || !gRuntime.weather.haveNam0 || !gRuntime.climate.valid) {
        return false;
    }

    const TimeWeightsQ1400 weights = WeightsForHour(gTestHour, gRuntime.climate);
    float rawAmbient[3]{0.0f, 0.0f, 0.0f};
    float rawSunlight[3]{0.0f, 0.0f, 0.0f};
    for (int tod = 0; tod < 4; ++tod) {
        const float w = weights.w[tod];
        for (int c = 0; c < 3; ++c) {
            rawAmbient[c] += gRuntime.weather.encoded[3][tod][c] * w;
            rawSunlight[c] += gRuntime.weather.encoded[4][tod][c] * w;
        }
    }

    // Q15.11 PC reference capture: fSunlightDimmer=1.5 -> PSLightColor x2.5.
    baseSunDimmer = 1.5f;
    effectiveSunScale = 2.5f;
    for (int c = 0; c < 3; ++c) {
        ambient[c] = rawAmbient[c];
        sunlight[c] = rawSunlight[c] * effectiveSunScale;
    }
    return true;
}

void Q1590UploadPcSp17LightConstants() {
    float ambient[3]{0.34f, 0.34f, 0.34f};
    float sunlight[3]{0.66f, 0.66f, 0.66f};
    float baseSunDimmer = 0.0f;
    float effectiveSunScale = 1.0f;
    if (!Q1590GetPcSp17LightConstants(
            ambient, sunlight, baseSunDimmer, effectiveSunScale)) {
        return;
    }

    if (gAmbientColorLocationQ1000 >= 0) {
        glUniform3fv(gAmbientColorLocationQ1000, 1, ambient);
    }
    if (gSunlightColorLocationQ1000 >= 0) {
        glUniform3fv(gSunlightColorLocationQ1000, 1, sunlight);
    }

    // Q15.7's encoded-BaseMap branch changes more than the PC capture proves.
    // Keep it disabled for statics so this test changes only the two proven
    // SP17 lighting constants. Terrain remains untouched by Q15.9.
    if (gLegacyColourDomainLocationQ1570 >= 0) {
        glUniform1f(gLegacyColourDomainLocationQ1570, 0.0f);
    }

    static uint32_t q1590LastWeather = 0u;
    static int q1590LastHour = -1;
    const int hourBucket = static_cast<int>(std::floor(fo3todq1400::gTestHour * 10.0f));
    if (q1590LastWeather != fo3todq1400::gRuntime.weatherFormId ||
        q1590LastHour != hourBucket) {
        q1590LastWeather = fo3todq1400::gRuntime.weatherFormId;
        q1590LastHour = hourBucket;
        Q6H_LOGI(
            "Q15.13 SP17 CORE: weather=%08X EDID=%s hour=%.2f ambient=(%.6f %.6f %.6f) sunlight=(%.6f %.6f %.6f) baseSunDimmer=%.3f effectiveSunScale=%.3f scope=static-PPLighting core=PC_DP3_DIFFUSE_TANGENT_HALF_SPEC normalAlphaSpec=1 lowNdotLSpecGate=1 syntheticSpec032=0 baseMap=GL_SRGB8_ALPHA8_DECODE terrainChanged=0 sunScaleCaptured=2.5",
            fo3todq1400::gRuntime.weatherFormId,
            fo3todq1400::gRuntime.weather.editorId.empty()
                ? "<none>" : fo3todq1400::gRuntime.weather.editorId.c_str(),
            fo3todq1400::gTestHour,
            ambient[0], ambient[1], ambient[2],
            sunlight[0], sunlight[1], sunlight[2],
            baseSunDimmer, effectiveSunScale);
    }
}

bool RayAabbQ7(float ox, float oy, float oz,
               float dx, float dy, float dz,
               const GpuObject& object, float& outT) {
    const bool q1740MegatonMainGate =
        object.editorId == "MegatonMainGate01" ||
        object.modelPath.find("MegatonMainGate01") != std::string::npos ||
        object.modelPath.find("megatonmaingate01") != std::string::npos;
    const float PAD = q1740MegatonMainGate ? 6.50f : 0.08f;
    const float mins[3]{object.minX - PAD, object.minY - PAD, object.minZ - PAD};
    const float maxs[3]{object.maxX + PAD, object.maxY + PAD, object.maxZ + PAD};
    const float origins[3]{ox, oy, oz};
    const float dirs[3]{dx, dy, dz};
    float tMin = 0.0f;
    float tMax = 3.0f;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(dirs[axis]) < 1e-6f) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return false;
            continue;
        }
        float t1 = (mins[axis] - origins[axis]) / dirs[axis];
        float t2 = (maxs[axis] - origins[axis]) / dirs[axis];
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }
    outT = tMin;
    return tMax >= 0.0f && tMin <= 3.0f;
}

bool QueryDoorInternalQ1700(float ox, float oy, float oz,
                            float dx, float dy, float dz,
                            Fo3DoorAimQ1700* outAim) {
    if (outAim) *outAim = {};
    if (!gSceneReady || IsFo3LoadingVisibleQ1700()) return false;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f) return false;
    dx /= len; dy /= len; dz /= len;

    const GpuObject* hit = nullptr;
    float bestT = 3.0f;
    for (const GpuObject& object : gObjects) {
        if (object.baseRecordType != "DOOR" || !object.teleport.valid) continue;
        float t = 0.0f;
        if (RayAabbQ7(ox, oy, oz, dx, dy, dz, object, t) && t < bestT) {
            bestT = t;
            hit = &object;
        }
    }
    if (!hit) {
        Fo3DoorAimQ1700 authoredAim;
        if (QueryFo3AuthoredDoorAnchorQ1730(
                gCurrentCellFormId,
                gSceneCenterXQ1730, gSceneCenterYQ1730, gSceneFloorZQ1730,
                FO3_UNITS_PER_METRE, FLOOR_Y, SCENE_FORWARD,
                ox, oy, oz, dx, dy, dz, &authoredAim) && authoredAim.valid) {
            if (outAim) *outAim = authoredAim;
            Q6H_LOGI("Q16.4 DOOR AIM FALLBACK: cell=%08X sourceDoor=%08X destinationDoor=%08X distance=%.2f source=ESM_XTEL_ANCHOR",
                     gCurrentCellFormId, authoredAim.sourceDoorRef,
                     authoredAim.destinationDoorRef, authoredAim.distance);
            return true;
        }
        return false;
    }

    if (outAim) {
        outAim->valid = true;
        outAim->sourceDoorRef = hit->refFormId;
        outAim->destinationDoorRef = hit->teleport.destinationDoorRefFormId;
        outAim->distance = bestT;
        outAim->x = hit->teleport.x;
        outAim->y = hit->teleport.y;
        outAim->z = hit->teleport.z;
        outAim->rx = hit->teleport.rx;
        outAim->ry = hit->teleport.ry;
        outAim->rz = hit->teleport.rz;
    }
    return true;
}

bool ActivateDoorInternalQ1700(float ox, float oy, float oz,
                               float dx, float dy, float dz) {
    Fo3DoorAimQ1700 aim;
    if (!QueryDoorInternalQ1700(ox, oy, oz, dx, dy, dz, &aim) || !aim.valid) {
        Q6H_LOGI("Q16.0 ACTIVATE MISS: maxDistance=3.0m");
        return false;
    }
    if (!QueueFo3DoorTransitionQ1700(aim.sourceDoorRef,
                                     aim.destinationDoorRef,
                                     aim.x, aim.y, aim.z,
                                     aim.rx, aim.ry, aim.rz)) {
        return false;
    }
    Q6H_LOGI("Q16.0 DOOR ACTIVATE: sourceDoor=%08X destinationDoor=%08X distance=%.2f input=RIGHT_A queue=Q7.4",
             aim.sourceDoorRef, aim.destinationDoorRef, aim.distance);
    return true;
}

bool ActivateDoorInternalQ7(float ox, float oy, float oz,
                            float dx, float dy, float dz) {
    return ActivateDoorInternalQ1700(ox, oy, oz, dx, dy, dz);
}

enum Q1800TransitionStage {
    Q1800_STAGE_IDLE = 0,
    Q1800_STAGE_LOAD_PLACEMENTS = 1,
    Q1800_STAGE_BUILD_CPU = 2,
    Q1800_STAGE_UPLOAD_GPU = 3,
    Q1800_STAGE_COLLISION = 4,
    Q1800_STAGE_SWAP = 5,
};

struct Q1800TransitionWork {
    Q1800TransitionStage stage = Q1800_STAGE_IDLE;
    Fo3CellTransitionRequestQ74 request{};
    std::vector<Fo3WorldPlacement> placements;
    std::vector<CpuObject> selected;
    std::vector<GpuObject> replacement;
    size_t placementIndex = 0u;
    size_t uploadIndex = 0u;
    size_t skipped = 0u;
    size_t unsupported = 0u;
    bool collisionReady = false;
};

Q1800TransitionWork gQ1800TransitionWork;

void Q1800AbortTransition(const char* reason) {
    Q74DeleteGpuObjects(gQ1800TransitionWork.replacement);
    Q6H_LOGE("Q16.10 PHASED LOAD FAILED: stage=%d cell=%08X reason=%s oldSceneRetained=1",
             static_cast<int>(gQ1800TransitionWork.stage),
             gQ1800TransitionWork.request.cellFormId,
             reason ? reason : "unknown");
    CancelFo3LoadingQ1700();
    gQ1800TransitionWork = {};
}

bool ProcessQ74TransitionRequestExperimentalQ1800() {
    Q1800TransitionWork& work = gQ1800TransitionWork;

    if (work.stage == Q1800_STAGE_IDLE) {
        Fo3CellTransitionRequestQ74 request;
        if (!ConsumeFo3CellTransitionRequestQ74(request) || !request.valid) return false;

        work = {};
        work.request = request;
        work.stage = Q1800_STAGE_LOAD_PLACEMENTS;
        MarkFo3TransitionWorkStartedQ1700();
        Q6H_LOGI("Q16.10 PHASED LOAD BEGIN: door=%08X cell=%08X worldspace=%08X loadingFrames=%u",
                 request.destinationDoorRef, request.cellFormId, request.worldspaceFormId,
                 GetFo3LoadingPresentedFramesQ1700());
        return true;
    }

    if (work.stage == Q1800_STAGE_LOAD_PLACEMENTS) {
        Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=placements begin",
                 work.request.cellFormId);
        if (!LoadFo3CellPlacementsQ74(work.request.cellFormId, work.placements)) {
            Q1800AbortTransition("cell-loader");
            return false;
        }
        work.selected.reserve(work.placements.size() * 2u);
        work.stage = Q1800_STAGE_BUILD_CPU;
        Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=placements ready count=%zu",
                 work.request.cellFormId, work.placements.size());
        return true;
    }

    if (work.stage == Q1800_STAGE_BUILD_CPU) {
        const auto sliceStart = std::chrono::steady_clock::now();
        size_t processedThisSlice = 0u;
        while (work.placementIndex < work.placements.size()) {
            const Fo3WorldPlacement& placement = work.placements[work.placementIndex++];
            ++processedThisSlice;

            if (Q74ShouldSkipPlacement(placement)) {
                ++work.skipped;
            } else {
                std::vector<CpuObject> parts;
                if (!BuildCpuObjects(placement, parts)) {
                    ++work.unsupported;
                } else {
                    for (CpuObject& part : parts) work.selected.push_back(std::move(part));
                }
            }

            const float elapsedMs = std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - sliceStart).count();
            if (processedThisSlice >= 12u || elapsedMs >= 3.0f) break;
        }

        if (work.placementIndex >= work.placements.size()) {
            if (work.selected.size() < 2u) {
                Q1800AbortTransition("cpu-build-too-small");
                return false;
            }
            work.replacement.reserve(work.selected.size());
            work.stage = Q1800_STAGE_UPLOAD_GPU;
            Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=cpu ready drawShapes=%zu skipped=%zu unsupported=%zu",
                     work.request.cellFormId, work.selected.size(),
                     work.skipped, work.unsupported);
        }
        return true;
    }

    if (work.stage == Q1800_STAGE_UPLOAD_GPU) {
        const auto sliceStart = std::chrono::steady_clock::now();
        size_t uploadedThisSlice = 0u;
        while (work.uploadIndex < work.selected.size()) {
            CpuObject& cpu = work.selected[work.uploadIndex++];
            GpuObject gpu;
            if (UploadCpuObject(cpu,
                                work.request.x, work.request.y, work.request.z,
                                gpu)) {
                work.replacement.push_back(std::move(gpu));
            } else {
                if (gpu.vbo) glDeleteBuffers(1, &gpu.vbo);
                if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
            }
            ++uploadedThisSlice;

            const float elapsedMs = std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - sliceStart).count();
            if (uploadedThisSlice >= 8u || elapsedMs >= 3.0f) break;
        }

        if (work.uploadIndex >= work.selected.size()) {
            if (work.replacement.size() < 2u) {
                Q1800AbortTransition("gpu-upload-too-small");
                return false;
            }
            work.stage = Q1800_STAGE_COLLISION;
            Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=gpu ready objects=%zu",
                     work.request.cellFormId, work.replacement.size());
        }
        return true;
    }

    if (work.stage == Q1800_STAGE_COLLISION) {
        std::vector<Fo3WorldPlacement> collisionPlacements;
        collisionPlacements.reserve(work.selected.size());
        for (const CpuObject& cpu : work.selected) {
            collisionPlacements.push_back(cpu.placement);
        }

        work.collisionReady = InitializeFo3CollisionOverlay(
            collisionPlacements,
            work.request.x, work.request.y, work.request.z,
            SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
        work.stage = Q1800_STAGE_SWAP;
        Q6H_LOGI("Q16.10 LOAD STAGE: cell=%08X phase=collision ready=%d",
                 work.request.cellFormId, work.collisionReady ? 1 : 0);
        return true;
    }

    if (work.stage == Q1800_STAGE_SWAP) {
        const Fo3CellTransitionRequestQ74 request = work.request;
        const size_t skipped = work.skipped;
        const size_t unsupported = work.unsupported;
        const bool collisionReady = work.collisionReady;
        const size_t oldObjects = gObjects.size();

        Q74DeleteGpuObjects(gObjects);
        gObjects = std::move(work.replacement);
        gSceneReady = !gObjects.empty();
        gLoggedFirstDraw = false;

        size_t triangles = 0u;
        size_t realDiffuse = 0u;
        size_t realNormal = 0u;
        for (const GpuObject& object : gObjects) {
            triangles += static_cast<size_t>(object.vertexCount / 3);
            if (object.realDiffuse) ++realDiffuse;
            if (object.realNormal) ++realNormal;
        }

        // Q16.0's completion hook advances WORK -> POST and requests the player
        // origin reset. POST remains visible until one destination frame reaches
        // xrEndFrame, so the new world cannot tear through mid-frame.
        CompleteFo3CellTransitionQ74(request.cellFormId);
        gCurrentCellFormId = request.cellFormId;

        Q6H_LOGI("Q16.10 PHASED LOAD COMPLETE: cell=%08X worldspace=%08X oldObjects=%zu newObjects=%zu triangles=%zu realDiffuse=%zu realNormal=%zu skipped=%zu unsupported=%zu collisionReady=%d loading=POST",
                 request.cellFormId, request.worldspaceFormId,
                 oldObjects, gObjects.size(), triangles, realDiffuse, realNormal,
                 skipped, unsupported, collisionReady ? 1 : 0);

        work = {};
        return true;
    }

    Q1800AbortTransition("invalid-stage");
    return false;
}

constexpr float Q1890_EXTERIOR_CELL_SIZE = 4096.0f;
constexpr float Q1890_BOUNDARY_HYSTERESIS = 96.0f;
bool gExteriorStreamingActiveQ1890 = false;
bool gExteriorStreamBusyQ1890 = false;
uint32_t gExteriorWorldspaceQ1890 = 0u;
uint32_t gExteriorPersistentCellQ1890 = 0u;
float gExteriorOriginXQ1890 = 0.0f;
float gExteriorOriginYQ1890 = 0.0f;
float gExteriorOriginZQ1890 = 0.0f;
int32_t gExteriorWindowGridXQ1890 = 0;
int32_t gExteriorWindowGridYQ1890 = 0;
uint64_t gExteriorWindowGenerationQ1890 = 0u;

bool Q1890InsideCandidatePastHysteresis(float gameX, float gameY,
                                        int32_t oldX, int32_t oldY,
                                        int32_t newX, int32_t newY) {
    const float localX = gameX - static_cast<float>(newX) * Q1890_EXTERIOR_CELL_SIZE;
    const float localY = gameY - static_cast<float>(newY) * Q1890_EXTERIOR_CELL_SIZE;
    if (newX > oldX && localX < Q1890_BOUNDARY_HYSTERESIS) return false;
    if (newX < oldX && localX > Q1890_EXTERIOR_CELL_SIZE - Q1890_BOUNDARY_HYSTERESIS) return false;
    if (newY > oldY && localY < Q1890_BOUNDARY_HYSTERESIS) return false;
    if (newY < oldY && localY > Q1890_EXTERIOR_CELL_SIZE - Q1890_BOUNDARY_HYSTERESIS) return false;
    return true;
}

bool RebuildExteriorWindowQ1890(float selectionGameX, float selectionGameY,
                                int32_t targetGridX, int32_t targetGridY) {
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890) return false;
    gExteriorStreamBusyQ1890 = true;
    const uint64_t generation = ++gExteriorWindowGenerationQ1890;

    Q6H_LOGI("Q16.17 WINDOW BUILD BEGIN: generation=%llu worldspace=%08X persistent=%08X fromGrid=(%d,%d) toGrid=(%d,%d) selection=(%.2f %.2f) originXTEL=(%.2f %.2f %.2f)",
             static_cast<unsigned long long>(generation),
             gExteriorWorldspaceQ1890, gExteriorPersistentCellQ1890,
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             targetGridX, targetGridY, selectionGameX, selectionGameY,
             gExteriorOriginXQ1890, gExteriorOriginYQ1890, gExteriorOriginZQ1890);

    PumpFo3AndroidEventsQ1860();
    std::vector<Fo3WorldPlacement> placements;
    if (!LoadFo3WorldspaceNeighborhoodQ75(
            gExteriorWorldspaceQ1890,
            gExteriorPersistentCellQ1890,
            selectionGameX, selectionGameY,
            placements)) {
        Q6H_LOGE("Q16.17 WINDOW BUILD FAILED: generation=%llu phase=worldspace-load targetGrid=(%d,%d) oldSceneRetained=1",
                 static_cast<unsigned long long>(generation), targetGridX, targetGridY);
        gExteriorStreamBusyQ1890 = false;
        return false;
    }

    const size_t dynamicOnlyModels = ConfigureFo3CollisionPolicyQ710(placements);
    std::vector<CpuObject> selected;
    selected.reserve(placements.size() * 2u);
    size_t skipped = 0u;
    size_t unsupported = 0u;
    size_t cpuBoundaries = 0u;
    for (const Fo3WorldPlacement& placement : placements) {
        PumpFo3AndroidEventsQ1860();
        ++cpuBoundaries;
        if (Q74ShouldSkipPlacement(placement)) {
            ++skipped;
            continue;
        }
        std::vector<CpuObject> parts;
        if (!BuildCpuObjects(placement, parts)) {
            ++unsupported;
            continue;
        }
        for (CpuObject& part : parts) selected.push_back(std::move(part));
    }
    if (selected.size() < 2u) {
        Q6H_LOGE("Q16.17 WINDOW BUILD FAILED: generation=%llu phase=cpu placements=%zu shapes=%zu skipped=%zu unsupported=%zu oldSceneRetained=1",
                 static_cast<unsigned long long>(generation), placements.size(),
                 selected.size(), skipped, unsupported);
        gExteriorStreamBusyQ1890 = false;
        return false;
    }

    std::vector<GpuObject> replacement;
    replacement.reserve(selected.size());
    size_t gpuBoundaries = 0u;
    for (CpuObject& cpu : selected) {
        PumpFo3AndroidEventsQ1860();
        ++gpuBoundaries;
        GpuObject gpu;
        if (UploadCpuObject(cpu,
                            gExteriorOriginXQ1890,
                            gExteriorOriginYQ1890,
                            gExteriorOriginZQ1890,
                            gpu)) {
            replacement.push_back(std::move(gpu));
        } else {
            if (gpu.vbo) glDeleteBuffers(1, &gpu.vbo);
            if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
        }
    }
    if (replacement.size() < 2u) {
        Q74DeleteGpuObjects(replacement);
        Q6H_LOGE("Q16.17 WINDOW BUILD FAILED: generation=%llu phase=gpu shapes=%zu oldSceneRetained=1",
                 static_cast<unsigned long long>(generation), replacement.size());
        gExteriorStreamBusyQ1890 = false;
        return false;
    }

    std::vector<Fo3WorldPlacement> collisionPlacements;
    collisionPlacements.reserve(selected.size());
    for (const CpuObject& cpu : selected) collisionPlacements.push_back(cpu.placement);

    PumpFo3AndroidEventsQ1860();
    const bool collisionReady = InitializeFo3CollisionOverlay(
        collisionPlacements,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    PumpFo3AndroidEventsQ1860();

    // Select LAND around the player's NEW grid but retain the original XTEL as
    // the render/grounding origin. This is the key no-snap distinction.
    SetFo3TerrainSelectionOverrideQ1890(true, selectionGameX, selectionGameY);
    const bool terrainReady = InitializeFo3TerrainRenderQ76(
        gExteriorWorldspaceQ1890,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    SetFo3TerrainSelectionOverrideQ1890(false, 0.0f, 0.0f);
    if (terrainReady) {
        ActivateFo3TerrainGroundingQ77(
            gExteriorWorldspaceQ1890,
            gExteriorOriginXQ1890,
            gExteriorOriginYQ1890,
            gExteriorOriginZQ1890,
            SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    }
    PumpFo3AndroidEventsQ1860();

    const size_t oldObjects = gObjects.size();
    Q74DeleteGpuObjects(gObjects);
    gObjects = std::move(replacement);
    gSceneReady = !gObjects.empty();
    gLoggedFirstDraw = false;
    gExteriorWindowGridXQ1890 = targetGridX;
    gExteriorWindowGridYQ1890 = targetGridY;

    size_t triangles = 0u;
    for (const GpuObject& object : gObjects) {
        triangles += static_cast<size_t>(object.vertexCount / 3);
    }

    Q6H_LOGI("Q16.17 WINDOW BUILD READY: generation=%llu grid=(%d,%d) oldObjects=%zu newObjects=%zu triangles=%zu placements=%zu cpuBoundaries=%zu gpuBoundaries=%zu dynamicOnlyModels=%zu collisionReady=%d terrainReady=%d originPreserved=1 playerReset=0 loadingScreen=0",
             static_cast<unsigned long long>(generation),
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             oldObjects, gObjects.size(), triangles, placements.size(),
             cpuBoundaries, gpuBoundaries, dynamicOnlyModels,
             collisionReady ? 1 : 0, terrainReady ? 1 : 0);

    gExteriorStreamBusyQ1890 = false;
    return true;
}

enum class Q1900StreamPhase : uint8_t {
    Metadata = 0,
    Cpu,
    Gpu,
    Collision,
    Terrain,
    Commit,
};

struct Q1900MetadataTask {
    uint32_t worldspace = 0u;
    uint32_t persistentCell = 0u;
    float selectionGameX = 0.0f;
    float selectionGameY = 0.0f;
    std::vector<Fo3WorldPlacement> placements;
    bool success = false;
    uint64_t elapsedUs = 0u;
    std::atomic<bool> ready{false};
};

struct Q1900PendingStream {
    bool active = false;
    uint64_t generation = 0u;
    int32_t targetGridX = 0;
    int32_t targetGridY = 0;
    float selectionGameX = 0.0f;
    float selectionGameY = 0.0f;
    uint32_t sourceWorldspace = 0u;
    uint32_t sourcePersistentCell = 0u;
    float sourceOriginX = 0.0f;
    float sourceOriginY = 0.0f;
    float sourceOriginZ = 0.0f;
    Q1900StreamPhase phase = Q1900StreamPhase::Metadata;
    std::shared_ptr<Q1900MetadataTask> metadata;
    std::vector<Fo3WorldPlacement> targetPlacements;
    std::vector<Fo3WorldPlacement> incomingPlacements;
    std::vector<Fo3WorldPlacement> collisionPlacements;
    std::vector<Fo3WorldPlacement> collisionPrimePlacements;
    std::vector<CpuObject> incomingCpu;
    std::vector<GpuObject> incomingGpu;
    std::unordered_map<uint32_t, uint8_t> targetRefs;
    std::unordered_map<uint32_t, uint8_t> visibleRefs;
    size_t cpuCursor = 0u;
    size_t gpuCursor = 0u;
    size_t collisionPrimeCursor = 0u;
    size_t collisionPrimeBuilt = 0u;
    size_t collisionPrimeNegative = 0u;
    size_t collisionPrimeFailed = 0u;
    size_t collisionPrimeTriangles = 0u;
    size_t collisionPrimeFrames = 0u;
    uint64_t collisionPrimeMaxItemUs = 0u;
    bool collisionWindowPrepared = false;
    size_t retainedShapes = 0u;
    size_t skippedPlacements = 0u;
    size_t unsupportedPlacements = 0u;
    size_t dynamicOnlyModels = 0u;
    size_t frames = 0u;
    std::chrono::steady_clock::time_point startedAt{};
    uint64_t metadataUs = 0u;
    uint64_t cpuUs = 0u;
    uint64_t gpuUs = 0u;
    uint64_t collisionUs = 0u;
    uint64_t collisionPrimeUs = 0u;
    uint64_t collisionPublishUs = 0u;
    uint64_t terrainUs = 0u;
    bool collisionReady = false;
    bool terrainReady = false;
};

Q1900PendingStream gPendingStreamQ1900;
bool gQ1920LatestGridValid = false;
int32_t gQ1920LatestGridX = 0;
int32_t gQ1920LatestGridY = 0;

constexpr size_t Q1900_CPU_PLACEMENTS_PER_FRAME = 2u; // Q18.3 keep CPU prep below a VR frame
constexpr size_t Q1900_GPU_SHAPES_PER_FRAME = 1u; // Q18.3 one geometry/texture upload per frame
constexpr uint64_t Q1820_COLLISION_PRIME_BUDGET_US = 2500u;

void Q1900DeleteGpuShape(GpuObject& object) {
    // diffuse/normal are shared texture-cache handles and must survive a REFR
    // leaving the live window. Only per-shape geometry belongs to this object.
    if (object.vbo) {
        glDeleteBuffers(1, &object.vbo);
        object.vbo = 0u;
    }
    if (object.vao) {
        glDeleteVertexArrays(1, &object.vao);
        object.vao = 0u;
    }
}

void Q1900CancelPending(const char* reason) {
    if (!gPendingStreamQ1900.active) {
        gExteriorStreamBusyQ1890 = false;
        return;
    }
    const uint64_t generation = gPendingStreamQ1900.generation;
    for (GpuObject& object : gPendingStreamQ1900.incomingGpu) {
        Q1900DeleteGpuShape(object);
    }
    Q6H_LOGW("Q16.27 STREAM CANCELLED: generation=%llu reason=%s oldWindowRetained=1",
             static_cast<unsigned long long>(generation),
             reason ? reason : "unknown");
    gPendingStreamQ1900 = Q1900PendingStream{};
    gExteriorStreamBusyQ1890 = false;
}

bool Q1900BeginStream(float selectionGameX, float selectionGameY,
                      int32_t targetGridX, int32_t targetGridY) {
    if (!gExteriorStreamingActiveQ1890 || gExteriorStreamBusyQ1890 ||
        gPendingStreamQ1900.active) return false;

    gExteriorStreamBusyQ1890 = true;
    gPendingStreamQ1900 = Q1900PendingStream{};
    gPendingStreamQ1900.active = true;
    gPendingStreamQ1900.generation = ++gExteriorWindowGenerationQ1890;
    gPendingStreamQ1900.targetGridX = targetGridX;
    gPendingStreamQ1900.targetGridY = targetGridY;
    gPendingStreamQ1900.selectionGameX = selectionGameX;
    gPendingStreamQ1900.selectionGameY = selectionGameY;
    gPendingStreamQ1900.sourceWorldspace = gExteriorWorldspaceQ1890;
    gPendingStreamQ1900.sourcePersistentCell = gExteriorPersistentCellQ1890;
    gPendingStreamQ1900.sourceOriginX = gExteriorOriginXQ1890;
    gPendingStreamQ1900.sourceOriginY = gExteriorOriginYQ1890;
    gPendingStreamQ1900.sourceOriginZ = gExteriorOriginZQ1890;
    gPendingStreamQ1900.phase = Q1900StreamPhase::Metadata;
    gPendingStreamQ1900.startedAt = std::chrono::steady_clock::now();

    auto task = std::make_shared<Q1900MetadataTask>();
    task->worldspace = gExteriorWorldspaceQ1890;
    task->persistentCell = gExteriorPersistentCellQ1890;
    task->selectionGameX = selectionGameX;
    task->selectionGameY = selectionGameY;
    gPendingStreamQ1900.metadata = task;

    Q6H_LOGI("Q16.27 STREAM QUEUED: generation=%llu worldspace=%08X persistent=%08X fromGrid=(%d,%d) toGrid=(%d,%d) selection=(%.2f %.2f) retainedWindowLive=1 metadataThread=worker cpuBudget=%zu gpuBudget=%zu",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             task->worldspace, task->persistentCell,
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             targetGridX, targetGridY, selectionGameX, selectionGameY,
             Q1900_CPU_PLACEMENTS_PER_FRAME, Q1900_GPU_SHAPES_PER_FRAME);

    std::thread([task]() {
        const auto q1800MetadataStarted = std::chrono::steady_clock::now();
        SetFo3WorldspaceGridRadiusOverrideQ1950(2); // Q18 streamed 5x5 resident
        task->success = LoadFo3WorldspaceNeighborhoodQ75(
            task->worldspace, task->persistentCell,
            task->selectionGameX, task->selectionGameY,
            task->placements);
        SetFo3WorldspaceGridRadiusOverrideQ1950(-1);
        task->elapsedUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - q1800MetadataStarted).count());
        task->ready.store(true, std::memory_order_release);
    }).detach();
    return true;
}

void Q1900PrepareMetadata() {
    auto task = gPendingStreamQ1900.metadata;
    if (!task || !task->ready.load(std::memory_order_acquire)) return;

    if (!task->success || task->placements.empty()) {
        Q6H_LOGE("Q16.27 STREAM FAILED: generation=%llu phase=metadata placements=%zu oldWindowRetained=1",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 task ? task->placements.size() : 0u);
        Q1900CancelPending("metadata-failed");
        return;
    }

    gPendingStreamQ1900.metadataUs = task->elapsedUs;
    gPendingStreamQ1900.targetPlacements = std::move(task->placements);
    gPendingStreamQ1900.metadata.reset();
    gPendingStreamQ1900.dynamicOnlyModels =
        ConfigureFo3CollisionPolicyQ710(gPendingStreamQ1900.targetPlacements);

    std::unordered_map<uint32_t, uint8_t> existingRefs;
    existingRefs.reserve(gObjects.size());
    for (const GpuObject& object : gObjects) {
        if (object.refFormId != 0u) existingRefs[object.refFormId] = 1u;
    }

    gPendingStreamQ1900.targetRefs.reserve(
        gPendingStreamQ1900.targetPlacements.size());
    std::unordered_map<uint32_t, uint8_t> queuedEnteringRefs;
    queuedEnteringRefs.reserve(gPendingStreamQ1900.targetPlacements.size());

    for (const Fo3WorldPlacement& placement : gPendingStreamQ1900.targetPlacements) {
        if (placement.refFormId == 0u) continue;
        gPendingStreamQ1900.targetRefs[placement.refFormId] = 1u;
        if (existingRefs.find(placement.refFormId) == existingRefs.end() &&
            queuedEnteringRefs.emplace(placement.refFormId, 1u).second) {
            gPendingStreamQ1900.incomingPlacements.push_back(placement);
        }
    }

    for (const GpuObject& object : gObjects) {
        if (gPendingStreamQ1900.targetRefs.find(object.refFormId) !=
            gPendingStreamQ1900.targetRefs.end()) {
            ++gPendingStreamQ1900.retainedShapes;
            gPendingStreamQ1900.visibleRefs[object.refFormId] = 1u;
        }
    }

    gPendingStreamQ1900.incomingCpu.reserve(
        gPendingStreamQ1900.incomingPlacements.size() * 2u);
    // Q16.27: publish authored local physics before expensive entering visuals.
    gPendingStreamQ1900.phase = Q1900StreamPhase::Collision;

    Q6H_LOGI("Q16.27 METADATA READY: generation=%llu targetPlacements=%zu targetRefs=%zu enteringPlacements=%zu retainedShapes=%zu dynamicOnlyModels=%zu oldWindowStillLive=1",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             gPendingStreamQ1900.targetPlacements.size(),
             gPendingStreamQ1900.targetRefs.size(),
             gPendingStreamQ1900.incomingPlacements.size(),
             gPendingStreamQ1900.retainedShapes,
             gPendingStreamQ1900.dynamicOnlyModels);
}

void Q1900AdvanceCpu() {
    const auto q1800CpuStarted = std::chrono::steady_clock::now();
    size_t budget = Q1900_CPU_PLACEMENTS_PER_FRAME;
    while (budget-- > 0u &&
           gPendingStreamQ1900.cpuCursor < gPendingStreamQ1900.incomingPlacements.size()) {
        const Fo3WorldPlacement& placement =
            gPendingStreamQ1900.incomingPlacements[gPendingStreamQ1900.cpuCursor++];
        if (Q74ShouldSkipPlacement(placement)) {
            ++gPendingStreamQ1900.skippedPlacements;
            continue;
        }
        std::vector<CpuObject> parts;
        if (!BuildCpuObjects(placement, parts)) {
            ++gPendingStreamQ1900.unsupportedPlacements;
            continue;
        }
        for (CpuObject& part : parts) {
            gPendingStreamQ1900.incomingCpu.push_back(std::move(part));
        }
    }

    gPendingStreamQ1900.cpuUs += static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - q1800CpuStarted).count());

    if (gPendingStreamQ1900.cpuCursor >=
        gPendingStreamQ1900.incomingPlacements.size()) {
        gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingCpu.empty()
            ? Q1900StreamPhase::Terrain : Q1900StreamPhase::Gpu;
        Q6H_LOGI("Q16.27 CPU READY: generation=%llu enteringPlacements=%zu cpuShapes=%zu skipped=%zu unsupported=%zu frames=%zu",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.incomingPlacements.size(),
                 gPendingStreamQ1900.incomingCpu.size(),
                 gPendingStreamQ1900.skippedPlacements,
                 gPendingStreamQ1900.unsupportedPlacements,
                 gPendingStreamQ1900.frames);
    }
}

void Q1900AdvanceGpu() {
    const auto q1800GpuStarted = std::chrono::steady_clock::now();
    size_t budget = Q1900_GPU_SHAPES_PER_FRAME;
    while (budget-- > 0u &&
           gPendingStreamQ1900.gpuCursor < gPendingStreamQ1900.incomingCpu.size()) {
        CpuObject& cpu = gPendingStreamQ1900.incomingCpu[gPendingStreamQ1900.gpuCursor++];
        const uint32_t refFormId = cpu.placement.refFormId;
        GpuObject gpu;
        if (UploadCpuObject(cpu,
                            gExteriorOriginXQ1890,
                            gExteriorOriginYQ1890,
                            gExteriorOriginZQ1890,
                            gpu)) {
            gPendingStreamQ1900.visibleRefs[refFormId] = 1u;
            gPendingStreamQ1900.incomingGpu.push_back(std::move(gpu));
        } else {
            Q1900DeleteGpuShape(gpu);
        }
        cpu = CpuObject{};
    }

    gPendingStreamQ1900.gpuUs += static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - q1800GpuStarted).count());

    if (gPendingStreamQ1900.gpuCursor >= gPendingStreamQ1900.incomingCpu.size()) {
        gPendingStreamQ1900.phase = Q1900StreamPhase::Terrain;
        Q6H_LOGI("Q16.27 GPU READY: generation=%llu uploadedShapes=%zu visibleRefs=%zu frames=%zu textureCachePreserved=1",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.incomingGpu.size(),
                 gPendingStreamQ1900.visibleRefs.size(),
                 gPendingStreamQ1900.frames);
    }
}

void Q1900AdvanceCollision() {
    if (gExteriorWorldspaceQ1890 == 0x0000003Cu && gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 1 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 1)) {
        Q6H_LOGW("Q16.27 RESIDENT PREP STALE: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel-before-collision reason=5x5-no-longer-covers-active-3x3",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("resident-no-longer-covers-active-3x3");
        return;
    }

    if (!gPendingStreamQ1900.collisionWindowPrepared) {
        std::unordered_map<uint32_t, uint8_t> seenRefs;
        seenRefs.reserve(gPendingStreamQ1900.targetPlacements.size());
        size_t outsideCollisionWindow = 0u;
        constexpr int Q1950_COLLISION_GRID_RADIUS = 1;
        const int32_t collisionGridX = gPendingStreamQ1900.targetGridX;
        const int32_t collisionGridY = gPendingStreamQ1900.targetGridY;

        gPendingStreamQ1900.collisionPlacements.reserve(
            gPendingStreamQ1900.targetPlacements.size());
        for (const Fo3WorldPlacement& placement :
             gPendingStreamQ1900.targetPlacements) {
            const int32_t placementGridX = placement.hasExteriorGrid
                ? placement.gridX
                : static_cast<int32_t>(
                    std::floor(placement.x / Q1890_EXTERIOR_CELL_SIZE));
            const int32_t placementGridY = placement.hasExteriorGrid
                ? placement.gridY
                : static_cast<int32_t>(
                    std::floor(placement.y / Q1890_EXTERIOR_CELL_SIZE));
            if (std::abs(placementGridX - collisionGridX) >
                    Q1950_COLLISION_GRID_RADIUS ||
                std::abs(placementGridY - collisionGridY) >
                    Q1950_COLLISION_GRID_RADIUS) {
                ++outsideCollisionWindow;
                continue;
            }
            if (placement.refFormId == 0u ||
                !seenRefs.emplace(placement.refFormId, 1u).second ||
                Q74ShouldSkipPlacement(placement)) {
                continue;
            }
            gPendingStreamQ1900.collisionPlacements.push_back(placement);
            if (!IsFo3CollisionPlacementCachedQ1820(placement.refFormId))
                gPendingStreamQ1900.collisionPrimePlacements.push_back(placement);
        }
        gPendingStreamQ1900.collisionWindowPrepared = true;

        Q6H_LOGI("Q18.2 COLLISION WINDOW: generation=%llu targetGrid=(%d,%d) visualPlacements=%zu localRenderableRefs=%zu primeMissRefs=%zu outside3x3=%zu radius=1 prewarmBudgetUs=%llu oldCollisionLive=1",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX,
                 gPendingStreamQ1900.targetGridY,
                 gPendingStreamQ1900.targetPlacements.size(),
                 gPendingStreamQ1900.collisionPlacements.size(),
                 gPendingStreamQ1900.collisionPrimePlacements.size(),
                 outsideCollisionWindow,
                 static_cast<unsigned long long>(
                     Q1820_COLLISION_PRIME_BUDGET_US));
    }

    if (gPendingStreamQ1900.collisionPrimeCursor <
        gPendingStreamQ1900.collisionPrimePlacements.size()) {
        const auto frameStarted = std::chrono::steady_clock::now();
        ++gPendingStreamQ1900.collisionPrimeFrames;
        size_t processedThisFrame = 0u;

        while (gPendingStreamQ1900.collisionPrimeCursor <
               gPendingStreamQ1900.collisionPrimePlacements.size()) {
            const Fo3WorldPlacement& placement =
                gPendingStreamQ1900.collisionPrimePlacements[
                    gPendingStreamQ1900.collisionPrimeCursor++];
            size_t primedTriangles = 0u;
            const auto itemStarted = std::chrono::steady_clock::now();
            const bool primed = PrimeFo3CollisionPlacementCacheQ1820(
                placement,
                gExteriorOriginXQ1890,
                gExteriorOriginYQ1890,
                gExteriorOriginZQ1890,
                SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE,
                &primedTriangles);
            const uint64_t itemUs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - itemStarted).count());
            gPendingStreamQ1900.collisionPrimeUs += itemUs;
            gPendingStreamQ1900.collisionPrimeMaxItemUs =
                std::max(gPendingStreamQ1900.collisionPrimeMaxItemUs, itemUs);
            if (!primed) {
                ++gPendingStreamQ1900.collisionPrimeFailed;
            } else if (primedTriangles == 0u) {
                ++gPendingStreamQ1900.collisionPrimeNegative;
            } else {
                ++gPendingStreamQ1900.collisionPrimeBuilt;
                gPendingStreamQ1900.collisionPrimeTriangles += primedTriangles;
            }
            ++processedThisFrame;
            PumpFo3AndroidEventsQ1860();

            const uint64_t frameUs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - frameStarted).count());
            if (frameUs >= Q1820_COLLISION_PRIME_BUDGET_US) break;
        }

        if (gPendingStreamQ1900.collisionPrimeCursor <
            gPendingStreamQ1900.collisionPrimePlacements.size()) {
            if (gPendingStreamQ1900.collisionPrimeFrames <= 3u ||
                (gPendingStreamQ1900.collisionPrimeFrames % 30u) == 0u) {
                Q6H_LOGI("Q18.2 COLLISION PRIME: generation=%llu cursor=%zu/%zu processed=%zu built=%zu negative=%zu failed=%zu triangles=%zu primeUs=%llu maxItemUs=%llu oldCollisionLive=1",
                         static_cast<unsigned long long>(
                             gPendingStreamQ1900.generation),
                         gPendingStreamQ1900.collisionPrimeCursor,
                         gPendingStreamQ1900.collisionPrimePlacements.size(),
                         processedThisFrame,
                         gPendingStreamQ1900.collisionPrimeBuilt,
                         gPendingStreamQ1900.collisionPrimeNegative,
                         gPendingStreamQ1900.collisionPrimeFailed,
                         gPendingStreamQ1900.collisionPrimeTriangles,
                         static_cast<unsigned long long>(
                             gPendingStreamQ1900.collisionPrimeUs),
                         static_cast<unsigned long long>(
                             gPendingStreamQ1900.collisionPrimeMaxItemUs));
            }
            return;
        }

        Q6H_LOGI("Q18.2 COLLISION PRIME READY: generation=%llu refs=%zu built=%zu negative=%zu failed=%zu triangles=%zu frames=%zu primeUs=%llu maxItemUs=%llu",
                 static_cast<unsigned long long>(
                     gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.collisionPrimePlacements.size(),
                 gPendingStreamQ1900.collisionPrimeBuilt,
                 gPendingStreamQ1900.collisionPrimeNegative,
                 gPendingStreamQ1900.collisionPrimeFailed,
                 gPendingStreamQ1900.collisionPrimeTriangles,
                 gPendingStreamQ1900.collisionPrimeFrames,
                 static_cast<unsigned long long>(
                     gPendingStreamQ1900.collisionPrimeUs),
                 static_cast<unsigned long long>(
                     gPendingStreamQ1900.collisionPrimeMaxItemUs));
        return; // publish on a clean frame after the final prewarm item
    }

    const auto publishStarted = std::chrono::steady_clock::now();
    PumpFo3AndroidEventsQ1860();
    SetNextFo3CollisionExteriorModeQ1931(true);
    gPendingStreamQ1900.collisionReady = InitializeFo3CollisionOverlay(
        gPendingStreamQ1900.collisionPlacements,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    PumpFo3AndroidEventsQ1860();

    gPendingStreamQ1900.collisionPublishUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - publishStarted).count());
    gPendingStreamQ1900.collisionUs =
        gPendingStreamQ1900.collisionPrimeUs +
        gPendingStreamQ1900.collisionPublishUs;

    gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingPlacements.empty()
        ? Q1900StreamPhase::Terrain : Q1900StreamPhase::Cpu;

    Q6H_LOGI("Q18.2 COLLISION READY: generation=%llu uniqueRenderableRefs=%zu ready=%d staged=1 primeRefs=%zu primeFrames=%zu primeUs=%llu publishUs=%llu totalCollisionUs=%llu maxPrimeItemUs=%llu",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             gPendingStreamQ1900.collisionPlacements.size(),
             gPendingStreamQ1900.collisionReady ? 1 : 0,
             gPendingStreamQ1900.collisionPrimePlacements.size(),
             gPendingStreamQ1900.collisionPrimeFrames,
             static_cast<unsigned long long>(
                 gPendingStreamQ1900.collisionPrimeUs),
             static_cast<unsigned long long>(
                 gPendingStreamQ1900.collisionPublishUs),
             static_cast<unsigned long long>(
                 gPendingStreamQ1900.collisionUs),
             static_cast<unsigned long long>(
                 gPendingStreamQ1900.collisionPrimeMaxItemUs));
}

void Q1900AdvanceTerrain() {
    const auto q1800TerrainStarted = std::chrono::steady_clock::now();
    static bool q1950TerrainWindowValid = false;
    static uint32_t q1950TerrainWorldspace = 0u;
    static uint32_t q1950TerrainPersistent = 0u;
    static int32_t q1950TerrainGridX = 0;
    static int32_t q1950TerrainGridY = 0;

    const bool q1950TerrainContextChanged =
        !q1950TerrainWindowValid ||
        q1950TerrainWorldspace != gExteriorWorldspaceQ1890 ||
        q1950TerrainPersistent != gExteriorPersistentCellQ1890;
    if (q1950TerrainContextChanged) {
        q1950TerrainWindowValid = true;
        q1950TerrainWorldspace = gExteriorWorldspaceQ1890;
        q1950TerrainPersistent = gExteriorPersistentCellQ1890;
        q1950TerrainGridX = gExteriorWindowGridXQ1890;
        q1950TerrainGridY = gExteriorWindowGridYQ1890;
        Q6H_LOGI("Q16.27 TERRAIN RESIDENCY RESET: worldspace=%08X persistent=%08X centre=(%d,%d) radius=3",
                 q1950TerrainWorldspace, q1950TerrainPersistent,
                 q1950TerrainGridX, q1950TerrainGridY);
    }

    const int32_t q1990TerrainReferenceGridX = gQ1920LatestGridValid
        ? gQ1920LatestGridX : gPendingStreamQ1900.targetGridX;
    const int32_t q1990TerrainReferenceGridY = gQ1920LatestGridValid
        ? gQ1920LatestGridY : gPendingStreamQ1900.targetGridY;
    const int q1950TerrainDx = std::abs(q1990TerrainReferenceGridX - q1950TerrainGridX);
    const int q1950TerrainDy = std::abs(q1990TerrainReferenceGridY - q1950TerrainGridY);
    // Radius 3 can keep an actual-centred radius-1 near world fully backed while
    // actualGrid moves two cells from the retained LAND centre.
    if (q1950TerrainDx <= 2 && q1950TerrainDy <= 2) {
        gPendingStreamQ1900.terrainReady = true;
        gPendingStreamQ1900.terrainUs += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - q1800TerrainStarted).count());
        gPendingStreamQ1900.phase = Q1900StreamPhase::Commit;
        Q6H_LOGI("Q16.27 TERRAIN RETAIN: generation=%llu retainedCentre=(%d,%d) target=(%d,%d) delta=(%d,%d) radius=3 fullGpuRebuild=0",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 q1950TerrainGridX, q1950TerrainGridY,
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 q1950TerrainDx, q1950TerrainDy);
        return;
    }

    PumpFo3AndroidEventsQ1860();
    const float q1990TerrainSelectionGameX =
        (static_cast<float>(q1990TerrainReferenceGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const float q1990TerrainSelectionGameY =
        (static_cast<float>(q1990TerrainReferenceGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    SetFo3TerrainSelectionOverrideQ1890(
        true, q1990TerrainSelectionGameX, q1990TerrainSelectionGameY);
    gPendingStreamQ1900.terrainReady = InitializeFo3TerrainRenderQ76(
        gExteriorWorldspaceQ1890,
        gExteriorOriginXQ1890,
        gExteriorOriginYQ1890,
        gExteriorOriginZQ1890,
        SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    SetFo3TerrainSelectionOverrideQ1890(false, 0.0f, 0.0f);
    if (gPendingStreamQ1900.terrainReady) {
        q1950TerrainGridX = q1990TerrainReferenceGridX;
        q1950TerrainGridY = q1990TerrainReferenceGridY;
        Q6H_LOGI("Q16.27 TERRAIN RECENTER: generation=%llu centre=(%d,%d) radius=3 fullGpuRebuild=1",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 q1950TerrainGridX, q1950TerrainGridY);
    }
    if (gPendingStreamQ1900.terrainReady) {
        ActivateFo3TerrainGroundingQ77(
            gExteriorWorldspaceQ1890,
            gExteriorOriginXQ1890,
            gExteriorOriginYQ1890,
            gExteriorOriginZQ1890,
            SCENE_FORWARD, FLOOR_Y, FO3_UNITS_PER_METRE);
    }
    PumpFo3AndroidEventsQ1860();
    gPendingStreamQ1900.terrainUs += static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - q1800TerrainStarted).count());
    gPendingStreamQ1900.phase = Q1900StreamPhase::Commit;

    Q6H_LOGI("Q18 TERRAIN READY: generation=%llu selection=(%.2f %.2f) ready=%d isolatedFrame=1 originPreserved=1",
             static_cast<unsigned long long>(gPendingStreamQ1900.generation),
             gPendingStreamQ1900.selectionGameX,
             gPendingStreamQ1900.selectionGameY,
             gPendingStreamQ1900.terrainReady ? 1 : 0);
}

void Q1900CommitWindow() {
    const auto q1800CommitStarted = std::chrono::steady_clock::now();
    if (gExteriorWorldspaceQ1890 == 0x0000003Cu && gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 1 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 1)) {
        Q6H_LOGW("Q16.27 RESIDENT COMMIT STALE: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel reason=5x5-no-longer-covers-active-3x3",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("resident-no-longer-covers-active-3x3-at-commit");
        return;
    }
    const size_t oldShapes = gObjects.size();
    const size_t enteringShapes = gPendingStreamQ1900.incomingGpu.size();
    std::vector<GpuObject> replacement;
    replacement.reserve(gPendingStreamQ1900.retainedShapes + enteringShapes);

    size_t retiredShapes = 0u;
    for (GpuObject& object : gObjects) {
        if (gPendingStreamQ1900.targetRefs.find(object.refFormId) !=
            gPendingStreamQ1900.targetRefs.end()) {
            replacement.push_back(std::move(object));
            // GpuObject is not an owning RAII type; clear moved geometry handles
            // so no later cleanup path can accidentally delete retained shapes.
            object.vbo = 0u;
            object.vao = 0u;
        } else {
            Q1900DeleteGpuShape(object);
            ++retiredShapes;
        }
    }
    gObjects.clear();

    for (GpuObject& object : gPendingStreamQ1900.incomingGpu) {
        replacement.push_back(std::move(object));
        object.vbo = 0u;
        object.vao = 0u;
    }

    gObjects = std::move(replacement);
    gSceneReady = !gObjects.empty();
    gLoggedFirstDraw = false;
    gExteriorWindowGridXQ1890 = gPendingStreamQ1900.targetGridX;
    gExteriorWindowGridYQ1890 = gPendingStreamQ1900.targetGridY;

    size_t triangles = 0u;
    for (const GpuObject& object : gObjects) {
        triangles += static_cast<size_t>(object.vertexCount / 3);
    }

    const uint64_t generation = gPendingStreamQ1900.generation;
    const size_t frames = gPendingStreamQ1900.frames;
    const size_t retainedShapes = gPendingStreamQ1900.retainedShapes;
    const bool collisionReady = gPendingStreamQ1900.collisionReady;
    const bool terrainReady = gPendingStreamQ1900.terrainReady;

    const uint64_t q1800CommitUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - q1800CommitStarted).count());
    const uint64_t q1800TotalUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - gPendingStreamQ1900.startedAt).count());

    Q6H_LOGI("Q18 WINDOW READY: generation=%llu grid=(%d,%d) oldShapes=%zu retainedShapes=%zu enteringShapes=%zu retiredShapes=%zu liveShapes=%zu triangles=%zu stagedFrames=%zu collisionReady=%d terrainReady=%d metadataUs=%llu cpuUs=%llu gpuUs=%llu collisionUs=%llu collisionPrimeUs=%llu collisionPublishUs=%llu terrainUs=%llu commitUs=%llu totalUs=%llu originPreserved=1 playerReset=0 fullSceneRebuild=0",
             static_cast<unsigned long long>(generation),
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             oldShapes, retainedShapes, enteringShapes, retiredShapes,
             gObjects.size(), triangles, frames,
             collisionReady ? 1 : 0, terrainReady ? 1 : 0,
             static_cast<unsigned long long>(gPendingStreamQ1900.metadataUs),
             static_cast<unsigned long long>(gPendingStreamQ1900.cpuUs),
             static_cast<unsigned long long>(gPendingStreamQ1900.gpuUs),
             static_cast<unsigned long long>(gPendingStreamQ1900.collisionUs),
             static_cast<unsigned long long>(gPendingStreamQ1900.collisionPrimeUs),
             static_cast<unsigned long long>(gPendingStreamQ1900.collisionPublishUs),
             static_cast<unsigned long long>(gPendingStreamQ1900.terrainUs),
             static_cast<unsigned long long>(q1800CommitUs),
             static_cast<unsigned long long>(q1800TotalUs));

    gPendingStreamQ1900 = Q1900PendingStream{};
    gExteriorStreamBusyQ1890 = false;
}

void Q1900AdvanceStream() {
    if (!gPendingStreamQ1900.active) return;
    ++gPendingStreamQ1900.frames;

    // Let explicit door/cell loading own the renderer while its loading state is
    // visible. A detached metadata read may finish meanwhile, but is not consumed.
    if (IsFo3LoadingVisibleQ1700()) return;

    const bool contextChanged =
        !gExteriorStreamingActiveQ1890 ||
        gPendingStreamQ1900.sourceWorldspace != gExteriorWorldspaceQ1890 ||
        gPendingStreamQ1900.sourcePersistentCell != gExteriorPersistentCellQ1890 ||
        std::fabs(gPendingStreamQ1900.sourceOriginX - gExteriorOriginXQ1890) > 0.01f ||
        std::fabs(gPendingStreamQ1900.sourceOriginY - gExteriorOriginYQ1890) > 0.01f ||
        std::fabs(gPendingStreamQ1900.sourceOriginZ - gExteriorOriginZQ1890) > 0.01f;
    if (contextChanged) {
        Q1900CancelPending("scene-context-changed");
        return;
    }

    if (gQ1920LatestGridValid &&
        (std::abs(gPendingStreamQ1900.targetGridX - gQ1920LatestGridX) > 1 ||
         std::abs(gPendingStreamQ1900.targetGridY - gQ1920LatestGridY) > 1)) {
        Q6H_LOGW("Q16.27 RESIDENT STALE EARLY: generation=%llu target=(%d,%d) actual=(%d,%d) action=cancel-before-more-work reason=5x5-no-longer-covers-active-3x3",
                 static_cast<unsigned long long>(gPendingStreamQ1900.generation),
                 gPendingStreamQ1900.targetGridX, gPendingStreamQ1900.targetGridY,
                 gQ1920LatestGridX, gQ1920LatestGridY);
        Q1900CancelPending("resident-no-longer-covers-active-3x3");
        return;
    }

    switch (gPendingStreamQ1900.phase) {
        case Q1900StreamPhase::Metadata:
            Q1900PrepareMetadata();
            break;
        case Q1900StreamPhase::Cpu:
            Q1900AdvanceCpu();
            break;
        case Q1900StreamPhase::Gpu:
            Q1900AdvanceGpu();
            break;
        case Q1900StreamPhase::Collision:
            Q1900AdvanceCollision();
            break;
        case Q1900StreamPhase::Terrain:
            Q1900AdvanceTerrain();
            break;
        case Q1900StreamPhase::Commit:
            Q1900CommitWindow();
            break;
    }
}

void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    if (!gExteriorStreamingActiveQ1890) {
        gQ1920LatestGridValid = false;
        return;
    }

    const float gameX = gExteriorOriginXQ1890 +
                        virtualHeadX * FO3_UNITS_PER_METRE;
    const float gameY = gExteriorOriginYQ1890 +
                        (SCENE_FORWARD - virtualHeadZ) * FO3_UNITS_PER_METRE;
    const int32_t actualGridX = static_cast<int32_t>(
        std::floor(gameX / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t actualGridY = static_cast<int32_t>(
        std::floor(gameY / Q1890_EXTERIOR_CELL_SIZE));
    gQ1920LatestGridValid = true;
    gQ1920LatestGridX = actualGridX;
    gQ1920LatestGridY = actualGridY;
    if (gExteriorWorldspaceQ1890 == 0x0000003Cu) {
        Q1990EnsureNativeLodForCell(actualGridX, actualGridY,
                                    gExteriorOriginXQ1890,
                                    gExteriorOriginYQ1890,
                                    gExteriorOriginZQ1890);
    }

    static bool q1970ActiveLogReady = false;
    static int32_t q1970LastActiveGridX = 0;
    static int32_t q1970LastActiveGridY = 0;
    if (!q1970ActiveLogReady || actualGridX != q1970LastActiveGridX ||
        actualGridY != q1970LastActiveGridY) {
        q1970ActiveLogReady = true;
        q1970LastActiveGridX = actualGridX;
        q1970LastActiveGridY = actualGridY;
        size_t q1970ActiveShapes = 0u;
        size_t q1970ActiveTriangles = 0u;
        for (const GpuObject& q1970Object : gObjects) {
            if (!Q1970ShouldRenderFullDetail(q1970Object)) continue;
            ++q1970ActiveShapes;
            q1970ActiveTriangles += static_cast<size_t>(q1970Object.vertexCount / 3);
        }
        Q6H_LOGI("Q16.27 ACTIVE DRAW: actual=(%d,%d) residentCentre=(%d,%d) residentShapes=%zu activeShapes=%zu activeTriangles=%zu activeRadius=1 residentRadius=2",
                 actualGridX, actualGridY,
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 gObjects.size(), q1970ActiveShapes, q1970ActiveTriangles);
    }

    // Give a pending generation current player position before it advances.
    Q1900AdvanceStream();
    if (gExteriorStreamBusyQ1890 || IsFo3LoadingVisibleQ1700()) return;

    static uint32_t q1920MotionWorldspace = 0u;
    static uint32_t q1920MotionPersistent = 0u;
    static float q1920MotionOriginX = 0.0f;
    static float q1920MotionOriginY = 0.0f;
    static bool q1920MotionReady = false;
    static float q1920PreviousGameX = 0.0f;
    static float q1920PreviousGameY = 0.0f;
    static float q1920SmoothDx = 0.0f;
    static float q1920SmoothDy = 0.0f;
    static int32_t q1920PreviousActualGridX = 0;
    static int32_t q1920PreviousActualGridY = 0;

    const bool newContext = !q1920MotionReady ||
        q1920MotionWorldspace != gExteriorWorldspaceQ1890 ||
        q1920MotionPersistent != gExteriorPersistentCellQ1890 ||
        std::fabs(q1920MotionOriginX - gExteriorOriginXQ1890) > 0.01f ||
        std::fabs(q1920MotionOriginY - gExteriorOriginYQ1890) > 0.01f;
    if (newContext) {
        q1920MotionWorldspace = gExteriorWorldspaceQ1890;
        q1920MotionPersistent = gExteriorPersistentCellQ1890;
        q1920MotionOriginX = gExteriorOriginXQ1890;
        q1920MotionOriginY = gExteriorOriginYQ1890;
        q1920MotionReady = true;
        q1920PreviousGameX = gameX;
        q1920PreviousGameY = gameY;
        q1920SmoothDx = 0.0f;
        q1920SmoothDy = 0.0f;
        q1920PreviousActualGridX = actualGridX;
        q1920PreviousActualGridY = actualGridY;
        Q6H_LOGI("Q16.27 MOTION RESET: worldspace=%08X persistent=%08X actual=(%d,%d) reason=context",
                 gExteriorWorldspaceQ1890, gExteriorPersistentCellQ1890,
                 actualGridX, actualGridY);
        return;
    }

    const int32_t q1830PreviousActualGridX = q1920PreviousActualGridX;
    const int32_t q1830PreviousActualGridY = q1920PreviousActualGridY;
    const bool actualCellChanged =
        actualGridX != q1830PreviousActualGridX ||
        actualGridY != q1830PreviousActualGridY;
    q1920PreviousActualGridX = actualGridX;
    q1920PreviousActualGridY = actualGridY;

    const float frameDx = gameX - q1920PreviousGameX;
    const float frameDy = gameY - q1920PreviousGameY;
    q1920PreviousGameX = gameX;
    q1920PreviousGameY = gameY;

    // A single rendered-frame delta larger than this is a stall, teleport or
    // scene discontinuity, not a locomotion velocity sample. Do not predict from it.
    constexpr float Q1920_MAX_VALID_FRAME_DELTA = 96.0f;
    if (std::fabs(frameDx) > Q1920_MAX_VALID_FRAME_DELTA ||
        std::fabs(frameDy) > Q1920_MAX_VALID_FRAME_DELTA) {
        const int q1830CellDx = actualGridX - q1830PreviousActualGridX;
        const int q1830CellDy = actualGridY - q1830PreviousActualGridY;
        const bool q1830AdjacentTravel =
            std::abs(q1830CellDx) <= 2 && std::abs(q1830CellDy) <= 2 &&
            (q1830CellDx != 0 || q1830CellDy != 0);
        if (q1830AdjacentTravel) {
            q1920SmoothDx = q1830CellDx > 0 ? 1.0f : (q1830CellDx < 0 ? -1.0f : 0.0f);
            q1920SmoothDy = q1830CellDy > 0 ? 1.0f : (q1830CellDy < 0 ? -1.0f : 0.0f);
        } else {
            q1920SmoothDx = 0.0f;
            q1920SmoothDy = 0.0f;
        }
        Q6H_LOGW("Q18.3 MOTION SAMPLE REJECTED: delta=(%.2f %.2f) actual=(%d,%d) inferredCellDirection=(%d,%d) reason=stall-or-teleport",
                 frameDx, frameDy, actualGridX, actualGridY,
                 q1830CellDx, q1830CellDy);
    } else {
        q1920SmoothDx = q1920SmoothDx * 0.82f + frameDx * 0.18f;
        q1920SmoothDy = q1920SmoothDy * 0.82f + frameDy * 0.18f;
    }

    // Q18.3: crossing an authored CELL boundary is not itself a reason to rebuild.
    // A resident radius-2 window still fully covers the actual-centred radius-1
    // draw/collision neighborhood while actual is at most one CELL from its centre.
    // Keep using that runway and prepare the next strip from motion direction.
    if (actualCellChanged &&
        std::abs(actualGridX - gExteriorWindowGridXQ1890) <= 1 &&
        std::abs(actualGridY - gExteriorWindowGridYQ1890) <= 1) {
        Q6H_LOGI("Q18.3 RESIDENT RUNWAY: window=(%d,%d) actual=(%d,%d) boundaryRecenter=0 residentRadius=2 activeRadius=1",
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 actualGridX, actualGridY);
    }

    // Q18.4: if streaming ever falls far enough behind that the live 5x5 no
    // longer covers the actual-centred 3x3, this is recovery, not prediction.
    // Jump the target centre directly under the player. The old one-CELL step
    // could itself be >1 CELL behind actual, so the stale guard cancelled it
    // immediately and requeued forever.
    if (std::abs(actualGridX - gExteriorWindowGridXQ1890) > 1 ||
        std::abs(actualGridY - gExteriorWindowGridYQ1890) > 1) {
        const int32_t q1980CatchupGridX = actualGridX;
        const int32_t q1980CatchupGridY = actualGridY;
        const float selectionX =
            (static_cast<float>(q1980CatchupGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
        const float selectionY =
            (static_cast<float>(q1980CatchupGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
        Q6H_LOGW("Q18.4 DIRECT CATCHUP: window=(%d,%d) actual=(%d,%d) target=(%d,%d) selection=(%.2f %.2f) reason=resident-runway-missed",
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 actualGridX, actualGridY, q1980CatchupGridX, q1980CatchupGridY,
                 selectionX, selectionY);
        Q1900BeginStream(selectionX, selectionY,
                         q1980CatchupGridX, q1980CatchupGridY);
        return;
    }

    // MegatonExterior (00000A74) is a small child worldspace whose loader already
    // selects the complete authored worldspace. Recentering it buys nothing and
    // was the source of the stale generation observed during the Megaton exit.
    if (gExteriorWorldspaceQ1890 == 0x00000A74u) return;

    const float cellMinX = static_cast<float>(actualGridX) * Q1890_EXTERIOR_CELL_SIZE;
    const float cellMinY = static_cast<float>(actualGridY) * Q1890_EXTERIOR_CELL_SIZE;
    const float localX = gameX - cellMinX;
    const float localY = gameY - cellMinY;
    const float distWest = localX;
    const float distEast = Q1890_EXTERIOR_CELL_SIZE - localX;
    const float distSouth = localY;
    const float distNorth = Q1890_EXTERIOR_CELL_SIZE - localY;

    constexpr float Q1920_PREFETCH_DISTANCE = 4096.0f; // Q16.27 earliest stable-direction adjacent prefetch
    constexpr float Q1920_DIRECTION_EPSILON = 0.35f;
    int axis = 0; // 1 = X, 2 = Y
    int step = 0;
    float bestDistance = Q1920_PREFETCH_DISTANCE + 1.0f;

    if (q1920SmoothDx > Q1920_DIRECTION_EPSILON && distEast < bestDistance) {
        axis = 1; step = 1; bestDistance = distEast;
    }
    if (q1920SmoothDx < -Q1920_DIRECTION_EPSILON && distWest < bestDistance) {
        axis = 1; step = -1; bestDistance = distWest;
    }
    if (q1920SmoothDy > Q1920_DIRECTION_EPSILON && distNorth < bestDistance) {
        axis = 2; step = 1; bestDistance = distNorth;
    }
    if (q1920SmoothDy < -Q1920_DIRECTION_EPSILON && distSouth < bestDistance) {
        axis = 2; step = -1; bestDistance = distSouth;
    }
    if (axis == 0 || bestDistance > Q1920_PREFETCH_DISTANCE) return;

    // Move the resident centre by exactly one CELL. If it is already one CELL
    // ahead of the player, hold it there; if it trails by one, this catches the
    // resident window up without treating the boundary as a load trigger.
    int32_t desiredGridX = gExteriorWindowGridXQ1890;
    int32_t desiredGridY = gExteriorWindowGridYQ1890;
    if (axis == 1) {
        desiredGridX = std::clamp(gExteriorWindowGridXQ1890 + step,
                                  actualGridX - 1, actualGridX + 1);
    } else {
        desiredGridY = std::clamp(gExteriorWindowGridYQ1890 + step,
                                  actualGridY - 1, actualGridY + 1);
    }

    if (desiredGridX == gExteriorWindowGridXQ1890 &&
        desiredGridY == gExteriorWindowGridYQ1890) return;

    const float selectionGameX =
        (static_cast<float>(desiredGridX) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    const float selectionGameY =
        (static_cast<float>(desiredGridY) + 0.5f) * Q1890_EXTERIOR_CELL_SIZE;
    Q6H_LOGI("Q18.3 RESIDENT PREFETCH: window=(%d,%d) actual=(%d,%d) desired=(%d,%d) axis=%c step=%d edgeDistance=%.1f smoothDelta=(%.2f %.2f) selection=(%.2f %.2f)",
             gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
             actualGridX, actualGridY, desiredGridX, desiredGridY,
             axis == 1 ? 'X' : 'Y', step, bestDistance,
             q1920SmoothDx, q1920SmoothDy,
             selectionGameX, selectionGameY);
    Q1900BeginStream(selectionGameX, selectionGameY,
                     desiredGridX, desiredGridY);
}

int32_t Q1970FloorToLevel4Block(int32_t cell) {
    int32_t quotient = cell / 4;
    if (cell < 0 && (cell % 4) != 0) --quotient;
    return quotient * 4;
}

void Q1970ProbeLodPath(const char* kind, const std::string& path,
                       int32_t blockX, int32_t blockY) {
    std::vector<uint8_t> raw;
    std::string resolved;
    const bool found = LoadFalloutMeshFile(path, raw, &resolved);
    std::vector<Fo3StaticNifMesh> meshes;
    const bool parsed = found && LoadFo3StaticNifMeshes(path, meshes) && !meshes.empty();

    float minX = 1e30f, minY = 1e30f, minZ = 1e30f;
    float maxX = -1e30f, maxY = -1e30f, maxZ = -1e30f;
    size_t vertices = 0u;
    size_t triangles = 0u;
    if (parsed) {
        for (const Fo3StaticNifMesh& mesh : meshes) {
            vertices += mesh.positions.size() / 3u;
            triangles += mesh.indices.size() / 3u;
            for (size_t i = 0u; i + 2u < mesh.positions.size(); i += 3u) {
                minX = std::min(minX, mesh.positions[i]);
                minY = std::min(minY, mesh.positions[i + 1u]);
                minZ = std::min(minZ, mesh.positions[i + 2u]);
                maxX = std::max(maxX, mesh.positions[i]);
                maxY = std::max(maxY, mesh.positions[i + 1u]);
                maxZ = std::max(maxZ, mesh.positions[i + 2u]);
            }
        }
    }

    Q6H_LOGI("Q16.27 NATIVE LOD PROBE: kind=%s block=(%d,%d) found=%d bytes=%zu parsed=%d shapes=%zu vertices=%zu triangles=%zu bounds=[%.1f %.1f %.1f]-[%.1f %.1f %.1f] path=%s resolved=%s",
             kind, blockX, blockY, found ? 1 : 0, raw.size(), parsed ? 1 : 0,
             meshes.size(), vertices, triangles,
             parsed ? minX : 0.0f, parsed ? minY : 0.0f, parsed ? minZ : 0.0f,
             parsed ? maxX : 0.0f, parsed ? maxY : 0.0f, parsed ? maxZ : 0.0f,
             path.c_str(), resolved.empty() ? "<none>" : resolved.c_str());
}

void Q1970ProbeNativeLod(float gameX, float gameY) {
    static bool q1970Probed = false;
    if (q1970Probed) return;
    q1970Probed = true;

    const int32_t cellX = static_cast<int32_t>(std::floor(gameX / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t cellY = static_cast<int32_t>(std::floor(gameY / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t blockX = Q1970FloorToLevel4Block(cellX);
    const int32_t blockY = Q1970FloorToLevel4Block(cellY);
    const std::string suffix = "Wasteland.Level4.X" + std::to_string(blockX) +
                               ".Y" + std::to_string(blockY) + ".NIF";
    const std::string terrainPath = "Landscape\\LOD\\Wasteland\\" + suffix;
    const std::string objectPath = "Landscape\\LOD\\Wasteland\\Blocks\\" + suffix;
    Q6H_LOGI("Q16.27 NATIVE LOD EXPECTED: playerCell=(%d,%d) level4Block=(%d,%d)",
             cellX, cellY, blockX, blockY);
    Q1970ProbeLodPath("terrain", terrainPath, blockX, blockY);
    Q1970ProbeLodPath("objects", objectPath, blockX, blockY);
}

bool Q1970GetActiveGrid(int32_t& gridX, int32_t& gridY) {
    if (gExteriorWorldspaceQ1890 != 0x0000003Cu) return false;
    if (gQ1920LatestGridValid) {
        gridX = gQ1920LatestGridX;
        gridY = gQ1920LatestGridY;
    } else {
        gridX = gExteriorWindowGridXQ1890;
        gridY = gExteriorWindowGridYQ1890;
    }
    return true;
}

bool Q1970ShouldRenderFullDetail(const GpuObject& object) {
    if (object.q1990NativeLod) return true;
    int32_t activeGridX = 0;
    int32_t activeGridY = 0;
    if (!Q1970GetActiveGrid(activeGridX, activeGridY)) return true;
    constexpr int Q1970_ACTIVE_VISUAL_RADIUS = 1; // Q16.27 3x3 full-detail draw set
    return std::abs(object.q1970GridX - activeGridX) <= Q1970_ACTIVE_VISUAL_RADIUS &&
           std::abs(object.q1970GridY - activeGridY) <= Q1970_ACTIVE_VISUAL_RADIUS;
}

bool Q1810GetNativeLodNearClip(float& minX, float& maxX,
                               float& minZ, float& maxZ) {
    int32_t activeGridX = 0;
    int32_t activeGridY = 0;
    if (!Q1970GetActiveGrid(activeGridX, activeGridY)) return false;

    constexpr int Q1810_ACTIVE_RADIUS = 1;
    const float minGameX =
        static_cast<float>(activeGridX - Q1810_ACTIVE_RADIUS) * Q1890_EXTERIOR_CELL_SIZE;
    const float maxGameX =
        static_cast<float>(activeGridX + Q1810_ACTIVE_RADIUS + 1) * Q1890_EXTERIOR_CELL_SIZE;
    const float minGameY =
        static_cast<float>(activeGridY - Q1810_ACTIVE_RADIUS) * Q1890_EXTERIOR_CELL_SIZE;
    const float maxGameY =
        static_cast<float>(activeGridY + Q1810_ACTIVE_RADIUS + 1) * Q1890_EXTERIOR_CELL_SIZE;

    minX = (minGameX - gExteriorOriginXQ1890) / FO3_UNITS_PER_METRE;
    maxX = (maxGameX - gExteriorOriginXQ1890) / FO3_UNITS_PER_METRE;

    const float zAtMinGameY =
        SCENE_FORWARD - (minGameY - gExteriorOriginYQ1890) / FO3_UNITS_PER_METRE;
    const float zAtMaxGameY =
        SCENE_FORWARD - (maxGameY - gExteriorOriginYQ1890) / FO3_UNITS_PER_METRE;
    minZ = std::min(zAtMinGameY, zAtMaxGameY);
    maxZ = std::max(zAtMinGameY, zAtMaxGameY);
    return true;
}

void DrawSceneObject(const GpuObject& object) {
    if (!Q1970ShouldRenderFullDetail(object)) return;

    bool q1810ClipNativeLod = false;
    float q1810MinX = 0.0f, q1810MaxX = 0.0f;
    float q1810MinZ = 0.0f, q1810MaxZ = 0.0f;
    if (object.q1990NativeLod &&
        gNativeLodClipEnabledLocationQ1810 >= 0 &&
        gNativeLodClipBoundsLocationQ1810 >= 0) {
        q1810ClipNativeLod =
            Q1810GetNativeLodNearClip(q1810MinX, q1810MaxX,
                                      q1810MinZ, q1810MaxZ);
    }
    if (gNativeLodClipEnabledLocationQ1810 >= 0) {
        glUniform1f(gNativeLodClipEnabledLocationQ1810,
                    q1810ClipNativeLod ? 1.0f : 0.0f);
    }
    if (q1810ClipNativeLod && gNativeLodClipBoundsLocationQ1810 >= 0) {
        glUniform4f(gNativeLodClipBoundsLocationQ1810,
                    q1810MinX, q1810MaxX, q1810MinZ, q1810MaxZ);
        static bool q1810ClipLogged = false;
        if (!q1810ClipLogged) {
            q1810ClipLogged = true;
            Q6H_LOGI("Q18.1 NATIVE LOD NEAR CLIP: activeRadius=1 boundsRenderXZ=(%.3f..%.3f, %.3f..%.3f) mode=fragment-clip-preserve-macroblock-outside-near-world",
                     q1810MinX, q1810MaxX, q1810MinZ, q1810MaxZ);
        }
    }
    glUniform1f(gGlossinessLocation, object.glossiness);
    glUniform1f(gNoLightingLocationQ1020, object.noLighting ? 1.0f : 0.0f);
    glUniform1f(gNoLightingFalloffLocationQ1160, object.noLightingFalloff ? 1.0f : 0.0f);
    glUniform4fv(gNoLightingFalloffParamsLocationQ1160, 1, object.noLightingFalloffParams);
    glUniform1f(gUseVertexColorLocationQ1020, object.useVertexColor ? 1.0f : 0.0f);
    glUniform1f(gUseVertexAlphaLocationQ1020, object.useVertexAlpha ? 1.0f : 0.0f);
    glUniform1f(gSpecularEnabledLocationQ1020, object.specularEnabled ? 1.0f : 0.0f);
    glUniform3fv(gSpecularColorLocationQ1020, 1, object.specularColor);
    glUniform3fv(gEmissiveColorLocationQ1020, 1, object.emissiveColor);
    glUniform1f(gEmissiveMultLocationQ1020, object.emissiveMult);
    glUniform1f(gGlowEnabledLocationQ1020, object.realGlow ? 1.0f : 0.0f);
    glUniform1f(gExternalEmittanceEnabledLocationQ1380,
                object.externalEmittanceEnabledQ1380 ? 1.0f : 0.0f);
    glUniform3fv(gExternalEmittanceColorLocationQ1380, 1,
                 object.externalEmittanceColorQ1380);
    glUniform1f(gNormalStrengthLocation, object.realNormal ? 1.0f : 0.0f);
    glUniform1f(gMaterialAlphaLocation, object.materialAlpha);
    glUniform1f(gAlphaTestLocation, object.alphaTest ? 1.0f : 0.0f);
    glUniform1f(gAlphaThresholdLocation, object.alphaThreshold);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, object.diffuse);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, object.normal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, object.glow);
    glBindVertexArray(object.vao);

    if (object.modelPath.find("megatonbrasslanternsign") != std::string::npos ||
        object.modelPath.find("megatonchurchofatom") != std::string::npos ||
        object.modelPath.find("signstop02") != std::string::npos) {
        Q6H_LOGI("Q12.1 CULL TARGET: model=%s stencil=%d mode=%u",
                 object.modelPath.c_str(), object.stencilDrawModePresent ? 1 : 0,
                 static_cast<unsigned>(object.stencilDrawMode));
    }

    const GLboolean q1160CullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLint q1160PreviousFrontFace = GL_CCW;
    GLint q1160PreviousCullMode = GL_BACK;
    glGetIntegerv(GL_FRONT_FACE, &q1160PreviousFrontFace);
    glGetIntegerv(GL_CULL_FACE_MODE, &q1160PreviousCullMode);

    // Gamebryo default: ordinary geometry is single-sided.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // NiStencilProperty is FO3's authored override for two-sided / reversed
    // face rendering. Preserve Q11.6's decoded draw mode exactly.
    if (object.stencilDrawModePresent) {
        if (object.stencilDrawMode == 3u) {
            glDisable(GL_CULL_FACE);
        } else {
            glFrontFace(object.stencilDrawMode == 2u ? GL_CW : GL_CCW);
        }
    }

    const GLboolean q1170OffsetWasEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    GLfloat q1170OldFactor = 0.0f, q1170OldUnits = 0.0f;
    if (object.decalQ1170) {
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &q1170OldFactor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &q1170OldUnits);
        glEnable(GL_POLYGON_OFFSET_FILL);
        // Standard (non-reversed) depth: negative offset pulls the decal toward
        // the viewer, matching the intent of Gamebryo's decal render state.
        glPolygonOffset(-0.65f, -1.0f);
    }

    glDrawArrays(GL_TRIANGLES, 0, object.vertexCount);

    if (object.decalQ1170) {
        glPolygonOffset(q1170OldFactor, q1170OldUnits);
        if (q1170OffsetWasEnabled) glEnable(GL_POLYGON_OFFSET_FILL);
        else glDisable(GL_POLYGON_OFFSET_FILL);
    }

    glFrontFace(static_cast<GLenum>(q1160PreviousFrontFace));
    glCullFace(static_cast<GLenum>(q1160PreviousCullMode));
    if (q1160CullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

bool Q1030InitializeRenderProgramOnly() {
    if (gProgram) return true;
    gProgram = CreateQ6HProgram();
    if (!gProgram) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: stage=renderer reason=shader-program");
        return false;
    }

    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    gDiffuseLocation = glGetUniformLocation(gProgram, "uDiffuse");
    gNormalLocation = glGetUniformLocation(gProgram, "uNormalGloss");
    gGlossinessLocation = glGetUniformLocation(gProgram, "uGlossiness");
    gNormalStrengthLocation = glGetUniformLocation(gProgram, "uNormalStrength");
    gMaterialAlphaLocation = glGetUniformLocation(gProgram, "uMaterialAlpha");
    gAlphaTestLocation = glGetUniformLocation(gProgram, "uAlphaTest");
    gAlphaThresholdLocation = glGetUniformLocation(gProgram, "uAlphaThreshold");

    gAmbientColorLocationQ1000 = glGetUniformLocation(gProgram, "uAmbientColor");
    gSunlightColorLocationQ1000 = glGetUniformLocation(gProgram, "uSunlightColor");
    gSunDirectionLocationQ1000 = glGetUniformLocation(gProgram, "uSunDirection");

    gEyePositionLocationQ1010 = glGetUniformLocation(gProgram, "uEyePosition");
    gFogColorLocationQ1010 = glGetUniformLocation(gProgram, "uFogColor");
    gFogNearLocationQ1010 = glGetUniformLocation(gProgram, "uFogNear");
    gFogFarLocationQ1010 = glGetUniformLocation(gProgram, "uFogFar");
    gFogPowerLocationQ1410 = glGetUniformLocation(gProgram, "uFogPower");
    gFogEnabledLocationQ1450 = glGetUniformLocation(gProgram, "uQ1450FogEnabled");
    gRenderStageLocationQ1560 = glGetUniformLocation(gProgram, "uRenderStageQ1560");
    gLegacyColourDomainLocationQ1570 = glGetUniformLocation(gProgram, "uLegacyColourDomainQ1570");
    gLegacyAmbientLocationQ1570 = glGetUniformLocation(gProgram, "uLegacyAmbientQ1570");
    gLegacySunlightLocationQ1570 = glGetUniformLocation(gProgram, "uLegacySunlightQ1570");
    gFogNearVertexLocationQ1532 = glGetUniformLocation(gProgram, "uFogNearVertexQ1532");
    gFogFarVertexLocationQ1532 = glGetUniformLocation(gProgram, "uFogFarVertexQ1532");
    gFogPowerVertexLocationQ1532 = glGetUniformLocation(gProgram, "uFogPowerVertexQ1532");
    gSunDirectionVertexLocationQ1540 = glGetUniformLocation(gProgram, "uSunDirectionVertexQ1540");
    gEyePositionVertexLocationQ1630 = glGetUniformLocation(gProgram, "uEyePositionVertexQ1630");
    gPpDiffuseDomainLocationQ1470 = glGetUniformLocation(gProgram, "uQ1470LegacyPpDiffuseDomain");
    gLocalLightCountLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightCount");
    gLocalLightPosRadiusLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightPosRadius[0]");
    gLocalLightColorFalloffLocationQ1010 = glGetUniformLocation(gProgram, "uLocalLightColorFalloff[0]");

    gGlowLocationQ1020 = glGetUniformLocation(gProgram, "uGlow");
    gNoLightingLocationQ1020 = glGetUniformLocation(gProgram, "uNoLighting");
    gNoLightingFalloffLocationQ1160 = glGetUniformLocation(gProgram, "uNoLightingFalloff");
    gNoLightingFalloffParamsLocationQ1160 = glGetUniformLocation(gProgram, "uNoLightingFalloffParams");
    gUseVertexColorLocationQ1020 = glGetUniformLocation(gProgram, "uUseVertexColor");
    gUseVertexAlphaLocationQ1020 = glGetUniformLocation(gProgram, "uUseVertexAlpha");
    gSpecularEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uSpecularEnabled");
    gSpecularColorLocationQ1020 = glGetUniformLocation(gProgram, "uSpecularColor");
    gEmissiveColorLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveColor");
    gEmissiveMultLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveMult");
    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uGlowEnabled");
    gExternalEmittanceEnabledLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceEnabledQ1380");
    gExternalEmittanceColorLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceColorQ1380");
    gNativeLodClipEnabledLocationQ1810 =
        glGetUniformLocation(gProgram, "uNativeLodClipEnabledQ1810");
    gNativeLodClipBoundsLocationQ1810 =
        glGetUniformLocation(gProgram, "uNativeLodClipBoundsQ1810");
    gLightMvpLocationQ1050 = glGetUniformLocation(gProgram, "uLightMvp");
    gShadowMapLocationQ1050 = glGetUniformLocation(gProgram, "uShadowMap");
    gShadowTexelLocationQ1050 = glGetUniformLocation(gProgram, "uShadowTexelSize");
    gShadowsEnabledLocationQ1050 = glGetUniformLocation(gProgram, "uShadowsEnabled");

    // Only the transform and diffuse sampler are mandatory to build/draw the
    // scene. GLES may legally optimise optional material uniforms to -1.
    const bool coreReady = gMvpLocation >= 0 && gDiffuseLocation >= 0;
    Q6H_LOGI("Q10.3 RENDERER READY: program=%u core=%d mvp=%d diffuse=%d normal=%d ambient=%d sun=%d source=no-house-bootstrap",
             gProgram, coreReady ? 1 : 0, gMvpLocation, gDiffuseLocation,
             gNormalLocation, gAmbientColorLocationQ1000, gSunDirectionLocationQ1000);
    return coreReady;
}

bool Q1030BootMegatonOnRender() {
    static bool attempted = false;
    static bool succeeded = false;
    if (succeeded) return true;
    if (attempted) return false;
    attempted = true;

    Q6H_LOGI("Q10.4 DIRECT BOOT BEGIN: transition=CapitalWasteland-to-Megaton worldspace=00000A74 stage=first-render");
    if (!Q1030InitializeRenderProgramOnly()) return false;
    if (!QueueFo3MegatonEntryQ1860()) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: stage=xtel reason=gate-not-found");
        return false;
    }
    Q6H_LOGI("Q10.4 DIRECT BOOT XTEL READY: destination=resolved-by-XTEL-owner source=Fallout3.esm");
    if (!ProcessQ74TransitionRequest()) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: stage=exterior-load reason=scene-swap");
        return false;
    }

    succeeded = gSceneReady && !gObjects.empty();
    Q6H_LOGI("Q10.4 DIRECT MEGATON ENTRY READY: destination=resolved-by-XTEL worldspace=00000A74 objects=%zu sceneReady=%d bootstrapCell=NONE source=Fallout3.esm/XTEL",
             gObjects.size(), gSceneReady ? 1 : 0);
    return succeeded;
}

struct Q1990NativeLodBlock {
    int32_t blockX = 0;
    int32_t blockY = 0;
    uint64_t lastUse = 0u;
    std::vector<GpuObject> terrain;
    std::vector<GpuObject> objects;
};

std::vector<Q1990NativeLodBlock> gQ1990NativeLodBlocks;
uint64_t gQ1990NativeLodSerial = 0u;
bool gQ1990NativeLodOriginValid = false;
float gQ1990NativeLodCenterX = 0.0f;
float gQ1990NativeLodCenterY = 0.0f;
float gQ1990NativeLodFloorZ = 0.0f;
int32_t gQ1990NativeLodCentreBlockX = 0;
int32_t gQ1990NativeLodCentreBlockY = 0;
bool gQ1990NativeLodDrawLogged = false;

int32_t Q1990FloorToLevel4Block(int32_t cell) {
    int32_t quotient = cell / 4;
    if (cell < 0 && (cell % 4) != 0) --quotient;
    return quotient * 4;
}

void Q1990DeleteLodGpu(GpuObject& object) {
    if (object.vbo) glDeleteBuffers(1, &object.vbo);
    if (object.vao) glDeleteVertexArrays(1, &object.vao);
    object.vbo = 0u;
    object.vao = 0u;
}

void Q1990ClearNativeLodGeometry() {
    for (Q1990NativeLodBlock& block : gQ1990NativeLodBlocks) {
        for (GpuObject& object : block.terrain) Q1990DeleteLodGpu(object);
        for (GpuObject& object : block.objects) Q1990DeleteLodGpu(object);
    }
    gQ1990NativeLodBlocks.clear();
    gQ1990NativeLodDrawLogged = false;
}

bool Q1990UploadNativeLodNif(const std::string& path,
                             float centerX, float centerY, float floorZ,
                             std::vector<GpuObject>& output,
                             size_t& outTriangles) {
    std::vector<Fo3StaticNifMesh> meshes;
    if (!LoadFo3StaticNifMeshes(path, meshes) || meshes.empty()) return false;
    bool any = false;
    for (Fo3StaticNifMesh& mesh : meshes) {
        const size_t vertexCount = mesh.positions.size() / 3u;
        if (vertexCount == 0u || mesh.indices.empty() ||
            mesh.normals.size() / 3u != vertexCount ||
            mesh.tangents.size() / 3u != vertexCount ||
            mesh.bitangents.size() / 3u != vertexCount ||
            mesh.texcoords.size() / 2u != vertexCount) {
            continue;
        }
        CpuObject cpu;
        cpu.placement.refFormId = 0u;
        cpu.placement.baseFormId = 0u;
        cpu.placement.scale = 1.0f;
        cpu.placement.modelPath = path;
        cpu.mesh = std::move(mesh);
        cpu.positionsGame.reserve(vertexCount);
        cpu.normalsGame.reserve(vertexCount);
        cpu.tangentsGame.reserve(vertexCount);
        cpu.bitangentsGame.reserve(vertexCount);
        for (size_t i = 0u; i < vertexCount; ++i) {
            cpu.positionsGame.push_back(Vec3{
                cpu.mesh.positions[i * 3u], cpu.mesh.positions[i * 3u + 1u], cpu.mesh.positions[i * 3u + 2u]});
            cpu.normalsGame.push_back(Vec3{
                cpu.mesh.normals[i * 3u], cpu.mesh.normals[i * 3u + 1u], cpu.mesh.normals[i * 3u + 2u]});
            cpu.tangentsGame.push_back(Vec3{
                cpu.mesh.tangents[i * 3u], cpu.mesh.tangents[i * 3u + 1u], cpu.mesh.tangents[i * 3u + 2u]});
            cpu.bitangentsGame.push_back(Vec3{
                cpu.mesh.bitangents[i * 3u], cpu.mesh.bitangents[i * 3u + 1u], cpu.mesh.bitangents[i * 3u + 2u]});
        }
        GpuObject gpu;
        if (!UploadCpuObject(cpu, centerX, centerY, floorZ, gpu)) {
            Q1990DeleteLodGpu(gpu);
            continue;
        }
        gpu.q1990NativeLod = true;
        outTriangles += static_cast<size_t>(gpu.vertexCount / 3);
        output.push_back(std::move(gpu));
        any = true;
    }
    return any;
}

Q1990NativeLodBlock* Q1990FindLodBlock(int32_t blockX, int32_t blockY) {
    for (Q1990NativeLodBlock& block : gQ1990NativeLodBlocks)
        if (block.blockX == blockX && block.blockY == blockY) return &block;
    return nullptr;
}

constexpr int Q1840_LEVEL4_BLOCK_CELLS = 4;
// Fallout.ini: uGridDistantCount=20. Keep that authored distant-grid horizon
// separate from the 3x3 detailed draw radius and the 5x5 resident REFR window.
constexpr int Q1840_DISTANT_GRID_RADIUS_CELLS = 20;
constexpr int Q1840_DISTANT_BLOCK_RADIUS =
    (Q1840_DISTANT_GRID_RADIUS_CELLS + Q1840_LEVEL4_BLOCK_CELLS - 1) /
    Q1840_LEVEL4_BLOCK_CELLS; // 5 Level4 blocks each direction => up to 11x11.
constexpr int Q1840_DISTANT_BLOCK_COORD_RADIUS =
    Q1840_DISTANT_BLOCK_RADIUS * Q1840_LEVEL4_BLOCK_CELLS;
constexpr size_t Q1840_DISTANT_TARGET_BLOCKS =
    static_cast<size_t>((Q1840_DISTANT_BLOCK_RADIUS * 2 + 1) *
                        (Q1840_DISTANT_BLOCK_RADIUS * 2 + 1));
constexpr size_t Q1840_DISTANT_CACHE_BLOCKS = 144u;

bool Q1990LodBlockDesired(int32_t blockX, int32_t blockY) {
    return std::abs(blockX - gQ1990NativeLodCentreBlockX) <=
               Q1840_DISTANT_BLOCK_COORD_RADIUS &&
           std::abs(blockY - gQ1990NativeLodCentreBlockY) <=
               Q1840_DISTANT_BLOCK_COORD_RADIUS;
}

void Q1990EnsureNativeLodForCell(int32_t cellX, int32_t cellY,
                                 float centerX, float centerY, float floorZ) {
    const bool sameOrigin = gQ1990NativeLodOriginValid &&
        std::fabs(gQ1990NativeLodCenterX - centerX) < 0.01f &&
        std::fabs(gQ1990NativeLodCenterY - centerY) < 0.01f &&
        std::fabs(gQ1990NativeLodFloorZ - floorZ) < 0.01f;
    if (!sameOrigin) {
        Q1990ClearNativeLodGeometry();
        gQ1990NativeLodOriginValid = true;
        gQ1990NativeLodCenterX = centerX;
        gQ1990NativeLodCenterY = centerY;
        gQ1990NativeLodFloorZ = floorZ;
    }

    const int32_t centreBlockX = Q1990FloorToLevel4Block(cellX);
    const int32_t centreBlockY = Q1990FloorToLevel4Block(cellY);
    const bool centreChanged =
        !sameOrigin ||
        centreBlockX != gQ1990NativeLodCentreBlockX ||
        centreBlockY != gQ1990NativeLodCentreBlockY;

    if (centreChanged) {
        gQ1990NativeLodCentreBlockX = centreBlockX;
        gQ1990NativeLodCentreBlockY = centreBlockY;
        ++gQ1990NativeLodSerial;
        gQ1990NativeLodDrawLogged = false;
        for (Q1990NativeLodBlock& block : gQ1990NativeLodBlocks) {
            if (Q1990LodBlockDesired(block.blockX, block.blockY))
                block.lastUse = gQ1990NativeLodSerial;
        }
        Q6H_LOGI("Q18.4 NATIVE LOD HORIZON: playerCell=(%d,%d) centreBlock=(%d,%d) radiusCells=%d radiusLevel4Blocks=%d targetBlocks=%zu source=Fallout.ini/uGridDistantCount",
                 cellX, cellY, centreBlockX, centreBlockY,
                 Q1840_DISTANT_GRID_RADIUS_CELLS,
                 Q1840_DISTANT_BLOCK_RADIUS,
                 Q1840_DISTANT_TARGET_BLOCKS);
    }

    // Pick exactly one missing authored Level4 block, nearest ring first.
    // The old 3x3 implementation synchronously loaded an entire strip when the
    // player crossed a Level4 boundary. Growing the horizon to Fallout's 20-cell
    // setting would make that catastrophic, so the far world fills progressively.
    int32_t nextBlockX = 0;
    int32_t nextBlockY = 0;
    int bestRing = 1000000;
    int bestManhattan = 1000000;
    bool missingFound = false;
    for (int dy = -Q1840_DISTANT_BLOCK_RADIUS;
         dy <= Q1840_DISTANT_BLOCK_RADIUS; ++dy) {
        for (int dx = -Q1840_DISTANT_BLOCK_RADIUS;
             dx <= Q1840_DISTANT_BLOCK_RADIUS; ++dx) {
            const int32_t bx =
                centreBlockX + dx * Q1840_LEVEL4_BLOCK_CELLS;
            const int32_t by =
                centreBlockY + dy * Q1840_LEVEL4_BLOCK_CELLS;
            if (Q1990FindLodBlock(bx, by)) continue;
            const int ring = std::max(std::abs(dx), std::abs(dy));
            const int manhattan = std::abs(dx) + std::abs(dy);
            if (!missingFound || ring < bestRing ||
                (ring == bestRing && manhattan < bestManhattan)) {
                missingFound = true;
                bestRing = ring;
                bestManhattan = manhattan;
                nextBlockX = bx;
                nextBlockY = by;
            }
        }
    }

    // Always establish the old 3x3 core (rings 0..1). Beyond that, don't let
    // far-LOD expansion compete with an active detailed/collision generation.
    if (missingFound && (bestRing <= 1 || !gExteriorStreamBusyQ1890)) {
        const auto q1840LoadStarted = std::chrono::steady_clock::now();
        Q1990NativeLodBlock block;
        block.blockX = nextBlockX;
        block.blockY = nextBlockY;
        block.lastUse = gQ1990NativeLodSerial;

        const std::string suffix =
            "Wasteland.Level4.X" + std::to_string(nextBlockX) +
            ".Y" + std::to_string(nextBlockY) + ".NIF";
        const std::string terrainPath =
            "Landscape\\LOD\\Wasteland\\" + suffix;
        const std::string objectPath =
            "Landscape\\LOD\\Wasteland\\Blocks\\" + suffix;
        size_t terrainTriangles = 0u;
        size_t objectTriangles = 0u;
        const bool terrainReady = Q1990UploadNativeLodNif(
            terrainPath, centerX, centerY, floorZ,
            block.terrain, terrainTriangles);
        const bool objectsReady = Q1990UploadNativeLodNif(
            objectPath, centerX, centerY, floorZ,
            block.objects, objectTriangles);
        const uint64_t q1840LoadUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - q1840LoadStarted).count());

        Q6H_LOGI("Q18.4 NATIVE LOD BLOCK READY: block=(%d,%d) ring=%d terrainReady=%d terrainShapes=%zu terrainTriangles=%zu objectsReady=%d objectShapes=%zu objectTriangles=%zu loadUs=%llu detailedStreamBusy=%d",
                 nextBlockX, nextBlockY, bestRing,
                 terrainReady ? 1 : 0, block.terrain.size(), terrainTriangles,
                 objectsReady ? 1 : 0, block.objects.size(), objectTriangles,
                 static_cast<unsigned long long>(q1840LoadUs),
                 gExteriorStreamBusyQ1890 ? 1 : 0);
        // Empty authored coordinates are cached too, so we do not probe them
        // every frame while filling the distant horizon.
        gQ1990NativeLodBlocks.push_back(std::move(block));
    }

    while (gQ1990NativeLodBlocks.size() > Q1840_DISTANT_CACHE_BLOCKS) {
        size_t victim = gQ1990NativeLodBlocks.size();
        uint64_t oldest = UINT64_MAX;
        for (size_t i = 0u; i < gQ1990NativeLodBlocks.size(); ++i) {
            const Q1990NativeLodBlock& block = gQ1990NativeLodBlocks[i];
            if (Q1990LodBlockDesired(block.blockX, block.blockY)) continue;
            if (block.lastUse < oldest) {
                oldest = block.lastUse;
                victim = i;
            }
        }
        if (victim >= gQ1990NativeLodBlocks.size()) break;
        for (GpuObject& object : gQ1990NativeLodBlocks[victim].terrain)
            Q1990DeleteLodGpu(object);
        for (GpuObject& object : gQ1990NativeLodBlocks[victim].objects)
            Q1990DeleteLodGpu(object);
        gQ1990NativeLodBlocks.erase(
            gQ1990NativeLodBlocks.begin() +
            static_cast<std::ptrdiff_t>(victim));
    }

    size_t desiredLoaded = 0u;
    for (const Q1990NativeLodBlock& block : gQ1990NativeLodBlocks)
        if (Q1990LodBlockDesired(block.blockX, block.blockY))
            ++desiredLoaded;

    static size_t q1840LastLoggedLoaded = static_cast<size_t>(-1);
    static int32_t q1840LastLoggedCentreX = INT32_MIN;
    static int32_t q1840LastLoggedCentreY = INT32_MIN;
    if (desiredLoaded != q1840LastLoggedLoaded ||
        centreBlockX != q1840LastLoggedCentreX ||
        centreBlockY != q1840LastLoggedCentreY) {
        q1840LastLoggedLoaded = desiredLoaded;
        q1840LastLoggedCentreX = centreBlockX;
        q1840LastLoggedCentreY = centreBlockY;
        Q6H_LOGI("Q18.4 NATIVE LOD WINDOW: playerCell=(%d,%d) centreBlock=(%d,%d) desiredLoaded=%zu/%zu cachedBlocks=%zu radiusCells=%d stagedOneBlockPerFrame=1 outerLoadsYieldToDetailedStreaming=1",
                 cellX, cellY, centreBlockX, centreBlockY,
                 desiredLoaded, Q1840_DISTANT_TARGET_BLOCKS,
                 gQ1990NativeLodBlocks.size(),
                 Q1840_DISTANT_GRID_RADIUS_CELLS);
    }
}

void Q1990RenderNativeLod(bool alphaPass) {
    if (gExteriorWorldspaceQ1890 != 0x0000003Cu || gQ1990NativeLodBlocks.empty()) return;
    size_t drawnShapes = 0u;
    size_t drawnTriangles = 0u;
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 6.0f);
    for (Q1990NativeLodBlock& block : gQ1990NativeLodBlocks) {
        if (!Q1990LodBlockDesired(block.blockX, block.blockY)) continue;
        // The current 4x4 macroblock is completely covered by the actual-centred
        // 7x7 LAND runway, so its coarse terrain stays suppressed. Q18.1 clips
        // all remaining native LOD fragments against the exact active 3x3 near
        // rectangle in DrawSceneObject, preventing coarse object LOD from
        // overlapping detailed REFR geometry while preserving the same macroblock
        // outside the near cells.
        if (block.blockX != gQ1990NativeLodCentreBlockX ||
            block.blockY != gQ1990NativeLodCentreBlockY) {
            for (const GpuObject& object : block.terrain) {
                if (object.alphaBlend != alphaPass) continue;
                DrawSceneObject(object);
                ++drawnShapes;
                drawnTriangles += static_cast<size_t>(object.vertexCount / 3);
            }
        }
        for (const GpuObject& object : block.objects) {
            if (object.alphaBlend != alphaPass) continue;
            DrawSceneObject(object);
            ++drawnShapes;
            drawnTriangles += static_cast<size_t>(object.vertexCount / 3);
        }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (!alphaPass && !gQ1990NativeLodDrawLogged) {
        gQ1990NativeLodDrawLogged = true;
        Q6H_LOGI("Q18.4 NATIVE LOD DRAW: shapes=%zu triangles=%zu radiusCells=20 currentTerrainBlockSuppressed=1 polygonOffset=1 shadows=0",
                 drawnShapes, drawnTriangles);
    }
}

void RenderScene() {
    if (!gSceneReady) Q1030BootMegatonOnRender();
    ProcessQ74TransitionRequest();
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

    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0, previousTexture3 = 0;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture3);

    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean depthTestWasEnabledQ1200 = glIsEnabled(GL_DEPTH_TEST);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    GLint previousBlendSrcRgb = GL_ONE, previousBlendDstRgb = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE, previousBlendDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp);
    glUniform1i(gDiffuseLocation, 0);
    glUniform1i(gNormalLocation, 1);
    glUniform1i(gGlowLocationQ1020, 2);
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    if (q1000Env.valid) {
        float q1500Ambient[3]{q1000Env.ambient[0], q1000Env.ambient[1], q1000Env.ambient[2]};
        if (GetFo3LegacyPpDiffuseDomainQ1470()) {
            const float q1500AmbientLuma =
                0.2126f * q1500Ambient[0] + 0.7152f * q1500Ambient[1] + 0.0722f * q1500Ambient[2];
            q1500Ambient[0] = q1500AmbientLuma * 0.65f;
            q1500Ambient[1] = q1500AmbientLuma * 0.65f;
            q1500Ambient[2] = q1500AmbientLuma * 0.65f;
        }
        glUniform3fv(gAmbientColorLocationQ1000, 1, q1500Ambient);
        glUniform3fv(gSunlightColorLocationQ1000, 1, q1000Env.sunlight);
        glUniform3fv(gSunDirectionLocationQ1000, 1, q1000Env.sunDirection);
    } else {
        glUniform3f(gAmbientColorLocationQ1000, 0.34f, 0.34f, 0.34f);
        glUniform3f(gSunlightColorLocationQ1000, 0.66f, 0.66f, 0.66f);
        glUniform3f(gSunDirectionLocationQ1000, 0.35f, 0.85f, 0.40f);
    }


    glUniform3fv(gEyePositionLocationQ1010, 1, gFo3EyePositionQ1010);
    glUniform1i(gLocalLightCountLocationQ1010, gFo3SelectedLightCountQ1010);
    glUniform4fv(gLocalLightPosRadiusLocationQ1010, FO3_SHADER_LIGHTS_Q1010,
                 gFo3SelectedLightPosRadiusQ1010);
    glUniform4fv(gLocalLightColorFalloffLocationQ1010, FO3_SHADER_LIGHTS_Q1010,
                 gFo3SelectedLightColorFalloffQ1010);
    if (gFogPowerLocationQ1410 >= 0) glUniform1f(gFogPowerLocationQ1410, GetFo3FogPowerQ1410());
    if (gFogEnabledLocationQ1450 >= 0) {
        glUniform1f(gFogEnabledLocationQ1450, GetFo3FogEnabledQ1450() ? 1.0f : 0.0f);
    }
    if (gRenderStageLocationQ1560 >= 0) {
        glUniform1i(gRenderStageLocationQ1560, GetFo3RenderStageQ1560());
    }
    if (gLegacyColourDomainLocationQ1570 >= 0) {
        glUniform1f(gLegacyColourDomainLocationQ1570,
                    GetFo3LegacyColourDomainQ1570() ? 1.0f : 0.0f);
    }
    Q1590UploadPcSp17LightConstants();
    if (gLegacyAmbientLocationQ1570 >= 0 && gLegacySunlightLocationQ1570 >= 0) {
        float q1570RawAmbient[3]{0.34f, 0.34f, 0.34f};
        float q1570RawSunlight[3]{0.66f, 0.66f, 0.66f};
        RefreshFo3RawWeatherLightingQ1480();
        Q1580LogLightTrace(q1000Env);
        const bool q1570RawReady =
            GetFo3RawWeatherLightingQ1480(q1570RawAmbient, q1570RawSunlight);
        glUniform3fv(gLegacyAmbientLocationQ1570, 1, q1570RawAmbient);
        glUniform3fv(gLegacySunlightLocationQ1570, 1, q1570RawSunlight);

        static int q1570LastLoggedMode = -1;
        const int q1570Mode = GetFo3LegacyColourDomainQ1570() ? 1 : 0;
        if (q1570Mode != q1570LastLoggedMode) {
            q1570LastLoggedMode = q1570Mode;
            Q6H_LOGI("Q15.7 LEGACY DOMAIN: mode=%s rawReady=%d ambient=(%.3f %.3f %.3f) sunlight=(%.3f %.3f %.3f) scope=statics+LAND baseMapEncoded=1 outputLinear=1 fakeLighting=0",
                     GetFo3LegacyColourDomainNameQ1570(), q1570RawReady ? 1 : 0,
                     q1570RawAmbient[0], q1570RawAmbient[1], q1570RawAmbient[2],
                     q1570RawSunlight[0], q1570RawSunlight[1], q1570RawSunlight[2]);
        }
    }
    const float q1532StaticFogNear =
        (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f)
            ? std::max(0.0f, q1000Env.fogNear / FO3_UNITS_PER_METRE)
            : 10000.0f;
    const float q1532StaticFogFar =
        (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f)
            ? std::max(0.1f, q1000Env.fogFar / FO3_UNITS_PER_METRE)
            : 10001.0f;
    if (gFogNearVertexLocationQ1532 >= 0)
        glUniform1f(gFogNearVertexLocationQ1532, q1532StaticFogNear);
    if (gFogFarVertexLocationQ1532 >= 0)
        glUniform1f(gFogFarVertexLocationQ1532, q1532StaticFogFar);
    if (gFogPowerVertexLocationQ1532 >= 0)
        glUniform1f(gFogPowerVertexLocationQ1532, GetFo3FogPowerQ1410());
    if (gSunDirectionVertexLocationQ1540 >= 0) {
        if (q1000Env.valid) {
            glUniform3fv(gSunDirectionVertexLocationQ1540, 1, q1000Env.sunDirection);
        } else {
            glUniform3f(gSunDirectionVertexLocationQ1540, 0.35f, 0.85f, 0.40f);
        }
    }
    if (gEyePositionVertexLocationQ1630 >= 0) {
        glUniform3fv(gEyePositionVertexLocationQ1630, 1, gFo3EyePositionQ1010);
    }
    if (gPpDiffuseDomainLocationQ1470 >= 0) {
        glUniform1f(gPpDiffuseDomainLocationQ1470, 0.0f);
    }
    static bool q1470ReadyLogged = false;
    if (!q1470ReadyLogged) {
        q1470ReadyLogged = true;
        Q6H_LOGI("Q15.1 AMBIENT STRENGTH A/B READY: mode=%s control=LEFT_Y scope=statics+LAND ambientNeutralLuminance=Rec709Linear ambientScale=0.65 authoredSunlightRGB=1 baseMapUnchanged=1 localLightsUnchanged=1 fogUnchanged=1 postUnchanged=1",
                 GetFo3PpDiffuseDomainNameQ1470());
    }
    static bool q1450FogReadyLogged = false;
    if (!q1450FogReadyLogged && q1000Env.valid) {
        q1450FogReadyLogged = true;
        Q6H_LOGI("Q15.6 RENDER STAGE READY: mode=%s control=LEFT_X weather=%08X fogRGB=(%.3f %.3f %.3f) nearGame=%.1f farGame=%.1f nearRender=%.3f farRender=%.3f power=%.3f stages=RAW_FOG_HDR_FINAL authoredLightingOnly=1 tangentABRetired=1",
                 GetFo3FogModeNameQ1450(), q1000Env.weatherFormId,
                 q1000Env.fog[0], q1000Env.fog[1], q1000Env.fog[2],
                 q1000Env.fogNear, q1000Env.fogFar,
                 q1000Env.fogNear / FO3_UNITS_PER_METRE,
                 q1000Env.fogFar / FO3_UNITS_PER_METRE,
                 GetFo3FogPowerQ1410());
    }
    if (q1000Env.valid && q1000Env.fogFar > q1000Env.fogNear + 1.0f) {
        glUniform3fv(gFogColorLocationQ1010, 1, q1000Env.fog);
        glUniform1f(gFogNearLocationQ1010, std::max(0.0f, q1000Env.fogNear / FO3_UNITS_PER_METRE));
        glUniform1f(gFogFarLocationQ1010, std::max(0.1f, q1000Env.fogFar / FO3_UNITS_PER_METRE));
    } else {
        glUniform3f(gFogColorLocationQ1010, 0.0f, 0.0f, 0.0f);
        glUniform1f(gFogNearLocationQ1010, 10000.0f);
        glUniform1f(gFogFarLocationQ1010, 10001.0f);
    }

    const bool q1050ShadowActive = false;
    static bool q1430Logged = false;
    if (!q1430Logged) {
        q1430Logged = true;
        Q6H_LOGI("Q14.3 VANILLA EXTERIOR SHADOWS: staticArchCast=0 landCast=0 directionalLambert=1 actorShadowScope=unchanged q1050WorldShadowMap=disabled tint=none source=FO3-default-render-semantics");
    }
    if (gLightMvpLocationQ1050 >= 0) glUniformMatrix4fv(gLightMvpLocationQ1050,1,GL_FALSE,gLightMvpQ1050);
    if (gShadowMapLocationQ1050 >= 0) glUniform1i(gShadowMapLocationQ1050,3);
    if (gShadowTexelLocationQ1050 >= 0) glUniform2f(gShadowTexelLocationQ1050,1.0f/Q1050_SHADOW_SIZE,1.0f/Q1050_SHADOW_SIZE);
    if (gShadowsEnabledLocationQ1050 >= 0) glUniform1f(gShadowsEnabledLocationQ1050,q1050ShadowActive?1.0f:0.0f);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D,q1050ShadowActive?gShadowDepthQ1050:0u);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_BLEND);
    Q1990RenderNativeLod(false);
    for (const GpuObject& object : gObjects) {
        if (object.alphaBlend) continue;
        if (object.zBufferTestQ1200) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        DrawSceneObject(object);
    }

    glEnable(GL_BLEND);
    Q1990RenderNativeLod(true);
    for (const GpuObject& object : gObjects) {
        if (!object.alphaBlend) continue;
        if (object.zBufferTestQ1200) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        glBlendFunc(Q1150BlendFactor(object.alphaSourceBlend, true),
                    Q1150BlendFactor(object.alphaDestBlend, false));
        DrawSceneObject(object);
    }

    glDepthMask(previousDepthMask);
    if (depthTestWasEnabledQ1200) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glBlendFuncSeparate(static_cast<GLenum>(previousBlendSrcRgb),
                        static_cast<GLenum>(previousBlendDstRgb),
                        static_cast<GLenum>(previousBlendSrcAlpha),
                        static_cast<GLenum>(previousBlendDstAlpha));
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);

    RenderFo3CollisionOverlay(mvp);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindVertexArray(static_cast<GLuint>(mainVao));
    glUseProgram(static_cast<GLuint>(mainProgram));

    if (!gLoggedFirstDraw) {
        gLoggedFirstDraw = true;
        Q6H_LOGI("Q6H VISIBLE: %zu Bethesda-placed Megaton objects submitted to both-eye OpenXR render path using ESM transforms",
                 gObjects.size());
    }
}

bool GetFo3FogEnabledQ1450Bridge() {
    return GetFo3FogEnabledQ1450();
}

int GetFo3RenderStageQ1560Bridge() {
    return GetFo3RenderStageQ1560();
}

void Q6HGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    glGenFramebuffers(n, framebuffers);
    static bool logged = false;
    if (!logged) {
        logged = true;
        Q6H_LOGI("Q10.3 FRAMEBUFFER READY: exteriorBoot=deferred-to-first-render bootstrapCell=NONE");
    }
}

GLuint q1280PostFbo = 0u;
GLuint q1280PostColor = 0u;
GLuint q1370PostDepth = 0u;
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
GLint q1350ExposureLocation = -1;
GLint q1370DepthLocation = -1;
GLint q1520TargetLumLocation = -1;
GLint q1560PostRenderStageLocation = -1;
GLint q1670PcBloomLocation = -1;
GLint q1670PcBloomReadyLocation = -1;

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
            vec3 currentAverageQ1520 = vec3(0.0);
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    vec2 uv = (vec2(float(x), float(y)) + vec2(0.5)) / 4.0;
                    currentAverageQ1520 += max(texture(uScene, uv).rgb, vec3(0.0));
                }
            }
            currentAverageQ1520 *= (1.0 / 16.0);

            vec3 previousAverageQ1520 = max(texture(uPrev, vec2(0.5)).rgb, vec3(0.0));

            // ISHDRADAPT uses p = pow(EyeAdaptSpeed, TimingData.z), then
            // (1-p)*previous + p*current. Q15.2 uses one settled comparison step
            // per stereo frame while TimingData.z's CPU feed is traced; steady
            // state is identical and that is the visual target of this build.
            float pQ1520 = clamp(uEyeAdaptSpeed, 0.0, 1.0);
            vec3 adaptedQ1520 = (uFirstFrame != 0)
                ? currentAverageQ1520
                : mix(previousAverageQ1520, currentAverageQ1520, pQ1520);

            float adaptedMagnitudeQ1520 = length(adaptedQ1520);
            float safeMagnitudeQ1520 = max(0.01, adaptedMagnitudeQ1520);
            float clampedMagnitudeQ1520 = min(safeMagnitudeQ1520, max(uTargetLum, 0.01));
            adaptedQ1520 *= clampedMagnitudeQ1520 / safeMagnitudeQ1520;

            // RGB is the adapted/clamped vector itself, matching ISHDRADAPT.
            // Alpha carries current magnitude only for diagnostics.
            fragColor = vec4(adaptedQ1520,
                             clamp(length(currentAverageQ1520) * 0.25, 0.0, 1.0));
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
    const uint8_t initialPixel[4]{0u, 0u, 0u, 255u};
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
        Q6H_LOGI("Q15.2 SP17 HDR ADAPT READY: probe=4x4-log-average history=RGBA8-packed16 gpuOnly=1 update=eye0-once-per-stereo-frame exposureClamp=0.750..1.350 semantics=eyeAdapt-retention targetLumBridge=quarter-scale sqrtResponse=1");
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
    const float targetLum = image.valid ? std::clamp(image.hdrTargetLum, 0.001f, 4.0f) : 1.0f;
    const float upperLum = image.valid ? std::clamp(image.hdrUpperLumClamp, 0.01f, 4.0f) : 1.0f;
    const float eyeSpeed = image.valid ? std::clamp(image.hdrEyeAdaptSpeed, 0.0f, 1.0f) : 0.5f;
    glUniform1f(q1350AdaptTargetLocation, upperLum);
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
        Q6H_LOGI("Q15.2 SP17 HDR HISTORY: sceneLum=%.4f targetLum=%.3f desiredExposure=%.4f adaptedExposure=%.4f eyeAdaptSpeed=%.3f update=%llu stereoShared=1 gpuHistory=1 mapping=sqrt(targetQuarter/logAverage) clamp=0.75..1.35",
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
        uniform sampler2D uExposureQ1350;
        uniform sampler2D uDepthQ1370;
        uniform float uTargetLumQ1520;
        uniform int uRenderStageQ1560;
        uniform sampler2D uPcBloomQ1670;
        uniform float uPcBloomReadyQ1670;
        out vec4 fragColor;

        float Q1280Lum(vec3 c) {
            return dot(c, vec3(0.2126, 0.7152, 0.0722));
        }

        vec3 Q1280Bright(vec2 uv) {
            vec3 c = texture(uScene, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
            // shaderpackage017 / ISHDRBRIGHT.pso:
            // max(Src0.rgb - HDRParam.x, 0) * HDRParam.y
            return max(c - vec3(max(uBloomThreshold, 0.0)), vec3(0.0)) *
                   max(uBloomScale, 0.0);
        }

        vec3 Q1340LinearToSrgb(vec3 c) {
            c = max(c, vec3(0.0));
            bvec3 low = lessThanEqual(c, vec3(0.0031308));
            vec3 lo = c * 12.92;
            vec3 hi = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
            return mix(hi, lo, vec3(low));
        }

        vec3 Q1340SrgbToLinear(vec3 c) {
            c = max(c, vec3(0.0));
            bvec3 low = lessThanEqual(c, vec3(0.04045));
            vec3 lo = c / 12.92;
            vec3 hi = pow((c + 0.055) / 1.055, vec3(2.4));
            return mix(hi, lo, vec3(low));
        }


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

        void main() {
            vec3 colour = texture(uScene, vUv).rgb;

            // RAW and FOG isolate the pre-post scene exactly. Fog itself is
            // already controlled in the world shaders by the same stage value.
            if (uRenderStageQ1560 <= 1) {
                fragColor = vec4(max(colour, vec3(0.0)), 1.0);
                return;
            }

            // Q15.17 / PC calls 4618165 + 4618189: Src0 is already the
            // 256x256 bright-pass Gaussian result. Final call 4618221 samples it
            // linearly at the presentation UV.
            vec3 q1520HdrBright = uPcBloomReadyQ1670 > 0.5
                ? texture(uPcBloomQ1670, vUv).rgb
                : vec3(0.0);

            // Q13.4: Fallout's cinematic controls are final-frame controls. Do
            // not run them against raw linear HDR radiance: contrast around an
            // authored average luminance near 1.0 can otherwise send ordinary
            // dark surfaces negative and the old final max() turns them black.
            // Keep bloom/lighting linear, temporarily enter display transfer
            // space for the film controls, then return to linear for the sRGB
            // OpenXR target.
            float contactAoQ1370 = Q1370ContactAo(vUv, texture(uDepthQ1370, vUv).r);
            float aoLumQ1370 = Q1280Lum(max(colour, vec3(0.0)));
            float aoMaterialMaskQ1370 = 1.0 - smoothstep(0.70, 1.35, aoLumQ1370);
            colour *= mix(1.0, contactAoQ1370, aoMaterialMaskQ1370);

            // shaderpackage017 / ISHDRBLENDINSHADER(CIN): Src0.a in vanilla
            // carries the adapted HDR magnitude through the blur chain. Quest
            // samples the retained adapted RGB history and reconstructs that
            // same magnitude directly.
            vec3 adaptedRgbQ1520 = max(texture(uExposureQ1350, vec2(0.5)).rgb,
                                       vec3(0.0));
            float adaptedMagnitudeQ1520 = max(length(adaptedRgbQ1520), 0.01);
            float targetLumQ1520 = max(uTargetLumQ1520, 0.001);
            float denomQ1520 = max(adaptedMagnitudeQ1520, targetLumQ1520);
            float bloomWeightQ1520 = 0.5 / denomQ1520;
            float sceneWeightQ1520 = targetLumQ1520 / denomQ1520;
            colour = max(q1520HdrBright * bloomWeightQ1520, vec3(0.0)) +
                     colour * sceneWeightQ1520;

            // Q15.14 / PC call 4618221: literal captured final-film arithmetic.
            // The PC performs this directly on the FP16 HDR-combined values.
            vec3 q1640PcOutput = colour;
            float q1640Lum = dot(q1640PcOutput,
                                 vec3(0.298999995, 0.587000012, 0.114));

            // lrp r1.xyz, c19.x, r0, r0.w ; c19.x = 0.875 saturation
            q1640PcOutput = mix(vec3(q1640Lum), q1640PcOutput, 0.875);

            // mad/mad tint pair with c20 =
            // (0.7399142, 0.5749559, 0.3128335, 0.6).
            vec3 q1640TintTarget = q1640Lum *
                vec3(0.7399142, 0.5749559, 0.3128335);
            q1640PcOutput = mix(q1640PcOutput, q1640TintTarget, 0.6);

            // c19.w brightness=1.1, c19.y contrastAverage=0,
            // c19.z contrast=1.02. Fade c22=(0,0,0,0), so Fade is identity.
            q1640PcOutput = (q1640PcOutput * 1.1 - vec3(0.0)) * 1.02 + vec3(0.0);

            // X8R8G8B8 clamps on the PC. Match that numeric result before
            // compensating for Quest's sRGB swapchain storage conversion.
            q1640PcOutput = clamp(q1640PcOutput, vec3(0.0), vec3(1.0));

            // PC: X8R8G8B8 + D3DRS_SRGBWRITEENABLE=0, so its shader numeric
            // value becomes the stored/display code directly. Quest prefers an
            // sRGB OpenXR attachment; inverse-transfer here so attachment encode
            // lands on the identical final code value.
            // Q15.15: preserve the exact numeric code value emitted by the PC
            // X8R8G8B8/SRGBWRITE=0 path. Do not apply a second transfer here.
            colour = q1640PcOutput;
            fragColor = vec4(max(colour, vec3(0.0)), 1.0);
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
    q1350ExposureLocation = glGetUniformLocation(q1280PostProgram, "uExposureQ1350");
    q1370DepthLocation = glGetUniformLocation(q1280PostProgram, "uDepthQ1370");
    q1520TargetLumLocation = glGetUniformLocation(q1280PostProgram, "uTargetLumQ1520");
    q1560PostRenderStageLocation = glGetUniformLocation(q1280PostProgram, "uRenderStageQ1560");
    q1670PcBloomLocation = glGetUniformLocation(q1280PostProgram, "uPcBloomQ1670");
    q1670PcBloomReadyLocation = glGetUniformLocation(q1280PostProgram, "uPcBloomReadyQ1670");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0 &&
           q1520TargetLumLocation >= 0 && q1560PostRenderStageLocation >= 0 &&
           q1670PcBloomLocation >= 0 && q1670PcBloomReadyLocation >= 0;
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
    if (!q1370PostDepth) glGenTextures(1, &q1370PostDepth);

    if (q1280PostWidth == width && q1280PostHeight == height &&
        q1280PostColor && q1370PostDepth) return true;

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

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Q6H_LOGE("Q12.8 POST target incomplete: status=0x%X size=%dx%d",
                 status, width, height);
        return false;
    }

    q1280PostWidth = width;
    q1280PostHeight = height;
    if (!q1280PostLoggedGpu) {
        q1280PostLoggedGpu = true;
        Q6H_LOGI("Q13.7 CONTACT AO READY: size=%dx%d depth=DEPTH_COMPONENT24 sampleable=1 taps=8 maxDarken=0.220 radiusPx=2.5..7.0 skyExcluded=1 emissiveProtected=1 stereoSequential=1",
                 width, height);
        Q6H_LOGI("Q12.8 POST GPU READY: size=%dx%d format=%s pcBloom=Q15.17-640x256-256x256-15tapV-15tapH stereoSequential=1",
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
    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0, previousTexture3 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture3);
    glActiveTexture(GL_TEXTURE0);
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    const bool q1670BloomReadyQ1670 = Q1670RenderPcBloomQ1670();

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
    if (Q1350EnsureAdaptationQ1350()) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, q1350AdaptTexture[q1350AdaptIndex]);
        glUniform1i(q1350ExposureLocation, 1);
        glActiveTexture(GL_TEXTURE0);
    }
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, q1370PostDepth);
    glUniform1i(q1370DepthLocation, 2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, q1670BloomTexture[1]);
    glUniform1i(q1670PcBloomLocation, 3);
    glUniform1f(q1670PcBloomReadyLocation, q1670BloomReadyQ1670 ? 1.0f : 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(q1280TexelLocation,
                1.0f / static_cast<float>(std::max<GLsizei>(width, 1)),
                1.0f / static_cast<float>(std::max<GLsizei>(height, 1)));

    const Fo3ImageSpaceQ1280& image = GetFo3ImageSpaceQ1280();
    const int q1560Stage = GetFo3RenderStageQ1560();
    glUniform1i(q1560PostRenderStageLocation, q1560Stage);
    glUniform1f(q1520TargetLumLocation, 1.2f);
    // Stage 2 keeps HDR/adaptation but removes only cinematic IMGS controls.
    const int flags = (q1560Stage >= FO3_RENDER_STAGE_FINAL_Q1560 && image.valid)
        ? static_cast<int>(image.cinematicFlags)
        : 0;
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

    static bool q1640Logged = false;
    if (!q1640Logged) {
        q1640Logged = true;
        Q6H_LOGI("Q15.15 PC HDR OUTPUT DOMAIN: call=4618221 targetLum=1.200 saturation=0.875 tint=(0.739914 0.574956 0.312834) tintValue=0.600 contrastAvg=0.000 contrast=1.020 brightness=1.100 fade=0 lum=REC601 pcTarget=X8R8G8B8 pcSrgbWrite=0 questSrgbCompensation=0 directPcCodeWrite=1");
    }

    static uint32_t lastLoggedImageSpace = 0xFFFFFFFFu;
    const uint32_t currentImageSpace = image.valid ? image.imageSpaceFormId : 0u;
    if (lastLoggedImageSpace != currentImageSpace) {
        lastLoggedImageSpace = currentImageSpace;
        Q6H_LOGI("Q13.4 HDR POST ACTIVE: IMGS=%08X flags=0x%02X saturation=%.3f contrastAvg=%.3f contrast=%.3f brightness=%.3f tintValue=%.3f bloomExterior=%.3f bloomApplied=%.3f targetLum=%.3f upperLum=%.3f eyeAdaptSpeed=%.3f cinematicDomain=vanilla-native isc=ISCinematic weights=0.299/0.587/0.114 order=sat-tint-brightness-contrast fade=neutral eyeAdapt=q152-sp17-rgb-vector",
                 currentImageSpace, image.valid ? image.cinematicFlags : 0u,
                 image.valid ? image.cinematicSaturation : 1.0f,
                 image.valid ? image.cinematicContrastAvgLum : 0.5f,
                 image.valid ? image.cinematicContrast : 1.0f,
                 image.valid ? image.cinematicBrightness : 1.0f,
                 image.valid ? image.cinematicTintValue : 0.0f,
                 image.valid ? image.bloomAlphaExterior : 0.0f,
                 bloomAlpha * 0.35f,
                 image.valid ? image.hdrTargetLum : 1.0f,
                 image.valid ? image.hdrUpperLumClamp : 1.0f,
                 image.valid ? image.hdrEyeAdaptSpeed : 0.5f);
    }

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture3));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture2));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture1));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture0));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    glDepthMask(previousDepthMask);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    q1280PostActive = false;
}

void Q1350CompositeEyePostQ1350(uint32_t eyeIndex, GLuint swapchainFbo,
                                GLsizei width, GLsizei height) {
    if (eyeIndex == 0u) Q1350UpdateExposureQ1350();
    Q1280CompositeEyePostQ1280(swapchainFbo, width, height);
}

void Q1280ShutdownPostQ1280() {
    Q1670ShutdownPcBloomQ1670();
    if (q1370PostDepth) glDeleteTextures(1, &q1370PostDepth);
    q1370PostDepth = 0u;
    if (q1350AdaptTexture[0] || q1350AdaptTexture[1]) glDeleteTextures(2, q1350AdaptTexture);
    if (q1350AdaptFbo) glDeleteFramebuffers(1, &q1350AdaptFbo);
    if (q1350AdaptVao) glDeleteVertexArrays(1, &q1350AdaptVao);
    if (q1350AdaptProgram) glDeleteProgram(q1350AdaptProgram);
    q1350AdaptTexture[0] = q1350AdaptTexture[1] = 0u;
    q1350AdaptFbo = q1350AdaptVao = q1350AdaptProgram = 0u;
    q1350AdaptFrame = 0u;
    q1350AdaptIndex = 0;

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

void Q6HDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    Q1280ShutdownPostQ1280();
    glDeleteFramebuffers(n, framebuffers);
    ShutdownFo3CollisionOverlay();
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
    if (gShadowProgramQ1050) glDeleteProgram(gShadowProgramQ1050);
    if (gShadowDepthQ1050) glDeleteTextures(1,&gShadowDepthQ1050);
    if (gShadowFboQ1050) glDeleteFramebuffers(1,&gShadowFboQ1050);
    gShadowProgramQ1050=0; gShadowDepthQ1050=0; gShadowFboQ1050=0;
    gShadowReadyQ1050=false; gShadowDirtyQ1050=true;
    SetFo3TerrainShadowQ1050(0,nullptr,false);
    if (gProgram) glDeleteProgram(gProgram);
    if (gDepthRenderbuffer) glDeleteRenderbuffers(1, &gDepthRenderbuffer);
    gProgram = 0;
    gDepthRenderbuffer = 0;
    gSceneReady = false;
    gLoggedFirstDraw = false;
    gQ1380ExternalFlagShapes = 0u;
    gQ1380ExternalResolvedShapes = 0u;
    gQ1380ExternalFixedShapes = 0u;
    gQ1380ExternalRegionShapes = 0u;
    ResetFo3ExternalEmittanceQ1380();
}

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

void Q6HDisable(GLenum cap) {
    if (cap == GL_DEPTH_TEST) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        return;
    }
    glDisable(cap);
}

void Q6HClear(GLbitfield mask) {
    glClear(mask | GL_DEPTH_BUFFER_BIT);
}

void Q6HDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (mode == GL_TRIANGLES && count == 3 && (first == 0 || first == 3)) return;
    if (mode == GL_TRIANGLES && count == 3 && first == 6) {
        RenderScene();
        return;
    }
    glDrawArrays(mode, first, count);
}

} // namespace

bool QueryFo3DoorAimQ1700(float originX, float originY, float originZ,
                          float dirX, float dirY, float dirZ,
                          Fo3DoorAimQ1700* outAim) {
    return QueryDoorInternalQ1700(originX, originY, originZ,
                                  dirX, dirY, dirZ, outAim);
}

bool ActivateFo3DoorQ1700(float originX, float originY, float originZ,
                          float dirX, float dirY, float dirZ) {
    return ActivateDoorInternalQ1700(originX, originY, originZ,
                                     dirX, dirY, dirZ);
}

#define glGenFramebuffers Q6HGenFramebuffers
#define glDeleteFramebuffers Q6HDeleteFramebuffers
#define glViewport Q6HViewport
#define glDisable Q6HDisable
#define glClear Q6HClear
#define glDrawArrays Q6HDrawArrays
#include "fo3-runtime-loop.inc"
#undef glDrawArrays
#undef glClear
#undef glDisable
#undef glViewport
#undef glDeleteFramebuffers
#undef glGenFramebuffers

// Q16.15 XTEL CACHE: verified by PrimeFo3AuthoredDoorAnchorsQ1870 live hook.
