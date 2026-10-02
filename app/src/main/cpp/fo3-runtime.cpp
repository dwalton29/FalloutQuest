#include "fo3-water-q2070.h"
void SetFo3WorldspaceGridRadiusOverrideQ1950(int radius);
void SetNextFo3CollisionExteriorModeQ1931(bool exterior);
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
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
#include "fo3-npc-q23.h"
#include "fo3-static-nif.h"
#include "fo3-bsa-reader.h"
#include "fo3-texture-bsa.h"
#include "fo3-collision-overlay.h"
#include "fo3-nif-collision-q6f.h"
#include "fo3-transition-q74.h"
#include "fo3-environment-q1000.h"
#include "fo3-imagespace-q1280.h"
#include "fo3-weather-imad-q1300.h"
#include "fo3-weather-light-q1320.h"
#include "fo3-pc-sky-q1660.h"
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
#include <deque>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
struct GpuObject;
bool Q1970ShouldRenderFullDetail(const GpuObject& object);
bool Q220IsLooseRecordType(const std::string& type);
void Q1970ProbeNativeLod(float gameX, float gameY);
void Q2022ProbeLodArchive(int32_t cellX, int32_t cellY);
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

// Q20.8A water executes before the post-process declarations later in this
// translation unit. Forward-declare the existing Q20.6 resolve state here.
extern GLuint q1280PostFbo;
extern GLuint q1280PostColor;
extern GLuint q1370PostDepth;
extern GLsizei q1280PostWidth;
extern GLsizei q1280PostHeight;
extern GLuint q2060MsaaFbo;
extern GLsizei q2060MsaaSamples;
extern bool q2060MsaaActive;
void Q2060ResolveEyeMsaaQ2060();

extern bool gQ1920LatestGridValid;
extern int32_t gQ1920LatestGridX;
extern int32_t gQ1920LatestGridY;
void Q1990EnsureNativeLodForCell(int32_t cellX, int32_t cellY,
                                 float centerX, float centerY, float floorZ);

void Q1970AdvanceNativeLodQ19(int32_t cellX, int32_t cellY,
                              float centerX, float centerY, float floorZ);
bool Q1970AnyLodWorkerQ19();
bool Q2013NativeLodBootstrapReadyQ19(
    int32_t cellX, int32_t cellY, size_t* outReady);
bool Q2013HasNativeObjectLodForCellQ19(int32_t cellX, int32_t cellY);
bool Q1990LodBlockDesired(int32_t blockX, int32_t blockY);
bool Q1990TerrainLodBlockDesired(int32_t blockX, int32_t blockY);
void Q2024GetAuthoredWarmupState(size_t& level32Ready,
                                 size_t& level32Target,
                                 size_t& highReady,
                                 size_t& highTarget);


constexpr const char* Q6H_TAG = "FalloutQuest";
constexpr float FO3_UNITS_PER_METRE = 70.0f;
constexpr float FLOOR_Y = -1.55f;
constexpr float SCENE_FORWARD = 0.00f;
constexpr size_t MAX_SCENE_OBJECTS = 1200u;
constexpr size_t MAX_MODEL_ATTEMPTS = 2000u;
constexpr float MAX_MODEL_EXTENT_UNITS = 20000.0f;
constexpr uint32_t Q2025_FLAG_VISIBLE_WHEN_DISTANT = 0x00008000u;
constexpr uint32_t Q2025_FLAG_HIGH_PRIORITY_LOD = 0x00010000u;

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

void Q2016MulMat4(const float a[16], const float b[16], float out[16]) {
    float r[16]{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k) {
                r[col * 4 + row] +=
                    a[k * 4 + row] * b[col * 4 + k];
            }
        }
    }
    std::copy(r, r + 16, out);
}

bool Q2016InvertAffine(const float m[16], float out[16]) {
    const float a00=m[0], a01=m[4], a02=m[8];
    const float a10=m[1], a11=m[5], a12=m[9];
    const float a20=m[2], a21=m[6], a22=m[10];
    const float c00 = a11*a22 - a12*a21;
    const float c01 = a02*a21 - a01*a22;
    const float c02 = a01*a12 - a02*a11;
    const float det = a00*c00 + a10*c01 + a20*c02;
    if (std::fabs(det) < 1.0e-8f) return false;
    const float invDet = 1.0f / det;

    // Column-major inverse of the 3x3 affine basis.
    out[0]  = c00 * invDet;
    out[4]  = c01 * invDet;
    out[8]  = c02 * invDet;
    out[1]  = (a12*a20 - a10*a22) * invDet;
    out[5]  = (a00*a22 - a02*a20) * invDet;
    out[9]  = (a02*a10 - a00*a12) * invDet;
    out[2]  = (a10*a21 - a11*a20) * invDet;
    out[6]  = (a01*a20 - a00*a21) * invDet;
    out[10] = (a00*a11 - a01*a10) * invDet;
    out[3]=out[7]=out[11]=0.0f;
    out[15]=1.0f;

    const float tx=m[12], ty=m[13], tz=m[14];
    out[12] = -(out[0]*tx + out[4]*ty + out[8]*tz);
    out[13] = -(out[1]*tx + out[5]*ty + out[9]*tz);
    out[14] = -(out[2]*tx + out[6]*ty + out[10]*tz);
    return true;
}

void Q2016BuildPlacementMatrix(
        const Fo3WorldPlacement& placement,
        float centerX, float centerY, float floorZ,
        float out[16]) {
    const Vec3 xAxis = GameDirectionToOpenXr(
        ApplyEsmRotation({1.0f, 0.0f, 0.0f}, placement));
    const Vec3 yAxis = GameDirectionToOpenXr(
        ApplyEsmRotation({0.0f, 1.0f, 0.0f}, placement));
    const Vec3 zAxis = GameDirectionToOpenXr(
        ApplyEsmRotation({0.0f, 0.0f, 1.0f}, placement));
    const float s = placement.scale;

    std::fill(out, out + 16, 0.0f);
    out[0]=xAxis.x*s; out[1]=xAxis.y*s; out[2]=xAxis.z*s;
    out[4]=yAxis.x*s; out[5]=yAxis.y*s; out[6]=yAxis.z*s;
    out[8]=zAxis.x*s; out[9]=zAxis.y*s; out[10]=zAxis.z*s;
    out[12]=(placement.x-centerX)/FO3_UNITS_PER_METRE;
    out[13]=FLOOR_Y+(placement.z-floorZ)/FO3_UNITS_PER_METRE;
    out[14]=SCENE_FORWARD-(placement.y-centerY)/FO3_UNITS_PER_METRE;
    out[15]=1.0f;
}

bool Q2016RelativeMatrix(
        const float representative[16],
        const float target[16],
        float out[16]) {
    float inverse[16]{};
    if (!Q2016InvertAffine(representative, inverse)) return false;
    Q2016MulMat4(target, inverse, out);
    return true;
}

struct CpuObject {
    Fo3WorldPlacement placement;
    Fo3StaticNifMesh mesh;
    std::vector<Vec3> positionsGame;
    std::vector<Vec3> normalsGame;
    std::vector<Vec3> tangentsGame;
    std::vector<Vec3> bitangentsGame;

    // Q19.6: final interleaved vertex stream is prepared by the CELL worker.
    // Non-Q19 callers lazily populate it in UploadCpuObject as before.
    std::vector<float> q1960ExpandedVertices;
    bool q1960ExpandedReady = false;
    float q1960MinX = 0.0f, q1960MaxX = 0.0f;
    float q1960MinY = 0.0f, q1960MaxY = 0.0f;
    float q1960MinZ = 0.0f, q1960MaxZ = 0.0f;
    uint32_t q2016ShapeIndex = 0u;
};

struct GpuObject {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint diffuse = 0;
    GLuint normal = 0;
    GLuint glow = 0;
    GLuint environmentCube = 0;
    GLuint environmentMask = 0;
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
    bool realEnvironmentCube = false;
    bool realEnvironmentMask = false;
    bool environmentEnabledQ2050 = false;
    float environmentMapScaleQ2050 = 1.0f;
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
    bool q2024TerrainLod = false;
    bool q2025LandscapeRock = false;
    bool q2025VisibleWhenDistant = false;
    bool q2025HighPriorityLod = false;
    bool q210PlayerBody = false;
    bool q220LooseObject = false;
    bool q230NpcActor = false;
    float q223PlacementScale = 1.0f;
    float q220DynamicTransform[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    // Q21.16: authored upperbody.nif can participate in the VR IK solve
    // without being visually drawn underneath equipped armour.
    bool q215IkReferenceOnly = false;
    std::string baseRecordType;
    Fo3DoorTeleport teleport;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
    uint32_t q2016ShapeIndex = 0u;
    uint64_t q2016BatchKey = 0u;
    float q2016PlacementMatrix[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    bool q2017SharedCandidate = false;
    bool q2017SharedGeometry = false;
    uint64_t q2017SharedMeshKey = 0u;
    float q2017RelativeMatrix[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    std::vector<Vec3> q1580NormalSamples;
};

struct CachedGpuTexture {
    GLuint id = 0;
    bool real = false;
};

// Q20.15: keep the 7x7 buffer resident, but reject AABBs outside the current
// eye frustum before any GL state changes. Bounds are already in scene metres.
// These runtime objects are defined later in this translation unit.
extern bool gWaterReflectionPassQ2090;
extern std::vector<GpuObject> gObjects;
bool gQ2015FrustumCullActive = false;
float gQ2015FrustumMvp[16]{};
uint64_t gQ2015CullTested = 0u;
uint64_t gQ2015CullRejected = 0u;
uint64_t gQ2015CullPassed = 0u;
uint64_t gQ2015CullScopes = 0u;

bool Q2015AabbVisible(const GpuObject& object) {
    // Q20.20: reflection passes now install their own reflected-camera frustum.
    // Do not bypass culling merely because we are drawing the planar mirror.
    if (!gQ2015FrustumCullActive) return true;
    if (object.minX > object.maxX || object.minY > object.maxY ||
        object.minZ > object.maxZ) return true;

    ++gQ2015CullTested;
    const float cx = 0.5f * (object.minX + object.maxX);
    const float cy = 0.5f * (object.minY + object.maxY);
    const float cz = 0.5f * (object.minZ + object.maxZ);
    const float ex = 0.5f * (object.maxX - object.minX);
    const float ey = 0.5f * (object.maxY - object.minY);
    const float ez = 0.5f * (object.maxZ - object.minZ);

    const float* m = gQ2015FrustumMvp;
    const float planes[6][4] = {
        {m[3] + m[0],  m[7] + m[4],  m[11] + m[8],  m[15] + m[12]},
        {m[3] - m[0],  m[7] - m[4],  m[11] - m[8],  m[15] - m[12]},
        {m[3] + m[1],  m[7] + m[5],  m[11] + m[9],  m[15] + m[13]},
        {m[3] - m[1],  m[7] - m[5],  m[11] - m[9],  m[15] - m[13]},
        {m[3] + m[2],  m[7] + m[6],  m[11] + m[10], m[15] + m[14]},
        {m[3] - m[2],  m[7] - m[6],  m[11] - m[10], m[15] - m[14]},
    };

    constexpr float Q2015_FRUSTUM_PADDING_METRES = 1.0f;
    for (const auto& p : planes) {
        const float len =
            std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
        if (len < 1e-6f) continue;
        const float inv = 1.0f / len;
        const float a = p[0] * inv;
        const float b = p[1] * inv;
        const float c = p[2] * inv;
        const float d = p[3] * inv;
        const float distance = a*cx + b*cy + c*cz + d;
        const float radius =
            std::fabs(a)*ex + std::fabs(b)*ey + std::fabs(c)*ez;
        if (distance + radius < -Q2015_FRUSTUM_PADDING_METRES) {
            ++gQ2015CullRejected;
            return false;
        }
    }
    ++gQ2015CullPassed;
    return true;
}

struct Q2015FrustumCullScope {
    bool previousActive = false;
    float previousMvp[16]{};

    explicit Q2015FrustumCullScope(const float* mvp) {
        previousActive = gQ2015FrustumCullActive;
        if (previousActive)
            std::copy(gQ2015FrustumMvp, gQ2015FrustumMvp + 16, previousMvp);
        std::copy(mvp, mvp + 16, gQ2015FrustumMvp);
        gQ2015FrustumCullActive = true;
    }
    ~Q2015FrustumCullScope() {
        if (previousActive) {
            std::copy(previousMvp, previousMvp + 16, gQ2015FrustumMvp);
            gQ2015FrustumCullActive = true;
        } else {
            gQ2015FrustumCullActive = false;
        }
        ++gQ2015CullScopes;
        if ((gQ2015CullScopes % 300u) == 0u) {
            const double pct = gQ2015CullTested > 0u
                ? 100.0 * static_cast<double>(gQ2015CullRejected) /
                  static_cast<double>(gQ2015CullTested)
                : 0.0;
            Q6H_LOGI("Q20.20 FRUSTUM: tested=%llu rejected=%llu passed=%llu rejectedPct=%.1f liveShapes=%zu scope=nested-main+reflection-aabb conservativePaddingM=1.0",
                     static_cast<unsigned long long>(gQ2015CullTested),
                     static_cast<unsigned long long>(gQ2015CullRejected),
                     static_cast<unsigned long long>(gQ2015CullPassed),
                     pct, gObjects.size());
            gQ2015CullTested = gQ2015CullRejected = gQ2015CullPassed = 0u;
        }
    }
};

GLuint gProgram = 0;
GLint gMvpLocation = -1;
GLint gInstancingEnabledLocationQ2016 = -1;
GLint gObjectTransformEnabledLocationQ2017 = -1;
GLint gObjectTransformLocationQ2017 = -1;
GLuint gInstanceBufferQ2016 = 0u;
bool gInstancedDrawActiveQ2016 = false;
size_t gInstanceMatrixBaseFloatQ2017 = 0u;
GLsizei gInstanceCountQ2017 = 0;
std::vector<float> gInstanceMatricesQ2016;
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
GLint gEnvironmentCubeLocationQ2050 = -1;
GLint gEnvironmentMaskLocationQ2050 = -1;
GLint gEnvironmentPassLocationQ2050 = -1;
GLint gEnvironmentScaleLocationQ2050 = -1;
GLint gEnvironmentCustomMaskLocationQ2050 = -1;
bool gEnvironmentPassEnabledQ205A = true;
size_t gEnvironmentCandidatesQ205A = 0u;
size_t gEnvironmentEnabledMaterialsQ205A = 0u;
size_t gEnvironmentCubeFailuresQ205A = 0u;
size_t gEnvironmentMaskFailuresQ205A = 0u;
uint64_t gEnvironmentHeartbeatFrameQ205A = 0u;
GLint gEmissiveColorLocationQ1020 = -1;
GLint gEmissiveMultLocationQ1020 = -1;
GLint gGlowEnabledLocationQ1020 = -1;
GLint gExternalEmittanceEnabledLocationQ1380 = -1;
GLint gExternalEmittanceColorLocationQ1380 = -1;
GLint gNativeLodClipEnabledLocationQ1810 = -1;
GLint gLodFadeModeLocationQ2021 = -1;
GLint gTerrainLodModeLocationQ2024 = -1;
GLint gLodFadePlayerLocationQ2021 = -1;
GLint gLodFadeRangeLocationQ2021 = -1;
GLint gNativeLodClipCellCountLocationQ1900 = -1;
GLint gNativeLodClipCellsLocationQ1900 = -1;
GLint gWaterReflectionClipEnabledLocationQ2090 = -1;
GLint gWaterReflectionPlaneYLocationQ2090 = -1;
bool gWaterReflectionPassQ2090 = false;
float gWaterReflectionPlaneYQ2090 = 0.0f;
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

// Q21.0: real Fallout actor geometry kept outside CELL ownership.
std::vector<GpuObject> gQ210PlayerBody;
std::vector<GpuObject> gQ230NpcActors;
bool gQ230NpcAttempted = false;
bool gQ230NpcReady = false;
bool gQ210PlayerBodyAttempted = false;
bool gQ210PlayerBodyReady = false;
uint64_t gQ210PlayerBodyFrames = 0u;
float gQ210PlayerRoot[16]{
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1
};
float gQ210Head[4]{0.0f, 0.0f, 0.0f, 0.0f};
float gQ210LeftHand[3]{0.0f, 0.0f, 0.0f};
float gQ210RightHand[3]{0.0f, 0.0f, 0.0f};
float gQ218LeftHandQuat[4]{0.0f, 0.0f, 0.0f, 1.0f};
float gQ218RightHandQuat[4]{0.0f, 0.0f, 0.0f, 1.0f};
bool gQ210LeftHandValid = false;
bool gQ210RightHandValid = false;

// Q21.17 controller-driven finger pose. These are VR input/calibration values;
// the actual finger grouping and curl axes are derived from Fallout's authored
// hand skeleton at runtime.
float gQ217FingerTrigger[2]{0.0f, 0.0f};
float gQ217FingerGrip[2]{0.0f, 0.0f};
bool gQ217TriggerTouched[2]{false, false};
bool gQ217ThumbTouched[2]{false, false};

// Q21.9 VR-specific retarget calibration. Both arms share one scale so the
// avatar stays symmetric. The scale only grows during a session, avoiding
// visible arm-length "breathing" during ordinary controller motion.
constexpr float Q219_ARM_BASE_SCALE = 1.12f;
constexpr float Q219_ARM_MAX_SCALE = 1.22f;
constexpr float Q219_TARGET_EXTENSION_RATIO = 0.93f;
constexpr float Q219_SCALE_GROW_PER_FRAME = 0.0025f;
float gQ219ArmLengthScale = Q219_ARM_BASE_SCALE;

// Q21.13 VR torso inference: preserve independent head look inside a neck
// dead-zone, then let the torso follow the excess yaw. Snap/locomotion yaw is
// applied immediately so artificial turning stays coherent.
constexpr float Q213_NECK_YAW_LIMIT = 0.6108652382f; // 35 degrees
constexpr float Q213_TORSO_FOLLOW_MAX_STEP = 0.0261799388f; // 1.5 deg/frame
// Q21.14 VR-specific controller-to-avatar calibration requested from in-headset
// testing: move each solved hand 2.5 cm farther away from the body centreline.
constexpr float Q214_HAND_OUTWARD_OFFSET = 0.025f;
bool gQ213TorsoYawReady = false;
float gQ213TorsoYaw = 0.0f;
float gQ213LastLocomotionYaw = 0.0f;

uint64_t gQ211TrackingSerial = 0u;
uint64_t gQ211LastSkinnedSerial = ~0ull;

struct Q211PlayerRigPart {
    size_t gpuIndex = 0u;
    std::string sourceModelPath;
    std::vector<float> bindExpanded;
    std::vector<float> workExpanded;
    std::vector<uint16_t> expandedBoneIndices; // 4 per expanded vertex
    std::vector<float> expandedBoneWeights;     // 4 per expanded vertex
    std::vector<Fo3NifSkinBone> bones;
    int leftUpperArm = -1;
    int leftForearm = -1;
    int leftHand = -1;
    int rightUpperArm = -1;
    int rightForearm = -1;
    int rightHand = -1;
    Vec3 leftPalmAnchor{};
    Vec3 rightPalmAnchor{};
    Vec3 leftForearmCentroid{};
    Vec3 rightForearmCentroid{};
    bool leftPalmAnchorValid = false;
    bool rightPalmAnchorValid = false;
    bool leftForearmCentroidValid = false;
    bool rightForearmCentroidValid = false;
    bool leftChainReady = false;
    bool rightChainReady = false;
};

std::vector<Q211PlayerRigPart> gQ211PlayerRigParts;

std::unordered_map<std::string, CachedGpuTexture> gTextureCache;
std::unordered_map<std::string, CachedGpuTexture> gCubeTextureCacheQ2050;
std::unordered_map<std::string, Fo3RgbaTexture> gQ234GeneratedTextures;

struct Q2017SharedGeometryEntry {
    GLuint vao = 0u;
    GLuint vbo = 0u;
    GLsizei vertexCount = 0;
    std::string modelPath;
    uint32_t shapeIndex = 0u;
    float ownerPlacementMatrix[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    size_t refs = 0u;
};
std::unordered_map<uint64_t, Q2017SharedGeometryEntry> gQ2017SharedGeometry;
uint64_t gQ2017SharedHits = 0u;
uint64_t gQ2017SharedMisses = 0u;
uint64_t gQ2017SharedAvoidedBytes = 0u;

uint64_t Q2017SharedMeshKey(const GpuObject& gpu) {
    uint64_t hash =
        static_cast<uint64_t>(std::hash<std::string>{}(gpu.modelPath));
    auto mix = [&](uint64_t value) {
        hash ^= value + 0x9e3779b97f4a7c15ULL +
                (hash << 6u) + (hash >> 2u);
    };
    mix(gpu.q2016ShapeIndex);
    mix(static_cast<uint64_t>(gpu.vertexCount));
    return hash == 0u ? 1u : hash;
}

bool Q2017CanShareGeometry(const GpuObject& gpu) {
    const bool authoredStatic =
        gpu.baseRecordType == "STAT" ||
        gpu.baseRecordType == "SCOL" ||
        gpu.baseRecordType == "TREE";
    return gExteriorStreamingActiveQ1890 &&
           authoredStatic &&
           !gpu.q1990NativeLod &&
           !gpu.alphaBlend &&
           !gpu.decalQ1170 &&
           !gpu.externalEmittanceFlagQ1380 &&
           gpu.vertexCount > 0 &&
           !gpu.modelPath.empty();
}

bool Q2017TryReuseSharedGeometry(GpuObject& gpu) {
    if (!gpu.q2017SharedCandidate || gpu.q2017SharedMeshKey == 0u)
        return false;
    const auto found = gQ2017SharedGeometry.find(gpu.q2017SharedMeshKey);
    if (found == gQ2017SharedGeometry.end()) return false;
    Q2017SharedGeometryEntry& shared = found->second;
    if (shared.modelPath != gpu.modelPath ||
        shared.shapeIndex != gpu.q2016ShapeIndex ||
        shared.vertexCount != gpu.vertexCount ||
        shared.vao == 0u || shared.vbo == 0u) {
        return false;
    }

    float relative[16]{};
    if (!Q2016RelativeMatrix(
            shared.ownerPlacementMatrix,
            gpu.q2016PlacementMatrix,
            relative)) {
        return false;
    }
    std::copy(relative, relative + 16, gpu.q2017RelativeMatrix);
    gpu.vao = shared.vao;
    gpu.vbo = shared.vbo;
    gpu.q2017SharedGeometry = true;
    ++shared.refs;
    ++gQ2017SharedHits;
    gQ2017SharedAvoidedBytes +=
        static_cast<uint64_t>(gpu.vertexCount) * 18u * sizeof(float);
    return true;
}

void Q2017PublishSharedGeometry(GpuObject& gpu) {
    if (!gpu.q2017SharedCandidate ||
        gpu.q2017SharedGeometry ||
        gpu.q2017SharedMeshKey == 0u ||
        gpu.vao == 0u || gpu.vbo == 0u ||
        gpu.vertexCount <= 0) {
        return;
    }

    const auto existing =
        gQ2017SharedGeometry.find(gpu.q2017SharedMeshKey);
    if (existing != gQ2017SharedGeometry.end()) {
        Q2017SharedGeometryEntry& shared = existing->second;
        if (shared.modelPath == gpu.modelPath &&
            shared.shapeIndex == gpu.q2016ShapeIndex &&
            shared.vertexCount == gpu.vertexCount) {
            float relative[16]{};
            if (Q2016RelativeMatrix(
                    shared.ownerPlacementMatrix,
                    gpu.q2016PlacementMatrix,
                    relative)) {
                glDeleteBuffers(1, &gpu.vbo);
                glDeleteVertexArrays(1, &gpu.vao);
                gpu.vbo = shared.vbo;
                gpu.vao = shared.vao;
                std::copy(relative, relative + 16,
                          gpu.q2017RelativeMatrix);
                gpu.q2017SharedGeometry = true;
                ++shared.refs;
                ++gQ2017SharedHits;
                return;
            }
        }
        return;
    }

    Q2017SharedGeometryEntry shared;
    shared.vao = gpu.vao;
    shared.vbo = gpu.vbo;
    shared.vertexCount = gpu.vertexCount;
    shared.modelPath = gpu.modelPath;
    shared.shapeIndex = gpu.q2016ShapeIndex;
    std::copy(gpu.q2016PlacementMatrix,
              gpu.q2016PlacementMatrix + 16,
              shared.ownerPlacementMatrix);
    shared.refs = 1u;
    gQ2017SharedGeometry.emplace(gpu.q2017SharedMeshKey, std::move(shared));
    gpu.q2017SharedGeometry = true;
    std::fill(gpu.q2017RelativeMatrix,
              gpu.q2017RelativeMatrix + 16, 0.0f);
    gpu.q2017RelativeMatrix[0] = gpu.q2017RelativeMatrix[5] =
        gpu.q2017RelativeMatrix[10] = gpu.q2017RelativeMatrix[15] = 1.0f;
    ++gQ2017SharedMisses;
}

bool Q2017ReleaseSharedGeometry(GpuObject& gpu) {
    if (!gpu.q2017SharedGeometry || gpu.q2017SharedMeshKey == 0u)
        return false;
    const auto found = gQ2017SharedGeometry.find(gpu.q2017SharedMeshKey);
    if (found != gQ2017SharedGeometry.end()) {
        Q2017SharedGeometryEntry& shared = found->second;
        if (shared.refs > 0u) --shared.refs;
        if (shared.refs == 0u) {
            if (shared.vbo) glDeleteBuffers(1, &shared.vbo);
            if (shared.vao) glDeleteVertexArrays(1, &shared.vao);
            gQ2017SharedGeometry.erase(found);
        }
    }
    gpu.vbo = 0u;
    gpu.vao = 0u;
    gpu.q2017SharedGeometry = false;
    gpu.q2017SharedCandidate = false;
    gpu.q2017SharedMeshKey = 0u;
    return true;
}

void Q2017ForceClearSharedGeometry() {
    for (auto& entry : gQ2017SharedGeometry) {
        if (entry.second.vbo) glDeleteBuffers(1, &entry.second.vbo);
        if (entry.second.vao) glDeleteVertexArrays(1, &entry.second.vao);
    }
    gQ2017SharedGeometry.clear();
}

// Q19: decoded DDS data is prepared on the serialized asset worker and
// consumed later by the render thread. OpenGL handles remain render-thread only.
struct Q1900PreparedTextureQ19 {
    Fo3RgbaTexture texture;
    bool real = false;
};
std::mutex gQ1900TextureCpuMutexQ19;
std::mutex gQ2012TextureDecodeMutexQ19;
std::unordered_map<std::string, Q1900PreparedTextureQ19>
    gQ1900PreparedTexturesQ19;
std::unordered_set<std::string> gQ1900KnownGpuTextureKeysQ19;

bool gQ1960CaptureVboQ19 = false;
std::vector<float>* gQ1960CapturedVboVerticesQ19 = nullptr;
GLuint gDepthRenderbuffer = 0;
GLsizei gDepthWidth = 0;
GLsizei gDepthHeight = 0;
bool gSceneReady = false;

// Q20.9 PC WATER000 planar ReflectionMap. FalloutPrefs requests a 1024x1024
// reflection target. It is regenerated per eye because a single monoscopic
// reflection is incorrect in stereo VR.
constexpr GLsizei Q2090_REFLECTION_SIZE = 1024;
GLuint gWaterReflectionFboQ2090 = 0u;
GLuint gWaterReflectionColorQ2090 = 0u;
GLuint gWaterReflectionDepthQ2090 = 0u;
bool gWaterReflectionTargetReadyQ2090 = false;
bool gWaterReflectionTargetLoggedQ2090 = false;
float gWaterSkyMvpQ2090[16]{};
bool gWaterSkyMvpReadyQ2090 = false;
uint64_t gWaterReflectionFramesQ2090 = 0u;

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

bool Q1900GpuTextureKnownQ19(const std::string& path) {
    if (path.empty()) return true;
    const std::string key = TextureCacheKey(path, "");
    std::lock_guard<std::mutex> lock(gQ1900TextureCpuMutexQ19);
    return gQ1900KnownGpuTextureKeysQ19.find(key) !=
               gQ1900KnownGpuTextureKeysQ19.end() ||
           gQ1900PreparedTexturesQ19.find(key) !=
               gQ1900PreparedTexturesQ19.end();
}

void Q1900PrepareTextureCpuQ19(const std::string& path) {
    if (path.empty()) return;
    const std::string key = TextureCacheKey(path, "");
    {
        std::lock_guard<std::mutex> lock(gQ1900TextureCpuMutexQ19);
        if (gQ1900KnownGpuTextureKeysQ19.find(key) !=
                gQ1900KnownGpuTextureKeysQ19.end() ||
            gQ1900PreparedTexturesQ19.find(key) !=
                gQ1900PreparedTexturesQ19.end()) {
            return;
        }
    }

    Fo3RgbaTexture decoded;
    bool real = false;
    {
        // Multiple detailed CELL workers may prepare textures concurrently in
        // Q20.12. Keep the lazy texture-BSA/index decoder single-entry while
        // still allowing NIF transforms to run in parallel.
        std::lock_guard<std::mutex> decodeLock(gQ2012TextureDecodeMutexQ19);
        real = LoadFalloutTextureRgba(path, decoded);
    }
    std::lock_guard<std::mutex> lock(gQ1900TextureCpuMutexQ19);
    if (gQ1900KnownGpuTextureKeysQ19.find(key) ==
            gQ1900KnownGpuTextureKeysQ19.end() &&
        gQ1900PreparedTexturesQ19.find(key) ==
            gQ1900PreparedTexturesQ19.end()) {
        Q1900PreparedTextureQ19 prepared;
        prepared.texture = std::move(decoded);
        prepared.real = real;
        gQ1900PreparedTexturesQ19.emplace(key, std::move(prepared));
    }
}

bool Q1900TakePreparedTextureQ19(const std::string& key,
                                 Fo3RgbaTexture& texture,
                                 bool& real) {
    std::lock_guard<std::mutex> lock(gQ1900TextureCpuMutexQ19);
    auto found = gQ1900PreparedTexturesQ19.find(key);
    if (found == gQ1900PreparedTexturesQ19.end()) return false;
    texture = std::move(found->second.texture);
    real = found->second.real;
    gQ1900PreparedTexturesQ19.erase(found);
    return true;
}

void Q1900MarkGpuTextureKnownQ19(const std::string& key) {
    if (key.empty() || key.rfind("<fallback>:", 0) == 0) return;
    std::lock_guard<std::mutex> lock(gQ1900TextureCpuMutexQ19);
    gQ1900KnownGpuTextureKeysQ19.insert(key);
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
        layout(location = 6) in vec4 aInstance0Q2016;
        layout(location = 7) in vec4 aInstance1Q2016;
        layout(location = 8) in vec4 aInstance2Q2016;
        layout(location = 9) in vec4 aInstance3Q2016;
        uniform mat4 uMvp;
        uniform float uInstancingEnabledQ2016;
        uniform float uObjectTransformEnabledQ2017;
        uniform mat4 uObjectTransformQ2017;
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
            mat4 q2016Instance = mat4(
                aInstance0Q2016, aInstance1Q2016,
                aInstance2Q2016, aInstance3Q2016);
            mat4 q2017Transform = mat4(1.0);
            if (uInstancingEnabledQ2016 > 0.5) {
                q2017Transform = q2016Instance;
            } else if (uObjectTransformEnabledQ2017 > 0.5) {
                q2017Transform = uObjectTransformQ2017;
            }
            vec4 q2016WorldPosition =
                q2017Transform * vec4(aPosition, 1.0);
            mat3 q2016Basis = mat3(q2017Transform);
            vec3 q2016Normal = normalize(q2016Basis * aNormal);
            vec3 q2016Tangent = normalize(q2016Basis * aTangent);
            vec3 q2016Bitangent = normalize(q2016Basis * aBitangent);
            vNormal = q2016Normal;
            vTangent = q2016Tangent;
            vBitangent = q2016Bitangent;
            vUv = aUv;
            // SLS1011.vso: LightData is dotted with the authored T/B/N rows in
            // the vertex stage, then encoded for interpolation to the pixel stage.
            vec3 q1540LightData = normalize(uSunDirectionVertexQ1540);
            vec3 q1540LightTangent = vec3(
                dot(q2016Tangent, q1540LightData),
                dot(q2016Bitangent, q1540LightData),
                dot(q2016Normal, q1540LightData));
            // PC SLS vertex shader: dp3 T/B/N then rsq-normalize before oT1.
            float q1610LightLen2 = dot(q1540LightTangent, q1540LightTangent);
            vSp17LightEncodedQ1540 = q1540LightTangent *
                inversesqrt(max(q1610LightLen2, 1.0e-12));

            // PC SLS: normalize(EyePosition - vertex), add LightData, normalize,
            // project the half vector into T/B/N, then normalize before oT3.
            vec3 q1630ViewWorld =
                normalize(uEyePositionVertexQ1630 - q2016WorldPosition.xyz);
            vec3 q1630HalfWorld = normalize(q1630ViewWorld + q1540LightData);
            vec3 q1630HalfTangent = vec3(
                dot(q2016Tangent, q1630HalfWorld),
                dot(q2016Bitangent, q1630HalfWorld),
                dot(q2016Normal, q1630HalfWorld));
            float q1630HalfLen2 = dot(q1630HalfTangent, q1630HalfTangent);
            vSp17HalfQ1630 = q1630HalfTangent *
                inversesqrt(max(q1630HalfLen2, 1.0e-12));
            vPosition = q2016WorldPosition.xyz;
            vColor = aColor;
            vShadowCoord = uLightMvp * q2016WorldPosition;
            vec4 q1532Clip = uMvp * q2016WorldPosition;
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
        uniform samplerCube uEnvironmentCubeQ2050;
        uniform sampler2D uEnvironmentMaskQ2050;
        uniform float uEnvironmentPassQ2050;
        uniform float uEnvironmentScaleQ2050;
        uniform float uEnvironmentCustomMaskQ2050;
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
        uniform int uNativeLodClipCellCountQ1900;
        uniform vec4 uNativeLodClipCellsQ1900[25];
        uniform float uLodFadeModeQ2021;
        uniform float uTerrainLodModeQ2024;
        uniform vec2 uLodFadePlayerQ2021;
        uniform vec2 uLodFadeRangeQ2021;
        uniform float uWaterReflectionClipEnabledQ2090;
        uniform float uWaterReflectionPlaneYQ2090;
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
            if (uWaterReflectionClipEnabledQ2090 > 0.5 &&
                vPosition.y < uWaterReflectionPlaneYQ2090) {
                discard;
            }
            // Q19: clip authored Level4 only where the corresponding detailed
            // exterior CELL is actually resident. A slow/missing CELL therefore
            // keeps its LOD instead of turning into a hole at the boundary.
            if (uNativeLodClipEnabledQ1810 > 0.5) {
                for (int q1900I = 0; q1900I < 25; ++q1900I) {
                    if (q1900I >= uNativeLodClipCellCountQ1900) break;
                    vec4 q1900Bounds = uNativeLodClipCellsQ1900[q1900I];
                    if (vPosition.x >= q1900Bounds.x &&
                        vPosition.x <= q1900Bounds.y &&
                        vPosition.z >= q1900Bounds.z &&
                        vPosition.z <= q1900Bounds.w) {
                        discard;
                    }
                }
            }
            float q2021FadeAlpha = 1.0;
            if (uLodFadeModeQ2021 > 0.5) {
                float q2021Distance =
                    length(vPosition.xz - uLodFadePlayerQ2021);
                float q2021Span = max(
                    uLodFadeRangeQ2021.y -
                    uLodFadeRangeQ2021.x, 0.001);
                float q2021T = clamp(
                    (q2021Distance - uLodFadeRangeQ2021.x) /
                    q2021Span, 0.0, 1.0);
                float q2021DetailWeight =
                    1.0 - smoothstep(0.0, 1.0, q2021T);
                q2021FadeAlpha =
                    uLodFadeModeQ2021 < 1.5
                        ? q2021DetailWeight
                        : (1.0 - q2021DetailWeight);
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
            alpha *= q2021FadeAlpha;

            vec4 normalGloss = texture(uNormalGloss, vUv);
            vec3 tangentNormal = normalGloss.rgb * 2.0 - 1.0;
            tangentNormal.xy *= uNormalStrength;
            tangentNormal = normalize(tangentNormal);
            // Q11.3: no tangent-space Y inversion; UVs are no longer double-flipped.

            vec3 N = normalize(vNormal);
            vec3 T = normalize(vTangent - N * dot(N, vTangent));
            vec3 B = normalize(vBitangent - N * dot(N, vBitangent));
            vec3 mappedNormal = normalize(mat3(T, B, N) * tangentNormal);

            // Q20.5 SP17 environment pass. The supplied Megaton NIFs bind
            // slot 4 as EnvironmentCubeMap and slot 5 as CustomEnvMask.
            // PC SLS2057 computes 2*dot(N,V)*N - V, samples the cube, then
            // multiplies by the custom mask, Toggles.z (NIF env scale),
            // optional vertex colour, and vertex fog visibility.
            if (uEnvironmentPassQ2050 > 0.5) {
                // SP17 SLS2050.vso scales the authored tangent and bitangent
                // by exactly 0.1 before ObjToCubeSpace, while leaving the normal
                // at full strength. Reconstruct the same reflection normal here:
                // normal-map X/Y perturbation is therefore one tenth of Z.
                vec3 q2050ReflectionNormal = normalize(
                    T * (tangentNormal.x * 0.1) +
                    B * (tangentNormal.y * 0.1) +
                    N * tangentNormal.z);
                vec3 q2050SurfaceToEye = normalize(uEyePosition - vPosition);
                vec3 q2050ReflectionScene =
                    reflect(-q2050SurfaceToEye, q2050ReflectionNormal);

                // Gamebryo -> OpenXR is (x,z,-y); invert that bridge for the
                // authored Fallout cubemap coordinate domain.
                vec3 q2050ReflectionGame = vec3(
                    q2050ReflectionScene.x,
                    -q2050ReflectionScene.z,
                    q2050ReflectionScene.y);

                vec3 q2050Cube =
                    texture(uEnvironmentCubeQ2050, q2050ReflectionGame).rgb;
                float q2050Mask = uEnvironmentCustomMaskQ2050 > 0.5
                    ? texture(uEnvironmentMaskQ2050, vUv).r
                    : normalGloss.a;
                q2050Mask *= uEnvironmentScaleQ2050;

                vec3 q2050Env = q2050Cube * q2050Mask;
                q2050Env *= mix(vec3(1.0), vColor.rgb, uUseVertexColor);
                q2050Env *= q2021FadeAlpha;
                float q2050FogVisibility = uRenderStageQ1560 >= 1
                    ? (1.0 - vFogFactorQ1532)
                    : 1.0;
                q2050Env *= q2050FogVisibility;

                // PC c1.w is an additional per-object environment fade. Its
                // source has not yet been recovered, so Q20.5 leaves that
                // scalar neutral rather than inventing a distance formula.
                fragColor = vec4(max(q2050Env, vec3(0.0)), 1.0);
                return;
            }

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
                                  * uSpecularEnabled
                                  * (1.0 - uTerrainLodModeQ2024);
            float q1050SunVisibility = Q1050ShadowVisibility(vShadowCoord, mappedNormal, lightDirection);
            vec3 q1630Sp17Lighting = max(
                uAmbientColor + uSunlightColor * lambert, vec3(0.0));

            // Q20.24: landscape LOD is not a static-object PPLighting material.
            // Match the existing detailed LAND lighting until Fallout3.exe's
            // dedicated landscape-LOD shader is recovered.
            if (uTerrainLodModeQ2024 > 0.5) {
                vec3 q2024LandLight =
                    normalize(vec3(0.35, 0.85, 0.40));
                float q2024LandLambert =
                    max(dot(N, q2024LandLight), 0.0);
                q1630Sp17Lighting =
                    vec3(0.48 + 0.52 * q2024LandLambert);
            }

            vec3 q1470WorldDiffuse = baseColor * q1630Sp17Lighting;
            if (uLegacyColourDomainQ1570 > 0.5 &&
                uNoLighting <= 0.5 &&
                uTerrainLodModeQ2024 <= 0.5) {
                vec3 q1570BaseEncoded = Q1470LinearToSrgb(baseColor);
                vec3 q1570EncodedDiffuse = q1570BaseEncoded *
                    (uLegacyAmbientQ1570 + uLegacySunlightQ1570 * lambert);
                q1470WorldDiffuse = Q1470SrgbToLinear(q1570EncodedDiffuse);
            }
            vec3 lit = uNoLighting > 0.5
                ? baseColor
                : q1470WorldDiffuse + q1630SpecularRgb;
            for (int i = 0; i < 8; ++i) {
                if (uNoLighting > 0.5 ||
                    uTerrainLodModeQ2024 > 0.5 ||
                    i >= uLocalLightCount) break;
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
            // Q20.3c PC static parity: SLS1011.vso computes FogParam in
            // the vertex shader and SLS1017.pso consumes the interpolated D1.w.
            // Do not recompute a separate world-distance fog in the pixel stage.
            float fogFactor = uRenderStageQ1560 >= 1
                ? vFogFactorQ1532
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
        Q1900MarkGpuTextureKnownQ19(cacheKey);
        if (!gExteriorStreamingActiveQ1890) Q6H_LOGI("Q6H GPU %s CACHE HIT: ref=%08X key=%s real=%d",
                 label, refFormId, cacheKey.c_str(), real ? 1 : 0);
        return true;
    }

    Fo3RgbaTexture texture;
    bool q234Generated = false;
    const auto q234It = gQ234GeneratedTextures.find(path);
    if (q234It != gQ234GeneratedTextures.end()) {
        texture = q234It->second;
        real = true;
        q234Generated = true;
    }

    bool q1900PreparedQ19 = false;
    if (!q234Generated && !path.empty()) {
        q1900PreparedQ19 =
            Q1900TakePreparedTextureQ19(TextureCacheKey(path, ""), texture, real);
    }
    if (!q234Generated && !q1900PreparedQ19) {
        real = !path.empty() && LoadFalloutTextureRgba(path, texture);
    }
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
    // Q20.3c PC static parity: the complete D3D9 trace contains no
    // D3DSAMP_SRGBTEXTURE state change. D3D9 therefore leaves sampler sRGB
    // decode at its default disabled state. Preserve decoded DDS bytes as raw
    // normalized texture values for BaseMap as well as data textures.
    const GLenum q1620InternalFormat = GL_RGBA8;
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
    Q1900MarkGpuTextureKnownQ19(cacheKey);
    if (!gExteriorStreamingActiveQ1890) Q6H_LOGI("Q6H GPU %s CACHE MISS: ref=%08X source=%s %dx%d format=%s uniqueTextures=%zu",
             label, refFormId, real ? texture.sourcePath.c_str() : "<fallback>",
             texture.width, texture.height, texture.format.c_str(), gTextureCache.size());
    return true;
}


bool UploadCubeTextureQ2050(const std::string& path,
                            GLuint& textureId, bool& real,
                            const char* label, uint32_t refFormId) {
    textureId = 0u;
    real = false;
    if (path.empty()) return true;

    const std::string cacheKey = std::string("cube:") + TextureCacheKey(path, label);
    const auto cached = gCubeTextureCacheQ2050.find(cacheKey);
    if (cached != gCubeTextureCacheQ2050.end()) {
        textureId = cached->second.id;
        real = cached->second.real;
        return true;
    }

    Fo3RgbaCubeTexture cube;
    real = LoadFalloutCubeTextureRgba(path, cube);
    if (!real || cube.width <= 0 || cube.height <= 0 ||
        cube.mipLevels <= 0) {
        ++gEnvironmentCubeFailuresQ205A;
        Q6H_LOGW("Q20.5A CUBE UPLOAD SKIP: ref=%08X path=%s reason=decode-failed failures=%zu",
                 refFormId, path.c_str(), gEnvironmentCubeFailuresQ205A);
        return true;
    }

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                    cube.mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

    for (int face = 0; face < 6; ++face) {
        int w = cube.width;
        int h = cube.height;
        if (static_cast<int>(cube.rgbaLevels[face].size()) != cube.mipLevels) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glDeleteTextures(1, &textureId);
            textureId = 0u;
            real = false;
            return true;
        }
        for (int mip = 0; mip < cube.mipLevels; ++mip) {
            const std::vector<uint8_t>& rgba = cube.rgbaLevels[face][mip];
            const size_t expected =
                static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
            if (rgba.size() != expected) {
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                glDeleteTextures(1, &textureId);
                textureId = 0u;
                real = false;
                return true;
            }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mip, GL_RGBA8,
                         w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            w = std::max(1, w >> 1);
            h = std::max(1, h >> 1);
        }
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &textureId);
        textureId = 0u;
        real = false;
        return true;
    }

    gCubeTextureCacheQ2050.emplace(cacheKey,
                                   CachedGpuTexture{textureId, true});
    Q6H_LOGI("Q20.5 CUBE GPU READY: ref=%08X path=%s size=%dx%d mips=%d format=%s cache=%zu faces=+X,-X,+Y,-Y,+Z,-Z",
             refFormId, path.c_str(), cube.width, cube.height, cube.mipLevels,
             cube.format.c_str(), gCubeTextureCacheQ2050.size());
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

    // Q20.12: detailed CELLs can build in parallel. Keep parsed NIFs immutable
    // behind shared_ptrs so cache lookup/insertion is thread-safe without
    // serializing each placement's transform/expanded-vertex work.
    using Q2012CachedMeshes =
        std::shared_ptr<const std::vector<Fo3StaticNifMesh>>;
    static std::mutex modelCacheMutex;
    static std::unordered_map<std::string, Q2012CachedMeshes> modelCache;

    Q2012CachedMeshes cachedMeshes;
    {
        std::lock_guard<std::mutex> lock(modelCacheMutex);
        const auto cached = modelCache.find(placement.modelPath);
        if (cached != modelCache.end()) cachedMeshes = cached->second;
    }

    if (!cachedMeshes) {
        std::vector<Fo3StaticNifMesh> meshes;
        if (!LoadFo3StaticNifMeshes(placement.modelPath, meshes)) return false;
        auto candidate = std::make_shared<const std::vector<Fo3StaticNifMesh>>(
            std::move(meshes));
        bool inserted = false;
        size_t uniqueModels = 0u;
        {
            std::lock_guard<std::mutex> lock(modelCacheMutex);
            const auto result =
                modelCache.emplace(placement.modelPath, candidate);
            cachedMeshes = result.first->second;
            inserted = result.second;
            uniqueModels = modelCache.size();
        }
        if (inserted) {
            Q6H_LOGI("Q20.12 MODEL CACHE MISS: model=%s shapes=%zu uniqueModels=%zu parallelSafe=1",
                     placement.modelPath.c_str(), cachedMeshes->size(),
                     uniqueModels);
        }
    }

    size_t shapeIndex = 0;
    for (const Fo3StaticNifMesh& cachedMesh : *cachedMeshes) {
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
        out.q2016ShapeIndex = static_cast<uint32_t>(shapeIndex);
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

bool ResolveDoorTeleportCachedQ1698(uint32_t refFormId,
                                      bool wastelandExterior,
                                      Fo3DoorTeleport& out) {
    static std::unordered_map<uint32_t, Fo3DoorTeleport> cache;
    const auto cached = cache.find(refFormId);
    if (cached != cache.end()) {
        out = cached->second;
        return out.valid;
    }

    Fo3DoorTeleport resolved;
    bool q1920IndexReady = false;
    bool q1920IndexedLookup = false;
    if (wastelandExterior) {
        q1920IndexedLookup =
            LookupFo3WastelandDoorTeleportQ1920(
                refFormId, &resolved, &q1920IndexReady);
    }

    if (wastelandExterior && q1920IndexReady) {
        if (q1920IndexedLookup && resolved.valid) {
            Q6H_LOGI("Q19.2 DOOR XTEL INDEX HIT: sourceDoor=%08X destinationDoor=%08X destinationCell=%08X renderThreadEsmScan=0",
                     refFormId, resolved.destinationDoorRefFormId,
                     resolved.destinationCellFormId);
        } else {
            Q6H_LOGI("Q19.2 DOOR XTEL INDEX ABSENT: sourceDoor=%08X renderThreadEsmScan=0",
                     refFormId);
        }
    } else {
        // Interior and non-Wasteland worlds retain the proven generic resolver.
        // If the Wasteland index failed unexpectedly, correctness wins over the
        // optimisation and this path remains a safe fallback.
        ResolveFo3DoorTeleportQ1700(refFormId, &resolved);
        if (wastelandExterior) {
            Q6H_LOGW("Q19.2 DOOR XTEL INDEX FALLBACK: sourceDoor=%08X reason=index-unavailable",
                     refFormId);
        }
    }

    cache.emplace(refFormId, resolved);
    out = resolved;
    return out.valid;
}

bool PrepareExpandedVertexStreamQ1960(
        CpuObject& cpu, float centerX, float centerY, float floorZ) {
    constexpr size_t FLOATS_PER_VERTEX = 18u;
    if (cpu.q1960ExpandedReady && !cpu.q1960ExpandedVertices.empty())
        return true;

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

    cpu.q1960ExpandedVertices = std::move(expanded);
    cpu.q1960ExpandedReady = true;
    cpu.q1960MinX = objectMinimum.x; cpu.q1960MaxX = objectMaximum.x;
    cpu.q1960MinY = objectMinimum.y; cpu.q1960MaxY = objectMaximum.y;
    cpu.q1960MinZ = objectMinimum.z; cpu.q1960MaxZ = objectMaximum.z;
    return true;
}

bool UploadCpuObject(CpuObject& cpu, float centerX, float centerY, float floorZ,
                     GpuObject& gpu) {
    constexpr size_t FLOATS_PER_VERTEX = 18u;
    const size_t vertexCount = cpu.positionsGame.size();
    if (!PrepareExpandedVertexStreamQ1960(cpu, centerX, centerY, floorZ))
        return false;

    std::vector<float> expanded = std::move(cpu.q1960ExpandedVertices);
    cpu.q1960ExpandedReady = false;
    const size_t q1960ExpandedFloatCount = expanded.size();
    if (q1960ExpandedFloatCount == 0u) return false;

    Vec3 objectMinimum{cpu.q1960MinX, cpu.q1960MinY, cpu.q1960MinZ};
    Vec3 objectMaximum{cpu.q1960MaxX, cpu.q1960MaxY, cpu.q1960MaxZ};

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

    // Q20.4b Fallout3.exe PPLighting rule:
    // Toggles.x is driven by the geometry's vertex-colour stream presence,
    // not BSShaderPPLightingProperty shaderFlags2 bit 0x20.  This replaces
    // Q20.4a's Church-only trace override with the recovered engine rule.
    gpu.useVertexColor = q1120HasVertexColorStream;

    const bool q204bOldLitVertexColorGate =
        !cpu.mesh.noLighting &&
        (cpu.mesh.shaderFlags2 & 0x00000020u) != 0u;
    if (q1120HasVertexColorStream && !cpu.mesh.noLighting &&
        !q204bOldLitVertexColorGate) {
        // Diagnostic only: these are shapes Q20.4a and earlier incorrectly
        // forced to white even though Fallout3.exe enables Toggles.x.
        static uint32_t q204bRecoveredLitVcolorShapes = 0u;
        ++q204bRecoveredLitVcolorShapes;
        if (q204bRecoveredLitVcolorShapes <= 24u) {
            Q6H_LOGI(
                "Q20.4B EXE VCOLOR: ref=%08X model=%s stream=1 shaderFlags2=%08X oldGate=0 exeRule=1 recoveredIndex=%u",
                cpu.placement.refFormId, cpu.placement.modelPath.c_str(),
                cpu.mesh.shaderFlags2, q204bRecoveredLitVcolorShapes);
        } else if (q204bRecoveredLitVcolorShapes == 25u) {
            Q6H_LOGI("Q20.4B EXE VCOLOR: additional recovered lit shapes suppressed from log");
        }
    }

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
    gpu.specularEnabled = !gpu.noLighting && (cpu.mesh.shaderFlags1 & 0x00000001u) != 0u;
    for (int i = 0; i < 3; ++i) { gpu.specularColor[i] = cpu.mesh.specularColor[i]; gpu.emissiveColor[i] = cpu.mesh.emissiveColor[i]; }
    gpu.emissiveMult = std::max(0.0f, cpu.mesh.emissiveMult);
    gpu.environmentMapScaleQ2050 = std::max(0.0f, cpu.mesh.environmentMapScale);
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
    const std::string q2025ModelLower =
        TextureCacheKey(cpu.placement.modelPath, "");
    gpu.q2025LandscapeRock =
        q2025ModelLower.find("landscape\\rocks\\") != std::string::npos;
    const uint32_t q2025LodFlags =
        cpu.placement.referenceRecordFlags |
        cpu.placement.baseRecordFlags;
    gpu.q2025VisibleWhenDistant =
        (q2025LodFlags & Q2025_FLAG_VISIBLE_WHEN_DISTANT) != 0u;
    gpu.q2025HighPriorityLod =
        (cpu.placement.referenceRecordFlags &
         Q2025_FLAG_HIGH_PRIORITY_LOD) != 0u;
    gpu.q2016ShapeIndex = cpu.q2016ShapeIndex;
    Q2016BuildPlacementMatrix(
        cpu.placement, centerX, centerY, floorZ,
        gpu.q2016PlacementMatrix);
    gpu.q1970GridX = cpu.placement.hasExteriorGrid
        ? cpu.placement.gridX
        : static_cast<int32_t>(std::floor(cpu.placement.x / Q1890_EXTERIOR_CELL_SIZE));
    gpu.q1970GridY = cpu.placement.hasExteriorGrid
        ? cpu.placement.gridY
        : static_cast<int32_t>(std::floor(cpu.placement.y / Q1890_EXTERIOR_CELL_SIZE));
    gpu.baseRecordType = cpu.placement.baseRecordType;
    gpu.q220LooseObject =
        cpu.placement.refFormId != 0u &&
        Q220IsLooseRecordType(gpu.baseRecordType);
    gpu.q223PlacementScale = cpu.placement.scale;
    if (gpu.q220LooseObject) {
        static std::unordered_set<uint32_t> q220LoggedRefs;
        if (q220LoggedRefs.insert(gpu.refFormId).second) {
            Q6H_LOGI("Q22.5 LOOSE CANDIDATE: ref=%08X base=%08X type=%s edid=%s model=%s",
                     gpu.refFormId, gpu.baseFormId,
                     gpu.baseRecordType.c_str(),
                     gpu.editorId.empty() ? "<none>" : gpu.editorId.c_str(),
                     gpu.modelPath.c_str());
        }
    }
    if (gpu.baseRecordType == "DOOR") {
        const bool q1920WastelandExterior =
            gExteriorWorldspaceQ1890 == 0x0000003Cu &&
            (cpu.placement.hasExteriorGrid ||
             cpu.placement.persistentExteriorRef);
        ResolveDoorTeleportCachedQ1698(
            gpu.refFormId, q1920WastelandExterior, gpu.teleport);
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
    if (!cpu.mesh.environmentCubeTexturePath.empty()) {
        ++gEnvironmentCandidatesQ205A;
    }
    if (!UploadTexture(cpu.mesh.environmentMaskTexturePath,
                       {255u, 255u, 255u, 255u},
                       gpu.environmentMask, gpu.realEnvironmentMask,
                       "ENV_MASK", gpu.refFormId)) return false;
    if (!cpu.mesh.environmentMaskTexturePath.empty() && !gpu.realEnvironmentMask) {
        ++gEnvironmentMaskFailuresQ205A;
    }
    if (!UploadCubeTextureQ2050(cpu.mesh.environmentCubeTexturePath,
                                gpu.environmentCube,
                                gpu.realEnvironmentCube,
                                "ENV_CUBE", gpu.refFormId)) return false;
    gpu.environmentEnabledQ2050 =
        !gpu.noLighting && gpu.realNormal && gpu.realEnvironmentCube &&
        gpu.environmentMapScaleQ2050 > 0.0f;
    if (gpu.environmentEnabledQ2050) {
        ++gEnvironmentEnabledMaterialsQ205A;
    }
    if (gpu.environmentEnabledQ2050) {
        Q6H_LOGI("Q20.5 ENV MATERIAL: ref=%08X model=%s cube=%s mask=%s customMask=%d scale=%.4f vertexColor=%d pcPass=SLS2057/2058 blend=ONE+ONE depth=EQUAL fadeScalar=neutral-until-recovered",
                 gpu.refFormId, gpu.modelPath.c_str(),
                 cpu.mesh.environmentCubeTexturePath.c_str(),
                 cpu.mesh.environmentMaskTexturePath.empty() ? "<normal-alpha>" :
                     cpu.mesh.environmentMaskTexturePath.c_str(),
                 gpu.realEnvironmentMask ? 1 : 0,
                 gpu.environmentMapScaleQ2050,
                 gpu.useVertexColor ? 1 : 0);
    }

    gpu.vertexCount =
        static_cast<GLsizei>(q1960ExpandedFloatCount / FLOATS_PER_VERTEX);
    gpu.q2017SharedCandidate = Q2017CanShareGeometry(gpu);
    gpu.q2017SharedMeshKey = gpu.q2017SharedCandidate
        ? Q2017SharedMeshKey(gpu) : 0u;

    const bool q2017Reused =
        gpu.q2017SharedCandidate && Q2017TryReuseSharedGeometry(gpu);
    if (!q2017Reused) {
        glGenVertexArrays(1, &gpu.vao);
        glBindVertexArray(gpu.vao);
        glGenBuffers(1, &gpu.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
        const GLsizeiptr q1960VboBytes =
            static_cast<GLsizeiptr>(
                q1960ExpandedFloatCount * sizeof(float));
        if (gQ1960CaptureVboQ19 && gQ1960CapturedVboVerticesQ19) {
            *gQ1960CapturedVboVerticesQ19 = std::move(expanded);
            glBufferData(GL_ARRAY_BUFFER, q1960VboBytes,
                         nullptr, GL_STATIC_DRAW);
        } else {
            glBufferData(GL_ARRAY_BUFFER, q1960VboBytes,
                         expanded.data(), GL_STATIC_DRAW);
        }

        constexpr GLsizei stride =
            static_cast<GLsizei>(
                FLOATS_PER_VERTEX * sizeof(float));
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            2, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            3, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(9 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(
            4, 2, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(12 * sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(
            5, 4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(14 * sizeof(float)));
        glEnableVertexAttribArray(5);
        glBindVertexArray(0);

        // Synchronous/non-Q19 callers have already filled the VBO.
        // Q19 publishes after its chunked glBufferSubData completes.
        if (!gQ1960CaptureVboQ19) {
            Q2017PublishSharedGeometry(gpu);
        }
    } else if (gQ1960CapturedVboVerticesQ19) {
        gQ1960CapturedVboVerticesQ19->clear();
    }

    // Same NIF path + shape index means identical authored vertex/material
    // content. Include render-state handles as a collision-resistant guard.
    uint64_t q2016Hash =
        static_cast<uint64_t>(std::hash<std::string>{}(gpu.modelPath));
    auto q2016Mix = [&](uint64_t value) {
        q2016Hash ^= value + 0x9e3779b97f4a7c15ULL +
                     (q2016Hash << 6u) + (q2016Hash >> 2u);
    };
    q2016Mix(gpu.q2016ShapeIndex);
    q2016Mix(static_cast<uint64_t>(gpu.vertexCount));
    q2016Mix(static_cast<uint64_t>(gpu.diffuse));
    q2016Mix(static_cast<uint64_t>(gpu.normal));
    q2016Mix(static_cast<uint64_t>(gpu.glow));
    q2016Mix(static_cast<uint64_t>(gpu.environmentCube));
    q2016Mix(static_cast<uint64_t>(gpu.environmentMask));
    q2016Mix(static_cast<uint64_t>(gpu.alphaTest));
    q2016Mix(static_cast<uint64_t>(gpu.zBufferTestQ1200) << 1u |
             static_cast<uint64_t>(gpu.zBufferWriteQ1200));
    q2016Mix(static_cast<uint64_t>(gpu.stencilDrawModePresent) << 8u |
             static_cast<uint64_t>(gpu.stencilDrawMode));
    gpu.q2016BatchKey = q2016Hash == 0u ? 1u : q2016Hash;

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
    gInstancingEnabledLocationQ2016 =
        glGetUniformLocation(gProgram, "uInstancingEnabledQ2016");
    gObjectTransformEnabledLocationQ2017 =
        glGetUniformLocation(gProgram, "uObjectTransformEnabledQ2017");
    gObjectTransformLocationQ2017 =
        glGetUniformLocation(gProgram, "uObjectTransformQ2017");
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
    gEnvironmentCubeLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentCubeQ2050");
    gEnvironmentMaskLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentMaskQ2050");
    gEnvironmentPassLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentPassQ2050");
    gEnvironmentScaleLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentScaleQ2050");
    gEnvironmentCustomMaskLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentCustomMaskQ2050");
    gEmissiveColorLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveColor");
    gEmissiveMultLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveMult");
    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uGlowEnabled");
    gExternalEmittanceEnabledLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceEnabledQ1380");
    gExternalEmittanceColorLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceColorQ1380");
    gNativeLodClipEnabledLocationQ1810 =
        glGetUniformLocation(gProgram, "uNativeLodClipEnabledQ1810");
    gLodFadeModeLocationQ2021 =
        glGetUniformLocation(gProgram, "uLodFadeModeQ2021");
    gTerrainLodModeLocationQ2024 =
        glGetUniformLocation(gProgram, "uTerrainLodModeQ2024");
    gLodFadePlayerLocationQ2021 =
        glGetUniformLocation(gProgram, "uLodFadePlayerQ2021");
    gLodFadeRangeLocationQ2021 =
        glGetUniformLocation(gProgram, "uLodFadeRangeQ2021");
    gNativeLodClipCellCountLocationQ1900 =
        glGetUniformLocation(gProgram, "uNativeLodClipCellCountQ1900");
    gNativeLodClipCellsLocationQ1900 =
        glGetUniformLocation(gProgram, "uNativeLodClipCellsQ1900[0]");
    gWaterReflectionClipEnabledLocationQ2090 =
        glGetUniformLocation(gProgram, "uWaterReflectionClipEnabledQ2090");
    gWaterReflectionPlaneYLocationQ2090 =
        glGetUniformLocation(gProgram, "uWaterReflectionPlaneYQ2090");
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
        if (Q2017ReleaseSharedGeometry(object)) continue;
        if (object.vbo) glDeleteBuffers(1, &object.vbo);
        if (object.vao) glDeleteVertexArrays(1, &object.vao);
        object.vbo = 0u;
        object.vao = 0u;
    }
    objects.clear();
}

bool gQ2013ExteriorWarmupPending = false;
std::chrono::steady_clock::time_point gQ2013ExteriorWarmupStarted{};

bool ProcessQ74TransitionRequest() {
    Fo3CellTransitionRequestQ74 request;
    if (!ConsumeFo3CellTransitionRequestQ74(request) || !request.valid) return false;

    const bool q2013HoldExteriorLoading =
        request.worldspaceFormId == 0x0000003Cu &&
        IsFo3LoadingVisibleQ1700();
    gQ2013ExteriorWarmupPending = q2013HoldExteriorLoading;
    // Q20.25: ProcessQ74TransitionRequest performs substantial synchronous
    // CELL/NIF/GPU scene work. Starting the async horizon timeout here meant
    // that work could consume the whole timeout before LOD got its first tick.
    // Arm the timer lazily on the first actual Wasteland streaming frame.
    gQ2013ExteriorWarmupStarted =
        std::chrono::steady_clock::time_point{};

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
        // Q20.13: start Level4 only after the new exterior/Q19 context exists.
        // The loading overlay remains up while that asynchronous warmup runs.
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
        Q6H_LOGI("Q16.27 STREAM CONTEXT: active=1 worldspace=%08X persistent=%08X grid=(%d,%d) originXTEL=(%.2f %.2f %.2f) residentRadius=2 activeRadius=1 collisionRadius=1 terrainRadius=3 rebuildMode=5x5-visible+7x7-warm+cached-3x3-collision+7x7-terrain+progressive-Level4-LOD",
                 gExteriorWorldspaceQ1890, gExteriorPersistentCellQ1890,
                 gExteriorWindowGridXQ1890, gExteriorWindowGridYQ1890,
                 gExteriorOriginXQ1890, gExteriorOriginYQ1890,
                 gExteriorOriginZQ1890);
        Q6H_LOGI("Q20.22B PROBE TRIGGER: phase=scene-transition worldspace=%08X grid=(%d,%d) wastelandExpected=0000003C willProbe=%d",
                 gExteriorWorldspaceQ1890,
                 gExteriorWindowGridXQ1890,
                 gExteriorWindowGridYQ1890,
                 gExteriorWorldspaceQ1890 == 0x0000003Cu ? 1 : 0);
        if (gExteriorWorldspaceQ1890 == 0x0000003Cu) {
            Q2022ProbeLodArchive(
                gExteriorWindowGridXQ1890,
                gExteriorWindowGridYQ1890);
        }
    } else {
        gExteriorStreamingActiveQ1890 = false;
        gExteriorWorldspaceQ1890 = 0u;
        gExteriorPersistentCellQ1890 = 0u;
        Q6H_LOGI("Q16.27 STREAM CONTEXT: active=0 reason=interior");
    }
    LoadFo3CellEnvironmentQ1410(request.cellFormId, request.worldspaceFormId,
                                request.x, request.y);
    Q6H_LOGI("Q20.9A WATER TRANSITION DISPATCH: cell=%08X worldspace=%08X XTEL=(%.2f %.2f %.2f) exteriorStreaming=%d",
             request.cellFormId, request.worldspaceFormId,
             request.x, request.y, request.z,
             gExteriorStreamingActiveQ1890 ? 1 : 0);
    if (request.worldspaceFormId != 0u) {
        const bool q209aWaterLoaded =
            LoadFo3WaterSceneQ2070(request.worldspaceFormId, request.x, request.y);
        Q6H_LOGI("Q20.9A WATER TRANSITION RESULT: worldspace=%08X loaded=%d nearbyWaterCells=%zu",
                 request.worldspaceFormId,
                 q209aWaterLoaded ? 1 : 0,
                 GetFo3WaterCellsQ2070().size());
    } else {
        ClearFo3WaterSceneQ2070();
        Q6H_LOGI("Q20.9A WATER TRANSITION RESULT: worldspace=00000000 loaded=0 nearbyWaterCells=0 reason=interior-or-missing-WRLD");
    }
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
    if (!gRuntime.ready || !gRuntime.weather.haveNam0 || !gRuntime.climate.valid ||
        !gFo3ImageSpaceQ1280.valid) {
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

    // Q20.3b: keep the capture-proven raw WTHR SP17 colour domain, but stop
    // freezing the directional-light multiplier at the noon capture's x2.5.
    // Q14 already carries the live blended ImageSpace/IMAD Sunlight Dimmer;
    // use the same authored runtime value here so statics follow 12:00->18:00
    // ->00:00 intensity changes without touching the still-unresolved direction.
    baseSunDimmer = gFo3ImageSpaceQ1280.hdrSunlightDimmer;
    effectiveSunScale =
        fo3weatherq1320::EffectiveSunlightScaleQ1320(baseSunDimmer);
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
            "Q15.13 SP17 CORE: weather=%08X EDID=%s hour=%.2f ambient=(%.6f %.6f %.6f) sunlight=(%.6f %.6f %.6f) baseSunDimmer=%.3f effectiveSunScale=%.3f scope=static-PPLighting core=PC_DP3_DIFFUSE_TANGENT_HALF_SPEC normalAlphaSpec=1 lowNdotLSpecGate=1 syntheticSpec032=0 baseMap=GL_RGBA8_RAW pcFog=VERTEX_INTERPOLATED LightData=PC_CAPTURE_Q20.3D vcolorRule=EXE_GEOMETRY_STREAM_Q20.4B sunScaleSource=Q14-ImageSpace",
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

// Q20.21 source-backed exterior handoff values from the supplied PC INIs:
// uGridsToLoad=5, fNoLODFarDistanceMax=10240,
// fLODFadeOutPercent=0.6000, bLODPopObjects=0.
// The fade therefore starts at 6144 game units and ends at 10240.
constexpr float Q2021_DETAIL_FADE_START_GAME = 6144.0f;
constexpr float Q2021_DETAIL_FADE_END_GAME = 10240.0f;
constexpr float Q2021_DETAIL_FADE_START_M =
    Q2021_DETAIL_FADE_START_GAME / FO3_UNITS_PER_METRE;
constexpr float Q2021_DETAIL_FADE_END_M =
    Q2021_DETAIL_FADE_END_GAME / FO3_UNITS_PER_METRE;

// Q20.25: most Fallout 3 rock/cliff STATs are not authored VWD. Do not
// pretend they have generated block LOD. Instead, for non-VWD landscape-rock
// detail that is already resident in the 7x7 visual cache, hold it through the
// old generic cutoff and fade it across the outer resident ring.
constexpr float Q2025_ROCK_FADE_START_GAME = 10240.0f;
constexpr float Q2025_ROCK_FADE_END_GAME = 14336.0f; // 3.5 exterior CELLs.
constexpr float Q2025_ROCK_FADE_START_M =
    Q2025_ROCK_FADE_START_GAME / FO3_UNITS_PER_METRE;
constexpr float Q2025_ROCK_FADE_END_M =
    Q2025_ROCK_FADE_END_GAME / FO3_UNITS_PER_METRE;

bool gQ2021PlayerSceneValid = false;
float gQ2021PlayerSceneX = 0.0f;
float gQ2021PlayerSceneZ = 0.0f;

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

    // Q20.9 generic water window follows the same actual-centred exterior
    // stream. The expensive ESM scan is catalogue-cached per worldspace.
    LoadFo3WaterSceneQ2070(
        gExteriorWorldspaceQ1890, selectionGameX, selectionGameY);

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
    // Q20.17 shared STAT/SCOL/TREE VBOs live until their final REFR leaves.
    if (Q2017ReleaseSharedGeometry(object)) return;
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

    const float q2090WaterSelectionX =
        (static_cast<float>(gExteriorWindowGridXQ1890) + 0.5f) *
        Q1890_EXTERIOR_CELL_SIZE;
    const float q2090WaterSelectionY =
        (static_cast<float>(gExteriorWindowGridYQ1890) + 0.5f) *
        Q1890_EXTERIOR_CELL_SIZE;
    LoadFo3WaterSceneQ2070(
        gExteriorWorldspaceQ1890,
        q2090WaterSelectionX, q2090WaterSelectionY);

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

#include "fo3-cell-streaming-q19.inc"

void UpdateFo3ExteriorStreamingQ1890(float virtualHeadX, float virtualHeadZ) {
    static std::chrono::steady_clock::time_point q1970PreviousUpdate{};
    static uint64_t q1970PreviousLodUs = 0u;
    static uint64_t q1970PreviousDetailUs = 0u;
    const auto q1970UpdateStarted = std::chrono::steady_clock::now();
    if (q1970PreviousUpdate.time_since_epoch().count() != 0) {
        const uint64_t gapUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                q1970UpdateStarted - q1970PreviousUpdate).count());
        if (gapUs >= 50000u) {
            Q6H_LOGW("Q19.7 FRAME GAP: gapUs=%llu thresholdUs=50000 previousLodUs=%llu previousDetailUs=%llu worldspace=%08X",
                     static_cast<unsigned long long>(gapUs),
                     static_cast<unsigned long long>(q1970PreviousLodUs),
                     static_cast<unsigned long long>(q1970PreviousDetailUs),
                     gExteriorWorldspaceQ1890);
        }
    }
    q1970PreviousUpdate = q1970UpdateStarted;
    q1970PreviousLodUs = 0u;
    q1970PreviousDetailUs = 0u;

    if (!gExteriorStreamingActiveQ1890) {
        gQ1920LatestGridValid = false;
        gQ2021PlayerSceneValid = false;
        return;
    }

    const float gameX = gExteriorOriginXQ1890 +
                        virtualHeadX * FO3_UNITS_PER_METRE;
    const float gameY = gExteriorOriginYQ1890 +
                        (SCENE_FORWARD - virtualHeadZ) * FO3_UNITS_PER_METRE;
    gQ2021PlayerSceneValid = true;
    gQ2021PlayerSceneX = virtualHeadX;
    gQ2021PlayerSceneZ = virtualHeadZ;
    const int32_t actualGridX = static_cast<int32_t>(
        std::floor(gameX / Q1890_EXTERIOR_CELL_SIZE));
    const int32_t actualGridY = static_cast<int32_t>(
        std::floor(gameY / Q1890_EXTERIOR_CELL_SIZE));
    gQ1920LatestGridValid = true;
    gQ1920LatestGridX = actualGridX;
    gQ1920LatestGridY = actualGridY;
    if (gExteriorWorldspaceQ1890 == 0x0000003Cu) {
        Q2022ProbeLodArchive(actualGridX, actualGridY);
    } else {
        static bool q2022LoggedNonWasteland = false;
        if (!q2022LoggedNonWasteland) {
            q2022LoggedNonWasteland = true;
            Q6H_LOGI("Q20.22B PROBE WAIT: phase=runtime worldspace=%08X actual=(%d,%d) reason=not-wasteland-0000003C",
                     gExteriorWorldspaceQ1890, actualGridX, actualGridY);
        }
    }
    if (gExteriorWorldspaceQ1890 != 0u) {
        // Q20.10: CELL-specific streaming now owns every exterior worldspace,
        // including child worlds such as MegatonWorld. Native Level4 LOD remains
        // Wasteland-only because those archive paths are authored for 0000003C.
        const auto q1970DetailStarted = std::chrono::steady_clock::now();
        Q1900UpdateCellStreamingQ19(gameX, gameY, actualGridX, actualGridY);
        q1970PreviousDetailUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - q1970DetailStarted).count());

        if (gExteriorWorldspaceQ1890 == 0x0000003Cu) {
            if (gQ2013ExteriorWarmupPending &&
                IsFo3LoadingVisibleQ1700() &&
                gQ2013ExteriorWarmupStarted.time_since_epoch().count() == 0) {
                gQ2013ExteriorWarmupStarted =
                    std::chrono::steady_clock::now();
                Q6H_LOGI("Q20.25 EXTERIOR WARMUP STREAM START: actual=(%d,%d) timerOrigin=first-wasteland-stream-frame",
                         actualGridX, actualGridY);
            }

            const auto q1970LodStarted = std::chrono::steady_clock::now();
            Q1990EnsureNativeLodForCell(actualGridX, actualGridY,
                                        gExteriorOriginXQ1890,
                                        gExteriorOriginYQ1890,
                                        gExteriorOriginZQ1890);
            q1970PreviousLodUs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - q1970LodStarted).count());

            if (gQ2013ExteriorWarmupPending && IsFo3LoadingVisibleQ1700()) {
                size_t detailReady = 0u;
                size_t lodReady = 0u;
                const bool detailComplete =
                    Q1900WarmShellReadyQ2013(
                        actualGridX, actualGridY, &detailReady);
                Q2013NativeLodBootstrapReadyQ19(
                    actualGridX, actualGridY, &lodReady);
                size_t level32Ready = 0u, level32Target = 0u;
                size_t highReady = 0u, highTarget = 0u;
                Q2024GetAuthoredWarmupState(
                    level32Ready, level32Target,
                    highReady, highTarget);
                const bool nearLodComplete = lodReady >= 9u;
                const bool horizonComplete =
                    level32Target > 0u &&
                    level32Ready >= level32Target &&
                    highTarget > 0u &&
                    highReady >= highTarget;
                const uint64_t warmupUs =
                    gQ2013ExteriorWarmupStarted.time_since_epoch().count() == 0
                        ? 0u
                        : static_cast<uint64_t>(
                            std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() -
                                gQ2013ExteriorWarmupStarted).count());
                const bool timeout = warmupUs >= 25000000u;
                if ((detailComplete && nearLodComplete && horizonComplete) ||
                    timeout) {
                    Q6H_LOGI("Q20.25 EXTERIOR WARMUP COMPLETE: detailReady=%zu/49 nearLevel4=%zu/9 level32=%zu/%zu high=%zu/%zu elapsedUs=%llu timeout=%d action=release-loading-screen",
                             detailReady, lodReady,
                             level32Ready, level32Target,
                             highReady, highTarget,
                             static_cast<unsigned long long>(warmupUs),
                             timeout ? 1 : 0);
                    gQ2013ExteriorWarmupPending = false;
                    NotifyFo3TransitionCompleteQ1700();
                }
            }
        }

        const uint64_t q1970TotalUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - q1970UpdateStarted).count());
        if (q1970TotalUs >= 50000u) {
            Q6H_LOGW("Q20.10 STREAM STALL: totalUs=%llu lodUs=%llu detailUs=%llu worldspace=%08X actual=(%d,%d) streamBusy=%d",
                     static_cast<unsigned long long>(q1970TotalUs),
                     static_cast<unsigned long long>(q1970PreviousLodUs),
                     static_cast<unsigned long long>(q1970PreviousDetailUs),
                     gExteriorWorldspaceQ1890,
                     actualGridX, actualGridY,
                     gExteriorStreamBusyQ1890 ? 1 : 0);
        }
        return;
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
        Q6H_LOGI("Q20.21 VANILLA HANDOFF: actual=(%d,%d) residentCentre=(%d,%d) residentShapes=%zu activeShapes=%zu activeTriangles=%zu uGrids=5 distantCount=20 fadeStartGame=6144 fadeEndGame=10240 fadePercent=0.6 popObjects=0 mode=continuous-radius+a2c",
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

    const int cheb =
        std::max(std::abs(object.q1970GridX - activeGridX),
                 std::abs(object.q1970GridY - activeGridY));

    if (gExteriorWorldspaceQ1890 != 0x0000003Cu ||
        !gQ2021PlayerSceneValid) {
        if (cheb <= 2) return true;
        if (cheb > 3) return false;
        return true;
    }

    // Q20.25: keep the generic Q20.21 fade for authored VWD/static objects.
    // Non-VWD landscape rocks have no generated block replacement in the
    // overwhelming majority of the ESM, so let already-resident rock detail
    // survive to the edge of the 7x7 visual cache and fade there instead.
    if (cheb > 3) return false;
    const float nearestX =
        gQ2021PlayerSceneX < object.minX ? object.minX :
        (gQ2021PlayerSceneX > object.maxX ? object.maxX :
         gQ2021PlayerSceneX);
    const float nearestZ =
        gQ2021PlayerSceneZ < object.minZ ? object.minZ :
        (gQ2021PlayerSceneZ > object.maxZ ? object.maxZ :
         gQ2021PlayerSceneZ);
    const float dx = nearestX - gQ2021PlayerSceneX;
    const float dz = nearestZ - gQ2021PlayerSceneZ;
    const float fadeEnd =
        object.q2025LandscapeRock &&
        !object.q2025VisibleWhenDistant
            ? Q2025_ROCK_FADE_END_M
            : Q2021_DETAIL_FADE_END_M;
    return dx * dx + dz * dz <= fadeEnd * fadeEnd;
}

int Q1900BuildNativeLodClipCells(float* bounds, bool objectLod) {
    int32_t activeGridX = 0;
    int32_t activeGridY = 0;
    if (!Q1970GetActiveGrid(activeGridX, activeGridY)) return 0;

    int count = 0;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int32_t cellX = activeGridX + dx;
            const int32_t cellY = activeGridY + dy;
            if (objectLod && !Q1900CellVisualReadyQ19(cellX, cellY)) continue;

            const float minGameX =
                static_cast<float>(cellX) * Q1890_EXTERIOR_CELL_SIZE;
            const float maxGameX =
                static_cast<float>(cellX + 1) * Q1890_EXTERIOR_CELL_SIZE;
            const float minGameY =
                static_cast<float>(cellY) * Q1890_EXTERIOR_CELL_SIZE;
            const float maxGameY =
                static_cast<float>(cellY + 1) * Q1890_EXTERIOR_CELL_SIZE;
            const float minX =
                (minGameX - gExteriorOriginXQ1890) / FO3_UNITS_PER_METRE;
            const float maxX =
                (maxGameX - gExteriorOriginXQ1890) / FO3_UNITS_PER_METRE;
            const float z0 =
                SCENE_FORWARD -
                (minGameY - gExteriorOriginYQ1890) / FO3_UNITS_PER_METRE;
            const float z1 =
                SCENE_FORWARD -
                (maxGameY - gExteriorOriginYQ1890) / FO3_UNITS_PER_METRE;

            bounds[count * 4 + 0] = minX;
            bounds[count * 4 + 1] = maxX;
            bounds[count * 4 + 2] = std::min(z0, z1);
            bounds[count * 4 + 3] = std::max(z0, z1);
            ++count;
        }
    }
    return count;
}

void DrawSceneObject(const GpuObject& object, bool environmentPassQ2050 = false) {
    if (!object.q210PlayerBody &&
        !object.q230NpcActor &&
        !Q1970ShouldRenderFullDetail(object)) return;
    if (!object.q210PlayerBody &&
        !object.q230NpcActor &&
        !object.q220LooseObject &&
        !Q2015AabbVisible(object)) return;
    if (environmentPassQ2050 && !object.environmentEnabledQ2050) return;
    if (gInstancingEnabledLocationQ2016 >= 0) {
        glUniform1f(gInstancingEnabledLocationQ2016,
                    gInstancedDrawActiveQ2016 ? 1.0f : 0.0f);
    }
    const bool q2017UseObjectTransform =
        (object.q2017SharedGeometry ||
         object.q210PlayerBody ||
         object.q220LooseObject) &&
        !gInstancedDrawActiveQ2016;
    if (gObjectTransformEnabledLocationQ2017 >= 0) {
        glUniform1f(gObjectTransformEnabledLocationQ2017,
                    q2017UseObjectTransform ? 1.0f : 0.0f);
    }
    if (q2017UseObjectTransform &&
        gObjectTransformLocationQ2017 >= 0) {
        const float* q220Transform =
            object.q210PlayerBody
                ? gQ210PlayerRoot
                : object.q220LooseObject
                    ? object.q220DynamicTransform
                    : object.q2017RelativeMatrix;
        glUniformMatrix4fv(
            gObjectTransformLocationQ2017, 1, GL_FALSE,
            q220Transform);
    }

    float q1900LodClipCells[25 * 4]{};
    int q1900LodClipCount = 0;
    if (object.q1990NativeLod &&
        gNativeLodClipEnabledLocationQ1810 >= 0 &&
        gNativeLodClipCellCountLocationQ1900 >= 0 &&
        gNativeLodClipCellsLocationQ1900 >= 0) {
        const std::string q1900Path = object.modelPath;
        const bool q1900ObjectLod =
            q1900Path.find("\\blocks\\") != std::string::npos;
        q1900LodClipCount = q1900ObjectLod
            ? 0
            : Q1900BuildNativeLodClipCells(
                  q1900LodClipCells, false);
    }
    if (gNativeLodClipEnabledLocationQ1810 >= 0) {
        glUniform1f(gNativeLodClipEnabledLocationQ1810,
                    q1900LodClipCount > 0 ? 1.0f : 0.0f);
    }

    if (gLodFadeModeLocationQ2021 >= 0) {
        float q2021Mode = 0.0f;
        if (gExteriorWorldspaceQ1890 == 0x0000003Cu &&
            gQ2021PlayerSceneValid) {
            if (!object.q1990NativeLod &&
                !object.q210PlayerBody &&
                !object.q230NpcActor) {
                q2021Mode = 1.0f;
            } else if (object.modelPath.find("\\blocks\\") !=
                       std::string::npos) {
                q2021Mode = 2.0f;
            }
        }
        glUniform1f(gLodFadeModeLocationQ2021, q2021Mode);
    }
    if (gTerrainLodModeLocationQ2024 >= 0) {
        glUniform1f(gTerrainLodModeLocationQ2024,
                    object.q2024TerrainLod ? 1.0f : 0.0f);
    }
    if (gLodFadePlayerLocationQ2021 >= 0) {
        glUniform2f(gLodFadePlayerLocationQ2021,
                    gQ2021PlayerSceneX, gQ2021PlayerSceneZ);
    }
    if (gLodFadeRangeLocationQ2021 >= 0) {
        const bool q2025ExtendedRockFade =
            !object.q1990NativeLod &&
            object.q2025LandscapeRock &&
            !object.q2025VisibleWhenDistant;
        glUniform2f(gLodFadeRangeLocationQ2021,
                    q2025ExtendedRockFade
                        ? Q2025_ROCK_FADE_START_M
                        : Q2021_DETAIL_FADE_START_M,
                    q2025ExtendedRockFade
                        ? Q2025_ROCK_FADE_END_M
                        : Q2021_DETAIL_FADE_END_M);
    }
    if (gNativeLodClipCellCountLocationQ1900 >= 0) {
        glUniform1i(gNativeLodClipCellCountLocationQ1900,
                    q1900LodClipCount);
    }
    if (q1900LodClipCount > 0 &&
        gNativeLodClipCellsLocationQ1900 >= 0) {
        glUniform4fv(gNativeLodClipCellsLocationQ1900,
                     q1900LodClipCount, q1900LodClipCells);
    }
    glUniform1f(gGlossinessLocation, object.glossiness);
    glUniform1f(gNoLightingLocationQ1020, object.noLighting ? 1.0f : 0.0f);
    glUniform1f(gNoLightingFalloffLocationQ1160, object.noLightingFalloff ? 1.0f : 0.0f);
    glUniform4fv(gNoLightingFalloffParamsLocationQ1160, 1, object.noLightingFalloffParams);
    // Q20.25: landscape-LOD NIF vertex colours are not the detailed LAND
    // material colour path. Multiplying them into the diffuse was driving far
    // terrain towards black. Keep the authored stream for diagnostics, but do
    // not apply it to landscape LOD colour/alpha.
    const bool q2025ApplyVertexColor =
        object.useVertexColor && !object.q2024TerrainLod;
    const bool q2025ApplyVertexAlpha =
        object.useVertexAlpha && !object.q2024TerrainLod;
    glUniform1f(gUseVertexColorLocationQ1020,
                q2025ApplyVertexColor ? 1.0f : 0.0f);
    glUniform1f(gUseVertexAlphaLocationQ1020,
                q2025ApplyVertexAlpha ? 1.0f : 0.0f);
    glUniform1f(gSpecularEnabledLocationQ1020, object.specularEnabled ? 1.0f : 0.0f);
    glUniform3fv(gSpecularColorLocationQ1020, 1, object.specularColor);
    if (gEnvironmentPassLocationQ2050 >= 0)
        glUniform1f(gEnvironmentPassLocationQ2050, environmentPassQ2050 ? 1.0f : 0.0f);
    if (gEnvironmentScaleLocationQ2050 >= 0)
        glUniform1f(gEnvironmentScaleLocationQ2050, object.environmentMapScaleQ2050);
    if (gEnvironmentCustomMaskLocationQ2050 >= 0)
        glUniform1f(gEnvironmentCustomMaskLocationQ2050,
                    object.realEnvironmentMask ? 1.0f : 0.0f);
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
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_CUBE_MAP,
                  object.environmentEnabledQ2050 ? object.environmentCube : 0u);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D,
                  object.realEnvironmentMask ? object.environmentMask : 0u);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(object.vao);

    if (gInstancedDrawActiveQ2016 &&
        gInstanceCountQ2017 > 0 &&
        gInstanceBufferQ2016 != 0u) {
        glBindBuffer(GL_ARRAY_BUFFER, gInstanceBufferQ2016);
        constexpr GLsizei q2016Stride =
            static_cast<GLsizei>(16u * sizeof(float));
        const uintptr_t q2017BaseBytes =
            static_cast<uintptr_t>(
                gInstanceMatrixBaseFloatQ2017 * sizeof(float));
        for (GLuint q2016Column = 0u; q2016Column < 4u; ++q2016Column) {
            const GLuint location = 6u + q2016Column;
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(
                location, 4, GL_FLOAT, GL_FALSE, q2016Stride,
                reinterpret_cast<const void*>(
                    q2017BaseBytes +
                    q2016Column * 4u * sizeof(float)));
            glVertexAttribDivisor(location, 1u);
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0u);
    }

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
    glFrontFace(gWaterReflectionPassQ2090 ? GL_CW : GL_CCW);

    // NiStencilProperty is FO3's authored override for two-sided / reversed
    // face rendering. Preserve Q11.6's decoded draw mode exactly.
    if (object.stencilDrawModePresent) {
        if (object.stencilDrawMode == 3u) {
            glDisable(GL_CULL_FACE);
        } else {
            GLenum q2090Front = object.stencilDrawMode == 2u ? GL_CW : GL_CCW;
            if (gWaterReflectionPassQ2090) {
                q2090Front = q2090Front == GL_CW ? GL_CCW : GL_CW;
            }
            glFrontFace(q2090Front);
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

    if (gInstancedDrawActiveQ2016 &&
        gInstanceCountQ2017 > 0) {
        glDrawArraysInstanced(
            GL_TRIANGLES, 0, object.vertexCount,
            gInstanceCountQ2017);
        for (GLuint q2016Column = 0u; q2016Column < 4u; ++q2016Column) {
            const GLuint location = 6u + q2016Column;
            glVertexAttribDivisor(location, 0u);
            glDisableVertexAttribArray(location);
        }
    } else {
        glDrawArrays(GL_TRIANGLES, 0, object.vertexCount);
    }

    if (object.decalQ1170) {
        glPolygonOffset(q1170OldFactor, q1170OldUnits);
        if (q1170OffsetWasEnabled) glEnable(GL_POLYGON_OFFSET_FILL);
        else glDisable(GL_POLYGON_OFFSET_FILL);
    }

    glFrontFace(static_cast<GLenum>(q1160PreviousFrontFace));
    glCullFace(static_cast<GLenum>(q1160PreviousCullMode));
    if (q1160CullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}

bool Q2017EligibleForInstancing(const GpuObject& object) {
    return object.q2017SharedGeometry &&
           !object.q220LooseObject &&
           gExteriorStreamingActiveQ1890 &&
           !object.q1990NativeLod &&
           !object.alphaBlend &&
           !object.decalQ1170 &&
           !object.externalEmittanceFlagQ1380 &&
           object.baseRecordType != "DOOR" &&
           object.q2016BatchKey != 0u &&
           object.vao != 0u &&
           object.vertexCount > 0;
}

struct Q2017InstanceBatch {
    const GpuObject* representative = nullptr;
    size_t matrixBaseFloat = 0u;
    GLsizei count = 0;
};

void Q2017RenderOpaqueDetailedInstanced() {
    std::unordered_map<uint64_t, std::vector<const GpuObject*>> groups;
    groups.reserve(gObjects.size() / 3u + 1u);
    std::vector<const GpuObject*> singles;
    singles.reserve(gObjects.size() / 4u + 1u);

    size_t visible = 0u;
    for (const GpuObject& object : gObjects) {
        if (object.alphaBlend) continue;
        if (!Q1970ShouldRenderFullDetail(object)) continue;
        if (!Q2015AabbVisible(object)) continue;
        ++visible;
        if (Q2017EligibleForInstancing(object)) {
            groups[object.q2016BatchKey].push_back(&object);
        } else {
            singles.push_back(&object);
        }
    }

    std::vector<Q2017InstanceBatch> batches;
    batches.reserve(groups.size());
    std::vector<const GpuObject*> groupFallbacks;
    gInstanceMatricesQ2016.clear();
    gInstanceMatricesQ2016.reserve(visible * 16u);

    size_t instancedObjects = 0u;
    size_t largestBatch = 0u;
    for (auto& entry : groups) {
        std::vector<const GpuObject*>& group = entry.second;
        if (group.empty()) continue;
        const GpuObject* representative = group.front();

        const size_t matrixStart = gInstanceMatricesQ2016.size();
        size_t validCount = 0u;
        for (const GpuObject* target : group) {
            if (!target ||
                target->q2017SharedMeshKey !=
                    representative->q2017SharedMeshKey ||
                target->vao != representative->vao ||
                target->vbo != representative->vbo ||
                target->vertexCount != representative->vertexCount ||
                target->diffuse != representative->diffuse ||
                target->normal != representative->normal ||
                target->glow != representative->glow ||
                target->environmentCube != representative->environmentCube ||
                target->environmentMask != representative->environmentMask) {
                if (target) groupFallbacks.push_back(target);
                continue;
            }
            gInstanceMatricesQ2016.insert(
                gInstanceMatricesQ2016.end(),
                target->q2017RelativeMatrix,
                target->q2017RelativeMatrix + 16);
            ++validCount;
        }

        if (validCount < 2u) {
            gInstanceMatricesQ2016.resize(matrixStart);
            for (const GpuObject* target : group) {
                if (target &&
                    std::find(groupFallbacks.begin(),
                              groupFallbacks.end(),
                              target) == groupFallbacks.end()) {
                    groupFallbacks.push_back(target);
                }
            }
            continue;
        }

        Q2017InstanceBatch batch;
        batch.representative = representative;
        batch.matrixBaseFloat = matrixStart;
        batch.count = static_cast<GLsizei>(validCount);
        batches.push_back(batch);
        instancedObjects += validCount;
        largestBatch = std::max(largestBatch, validCount);
    }

    // One driver upload for every instance matrix used by this eye.
    if (!gInstanceMatricesQ2016.empty()) {
        if (gInstanceBufferQ2016 == 0u)
            glGenBuffers(1, &gInstanceBufferQ2016);
        glBindBuffer(GL_ARRAY_BUFFER, gInstanceBufferQ2016);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                gInstanceMatricesQ2016.size() * sizeof(float)),
            gInstanceMatricesQ2016.data(),
            GL_STREAM_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0u);
    }

    size_t fallbackDraws = 0u;
    for (const GpuObject* object : singles) {
        if (!object) continue;
        if (object->zBufferTestQ1200) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
        glDepthMask(object->zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        DrawSceneObject(*object);
        ++fallbackDraws;
    }
    for (const GpuObject* object : groupFallbacks) {
        if (!object) continue;
        if (object->zBufferTestQ1200) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
        glDepthMask(object->zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        DrawSceneObject(*object);
        ++fallbackDraws;
    }

    for (const Q2017InstanceBatch& batch : batches) {
        if (!batch.representative || batch.count <= 0) continue;
        if (batch.representative->zBufferTestQ1200)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        glDepthMask(
            batch.representative->zBufferWriteQ1200
                ? GL_TRUE : GL_FALSE);

        gInstanceMatrixBaseFloatQ2017 = batch.matrixBaseFloat;
        gInstanceCountQ2017 = batch.count;
        gInstancedDrawActiveQ2016 = true;
        DrawSceneObject(*batch.representative);
        gInstancedDrawActiveQ2016 = false;
    }

    static uint64_t q2017EyePasses = 0u;
    ++q2017EyePasses;
    if ((q2017EyePasses % 240u) == 1u) {
        const size_t instancedDraws = batches.size();
        const size_t saved =
            instancedObjects > instancedDraws
                ? instancedObjects - instancedDraws : 0u;
        Q6H_LOGI("Q20.17 SHARED INSTANCING: visibleOpaque=%zu sharedMeshes=%zu cacheHits=%llu cacheMisses=%llu avoidedVboMB=%.2f instancedObjects=%zu instancedDraws=%zu fallbackDraws=%zu drawsSaved=%zu largestBatch=%zu matrixUploadsPerEye=%d liveShapes=%zu orientationBasis=C*R*[X,Y,Z]",
                 visible, gQ2017SharedGeometry.size(),
                 static_cast<unsigned long long>(gQ2017SharedHits),
                 static_cast<unsigned long long>(gQ2017SharedMisses),
                 static_cast<double>(gQ2017SharedAvoidedBytes) /
                     (1024.0 * 1024.0),
                 instancedObjects, instancedDraws, fallbackDraws,
                 saved, largestBatch, 1, gObjects.size());
    }

    gInstancedDrawActiveQ2016 = false;
    gInstanceMatrixBaseFloatQ2017 = 0u;
    gInstanceCountQ2017 = 0;
    gInstanceMatricesQ2016.clear();
}

bool Q1030InitializeRenderProgramOnly() {
    if (gProgram) return true;
    gProgram = CreateQ6HProgram();
    if (!gProgram) {
        Q6H_LOGE("Q10.3 DIRECT BOOT FAILED: stage=renderer reason=shader-program");
        return false;
    }

    gMvpLocation = glGetUniformLocation(gProgram, "uMvp");
    gInstancingEnabledLocationQ2016 =
        glGetUniformLocation(gProgram, "uInstancingEnabledQ2016");
    gObjectTransformEnabledLocationQ2017 =
        glGetUniformLocation(gProgram, "uObjectTransformEnabledQ2017");
    gObjectTransformLocationQ2017 =
        glGetUniformLocation(gProgram, "uObjectTransformQ2017");
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
    gEnvironmentCubeLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentCubeQ2050");
    gEnvironmentMaskLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentMaskQ2050");
    gEnvironmentPassLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentPassQ2050");
    gEnvironmentScaleLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentScaleQ2050");
    gEnvironmentCustomMaskLocationQ2050 = glGetUniformLocation(gProgram, "uEnvironmentCustomMaskQ2050");
    gEmissiveColorLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveColor");
    gEmissiveMultLocationQ1020 = glGetUniformLocation(gProgram, "uEmissiveMult");
    gGlowEnabledLocationQ1020 = glGetUniformLocation(gProgram, "uGlowEnabled");
    gExternalEmittanceEnabledLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceEnabledQ1380");
    gExternalEmittanceColorLocationQ1380 =
        glGetUniformLocation(gProgram, "uExternalEmittanceColorQ1380");
    gNativeLodClipEnabledLocationQ1810 =
        glGetUniformLocation(gProgram, "uNativeLodClipEnabledQ1810");
    gLodFadeModeLocationQ2021 =
        glGetUniformLocation(gProgram, "uLodFadeModeQ2021");
    gTerrainLodModeLocationQ2024 =
        glGetUniformLocation(gProgram, "uTerrainLodModeQ2024");
    gLodFadePlayerLocationQ2021 =
        glGetUniformLocation(gProgram, "uLodFadePlayerQ2021");
    gLodFadeRangeLocationQ2021 =
        glGetUniformLocation(gProgram, "uLodFadeRangeQ2021");
    gNativeLodClipCellCountLocationQ1900 =
        glGetUniformLocation(gProgram, "uNativeLodClipCellCountQ1900");
    gNativeLodClipCellsLocationQ1900 =
        glGetUniformLocation(gProgram, "uNativeLodClipCellsQ1900[0]");
    gWaterReflectionClipEnabledLocationQ2090 =
        glGetUniformLocation(gProgram, "uWaterReflectionClipEnabledQ2090");
    gWaterReflectionPlaneYLocationQ2090 =
        glGetUniformLocation(gProgram, "uWaterReflectionPlaneYQ2090");
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

// Q20.23: Bethesda ships a real terrain pyramid (Level8/16/32) plus a
// sparse Level4.High VWD object set. Keep those caches separate from the
// proven Level4 near-LOD cache so the detailed/Level4 handoff stays intact.
enum class Q2023LodAssetKind : uint8_t {
    CoarseTerrain = 0,
    HighObjects = 1,
};

struct Q2023LodAsset {
    Q2023LodAssetKind kind = Q2023LodAssetKind::CoarseTerrain;
    std::string path;
    int levelCells = 0;
    int32_t blockX = 0;
    int32_t blockY = 0;
    bool loaded = false;
    bool pending = false;
};

struct Q2023CoarseTerrainTile {
    int levelCells = 0;
    int32_t blockX = 0;
    int32_t blockY = 0;
    std::vector<GpuObject> terrain;
};

struct Q2023HighObjectBlock {
    int32_t blockX = 0;
    int32_t blockY = 0;
    std::vector<GpuObject> objects;
};

std::vector<Q2023LodAsset> gQ2023LodAssets;
std::vector<Q2023CoarseTerrainTile> gQ2023CoarseTerrainTiles;
std::vector<Q2023HighObjectBlock> gQ2023HighObjectBlocks;
bool gQ2023LodAssetsDiscovered = false;
bool gQ2023LodDiscoveryLogged = false;

void Q2024GetAuthoredWarmupState(size_t& level32Ready,
                                 size_t& level32Target,
                                 size_t& highReady,
                                 size_t& highTarget) {
    level32Ready = 0u;
    level32Target = 0u;
    highReady = gQ2023HighObjectBlocks.size();
    highTarget = 0u;
    for (const Q2023CoarseTerrainTile& tile : gQ2023CoarseTerrainTiles) {
        if (tile.levelCells == 32) ++level32Ready;
    }
    for (const Q2023LodAsset& asset : gQ2023LodAssets) {
        if (asset.kind == Q2023LodAssetKind::HighObjects) {
            ++highTarget;
        } else if (asset.kind == Q2023LodAssetKind::CoarseTerrain &&
                   asset.levelCells == 32) {
            ++level32Target;
        }
    }
}

uint64_t gQ1990NativeLodSerial = 0u;
bool gQ1990NativeLodOriginValid = false;
float gQ1990NativeLodCenterX = 0.0f;
float gQ1990NativeLodCenterY = 0.0f;
float gQ1990NativeLodFloorZ = 0.0f;
int32_t gQ1990NativeLodCentreBlockX = 0;
int32_t gQ1990NativeLodCentreBlockY = 0;
bool gQ1990NativeLodDrawLogged = false;
// Q20.1: requested Level4 centre follows the player, while the visible centre
// advances only when an equally coherent shell is available. This avoids the
// horizon reshuffling at every four-CELL Level4 boundary.
bool gQ2010VisibleLodValid = false;
int32_t gQ2010VisibleLodCentreBlockX = 0;
int32_t gQ2010VisibleLodCentreBlockY = 0;
int gQ2010VisibleLodRing = -1;

int32_t Q1990FloorToLevel4Block(int32_t cell) {
    int32_t quotient = cell / 4;
    if (cell < 0 && (cell % 4) != 0) --quotient;
    return quotient * 4;
}

void Q1990DeleteLodGpu(GpuObject& object) {
    if (Q2017ReleaseSharedGeometry(object)) return;
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
    for (Q2023CoarseTerrainTile& tile : gQ2023CoarseTerrainTiles) {
        for (GpuObject& object : tile.terrain) Q1990DeleteLodGpu(object);
    }
    for (Q2023HighObjectBlock& block : gQ2023HighObjectBlocks) {
        for (GpuObject& object : block.objects) Q1990DeleteLodGpu(object);
    }
    gQ1990NativeLodBlocks.clear();
    gQ2023CoarseTerrainTiles.clear();
    gQ2023HighObjectBlocks.clear();
    for (Q2023LodAsset& asset : gQ2023LodAssets) {
        asset.loaded = false;
        asset.pending = false;
    }
    gQ1990NativeLodDrawLogged = false;
    gQ2010VisibleLodValid = false;
    gQ2010VisibleLodCentreBlockX = 0;
    gQ2010VisibleLodCentreBlockY = 0;
    gQ2010VisibleLodRing = -1;
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

// Q20.22: authoritative Fallout - Meshes.bsa LOD hierarchy probe.
// This is diagnostic only: no rendering/streaming behaviour is changed.
bool gQ2022LodArchiveProbeDone = false;

int32_t Q2022FloorToSpan(int32_t cell, int span) {
    int32_t quotient = cell / span;
    if (cell < 0 && (cell % span) != 0) --quotient;
    return quotient * span;
}

bool Q2023ParseAxis(const std::string& path, const char* marker, int32_t& out) {
    const size_t markerPos = path.find(marker);
    if (markerPos == std::string::npos) return false;
    size_t p = markerPos + std::strlen(marker);
    bool negative = false;
    if (p < path.size() && path[p] == '-') {
        negative = true;
        ++p;
    }
    if (p >= path.size() || !std::isdigit(static_cast<unsigned char>(path[p])))
        return false;
    int32_t value = 0;
    while (p < path.size() &&
           std::isdigit(static_cast<unsigned char>(path[p]))) {
        value = value * 10 + static_cast<int32_t>(path[p] - '0');
        ++p;
    }
    out = negative ? -value : value;
    return true;
}

int Q2023ParseTerrainLevel(const std::string& path) {
    if (path.find(".level32.") != std::string::npos) return 32;
    if (path.find(".level16.") != std::string::npos) return 16;
    if (path.find(".level8.") != std::string::npos) return 8;
    if (path.find(".level4.") != std::string::npos) return 4;
    return 0;
}

bool Q2023EndsWith(const std::string& value, const char* suffix) {
    const size_t n = std::strlen(suffix);
    return value.size() >= n &&
           value.compare(value.size() - n, n, suffix) == 0;
}

bool Q2023DiscoverLodAssetsQ19() {
    if (gQ2023LodAssetsDiscovered) return true;

    std::vector<FalloutMeshIndexEntry> entries;
    if (!ListFalloutMeshFilesByPrefix(
            "Landscape\\LOD\\Wasteland\\", entries)) {
        return false;
    }

    size_t terrain8 = 0u, terrain16 = 0u, terrain32 = 0u;
    size_t high = 0u, postApocalypseSkipped = 0u, treeDtlDeferred = 0u;
    gQ2023LodAssets.clear();
    gQ2023LodAssets.reserve(384u);

    for (const FalloutMeshIndexEntry& entry : entries) {
        const std::string& path = entry.path;
        if (path.find("\\trees\\") != std::string::npos &&
            Q2023EndsWith(path, ".dtl")) {
            ++treeDtlDeferred;
            continue;
        }
        if (!Q2023EndsWith(path, ".nif")) continue;

        if (path.find(".postapocalypse.nif") != std::string::npos) {
            ++postApocalypseSkipped;
            continue;
        }

        int32_t x = 0, y = 0;
        if (!Q2023ParseAxis(path, ".x", x) ||
            !Q2023ParseAxis(path, ".y", y)) {
            continue;
        }

        const bool inBlocks =
            path.find("\\blocks\\") != std::string::npos;
        if (inBlocks &&
            path.find(".level4.high.") != std::string::npos) {
            Q2023LodAsset asset;
            asset.kind = Q2023LodAssetKind::HighObjects;
            asset.path = path;
            asset.levelCells = 4;
            asset.blockX = x;
            asset.blockY = y;
            gQ2023LodAssets.push_back(std::move(asset));
            ++high;
            continue;
        }

        if (inBlocks) continue;
        const int level = Q2023ParseTerrainLevel(path);
        if (level != 8 && level != 16 && level != 32) continue;

        Q2023LodAsset asset;
        asset.kind = Q2023LodAssetKind::CoarseTerrain;
        asset.path = path;
        asset.levelCells = level;
        asset.blockX = x;
        asset.blockY = y;
        gQ2023LodAssets.push_back(std::move(asset));
        if (level == 8) ++terrain8;
        else if (level == 16) ++terrain16;
        else ++terrain32;
    }

    gQ2023LodAssetsDiscovered = true;
    if (!gQ2023LodDiscoveryLogged) {
        gQ2023LodDiscoveryLogged = true;
        Q6H_LOGI("Q20.23 LOD PYRAMID DISCOVERED: assets=%zu terrain8=%zu terrain16=%zu terrain32=%zu highObjects=%zu postApocalypseSkipped=%zu treeDtlDeferred=%zu source=Fallout-Meshes.bsa-index",
                 gQ2023LodAssets.size(), terrain8, terrain16, terrain32,
                 high, postApocalypseSkipped, treeDtlDeferred);
    }
    return true;
}

bool Q2023TileDesiredQ19(
        int levelCells, int32_t blockX, int32_t blockY,
        int32_t playerCellX, int32_t playerCellY) {
    if (levelCells == 32) return true;
    const int32_t centreX = blockX + levelCells / 2;
    const int32_t centreY = blockY + levelCells / 2;
    const int cheb = std::max(
        std::abs(centreX - playerCellX),
        std::abs(centreY - playerCellY));
    if (levelCells == 16) return cheb <= 72;
    if (levelCells == 8) return cheb <= 44;
    return false;
}

bool Q2023AssetDesiredQ19(
        const Q2023LodAsset& asset,
        int32_t playerCellX, int32_t playerCellY) {
    if (asset.kind == Q2023LodAssetKind::HighObjects) return true;
    return Q2023TileDesiredQ19(
        asset.levelCells, asset.blockX, asset.blockY,
        playerCellX, playerCellY);
}

Q2023CoarseTerrainTile* Q2023FindCoarseTerrainTileQ19(
        int levelCells, int32_t blockX, int32_t blockY) {
    for (Q2023CoarseTerrainTile& tile : gQ2023CoarseTerrainTiles) {
        if (tile.levelCells == levelCells &&
            tile.blockX == blockX && tile.blockY == blockY) {
            return &tile;
        }
    }
    return nullptr;
}

bool Q2023TileActiveQ19(
        int levelCells, int32_t blockX, int32_t blockY,
        int32_t playerCellX, int32_t playerCellY) {
    return Q2023TileDesiredQ19(
               levelCells, blockX, blockY,
               playerCellX, playerCellY) &&
           Q2023FindCoarseTerrainTileQ19(
               levelCells, blockX, blockY) != nullptr;
}

bool Q2023TileFullyRefinedQ19(
        int levelCells, int32_t blockX, int32_t blockY,
        int32_t playerCellX, int32_t playerCellY) {
    const int childLevel = levelCells / 2;
    if (childLevel < 4) return false;
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const int32_t childX = blockX + dx * childLevel;
            const int32_t childY = blockY + dy * childLevel;
            if (childLevel == 4) {
                Q1990NativeLodBlock* child =
                    Q1990FindLodBlock(childX, childY);
                if (!child || !Q1990TerrainLodBlockDesired(childX, childY))
                    return false;
            } else if (!Q2023TileActiveQ19(
                           childLevel, childX, childY,
                           playerCellX, playerCellY)) {
                return false;
            }
        }
    }
    return true;
}

void Q2022ProbeLodArchive(int32_t cellX, int32_t cellY) {
    if (gQ2022LodArchiveProbeDone) return;

    Q6H_LOGI("Q20.22B LOD ARCHIVE PROBE BEGIN: playerCell=(%d,%d) worldspace=%08X source=scene-transition-or-runtime",
             cellX, cellY, gExteriorWorldspaceQ1890);

    std::vector<FalloutMeshIndexEntry> entries;
    if (!ListFalloutMeshFilesByPrefix(
            "Landscape\\LOD\\Wasteland\\", entries)) {
        Q6H_LOGE("Q20.22B LOD ARCHIVE PROBE FAILED: reason=bsa-index-unavailable retry=1");
        return;
    }
    gQ2022LodArchiveProbeDone = true;

    struct Bucket {
        const char* name;
        const char* needle;
        size_t count = 0u;
        uint64_t bytes = 0u;
        std::vector<const FalloutMeshIndexEntry*> samples;
    };
    Bucket buckets[] = {
        {"Level4", "level4"},
        {"Level8", "level8"},
        {"Level16", "level16"},
        {"Level32", "level32"},
        {"Blocks", "\\blocks\\"},
        {"Trees", "\\trees\\"},
        {"High", "high"},
    };

    size_t nifCount = 0u;
    size_t otherLevelCount = 0u;
    for (const FalloutMeshIndexEntry& entry : entries) {
        if (entry.path.size() < 4u ||
            entry.path.substr(entry.path.size() - 4u) != ".nif") {
            continue;
        }
        ++nifCount;
        bool knownLevel = false;
        for (Bucket& bucket : buckets) {
            if (entry.path.find(bucket.needle) == std::string::npos) continue;
            ++bucket.count;
            bucket.bytes += entry.storedBytes;
            if (bucket.samples.size() < 5u)
                bucket.samples.push_back(&entry);
            if (bucket.name[0] == 'L') knownLevel = true;
        }
        if (entry.path.find("level") != std::string::npos &&
            !knownLevel) {
            ++otherLevelCount;
        }
    }

    Q6H_LOGI("Q20.22 LOD ARCHIVE SUMMARY: playerCell=(%d,%d) prefix=landscape\\lod\\wasteland entries=%zu nif=%zu level4=%zu level8=%zu level16=%zu level32=%zu blocks=%zu trees=%zu high=%zu otherLevel=%zu source=Fallout-Meshes.bsa-index renderChanges=0",
             cellX, cellY, entries.size(), nifCount,
             buckets[0].count, buckets[1].count,
             buckets[2].count, buckets[3].count,
             buckets[4].count, buckets[5].count,
             buckets[6].count, otherLevelCount);

    for (const Bucket& bucket : buckets) {
        Q6H_LOGI("Q20.22 LOD ARCHIVE BUCKET: category=%s count=%zu storedMB=%.2f samples=%zu",
                 bucket.name, bucket.count,
                 static_cast<double>(bucket.bytes) /
                     (1024.0 * 1024.0),
                 bucket.samples.size());
        for (const FalloutMeshIndexEntry* sample : bucket.samples) {
            Q6H_LOGI("Q20.22 LOD ARCHIVE SAMPLE: category=%s path=%s storedBytes=%u compressed=%d",
                     bucket.name, sample->path.c_str(),
                     sample->storedBytes,
                     sample->compressed ? 1 : 0);
        }
    }

    const int spans[] = {4, 8, 16, 32};
    for (int span : spans) {
        const int32_t bx = Q2022FloorToSpan(cellX, span);
        const int32_t by = Q2022FloorToSpan(cellY, span);
        const std::string level =
            "level" + std::to_string(span);
        const std::string coord =
            ".x" + std::to_string(bx) +
            ".y" + std::to_string(by);

        size_t localMatches = 0u;
        for (const FalloutMeshIndexEntry& entry : entries) {
            if (entry.path.find(level) == std::string::npos ||
                entry.path.find(coord) == std::string::npos) {
                continue;
            }
            ++localMatches;
            if (localMatches <= 16u) {
                Q6H_LOGI("Q20.22 LOD LOCAL: span=%d aligned=(%d,%d) path=%s storedBytes=%u compressed=%d",
                         span, bx, by, entry.path.c_str(),
                         entry.storedBytes,
                         entry.compressed ? 1 : 0);
            }
        }
        Q6H_LOGI("Q20.22 LOD LOCAL SUMMARY: span=%d playerCell=(%d,%d) aligned=(%d,%d) matches=%zu",
                 span, cellX, cellY, bx, by, localMatches);
    }

    // Also surface exact archive conventions that do not fit our guessed
    // Level4/8/16/32 vocabulary.
    size_t unusualLogged = 0u;
    for (const FalloutMeshIndexEntry& entry : entries) {
        if (entry.path.size() < 4u ||
            entry.path.substr(entry.path.size() - 4u) != ".nif" ||
            entry.path.find("level") == std::string::npos) {
            continue;
        }
        const bool known =
            entry.path.find("level4") != std::string::npos ||
            entry.path.find("level8") != std::string::npos ||
            entry.path.find("level16") != std::string::npos ||
            entry.path.find("level32") != std::string::npos;
        if (known) continue;
        Q6H_LOGI("Q20.22 LOD UNUSUAL LEVEL: path=%s storedBytes=%u",
                 entry.path.c_str(), entry.storedBytes);
        if (++unusualLogged >= 20u) break;
    }
}

// Q20.24: separate terrain refinement from object-LOD range.
// Terrain Level4 keeps the existing uGridDistantCount=20 window. The PC
// profile separately requests fBlockLoadDistance=125000 (~30.5 CELLs), so
// object blocks are searched through the next aligned four-CELL macroblock.
constexpr int Q1840_DISTANT_GRID_RADIUS_CELLS = 20;
constexpr int Q1840_DISTANT_BLOCK_RADIUS =
    (Q1840_DISTANT_GRID_RADIUS_CELLS + Q1840_LEVEL4_BLOCK_CELLS - 1) /
    Q1840_LEVEL4_BLOCK_CELLS;
constexpr int Q1840_DISTANT_BLOCK_COORD_RADIUS =
    Q1840_DISTANT_BLOCK_RADIUS * Q1840_LEVEL4_BLOCK_CELLS;
constexpr size_t Q1840_DISTANT_TARGET_BLOCKS =
    static_cast<size_t>((Q1840_DISTANT_BLOCK_RADIUS * 2 + 1) *
                        (Q1840_DISTANT_BLOCK_RADIUS * 2 + 1));

constexpr int Q2024_OBJECT_LOD_RADIUS_CELLS = 31;
constexpr int Q2024_OBJECT_LOD_BLOCK_RADIUS =
    (Q2024_OBJECT_LOD_RADIUS_CELLS + Q1840_LEVEL4_BLOCK_CELLS - 1) /
    Q1840_LEVEL4_BLOCK_CELLS;
constexpr int Q2024_OBJECT_LOD_BLOCK_COORD_RADIUS =
    Q2024_OBJECT_LOD_BLOCK_RADIUS * Q1840_LEVEL4_BLOCK_CELLS;
constexpr size_t Q2024_OBJECT_LOD_TARGET_BLOCKS =
    static_cast<size_t>((Q2024_OBJECT_LOD_BLOCK_RADIUS * 2 + 1) *
                        (Q2024_OBJECT_LOD_BLOCK_RADIUS * 2 + 1));
constexpr size_t Q1840_DISTANT_CACHE_BLOCKS = 324u;

bool Q1990TerrainLodBlockDesired(int32_t blockX, int32_t blockY) {
    return std::abs(blockX - gQ1990NativeLodCentreBlockX) <=
               Q1840_DISTANT_BLOCK_COORD_RADIUS &&
           std::abs(blockY - gQ1990NativeLodCentreBlockY) <=
               Q1840_DISTANT_BLOCK_COORD_RADIUS;
}

bool Q1990LodBlockDesired(int32_t blockX, int32_t blockY) {
    return std::abs(blockX - gQ1990NativeLodCentreBlockX) <=
               Q2024_OBJECT_LOD_BLOCK_COORD_RADIUS &&
           std::abs(blockY - gQ1990NativeLodCentreBlockY) <=
               Q2024_OBJECT_LOD_BLOCK_COORD_RADIUS;
}

bool Q2013HasNativeObjectLodForCellQ19(int32_t cellX, int32_t cellY) {
    const int32_t blockX = Q1990FloorToLevel4Block(cellX);
    const int32_t blockY = Q1990FloorToLevel4Block(cellY);
    const Q1990NativeLodBlock* block = Q1990FindLodBlock(blockX, blockY);
    return block && !block->objects.empty();
}

size_t Q2013ProgressiveLodCountQ19() {
    size_t count = 0u;
    for (const Q1990NativeLodBlock& block : gQ1990NativeLodBlocks) {
        if (Q1990LodBlockDesired(block.blockX, block.blockY)) ++count;
    }
    return count;
}

bool Q2013NativeLodBootstrapReadyQ19(
        int32_t cellX, int32_t cellY, size_t* outReady) {
    const int32_t centreX = Q1990FloorToLevel4Block(cellX);
    const int32_t centreY = Q1990FloorToLevel4Block(cellY);
    size_t ready = 0u;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (Q1990FindLodBlock(
                    centreX + dx * Q1840_LEVEL4_BLOCK_CELLS,
                    centreY + dy * Q1840_LEVEL4_BLOCK_CELLS)) {
                ++ready;
            }
        }
    }
    if (outReady) *outReady = ready;
    return ready == 25u;
}

int Q2010NativeLodRingForBlockAround(
        int32_t blockX, int32_t blockY,
        int32_t centreBlockX, int32_t centreBlockY) {
    const int dx = std::abs(blockX - centreBlockX) /
                   Q1840_LEVEL4_BLOCK_CELLS;
    const int dy = std::abs(blockY - centreBlockY) /
                   Q1840_LEVEL4_BLOCK_CELLS;
    return std::max(dx, dy);
}

int Q2010CompleteNativeLodRingAround(
        int32_t centreBlockX, int32_t centreBlockY) {
    int completeRing = -1;
    for (int ring = 0; ring <= Q1840_DISTANT_BLOCK_RADIUS; ++ring) {
        bool complete = true;
        for (int dy = -ring; dy <= ring && complete; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring) continue;
                const int32_t bx = centreBlockX +
                    dx * Q1840_LEVEL4_BLOCK_CELLS;
                const int32_t by = centreBlockY +
                    dy * Q1840_LEVEL4_BLOCK_CELLS;
                if (!Q1990FindLodBlock(bx, by)) {
                    complete = false;
                    break;
                }
            }
        }
        if (!complete) break;
        completeRing = ring;
    }
    return completeRing;
}

size_t Q2010NativeLodVisibleBlockCount(int completeRing) {
    if (completeRing < 0) return 0u;
    const int side = completeRing * 2 + 1;
    return static_cast<size_t>(side * side);
}

void Q2010AdvanceVisibleNativeLodWindow() {
    // Q20.13: no complete-ring publication barrier. Every completed authored
    // Level4 block is immediately eligible to draw.
    gQ2010VisibleLodCentreBlockX = gQ1990NativeLodCentreBlockX;
    gQ2010VisibleLodCentreBlockY = gQ1990NativeLodCentreBlockY;
    gQ2010VisibleLodValid = !gQ1990NativeLodBlocks.empty();
    gQ2010VisibleLodRing =
        gQ2010VisibleLodValid ? Q1840_DISTANT_BLOCK_RADIUS : -1;
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
        Q6H_LOGI("Q20.24 NATIVE LOD HORIZON: playerCell=(%d,%d) centreBlock=(%d,%d) terrainRadiusCells=%d terrainBlocksRadius=%d terrainTarget=%zu objectRadiusCells=%d objectBlocksRadius=%d objectSearchTarget=%zu source=uGridDistantCount+fBlockLoadDistance125000",
                 cellX, cellY, centreBlockX, centreBlockY,
                 Q1840_DISTANT_GRID_RADIUS_CELLS,
                 Q1840_DISTANT_BLOCK_RADIUS,
                 Q1840_DISTANT_TARGET_BLOCKS,
                 Q2024_OBJECT_LOD_RADIUS_CELLS,
                 Q2024_OBJECT_LOD_BLOCK_RADIUS,
                 Q2024_OBJECT_LOD_TARGET_BLOCKS);
    }

    // Q19.7: Level4 extraction/parse/texture decode and GPU publication are
    // staged by the Q19 streamer. This call is bounded and never loads a full
    // authored macroblock synchronously on the VR thread.
    Q1970AdvanceNativeLodQ19(
        cellX, cellY, centerX, centerY, floorZ);

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

    Q2010AdvanceVisibleNativeLodWindow();

    static size_t q1840LastLoggedLoaded = static_cast<size_t>(-1);
    static int32_t q1840LastLoggedCentreX = INT32_MIN;
    static int32_t q1840LastLoggedCentreY = INT32_MIN;
    if (desiredLoaded != q1840LastLoggedLoaded ||
        centreBlockX != q1840LastLoggedCentreX ||
        centreBlockY != q1840LastLoggedCentreY) {
        q1840LastLoggedLoaded = desiredLoaded;
        q1840LastLoggedCentreX = centreBlockX;
        q1840LastLoggedCentreY = centreBlockY;
        Q6H_LOGI("Q20.24 NATIVE LOD WINDOW: playerCell=(%d,%d) requestedCentre=(%d,%d) objectBlocksVisited=%zu/%zu cachedBlocks=%zu terrainRadiusCells=%d objectRadiusCells=%d progressiveVisibleBlocks=%zu publishPolicy=split-terrain-object-horizons",
                 cellX, cellY, centreBlockX, centreBlockY,
                 desiredLoaded, Q2024_OBJECT_LOD_TARGET_BLOCKS,
                 gQ1990NativeLodBlocks.size(),
                 Q1840_DISTANT_GRID_RADIUS_CELLS,
                 Q2024_OBJECT_LOD_RADIUS_CELLS,
                 Q2013ProgressiveLodCountQ19());
    }
}

// -----------------------------------------------------------------------------
// Q19.7: staged native Level4 LOD.
// CPU extraction/scenegraph/texture decode shares the serialized asset lane with
// CELL/collision work. GPU upload reuses Q19.6's resumable texture/VBO stages.
// -----------------------------------------------------------------------------
struct Q1970LodWorkerTaskQ19 {
    uint64_t contextSerial = 0u;
    int32_t blockX = 0;
    int32_t blockY = 0;
    int ring = 0;
    bool q2023AuthoredAsset = false;
    size_t q2023AssetIndex = static_cast<size_t>(-1);
    Q2023LodAssetKind q2023Kind = Q2023LodAssetKind::CoarseTerrain;
    int q2023LevelCells = 4;
    std::string q2023Path;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float floorZ = 0.0f;
    std::vector<CpuObject> cpu;
    std::vector<Q1900TextureRequestQ19> textures;
    size_t terrainShapes = 0u;
    size_t objectShapes = 0u;
    size_t terrainTriangles = 0u;
    size_t objectTriangles = 0u;
    uint64_t workerUs = 0u;
    bool success = false;
    std::atomic<bool> ready{false};
};

constexpr size_t Q2013_LOD_WORKER_SLOTS = 4u;
constexpr size_t Q2014_LOD_UPLOAD_BACKLOG_LIMIT = 12u;
std::array<std::shared_ptr<Q1970LodWorkerTaskQ19>,
           Q2013_LOD_WORKER_SLOTS> gQ2013LodWorkersQ19;

struct Q1970LodUploadTaskQ2013 {
    std::unique_ptr<Q1900CellStateQ19> state;
    int32_t blockX = 0;
    int32_t blockY = 0;
    int ring = 0;
    bool q2023AuthoredAsset = false;
    size_t q2023AssetIndex = static_cast<size_t>(-1);
    Q2023LodAssetKind q2023Kind = Q2023LodAssetKind::CoarseTerrain;
    int q2023LevelCells = 4;
    uint64_t workerUs = 0u;
};
std::deque<Q1970LodUploadTaskQ2013> gQ2013LodUploadsQ19;

bool Q1970AnyLodWorkerQ19() {
    for (const auto& worker : gQ2013LodWorkersQ19) {
        if (worker) return true;
    }
    return false;
}

size_t Q2013ActiveLodWorkersQ19() {
    size_t active = 0u;
    for (const auto& worker : gQ2013LodWorkersQ19) {
        if (worker) ++active;
    }
    return active;
}

bool Q1970LodBlockPendingQ19(int32_t blockX, int32_t blockY) {
    for (const auto& worker : gQ2013LodWorkersQ19) {
        if (worker && worker->blockX == blockX &&
            worker->blockY == blockY) return true;
    }
    for (const auto& upload : gQ2013LodUploadsQ19) {
        if (upload.blockX == blockX && upload.blockY == blockY) return true;
    }
    return false;
}

bool Q1970NearDetailSafeQ19(int32_t actualGridX, int32_t actualGridY) {
    // Q20.12: LOD may prepare concurrently with outer detailed/prefetch work,
    // but never before the active 3x3 has real detailed coverage.
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto found = gQ1900CellsQ19.find(
                Q1900CellKeyQ19(actualGridX + dx, actualGridY + dy));
            if (found == gQ1900CellsQ19.end() ||
                !found->second.visualReady) {
                return false;
            }
        }
    }
    return true;
}

bool Q1970AppendLodNifCpuQ19(
        const std::string& path,
        const std::shared_ptr<Q1970LodWorkerTaskQ19>& task,
        bool terrain,
        std::unordered_set<std::string>& textureSeen) {
    std::vector<Fo3StaticNifMesh> meshes;
    if (!LoadFo3StaticNifMeshes(path, meshes) || meshes.empty())
        return false;

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
                cpu.mesh.positions[i * 3u],
                cpu.mesh.positions[i * 3u + 1u],
                cpu.mesh.positions[i * 3u + 2u]});
            cpu.normalsGame.push_back(Vec3{
                cpu.mesh.normals[i * 3u],
                cpu.mesh.normals[i * 3u + 1u],
                cpu.mesh.normals[i * 3u + 2u]});
            cpu.tangentsGame.push_back(Vec3{
                cpu.mesh.tangents[i * 3u],
                cpu.mesh.tangents[i * 3u + 1u],
                cpu.mesh.tangents[i * 3u + 2u]});
            cpu.bitangentsGame.push_back(Vec3{
                cpu.mesh.bitangents[i * 3u],
                cpu.mesh.bitangents[i * 3u + 1u],
                cpu.mesh.bitangents[i * 3u + 2u]});
        }

        if (!PrepareExpandedVertexStreamQ1960(
                cpu, task->centerX, task->centerY, task->floorZ)) {
            continue;
        }

        Q1900AddTextureRequestQ19(
            task->textures, textureSeen,
            cpu.mesh.diffuseTexturePath, "DIFFUSE",
            {180u,180u,180u,255u});
        Q1900AddTextureRequestQ19(
            task->textures, textureSeen,
            cpu.mesh.normalTexturePath, "NORMAL",
            {128u,128u,255u,255u});
        Q1900AddTextureRequestQ19(
            task->textures, textureSeen,
            cpu.mesh.glowTexturePath, "GLOW",
            {0u,0u,0u,255u});
        Q1900AddTextureRequestQ19(
            task->textures, textureSeen,
            cpu.mesh.environmentMaskTexturePath, "ENV_MASK",
            {255u,255u,255u,255u});

        const size_t triangles = cpu.mesh.indices.size() / 3u;
        if (terrain) {
            ++task->terrainShapes;
            task->terrainTriangles += triangles;
        } else {
            ++task->objectShapes;
            task->objectTriangles += triangles;
        }
        task->cpu.push_back(std::move(cpu));
        any = true;
    }
    return any;
}

void Q1970RunLodWorkerQ19(
        const std::shared_ptr<Q1970LodWorkerTaskQ19>& task) {
    const auto started = std::chrono::steady_clock::now();
    std::unordered_set<std::string> textureSeen;

    bool terrainReady = false;
    bool objectsReady = false;
    if (task->q2023AuthoredAsset) {
        if (task->q2023Kind == Q2023LodAssetKind::CoarseTerrain) {
            terrainReady = Q1970AppendLodNifCpuQ19(
                task->q2023Path, task, true, textureSeen);
        } else {
            objectsReady = Q1970AppendLodNifCpuQ19(
                task->q2023Path, task, false, textureSeen);
        }
    } else {
        const std::string suffix =
            "Wasteland.Level4.X" + std::to_string(task->blockX) +
            ".Y" + std::to_string(task->blockY) + ".NIF";
        const std::string terrainPath =
            "Landscape\\LOD\\Wasteland\\" + suffix;
        const std::string objectPath =
            "Landscape\\LOD\\Wasteland\\Blocks\\" + suffix;
        if (Q1990TerrainLodBlockDesired(
                task->blockX, task->blockY)) {
            terrainReady = Q1970AppendLodNifCpuQ19(
                terrainPath, task, true, textureSeen);
        }
        objectsReady = Q1970AppendLodNifCpuQ19(
            objectPath, task, false, textureSeen);
    }

    for (const Q1900TextureRequestQ19& request : task->textures) {
        Q1900PrepareTextureCpuQ19(request.path);
    }

    task->success = terrainReady || objectsReady || task->cpu.empty();
    task->workerUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    task->ready.store(true, std::memory_order_release);
}

void Q1970ConsumeLodWorkersQ19() {
    for (size_t slot = 0u; slot < Q2013_LOD_WORKER_SLOTS; ++slot) {
        auto& worker = gQ2013LodWorkersQ19[slot];
        if (!worker || !worker->ready.load(std::memory_order_acquire)) continue;

        const auto task = worker;
        worker.reset();
        const bool contextStale =
            task->contextSerial != gQ1900ContextSerialQ19 ||
            std::fabs(task->centerX - gExteriorOriginXQ1890) > 0.01f ||
            std::fabs(task->centerY - gExteriorOriginYQ1890) > 0.01f ||
            std::fabs(task->floorZ - gExteriorOriginZQ1890) > 0.01f;
        const bool level4Stale =
            !task->q2023AuthoredAsset &&
            !Q1990LodBlockDesired(task->blockX, task->blockY);
        const bool level4Duplicate =
            !task->q2023AuthoredAsset &&
            Q1990FindLodBlock(task->blockX, task->blockY) != nullptr;
        const bool authoredDuplicate =
            task->q2023AuthoredAsset &&
            task->q2023AssetIndex < gQ2023LodAssets.size() &&
            gQ2023LodAssets[task->q2023AssetIndex].loaded;
        if (contextStale || level4Stale ||
            level4Duplicate || authoredDuplicate) {
            if (task->q2023AuthoredAsset &&
                task->q2023AssetIndex < gQ2023LodAssets.size()) {
                gQ2023LodAssets[task->q2023AssetIndex].pending = false;
            }
            Q6H_LOGI("Q20.23 LOD CPU STALE: slot=%zu block=(%d,%d) ring=%d authored=%d level=%d workerUs=%llu action=discard",
                     slot + 1u, task->blockX, task->blockY, task->ring,
                     task->q2023AuthoredAsset ? 1 : 0,
                     task->q2023LevelCells,
                     static_cast<unsigned long long>(task->workerUs));
            continue;
        }

        Q1970LodUploadTaskQ2013 upload;
        upload.state = std::make_unique<Q1900CellStateQ19>();
        upload.state->gridX = task->blockX;
        upload.state->gridY = task->blockY;
        upload.state->stage = Q1900CellStageQ19::Uploading;
        upload.state->textures = std::move(task->textures);
        upload.state->cpu = std::move(task->cpu);
        upload.blockX = task->blockX;
        upload.blockY = task->blockY;
        upload.ring = task->ring;
        upload.q2023AuthoredAsset = task->q2023AuthoredAsset;
        upload.q2023AssetIndex = task->q2023AssetIndex;
        upload.q2023Kind = task->q2023Kind;
        upload.q2023LevelCells = task->q2023LevelCells;
        upload.workerUs = task->workerUs;
        gQ2013LodUploadsQ19.push_back(std::move(upload));

        Q6H_LOGI("Q20.23 LOD CPU READY: slot=%zu block=(%d,%d) ring=%d authored=%d level=%d queuedUploads=%zu workerUs=%llu",
                 slot + 1u, task->blockX, task->blockY, task->ring,
                 task->q2023AuthoredAsset ? 1 : 0,
                 task->q2023LevelCells,
                 gQ2013LodUploadsQ19.size(),
                 static_cast<unsigned long long>(task->workerUs));
    }
}

void Q1970AdvanceLodGpuQ19() {
    if (gQ2013LodUploadsQ19.empty()) return;

    // Drop work that fell outside the current authored horizon before it can
    // sit in front of newly-near blocks after a Level4 centre change.
    for (auto it = gQ2013LodUploadsQ19.begin();
         it != gQ2013LodUploadsQ19.end();) {
        const bool level4Drop =
            !it->q2023AuthoredAsset &&
            (!Q1990LodBlockDesired(it->blockX, it->blockY) ||
             Q1990FindLodBlock(it->blockX, it->blockY));
        const bool authoredDrop =
            it->q2023AuthoredAsset &&
            it->q2023AssetIndex < gQ2023LodAssets.size() &&
            gQ2023LodAssets[it->q2023AssetIndex].loaded;
        if (level4Drop || authoredDrop) {
            if (it->q2023AuthoredAsset &&
                it->q2023AssetIndex < gQ2023LodAssets.size()) {
                gQ2023LodAssets[it->q2023AssetIndex].pending = false;
            }
            it = gQ2013LodUploadsQ19.erase(it);
        } else {
            ++it;
        }
    }
    if (gQ2013LodUploadsQ19.empty()) return;

    // Always publish the currently-nearest completed block first rather than
    // preserving worker-completion FIFO order.
    auto best = std::min_element(
        gQ2013LodUploadsQ19.begin(), gQ2013LodUploadsQ19.end(),
        [](const Q1970LodUploadTaskQ2013& a,
           const Q1970LodUploadTaskQ2013& b) {
            auto priority = [](const Q1970LodUploadTaskQ2013& item) {
                // Q20.24: secure the immediate Level4 ring, establish the
                // authored horizon/landmarks, then fill refinement and the
                // farther object-LOD range.
                if (!item.q2023AuthoredAsset) {
                    if (item.ring <= 1) return 0;
                    if (item.ring <= 2) return 3;
                    return 6;
                }
                if (item.q2023LevelCells == 32) return 1;
                if (item.q2023Kind == Q2023LodAssetKind::HighObjects) return 2;
                if (item.q2023LevelCells == 16) return 4;
                return 5;
            };
            const int ap = priority(a);
            const int bp = priority(b);
            if (ap != bp) return ap < bp;
            const int am = std::abs(a.blockX - gQ1990NativeLodCentreBlockX) +
                           std::abs(a.blockY - gQ1990NativeLodCentreBlockY);
            const int bm = std::abs(b.blockX - gQ1990NativeLodCentreBlockX) +
                           std::abs(b.blockY - gQ1990NativeLodCentreBlockY);
            return am < bm;
        });
    if (best != gQ2013LodUploadsQ19.begin()) {
        std::iter_swap(best, gQ2013LodUploadsQ19.begin());
    }

    const bool q2015Loading = IsFo3LoadingVisibleQ1700();
    const size_t q2015LodGpuBytes =
        q2015Loading ? 8u * 1024u * 1024u : 1024u * 1024u;
    const uint64_t q2015LodGpuBudgetUs =
        q2015Loading ? 6000u : 900u;
    const auto frameStarted = std::chrono::steady_clock::now();
    size_t bytesThisFrame = 0u;

    while (!gQ2013LodUploadsQ19.empty()) {
        Q1970LodUploadTaskQ2013& pending = gQ2013LodUploadsQ19.front();
        const bool level4Drop =
            !pending.q2023AuthoredAsset &&
            (!Q1990LodBlockDesired(pending.blockX, pending.blockY) ||
             Q1990FindLodBlock(pending.blockX, pending.blockY));
        const bool authoredDrop =
            pending.q2023AuthoredAsset &&
            pending.q2023AssetIndex < gQ2023LodAssets.size() &&
            gQ2023LodAssets[pending.q2023AssetIndex].loaded;
        if (level4Drop || authoredDrop) {
            if (pending.q2023AuthoredAsset &&
                pending.q2023AssetIndex < gQ2023LodAssets.size()) {
                gQ2023LodAssets[pending.q2023AssetIndex].pending = false;
            }
            gQ2013LodUploadsQ19.pop_front();
            continue;
        }

        Q1900CellStateQ19& state = *pending.state;
        while (state.textureCursor < state.textures.size()) {
            Q1960AdvanceTextureQ19(state, bytesThisFrame);
            if (bytesThisFrame >= q2015LodGpuBytes ||
                Q1960ElapsedUsQ19(frameStarted) >= q2015LodGpuBudgetUs) {
                return;
            }
            // Q20.14: active means "more chunks remain", not "yield now".
            if (state.q1960TextureUpload.active) continue;
        }
        while (state.gpuCursor < state.cpu.size()) {
            Q1960AdvanceShapeQ19(state, bytesThisFrame);
            if (bytesThisFrame >= q2015LodGpuBytes ||
                Q1960ElapsedUsQ19(frameStarted) >= q2015LodGpuBudgetUs) {
                return;
            }
            if (state.q1960ShapeUpload.active) continue;
        }
        if (state.textureCursor < state.textures.size() ||
            state.gpuCursor < state.cpu.size() ||
            state.q1960TextureUpload.active ||
            state.q1960ShapeUpload.active) return;

        size_t terrainTriangles = 0u;
        size_t objectTriangles = 0u;
        const int32_t readyX = pending.blockX;
        const int32_t readyY = pending.blockY;
        const int readyRing = pending.ring;
        const uint64_t workerUs = pending.workerUs;
        const uint64_t gpuUs = state.gpuUs;
        size_t publishedTerrainShapes = 0u;
        size_t publishedObjectShapes = 0u;

        if (!pending.q2023AuthoredAsset) {
            Q1990NativeLodBlock block;
            block.blockX = pending.blockX;
            block.blockY = pending.blockY;
            block.lastUse = gQ1990NativeLodSerial;
            for (GpuObject& gpu : state.stagedGpu) {
                gpu.q1990NativeLod = true;
                const std::string lower = TextureCacheKey(gpu.modelPath, "");
                const bool objectLod =
                    lower.find("landscape\\lod\\wasteland\\blocks\\") !=
                    std::string::npos;
                if (objectLod) {
                    objectTriangles += static_cast<size_t>(gpu.vertexCount / 3);
                    block.objects.push_back(std::move(gpu));
                } else {
                    gpu.q2024TerrainLod = true;
                    terrainTriangles += static_cast<size_t>(gpu.vertexCount / 3);
                    block.terrain.push_back(std::move(gpu));
                }
                gpu.vbo = 0u;
                gpu.vao = 0u;
            }
            publishedTerrainShapes = block.terrain.size();
            publishedObjectShapes = block.objects.size();
            gQ1990NativeLodBlocks.push_back(std::move(block));
            Q2010AdvanceVisibleNativeLodWindow();
        } else if (pending.q2023Kind == Q2023LodAssetKind::CoarseTerrain) {
            Q2023CoarseTerrainTile tile;
            tile.levelCells = pending.q2023LevelCells;
            tile.blockX = pending.blockX;
            tile.blockY = pending.blockY;
            for (GpuObject& gpu : state.stagedGpu) {
                gpu.q1990NativeLod = true;
                gpu.q2024TerrainLod = true;
                terrainTriangles += static_cast<size_t>(gpu.vertexCount / 3);
                tile.terrain.push_back(std::move(gpu));
                gpu.vbo = 0u;
                gpu.vao = 0u;
            }
            publishedTerrainShapes = tile.terrain.size();
            gQ2023CoarseTerrainTiles.push_back(std::move(tile));
        } else {
            Q2023HighObjectBlock block;
            block.blockX = pending.blockX;
            block.blockY = pending.blockY;
            for (GpuObject& gpu : state.stagedGpu) {
                gpu.q1990NativeLod = true;
                objectTriangles += static_cast<size_t>(gpu.vertexCount / 3);
                block.objects.push_back(std::move(gpu));
                gpu.vbo = 0u;
                gpu.vao = 0u;
            }
            publishedObjectShapes = block.objects.size();
            gQ2023HighObjectBlocks.push_back(std::move(block));
        }
        state.stagedGpu.clear();

        if (pending.q2023AuthoredAsset &&
            pending.q2023AssetIndex < gQ2023LodAssets.size()) {
            gQ2023LodAssets[pending.q2023AssetIndex].loaded = true;
            gQ2023LodAssets[pending.q2023AssetIndex].pending = false;
        }

        const bool publishedAuthored = pending.q2023AuthoredAsset;
        const int publishedLevel = pending.q2023LevelCells;
        const bool publishedHigh =
            pending.q2023AuthoredAsset &&
            pending.q2023Kind == Q2023LodAssetKind::HighObjects;
        gQ2013LodUploadsQ19.pop_front();
        gQ1990NativeLodDrawLogged = false;

        Q6H_LOGI("Q20.23 LOD BLOCK READY: block=(%d,%d) ring=%d authored=%d level=%d high=%d terrainShapes=%zu terrainTriangles=%zu objectShapes=%zu objectTriangles=%zu workerUs=%llu gpuUs=%llu remainingUploads=%zu level4VisibleBlocks=%zu coarseTiles=%zu highBlocks=%zu",
                 readyX, readyY, readyRing,
                 publishedAuthored ? 1 : 0, publishedLevel,
                 publishedHigh ? 1 : 0,
                 publishedTerrainShapes, terrainTriangles,
                 publishedObjectShapes, objectTriangles,
                 static_cast<unsigned long long>(workerUs),
                 static_cast<unsigned long long>(gpuUs),
                 gQ2013LodUploadsQ19.size(),
                 Q2013ProgressiveLodCountQ19(),
                 gQ2023CoarseTerrainTiles.size(),
                 gQ2023HighObjectBlocks.size());

        if (bytesThisFrame >= q2015LodGpuBytes ||
            Q1960ElapsedUsQ19(frameStarted) >= q2015LodGpuBudgetUs) return;
    }
}

bool Q1970ChooseMissingLodQ19(
        int32_t& nextBlockX, int32_t& nextBlockY, int& bestRing) {
    int bestManhattan = 1000000;
    bestRing = 1000000;
    bool foundMissing = false;
    for (int dy = -Q2024_OBJECT_LOD_BLOCK_RADIUS;
         dy <= Q2024_OBJECT_LOD_BLOCK_RADIUS; ++dy) {
        for (int dx = -Q2024_OBJECT_LOD_BLOCK_RADIUS;
             dx <= Q2024_OBJECT_LOD_BLOCK_RADIUS; ++dx) {
            const int32_t bx =
                gQ1990NativeLodCentreBlockX +
                dx * Q1840_LEVEL4_BLOCK_CELLS;
            const int32_t by =
                gQ1990NativeLodCentreBlockY +
                dy * Q1840_LEVEL4_BLOCK_CELLS;
            if (Q1990FindLodBlock(bx, by) ||
                Q1970LodBlockPendingQ19(bx, by)) continue;
            const int ring = std::max(std::abs(dx), std::abs(dy));
            const int manhattan = std::abs(dx) + std::abs(dy);
            if (!foundMissing || ring < bestRing ||
                (ring == bestRing && manhattan < bestManhattan)) {
                foundMissing = true;
                bestRing = ring;
                bestManhattan = manhattan;
                nextBlockX = bx;
                nextBlockY = by;
            }
        }
    }
    return foundMissing;
}

bool Q2023ChooseMissingAuthoredLodQ19(
        int32_t playerCellX, int32_t playerCellY,
        size_t& outAssetIndex) {
    if (!Q2023DiscoverLodAssetsQ19()) return false;

    bool found = false;
    int bestGroup = 100;
    int bestDistance = 1000000;
    for (size_t i = 0u; i < gQ2023LodAssets.size(); ++i) {
        Q2023LodAsset& asset = gQ2023LodAssets[i];
        if (asset.loaded || asset.pending ||
            !Q2023AssetDesiredQ19(asset, playerCellX, playerCellY)) {
            continue;
        }

        int group = 4;
        if (asset.kind == Q2023LodAssetKind::HighObjects) group = 1;
        else if (asset.levelCells == 32) group = 0;
        else if (asset.levelCells == 16) group = 2;
        else if (asset.levelCells == 8) group = 3;

        const int32_t centreX = asset.blockX + asset.levelCells / 2;
        const int32_t centreY = asset.blockY + asset.levelCells / 2;
        const int distance = std::max(
            std::abs(centreX - playerCellX),
            std::abs(centreY - playerCellY));
        if (!found || group < bestGroup ||
            (group == bestGroup && distance < bestDistance)) {
            found = true;
            bestGroup = group;
            bestDistance = distance;
            outAssetIndex = i;
        }
    }
    return found;
}

void Q1970AdvanceNativeLodQ19(int32_t cellX, int32_t cellY,
                              float centerX, float centerY, float floorZ) {
    // Q20.23A: persistent gate telemetry. Previous LOD diagnostics were mostly
    // transition/change driven, so by the time logcat was inspected the useful
    // lines could already have rolled out of the buffer.
    static uint64_t q2023aGatePulse = 0u;
    ++q2023aGatePulse;
    const bool q2023aPulse =
        q2023aGatePulse == 1u || (q2023aGatePulse % 120u) == 0u;

    size_t q2023aNearReady = 0u;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto found = gQ1900CellsQ19.find(
                Q1900CellKeyQ19(cellX + dx, cellY + dy));
            if (found != gQ1900CellsQ19.end() &&
                found->second.visualReady) {
                ++q2023aNearReady;
            }
        }
    }
    const bool q2023aNearSafe = q2023aNearReady == 9u;
    const bool q2023aTerrainBusy =
        IsFo3TerrainStreamingCpuBusyQ2000();
    const bool q2023aCollisionBusy =
        static_cast<bool>(gQ1930CollisionTaskQ19);
    size_t q2023aBootstrapReady = 0u;
    const bool q2023aBootstrapComplete =
        Q2013NativeLodBootstrapReadyQ19(
            cellX, cellY, &q2023aBootstrapReady);
    size_t q2023bLevel8 = 0u;
    size_t q2023bLevel16 = 0u;
    size_t q2023bLevel32 = 0u;
    size_t q2024FarObjectBlocks = 0u;
    size_t q2025TerrainShapes = 0u;
    size_t q2025TerrainRealDiffuse = 0u;
    size_t q2025TerrainVertexColor = 0u;
    for (const Q2023CoarseTerrainTile& tile : gQ2023CoarseTerrainTiles) {
        if (tile.levelCells == 8) ++q2023bLevel8;
        else if (tile.levelCells == 16) ++q2023bLevel16;
        else if (tile.levelCells == 32) ++q2023bLevel32;
    }
    for (const Q1990NativeLodBlock& block : gQ1990NativeLodBlocks) {
        if (!Q1990TerrainLodBlockDesired(block.blockX, block.blockY) &&
            !block.objects.empty()) {
            ++q2024FarObjectBlocks;
        }
        for (const GpuObject& object : block.terrain) {
            ++q2025TerrainShapes;
            if (object.realDiffuse) ++q2025TerrainRealDiffuse;
            if (object.useVertexColor) ++q2025TerrainVertexColor;
        }
    }
    for (const Q2023CoarseTerrainTile& tile : gQ2023CoarseTerrainTiles) {
        for (const GpuObject& object : tile.terrain) {
            ++q2025TerrainShapes;
            if (object.realDiffuse) ++q2025TerrainRealDiffuse;
            if (object.useVertexColor) ++q2025TerrainVertexColor;
        }
    }

    if (q2023aPulse) {
        Q6H_LOGI("Q20.25 LOD STATE: pulse=%llu cell=(%d,%d) contextReady=%d wasteland=%d collisionBusy=%d terrainBusy=%d near3x3Ready=%zu/9 nearSafe=%d bootstrapLevel4=%zu/25 bootstrapComplete=%d level4DesiredLoaded=%zu level8=%zu level16=%zu level32=%zu coarseTiles=%zu highBlocks=%zu farObjectBlocks=%zu terrainShapes=%zu terrainRealDiffuse=%zu terrainFallbackDiffuse=%zu terrainVertexColorStreams=%zu activeWorkers=%zu uploads=%zu assetsDiscovered=%d assets=%zu",
                 static_cast<unsigned long long>(q2023aGatePulse),
                 cellX, cellY,
                 gQ1900ContextReadyQ19 ? 1 : 0,
                 gExteriorWorldspaceQ1890 == 0x0000003Cu ? 1 : 0,
                 q2023aCollisionBusy ? 1 : 0,
                 q2023aTerrainBusy ? 1 : 0,
                 q2023aNearReady,
                 q2023aNearSafe ? 1 : 0,
                 q2023aBootstrapReady,
                 q2023aBootstrapComplete ? 1 : 0,
                 Q2013ProgressiveLodCountQ19(),
                 q2023bLevel8,
                 q2023bLevel16,
                 q2023bLevel32,
                 gQ2023CoarseTerrainTiles.size(),
                 gQ2023HighObjectBlocks.size(),
                 q2024FarObjectBlocks,
                 q2025TerrainShapes,
                 q2025TerrainRealDiffuse,
                 q2025TerrainShapes - q2025TerrainRealDiffuse,
                 q2025TerrainVertexColor,
                 Q2013ActiveLodWorkersQ19(),
                 gQ2013LodUploadsQ19.size(),
                 gQ2023LodAssetsDiscovered ? 1 : 0,
                 gQ2023LodAssets.size());
    }

    if (!gQ1900ContextReadyQ19 ||
        gExteriorWorldspaceQ1890 != 0x0000003Cu) return;

    Q1970ConsumeLodWorkersQ19();
    Q1970AdvanceLodGpuQ19();

    // Collision and terrain CPU work keep exclusive access to their mutable
    // caches; Level4 NIF work may coexist with detailed CELL workers.
    if (q2023aCollisionBusy ||
        q2023aTerrainBusy ||
        !q2023aNearSafe) return;

    if (gQ2013LodUploadsQ19.size() >= Q2014_LOD_UPLOAD_BACKLOG_LIMIT) {
        return;
    }

    for (size_t slot = 0u; slot < Q2013_LOD_WORKER_SLOTS; ++slot) {
        if (gQ2013LodWorkersQ19[slot]) continue;
        if (gQ2013LodUploadsQ19.size() + Q2013ActiveLodWorkersQ19() >=
            Q2014_LOD_UPLOAD_BACKLOG_LIMIT) {
            break;
        }

        size_t bootstrapReady = 0u;
        const bool bootstrapComplete =
            Q2013NativeLodBootstrapReadyQ19(
                cellX, cellY, &bootstrapReady);
        const bool authoredAllowed = bootstrapReady >= 9u;
        const bool q2024Loading = IsFo3LoadingVisibleQ1700();
        const bool preferAuthored =
            authoredAllowed &&
            (q2024Loading ? slot != 0u : ((slot & 1u) != 0u));

        int32_t blockX = 0;
        int32_t blockY = 0;
        int ring = 0;
        size_t authoredIndex = static_cast<size_t>(-1);
        bool authored = false;

        if (preferAuthored) {
            authored = Q2023ChooseMissingAuthoredLodQ19(
                cellX, cellY, authoredIndex);
        }
        if (!authored &&
            !Q1970ChooseMissingLodQ19(blockX, blockY, ring)) {
            authored = authoredAllowed &&
                Q2023ChooseMissingAuthoredLodQ19(
                    cellX, cellY, authoredIndex);
            if (!authored) break;
        }

        auto task = std::make_shared<Q1970LodWorkerTaskQ19>();
        task->contextSerial = gQ1900ContextSerialQ19;
        task->centerX = centerX;
        task->centerY = centerY;
        task->floorZ = floorZ;

        if (authored) {
            Q2023LodAsset& asset = gQ2023LodAssets[authoredIndex];
            asset.pending = true;
            task->q2023AuthoredAsset = true;
            task->q2023AssetIndex = authoredIndex;
            task->q2023Kind = asset.kind;
            task->q2023LevelCells = asset.levelCells;
            task->q2023Path = asset.path;
            task->blockX = asset.blockX;
            task->blockY = asset.blockY;
            const int32_t centreX =
                asset.blockX + asset.levelCells / 2;
            const int32_t centreY =
                asset.blockY + asset.levelCells / 2;
            task->ring = std::max(
                std::abs(centreX - cellX),
                std::abs(centreY - cellY));
        } else {
            task->blockX = blockX;
            task->blockY = blockY;
            task->ring = ring;
            task->q2023LevelCells = 4;
        }

        gQ2013LodWorkersQ19[slot] = task;

        Q6H_LOGI("Q20.23 LOD CPU START: slot=%zu/%zu block=(%d,%d) ring=%d authored=%d level=%d high=%d activeWorkers=%zu uploadBacklog=%zu/%zu bootstrap=%zu/25",
                 slot + 1u, Q2013_LOD_WORKER_SLOTS,
                 task->blockX, task->blockY, task->ring,
                 task->q2023AuthoredAsset ? 1 : 0,
                 task->q2023LevelCells,
                 task->q2023AuthoredAsset &&
                     task->q2023Kind == Q2023LodAssetKind::HighObjects ? 1 : 0,
                 Q2013ActiveLodWorkersQ19(),
                 gQ2013LodUploadsQ19.size(),
                 Q2014_LOD_UPLOAD_BACKLOG_LIMIT,
                 bootstrapReady);
        std::thread([task]() { Q1970RunLodWorkerQ19(task); }).detach();
    }
}

void Q1990RenderNativeLod(bool alphaPass) {
    if (gExteriorWorldspaceQ1890 != 0x0000003Cu) return;
    if (gQ1990NativeLodBlocks.empty() &&
        gQ2023CoarseTerrainTiles.empty() &&
        gQ2023HighObjectBlocks.empty()) return;

    const int32_t playerCellX =
        gQ1920LatestGridValid
            ? gQ1920LatestGridX
            : gExteriorWindowGridXQ1890;
    const int32_t playerCellY =
        gQ1920LatestGridValid
            ? gQ1920LatestGridY
            : gExteriorWindowGridYQ1890;

    size_t drawnShapes = 0u;
    size_t drawnTriangles = 0u;
    size_t drawnLevel8 = 0u;
    size_t drawnLevel16 = 0u;
    size_t drawnLevel32 = 0u;
    size_t drawnHigh = 0u;

    glEnable(GL_POLYGON_OFFSET_FILL);

    // Q20.23: draw the coarsest authored fallback first. A parent tile remains
    // visible until all four desired children are resident, so streaming never
    // punches holes into the horizon. Finer tiers use progressively smaller
    // polygon offsets and naturally win the depth test at overlap seams.
    const int levels[] = {32, 16, 8};
    for (int level : levels) {
        if (level == 32) glPolygonOffset(8.0f, 14.0f);
        else if (level == 16) glPolygonOffset(6.0f, 11.0f);
        else glPolygonOffset(4.0f, 8.0f);

        for (Q2023CoarseTerrainTile& tile : gQ2023CoarseTerrainTiles) {
            if (tile.levelCells != level ||
                !Q2023TileDesiredQ19(
                    tile.levelCells, tile.blockX, tile.blockY,
                    playerCellX, playerCellY)) {
                continue;
            }
            if (Q2023TileFullyRefinedQ19(
                    tile.levelCells, tile.blockX, tile.blockY,
                    playerCellX, playerCellY)) {
                continue;
            }
            for (const GpuObject& object : tile.terrain) {
                if (object.alphaBlend != alphaPass) continue;
                DrawSceneObject(object);
                ++drawnShapes;
                drawnTriangles += static_cast<size_t>(object.vertexCount / 3);
                if (level == 8) ++drawnLevel8;
                else if (level == 16) ++drawnLevel16;
                else ++drawnLevel32;
            }
        }
    }

    glPolygonOffset(2.0f, 6.0f);
    for (Q1990NativeLodBlock& block : gQ1990NativeLodBlocks) {
        if (!Q1990LodBlockDesired(block.blockX, block.blockY)) continue;
        if (Q1990TerrainLodBlockDesired(block.blockX, block.blockY) &&
            (block.blockX != gQ1990NativeLodCentreBlockX ||
             block.blockY != gQ1990NativeLodCentreBlockY)) {
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

    // The sparse .High VWD set is the long-distance landmark layer. Suppress a
    // high block whenever its normal Level4 object block is resident inside the
    // normal 20-cell horizon; outside that horizon the high mesh remains.
    for (Q2023HighObjectBlock& high : gQ2023HighObjectBlocks) {
        Q1990NativeLodBlock* normal =
            Q1990FindLodBlock(high.blockX, high.blockY);
        if (normal && !normal->objects.empty() &&
            Q1990LodBlockDesired(high.blockX, high.blockY)) {
            continue;
        }
        for (const GpuObject& object : high.objects) {
            if (object.alphaBlend != alphaPass) continue;
            DrawSceneObject(object);
            ++drawnShapes;
            ++drawnHigh;
            drawnTriangles += static_cast<size_t>(object.vertexCount / 3);
        }
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    if (!alphaPass && !gQ1990NativeLodDrawLogged) {
        gQ1990NativeLodDrawLogged = true;
        Q6H_LOGI("Q20.23 NATIVE LOD DRAW: shapes=%zu triangles=%zu level32Shapes=%zu level16Shapes=%zu level8Shapes=%zu highShapes=%zu level4Loaded=%zu coarseTiles=%zu highBlocks=%zu playerCell=(%d,%d) hierarchy=32>16>8>4>detail refinement=no-holes treesDTL=deferred postApocalypse=deferred",
                 drawnShapes, drawnTriangles,
                 drawnLevel32, drawnLevel16, drawnLevel8, drawnHigh,
                 Q2013ProgressiveLodCountQ19(),
                 gQ2023CoarseTerrainTiles.size(),
                 gQ2023HighObjectBlocks.size(),
                 playerCellX, playerCellY);
    }
}

void SetFo3WaterSkyMvpQ2090(const float* skyMvp16) {
    if (!skyMvp16) {
        gWaterSkyMvpReadyQ2090 = false;
        return;
    }
    std::memcpy(gWaterSkyMvpQ2090, skyMvp16, 16u * sizeof(float));
    gWaterSkyMvpReadyQ2090 = true;
}

void Q2090MultiplyMatrix(const float a[16], const float b[16], float out[16]) {
    float result[16]{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k) {
                value += a[k * 4 + row] * b[column * 4 + k];
            }
            result[column * 4 + row] = value;
        }
    }
    std::memcpy(out, result, sizeof(result));
}

void Q2090BuildReflectionMatrix(float planeY, float out[16]) {
    // Column-vector OpenGL matrix for y' = 2h - y.
    const float reflection[16]{
        1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f, 2.0f * planeY, 0.0f, 1.0f
    };
    std::memcpy(out, reflection, sizeof(reflection));
}

bool Q2090EnsureReflectionTarget() {
    if (gWaterReflectionTargetReadyQ2090 &&
        gWaterReflectionFboQ2090 && gWaterReflectionColorQ2090 &&
        gWaterReflectionDepthQ2090) {
        return true;
    }

    if (!gWaterReflectionFboQ2090) glGenFramebuffers(1, &gWaterReflectionFboQ2090);
    if (!gWaterReflectionColorQ2090) glGenTextures(1, &gWaterReflectionColorQ2090);
    if (!gWaterReflectionDepthQ2090) glGenRenderbuffers(1, &gWaterReflectionDepthQ2090);

    GLint previousFbo = 0;
    GLint previousTexture = 0;
    GLint previousRenderbuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);

    glBindTexture(GL_TEXTURE_2D, gWaterReflectionColorQ2090);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLenum chosenFormat = GL_RGBA16F;
    glTexImage2D(GL_TEXTURE_2D, 0, chosenFormat,
                 Q2090_REFLECTION_SIZE, Q2090_REFLECTION_SIZE, 0,
                 GL_RGBA, GL_HALF_FLOAT, nullptr);

    glBindRenderbuffer(GL_RENDERBUFFER, gWaterReflectionDepthQ2090);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          Q2090_REFLECTION_SIZE, Q2090_REFLECTION_SIZE);

    glBindFramebuffer(GL_FRAMEBUFFER, gWaterReflectionFboQ2090);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gWaterReflectionColorQ2090, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, gWaterReflectionDepthQ2090);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        chosenFormat = GL_RGBA8;
        glBindTexture(GL_TEXTURE_2D, gWaterReflectionColorQ2090);
        glTexImage2D(GL_TEXTURE_2D, 0, chosenFormat,
                     Q2090_REFLECTION_SIZE, Q2090_REFLECTION_SIZE, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, gWaterReflectionFboQ2090);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, gWaterReflectionColorQ2090, 0);
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    }

    gWaterReflectionTargetReadyQ2090 =
        status == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(previousRenderbuffer));

    if (!gWaterReflectionTargetLoggedQ2090) {
        gWaterReflectionTargetLoggedQ2090 = true;
        if (gWaterReflectionTargetReadyQ2090) {
            Q6H_LOGI("Q20.9 WATER MIRROR TARGET READY: size=1024x1024 color=%s depth=DEPTH_COMPONENT24 source=FalloutPrefs iWaterReflectWidth/Height=1024 blur=pending",
                     chosenFormat == GL_RGBA16F ? "RGBA16F" : "RGBA8");
        } else {
            Q6H_LOGW("Q20.9 WATER MIRROR TARGET FAILED: size=1024x1024 status=0x%X fallback=WATER001-authored-reflection",
                     status);
        }
    }
    return gWaterReflectionTargetReadyQ2090;
}

bool Q2090AnyLoadedWaterAboveLand() {
    const auto& waters = GetFo3WaterCellsQ2070();
    if (waters.empty()) return false;
    const auto& terrain = GetFo3TerrainQ76();

    for (const Fo3WaterCellQ2070& water : waters) {
        const Fo3TerrainCellQ76* matchingLand = nullptr;
        for (const Fo3TerrainCellQ76& land : terrain) {
            if (land.cellFormId == water.cellFormId ||
                (land.gridX == water.gridX && land.gridY == water.gridY)) {
                matchingLand = &land;
                break;
            }
        }

        // A water CELL without local LAND can still expose water around
        // placed geometry, so do not suppress it merely because LAND is absent.
        if (!matchingLand || matchingLand->heights.empty()) return true;

        const float minLand =
            *std::min_element(matchingLand->heights.begin(),
                              matchingLand->heights.end());
        if (minLand < water.waterHeightGame) return true;
    }
    return false;
}

bool Q2090RenderWaterReflection(const float mainMvp[16],
                                float planeY,
                                float outReflectionMvp[16]) {
    const auto q2019ReflectionStarted =
        std::chrono::steady_clock::now();
    uint64_t q2019OpaqueUs = 0u;
    uint64_t q2019AlphaUs = 0u;
    uint64_t q2019EnvUs = 0u;
    uint64_t q2019LandUs = 0u;

    if (!mainMvp || !outReflectionMvp ||
        !gWaterSkyMvpReadyQ2090 ||
        !Q2090EnsureReflectionTarget()) {
        return false;
    }

    // Above-water WATER000 only. Underwater optics are a distinct renderer
    // state and remain a later milestone.
    if (gFo3EyePositionQ1010[1] <= planeY + 0.001f) return false;

    float reflection[16]{};
    Q2090BuildReflectionMatrix(planeY, reflection);
    Q2090MultiplyMatrix(mainMvp, reflection, outReflectionMvp);

    float skyFlip[16]{
        1.0f,  0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f,  0.0f, 1.0f, 0.0f,
        0.0f,  0.0f, 0.0f, 1.0f
    };
    float reflectedSkyMvp[16]{};
    Q2090MultiplyMatrix(gWaterSkyMvpQ2090, skyFlip, reflectedSkyMvp);

    GLint previousFbo = 0;
    GLint previousViewport[4]{};
    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousDepthFunc = GL_LESS;
    GLint previousBlendSrcRgb = GL_ONE, previousBlendDstRgb = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE, previousBlendDstAlpha = GL_ZERO;
    GLfloat previousClearColor[4]{};
    GLboolean previousDepthMask = GL_TRUE;
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor);

    const float originalEye[3]{
        gFo3EyePositionQ1010[0],
        gFo3EyePositionQ1010[1],
        gFo3EyePositionQ1010[2]
    };
    const float mirroredEye[3]{
        originalEye[0],
        2.0f * planeY - originalEye[1],
        originalEye[2]
    };

    glBindFramebuffer(GL_FRAMEBUFFER, gWaterReflectionFboQ2090);
    glViewport(0, 0, Q2090_REFLECTION_SIZE, Q2090_REFLECTION_SIZE);

    const Fo3EnvironmentQ1000& env = GetFo3EnvironmentQ1000();
    if (env.valid) {
        glClearColor(env.horizon[0], env.horizon[1], env.horizon[2], 1.0f);
    } else {
        glClearColor(0.18f, 0.18f, 0.14f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gWaterReflectionPassQ2090 = true;
    gWaterReflectionPlaneYQ2090 = planeY;
    gFo3EyePositionQ1010[0] = mirroredEye[0];
    gFo3EyePositionQ1010[1] = mirroredEye[1];
    gFo3EyePositionQ1010[2] = mirroredEye[2];

    // PC sky is a direction-space background, so reflect only Y direction;
    // the water-plane translation belongs to world geometry, not the dome.
    RenderFo3PcSkyQ1660(reflectedSkyMvp);

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, outReflectionMvp);
    if (gWaterReflectionClipEnabledLocationQ2090 >= 0) {
        glUniform1f(gWaterReflectionClipEnabledLocationQ2090, 1.0f);
    }
    if (gWaterReflectionPlaneYLocationQ2090 >= 0) {
        glUniform1f(gWaterReflectionPlaneYLocationQ2090, planeY);
    }
    if (gEyePositionLocationQ1010 >= 0)
        glUniform3fv(gEyePositionLocationQ1010, 1, mirroredEye);
    if (gEyePositionVertexLocationQ1630 >= 0)
        glUniform3fv(gEyePositionVertexLocationQ1630, 1, mirroredEye);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Q20.20: Q20.19 correctly instanced the mirror, but DrawSceneObject still
    // bypassed Q20.15 culling whenever gWaterReflectionPassQ2090 was true.
    // Install the reflected MVP as a nested frustum so native LOD, detailed
    // statics, alpha and environment passes only submit geometry the mirror
    // camera can actually see.
    Q2015FrustumCullScope q2020ReflectionFrustum(outReflectionMvp);

    const auto q2019ReflectionOpaqueStarted =
        std::chrono::steady_clock::now();
    const GLboolean q2021ReflectionA2cWas =
        glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
    if (q2060MsaaActive && q2060MsaaSamples > 1)
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    Q1990RenderNativeLod(false);
    // Q20.19: the planar mirror previously bypassed Q20.17 and submitted every
    // opaque detailed object one-by-one. Reuse the exact same shared-geometry
    // instancing path as the main eye. DrawSceneObject already flips winding
    // while gWaterReflectionPassQ2090 is true, so reflection semantics remain
    // unchanged while repeated STAT/SCOL/TREE geometry collapses to batches.
    Q2017RenderOpaqueDetailedInstanced();
    if (!q2021ReflectionA2cWas)
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    q2019OpaqueUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2019ReflectionOpaqueStarted).count());

    const auto q2019ReflectionAlphaStarted =
        std::chrono::steady_clock::now();
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

    q2019AlphaUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2019ReflectionAlphaStarted).count());

    const auto q2019ReflectionEnvStarted =
        std::chrono::steady_clock::now();
    // Preserve Q20.5 material reflections inside the planar reflection image.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    if (gEnvironmentPassEnabledQ205A) {
        for (const GpuObject& object : gObjects) {
            if (!object.environmentEnabledQ2050 ||
                !object.zBufferWriteQ1200) continue;
            DrawSceneObject(object, true);
        }
    }

    q2019EnvUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2019ReflectionEnvStarted).count());

    const auto q2019ReflectionLandStarted =
        std::chrono::steady_clock::now();
    // LAND uses its own program, so arm the same water-plane clip there.
    SetFo3TerrainWaterReflectionClipQ2090(true, planeY);
    RenderFo3CollisionOverlay(outReflectionMvp);
    SetFo3TerrainWaterReflectionClipQ2090(false, planeY);
    q2019LandUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2019ReflectionLandStarted).count());

    // Restore main-eye global/uniform state before the WATER000 draw.
    gFo3EyePositionQ1010[0] = originalEye[0];
    gFo3EyePositionQ1010[1] = originalEye[1];
    gFo3EyePositionQ1010[2] = originalEye[2];
    glUseProgram(gProgram);
    if (gWaterReflectionClipEnabledLocationQ2090 >= 0)
        glUniform1f(gWaterReflectionClipEnabledLocationQ2090, 0.0f);
    if (gEyePositionLocationQ1010 >= 0)
        glUniform3fv(gEyePositionLocationQ1010, 1, originalEye);
    if (gEyePositionVertexLocationQ1630 >= 0)
        glUniform3fv(gEyePositionVertexLocationQ1630, 1, originalEye);
    gWaterReflectionPassQ2090 = false;

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
    glViewport(previousViewport[0], previousViewport[1],
               previousViewport[2], previousViewport[3]);
    glClearColor(previousClearColor[0], previousClearColor[1],
                 previousClearColor[2], previousClearColor[3]);
    glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    glDepthMask(previousDepthMask);
    glBlendFuncSeparate(static_cast<GLenum>(previousBlendSrcRgb),
                        static_cast<GLenum>(previousBlendDstRgb),
                        static_cast<GLenum>(previousBlendSrcAlpha),
                        static_cast<GLenum>(previousBlendDstAlpha));
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));

    ++gWaterReflectionFramesQ2090;
    const uint64_t q2019ReflectionTotalUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2019ReflectionStarted).count());
    if (gWaterReflectionFramesQ2090 == 1u ||
        (gWaterReflectionFramesQ2090 % 120u) == 0u ||
        q2019ReflectionTotalUs >= 35000u) {
        Q6H_LOGI("Q20.19 WATER REFLECTION PHASES: totalUs=%llu opaqueUs=%llu alphaUs=%llu envUs=%llu landUs=%llu objects=%zu sharedMeshes=%zu target=1024x1024 instancedOpaque=1",
                 static_cast<unsigned long long>(q2019ReflectionTotalUs),
                 static_cast<unsigned long long>(q2019OpaqueUs),
                 static_cast<unsigned long long>(q2019AlphaUs),
                 static_cast<unsigned long long>(q2019EnvUs),
                 static_cast<unsigned long long>(q2019LandUs),
                 gObjects.size(), gQ2017SharedGeometry.size());
    }
    if (gWaterReflectionFramesQ2090 == 1u ||
        (gWaterReflectionFramesQ2090 % 600u) == 0u) {
        Q6H_LOGI("Q20.9 WATER MIRROR DRAW: frame=%llu planeY=%.5f eyeMain=(%.4f %.4f %.4f) eyeMirror=(%.4f %.4f %.4f) size=1024x1024 sky=PC-Q16.6 statics=%zu LAND=1 environmentPass=%d belowPlaneClip=1 stereoPerEye=1 blur=pending",
                 static_cast<unsigned long long>(gWaterReflectionFramesQ2090),
                 planeY,
                 originalEye[0], originalEye[1], originalEye[2],
                 mirroredEye[0], mirroredEye[1], mirroredEye[2],
                 gObjects.size(),
                 gEnvironmentPassEnabledQ205A ? 1 : 0);
    }
    return true;
}

struct Q1970RenderStallScopeQ19 {
    std::chrono::steady_clock::time_point started =
        std::chrono::steady_clock::now();
    ~Q1970RenderStallScopeQ19() {
        const uint64_t us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started).count());
        if (us >= 50000u) {
            Q6H_LOGW("Q19.7 RENDER STALL: renderUs=%llu thresholdUs=50000 sceneObjects=%zu lodBlocks=%zu",
                     static_cast<unsigned long long>(us),
                     gObjects.size(), gQ1990NativeLodBlocks.size());
        }
    }
};


bool Q210EndsWithInsensitive(std::string value, std::string suffix) {
    for (char& ch : value) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    for (char& ch : suffix) {
        if (ch == '/') ch = '\\';
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value.size() >= suffix.size() &&
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}


Vec3 Q211Add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 Q211Sub(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 Q211Mul(Vec3 v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}
float Q211Dot(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
Vec3 Q211Cross(Vec3 a, Vec3 b) {
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}
float Q211Length(Vec3 v) {
    return std::sqrt(std::max(0.0f, Q211Dot(v, v)));
}
Vec3 Q211NormalizeSafe(Vec3 v, Vec3 fallback = {1.0f, 0.0f, 0.0f}) {
    const float len = Q211Length(v);
    return len > 1.0e-6f ? Q211Mul(v, 1.0f / len) : fallback;
}

bool Q220IsLooseRecordType(const std::string& type) {
    return type == "MISC" ||
           type == "WEAP" ||
           type == "ARMO" ||
           type == "AMMO" ||
           type == "ALCH" ||
           type == "BOOK" ||
           type == "KEYM";
}

void Q220Identity(float out[16]) {
    std::fill(out, out + 16, 0.0f);
    out[0] = out[5] = out[10] = out[15] = 1.0f;
}

void Q220MulMat4(const float a[16], const float b[16], float out[16]) {
    float r[16]{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r[col*4 + row] =
                a[0*4 + row] * b[col*4 + 0] +
                a[1*4 + row] * b[col*4 + 1] +
                a[2*4 + row] * b[col*4 + 2] +
                a[3*4 + row] * b[col*4 + 3];
        }
    }
    std::copy(r, r + 16, out);
}

Vec3 Q220TransformPoint(const float m[16], Vec3 p) {
    return {
        m[0]*p.x + m[4]*p.y + m[8]*p.z + m[12],
        m[1]*p.x + m[5]*p.y + m[9]*p.z + m[13],
        m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]
    };
}

void Q220ControllerDeltaMatrix(
        Vec3 startPos, const float startQ[4],
        Vec3 currentPos, const float currentQ[4],
        float out[16]) {
    const float sx = -startQ[0], sy = -startQ[1],
                sz = -startQ[2], sw = startQ[3];
    const float cx = currentQ[0], cy = currentQ[1],
                cz = currentQ[2], cw = currentQ[3];

    float x = cw*sx + cx*sw + cy*sz - cz*sy;
    float y = cw*sy - cx*sz + cy*sw + cz*sx;
    float z = cw*sz + cx*sy - cy*sx + cz*sw;
    float w = cw*sw - cx*sx - cy*sy - cz*sz;
    const float len = std::sqrt(x*x + y*y + z*z + w*w);
    if (len > 1.0e-6f) {
        x /= len; y /= len; z /= len; w /= len;
    } else {
        x = y = z = 0.0f; w = 1.0f;
    }

    Q220Identity(out);
    out[0] = 1.0f - 2.0f*(y*y + z*z);
    out[1] = 2.0f*(x*y + z*w);
    out[2] = 2.0f*(x*z - y*w);
    out[4] = 2.0f*(x*y - z*w);
    out[5] = 1.0f - 2.0f*(x*x + z*z);
    out[6] = 2.0f*(y*z + x*w);
    out[8] = 2.0f*(x*z + y*w);
    out[9] = 2.0f*(y*z - x*w);
    out[10] = 1.0f - 2.0f*(x*x + y*y);

    const Vec3 rotatedStart{
        out[0]*startPos.x + out[4]*startPos.y + out[8]*startPos.z,
        out[1]*startPos.x + out[5]*startPos.y + out[9]*startPos.z,
        out[2]*startPos.x + out[6]*startPos.y + out[10]*startPos.z};
    out[12] = currentPos.x - rotatedStart.x;
    out[13] = currentPos.y - rotatedStart.y;
    out[14] = currentPos.z - rotatedStart.z;
}

struct Q223DynamicBody {
    uint32_t refFormId = 0u;
    bool initialized = false;
    bool held = false;
    bool dynamic = false;
    Vec3 authoredCenter{};
    float collisionRadius = 0.0f;
    Vec3 collisionHalfExtents{};
    bool sleeping = false;
    float sleepTimer = 0.0f;
    int motionSamples = 0;
    Vec3 linearVelocity{};
    Vec3 angularVelocity{};
    Vec3 lastPalm{};
    float lastPalmQuat[4]{0,0,0,1};
    Vec3 sampledLinearVelocity{};
    Vec3 sampledAngularVelocity{};
    std::chrono::steady_clock::time_point lastPalmSample{};
    std::chrono::steady_clock::time_point lastPhysicsStep{};
    std::string modelPath;
};

std::unordered_map<uint32_t, Q223DynamicBody> gQ223DynamicBodies;
std::unordered_map<std::string, float> gQ223CollisionRadiusUnitsCache;

struct Q220GrabState {
    bool active = false;
    uint32_t refFormId = 0u;
    Vec3 startPalm{};
    float startQuat[4]{0,0,0,1};
    float baseTransform[16]{
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    float previousGrip = 0.0f;
};

Q220GrabState gQ220Grab[2];

void Q220ResetGrabState() {
    gQ220Grab[0] = {};
    gQ220Grab[1] = {};
    gQ223DynamicBodies.clear();
}

void Q220SetRefTransform(uint32_t refFormId, const float transform[16]) {
    for (GpuObject& object : gObjects) {
        if (object.q220LooseObject &&
            object.refFormId == refFormId) {
            std::copy(transform, transform + 16,
                      object.q220DynamicTransform);
        }
    }
}

bool Q223GetRefTransform(uint32_t refFormId, float out[16]) {
    for (const GpuObject& object : gObjects) {
        if (object.q220LooseObject &&
            object.refFormId == refFormId) {
            std::copy(
                object.q220DynamicTransform,
                object.q220DynamicTransform + 16,
                out);
            return true;
        }
    }
    Q220Identity(out);
    return false;
}

float Q223CollisionRadiusUnits(const std::string& modelPath) {
    const auto cached =
        gQ223CollisionRadiusUnitsCache.find(modelPath);
    if (cached != gQ223CollisionRadiusUnitsCache.end())
        return cached->second;

    std::vector<Fo3NifCollisionShapeQ6F> shapes;
    if (!LoadFo3NifCollisionShapesQ6F(modelPath, shapes) ||
        shapes.empty()) {
        gQ223CollisionRadiusUnitsCache[modelPath] = 0.0f;
        return 0.0f;
    }

    Vec3 minimum{1e30f,1e30f,1e30f};
    Vec3 maximum{-1e30f,-1e30f,-1e30f};
    size_t points = 0u;
    for (const Fo3NifCollisionShapeQ6F& shape : shapes) {
        for (size_t i = 0u;
             i + 2u < shape.positions.size();
             i += 3u) {
            const Vec3 p{
                shape.positions[i + 0u],
                shape.positions[i + 1u],
                shape.positions[i + 2u]};
            minimum.x = std::min(minimum.x, p.x);
            minimum.y = std::min(minimum.y, p.y);
            minimum.z = std::min(minimum.z, p.z);
            maximum.x = std::max(maximum.x, p.x);
            maximum.y = std::max(maximum.y, p.y);
            maximum.z = std::max(maximum.z, p.z);
            ++points;
        }
    }

    if (points == 0u) {
        gQ223CollisionRadiusUnitsCache[modelPath] = 0.0f;
        return 0.0f;
    }

    const Vec3 half{
        (maximum.x - minimum.x) * 0.5f,
        (maximum.y - minimum.y) * 0.5f,
        (maximum.z - minimum.z) * 0.5f};
    const float radius = Q211Length(half);
    gQ223CollisionRadiusUnitsCache[modelPath] = radius;
    return radius;
}

Q223DynamicBody* Q223EnsureDynamicBody(uint32_t refFormId) {
    auto existing = gQ223DynamicBodies.find(refFormId);
    if (existing != gQ223DynamicBodies.end())
        return &existing->second;

    Vec3 minimum{1e30f,1e30f,1e30f};
    Vec3 maximum{-1e30f,-1e30f,-1e30f};
    std::string modelPath;
    float placementScale = 1.0f;
    bool found = false;

    for (const GpuObject& object : gObjects) {
        if (!object.q220LooseObject ||
            object.refFormId != refFormId) {
            continue;
        }
        minimum.x = std::min(minimum.x, object.minX);
        minimum.y = std::min(minimum.y, object.minY);
        minimum.z = std::min(minimum.z, object.minZ);
        maximum.x = std::max(maximum.x, object.maxX);
        maximum.y = std::max(maximum.y, object.maxY);
        maximum.z = std::max(maximum.z, object.maxZ);
        if (modelPath.empty()) {
            modelPath = object.modelPath;
            placementScale = object.q223PlacementScale;
        }
        found = true;
    }
    if (!found || modelPath.empty()) return nullptr;

    Q223DynamicBody body;
    body.refFormId = refFormId;
    body.initialized = true;
    body.authoredCenter = {
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f};
    body.modelPath = modelPath;

    const float radiusUnits =
        Q223CollisionRadiusUnits(modelPath);
    const Vec3 renderHalf{
        (maximum.x - minimum.x) * 0.5f,
        (maximum.y - minimum.y) * 0.5f,
        (maximum.z - minimum.z) * 0.5f};
    const float renderRadius =
        Q211Length(renderHalf);

    const bool authoredBhk =
        radiusUnits > 0.0f &&
        std::isfinite(radiusUnits);
    body.collisionHalfExtents = renderHalf;
    if (authoredBhk) {
        body.collisionRadius =
            (radiusUnits * placementScale) /
            FO3_UNITS_PER_METRE;
    } else {
        // Q22.4 fallback for Bethesda loose clutter with no bhkRigidBody.
        // The visible mesh is still Fallout-authored data; only the enclosing
        // sphere representation is a standalone-VR simplification.
        body.collisionRadius =
            std::max(0.015f, renderRadius);
    }

    Q6H_LOGI("Q22.5 PHYSICS BODY: ref=%08X model=%s radius=%.4fm source=%s renderHalf=(%.3f %.3f %.3f) placementScale=%.3f physics=enabled",
             refFormId, modelPath.c_str(),
             body.collisionRadius,
             authoredBhk
                 ? "authored-bhk-bounds"
                 : "authored-render-bounds-fallback",
             renderHalf.x, renderHalf.y, renderHalf.z,
             placementScale);

    auto inserted =
        gQ223DynamicBodies.emplace(refFormId, std::move(body));
    return &inserted.first->second;
}

void Q223SampleHeldMotion(
        Q223DynamicBody& body,
        Vec3 palm,
        const float quat[4]) {
    const auto now = std::chrono::steady_clock::now();
    if (body.lastPalmSample.time_since_epoch().count() != 0) {
        const float dt =
            std::chrono::duration<float>(
                now - body.lastPalmSample).count();
        if (dt >= 0.001f && dt <= 0.050f) {
            const Vec3 instantLinear =
                Q211Mul(
                    Q211Sub(palm, body.lastPalm),
                    1.0f / dt);
            if (body.motionSamples == 0) {
                body.sampledLinearVelocity = instantLinear;
            } else {
                body.sampledLinearVelocity =
                    Q211Add(
                        Q211Mul(body.sampledLinearVelocity,0.60f),
                        Q211Mul(instantLinear,0.40f));
            }

            const float sx = -body.lastPalmQuat[0];
            const float sy = -body.lastPalmQuat[1];
            const float sz = -body.lastPalmQuat[2];
            const float sw =  body.lastPalmQuat[3];
            const float cx = quat[0], cy = quat[1],
                        cz = quat[2], cw = quat[3];
            float qx = cw*sx + cx*sw + cy*sz - cz*sy;
            float qy = cw*sy - cx*sz + cy*sw + cz*sx;
            float qz = cw*sz + cx*sy - cy*sx + cz*sw;
            float qw = cw*sw - cx*sx - cy*sy - cz*sz;

            const float qlen =
                std::sqrt(
                    qx*qx + qy*qy + qz*qz + qw*qw);
            if (qlen > 1.0e-6f) {
                qx /= qlen; qy /= qlen;
                qz /= qlen; qw /= qlen;
            }
            if (qw < 0.0f) {
                qx = -qx; qy = -qy;
                qz = -qz; qw = -qw;
            }
            qw = std::clamp(qw, -1.0f, 1.0f);
            const float angle = 2.0f * std::acos(qw);
            const float sinHalf =
                std::sqrt(
                    std::max(0.0f, 1.0f - qw*qw));
            if (angle > 1.0e-5f &&
                sinHalf > 1.0e-5f) {
                const Vec3 axis{
                    qx / sinHalf,
                    qy / sinHalf,
                    qz / sinHalf};
                const Vec3 instantAngular =
                    Q211Mul(axis, angle / dt);
                if (body.motionSamples == 0) {
                    body.sampledAngularVelocity = instantAngular;
                } else {
                    body.sampledAngularVelocity =
                        Q211Add(
                            Q211Mul(body.sampledAngularVelocity,0.60f),
                            Q211Mul(instantAngular,0.40f));
                }
            } else if (body.motionSamples == 0) {
                body.sampledAngularVelocity = {};
            }
            ++body.motionSamples;
        }
    }

    body.lastPalm = palm;
    std::copy(quat, quat + 4, body.lastPalmQuat);
    body.lastPalmSample = now;
}

void Q223BuildMotionDelta(
        Vec3 currentCenter,
        Vec3 desiredCenter,
        Vec3 angularVelocity,
        float dt,
        float out[16]) {
    Q220Identity(out);

    const float omega = Q211Length(angularVelocity);
    if (omega > 1.0e-5f) {
        const Vec3 axis =
            Q211Mul(angularVelocity, 1.0f / omega);
        const float angle = omega * dt;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const float t = 1.0f - c;
        const float x = axis.x, y = axis.y, z = axis.z;

        out[0] = t*x*x + c;
        out[1] = t*x*y + s*z;
        out[2] = t*x*z - s*y;
        out[4] = t*x*y - s*z;
        out[5] = t*y*y + c;
        out[6] = t*y*z + s*x;
        out[8] = t*x*z + s*y;
        out[9] = t*y*z - s*x;
        out[10] = t*z*z + c;
    }

    const Vec3 rotatedCenter{
        out[0]*currentCenter.x +
            out[4]*currentCenter.y +
            out[8]*currentCenter.z,
        out[1]*currentCenter.x +
            out[5]*currentCenter.y +
            out[9]*currentCenter.z,
        out[2]*currentCenter.x +
            out[6]*currentCenter.y +
            out[10]*currentCenter.z};
    out[12] = desiredCenter.x - rotatedCenter.x;
    out[13] = desiredCenter.y - rotatedCenter.y;
    out[14] = desiredCenter.z - rotatedCenter.z;
}

void Q223AdvanceDynamicBodies() {
    if (gQ223DynamicBodies.empty()) return;

    const auto now = std::chrono::steady_clock::now();
    // Q22.3 standalone-VR runtime liberty: SI Earth gravity. Fallout's
    // supplied INI/ESM does not expose an unambiguous gravity acceleration.
    constexpr float GRAVITY_MPS2 = 9.80665f;
    // Fallout.ini [HAVOK] fMaxTime=0.016.
    constexpr float HAVOK_MAX_STEP = 0.016f;

    for (auto& entry : gQ223DynamicBodies) {
        Q223DynamicBody& body = entry.second;
        if (!body.dynamic || body.held ||
            body.sleeping ||
            !(body.collisionRadius > 0.0f)) {
            continue;
        }

        if (body.lastPhysicsStep.time_since_epoch().count() == 0) {
            body.lastPhysicsStep = now;
            continue;
        }

        float remaining =
            std::chrono::duration<float>(
                now - body.lastPhysicsStep).count();
        body.lastPhysicsStep = now;
        if (!(remaining > 0.0f)) continue;

        // Avoid a headset pause/resume producing seconds of catch-up work.
        remaining = std::min(remaining, 0.064f);

        while (remaining > 0.00001f) {
            const float dt =
                std::min(remaining, HAVOK_MAX_STEP);
            remaining -= dt;

            float currentTransform[16]{};
            if (!Q223GetRefTransform(
                    body.refFormId,
                    currentTransform)) {
                body.dynamic = false;
                break;
            }

            const Vec3 currentCenter =
                Q220TransformPoint(
                    currentTransform,
                    body.authoredCenter);

            body.linearVelocity.y -=
                GRAVITY_MPS2 * dt;
            const Vec3 desiredCenter =
                Q211Add(
                    currentCenter,
                    Q211Mul(body.linearVelocity, dt));

            float rx = desiredCenter.x;
            float ry = desiredCenter.y;
            float rz = desiredCenter.z;
            float nx = 0.0f, ny = 0.0f, nz = 0.0f;
            uint32_t contacts = 0u;

            Vec3 axisX{
                currentTransform[0],
                currentTransform[1],
                currentTransform[2]};
            Vec3 axisY{
                currentTransform[4],
                currentTransform[5],
                currentTransform[6]};
            Vec3 axisZ{
                currentTransform[8],
                currentTransform[9],
                currentTransform[10]};
            axisX = Q211NormalizeSafe(axisX,{1.0f,0.0f,0.0f});
            axisY = Q211NormalizeSafe(axisY,{0.0f,1.0f,0.0f});
            axisZ = Q211NormalizeSafe(axisZ,{0.0f,0.0f,1.0f});

            uint32_t candidates = 0u;
            const bool collisionReady =
                ResolveFo3DynamicBoxQ225(
                    body.refFormId,
                    currentCenter.x,
                    currentCenter.y,
                    currentCenter.z,
                    desiredCenter.x,
                    desiredCenter.y,
                    desiredCenter.z,
                    body.collisionHalfExtents.x,
                    body.collisionHalfExtents.y,
                    body.collisionHalfExtents.z,
                    axisX.x, axisX.y, axisX.z,
                    axisY.x, axisY.y, axisY.z,
                    axisZ.x, axisZ.y, axisZ.z,
                    body.collisionRadius,
                    &rx, &ry, &rz,
                    &nx, &ny, &nz,
                    &contacts,
                    &candidates);

            const Vec3 resolvedCenter{rx, ry, rz};
            if (collisionReady && contacts > 0u) {
                const Vec3 normal =
                    Q211NormalizeSafe(
                        Vec3{nx, ny, nz},
                        Vec3{0.0f, 1.0f, 0.0f});
                const float inward =
                    Q211Dot(
                        body.linearVelocity,
                        normal);
                if (inward < 0.0f) {
                    body.linearVelocity =
                        Q211Sub(
                            body.linearVelocity,
                            Q211Mul(normal, inward));
                }

                // Q22.5 temporary VR-side settling response. Preserve
                // tangential motion/spin instead of freezing at first touch.
                // These damping values are solver choices until authored
                // Havok material/inertia fields are connected.
                const float linearRetain =
                    std::exp(-1.25f * dt);
                const float angularRetain =
                    std::exp(-2.00f * dt);
                body.linearVelocity =
                    Q211Mul(body.linearVelocity,linearRetain);
                body.angularVelocity =
                    Q211Mul(body.angularVelocity,angularRetain);

                const float linearSpeed =
                    Q211Length(body.linearVelocity);
                const float angularSpeed =
                    Q211Length(body.angularVelocity);
                if (normal.y > 0.45f &&
                    linearSpeed < 0.08f &&
                    angularSpeed < 0.22f) {
                    body.sleepTimer += dt;
                } else {
                    body.sleepTimer = 0.0f;
                }

                if (body.sleepTimer >= 0.30f) {
                    body.sleeping = true;
                    body.dynamic = false;
                    body.linearVelocity = {};
                    body.angularVelocity = {};
                    Q6H_LOGI("Q22.5 SLEEP: ref=%08X center=(%.3f %.3f %.3f) after=%.2fs",
                             body.refFormId,
                             resolvedCenter.x,
                             resolvedCenter.y,
                             resolvedCenter.z,
                             body.sleepTimer);
                }

                static uint64_t q225ContactLog = 0u;
                ++q225ContactLog;
                if (q225ContactLog <= 40u ||
                    (q225ContactLog % 180u) == 0u) {
                    Q6H_LOGI("Q22.5 CONTACT: ref=%08X contacts=%u candidates=%u broadRadius=%.3f half=(%.3f %.3f %.3f) normal=(%.2f %.2f %.2f) velocity=(%.2f %.2f %.2f) angular=%.2f sleep=%.2f",
                             body.refFormId,
                             contacts,
                             candidates,
                             body.collisionRadius,
                             body.collisionHalfExtents.x,
                             body.collisionHalfExtents.y,
                             body.collisionHalfExtents.z,
                             normal.x, normal.y, normal.z,
                             body.linearVelocity.x,
                             body.linearVelocity.y,
                             body.linearVelocity.z,
                             angularSpeed,
                             body.sleepTimer);
                }
            }

            float motionDelta[16]{};
            Q223BuildMotionDelta(
                currentCenter,
                resolvedCenter,
                body.angularVelocity,
                dt,
                motionDelta);
            float finalTransform[16]{};
            Q220MulMat4(
                motionDelta,
                currentTransform,
                finalTransform);
            Q220SetRefTransform(
                body.refFormId,
                finalTransform);
            if (body.sleeping) break;
        }
    }
}

bool Q220FindNearestLooseRef(
        Vec3 hand,
        uint32_t& outRef,
        Vec3& outCenter,
        float& outSurfaceDistance) {
    struct Aggregate {
        Vec3 minimum{1e30f,1e30f,1e30f};
        Vec3 maximum{-1e30f,-1e30f,-1e30f};
        const float* transform = nullptr;
    };
    std::unordered_map<uint32_t, Aggregate> refs;
    for (const GpuObject& object : gObjects) {
        if (!object.q220LooseObject ||
            object.refFormId == 0u) continue;
        Aggregate& a = refs[object.refFormId];
        a.minimum.x = std::min(a.minimum.x, object.minX);
        a.minimum.y = std::min(a.minimum.y, object.minY);
        a.minimum.z = std::min(a.minimum.z, object.minZ);
        a.maximum.x = std::max(a.maximum.x, object.maxX);
        a.maximum.y = std::max(a.maximum.y, object.maxY);
        a.maximum.z = std::max(a.maximum.z, object.maxZ);
        if (!a.transform)
            a.transform = object.q220DynamicTransform;
    }

    constexpr float Q220_GRAB_REACH = 0.16f;
    bool found = false;
    float best = Q220_GRAB_REACH;
    for (const auto& entry : refs) {
        const Aggregate& a = entry.second;
        if (!a.transform) continue;
        const Vec3 originalCenter{
            (a.minimum.x + a.maximum.x) * 0.5f,
            (a.minimum.y + a.maximum.y) * 0.5f,
            (a.minimum.z + a.maximum.z) * 0.5f};
        const Vec3 center =
            Q220TransformPoint(a.transform, originalCenter);
        const Vec3 half{
            (a.maximum.x - a.minimum.x) * 0.5f,
            (a.maximum.y - a.minimum.y) * 0.5f,
            (a.maximum.z - a.minimum.z) * 0.5f};
        const float radius =
            std::max(0.015f, Q211Length(half));
        const float surfaceDistance =
            std::max(
                0.0f,
                Q211Length(Q211Sub(hand, center)) - radius);
        if (surfaceDistance <= best) {
            best = surfaceDistance;
            outRef = entry.first;
            outCenter = center;
            outSurfaceDistance = surfaceDistance;
            found = true;
        }
    }
    return found;
}

void Q220UpdateLooseGrab(
        int handIndex,
        bool handValid,
        Vec3 hand,
        const float quat[4],
        float grip) {
    Q220GrabState& state = gQ220Grab[handIndex];
    constexpr float PRESS = 0.65f;
    constexpr float RELEASE = 0.25f;

    if (!handValid) {
        state.previousGrip = grip;
        return;
    }

    if (!state.active &&
        state.previousGrip < PRESS &&
        grip >= PRESS) {
        uint32_t ref = 0u;
        Vec3 center{};
        float distance = 0.0f;
        if (Q220FindNearestLooseRef(
                hand, ref, center, distance)) {
            state.active = true;
            state.refFormId = ref;
            state.startPalm = hand;
            std::copy(quat, quat + 4, state.startQuat);

            if (Q223DynamicBody* body =
                    Q223EnsureDynamicBody(ref)) {
                body->held = true;
                body->dynamic = false;
                body->linearVelocity = {};
                body->angularVelocity = {};
                body->sampledLinearVelocity = {};
                body->sampledAngularVelocity = {};
                body->motionSamples = 0;
                body->sleeping = false;
                body->sleepTimer = 0.0f;
                body->lastPalmSample = {};
                body->lastPhysicsStep = {};
                Q223SampleHeldMotion(
                    *body, hand, quat);
            }

            float existingTransform[16]{};
            bool copied = false;
            for (const GpuObject& object : gObjects) {
                if (object.q220LooseObject &&
                    object.refFormId == ref) {
                    std::copy(
                        object.q220DynamicTransform,
                        object.q220DynamicTransform + 16,
                        existingTransform);
                    copied = true;
                    break;
                }
            }
            if (!copied) Q220Identity(existingTransform);

            // Q22.1 generic loose-clutter anchor:
            // snap the authored REFR bounds-center to the weighted Fallout
            // palm anchor produced by the same IK solve that draws the hand.
            float snapTranslation[16]{};
            Q220Identity(snapTranslation);
            snapTranslation[12] = hand.x - center.x;
            snapTranslation[13] = hand.y - center.y;
            snapTranslation[14] = hand.z - center.z;
            Q220MulMat4(
                snapTranslation,
                existingTransform,
                state.baseTransform);
            Q220SetRefTransform(ref, state.baseTransform);

            const float snapDistance =
                Q211Length(Q211Sub(hand, center));
            Q6H_LOGI("Q22.5 GRAB BEGIN: hand=%s ref=%08X surfaceDistance=%.3f snapDistance=%.3f objectCenter=(%.3f %.3f %.3f) palm=(%.3f %.3f %.3f) anchor=authored-bounds-center-to-weighted-player-palm",
                     handIndex == 0 ? "L" : "R",
                     ref, distance, snapDistance,
                     center.x, center.y, center.z,
                     hand.x, hand.y, hand.z);
        }
    }

    if (state.active) {
        if (Q223DynamicBody* body =
                Q223EnsureDynamicBody(state.refFormId)) {
            body->held = true;
            Q223SampleHeldMotion(*body, hand, quat);
        }

        if (grip <= RELEASE) {
            const uint32_t releasedRef =
                state.refFormId;
            if (Q223DynamicBody* body =
                    Q223EnsureDynamicBody(releasedRef)) {
                body->held = false;
                body->linearVelocity =
                    body->sampledLinearVelocity;
                body->angularVelocity =
                    body->sampledAngularVelocity;
                body->dynamic =
                    body->collisionRadius > 0.0f;
                body->sleeping = false;
                body->sleepTimer = 0.0f;
                body->lastPhysicsStep =
                    std::chrono::steady_clock::now();

                Q6H_LOGI("Q22.5 THROW RELEASE: hand=%s ref=%08X physics=%d linear=(%.2f %.2f %.2f)mps angular=(%.2f %.2f %.2f)radps radius=%.3f",
                         handIndex == 0 ? "L" : "R",
                         releasedRef,
                         body->dynamic ? 1 : 0,
                         body->linearVelocity.x,
                         body->linearVelocity.y,
                         body->linearVelocity.z,
                         body->angularVelocity.x,
                         body->angularVelocity.y,
                         body->angularVelocity.z,
                         body->collisionRadius);
            }
            state.active = false;
            state.refFormId = 0u;
        } else {
            float controllerDelta[16]{};
            Q220ControllerDeltaMatrix(
                state.startPalm, state.startQuat,
                hand, quat, controllerDelta);
            float finalTransform[16]{};
            Q220MulMat4(
                controllerDelta,
                state.baseTransform,
                finalTransform);
            Q220SetRefTransform(
                state.refFormId,
                finalTransform);
        }
    }

    state.previousGrip = grip;
}

void Q221UpdateLooseObjectsFromSolvedPalms(
        bool leftPalmValid,
        Vec3 leftPalmWorld,
        bool rightPalmValid,
        Vec3 rightPalmWorld) {
    Q223AdvanceDynamicBodies();
    Q220UpdateLooseGrab(
        0, leftPalmValid,
        leftPalmWorld, gQ218LeftHandQuat,
        gQ217FingerGrip[0]);
    Q220UpdateLooseGrab(
        1, rightPalmValid,
        rightPalmWorld, gQ218RightHandQuat,
        gQ217FingerGrip[1]);
}

std::string Q211Lower(std::string value) {
    for (char& ch : value)
        ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

int Q211FindPrimaryBone(
        const std::vector<Fo3NifSkinBone>& bones,
        const char* exactToken,
        const char* fallbackToken) {
    int fallback = -1;
    for (size_t i = 0u; i < bones.size(); ++i) {
        const std::string lower = Q211Lower(bones[i].name);
        if (lower.find(exactToken) != std::string::npos &&
            lower.find("twist") == std::string::npos) {
            return static_cast<int>(i);
        }
        if (fallback < 0 &&
            lower.find(fallbackToken) != std::string::npos &&
            lower.find("twist") == std::string::npos) {
            fallback = static_cast<int>(i);
        }
    }
    return fallback;
}


Vec3 Q211BindBonePoint(const Fo3NifSkinBone& bone);

int Q211FindHeadAnchorBone(
        const std::vector<Fo3NifSkinBone>& bones,
        bool& isNeck) {
    isNeck = false;
    for (size_t i = 0u; i < bones.size(); ++i) {
        const std::string lower = Q211Lower(bones[i].name);
        if (lower == "bip01 head" ||
            lower.find("bip01 head") != std::string::npos) {
            return static_cast<int>(i);
        }
    }
    for (size_t i = 0u; i < bones.size(); ++i) {
        const std::string lower = Q211Lower(bones[i].name);
        if (lower == "bip01 neck" ||
            lower.find("bip01 neck") != std::string::npos) {
            isNeck = true;
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool Q211FindAvatarHeadAnchor(Vec3& out) {
    // Prefer the authored Head node. Some body parts do not reference Head;
    // Neck is a stable fallback, then shoulder midpoint.
    for (const Q211PlayerRigPart& part : gQ211PlayerRigParts) {
        bool isNeck = false;
        const int bone = Q211FindHeadAnchorBone(part.bones, isNeck);
        if (bone >= 0) {
            out = Q211BindBonePoint(part.bones[bone]);
            if (isNeck) out.y += 0.14f;
            return true;
        }
    }

    for (const Q211PlayerRigPart& part : gQ211PlayerRigParts) {
        if (part.leftUpperArm >= 0 && part.rightUpperArm >= 0) {
            const Vec3 l = Q211BindBonePoint(
                part.bones[part.leftUpperArm]);
            const Vec3 r = Q211BindBonePoint(
                part.bones[part.rightUpperArm]);
            out = Q211Mul(Q211Add(l, r), 0.5f);
            out.y += 0.23f;
            return true;
        }
    }
    return false;
}

int Q211BoneRole(const std::string& authoredName) {
    const std::string name = Q211Lower(authoredName);
    const bool left =
        name.find("bip01 l ") != std::string::npos ||
        name.find(" l ") != std::string::npos;
    const bool right =
        name.find("bip01 r ") != std::string::npos ||
        name.find(" r ") != std::string::npos;
    if (!left && !right) return 0;

    // Fallout 3's skeleton uses both the main limb bones and dedicated
    // twist links. In particular FO3 assets commonly reference
    // "Bip01 L/R ForeTwist" rather than spelling the twist as ForearmTwist.
    // Those vertices must inherit the same solved limb transform or the
    // distal arm remains in bind pose while the main UpperArm/Forearm moves.
    if (name.find("upperarm") != std::string::npos ||
        name.find("uparmtwist") != std::string::npos)
        return left ? 1 : 4;
    if (name.find("forearm") != std::string::npos ||
        name.find("foretwist") != std::string::npos)
        return left ? 2 : 5;
    if (name.find("hand") != std::string::npos ||
        name.find("finger") != std::string::npos ||
        name.find("thumb") != std::string::npos)
        return left ? 3 : 6;
    return 0;
}

bool Q211WeightedGeometryAnchor(
        const Q211PlayerRigPart& part,
        int desiredRole,
        Vec3& out,
        float& outWeight) {
    constexpr size_t STRIDE = 18u;
    out = {};
    outWeight = 0.0f;
    const size_t vertices = part.bindExpanded.size() / STRIDE;
    if (part.expandedBoneIndices.size() != vertices * 4u ||
        part.expandedBoneWeights.size() != vertices * 4u) {
        return false;
    }

    for (size_t v = 0u; v < vertices; ++v) {
        float roleWeight = 0.0f;
        for (size_t slot = 0u; slot < 4u; ++slot) {
            const size_t at = v * 4u + slot;
            const uint16_t bone = part.expandedBoneIndices[at];
            if (bone >= part.bones.size()) continue;
            if (Q211BoneRole(part.bones[bone].name) == desiredRole) {
                roleWeight += part.expandedBoneWeights[at];
            }
        }
        if (roleWeight <= 0.001f) continue;
        const size_t base = v * STRIDE;
        out.x += part.bindExpanded[base + 0u] * roleWeight;
        out.y += part.bindExpanded[base + 1u] * roleWeight;
        out.z += part.bindExpanded[base + 2u] * roleWeight;
        outWeight += roleWeight;
    }

    if (outWeight <= 0.001f) return false;
    const float inv = 1.0f / outWeight;
    out.x *= inv;
    out.y *= inv;
    out.z *= inv;
    return std::isfinite(out.x) &&
           std::isfinite(out.y) &&
           std::isfinite(out.z);
}

// Q22.2: grabbing needs the centre of the visible palm, not Q21's distal
// arm endpoint. Use only the exact Hand bone's authored vertex weights so
// fingers/thumb/forearm cannot pull the interaction anchor toward the wrist.
bool Q222ExactBoneGeometryAnchor(
        const Q211PlayerRigPart& part,
        int boneIndex,
        Vec3& out,
        float& outWeight) {
    constexpr size_t STRIDE = 18u;
    out = {};
    outWeight = 0.0f;
    if (boneIndex < 0 ||
        static_cast<size_t>(boneIndex) >= part.bones.size()) {
        return false;
    }

    const size_t vertices = part.bindExpanded.size() / STRIDE;
    if (part.expandedBoneIndices.size() != vertices * 4u ||
        part.expandedBoneWeights.size() != vertices * 4u) {
        return false;
    }

    for (size_t v = 0u; v < vertices; ++v) {
        float exactWeight = 0.0f;
        for (size_t slot = 0u; slot < 4u; ++slot) {
            const size_t at = v * 4u + slot;
            if (part.expandedBoneIndices[at] ==
                static_cast<uint16_t>(boneIndex)) {
                exactWeight += part.expandedBoneWeights[at];
            }
        }
        if (exactWeight <= 0.001f) continue;

        const size_t base = v * STRIDE;
        out.x += part.bindExpanded[base + 0u] * exactWeight;
        out.y += part.bindExpanded[base + 1u] * exactWeight;
        out.z += part.bindExpanded[base + 2u] * exactWeight;
        outWeight += exactWeight;
    }

    if (outWeight <= 0.001f) return false;
    const float inv = 1.0f / outWeight;
    out.x *= inv;
    out.y *= inv;
    out.z *= inv;
    return std::isfinite(out.x) &&
           std::isfinite(out.y) &&
           std::isfinite(out.z);
}

bool Q222FindVisibleGrabPalmAnchor(
        bool left,
        Vec3& out,
        float& outWeight,
        std::string& outSource) {
    out = {};
    outWeight = 0.0f;
    outSource.clear();

    const char* preferred =
        left
            ? "characters\\_male\\lefthandpipboyglove.nif"
            : "characters\\_male\\righthand.nif";

    // Pass 0: exact visible hand/glove asset. Pass 1: any visible non-IK
    // player part that exposes the authored Hand bone.
    for (int pass = 0; pass < 2; ++pass) {
        Vec3 weighted{};
        float totalWeight = 0.0f;
        std::string sources;

        for (const Q211PlayerRigPart& part : gQ211PlayerRigParts) {
            const bool preferredMatch =
                Q210EndsWithInsensitive(
                    part.sourceModelPath, preferred);
            if (pass == 0 && !preferredMatch) continue;
            if (pass == 1) {
                if (preferredMatch) continue;
                if (Q210EndsWithInsensitive(
                        part.sourceModelPath,
                        "characters\\_male\\upperbody.nif")) {
                    continue;
                }
            }

            const int handBone =
                left ? part.leftHand : part.rightHand;
            Vec3 localAnchor{};
            float localWeight = 0.0f;
            if (!Q222ExactBoneGeometryAnchor(
                    part, handBone,
                    localAnchor, localWeight)) {
                continue;
            }

            weighted = Q211Add(
                weighted,
                Q211Mul(localAnchor, localWeight));
            totalWeight += localWeight;

            if (!sources.empty()) sources += ",";
            sources += part.sourceModelPath;
        }

        if (totalWeight > 0.001f) {
            out = Q211Mul(weighted, 1.0f / totalWeight);
            outWeight = totalWeight;
            outSource = sources;
            return true;
        }
    }

    return false;
}

Vec3 Q211BindBonePoint(const Fo3NifSkinBone& bone) {
    // Same game->OpenXR conversion used by PrepareExpandedVertexStreamQ1960
    // for a player mesh placed at the local origin.
    return {
        bone.bindPosition[0] / FO3_UNITS_PER_METRE,
        FLOOR_Y + bone.bindPosition[2] / FO3_UNITS_PER_METRE,
        SCENE_FORWARD - bone.bindPosition[1] / FO3_UNITS_PER_METRE
    };
}

Vec3 Q211TransformPoint(const float m[16], Vec3 p) {
    return {
        m[0]*p.x + m[4]*p.y + m[8]*p.z + m[12],
        m[1]*p.x + m[5]*p.y + m[9]*p.z + m[13],
        m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]
    };
}

Vec3 Q218TransformVector(const float m[16], Vec3 v) {
    return {
        m[0]*v.x + m[4]*v.y + m[8]*v.z,
        m[1]*v.x + m[5]*v.y + m[9]*v.z,
        m[2]*v.x + m[6]*v.y + m[10]*v.z
    };
}

Vec3 Q218RotateQuaternion(const float q[4], Vec3 v) {
    const Vec3 u{q[0], q[1], q[2]};
    const float qw = q[3];
    return Q211Add(
        Q211Add(
            Q211Mul(u, 2.0f * Q211Dot(u, v)),
            Q211Mul(v, qw*qw - Q211Dot(u, u))),
        Q211Mul(Q211Cross(u, v), 2.0f * qw));
}

struct Q220HandBasis {
    Vec3 littleToThumb{0.0f, 0.0f, -1.0f};
    Vec3 intoPalm{0.0f, -1.0f, 0.0f};
    bool ready = false;
};

bool Q221FindGlobalBonePoint(
        bool left,
        const std::string& authoredSuffix,
        Vec3& outPoint,
        std::string& outName) {
    const std::string prefix =
        std::string("bip01 ") + (left ? "l " : "r ") + authoredSuffix;
    bool found = false;
    size_t bestLength = std::numeric_limits<size_t>::max();

    for (const Q211PlayerRigPart& part : gQ211PlayerRigParts) {
        for (const Fo3NifSkinBone& bone : part.bones) {
            const std::string lower = Q211Lower(bone.name);
            if (lower == prefix) {
                outPoint = Q211BindBonePoint(bone);
                outName = bone.name;
                return true;
            }
            if (lower.rfind(prefix, 0u) == 0u &&
                lower.size() < bestLength) {
                outPoint = Q211BindBonePoint(bone);
                outName = bone.name;
                bestLength = lower.size();
                found = true;
            }
        }
    }
    return found;
}

bool Q220FindAuthoredHandBasis(bool left, Q220HandBasis& out) {
    out = {};

    Vec3 handPoint{}, middlePoint{}, littlePoint{};
    std::string handName, middleName, littleName;
    const bool handReady =
        Q221FindGlobalBonePoint(left, "hand", handPoint, handName);
    const bool middleReady =
        Q221FindGlobalBonePoint(left, "finger2", middlePoint, middleName);
    const bool littleReady =
        Q221FindGlobalBonePoint(left, "finger4", littlePoint, littleName);

    if (!handReady || !middleReady || !littleReady) {
        if ((gQ211TrackingSerial % 180u) == 1u) {
            Q6H_LOGI("Q21.13 HAND BONE LOOKUP: side=%s hand=%d(%s) middle2=%d(%s) little4=%d(%s)",
                     left ? "L" : "R",
                     handReady ? 1 : 0, handName.c_str(),
                     middleReady ? 1 : 0, middleName.c_str(),
                     littleReady ? 1 : 0, littleName.c_str());
        }
        return false;
    }

    Vec3 fingerForward =
        Q211Sub(middlePoint, handPoint);

    // Fallout does not expose Finger0 in these loaded hand partitions.
    // Finger4 is the little-finger chain and Finger2 is the middle chain, so
    // little -> middle is an authored approximation of OpenXR's required
    // little -> thumb (-Z) across-palm direction without inventing an offset.
    Vec3 littleToThumb =
        Q211Sub(middlePoint, littlePoint);

    if (Q211Length(fingerForward) < 0.02f ||
        Q211Length(littleToThumb) < 0.02f) {
        return false;
    }

    fingerForward = Q211NormalizeSafe(fingerForward);
    littleToThumb = Q211NormalizeSafe(littleToThumb);

    // In Fallout's authored T-pose the palm normal is perpendicular to the
    // finger direction and the across-palm direction.
    Vec3 intoPalm =
        Q211NormalizeSafe(Q211Cross(littleToThumb, fingerForward));

    // Q21.14: the authored across-hand axis is already correctly mirrored by
    // the Fallout skeleton, but the right-hand palm normal produced by the
    // same cross-product has the opposite anatomical sign. Mirror only this
    // normal; changing the across-hand axis would re-introduce the inversion.
    if (!left) intoPalm = Q211Mul(intoPalm, -1.0f);

    littleToThumb = Q211Sub(
        littleToThumb,
        Q211Mul(intoPalm, Q211Dot(littleToThumb, intoPalm)));
    if (Q211Length(littleToThumb) < 0.03f ||
        Q211Length(intoPalm) < 0.03f) {
        return false;
    }

    out.littleToThumb = Q211NormalizeSafe(littleToThumb);
    out.intoPalm = Q211NormalizeSafe(intoPalm);
    out.ready = true;

    if ((gQ211TrackingSerial % 180u) == 1u) {
        Q6H_LOGI("Q21.13 HAND BONE LOOKUP: side=%s hand=%s middle2=%s little4=%s handP=(%.3f %.3f %.3f) middleP=(%.3f %.3f %.3f) littleP=(%.3f %.3f %.3f)",
                 left ? "L" : "R",
                 handName.c_str(), middleName.c_str(), littleName.c_str(),
                 handPoint.x, handPoint.y, handPoint.z,
                 middlePoint.x, middlePoint.y, middlePoint.z,
                 littlePoint.x, littlePoint.y, littlePoint.z);
    }
    return true;
}

struct Q217FingerJoint {
    std::string name;
    Vec3 pivot{};
    int chain = -1;
    int depth = 0;
    bool thumb = false;
};

struct Q217FingerChain {
    int id = -1;
    std::vector<Q217FingerJoint> joints;
};

struct Q217FingerRig {
    bool attempted = false;
    bool ready = false;
    int indexChain = -1;
    float curlSign = 1.0f;
    Vec3 handPoint{};
    Vec3 fingerForward{};
    std::vector<Q217FingerChain> chains;
    std::vector<Q217FingerJoint> thumb;
    std::string summary;
};

Q217FingerRig gQ217FingerRig[2];

struct Q211Delta {
    float r[9]{
        1,0,0,
        0,1,0,
        0,0,1
    };
    Vec3 t{0.0f, 0.0f, 0.0f};
    Vec3 stretchPivot{0.0f, 0.0f, 0.0f};
    Vec3 stretchAxis{1.0f, 0.0f, 0.0f};
    float axialScale = 1.0f;
    bool active = false;
};

void Q211RotationFromTo(Vec3 from, Vec3 to, float out[9]) {
    const Vec3 a = Q211NormalizeSafe(from);
    const Vec3 b = Q211NormalizeSafe(to, a);
    const float d = std::clamp(Q211Dot(a, b), -1.0f, 1.0f);
    Vec3 axis = Q211Cross(a, b);
    float axisLen = Q211Length(axis);

    if (axisLen < 1.0e-5f) {
        if (d > 0.0f) {
            out[0]=1; out[1]=0; out[2]=0;
            out[3]=0; out[4]=1; out[5]=0;
            out[6]=0; out[7]=0; out[8]=1;
            return;
        }
        Vec3 helper =
            std::fabs(a.y) < 0.9f ? Vec3{0,1,0} : Vec3{1,0,0};
        axis = Q211NormalizeSafe(Q211Cross(a, helper));
        axisLen = 1.0f;
    } else {
        axis = Q211Mul(axis, 1.0f / axisLen);
    }

    const float angle = std::acos(d);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float one = 1.0f - c;
    const float x = axis.x, y = axis.y, z = axis.z;

    out[0] = c + x*x*one;
    out[1] = x*y*one - z*s;
    out[2] = x*z*one + y*s;
    out[3] = y*x*one + z*s;
    out[4] = c + y*y*one;
    out[5] = y*z*one - x*s;
    out[6] = z*x*one - y*s;
    out[7] = z*y*one + x*s;
    out[8] = c + z*z*one;
}

Vec3 Q211Rotate(const float r[9], Vec3 v) {
    return {
        r[0]*v.x + r[1]*v.y + r[2]*v.z,
        r[3]*v.x + r[4]*v.y + r[5]*v.z,
        r[6]*v.x + r[7]*v.y + r[8]*v.z
    };
}

Q211Delta Q211MakeDelta(
        Vec3 restPivot, Vec3 restDirection,
        Vec3 currentPivot, Vec3 currentDirection) {
    Q211Delta out;
    Q211RotationFromTo(restDirection, currentDirection, out.r);
    out.t = Q211Sub(
        currentPivot, Q211Rotate(out.r, restPivot));
    out.stretchPivot = restPivot;
    out.stretchAxis = Q211NormalizeSafe(restDirection);
    out.active = true;
    return out;
}

Q211Delta Q218MakeSegmentDelta(
        Vec3 restPivot, Vec3 restDirection,
        Vec3 currentPivot, Vec3 currentDirection,
        float axialScale) {
    Q211Delta out = Q211MakeDelta(
        restPivot, restDirection, currentPivot, currentDirection);
    out.axialScale = std::max(0.01f, axialScale);
    return out;
}

void Q218AxisRotation(Vec3 axis, float angle, float out[9]) {
    axis = Q211NormalizeSafe(axis);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float one = 1.0f - c;
    const float x = axis.x, y = axis.y, z = axis.z;
    out[0] = c + x*x*one;
    out[1] = x*y*one - z*s;
    out[2] = x*z*one + y*s;
    out[3] = y*x*one + z*s;
    out[4] = c + y*y*one;
    out[5] = y*z*one - x*s;
    out[6] = z*x*one - y*s;
    out[7] = z*y*one + x*s;
    out[8] = c + z*z*one;
}

Q211Delta Q218MakePivotRotation(Vec3 pivot, Vec3 axis, float angle) {
    Q211Delta out;
    Q218AxisRotation(axis, angle, out.r);
    out.t = Q211Sub(pivot, Q211Rotate(out.r, pivot));
    out.stretchPivot = pivot;
    out.active = true;
    return out;
}

Q211Delta Q218ComposeRigid(const Q211Delta& first, const Q211Delta& second) {
    if (!first.active) return second;
    if (!second.active) return first;
    Q211Delta out;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out.r[row*3 + col] =
                second.r[row*3 + 0] * first.r[0*3 + col] +
                second.r[row*3 + 1] * first.r[1*3 + col] +
                second.r[row*3 + 2] * first.r[2*3 + col];
        }
    }
    out.t = Q211Add(Q211Rotate(second.r, first.t), second.t);
    out.active = true;
    return out;
}

Vec3 Q211ApplyDelta(const Q211Delta& d, Vec3 p) {
    if (!d.active) return p;
    if (std::fabs(d.axialScale - 1.0f) > 0.0001f) {
        Vec3 rel = Q211Sub(p, d.stretchPivot);
        const float along = Q211Dot(rel, d.stretchAxis);
        rel = Q211Add(
            rel,
            Q211Mul(d.stretchAxis, along * (d.axialScale - 1.0f)));
        p = Q211Add(d.stretchPivot, rel);
    }
    return Q211Add(Q211Rotate(d.r, p), d.t);
}

Vec3 Q211ApplyDeltaVector(const Q211Delta& d, Vec3 v) {
    // Rotate normals/tangents with the segment but do not axially stretch them.
    return d.active ? Q211Rotate(d.r, v) : v;
}

bool Q220BasisRotation(
        Vec3 currentAcross, Vec3 currentIntoPalm,
        Vec3 targetAcross, Vec3 targetIntoPalm,
        float out[9]) {
    currentAcross = Q211NormalizeSafe(currentAcross);
    currentIntoPalm = Q211Sub(
        currentIntoPalm,
        Q211Mul(currentAcross, Q211Dot(currentIntoPalm, currentAcross)));
    targetAcross = Q211NormalizeSafe(targetAcross);
    targetIntoPalm = Q211Sub(
        targetIntoPalm,
        Q211Mul(targetAcross, Q211Dot(targetIntoPalm, targetAcross)));

    if (Q211Length(currentIntoPalm) < 0.03f ||
        Q211Length(targetIntoPalm) < 0.03f) {
        return false;
    }

    currentIntoPalm = Q211NormalizeSafe(currentIntoPalm);
    targetIntoPalm = Q211NormalizeSafe(targetIntoPalm);
    const Vec3 currentSide =
        Q211NormalizeSafe(Q211Cross(currentAcross, currentIntoPalm));
    const Vec3 targetSide =
        Q211NormalizeSafe(Q211Cross(targetAcross, targetIntoPalm));

    const Vec3 currentBasis[3]{
        currentAcross, currentIntoPalm, currentSide};
    const Vec3 targetBasis[3]{
        targetAcross, targetIntoPalm, targetSide};

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const float t0 = row == 0 ? targetBasis[0].x
                           : row == 1 ? targetBasis[0].y
                                      : targetBasis[0].z;
            const float t1 = row == 0 ? targetBasis[1].x
                           : row == 1 ? targetBasis[1].y
                                      : targetBasis[1].z;
            const float t2 = row == 0 ? targetBasis[2].x
                           : row == 1 ? targetBasis[2].y
                                      : targetBasis[2].z;
            const float c0 = col == 0 ? currentBasis[0].x
                           : col == 1 ? currentBasis[0].y
                                      : currentBasis[0].z;
            const float c1 = col == 0 ? currentBasis[1].x
                           : col == 1 ? currentBasis[1].y
                                      : currentBasis[1].z;
            const float c2 = col == 0 ? currentBasis[2].x
                           : col == 1 ? currentBasis[2].y
                                      : currentBasis[2].z;
            out[row * 3 + col] = t0*c0 + t1*c1 + t2*c2;
        }
    }
    return true;
}

Q211Delta Q220MakePivotRotationMatrix(
        Vec3 pivot, const float rotation[9]) {
    Q211Delta out;
    std::copy(rotation, rotation + 9, out.r);
    out.t = Q211Sub(pivot, Q211Rotate(out.r, pivot));
    out.stretchPivot = pivot;
    out.active = true;
    return out;
}

bool Q218ParseDigitBoneName(
        const std::string& authoredName,
        bool left,
        int& outChain,
        int& outDepth,
        bool& outThumb) {
    const std::string lower = Q211Lower(authoredName);
    const std::string sideToken =
        std::string("bip01 ") + (left ? "l " : "r ");
    if (lower.find(sideToken) == std::string::npos) return false;

    outChain = -1;
    outDepth = 0;
    outThumb = false;

    const size_t thumbAt = lower.find("thumb");
    if (thumbAt != std::string::npos) {
        outThumb = true;
        size_t digitAt = thumbAt + 5u;
        while (digitAt < lower.size() &&
               (lower[digitAt] < '0' || lower[digitAt] > '9')) {
            ++digitAt;
        }
        int depth = 0;
        bool have = false;
        while (digitAt < lower.size() &&
               lower[digitAt] >= '0' && lower[digitAt] <= '9') {
            depth = depth * 10 + static_cast<int>(lower[digitAt] - '0');
            have = true;
            ++digitAt;
        }
        outDepth = have ? depth : 0;
        return true;
    }

    const size_t fingerAt = lower.find("finger");
    if (fingerAt == std::string::npos) return false;
    size_t digitAt = fingerAt + 6u;
    while (digitAt < lower.size() &&
           (lower[digitAt] < '0' || lower[digitAt] > '9')) {
        ++digitAt;
    }
    if (digitAt >= lower.size()) return false;

    std::string digits;
    while (digitAt < lower.size() &&
           lower[digitAt] >= '0' && lower[digitAt] <= '9') {
        digits.push_back(lower[digitAt]);
        ++digitAt;
    }
    if (digits.empty()) return false;

    outChain = static_cast<int>(digits[0] - '0');
    outThumb = outChain == 0;
    if (digits.size() > 1u) {
        int depth = 0;
        for (size_t i = 1u; i < digits.size(); ++i)
            depth = depth * 10 + static_cast<int>(digits[i] - '0');
        outDepth = depth;
    } else {
        outDepth = 0;
    }
    return true;
}

bool Q217EnsureFingerRig(
        bool left,
        const Q220HandBasis& basis) {
    Q217FingerRig& rig = gQ217FingerRig[left ? 0 : 1];
    if (rig.attempted) return rig.ready;
    rig.attempted = true;

    if (!basis.ready) return false;

    std::string handName;
    if (!Q221FindGlobalBonePoint(
            left, "hand", rig.handPoint, handName)) {
        return false;
    }

    Vec3 middlePoint{};
    std::string middleName;
    if (!Q221FindGlobalBonePoint(
            left, "finger2", middlePoint, middleName)) {
        return false;
    }
    rig.fingerForward = Q211NormalizeSafe(
        Q211Sub(middlePoint, rig.handPoint));
    if (Q211Length(rig.fingerForward) < 0.03f) return false;

    std::unordered_map<int, std::vector<Q217FingerJoint>> byChain;
    std::unordered_set<std::string> seen;

    for (const Q211PlayerRigPart& part : gQ211PlayerRigParts) {
        for (const Fo3NifSkinBone& bone : part.bones) {
            int chain = -1;
            int depth = 0;
            bool thumb = false;
            if (!Q218ParseDigitBoneName(
                    bone.name, left, chain, depth, thumb)) {
                continue;
            }

            const std::string lower = Q211Lower(bone.name);
            if (!seen.insert(lower).second) continue;

            Q217FingerJoint joint;
            joint.name = lower;
            joint.pivot = Q211BindBonePoint(bone);
            joint.chain = chain;
            joint.depth = depth;
            joint.thumb = thumb;
            if (thumb) rig.thumb.push_back(joint);
            else byChain[chain].push_back(joint);
        }
    }

    for (auto& entry : byChain) {
        Q217FingerChain chain;
        chain.id = entry.first;
        chain.joints = std::move(entry.second);
        std::sort(
            chain.joints.begin(), chain.joints.end(),
            [&](const Q217FingerJoint& a,
                const Q217FingerJoint& b) {
                if (a.depth != b.depth) return a.depth < b.depth;
                return Q211Length(Q211Sub(a.pivot, rig.handPoint)) <
                       Q211Length(Q211Sub(b.pivot, rig.handPoint));
            });
        rig.chains.push_back(std::move(chain));
    }

    std::sort(
        rig.thumb.begin(), rig.thumb.end(),
        [&](const Q217FingerJoint& a,
            const Q217FingerJoint& b) {
            if (a.depth != b.depth) return a.depth < b.depth;
            return Q211Length(Q211Sub(a.pivot, rig.handPoint)) <
                   Q211Length(Q211Sub(b.pivot, rig.handPoint));
        });

    // Choose the most thumbward authored non-thumb chain as the index finger.
    // No FingerN number is assumed to mean index.
    float bestAcross = -1.0e30f;
    for (const Q217FingerChain& chain : rig.chains) {
        if (chain.joints.empty()) continue;
        const float across = Q211Dot(
            Q211Sub(chain.joints.front().pivot, rig.handPoint),
            basis.littleToThumb);
        if (across > bestAcross) {
            bestAcross = across;
            rig.indexChain = chain.id;
        }
    }

    // Q21.18 no longer needs a mirrored curl sign. Each joint axis is built
    // from its own authored segment direction x palm-normal, so positive
    // rotation bends that segment toward the palm on both hands.
    rig.curlSign = 1.0f;

    rig.ready = rig.indexChain >= 0 && !rig.chains.empty();

    std::string summary;
    for (const Q217FingerChain& chain : rig.chains) {
        if (!summary.empty()) summary += " ";
        summary += "F";
        summary += std::to_string(chain.id);
        summary += chain.id == rig.indexChain ? "[INDEX]={" : "={";
        for (size_t i = 0u; i < chain.joints.size(); ++i) {
            if (i) summary += ",";
            summary += chain.joints[i].name;
        }
        summary += "}";
    }
    if (!rig.thumb.empty()) {
        summary += " THUMB={";
        for (size_t i = 0u; i < rig.thumb.size(); ++i) {
            if (i) summary += ",";
            summary += rig.thumb[i].name;
        }
        summary += "}";
    }
    rig.summary = summary;

    Q6H_LOGI("Q21.20 FINGER MAP: side=%s ready=%d indexChain=%d chains=%zu thumbBones=%zu order=authored-numeric-suffix bendAxis=segment-cross-palm bones=%s",
             left ? "L" : "R",
             rig.ready ? 1 : 0,
             rig.indexChain,
             rig.chains.size(),
             rig.thumb.size(),
             rig.summary.empty() ? "<none>" : rig.summary.c_str());
    return rig.ready;
}

std::unordered_map<std::string, Q211Delta> Q217BuildFingerPose(
        bool left,
        const Q220HandBasis& basis,
        float trigger,
        bool triggerTouched,
        float grip,
        bool thumbTouched) {
    std::unordered_map<std::string, Q211Delta> out;
    if (!Q217EnsureFingerRig(left, basis)) return out;

    Q217FingerRig& rig = gQ217FingerRig[left ? 0 : 1];
    out.reserve(
        rig.thumb.size() +
        [&]() {
            size_t n = 0u;
            for (const Q217FingerChain& c : rig.chains)
                n += c.joints.size();
            return n;
        }());

    const float indexCurl = std::clamp(
        std::max(trigger, triggerTouched ? 0.16f : 0.0f),
        0.0f, 1.0f);
    const float gripCurl = std::clamp(grip, 0.0f, 1.0f);

    constexpr float DEG = 0.01745329251994329577f;
    const float jointDegrees[3]{55.0f, 45.0f, 35.0f};

    for (const Q217FingerChain& chain : rig.chains) {
        const float curl =
            chain.id == rig.indexChain ? indexCurl : gripCurl;
        if (curl <= 0.0001f) continue;

        Q211Delta cumulative;
        for (size_t i = 0u; i < chain.joints.size(); ++i) {
            const Q217FingerJoint& joint = chain.joints[i];

            Vec3 segmentDirection = rig.fingerForward;
            if (i + 1u < chain.joints.size()) {
                segmentDirection = Q211Sub(
                    chain.joints[i + 1u].pivot,
                    joint.pivot);
            } else if (i > 0u) {
                segmentDirection = Q211Sub(
                    joint.pivot,
                    chain.joints[i - 1u].pivot);
            }
            if (Q211Length(segmentDirection) < 0.005f)
                segmentDirection = rig.fingerForward;
            segmentDirection = Q211NormalizeSafe(
                segmentDirection, rig.fingerForward);

            // Q21.19 headset verification showed the authored palm normal used
            // by the wrist basis has the opposite sign for finger flexion.
            // Keep the per-joint authored axis/order from Q21.18, but invert
            // only the four finger-chain bend angles. Thumb opposition below
            // is already correct and remains untouched.
            Vec3 restAxis =
                Q211Cross(segmentDirection, basis.intoPalm);
            if (Q211Length(restAxis) < 0.005f)
                restAxis = basis.littleToThumb;
            restAxis = Q211NormalizeSafe(
                restAxis, basis.littleToThumb);

            const Vec3 pivot = cumulative.active
                ? Q211ApplyDelta(cumulative, joint.pivot)
                : joint.pivot;
            const Vec3 axis = cumulative.active
                ? Q211NormalizeSafe(
                      Q211ApplyDeltaVector(cumulative, restAxis),
                      restAxis)
                : restAxis;
            const float degrees =
                jointDegrees[std::min<size_t>(i, 2u)];
            const Q211Delta bend = Q218MakePivotRotation(
                pivot,
                axis,
                -curl * degrees * DEG);
            cumulative = Q218ComposeRigid(cumulative, bend);
            out[joint.name] = cumulative;
        }
    }

    // Thumb opposition is a VR-specific controller pose. Its axis and sign are
    // still derived from Fallout's authored thumb position and palm frame.
    if (thumbTouched && !rig.thumb.empty()) {
        Vec3 thumbDirection{};
        if (rig.thumb.size() >= 2u) {
            thumbDirection = Q211Sub(
                rig.thumb[1].pivot, rig.thumb[0].pivot);
        } else {
            thumbDirection = Q211Sub(
                rig.thumb[0].pivot, rig.handPoint);
        }

        if (Q211Length(thumbDirection) > 0.01f) {
            thumbDirection = Q211NormalizeSafe(thumbDirection);
            const Vec3 targetDirection = Q211NormalizeSafe(
                Q211Add(
                    Q211Mul(basis.littleToThumb, -1.0f),
                    Q211Mul(basis.intoPalm, 0.35f)));
            Vec3 thumbAxis =
                Q211Cross(thumbDirection, targetDirection);
            if (Q211Length(thumbAxis) > 0.01f) {
                thumbAxis = Q211NormalizeSafe(thumbAxis);
                // Q21.20: thumb contact should flex from the thumb knuckle,
                // not fold the whole thumb down from its hand-side base.
                // Leave authored thumb[0] attached to the hand, start at the
                // first actual knuckle, and cap the total opposition more
                // gently than Q21.17-Q21.19.
                const float targetAngle = std::min(
                    std::acos(std::clamp(
                        Q211Dot(thumbDirection, targetDirection),
                        -1.0f, 1.0f)),
                    28.0f * DEG);

                Q211Delta cumulative;
                for (size_t i = 1u; i < rig.thumb.size(); ++i) {
                    const Q217FingerJoint& joint = rig.thumb[i];
                    const Vec3 pivot = cumulative.active
                        ? Q211ApplyDelta(cumulative, joint.pivot)
                        : joint.pivot;
                    const Vec3 axis = cumulative.active
                        ? Q211ApplyDeltaVector(cumulative, thumbAxis)
                        : thumbAxis;
                    const float weight =
                        i == 1u ? 0.70f : 0.30f;
                    const Q211Delta bend = Q218MakePivotRotation(
                        pivot, axis, targetAngle * weight);
                    cumulative = Q218ComposeRigid(
                        cumulative, bend);
                    out[joint.name] = cumulative;
                }
            }
        }
    }

    return out;
}

float Q218WrapAngle(float angle) {
    constexpr float TWO_PI = 6.28318530718f;
    while (angle > 3.14159265359f) angle -= TWO_PI;
    while (angle < -3.14159265359f) angle += TWO_PI;
    return angle;
}

bool Q211SolveArm(
        bool left,
        Vec3 shoulder, Vec3 restElbow, Vec3 restHand,
        Vec3 target,
        float lengthScale,
        Vec3& outElbow,
        Vec3& outHand) {
    const float upperLen =
        Q211Length(Q211Sub(restElbow, shoulder)) * lengthScale;
    const float foreLen =
        Q211Length(Q211Sub(restHand, restElbow)) * lengthScale;
    if (upperLen < 0.05f || foreLen < 0.05f) return false;

    Vec3 toTarget = Q211Sub(target, shoulder);
    float dist = Q211Length(toTarget);
    if (dist < 0.04f) return false;
    const Vec3 dir = Q211Mul(toTarget, 1.0f / dist);

    const float maxReach = std::max(0.05f, upperLen + foreLen - 0.015f);
    const float minReach =
        std::max(0.03f, std::fabs(upperLen - foreLen) + 0.01f);
    dist = std::clamp(dist, minReach, maxReach);
    outHand = Q211Add(shoulder, Q211Mul(dir, dist));

    Vec3 preferred{
        left ? -0.75f : 0.75f,
        -0.30f,
        0.35f
    };
    preferred = Q211Sub(
        preferred, Q211Mul(dir, Q211Dot(preferred, dir)));
    if (Q211Length(preferred) < 0.05f) {
        preferred = Q211Cross(
            dir, std::fabs(dir.y) < 0.8f
                ? Vec3{0.0f, 1.0f, 0.0f}
                : Vec3{1.0f, 0.0f, 0.0f});
    }
    preferred = Q211NormalizeSafe(preferred);

    const float along =
        (upperLen*upperLen + dist*dist - foreLen*foreLen) /
        std::max(0.0001f, 2.0f * dist);
    const float sideSq =
        std::max(0.0f, upperLen*upperLen - along*along);
    const float side = std::sqrt(sideSq);
    outElbow = Q211Add(
        Q211Add(shoulder, Q211Mul(dir, along)),
        Q211Mul(preferred, side));
    return true;
}

struct Q213ArmPose {
    Q211Delta upper;
    Q211Delta fore;
    Q211Delta handDelta;
    Vec3 elbow{};
    Vec3 hand{};
    float restReach = 0.0f;
    float targetDistance = 0.0f;
    float wristTwist = 0.0f;
    bool solved = false;
};

bool Q219MeasureArmReach(
        const Q211PlayerRigPart& part,
        bool left,
        Vec3 target,
        float& outRestReach,
        float& outTargetDistance) {
    const int upper = left ? part.leftUpperArm : part.rightUpperArm;
    const int fore = left ? part.leftForearm : part.rightForearm;
    const int hand = left ? part.leftHand : part.rightHand;
    if (upper < 0 || fore < 0 || hand < 0) return false;

    const Vec3 shoulder = Q211BindBonePoint(part.bones[upper]);
    const Vec3 restElbow = Q211BindBonePoint(part.bones[fore]);
    Vec3 restHand = Q211BindBonePoint(part.bones[hand]);
    const bool palmAnchorValid =
        left ? part.leftPalmAnchorValid : part.rightPalmAnchorValid;
    if (palmAnchorValid) {
        restHand = left ? part.leftPalmAnchor : part.rightPalmAnchor;
    }

    outRestReach =
        Q211Length(Q211Sub(restElbow, shoulder)) +
        Q211Length(Q211Sub(restHand, restElbow));
    outTargetDistance = Q211Length(Q211Sub(target, shoulder));
    return outRestReach > 0.05f &&
           std::isfinite(outRestReach) &&
           std::isfinite(outTargetDistance);
}

Q213ArmPose Q213SolveMasterArm(
        const Q211PlayerRigPart& part,
        bool left,
        Vec3 target,
        const Q220HandBasis& authoredHandBasis,
        Vec3 gripLittleToThumb,
        Vec3 gripIntoPalm,
        bool gripOrientationValid,
        float armLengthScale) {
    Q213ArmPose pose;
    const int upper = left ? part.leftUpperArm : part.rightUpperArm;
    const int fore = left ? part.leftForearm : part.rightForearm;
    const int hand = left ? part.leftHand : part.rightHand;
    if (upper < 0 || fore < 0 || hand < 0) return pose;

    const Vec3 shoulder = Q211BindBonePoint(part.bones[upper]);
    const Vec3 restElbow = Q211BindBonePoint(part.bones[fore]);
    Vec3 restHand = Q211BindBonePoint(part.bones[hand]);
    const bool palmAnchorValid =
        left ? part.leftPalmAnchorValid : part.rightPalmAnchorValid;
    if (palmAnchorValid) {
        restHand = left ? part.leftPalmAnchor : part.rightPalmAnchor;
    }

    const Vec3 restUpper = Q211Sub(restElbow, shoulder);
    const Vec3 restFore = Q211Sub(restHand, restElbow);
    pose.restReach = Q211Length(restUpper) + Q211Length(restFore);
    pose.targetDistance = Q211Length(Q211Sub(target, shoulder));

    if (!Q211SolveArm(
            left, shoulder, restElbow, restHand,
            target, armLengthScale,
            pose.elbow, pose.hand)) {
        return pose;
    }

    pose.upper = Q218MakeSegmentDelta(
        shoulder, restUpper,
        shoulder, Q211Sub(pose.elbow, shoulder),
        armLengthScale);
    pose.fore = Q218MakeSegmentDelta(
        restElbow, restFore,
        pose.elbow, Q211Sub(pose.hand, pose.elbow),
        armLengthScale);

    // Keep the hand itself at authored size: move/rotate it with the stretched
    // forearm endpoint, then layer controller roll around the current wrist axis.
    pose.handDelta = Q211MakeDelta(
        restHand, restFore,
        pose.hand, Q211Sub(pose.hand, pose.elbow));

    if (gripOrientationValid && authoredHandBasis.ready) {
        const Vec3 currentLittleToThumb =
            Q211ApplyDeltaVector(
                pose.handDelta, authoredHandBasis.littleToThumb);
        const Vec3 currentIntoPalm =
            Q211ApplyDeltaVector(
                pose.handDelta, authoredHandBasis.intoPalm);

        float wristRotation[9]{};
        if (Q220BasisRotation(
                currentLittleToThumb, currentIntoPalm,
                gripLittleToThumb, gripIntoPalm,
                wristRotation)) {
            const float trace =
                wristRotation[0] + wristRotation[4] + wristRotation[8];
            pose.wristTwist = std::acos(
                std::clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f));
            const Q211Delta wrist =
                Q220MakePivotRotationMatrix(
                    pose.hand, wristRotation);
            pose.handDelta =
                Q218ComposeRigid(pose.handDelta, wrist);
        }
    }

    pose.solved = true;
    return pose;
}

void Q213AssignArmPoseToPart(
        const Q211PlayerRigPart& part,
        const Q213ArmPose& left,
        const Q213ArmPose& right,
        std::vector<Q211Delta>& deltas) {
    for (size_t i = 0u; i < part.bones.size(); ++i) {
        const int role = Q211BoneRole(part.bones[i].name);
        if (left.solved) {
            if (role == 1) deltas[i] = left.upper;
            else if (role == 2) deltas[i] = left.fore;
            else if (role == 3) deltas[i] = left.handDelta;
        }
        if (right.solved) {
            if (role == 4) deltas[i] = right.upper;
            else if (role == 5) deltas[i] = right.fore;
            else if (role == 6) deltas[i] = right.handDelta;
        }
    }
}

void Q211BuildArmDeltas(
        const Q211PlayerRigPart& part,
        bool left,
        Vec3 target,
        std::vector<Q211Delta>& deltas,
        Vec3& outElbow,
        bool& solved) {
    const int upper = left ? part.leftUpperArm : part.rightUpperArm;
    const int fore = left ? part.leftForearm : part.rightForearm;
    const int hand = left ? part.leftHand : part.rightHand;
    solved = false;
    if (upper < 0 || fore < 0 || hand < 0) return;

    const Vec3 shoulder = Q211BindBonePoint(part.bones[upper]);
    const Vec3 restElbow = Q211BindBonePoint(part.bones[fore]);

    // Q21.1D: use the actual weighted hand/finger vertex cloud as the distal
    // endpoint. Some FO3 hand bone bind transforms point opposite the visible
    // forearm geometry; using that pivot folded the forearm back toward the
    // bicep even though the shoulder/elbow solve itself was correct.
    Vec3 restHand = Q211BindBonePoint(part.bones[hand]);
    const bool palmAnchorValid =
        left ? part.leftPalmAnchorValid
             : part.rightPalmAnchorValid;
    if (palmAnchorValid) {
        restHand = left ? part.leftPalmAnchor
                        : part.rightPalmAnchor;
    }

    Vec3 currentHand{};
    if (!Q211SolveArm(
            left, shoulder, restElbow, restHand,
            target, 1.0f, outElbow, currentHand)) {
        return;
    }

    const Q211Delta upperDelta = Q211MakeDelta(
        shoulder, Q211Sub(restElbow, shoulder),
        shoulder, Q211Sub(outElbow, shoulder));
    const Q211Delta foreDelta = Q211MakeDelta(
        restElbow, Q211Sub(restHand, restElbow),
        outElbow, Q211Sub(currentHand, outElbow));

    for (size_t i = 0u; i < part.bones.size(); ++i) {
        const int role = Q211BoneRole(part.bones[i].name);
        if (left) {
            if (role == 1) deltas[i] = upperDelta;
            else if (role == 2 || role == 3) deltas[i] = foreDelta;
        } else {
            if (role == 4) deltas[i] = upperDelta;
            else if (role == 5 || role == 6) deltas[i] = foreDelta;
        }
    }
    solved = true;
}

void Q211UpdatePlayerRig() {
    if (gQ211LastSkinnedSerial == gQ211TrackingSerial) return;
    gQ211LastSkinnedSerial = gQ211TrackingSerial;
    if (gQ211PlayerRigParts.empty()) return;

    float invRoot[16]{};
    if (!Q2016InvertAffine(gQ210PlayerRoot, invRoot)) return;

    const Vec3 headWorld{
        gQ210Head[0], gQ210Head[1], gQ210Head[2]};
    const Vec3 leftTargetWorld{
        gQ210LeftHand[0], gQ210LeftHand[1], gQ210LeftHand[2]};
    const Vec3 rightTargetWorld{
        gQ210RightHand[0], gQ210RightHand[1], gQ210RightHand[2]};

    const Vec3 trackedHeadRoot = Q211TransformPoint(invRoot, headWorld);
    const Vec3 trackedLeftRoot =
        Q211TransformPoint(invRoot, leftTargetWorld);
    const Vec3 trackedRightRoot =
        Q211TransformPoint(invRoot, rightTargetWorld);

    // Q21.11: match the actual OpenXR grip basis exactly.
    // +X is away from the left palm / into the right palm.
    // -Z is the across-hand direction from little finger to thumb.
    const Vec3 leftIntoPalmWorld =
        Q218RotateQuaternion(gQ218LeftHandQuat, Vec3{-1.0f, 0.0f, 0.0f});
    const Vec3 rightIntoPalmWorld =
        Q218RotateQuaternion(gQ218RightHandQuat, Vec3{1.0f, 0.0f, 0.0f});
    const Vec3 leftAcrossWorld =
        Q218RotateQuaternion(gQ218LeftHandQuat, Vec3{0.0f, 0.0f, -1.0f});
    const Vec3 rightAcrossWorld =
        Q218RotateQuaternion(gQ218RightHandQuat, Vec3{0.0f, 0.0f, -1.0f});

    const Vec3 leftPalmRoot =
        Q211NormalizeSafe(Q218TransformVector(invRoot, leftIntoPalmWorld));
    const Vec3 rightPalmRoot =
        Q211NormalizeSafe(Q218TransformVector(invRoot, rightIntoPalmWorld));
    const Vec3 leftAcrossRoot =
        Q211NormalizeSafe(Q218TransformVector(invRoot, leftAcrossWorld));
    const Vec3 rightAcrossRoot =
        Q211NormalizeSafe(Q218TransformVector(invRoot, rightAcrossWorld));

    Q220HandBasis q220LeftAuthoredBasis;
    Q220HandBasis q220RightAuthoredBasis;
    const bool q220LeftBasisReady =
        Q220FindAuthoredHandBasis(true, q220LeftAuthoredBasis);
    const bool q220RightBasisReady =
        Q220FindAuthoredHandBasis(false, q220RightAuthoredBasis);

    Vec3 avatarHeadAnchor = trackedHeadRoot;
    const bool avatarHeadReady =
        Q211FindAvatarHeadAnchor(avatarHeadAnchor);

    // Q21.1B: Quest LOCAL space is session/HMD-relative, while Fallout's
    // bones are actor-model-relative. Map the physical controller offset from
    // the HMD onto Fallout's authored head/neck anchor. Feeding the raw LOCAL
    // Y directly made targets ~0.4-0.8 m below the actor and pulled the
    // forearm/hand geometry out of view.
    Vec3 leftTarget = Q211Add(
        avatarHeadAnchor,
        Q211Sub(trackedLeftRoot, trackedHeadRoot));
    Vec3 rightTarget = Q211Add(
        avatarHeadAnchor,
        Q211Sub(trackedRightRoot, trackedHeadRoot));

    // Body-root X is the authored left/right axis (left < 0, right > 0).
    // Keep the calibration symmetric so it cannot skew the avatar.
    leftTarget.x -= Q214_HAND_OUTWARD_OFFSET;
    rightTarget.x += Q214_HAND_OUTWARD_OFFSET;

    // Q21.3: solve each arm once from a part that has the complete chain.
    // Gamebryo skin partitions/shapes often reference only a subset of the
    // shared skeleton. Requiring every individual shape to contain
    // UpperArm+Forearm+Hand left forearm-heavy partitions frozen in bind pose.
    const Q211PlayerRigPart* q213LeftMaster = nullptr;
    const Q211PlayerRigPart* q213RightMaster = nullptr;
    for (const Q211PlayerRigPart& candidate : gQ211PlayerRigParts) {
        if (!q213LeftMaster && candidate.leftChainReady)
            q213LeftMaster = &candidate;
        if (!q213RightMaster && candidate.rightChainReady)
            q213RightMaster = &candidate;
    }

    if ((gQ211TrackingSerial % 180u) == 1u) {
        Q6H_LOGI("Q21.16 IK MASTER: left=%s right=%s expected=characters\\_male\\upperbody.nif",
                 q213LeftMaster
                     ? q213LeftMaster->sourceModelPath.c_str()
                     : "<none>",
                 q213RightMaster
                     ? q213RightMaster->sourceModelPath.c_str()
                     : "<none>");
    }

    float q219RequestedScale = Q219_ARM_BASE_SCALE;
    float q219LRest = 0.0f, q219LDist = 0.0f;
    float q219RRest = 0.0f, q219RDist = 0.0f;
    if (gQ210LeftHandValid && q213LeftMaster &&
        Q219MeasureArmReach(
            *q213LeftMaster, true, leftTarget,
            q219LRest, q219LDist)) {
        q219RequestedScale = std::max(
            q219RequestedScale,
            q219LDist /
                std::max(
                    0.05f,
                    q219LRest * Q219_TARGET_EXTENSION_RATIO));
    }
    if (gQ210RightHandValid && q213RightMaster &&
        Q219MeasureArmReach(
            *q213RightMaster, false, rightTarget,
            q219RRest, q219RDist)) {
        q219RequestedScale = std::max(
            q219RequestedScale,
            q219RDist /
                std::max(
                    0.05f,
                    q219RRest * Q219_TARGET_EXTENSION_RATIO));
    }
    q219RequestedScale = std::clamp(
        q219RequestedScale,
        Q219_ARM_BASE_SCALE,
        Q219_ARM_MAX_SCALE);
    if (q219RequestedScale > gQ219ArmLengthScale) {
        gQ219ArmLengthScale = std::min(
            q219RequestedScale,
            gQ219ArmLengthScale + Q219_SCALE_GROW_PER_FRAME);
    }

    Q213ArmPose q213LeftPose;
    Q213ArmPose q213RightPose;
    if (gQ210LeftHandValid && q213LeftMaster) {
        q213LeftPose =
            Q213SolveMasterArm(
                *q213LeftMaster, true, leftTarget,
                q220LeftAuthoredBasis,
                leftAcrossRoot, leftPalmRoot,
                q220LeftBasisReady,
                gQ219ArmLengthScale);
    }
    if (gQ210RightHandValid && q213RightMaster) {
        q213RightPose =
            Q213SolveMasterArm(
                *q213RightMaster, false, rightTarget,
                q220RightAuthoredBasis,
                rightAcrossRoot, rightPalmRoot,
                q220RightBasisReady,
                gQ219ArmLengthScale);
    }

    const std::unordered_map<std::string, Q211Delta> q217LeftFingerPose =
        q213LeftPose.solved
            ? Q217BuildFingerPose(
                  true, q220LeftAuthoredBasis,
                  gQ217FingerTrigger[0], gQ217TriggerTouched[0],
                  gQ217FingerGrip[0], gQ217ThumbTouched[0])
            : std::unordered_map<std::string, Q211Delta>{};
    const std::unordered_map<std::string, Q211Delta> q217RightFingerPose =
        q213RightPose.solved
            ? Q217BuildFingerPose(
                  false, q220RightAuthoredBasis,
                  gQ217FingerTrigger[1], gQ217TriggerTouched[1],
                  gQ217FingerGrip[1], gQ217ThumbTouched[1])
            : std::unordered_map<std::string, Q211Delta>{};

    size_t q213LeftAffectedParts = 0u;
    size_t q213RightAffectedParts = 0u;

    constexpr size_t STRIDE = 18u;
    for (Q211PlayerRigPart& part : gQ211PlayerRigParts) {
        if (part.gpuIndex >= gQ210PlayerBody.size() ||
            part.bindExpanded.empty() ||
            part.bindExpanded.size() != part.workExpanded.size()) {
            continue;
        }

        std::vector<Q211Delta> deltas(part.bones.size());
        Q213AssignArmPoseToPart(
            part, q213LeftPose, q213RightPose, deltas);

        // Layer authored-finger-space curls before the already-solved hand
        // transform. This preserves the Q21.16 wrist/arm solution.
        for (size_t boneIndex = 0u;
             boneIndex < part.bones.size();
             ++boneIndex) {
            const std::string lower =
                Q211Lower(part.bones[boneIndex].name);
            const auto leftFinger =
                q217LeftFingerPose.find(lower);
            if (leftFinger != q217LeftFingerPose.end()) {
                deltas[boneIndex] = Q218ComposeRigid(
                    leftFinger->second, deltas[boneIndex]);
                continue;
            }
            const auto rightFinger =
                q217RightFingerPose.find(lower);
            if (rightFinger != q217RightFingerPose.end()) {
                deltas[boneIndex] = Q218ComposeRigid(
                    rightFinger->second, deltas[boneIndex]);
            }
        }

        bool q213PartHasLeftArm = false;
        bool q213PartHasRightArm = false;
        for (const Fo3NifSkinBone& bone : part.bones) {
            const int role = Q211BoneRole(bone.name);
            q213PartHasLeftArm =
                q213PartHasLeftArm ||
                role == 1 || role == 2 || role == 3;
            q213PartHasRightArm =
                q213PartHasRightArm ||
                role == 4 || role == 5 || role == 6;
        }
        if (q213LeftPose.solved && q213PartHasLeftArm)
            ++q213LeftAffectedParts;
        if (q213RightPose.solved && q213PartHasRightArm)
            ++q213RightAffectedParts;

        std::copy(
            part.bindExpanded.begin(), part.bindExpanded.end(),
            part.workExpanded.begin());

        const size_t expandedVertices = part.bindExpanded.size() / STRIDE;
        if (part.expandedBoneIndices.size() != expandedVertices * 4u ||
            part.expandedBoneWeights.size() != expandedVertices * 4u) {
            continue;
        }

        for (size_t v = 0u; v < expandedVertices; ++v) {
            const size_t base = v * STRIDE;
            const Vec3 bindP{
                part.bindExpanded[base + 0u],
                part.bindExpanded[base + 1u],
                part.bindExpanded[base + 2u]};
            const Vec3 bindN{
                part.bindExpanded[base + 3u],
                part.bindExpanded[base + 4u],
                part.bindExpanded[base + 5u]};
            const Vec3 bindT{
                part.bindExpanded[base + 6u],
                part.bindExpanded[base + 7u],
                part.bindExpanded[base + 8u]};
            const Vec3 bindB{
                part.bindExpanded[base + 9u],
                part.bindExpanded[base + 10u],
                part.bindExpanded[base + 11u]};

            Vec3 p{0,0,0}, n{0,0,0}, t{0,0,0}, b{0,0,0};
            float sum = 0.0f;
            for (size_t slot = 0u; slot < 4u; ++slot) {
                const size_t at = v * 4u + slot;
                const uint16_t bone = part.expandedBoneIndices[at];
                const float weight = part.expandedBoneWeights[at];
                if (weight <= 0.000001f || bone >= deltas.size()) continue;
                p = Q211Add(p, Q211Mul(
                    Q211ApplyDelta(deltas[bone], bindP), weight));
                n = Q211Add(n, Q211Mul(
                    Q211ApplyDeltaVector(deltas[bone], bindN), weight));
                t = Q211Add(t, Q211Mul(
                    Q211ApplyDeltaVector(deltas[bone], bindT), weight));
                b = Q211Add(b, Q211Mul(
                    Q211ApplyDeltaVector(deltas[bone], bindB), weight));
                sum += weight;
            }
            if (sum < 0.999f) {
                const float remain = std::max(0.0f, 1.0f - sum);
                p = Q211Add(p, Q211Mul(bindP, remain));
                n = Q211Add(n, Q211Mul(bindN, remain));
                t = Q211Add(t, Q211Mul(bindT, remain));
                b = Q211Add(b, Q211Mul(bindB, remain));
            }

            n = Q211NormalizeSafe(n, bindN);
            t = Q211NormalizeSafe(t, bindT);
            b = Q211NormalizeSafe(b, bindB);

            part.workExpanded[base + 0u] = p.x;
            part.workExpanded[base + 1u] = p.y;
            part.workExpanded[base + 2u] = p.z;
            part.workExpanded[base + 3u] = n.x;
            part.workExpanded[base + 4u] = n.y;
            part.workExpanded[base + 5u] = n.z;
            part.workExpanded[base + 6u] = t.x;
            part.workExpanded[base + 7u] = t.y;
            part.workExpanded[base + 8u] = t.z;
            part.workExpanded[base + 9u] = b.x;
            part.workExpanded[base + 10u] = b.y;
            part.workExpanded[base + 11u] = b.z;
        }

        GpuObject& gpu = gQ210PlayerBody[part.gpuIndex];
        if (gpu.vbo != 0u) {
            glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
            glBufferSubData(
                GL_ARRAY_BUFFER, 0,
                static_cast<GLsizeiptr>(
                    part.workExpanded.size() * sizeof(float)),
                part.workExpanded.data());
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Q22.2: Q21's hand endpoint is an arm-retarget anchor and is slightly
    // proximal for VR gripping. Derive a separate interaction centre from the
    // visible hand/glove mesh's exact Bip01 Hand vertex weights, then carry
    // that point through the already-solved hand/wrist transform.
    Vec3 q222LeftGrabPalmRest{};
    Vec3 q222RightGrabPalmRest{};
    float q222LeftGrabPalmWeight = 0.0f;
    float q222RightGrabPalmWeight = 0.0f;
    std::string q222LeftGrabPalmSource;
    std::string q222RightGrabPalmSource;

    const bool q222LeftGrabAnchorReady =
        Q222FindVisibleGrabPalmAnchor(
            true,
            q222LeftGrabPalmRest,
            q222LeftGrabPalmWeight,
            q222LeftGrabPalmSource);
    const bool q222RightGrabAnchorReady =
        Q222FindVisibleGrabPalmAnchor(
            false,
            q222RightGrabPalmRest,
            q222RightGrabPalmWeight,
            q222RightGrabPalmSource);

    Vec3 q221LeftPalmWorld{};
    Vec3 q221RightPalmWorld{};
    const bool q221LeftPalmValid =
        gQ210LeftHandValid &&
        q213LeftPose.solved &&
        q222LeftGrabAnchorReady;
    const bool q221RightPalmValid =
        gQ210RightHandValid &&
        q213RightPose.solved &&
        q222RightGrabAnchorReady;

    if (q221LeftPalmValid) {
        const Vec3 posedPalmRoot =
            Q211ApplyDelta(
                q213LeftPose.handDelta,
                q222LeftGrabPalmRest);
        q221LeftPalmWorld =
            Q211TransformPoint(
                gQ210PlayerRoot,
                posedPalmRoot);
    }
    if (q221RightPalmValid) {
        const Vec3 posedPalmRoot =
            Q211ApplyDelta(
                q213RightPose.handDelta,
                q222RightGrabPalmRest);
        q221RightPalmWorld =
            Q211TransformPoint(
                gQ210PlayerRoot,
                posedPalmRoot);
    }

    Q221UpdateLooseObjectsFromSolvedPalms(
        q221LeftPalmValid, q221LeftPalmWorld,
        q221RightPalmValid, q221RightPalmWorld);

    if ((gQ211TrackingSerial % 180u) == 1u) {
        Q6H_LOGI("Q21.14 HAND BASIS: authored=(L%d,R%d) Lacross=(%.3f %.3f %.3f) Linward=(%.3f %.3f %.3f) Racross=(%.3f %.3f %.3f) Rinward=(%.3f %.3f %.3f) rightPalmMirror=1 outwardOffset=%.3fm source=global-authored-(Hand,Finger2,Finger4)",
                 q220LeftBasisReady ? 1 : 0,
                 q220RightBasisReady ? 1 : 0,
                 q220LeftAuthoredBasis.littleToThumb.x,
                 q220LeftAuthoredBasis.littleToThumb.y,
                 q220LeftAuthoredBasis.littleToThumb.z,
                 q220LeftAuthoredBasis.intoPalm.x,
                 q220LeftAuthoredBasis.intoPalm.y,
                 q220LeftAuthoredBasis.intoPalm.z,
                 q220RightAuthoredBasis.littleToThumb.x,
                 q220RightAuthoredBasis.littleToThumb.y,
                 q220RightAuthoredBasis.littleToThumb.z,
                 q220RightAuthoredBasis.intoPalm.x,
                 q220RightAuthoredBasis.intoPalm.y,
                 q220RightAuthoredBasis.intoPalm.z,
                 Q214_HAND_OUTWARD_OFFSET);
        Q6H_LOGI("Q22.5 PALM ANCHOR: L(valid=%d world=%.3f %.3f %.3f rest=%.3f %.3f %.3f weight=%.1f source=%s) R(valid=%d world=%.3f %.3f %.3f rest=%.3f %.3f %.3f weight=%.1f source=%s) mode=visible-exact-Hand-bone-weights",
                 q221LeftPalmValid ? 1 : 0,
                 q221LeftPalmWorld.x, q221LeftPalmWorld.y, q221LeftPalmWorld.z,
                 q222LeftGrabPalmRest.x, q222LeftGrabPalmRest.y, q222LeftGrabPalmRest.z,
                 q222LeftGrabPalmWeight,
                 q222LeftGrabPalmSource.empty()
                     ? "<none>"
                     : q222LeftGrabPalmSource.c_str(),
                 q221RightPalmValid ? 1 : 0,
                 q221RightPalmWorld.x, q221RightPalmWorld.y, q221RightPalmWorld.z,
                 q222RightGrabPalmRest.x, q222RightGrabPalmRest.y, q222RightGrabPalmRest.z,
                 q222RightGrabPalmWeight,
                 q222RightGrabPalmSource.empty()
                     ? "<none>"
                     : q222RightGrabPalmSource.c_str());
        Q6H_LOGI("Q21.20 FINGER INPUT: L(trigger=%.2f triggerTouch=%d grip=%.2f thumbTouch=%d posedBones=%zu) R(trigger=%.2f triggerTouch=%d grip=%.2f thumbTouch=%d posedBones=%zu) mapping=index=trigger lower3=squeeze thumb=capacitive",
                 gQ217FingerTrigger[0],
                 gQ217TriggerTouched[0] ? 1 : 0,
                 gQ217FingerGrip[0],
                 gQ217ThumbTouched[0] ? 1 : 0,
                 q217LeftFingerPose.size(),
                 gQ217FingerTrigger[1],
                 gQ217TriggerTouched[1] ? 1 : 0,
                 gQ217FingerGrip[1],
                 gQ217ThumbTouched[1] ? 1 : 0,
                 q217RightFingerPose.size());
        Q6H_LOGI("Q21.9 BODY/REACH: armScale=%.3f requested=%.3f base=%.3f max=%.3f targetExtension=%.2f measureL=(rest=%.3f dist=%.3f) measureR=(rest=%.3f dist=%.3f)",
                 gQ219ArmLengthScale, q219RequestedScale,
                 Q219_ARM_BASE_SCALE, Q219_ARM_MAX_SCALE,
                 Q219_TARGET_EXTENSION_RATIO,
                 q219LRest, q219LDist, q219RRest, q219RDist);
        Q6H_LOGI("Q21.8 ARM RETARGET: scale=%.3f L(rest=%.3f scaled=%.3f targetDist=%.3f ratio=%.3f twistDeg=%.1f) R(rest=%.3f scaled=%.3f targetDist=%.3f ratio=%.3f twistDeg=%.1f) wristMode=authored-hand-finger2-4-to-openxr-grip",
                 gQ219ArmLengthScale,
                 q213LeftPose.restReach,
                 q213LeftPose.restReach * gQ219ArmLengthScale,
                 q213LeftPose.targetDistance,
                 q213LeftPose.restReach > 0.001f
                     ? q213LeftPose.targetDistance /
                           (q213LeftPose.restReach * gQ219ArmLengthScale)
                     : 0.0f,
                 q213LeftPose.wristTwist * 57.2957795f,
                 q213RightPose.restReach,
                 q213RightPose.restReach * gQ219ArmLengthScale,
                 q213RightPose.targetDistance,
                 q213RightPose.restReach > 0.001f
                     ? q213RightPose.targetDistance /
                           (q213RightPose.restReach * gQ219ArmLengthScale)
                     : 0.0f,
                 q213RightPose.wristTwist * 57.2957795f);

        Q6H_LOGI("Q21.5 ARM IK: serial=%llu rigParts=%zu masters=(L%d,R%d) affectedParts=(L%zu,R%zu) headAnchorReady=%d leftValid=%d leftSolved=%d targetL=(%.3f %.3f %.3f) elbowL=(%.3f %.3f %.3f) handL=(%.3f %.3f %.3f) rightValid=%d rightSolved=%d targetR=(%.3f %.3f %.3f) elbowR=(%.3f %.3f %.3f) handR=(%.3f %.3f %.3f) mode=global-skeleton-pose-across-skin-partitions",
                 static_cast<unsigned long long>(gQ211TrackingSerial),
                 gQ211PlayerRigParts.size(),
                 q213LeftMaster ? 1 : 0,
                 q213RightMaster ? 1 : 0,
                 q213LeftAffectedParts,
                 q213RightAffectedParts,
                 avatarHeadReady ? 1 : 0,
                 gQ210LeftHandValid ? 1 : 0,
                 q213LeftPose.solved ? 1 : 0,
                 leftTarget.x, leftTarget.y, leftTarget.z,
                 q213LeftPose.elbow.x,
                 q213LeftPose.elbow.y,
                 q213LeftPose.elbow.z,
                 q213LeftPose.hand.x,
                 q213LeftPose.hand.y,
                 q213LeftPose.hand.z,
                 gQ210RightHandValid ? 1 : 0,
                 q213RightPose.solved ? 1 : 0,
                 rightTarget.x, rightTarget.y, rightTarget.z,
                 q213RightPose.elbow.x,
                 q213RightPose.elbow.y,
                 q213RightPose.elbow.z,
                 q213RightPose.hand.x,
                 q213RightPose.hand.y,
                 q213RightPose.hand.z);
    }
}

void Q210DeletePlayerBody() {
    for (GpuObject& object : gQ210PlayerBody) {
        if (Q2017ReleaseSharedGeometry(object)) continue;
        if (object.vbo) glDeleteBuffers(1, &object.vbo);
        if (object.vao) glDeleteVertexArrays(1, &object.vao);
        object.vbo = 0u;
        object.vao = 0u;
    }
    gQ210PlayerBody.clear();
    gQ211PlayerRigParts.clear();
    gQ211LastSkinnedSerial = ~0ull;
    gQ219ArmLengthScale = Q219_ARM_BASE_SCALE;
    gQ213TorsoYawReady = false;
    gQ217FingerRig[0] = {};
    gQ217FingerRig[1] = {};
    Q220ResetGrabState();
    gQ210PlayerBodyReady = false;
}

bool Q215IsDismemberCap(uint16_t bodyPart) {
    return (bodyPart >= 101u && bodyPart <= 113u) ||
           (bodyPart >= 201u && bodyPart <= 213u);
}

size_t Q215SuppressPlayerGoreCaps(Fo3StaticNifMesh& mesh) {
    const size_t triangleCount = mesh.indices.size() / 3u;
    if (!mesh.dismemberSkin ||
        mesh.skinTriangleBodyParts.size() != triangleCount) {
        return 0u;
    }

    std::vector<uint32_t> keptIndices;
    std::vector<uint16_t> keptBodyParts;
    keptIndices.reserve(mesh.indices.size());
    keptBodyParts.reserve(triangleCount);

    size_t removed = 0u;
    for (size_t tri = 0u; tri < triangleCount; ++tri) {
        const uint16_t bodyPart = mesh.skinTriangleBodyParts[tri];
        if (Q215IsDismemberCap(bodyPart)) {
            ++removed;
            continue;
        }
        keptIndices.push_back(mesh.indices[tri * 3u + 0u]);
        keptIndices.push_back(mesh.indices[tri * 3u + 1u]);
        keptIndices.push_back(mesh.indices[tri * 3u + 2u]);
        keptBodyParts.push_back(bodyPart);
    }

    if (removed > 0u) {
        mesh.indices.swap(keptIndices);
        mesh.skinTriangleBodyParts.swap(keptBodyParts);
    }
    return removed;
}

void Q230DeleteNpcActors() {
    for (GpuObject& object : gQ230NpcActors) {
        if (object.vbo) glDeleteBuffers(1, &object.vbo);
        if (object.vao) glDeleteVertexArrays(1, &object.vao);
    }
    gQ230NpcActors.clear();
    gQ230NpcReady = false;
    gQ230NpcAttempted = false;
}

void Q230ConvertBethesdaRotation(
        float rx, float ry, float rz,
        float& outRx, float& outRy, float& outRz) {
    // Exact conversion used by the existing Wasteland placement loader.
    const float sx=std::sin(rx), cx=std::cos(rx);
    const float sy=std::sin(ry), cy=std::cos(ry);
    const float sz=std::sin(rz), cz=std::cos(rz);
    const float m00=cy*cz;
    const float m01=sz*cy;
    const float m10=sx*sy*cz-sz*cx;
    const float m11=sx*sy*sz+cx*cz;
    const float m20=sx*sz+sy*cx*cz;
    const float m21=-sx*cz+sy*sz*cx;
    const float m22=cx*cy;
    outRy=std::asin(std::clamp(-m20,-1.0f,1.0f));
    const float cosY=std::cos(outRy);
    if(std::fabs(cosY)>1.0e-5f){
        outRx=std::atan2(m21,m22);
        outRz=std::atan2(m10,m00);
    } else {
        outRx=0.0f;
        outRz=std::atan2(-m01,m11);
    }
}

bool Q230EnsureNpcActors() {
    if (gQ230NpcReady) return true;
    if (gQ230NpcAttempted) return false;
    if (!gExteriorStreamingActiveQ1890 ||
        gExteriorWorldspaceQ1890 != 0x00000A74u) return false;
    gQ230NpcAttempted = true;

    std::vector<Fo3NpcActorQ230> actors;
    if (!LoadFo3MegatonExteriorActorsQ230(actors)) {
        Q6H_LOGW("Q23.0 NPC RENDER MISS: stage=esm-resolve");
        return false;
    }

    const Fo3NpcActorQ230* lucas = nullptr;
    for (const Fo3NpcActorQ230& actor : actors) {
        if (actor.editorId == "LucasSimms") {
            lucas = &actor;
            break;
        }
    }
    if (!lucas) {
        Q6H_LOGW("Q23.0 NPC RENDER MISS: stage=LucasSimms-not-found");
        return false;
    }

    std::vector<std::string> models;
    std::vector<std::string> faceGenModels;
    auto samePath=[](const std::string& a,const std::string& b){
        return a.size()==b.size() && Q210EndsWithInsensitive(a,b);
    };
    auto addModel=[&](const std::string& path){
        if(path.empty()) return;
        for(const std::string& existing:models)
            if(samePath(existing,path)) return;
        models.push_back(path);
    };
    auto addFaceGenModel=[&](const std::string& path){
        if(path.empty()) return;
        addModel(path);
        for(const std::string& existing:faceGenModels)
            if(samePath(existing,path)) return;
        faceGenModels.push_back(path);
    };

    if(!lucas->raceHeadModels.empty()){
        for(const std::string& path:lucas->raceHeadModels)
            addFaceGenModel(path);
    } else {
        addFaceGenModel(lucas->raceHeadModel);
    }
    addFaceGenModel(lucas->hairModel);
    for(const std::string& path:lucas->headPartModels)
        addFaceGenModel(path);
    for(const Fo3NpcVisualItemQ230& item:lucas->inventory)
        if(item.recordType=="ARMO") addModel(item.modelPath);

    auto isFaceGenModel=[&](const std::string& path){
        for(const std::string& candidate:faceGenModels)
            if(samePath(path,candidate)) return true;
        return false;
    };
    auto isEyeModel=[&](const std::string& path){
        if(lucas->raceHeadModels.size()<8u) return false;
        return (!lucas->raceHeadModels[6].empty() &&
                samePath(path,lucas->raceHeadModels[6])) ||
               (!lucas->raceHeadModels[7].empty() &&
                samePath(path,lucas->raceHeadModels[7]));
    };
    auto isHairTintModel=[&](const std::string& path){
        if(!lucas->hairModel.empty() && samePath(path,lucas->hairModel))
            return true;
        for(const std::string& hp:lucas->headPartModels)
            if(!hp.empty() && samePath(path,hp)) return true;
        return false;
    };
    auto combineFaceGen=[](const std::vector<float>& race,
                           const std::vector<float>& npc){
        const size_t count=std::max(race.size(),npc.size());
        std::vector<float> out(count,0.0f);
        for(size_t i=0u;i<count;++i){
            if(i<race.size()) out[i]+=race[i];
            if(i<npc.size()) out[i]+=npc[i];
        }
        return out;
    };

    const std::vector<float> q234FaceSym=
        combineFaceGen(lucas->raceFaceGenGeometrySymmetric,
                       lucas->faceGenGeometrySymmetric);
    const std::vector<float> q234FaceAsym=
        combineFaceGen(lucas->raceFaceGenGeometryAsymmetric,
                       lucas->faceGenGeometryAsymmetric);
    const std::vector<float> q234FaceTex=
        combineFaceGen(lucas->raceFaceGenTextureSymmetric,
                       lucas->faceGenTextureSymmetric);

    size_t cpuShapes=0u;
    size_t gpuShapes=0u;
    size_t triangles=0u;
    size_t faceGenEgmAssets=0u;
    size_t faceGenMorphedShapes=0u;
    size_t faceGenMorphedVertices=0u;
    size_t faceGenTextureShapes=0u;
    size_t faceGenTexturePixels=0u;
    size_t eyeTextureOverrides=0u;
    size_t hairTextureOverrides=0u;
    size_t hairTintShapes=0u;
    float rx=0.0f, ry=0.0f, rz=0.0f;
    Q230ConvertBethesdaRotation(
        lucas->rx,lucas->ry,lucas->rz,rx,ry,rz);

    auto q233RebuildBasis=[](CpuObject& part){
        const size_t vertexCount=part.positionsGame.size();
        if(vertexCount==0u) return;
        std::vector<Vec3> normals(vertexCount);
        std::vector<Vec3> tangents(vertexCount);
        std::vector<Vec3> bitangents(vertexCount);
        const size_t triCount=part.mesh.indices.size()/3u;
        for(size_t tri=0u;tri<triCount;++tri){
            const uint32_t ia=part.mesh.indices[tri*3u+0u];
            const uint32_t ib=part.mesh.indices[tri*3u+1u];
            const uint32_t ic=part.mesh.indices[tri*3u+2u];
            if(ia>=vertexCount||ib>=vertexCount||ic>=vertexCount) continue;
            const Vec3 a=part.positionsGame[ia];
            const Vec3 b=part.positionsGame[ib];
            const Vec3 c=part.positionsGame[ic];
            const Vec3 e1=Q211Sub(b,a);
            const Vec3 e2=Q211Sub(c,a);
            const Vec3 n=Q211Cross(e1,e2);
            normals[ia]=Q211Add(normals[ia],n);
            normals[ib]=Q211Add(normals[ib],n);
            normals[ic]=Q211Add(normals[ic],n);

            if(part.mesh.texcoords.size()==vertexCount*2u){
                const float u0=part.mesh.texcoords[ia*2u+0u];
                const float v0=part.mesh.texcoords[ia*2u+1u];
                const float u1=part.mesh.texcoords[ib*2u+0u];
                const float v1=part.mesh.texcoords[ib*2u+1u];
                const float u2=part.mesh.texcoords[ic*2u+0u];
                const float v2=part.mesh.texcoords[ic*2u+1u];
                const float du1=u1-u0, dv1=v1-v0;
                const float du2=u2-u0, dv2=v2-v0;
                const float det=du1*dv2-du2*dv1;
                if(std::fabs(det)>1.0e-8f){
                    const float inv=1.0f/det;
                    const Vec3 t={
                        (e1.x*dv2-e2.x*dv1)*inv,
                        (e1.y*dv2-e2.y*dv1)*inv,
                        (e1.z*dv2-e2.z*dv1)*inv};
                    const Vec3 bt={
                        (e2.x*du1-e1.x*du2)*inv,
                        (e2.y*du1-e1.y*du2)*inv,
                        (e2.z*du1-e1.z*du2)*inv};
                    tangents[ia]=Q211Add(tangents[ia],t);
                    tangents[ib]=Q211Add(tangents[ib],t);
                    tangents[ic]=Q211Add(tangents[ic],t);
                    bitangents[ia]=Q211Add(bitangents[ia],bt);
                    bitangents[ib]=Q211Add(bitangents[ib],bt);
                    bitangents[ic]=Q211Add(bitangents[ic],bt);
                }
            }
        }
        for(size_t i=0u;i<vertexCount;++i){
            const Vec3 n=Q211NormalizeSafe(normals[i],{0.0f,1.0f,0.0f});
            Vec3 t=tangents[i];
            t=Q211Sub(t,Q211Mul(n,Q211Dot(n,t)));
            t=Q211NormalizeSafe(t,{1.0f,0.0f,0.0f});
            Vec3 bt=Q211NormalizeSafe(Q211Cross(n,t),{0.0f,0.0f,1.0f});
            if(Q211Dot(bt,bitangents[i])<0.0f) bt=Q211Mul(bt,-1.0f);
            part.normalsGame[i]=n;
            part.tangentsGame[i]=t;
            part.bitangentsGame[i]=bt;
        }
    };

    auto q233ApplyMorph=[&](CpuObject& part,
                            const Fo3FaceGenMorphQ233& morph){
        const size_t vertexCount=part.positionsGame.size();
        if(vertexCount!=morph.vertexCount ||
           morph.deltaXYZ.size()!=vertexCount*3u) return false;
        const float* m=part.mesh.geometryDeltaToModel;
        for(size_t i=0u;i<vertexCount;++i){
            const float dx=morph.deltaXYZ[i*3u+0u];
            const float dy=morph.deltaXYZ[i*3u+1u];
            const float dz=morph.deltaXYZ[i*3u+2u];
            Vec3 deltaModel{
                m[0]*dx+m[1]*dy+m[2]*dz,
                m[3]*dx+m[4]*dy+m[5]*dz,
                m[6]*dx+m[7]*dy+m[8]*dz};
            deltaModel=Q211Mul(deltaModel,part.placement.scale);
            deltaModel=ApplyEsmRotation(deltaModel,part.placement);
            part.positionsGame[i]=Q211Add(part.positionsGame[i],deltaModel);
        }
        q233RebuildBasis(part);
        return true;
    };

    const float q234HairR=
        static_cast<float>(lucas->hairColor[0])/255.0f;
    const float q234HairG=
        static_cast<float>(lucas->hairColor[1])/255.0f;
    const float q234HairB=
        static_cast<float>(lucas->hairColor[2])/255.0f;

    for(const std::string& path:models){
        Fo3WorldPlacement placement;
        placement.refFormId=lucas->refFormId;
        placement.baseFormId=lucas->baseFormId;
        placement.baseRecordType="NPC_";
        placement.editorId=lucas->editorId;
        placement.modelPath=path;
        placement.x=lucas->x;
        placement.y=lucas->y;
        placement.z=lucas->z;
        placement.rx=rx;
        placement.ry=ry;
        placement.rz=rz;
        placement.scale=lucas->scale;

        std::vector<CpuObject> parts;
        if(!BuildCpuObjects(placement,parts)){
            Q6H_LOGW("Q23.4 NPC PART MISS: actor=%s model=%s stage=cpu",
                     lucas->editorId.c_str(),path.c_str());
            continue;
        }
        cpuShapes+=parts.size();

        Fo3FaceGenMorphQ233 morph;
        const bool faceGenCandidate=isFaceGenModel(path);
        const bool haveMorph=
            faceGenCandidate &&
            LoadFo3FaceGenMorphQ233(
                path,q234FaceSym,q234FaceAsym,morph);
        if(haveMorph) ++faceGenEgmAssets;

        for(CpuObject& part:parts){
            if(haveMorph && q233ApplyMorph(part,morph)){
                ++faceGenMorphedShapes;
                faceGenMorphedVertices+=part.positionsGame.size();
            }

            if(isEyeModel(path) && !lucas->eyeTexturePath.empty()){
                part.mesh.diffuseTexturePath=lucas->eyeTexturePath;
                ++eyeTextureOverrides;
            }

            if(!lucas->hairModel.empty() &&
               samePath(path,lucas->hairModel) &&
               !lucas->hairTexturePath.empty()){
                part.mesh.diffuseTexturePath=lucas->hairTexturePath;
                ++hairTextureOverrides;
            }

            // Apply FGTS only to the primary RACE face colour map, not ears,
            // mouth, teeth or eyes that happen to live in the same head NIF.
            const bool q234PrimaryHead=
                !lucas->raceHeadModels.empty() &&
                !lucas->raceHeadModels[0].empty() &&
                samePath(path,lucas->raceHeadModels[0]);
            const bool q234PrimaryFaceTexture=
                !lucas->raceHeadTextures.empty() &&
                !lucas->raceHeadTextures[0].empty() &&
                samePath(part.mesh.diffuseTexturePath,
                         lucas->raceHeadTextures[0]);
            if(q234PrimaryHead && q234PrimaryFaceTexture &&
               !q234FaceTex.empty()){
                Fo3FaceGenTextureQ234 generated;
                if(LoadFo3FaceGenTextureQ234(
                        path,part.mesh.diffuseTexturePath,
                        q234FaceTex,generated)){
                    const std::string key=
                        "__q234_facegen__/"+
                        lucas->editorId+"/"+
                        std::to_string(part.q2016ShapeIndex)+".dds";
                    Fo3RgbaTexture rgba;
                    rgba.width=generated.width;
                    rgba.height=generated.height;
                    rgba.rgba=std::move(generated.rgba);
                    rgba.sourcePath=
                        generated.baseTexturePath+" + "+generated.egtPath;
                    rgba.format="Q23.4-FaceGen-EGT";
                    gQ234GeneratedTextures[key]=std::move(rgba);
                    part.mesh.diffuseTexturePath=key;
                    ++faceGenTextureShapes;
                    faceGenTexturePixels+=
                        static_cast<size_t>(generated.width)*
                        static_cast<size_t>(generated.height);
                }
            }

            if(isHairTintModel(path)){
                const size_t vertexCount=part.positionsGame.size();
                if(part.mesh.vertexColors.size()!=vertexCount*4u){
                    part.mesh.vertexColors.assign(vertexCount*4u,1.0f);
                }
                for(size_t i=0u;i<vertexCount;++i){
                    part.mesh.vertexColors[i*4u+0u]*=q234HairR;
                    part.mesh.vertexColors[i*4u+1u]*=q234HairG;
                    part.mesh.vertexColors[i*4u+2u]*=q234HairB;
                }
                ++hairTintShapes;
            }

            Q215SuppressPlayerGoreCaps(part.mesh);
            GpuObject gpu;
            if(!UploadCpuObject(
                    part,
                    gExteriorOriginXQ1890,
                    gExteriorOriginYQ1890,
                    gExteriorOriginZQ1890,
                    gpu)){
                Q6H_LOGW("Q23.4 NPC PART MISS: actor=%s model=%s stage=gpu",
                         lucas->editorId.c_str(),path.c_str());
                continue;
            }
            gpu.q230NpcActor=true;
            triangles+=static_cast<size_t>(gpu.vertexCount/3);
            gQ230NpcActors.push_back(std::move(gpu));
            ++gpuShapes;
        }
    }

    gQ230NpcReady=!gQ230NpcActors.empty();
    Q6H_LOGI("Q23.4 NPC VISUAL READY: ready=%d actor=%s ref=%08X base=%08X assets=%zu cpuShapes=%zu gpuShapes=%zu triangles=%zu raceHeadParts=%zu egmAssets=%zu morphedShapes=%zu morphedVertices=%zu faceTextureShapes=%zu faceTexturePixels=%zu eyeTextureOverrides=%zu hairTextureOverrides=%zu hairTintShapes=%zu faceGenGeometryApplied=%d faceGenTextureApplied=%d combinedCoeffs=(%zu,%zu,%zu) npcCoeffs=(%zu,%zu,%zu) raceCoeffs=(%zu,%zu,%zu) hairRGB=(%u,%u,%u) pose=bind source=RACE-baseline+NPC-FaceGen+EGM/EGT+HCLR",
             gQ230NpcReady?1:0,
             lucas->fullName.empty()?lucas->editorId.c_str():lucas->fullName.c_str(),
             lucas->refFormId,lucas->baseFormId,
             models.size(),cpuShapes,gpuShapes,triangles,
             lucas->raceHeadModels.size(),
             faceGenEgmAssets,faceGenMorphedShapes,faceGenMorphedVertices,
             faceGenTextureShapes,faceGenTexturePixels,
             eyeTextureOverrides,hairTextureOverrides,hairTintShapes,
             faceGenMorphedShapes>0u?1:0,
             faceGenTextureShapes>0u?1:0,
             q234FaceSym.size(),q234FaceAsym.size(),q234FaceTex.size(),
             lucas->faceGenGeometrySymmetric.size(),
             lucas->faceGenGeometryAsymmetric.size(),
             lucas->faceGenTextureSymmetric.size(),
             lucas->raceFaceGenGeometrySymmetric.size(),
             lucas->raceFaceGenGeometryAsymmetric.size(),
             lucas->raceFaceGenTextureSymmetric.size(),
             static_cast<unsigned>(lucas->hairColor[0]),
             static_cast<unsigned>(lucas->hairColor[1]),
             static_cast<unsigned>(lucas->hairColor[2]));
    if(!gQ230NpcReady) Q230DeleteNpcActors();
    return gQ230NpcReady;
}

void Q230RenderNpcActors(bool alphaPass) {
    if (!gExteriorStreamingActiveQ1890 ||
        gExteriorWorldspaceQ1890 != 0x00000A74u) return;
    if (!Q230EnsureNpcActors()) return;

    for (const GpuObject& object : gQ230NpcActors) {
        if (object.alphaBlend != alphaPass) continue;
        if (object.zBufferTestQ1200) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
        glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        if(alphaPass){
            glEnable(GL_BLEND);
            glBlendFunc(
                Q1150BlendFactor(object.alphaSourceBlend,true),
                Q1150BlendFactor(object.alphaDestBlend,false));
        }
        DrawSceneObject(object);
    }
}

bool Q210EnsurePlayerBody() {
    if (gQ210PlayerBodyReady) return true;
    if (gQ210PlayerBodyAttempted) return false;
    gQ210PlayerBodyAttempted = true;

    std::vector<FalloutMeshIndexEntry> maleEntries;
    ListFalloutMeshFilesByPrefix("Characters\\_Male\\", maleEntries);

    auto q215FindMesh = [](const char* prefix, const char* suffix) {
        std::vector<FalloutMeshIndexEntry> entries;
        ListFalloutMeshFilesByPrefix(prefix, entries);
        for (const FalloutMeshIndexEntry& entry : entries) {
            if (Q210EndsWithInsensitive(entry.path, suffix))
                return entry.path;
        }
        return std::string{};
    };

    // Q21.15: exact Fallout3.esm equipment models for the local male player.
    // VaultSuit101 MODL -> Armor\\VaultSuit\\M\\Outfit.NIF
    // PipBoy MODL      -> PipBoy3000\\PipBoyArm.NIF
    // PipBoyGlove MODL -> Characters\\_Male\\LeftHandPipboyGlove.NIF
    std::vector<std::string> bodyPaths;
    const std::string q216UpperBodyIk = q215FindMesh(
        "Characters\\_Male\\",
        "characters\\_male\\upperbody.nif");
    const std::string q215Vault101 = q215FindMesh(
        "Armor\\VaultSuit\\M\\",
        "armor\\vaultsuit\\m\\outfit.nif");
    const std::string q215PipBoyGlove = q215FindMesh(
        "Characters\\_Male\\",
        "characters\\_male\\lefthandpipboyglove.nif");
    const std::string q215RightHand = q215FindMesh(
        "Characters\\_Male\\",
        "characters\\_male\\righthand.nif");
    const std::string q215PipBoy = q215FindMesh(
        "PipBoy3000\\",
        "pipboy3000\\pipboyarm.nif");

    // Put the hidden canonical body first: master-arm selection is intentionally
    // first-valid, preserving the exact Q21.14 authored arm lengths/pivots.
    for (const std::string* path :
         {&q216UpperBodyIk, &q215Vault101, &q215PipBoyGlove,
          &q215RightHand, &q215PipBoy}) {
        if (!path->empty()) bodyPaths.push_back(*path);
    }

    std::string skeletonPath = "Characters\\_Male\\Skeleton.NIF";
    for (const FalloutMeshIndexEntry& entry : maleEntries) {
        if (Q210EndsWithInsensitive(
                entry.path, "characters\\_male\\skeleton.nif")) {
            skeletonPath = entry.path;
            break;
        }
    }

    std::string q212BodyAssetSummary;
    for (const std::string& bodyPath : bodyPaths) {
        if (!q212BodyAssetSummary.empty()) q212BodyAssetSummary += ",";
        q212BodyAssetSummary += bodyPath;
    }
    Q6H_LOGI("Q21.16 PLAYER EQUIPMENT ASSETS: found=%zu expected=5 ikUpperBody=%d vault101=%d pipGlove=%d rightHand=%d pipBoy=%d paths=%s",
             bodyPaths.size(),
             q216UpperBodyIk.empty() ? 0 : 1,
             q215Vault101.empty() ? 0 : 1,
             q215PipBoyGlove.empty() ? 0 : 1,
             q215RightHand.empty() ? 0 : 1,
             q215PipBoy.empty() ? 0 : 1,
             q212BodyAssetSummary.empty()
                 ? "<none>"
                 : q212BodyAssetSummary.c_str());

    Fo3NifSkinProbe skeletonProbe;
    ProbeFo3NifSkin(skeletonPath, skeletonProbe);

    size_t cpuShapes = 0u;
    size_t gpuShapes = 0u;
    size_t triangles = 0u;
    size_t skinInstances = 0u;
    size_t referencedBones = 0u;

    for (const std::string& path : bodyPaths) {
        Fo3NifSkinProbe probe;
        ProbeFo3NifSkin(path, probe);
        skinInstances += probe.skinInstances;
        referencedBones += probe.referencedBones;

        Fo3WorldPlacement placement;
        placement.modelPath = path;
        placement.baseRecordType = "NPC_";
        placement.editorId = "FalloutQuestPlayerBodyQ210";
        placement.scale = 1.0f;

        std::vector<CpuObject> parts;
        if (!BuildCpuObjects(placement, parts)) {
            Q6H_LOGW("Q21.0 PLAYER BODY PART MISS: model=%s stage=cpu",
                     path.c_str());
            continue;
        }
        cpuShapes += parts.size();

        size_t q215CapsRemovedForModel = 0u;
        for (CpuObject& part : parts) {
            q215CapsRemovedForModel +=
                Q215SuppressPlayerGoreCaps(part.mesh);
            std::vector<float> q211BindExpanded;
            if (part.mesh.skinned &&
                PrepareExpandedVertexStreamQ1960(
                    part, 0.0f, 0.0f, 0.0f)) {
                q211BindExpanded = part.q1960ExpandedVertices;
            }

            GpuObject gpu;
            if (!UploadCpuObject(part, 0.0f, 0.0f, 0.0f, gpu)) {
                Q6H_LOGW("Q21.0 PLAYER BODY PART MISS: model=%s stage=gpu",
                         path.c_str());
                continue;
            }
            gpu.q210PlayerBody = true;
            gpu.q215IkReferenceOnly =
                Q210EndsWithInsensitive(
                    path, "characters\\_male\\upperbody.nif");
            triangles += static_cast<size_t>(gpu.vertexCount / 3);

            const size_t q211GpuIndex = gQ210PlayerBody.size();
            gQ210PlayerBody.push_back(std::move(gpu));

            if (part.mesh.skinned &&
                !q211BindExpanded.empty() &&
                part.mesh.skinBoneIndices.size() ==
                    part.positionsGame.size() * 4u &&
                part.mesh.skinBoneWeights.size() ==
                    part.positionsGame.size() * 4u) {
                Q211PlayerRigPart rig;
                rig.gpuIndex = q211GpuIndex;
                rig.sourceModelPath = path;
                rig.bindExpanded = std::move(q211BindExpanded);
                rig.workExpanded = rig.bindExpanded;
                rig.bones = part.mesh.skinBones;
                rig.expandedBoneIndices.reserve(
                    part.mesh.indices.size() * 4u);
                rig.expandedBoneWeights.reserve(
                    part.mesh.indices.size() * 4u);

                for (uint32_t sourceVertex : part.mesh.indices) {
                    if (sourceVertex >= part.positionsGame.size()) continue;
                    for (size_t slot = 0u; slot < 4u; ++slot) {
                        const size_t at =
                            static_cast<size_t>(sourceVertex) * 4u + slot;
                        rig.expandedBoneIndices.push_back(
                            part.mesh.skinBoneIndices[at]);
                        rig.expandedBoneWeights.push_back(
                            part.mesh.skinBoneWeights[at]);
                    }
                }

                rig.leftUpperArm = Q211FindPrimaryBone(
                    rig.bones, "bip01 l upperarm", "l upperarm");
                rig.leftForearm = Q211FindPrimaryBone(
                    rig.bones, "bip01 l forearm", "l forearm");
                rig.leftHand = Q211FindPrimaryBone(
                    rig.bones, "bip01 l hand", "l hand");
                rig.rightUpperArm = Q211FindPrimaryBone(
                    rig.bones, "bip01 r upperarm", "r upperarm");
                rig.rightForearm = Q211FindPrimaryBone(
                    rig.bones, "bip01 r forearm", "r forearm");
                rig.rightHand = Q211FindPrimaryBone(
                    rig.bones, "bip01 r hand", "r hand");
                rig.leftChainReady =
                    rig.leftUpperArm >= 0 &&
                    rig.leftForearm >= 0 &&
                    rig.leftHand >= 0;

                if (rig.leftChainReady || rig.rightChainReady) {
                    std::string q214ArmBones;
                    for (const Fo3NifSkinBone& bone : rig.bones) {
                        const int role = Q211BoneRole(bone.name);
                        if (role == 0) continue;
                        if (!q214ArmBones.empty()) q214ArmBones += ",";
                        q214ArmBones += bone.name;
                        q214ArmBones += ":";
                        q214ArmBones += std::to_string(role);
                    }
                    Q6H_LOGI("Q21.5 ARM BONE MAP: model=%s shape=%u bones=%s",
                             path.c_str(), part.q2016ShapeIndex,
                             q214ArmBones.empty()
                                 ? "<none>"
                                 : q214ArmBones.c_str());
                }
                rig.rightChainReady =
                    rig.rightUpperArm >= 0 &&
                    rig.rightForearm >= 0 &&
                    rig.rightHand >= 0;

                float q211dLHandWeight = 0.0f;
                float q211dRHandWeight = 0.0f;
                float q211dLForeWeight = 0.0f;
                float q211dRForeWeight = 0.0f;
                rig.leftPalmAnchorValid =
                    Q211WeightedGeometryAnchor(
                        rig, 3, rig.leftPalmAnchor, q211dLHandWeight);
                rig.rightPalmAnchorValid =
                    Q211WeightedGeometryAnchor(
                        rig, 6, rig.rightPalmAnchor, q211dRHandWeight);
                rig.leftForearmCentroidValid =
                    Q211WeightedGeometryAnchor(
                        rig, 2, rig.leftForearmCentroid, q211dLForeWeight);
                rig.rightForearmCentroidValid =
                    Q211WeightedGeometryAnchor(
                        rig, 5, rig.rightForearmCentroid, q211dRForeWeight);

                Vec3 q211cLUpper{}, q211cLFore{}, q211cLHand{};
                Vec3 q211cRUpper{}, q211cRFore{}, q211cRHand{};
                if (rig.leftChainReady) {
                    q211cLUpper = Q211BindBonePoint(rig.bones[rig.leftUpperArm]);
                    q211cLFore = Q211BindBonePoint(rig.bones[rig.leftForearm]);
                    q211cLHand = Q211BindBonePoint(rig.bones[rig.leftHand]);
                }
                if (rig.rightChainReady) {
                    q211cRUpper = Q211BindBonePoint(rig.bones[rig.rightUpperArm]);
                    q211cRFore = Q211BindBonePoint(rig.bones[rig.rightForearm]);
                    q211cRHand = Q211BindBonePoint(rig.bones[rig.rightHand]);
                }

                const Vec3 q211dLRestBoneDir =
                    Q211Sub(q211cLHand, q211cLFore);
                const Vec3 q211dRRestBoneDir =
                    Q211Sub(q211cRHand, q211cRFore);
                const Vec3 q211dLGeomDir =
                    Q211Sub(rig.leftPalmAnchor, q211cLFore);
                const Vec3 q211dRGeomDir =
                    Q211Sub(rig.rightPalmAnchor, q211cRFore);
                const float q211dLDot =
                    rig.leftPalmAnchorValid
                        ? Q211Dot(
                              Q211NormalizeSafe(q211dLRestBoneDir),
                              Q211NormalizeSafe(q211dLGeomDir))
                        : 0.0f;
                const float q211dRDot =
                    rig.rightPalmAnchorValid
                        ? Q211Dot(
                              Q211NormalizeSafe(q211dRRestBoneDir),
                              Q211NormalizeSafe(q211dRGeomDir))
                        : 0.0f;

                Q6H_LOGI("Q21.5 RIG PART: model=%s shape=%u gpuIndex=%zu expandedVertices=%zu leftChain=%d palmL=%d anchorL=(%.3f %.3f %.3f) boneHandL=(%.3f %.3f %.3f) distalDotL=%.3f rightChain=%d palmR=%d anchorR=(%.3f %.3f %.3f) boneHandR=(%.3f %.3f %.3f) distalDotR=%.3f handWeight=(%.1f,%.1f) foreWeight=(%.1f,%.1f)",
                         path.c_str(), part.q2016ShapeIndex,
                         q211GpuIndex, rig.bindExpanded.size() / 18u,
                         rig.leftChainReady ? 1 : 0,
                         rig.leftPalmAnchorValid ? 1 : 0,
                         rig.leftPalmAnchor.x,
                         rig.leftPalmAnchor.y,
                         rig.leftPalmAnchor.z,
                         q211cLHand.x, q211cLHand.y, q211cLHand.z,
                         q211dLDot,
                         rig.rightChainReady ? 1 : 0,
                         rig.rightPalmAnchorValid ? 1 : 0,
                         rig.rightPalmAnchor.x,
                         rig.rightPalmAnchor.y,
                         rig.rightPalmAnchor.z,
                         q211cRHand.x, q211cRHand.y, q211cRHand.z,
                         q211dRDot,
                         q211dLHandWeight, q211dRHandWeight,
                         q211dLForeWeight, q211dRForeWeight);

                if (rig.expandedBoneIndices.size() ==
                        (rig.bindExpanded.size() / 18u) * 4u) {
                    gQ211PlayerRigParts.push_back(std::move(rig));
                }
            }
            ++gpuShapes;
        }

        Q6H_LOGI("Q21.16 PLAYER PART: model=%s ikReferenceOnly=%d capTrianglesRemoved=%zu mode=BSDismember-authored-caps-only",
                 path.c_str(),
                 Q210EndsWithInsensitive(
                     path, "characters\\_male\\upperbody.nif") ? 1 : 0,
                 q215CapsRemovedForModel);
    }

    gQ210PlayerBodyReady = !gQ210PlayerBody.empty();
    Q6H_LOGI("Q21.16 PLAYER BODY READY: ready=%d archiveMaleEntries=%zu bodyPartsFound=%zu cpuShapes=%zu gpuShapes=%zu rigParts=%zu triangles=%zu skinInstances=%zu referencedBonesAcrossParts=%zu skeletonNodes=%u skeletonNamedNodes=%zu mode=real-FO3-weighted-skinning armIK=two-bone",
             gQ210PlayerBodyReady ? 1 : 0,
             maleEntries.size(), bodyPaths.size(),
             cpuShapes, gpuShapes, gQ211PlayerRigParts.size(), triangles,
             skinInstances, referencedBones,
             skeletonProbe.nodes, skeletonProbe.nodeNames.size());
    if (!gQ210PlayerBodyReady) Q210DeletePlayerBody();
    return gQ210PlayerBodyReady;
}

void Q210RenderPlayerBody(bool alphaPass) {
    if (!Q210EnsurePlayerBody()) return;
    Q211UpdatePlayerRig();
    for (const GpuObject& object : gQ210PlayerBody) {
        if (object.q215IkReferenceOnly) continue;
        if (object.alphaBlend != alphaPass) continue;
        if (object.zBufferTestQ1200) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
        glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        if (alphaPass) {
            glEnable(GL_BLEND);
            glBlendFunc(Q1150BlendFactor(object.alphaSourceBlend, true),
                        Q1150BlendFactor(object.alphaDestBlend, false));
        }
        DrawSceneObject(object);
    }

    ++gQ210PlayerBodyFrames;
    if ((gQ210PlayerBodyFrames % 360u) == 1u) {
        Q6H_LOGI("Q21.0B PLAYER BODY HEARTBEAT: shapes=%zu head=(%.3f %.3f %.3f yaw=%.1fdeg) rootY=%.3f left=(%d %.3f %.3f %.3f) right=(%d %.3f %.3f %.3f) pose=bind rootFollowsHMDXZ=1 rootFollowsFloorY=1",
                 gQ210PlayerBody.size(),
                 gQ210Head[0], gQ210Head[1], gQ210Head[2],
                 gQ210Head[3] * 57.2957795f,
                 gQ210PlayerRoot[13],
                 gQ210LeftHandValid ? 1 : 0,
                 gQ210LeftHand[0], gQ210LeftHand[1], gQ210LeftHand[2],
                 gQ210RightHandValid ? 1 : 0,
                 gQ210RightHand[0], gQ210RightHand[1], gQ210RightHand[2]);
    }
}

void RenderScene() {
    Q1970RenderStallScopeQ19 q1970RenderStallScope;
    if (!gSceneReady) Q1030BootMegatonOnRender();
    ProcessQ74TransitionRequest();

    // Q20.22C: run the LOD archive diagnostic from the render path as well as
    // transition/streaming. RenderScene is a proven heartbeat in captures, so
    // this cannot disappear just because a one-shot transition message rolled
    // out of logcat. Retry the archive query periodically while in Wasteland.
    static uint64_t q2022cProbeHeartbeat = 0u;
    ++q2022cProbeHeartbeat;
    const bool q2022cPulse =
        q2022cProbeHeartbeat == 1u ||
        (q2022cProbeHeartbeat % 120u) == 0u;
    const bool q2022cWasteland =
        gExteriorStreamingActiveQ1890 &&
        gExteriorWorldspaceQ1890 == 0x0000003Cu;
    if (q2022cPulse && q2022cWasteland &&
        !gQ2022LodArchiveProbeDone) {
        Q2022ProbeLodArchive(
            gExteriorWindowGridXQ1890,
            gExteriorWindowGridYQ1890);
    }
    if (q2022cPulse) {
        Q6H_LOGI("Q20.22C PROBE HEARTBEAT: frame=%llu sceneReady=%d exteriorActive=%d worldspace=%08X currentCell=%08X grid=(%d,%d) wasteland=%d probeDone=%d",
                 static_cast<unsigned long long>(q2022cProbeHeartbeat),
                 gSceneReady ? 1 : 0,
                 gExteriorStreamingActiveQ1890 ? 1 : 0,
                 gExteriorWorldspaceQ1890,
                 gCurrentCellFormId,
                 gExteriorWindowGridXQ1890,
                 gExteriorWindowGridYQ1890,
                 q2022cWasteland ? 1 : 0,
                 gQ2022LodArchiveProbeDone ? 1 : 0);
    }

    if (!gSceneReady || !gProgram || gObjects.empty()) return;

    static uint64_t q209aWaterHeartbeatFrame = 0u;
    ++q209aWaterHeartbeatFrame;
    if (q209aWaterHeartbeatFrame == 1u ||
        (q209aWaterHeartbeatFrame % 300u) == 0u) {
        Q6H_LOGI("Q20.9A WATER HEARTBEAT: build=Q20.9A exteriorStreaming=%d worldspace=%08X currentCell=%08X grid=(%d,%d) nearbyWaterCells=%zu sceneObjects=%zu",
                 gExteriorStreamingActiveQ1890 ? 1 : 0,
                 gExteriorWorldspaceQ1890,
                 gCurrentCellFormId,
                 gExteriorWindowGridXQ1890,
                 gExteriorWindowGridYQ1890,
                 GetFo3WaterCellsQ2070().size(),
                 gObjects.size());
    }

    GLint mainProgram = 0, mainVao = 0, previousActiveTexture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &mainProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &mainVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    if (mainProgram == 0) return;

    const GLint sourceMvp = glGetUniformLocation(static_cast<GLuint>(mainProgram), "uMvp");
    if (sourceMvp < 0) return;
    GLfloat mvp[16]{};
    glGetUniformfv(static_cast<GLuint>(mainProgram), sourceMvp, mvp);
    Q2015FrustumCullScope q2015FrustumScope(mvp);

    GLint previousTexture0 = 0, previousTexture1 = 0, previousTexture2 = 0, previousTexture3 = 0;
    GLint previousTexture4CubeQ2050 = 0, previousTexture5Q2050 = 0;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture1);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2);
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture3);
    glActiveTexture(GL_TEXTURE4);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previousTexture4CubeQ2050);
    glActiveTexture(GL_TEXTURE5);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture5Q2050);
    glActiveTexture(GL_TEXTURE0);

    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean depthTestWasEnabledQ1200 = glIsEnabled(GL_DEPTH_TEST);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    GLint previousDepthFuncQ2050 = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFuncQ2050);
    GLint previousBlendSrcRgb = GL_ONE, previousBlendDstRgb = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE, previousBlendDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);

    glUseProgram(gProgram);
    glUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp);
    if (gWaterReflectionClipEnabledLocationQ2090 >= 0) {
        glUniform1f(gWaterReflectionClipEnabledLocationQ2090, 0.0f);
    }
    glUniform1i(gDiffuseLocation, 0);
    glUniform1i(gNormalLocation, 1);
    glUniform1i(gGlowLocationQ1020, 2);
    if (gEnvironmentCubeLocationQ2050 >= 0)
        glUniform1i(gEnvironmentCubeLocationQ2050, 4);
    if (gEnvironmentMaskLocationQ2050 >= 0)
        glUniform1i(gEnvironmentMaskLocationQ2050, 5);
    const Fo3EnvironmentQ1000& q1000Env = GetFo3EnvironmentQ1000();
    float q203dPcLightDirection[3]{0.35f, 0.85f, 0.40f};
    const bool q203dPcLightReady =
        GetFo3PcLightDirectionQ203D(q203dPcLightDirection);
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
        glUniform3fv(gSunDirectionLocationQ1000, 1,
                     q203dPcLightReady ? q203dPcLightDirection : q1000Env.sunDirection);
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
        if (q203dPcLightReady) {
            glUniform3fv(gSunDirectionVertexLocationQ1540, 1, q203dPcLightDirection);
        } else if (q1000Env.valid) {
            glUniform3fv(gSunDirectionVertexLocationQ1540, 1, q1000Env.sunDirection);
        } else {
            glUniform3f(gSunDirectionVertexLocationQ1540, 0.35f, 0.85f, 0.40f);
        }
    }

    static int q203dLastLightHourBucket = -1;
    const int q203dLightHourBucket =
        static_cast<int>(std::floor(GetFo3TestHourQ1400() * 10.0f));
    if (q203dPcLightReady && q203dLightHourBucket != q203dLastLightHourBucket) {
        q203dLastLightHourBucket = q203dLightHourBucket;
        Q6H_LOGI(
            "Q20.3D LIGHTDATA: hour=%.2f pcLightOpenXR=(%.7f %.7f %.7f) oldSynthetic=(%.7f %.7f %.7f) source=PC_LAND_c18 anchors=00,12,18 interpolation=SLERP bridge=explicit staticSP17=1 LAND=1 skySun=unchanged",
            GetFo3TestHourQ1400(),
            q203dPcLightDirection[0], q203dPcLightDirection[1], q203dPcLightDirection[2],
            q1000Env.sunDirection[0], q1000Env.sunDirection[1], q1000Env.sunDirection[2]);
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
    const auto q2017OpaqueStarted = std::chrono::steady_clock::now();
    glDisable(GL_BLEND);
    const GLboolean q2021MainA2cWas =
        glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
    if (q2060MsaaActive && q2060MsaaSamples > 1)
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    Q1990RenderNativeLod(false);
    Q2017RenderOpaqueDetailedInstanced();
    Q230RenderNpcActors(false);
    Q210RenderPlayerBody(false);
    if (!q2021MainA2cWas)
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    const uint64_t q2017OpaqueUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2017OpaqueStarted).count());

    const auto q2017AlphaStarted = std::chrono::steady_clock::now();
    glEnable(GL_BLEND);
    Q1990RenderNativeLod(true);
    Q230RenderNpcActors(true);
    Q210RenderPlayerBody(true);
    for (const GpuObject& object : gObjects) {
        if (!object.alphaBlend) continue;
        if (object.zBufferTestQ1200) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
        glBlendFunc(Q1150BlendFactor(object.alphaSourceBlend, true),
                    Q1150BlendFactor(object.alphaDestBlend, false));
        DrawSceneObject(object);
    }
    const uint64_t q2017AlphaUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2017AlphaStarted).count());

    const auto q2017EnvStarted = std::chrono::steady_clock::now();
    // Q20.5 PC apitrace calls 14054-14061: Fallout switches to a
    // dedicated environment pass with ZWRITE=FALSE, ZFUNC=EQUAL and
    // additive ONE/ONE blending. Re-draw only authored reflective shapes.
    size_t q2050EnvironmentDraws = 0u;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    if (gEnvironmentPassEnabledQ205A) {
        for (const GpuObject& object : gObjects) {
            if (!object.environmentEnabledQ2050 || !object.zBufferWriteQ1200) continue;
            DrawSceneObject(object, true);
            ++q2050EnvironmentDraws;
        }
    }
    static bool q2050PassLogged = false;
    if (!q2050PassLogged) {
        q2050PassLogged = true;
        Q6H_LOGI("Q20.5 ENV PASS READY: draws=%zu source=SP17-SLS2057/2058+nif-slots4,5 pcTraceCalls=14054-14114 blend=ONE+ONE depthFunc=EQUAL depthWrite=0 cube=authored customMask=authored envScale=authored globalPost=Q20.4F-unchanged pcPerObjectFade=pending",
                 q2050EnvironmentDraws);
    }
    ++gEnvironmentHeartbeatFrameQ205A;
    if ((gEnvironmentHeartbeatFrameQ205A % 300u) == 1u) {
        Q6H_LOGI("Q20.5A ENV HEARTBEAT: passEnabled=%d drawsThisEye=%zu candidatesSeen=%zu enabledMaterials=%zu cubeFailures=%zu maskFailures=%zu objects=%zu",
                 gEnvironmentPassEnabledQ205A ? 1 : 0,
                 q2050EnvironmentDraws,
                 gEnvironmentCandidatesQ205A,
                 gEnvironmentEnabledMaterialsQ205A,
                 gEnvironmentCubeFailuresQ205A,
                 gEnvironmentMaskFailuresQ205A,
                 gObjects.size());
    }

    glDepthFunc(static_cast<GLenum>(previousDepthFuncQ2050));
    glDepthMask(previousDepthMask);
    if (depthTestWasEnabledQ1200) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glBlendFuncSeparate(static_cast<GLenum>(previousBlendSrcRgb),
                        static_cast<GLenum>(previousBlendDstRgb),
                        static_cast<GLenum>(previousBlendSrcAlpha),
                        static_cast<GLenum>(previousBlendDstAlpha));
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    const uint64_t q2017EnvUs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() -
            q2017EnvStarted).count());

    static uint64_t q2017PhaseEye = 0u;
    ++q2017PhaseEye;
    if ((q2017PhaseEye % 240u) == 1u ||
        q2017OpaqueUs >= 12000u ||
        q2017AlphaUs >= 12000u ||
        q2017EnvUs >= 12000u) {
        Q6H_LOGI("Q20.17 RENDER PHASES: opaqueUs=%llu alphaUs=%llu envUs=%llu sceneObjects=%zu lodBlocks=%zu",
                 static_cast<unsigned long long>(q2017OpaqueUs),
                 static_cast<unsigned long long>(q2017AlphaUs),
                 static_cast<unsigned long long>(q2017EnvUs),
                 gObjects.size(), gQ1990NativeLodBlocks.size());
    }

    // Q20.7B: LAND must be present in the depth buffer before water.
    // RenderFo3CollisionOverlay is the historical hook name but currently
    // dispatches the real LAND terrain renderer. Q20.7A drew water first with
    // depthWrite=0, so LAND painted over nearly the entire water plane.
    RenderFo3CollisionOverlay(mvp);

    if (gExteriorStreamingActiveQ1890 && gExteriorWorldspaceQ1890 != 0u) {
        // Q20.9 WATER000: first render the current eye's mirrored world into
        // FalloutPrefs' 1024x1024 ReflectionMap, then resolve the already-drawn
        // main scene for WATER001's RefractionMap + DepthMap inputs.
        float q2090PlaneY = 0.0f;
        float q2090HeightGame = 0.0f;
        uint32_t q2090PlaneCell = 0u;
        uint32_t q2090PlaneType = 0u;
        const bool q2090HavePlane = GetFo3DominantWaterPlaneQ2070(
            gFo3EyePositionQ1010[0], gFo3EyePositionQ1010[2],
            gExteriorOriginXQ1890, gExteriorOriginYQ1890,
            gExteriorOriginZQ1890, FLOOR_Y, SCENE_FORWARD,
            FO3_UNITS_PER_METRE,
            &q2090PlaneY, &q2090HeightGame,
            &q2090PlaneCell, &q2090PlaneType);

        float q2090ReflectionMvp[16]{};
        const size_t q209bExposedCells = GetFo3ExposedWaterCellCountQ209B();
        const bool q209bWaterInFrustum =
            q2090HavePlane && q209bExposedCells > 0u &&
            IsFo3WaterPotentiallyVisibleQ209B(
                mvp,
                gExteriorOriginXQ1890,
                gExteriorOriginYQ1890,
                gExteriorOriginZQ1890,
                FLOOR_Y,
                SCENE_FORWARD,
                FO3_UNITS_PER_METRE);
        const auto q2019WaterReflectionCallStarted =
            std::chrono::steady_clock::now();
        const bool q2090ReflectionReady =
            q2090HavePlane && q209bWaterInFrustum &&
            Q2090RenderWaterReflection(mvp, q2090PlaneY, q2090ReflectionMvp);
        const uint64_t q2019WaterReflectionCallUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() -
                q2019WaterReflectionCallStarted).count());

        static uint32_t q2090LastPlaneCellLogged = 0u;
        static int q209bLastExposedLogged = -1;
        static int q209bLastFrustumLogged = -1;
        if ((q2090HavePlane && q2090PlaneCell != q2090LastPlaneCellLogged) ||
            static_cast<int>(q209bExposedCells) != q209bLastExposedLogged ||
            (q209bWaterInFrustum ? 1 : 0) != q209bLastFrustumLogged) {
            q2090LastPlaneCellLogged = q2090PlaneCell;
            q209bLastExposedLogged = static_cast<int>(q209bExposedCells);
            q209bLastFrustumLogged = q209bWaterInFrustum ? 1 : 0;
            Q6H_LOGI("Q20.9B WATER VISIBILITY: worldspace=%08X loaded=%zu exposed=%zu frustumVisible=%d dominantCell=%08X WATR=%08X heightGame=%.3f planeSceneY=%.5f reflectionReady=%d policy=LAND-min<water+homogeneous-frustum",
                     gExteriorWorldspaceQ1890,
                     GetFo3WaterCellsQ2070().size(),
                     q209bExposedCells,
                     q209bWaterInFrustum ? 1 : 0,
                     q2090PlaneCell, q2090PlaneType,
                     q2090HeightGame, q2090PlaneY,
                     q2090ReflectionReady ? 1 : 0);
        }

        const auto q2019WaterPostStarted =
            std::chrono::steady_clock::now();
        const bool q2080SceneSnapshotReady =
            q2060MsaaActive && q2060MsaaSamples > 1 &&
            q2060MsaaFbo != 0u && q1280PostFbo != 0u &&
            q1280PostColor != 0u && q1370PostDepth != 0u &&
            q1280PostWidth > 0 && q1280PostHeight > 0;
        if (q2080SceneSnapshotReady) {
            Q2060ResolveEyeMsaaQ2060();
            glBindFramebuffer(GL_FRAMEBUFFER, q2060MsaaFbo);
            glViewport(0, 0, q1280PostWidth, q1280PostHeight);
        }

        const float q2080FallbackSunDirection[3]{0.35f, 0.85f, 0.40f};
        const float q2080FallbackSunColor[3]{1.0f, 1.0f, 1.0f};
        const float q2080FallbackFogColor[3]{0.0f, 0.0f, 0.0f};
        const float* q2080SunDirection = q203dPcLightReady
            ? q203dPcLightDirection
            : (q1000Env.valid ? q1000Env.sunDirection : q2080FallbackSunDirection);
        const float* q2080SunColor = q1000Env.valid
            ? q1000Env.sunlight : q2080FallbackSunColor;
        const float* q2080FogColor = q1000Env.valid
            ? q1000Env.fog : q2080FallbackFogColor;
        constexpr float q2080NearClipMetres = 0.04f;
        constexpr float q2080FarClipMetres =
            125000.0f / FO3_UNITS_PER_METRE;

        const auto q2019WaterDrawStarted =
            std::chrono::steady_clock::now();
        RenderFo3WaterSurfaceQ2070(
            mvp,
            gExteriorOriginXQ1890,
            gExteriorOriginYQ1890,
            gExteriorOriginZQ1890,
            FLOOR_Y,
            SCENE_FORWARD,
            FO3_UNITS_PER_METRE,
            q2080SceneSnapshotReady ? q1280PostColor : 0u,
            q2080SceneSnapshotReady ? q1370PostDepth : 0u,
            q1280PostWidth,
            q1280PostHeight,
            q2080SceneSnapshotReady,
            gFo3EyePositionQ1010,
            q2080SunDirection,
            q2080SunColor,
            q2080FogColor,
            q1532StaticFogNear,
            q1532StaticFogFar,
            GetFo3FogPowerQ1410(),
            q2080NearClipMetres,
            q2080FarClipMetres,
            q2090ReflectionReady ? gWaterReflectionColorQ2090 : 0u,
            q2090ReflectionReady,
            q2090ReflectionReady ? q2090ReflectionMvp : nullptr);
        const uint64_t q2019WaterDrawUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() -
                q2019WaterDrawStarted).count());
        const uint64_t q2019WaterPostUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() -
                q2019WaterPostStarted).count());
        static uint64_t q2019WaterEyePasses = 0u;
        ++q2019WaterEyePasses;
        if ((q2019WaterEyePasses % 120u) == 1u ||
            q2019WaterReflectionCallUs >= 35000u ||
            q2019WaterPostUs >= 12000u) {
            Q6H_LOGI("Q20.19 WATER OUTSIDE PHASES: reflectionUs=%llu postAndWaterUs=%llu waterDrawUs=%llu reflectionReady=%d waterInFrustum=%d exposedCells=%zu snapshotReady=%d",
                     static_cast<unsigned long long>(q2019WaterReflectionCallUs),
                     static_cast<unsigned long long>(q2019WaterPostUs),
                     static_cast<unsigned long long>(q2019WaterDrawUs),
                     q2090ReflectionReady ? 1 : 0,
                     q209bWaterInFrustum ? 1 : 0,
                     q209bExposedCells,
                     q2080SceneSnapshotReady ? 1 : 0);
        }
    }

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture5Q2050));
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(previousTexture4CubeQ2050));
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

// Q20.6: FalloutPrefs requests 8x MSAA. The Quest scene renders through an
// off-screen HDR target before post processing, so multisampling must live
// here rather than on the final OpenXR full-screen composite.
GLuint q2060MsaaFbo = 0u;
GLuint q2060MsaaColor = 0u;
GLuint q2060MsaaDepth = 0u;
GLsizei q2060MsaaWidth = 0;
GLsizei q2060MsaaHeight = 0;
GLsizei q2060MsaaSamples = 1;
GLint q2060GlMaxSamples = 1;
bool q2060MsaaActive = false;
bool q2060MsaaAttempted = false;
uint64_t q2060MsaaHeartbeatFrame = 0u;
uint32_t q2060OpenXrRecommendedSamples = 0u;
uint32_t q2060OpenXrMaxSamples = 0u;
constexpr GLsizei Q2060_PC_REQUESTED_MSAA = 8;

void Q2060SetOpenXrSampleInfo(uint32_t recommendedSamples, uint32_t maxSamples) {
    q2060OpenXrRecommendedSamples = recommendedSamples;
    q2060OpenXrMaxSamples = maxSamples;
    Q6H_LOGI("Q20.6 OPENXR SAMPLE INFO: recommended=%u max=%u sceneMsaaPolicy=offscreen-probe-up-to-PC8x",
             recommendedSamples, maxSamples);
}

GLuint q1280PostProgram = 0u;
GLuint q1280PostVao = 0u;
GLsizei q1280PostWidth = 0;
GLsizei q1280PostHeight = 0;
GLenum q1280PostInternalFormat = GL_RGBA8;
bool q1280PostActive = false;
bool q1280PostLoggedGpu = false;
// Q20.4E parity diagnostic: Q13.7 contact AO is a FalloutQuest-only effect,
// not part of the captured Fallout 3 SP17/final-film path. Keep it OFF by
// default and expose a runtime A/B toggle rather than contaminating vanilla.
bool q204eContactAoEnabled = false;

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
GLint q204eContactAoEnabledLocation = -1;
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
        uniform float uContactAoEnabledQ204E;
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
            // Q18.5: Fallout.ini fBlockLoadDistance=125000.0 game units.
            // FalloutQuest exterior scale is 70 game units per metre.
            const float farZ = 1785.7142857;
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
            // Q20.4E: this contact AO is a Quest-only diagnostic effect. It was
            // never observed in the captured Fallout 3 final path, so vanilla
            // parity defaults to disabled. RIGHT_B can enable it for a clean A/B.
            if (uContactAoEnabledQ204E > 0.5) {
                float contactAoQ1370 = Q1370ContactAo(vUv, texture(uDepthQ1370, vUv).r);
                float aoLumQ1370 = Q1280Lum(max(colour, vec3(0.0)));
                float aoMaterialMaskQ1370 = 1.0 - smoothstep(0.70, 1.35, aoLumQ1370);
                colour *= mix(1.0, contactAoQ1370, aoMaterialMaskQ1370);
            }

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

            // X8R8G8B8 clamps AND quantises the final shader result to an
            // 8-bit display code before the D3D9 gamma ramp is applied.
            q1640PcOutput = clamp(q1640PcOutput, vec3(0.0), vec3(1.0));
            vec3 q204fPcBackbufferCode =
                floor(q1640PcOutput * 255.0 + 0.5) / 255.0;

            // Q20.4F / apitrace call 39178:
            // IDirect3DDevice9::SetGammaRamp(... D3DSGR_CALIBRATE ...)
            // captured with FalloutPrefs fGamma=0.7600. R/G/B are identical
            // and the 256-entry WORD16 ramp is the game's x^0.76 curve
            // (to capture precision; only one entry differs by one 16-bit LSB).
            // Apply this AFTER the X8R8G8B8 quantisation, matching the PC's
            // scanout order rather than treating gamma as another film grade.
            vec3 q204fPcPresentedCode =
                pow(q204fPcBackbufferCode, vec3(0.76));

            // PC: X8R8G8B8 + D3DRS_SRGBWRITEENABLE=0 stores the shader
            // numeric value directly. The hardware gamma ramp then maps that
            // stored code to q204fPcPresentedCode. Quest uses a GL_RGBA8
            // OpenXR swapchain whose values are linear, so decode the *post-
            // gamma-ramp PC display code* to linear before submission. The
            // compositor presentation transfer then lands on the same code.
            colour = Q1340SrgbToLinear(q204fPcPresentedCode);
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
    q204eContactAoEnabledLocation = glGetUniformLocation(q1280PostProgram, "uContactAoEnabledQ204E");
    q1520TargetLumLocation = glGetUniformLocation(q1280PostProgram, "uTargetLumQ1520");
    q1560PostRenderStageLocation = glGetUniformLocation(q1280PostProgram, "uRenderStageQ1560");
    q1670PcBloomLocation = glGetUniformLocation(q1280PostProgram, "uPcBloomQ1670");
    q1670PcBloomReadyLocation = glGetUniformLocation(q1280PostProgram, "uPcBloomReadyQ1670");
    return q1280SceneLocation >= 0 && q1280TexelLocation >= 0 &&
           q1350ExposureLocation >= 0 && q1370DepthLocation >= 0 &&
           q204eContactAoEnabledLocation >= 0 &&
           q1520TargetLumLocation >= 0 && q1560PostRenderStageLocation >= 0 &&
           q1670PcBloomLocation >= 0 && q1670PcBloomReadyLocation >= 0;
}

bool Q1280DriverSupportsHalfFloatTarget() {
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (!extensions) return false;
    return std::strstr(extensions, "GL_EXT_color_buffer_half_float") != nullptr ||
           std::strstr(extensions, "GL_EXT_color_buffer_float") != nullptr;
}

bool Q2060AllocateMsaaTargetQ2060(GLsizei width, GLsizei height,
                                  GLenum colorFormat) {
    q2060MsaaActive = false;
    q2060MsaaSamples = 1;
    q2060MsaaAttempted = true;
    if (width <= 0 || height <= 0) {
        Q6H_LOGW("Q20.6 MSAA ALLOCATE ENTER: size=%dx%d valid=0", width, height);
        return false;
    }

    Q6H_LOGI("Q20.6 MSAA ALLOCATE ENTER: size=%dx%d color=%s xrRecommended=%u xrMax=%u",
             width, height,
             colorFormat == GL_RGBA16F ? "RGBA16F" : "RGBA8",
             q2060OpenXrRecommendedSamples, q2060OpenXrMaxSamples);

    glGetIntegerv(GL_MAX_SAMPLES, &q2060GlMaxSamples);
    q2060GlMaxSamples = std::max(q2060GlMaxSamples, 1);

    if (!q2060MsaaFbo) glGenFramebuffers(1, &q2060MsaaFbo);
    if (!q2060MsaaColor) glGenRenderbuffers(1, &q2060MsaaColor);
    if (!q2060MsaaDepth) glGenRenderbuffers(1, &q2060MsaaDepth);

    const GLsizei capped =
        std::min<GLsizei>(Q2060_PC_REQUESTED_MSAA,
                          static_cast<GLsizei>(q2060GlMaxSamples));
    const GLsizei candidates[] = {8, 4, 2};
    GLenum finalStatus = 0u;

    for (GLsizei samples : candidates) {
        if (samples > capped) continue;

        while (glGetError() != GL_NO_ERROR) {}

        glBindFramebuffer(GL_FRAMEBUFFER, q2060MsaaFbo);

        glBindRenderbuffer(GL_RENDERBUFFER, q2060MsaaColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                         colorFormat, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, q2060MsaaColor);

        glBindRenderbuffer(GL_RENDERBUFFER, q2060MsaaDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                         GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, q2060MsaaDepth);

        finalStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        const GLenum error = glGetError();
        if (finalStatus == GL_FRAMEBUFFER_COMPLETE && error == GL_NO_ERROR) {
            q2060MsaaSamples = samples;
            q2060MsaaWidth = width;
            q2060MsaaHeight = height;
            q2060MsaaActive = true;
            Q6H_LOGI("Q20.6 MSAA READY: pcRequested=%dx glMax=%d chosen=%dx size=%dx%d color=%s depth=DEPTH_COMPONENT24 sceneTarget=multisample-renderbuffer resolve=COLOR+DEPTH postPipeline=unchanged swapchainSamples=1 transparencyMsaa=pending",
                     Q2060_PC_REQUESTED_MSAA, q2060GlMaxSamples,
                     q2060MsaaSamples, width, height,
                     colorFormat == GL_RGBA16F ? "RGBA16F" : "RGBA8");
            return true;
        }
    }

    q2060MsaaWidth = width;
    q2060MsaaHeight = height;
    Q6H_LOGW("Q20.6 MSAA FALLBACK: pcRequested=%dx glMax=%d chosen=1x status=0x%X color=%s reason=no-complete-multisample-target",
             Q2060_PC_REQUESTED_MSAA, q2060GlMaxSamples, finalStatus,
             colorFormat == GL_RGBA16F ? "RGBA16F" : "RGBA8");
    return true;
}

void Q2060ResolveEyeMsaaQ2060() {
    if (!q2060MsaaActive || q2060MsaaSamples <= 1 ||
        !q2060MsaaFbo || !q1280PostFbo) return;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, q2060MsaaFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, q1280PostFbo);
    glBlitFramebuffer(0, 0, q1280PostWidth, q1280PostHeight,
                      0, 0, q1280PostWidth, q1280PostHeight,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                      GL_NEAREST);
}

bool Q1280AllocatePostTarget(GLsizei width, GLsizei height) {
    if (width <= 0 || height <= 0) return false;
    if (!q1280PostFbo) glGenFramebuffers(1, &q1280PostFbo);
    if (!q1280PostColor) glGenTextures(1, &q1280PostColor);
    if (!q1370PostDepth) glGenTextures(1, &q1370PostDepth);

    if (q1280PostWidth == width && q1280PostHeight == height &&
        q1280PostColor && q1370PostDepth) {
        // Q20.6A: an already-valid single-sample HDR target must not bypass
        // multisample initialisation. This path can be reached by subsequent
        // eye/setup passes before the new MSAA state has been established.
        if (!q2060MsaaAttempted ||
            q2060MsaaWidth != width || q2060MsaaHeight != height) {
            Q6H_LOGI("Q20.6A MSAA ENSURE FROM POST CACHE: post=%dx%d attempted=%d active=%d",
                     width, height, q2060MsaaAttempted ? 1 : 0,
                     q2060MsaaActive ? 1 : 0);
            Q2060AllocateMsaaTargetQ2060(width, height, q1280PostInternalFormat);
        }
        return true;
    }

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
    Q2060AllocateMsaaTargetQ2060(width, height, q1280PostInternalFormat);
    if (!q1280PostLoggedGpu) {
        q1280PostLoggedGpu = true;
        Q6H_LOGI("Q20.4E CONTACT AO A/B READY: size=%dx%d depth=DEPTH_COMPONENT24 taps=8 maxDarken=0.220 radiusPx=2.5..7.0 defaultEnabled=0 toggle=RIGHT_B syntheticQuestEffect=1",
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
    glBindFramebuffer(GL_FRAMEBUFFER,
                      q2060MsaaActive ? q2060MsaaFbo : q1280PostFbo);
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
    glUniform1f(q204eContactAoEnabledLocation, q204eContactAoEnabled ? 1.0f : 0.0f);
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
        Q6H_LOGI("Q20.4F PC PRESENTATION: finalPassCall=4618221 gammaRampCall=39178 targetLum=1.200 saturation=0.875 tint=(0.739914 0.574956 0.312834) tintValue=0.600 contrastAvg=0.000 contrast=1.020 brightness=1.100 pcTarget=X8R8G8B8 backbufferQuantise=8bit gammaExponent=0.760000 gammaRGBIdentical=1 pcSrgbWrite=0 questSwapchain=GL_RGBA8 postGammaCodeToLinear=SRGB_DECODE");
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
    // Resolve the multisampled HDR scene into the existing single-sample
    // colour/depth textures before eye adaptation, bloom and final-film work.
    Q2060ResolveEyeMsaaQ2060();
    ++q2060MsaaHeartbeatFrame;
    if ((q2060MsaaHeartbeatFrame % 300u) == 1u) {
        Q6H_LOGI("Q20.6A MSAA HEARTBEAT: attempted=%d active=%d chosen=%dx glMax=%d xrRecommended=%u xrMax=%u size=%dx%d msaaFbo=%u postFbo=%u",
                 q2060MsaaAttempted ? 1 : 0,
                 q2060MsaaActive ? 1 : 0,
                 q2060MsaaSamples, q2060GlMaxSamples,
                 q2060OpenXrRecommendedSamples, q2060OpenXrMaxSamples,
                 q2060MsaaWidth, q2060MsaaHeight,
                 q2060MsaaFbo, q1280PostFbo);
    }
    if (eyeIndex == 0u) Q1350UpdateExposureQ1350();
    Q1280CompositeEyePostQ1280(swapchainFbo, width, height);
}

void Q1280ShutdownPostQ1280() {
    Q1670ShutdownPcBloomQ1670();

    if (gWaterReflectionDepthQ2090)
        glDeleteRenderbuffers(1, &gWaterReflectionDepthQ2090);
    if (gWaterReflectionColorQ2090)
        glDeleteTextures(1, &gWaterReflectionColorQ2090);
    if (gWaterReflectionFboQ2090)
        glDeleteFramebuffers(1, &gWaterReflectionFboQ2090);
    gWaterReflectionDepthQ2090 = 0u;
    gWaterReflectionColorQ2090 = 0u;
    gWaterReflectionFboQ2090 = 0u;
    gWaterReflectionTargetReadyQ2090 = false;
    gWaterReflectionTargetLoggedQ2090 = false;
    gWaterReflectionFramesQ2090 = 0u;
    gWaterSkyMvpReadyQ2090 = false;
    if (q2060MsaaColor) glDeleteRenderbuffers(1, &q2060MsaaColor);
    if (q2060MsaaDepth) glDeleteRenderbuffers(1, &q2060MsaaDepth);
    if (q2060MsaaFbo) glDeleteFramebuffers(1, &q2060MsaaFbo);
    q2060MsaaColor = q2060MsaaDepth = q2060MsaaFbo = 0u;
    q2060MsaaWidth = q2060MsaaHeight = 0;
    q2060MsaaSamples = 1;
    q2060MsaaActive = false;
    q2060MsaaAttempted = false;
    q2060MsaaHeartbeatFrame = 0u;
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
    ShutdownFo3WaterQ2070();
    glDeleteFramebuffers(n, framebuffers);
    ShutdownFo3CollisionOverlay();
    for (GpuObject& object : gObjects) {
        Q1900DeleteGpuShape(object);
    }
    gObjects.clear();
    Q230DeleteNpcActors();
    Q1910DrainDeferredGpuDeletesQ19(true);
    Q2017ForceClearSharedGeometry();
    if (gInstanceBufferQ2016) {
        glDeleteBuffers(1, &gInstanceBufferQ2016);
        gInstanceBufferQ2016 = 0u;
    }
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
    if (q2060MsaaActive && q2060MsaaFbo != 0u &&
        q1370CurrentFbo == static_cast<GLint>(q2060MsaaFbo)) {
        // Q20.6 multisampled colour/depth renderbuffers are already attached.
        // Do not replace the depth attachment with the legacy single-sample RB.
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

void ToggleFo3ContactAoQ204E() {
    q204eContactAoEnabled = !q204eContactAoEnabled;
}

bool GetFo3ContactAoEnabledQ204E() {
    return q204eContactAoEnabled;
}

void ToggleFo3EnvironmentPassQ205A() {
    gEnvironmentPassEnabledQ205A = !gEnvironmentPassEnabledQ205A;
}

bool GetFo3EnvironmentPassEnabledQ205A() {
    return gEnvironmentPassEnabledQ205A;
}

} // namespace

void SetFo3PlayerBodyTrackingQ210(
        float headX, float headY, float headZ, float headYaw,
        float bodyYaw,
        float localHeadY,
        bool leftValid, float leftX, float leftY, float leftZ,
        float leftQx, float leftQy, float leftQz, float leftQw,
        bool rightValid, float rightX, float rightY, float rightZ,
        float rightQx, float rightQy, float rightQz, float rightQw,
        float leftTrigger, float leftGrip,
        bool leftTriggerTouched, bool leftThumbTouched,
        float rightTrigger, float rightGrip,
        bool rightTriggerTouched, bool rightThumbTouched) {
    gQ210Head[0] = headX;
    gQ210Head[1] = headY;
    gQ210Head[2] = headZ;
    gQ210Head[3] = headYaw;
    gQ210LeftHandValid = leftValid;
    gQ210RightHandValid = rightValid;
    gQ210LeftHand[0] = leftX;
    gQ210LeftHand[1] = leftY;
    gQ210LeftHand[2] = leftZ;
    gQ218LeftHandQuat[0] = leftQx;
    gQ218LeftHandQuat[1] = leftQy;
    gQ218LeftHandQuat[2] = leftQz;
    gQ218LeftHandQuat[3] = leftQw;
    gQ210RightHand[0] = rightX;
    gQ210RightHand[1] = rightY;
    gQ210RightHand[2] = rightZ;
    gQ218RightHandQuat[0] = rightQx;
    gQ218RightHandQuat[1] = rightQy;
    gQ218RightHandQuat[2] = rightQz;
    gQ218RightHandQuat[3] = rightQw;

    gQ217FingerTrigger[0] =
        std::clamp(leftTrigger, 0.0f, 1.0f);
    gQ217FingerTrigger[1] =
        std::clamp(rightTrigger, 0.0f, 1.0f);
    gQ217FingerGrip[0] =
        std::clamp(leftGrip, 0.0f, 1.0f);
    gQ217FingerGrip[1] =
        std::clamp(rightGrip, 0.0f, 1.0f);
    gQ217TriggerTouched[0] = leftTriggerTouched;
    gQ217TriggerTouched[1] = rightTriggerTouched;
    gQ217ThumbTouched[0] = leftThumbTouched;
    gQ217ThumbTouched[1] = rightThumbTouched;

    ++gQ211TrackingSerial;

    // Q21.13: infer torso yaw with a neck dead-zone. Looking around within
    // +/-35 degrees leaves the shoulders alone. Beyond that, the torso follows
    // only the excess angle. Artificial snap/locomotion yaw is applied
    // immediately so the body does not lag behind snap turns.
    if (!gQ213TorsoYawReady) {
        gQ213TorsoYaw = bodyYaw;
        gQ213LastLocomotionYaw = bodyYaw;
        gQ213TorsoYawReady = true;
    } else {
        const float locomotionDelta =
            Q218WrapAngle(bodyYaw - gQ213LastLocomotionYaw);
        gQ213TorsoYaw =
            Q218WrapAngle(gQ213TorsoYaw + locomotionDelta);
        gQ213LastLocomotionYaw = bodyYaw;
    }

    const float headRelative =
        Q218WrapAngle(headYaw - gQ213TorsoYaw);
    if (std::fabs(headRelative) > Q213_NECK_YAW_LIMIT) {
        const float desiredTorso =
            Q218WrapAngle(
                headYaw -
                std::copysign(Q213_NECK_YAW_LIMIT, headRelative));
        const float followDelta =
            Q218WrapAngle(desiredTorso - gQ213TorsoYaw);
        gQ213TorsoYaw = Q218WrapAngle(
            gQ213TorsoYaw +
            std::clamp(
                followDelta,
                -Q213_TORSO_FOLLOW_MAX_STEP,
                Q213_TORSO_FOLLOW_MAX_STEP));
    }

    const float resolvedBodyYaw = gQ213TorsoYaw;
    const float c = std::cos(resolvedBodyYaw);
    const float s = std::sin(resolvedBodyYaw);
    const float rootX = headX + s * 0.08f;
    const float rootZ = headZ + c * 0.08f;

    // Q21.7: align the authored Fallout head/neck anchor to the real HMD
    // vertically. Previously the body root stayed on the legacy LOCAL-space
    // floor while the arm targets were re-anchored to the authored head.
    // Any difference between those two head heights therefore shifted BOTH
    // virtual hands by that amount and left the camera sunk into the torso.
    Vec3 q217AvatarHeadAnchor{};
    const bool q217HeadAnchorReady =
        Q211FindAvatarHeadAnchor(q217AvatarHeadAnchor);
    const float q217LegacyRootY = headY - localHeadY;
    const float q217RootY = q217HeadAnchorReady
        ? headY - q217AvatarHeadAnchor.y
        : q217LegacyRootY;

    std::fill(gQ210PlayerRoot, gQ210PlayerRoot + 16, 0.0f);
    gQ210PlayerRoot[0] = c;
    gQ210PlayerRoot[2] = -s;
    gQ210PlayerRoot[5] = 1.0f;
    gQ210PlayerRoot[8] = s;
    gQ210PlayerRoot[10] = c;
    gQ210PlayerRoot[12] = rootX;
    gQ210PlayerRoot[13] = q217RootY;
    gQ210PlayerRoot[14] = rootZ;
    gQ210PlayerRoot[15] = 1.0f;

    if ((gQ211TrackingSerial % 180u) == 1u) {
        Q6H_LOGI("Q21.13 BODY HEAD ALIGN: ready=%d headYaw=%.1fdeg locomotionYaw=%.1fdeg torsoYaw=%.1fdeg neckYaw=%.1fdeg neckLimit=35deg headWorldY=%.3f localHeadY=%.3f authoredHeadY=%.3f legacyRootY=%.3f alignedRootY=%.3f correction=%.3f",
                 q217HeadAnchorReady ? 1 : 0,
                 headYaw * 57.2957795f,
                 bodyYaw * 57.2957795f,
                 resolvedBodyYaw * 57.2957795f,
                 Q218WrapAngle(headYaw - resolvedBodyYaw) * 57.2957795f,
                 headY, localHeadY,
                 q217HeadAnchorReady ? q217AvatarHeadAnchor.y : localHeadY,
                 q217LegacyRootY, q217RootY,
                 q217RootY - q217LegacyRootY);
    }
}

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
