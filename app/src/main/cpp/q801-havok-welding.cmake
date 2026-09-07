# Q8.1: preserve Fallout 3's authored packed-triangle Havok welding metadata
# and use it for universal internal/low-edge ghost-contact removal.
#
# Q8.0 reproduced more of hkpCharacterProxy's response architecture, but the
# collision parser was already reading the per-triangle 16-bit welding field and
# discarding it. That meant the proxy received unwelded triangle soup.

# -----------------------------------------------------------------------------
# 1) Preserve the authored hkTriangle::welding_info from hkPackedNiTriStripsData.
#    The source checkout is disposable in CI, so patching this header at CMake
#    configure time avoids another permanent fork of the parser while keeping
#    the runtime data structure exact.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/fo3-nif-collision-q6f.h" Q801_NIF_HEADER)

string(REPLACE
    "    std::vector<uint32_t> indices;\n    std::string modelPath;"
    "    std::vector<uint32_t> indices;\n    // Q8.1: one authored Havok welding value per emitted packed triangle.\n    std::vector<uint16_t> triangleWeldingInfo;\n    std::string modelPath;"
    Q801_NIF_HEADER "${Q801_NIF_HEADER}")

string(REPLACE
    "    struct Tri { uint16_t a,b,c; };"
    "    struct Tri { uint16_t a,b,c,weld; };"
    Q801_NIF_HEADER "${Q801_NIF_HEADER}")

string(REPLACE
    "        tris.push_back({a,b,d});"
    "        tris.push_back({a,b,d,weld});"
    Q801_NIF_HEADER "${Q801_NIF_HEADER}")

string(REPLACE
    "    out.indices.clear();\n    out.indices.reserve(static_cast<size_t>(triangleCount) * 3u);"
    "    out.indices.clear();\n    out.indices.reserve(static_cast<size_t>(triangleCount) * 3u);\n    out.triangleWeldingInfo.clear();\n    out.triangleWeldingInfo.reserve(triangleCount);"
    Q801_NIF_HEADER "${Q801_NIF_HEADER}")

string(REPLACE
    "        out.indices.insert(out.indices.end(), {t.a,t.b,t.c});"
    "        out.indices.insert(out.indices.end(), {t.a,t.b,t.c});\n        out.triangleWeldingInfo.push_back(t.weld);"
    Q801_NIF_HEADER "${Q801_NIF_HEADER}")

file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/fo3-nif-collision-q6f.h" "${Q801_NIF_HEADER}")

# -----------------------------------------------------------------------------
# 2) Carry welding + exact mesh vertex identity into world collision triangles.
#    Vertex ids let us reconstruct authored shared edges without fuzzy position
#    matching after placement transforms.
# -----------------------------------------------------------------------------
string(REPLACE
    "    bool stairsQ714 = false;\n    bool platformQ714 = false;\n};"
    "    bool stairsQ714 = false;\n    bool platformQ714 = false;\n\n    // Q8.1: packed Havok welding and exact source-mesh edge identity.\n    uint16_t weldingInfoQ801 = 0u;\n    uint64_t meshKeyQ801 = 0u;\n    uint32_t vertexAQ801 = 0xffffffffu;\n    uint32_t vertexBQ801 = 0xffffffffu;\n    uint32_t vertexCQ801 = 0xffffffffu;\n};"
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

string(REPLACE
    "                worldTriangle.surfaceKeyQ714 = MakeSurfaceKeyQ714(\n                    placement.refFormId, shape.sourceShapeBlock, subShapeIndexQ714);"
    "                worldTriangle.surfaceKeyQ714 = MakeSurfaceKeyQ714(\n                    placement.refFormId, shape.sourceShapeBlock, subShapeIndexQ714);\n                worldTriangle.meshKeyQ801 = MakeSurfaceKeyQ714(\n                    placement.refFormId, shape.sourceShapeBlock, 0xfffeu);\n                worldTriangle.vertexAQ801 = ia;\n                worldTriangle.vertexBQ801 = ib;\n                worldTriangle.vertexCQ801 = ic;\n                const size_t packedTriOrdinalQ801 = i / 3u;\n                if (packedTriOrdinalQ801 < shape.triangleWeldingInfo.size()) {\n                    worldTriangle.weldingInfoQ801 =\n                        shape.triangleWeldingInfo[packedTriOrdinalQ801];\n                }"
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")

# -----------------------------------------------------------------------------
# 3) Patch the Q8.0 clean-room proxy to consume authored welding. We reconstruct
#    exact shared edges by source vertex id, then suppress a non-walkable contact
#    only when:
#      - it lies on a shared authored edge,
#      - at least one adjacent triangle has authored welding info,
#      - the neighbour is walkable,
#      - the edge is within normal FO3 step reach.
#    Tall walls and non-welded boundaries remain blockers.
# -----------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q800.inc"
     Q801_CONTROLLER_SOURCE)
string(REPLACE "Q8.0" "Q8.1"
       Q801_CONTROLLER_SOURCE "${Q801_CONTROLLER_SOURCE}")

set(Q801_WELD_SOURCE [=[
struct HkWeldEdgeKeyQ801 {
    uint64_t mesh = 0u;
    uint32_t a = 0u;
    uint32_t b = 0u;
    bool operator==(const HkWeldEdgeKeyQ801& other) const {
        return mesh == other.mesh && a == other.a && b == other.b;
    }
};

struct HkWeldEdgeHashQ801 {
    size_t operator()(const HkWeldEdgeKeyQ801& key) const {
        uint64_t h = key.mesh ^ (static_cast<uint64_t>(key.a) << 32u) ^ key.b;
        h ^= h >> 33u; h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33u; h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33u;
        return static_cast<size_t>(h);
    }
};

std::vector<std::array<int32_t, 3>> gHkWeldNeighboursQ801;
bool gHkWeldAdjacencyReadyQ801 = false;
size_t gHkWeldTriangleCountQ801 = 0u;
const CollisionTriangle* gHkWeldTriangleDataQ801 = nullptr;
uint64_t gHkWeldPassLogsQ801 = 0u;
uint64_t gHkWeldReadyLogsQ801 = 0u;

void EnsureHkWeldAdjacencyQ801() {
    const CollisionTriangle* dataPtr = gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();
    if (gHkWeldAdjacencyReadyQ801 &&
        gHkWeldTriangleCountQ801 == gWorldTriangles.size() &&
        gHkWeldTriangleDataQ801 == dataPtr) return;

    gHkWeldNeighboursQ801.assign(gWorldTriangles.size(), std::array<int32_t,3>{-1,-1,-1});
    struct EdgeOwnerQ801 { size_t tri = 0u; int edge = 0; };
    std::unordered_map<HkWeldEdgeKeyQ801, EdgeOwnerQ801, HkWeldEdgeHashQ801> first;
    first.reserve(gWorldTriangles.size() * 2u);

    size_t authoredWeldTriangles = 0u;
    size_t sharedEdges = 0u;
    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (tri.weldingInfoQ801 != 0u) ++authoredWeldTriangles;
        if (tri.meshKeyQ801 == 0u ||
            tri.vertexAQ801 == 0xffffffffu ||
            tri.vertexBQ801 == 0xffffffffu ||
            tri.vertexCQ801 == 0xffffffffu) continue;

        const std::array<std::pair<uint32_t,uint32_t>,3> edges{{
            {tri.vertexAQ801, tri.vertexBQ801},
            {tri.vertexBQ801, tri.vertexCQ801},
            {tri.vertexCQ801, tri.vertexAQ801}
        }};
        for (int edge = 0; edge < 3; ++edge) {
            uint32_t a = edges[edge].first;
            uint32_t b = edges[edge].second;
            if (a > b) std::swap(a, b);
            const HkWeldEdgeKeyQ801 key{tri.meshKeyQ801, a, b};
            const auto found = first.find(key);
            if (found == first.end()) {
                first.emplace(key, EdgeOwnerQ801{triIndex, edge});
            } else {
                const EdgeOwnerQ801 owner = found->second;
                if (gHkWeldNeighboursQ801[owner.tri][owner.edge] < 0 &&
                    gHkWeldNeighboursQ801[triIndex][edge] < 0) {
                    gHkWeldNeighboursQ801[owner.tri][owner.edge] = static_cast<int32_t>(triIndex);
                    gHkWeldNeighboursQ801[triIndex][edge] = static_cast<int32_t>(owner.tri);
                    ++sharedEdges;
                }
            }
        }
    }

    gHkWeldAdjacencyReadyQ801 = true;
    gHkWeldTriangleCountQ801 = gWorldTriangles.size();
    gHkWeldTriangleDataQ801 = dataPtr;
    if (gHkWeldReadyLogsQ801 < 8u) {
        ++gHkWeldReadyLogsQ801;
        Q6G_LOGI("Q8.1 HAVOK WELD READY: triangles=%zu authoredWeldTriangles=%zu sharedEdges=%zu mode=packed-welding+exact-vertex-adjacency",
                 gWorldTriangles.size(), authoredWeldTriangles, sharedEdges);
    }
}

float HkPointSegmentDistanceQ801(float px, float py, float pz,
                                 const Vec3& a, const Vec3& b) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float abz = b.z - a.z;
    const float denom = abx*abx + aby*aby + abz*abz;
    if (denom <= 1e-12f) {
        const float dx = px - a.x, dy = py - a.y, dz = pz - a.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    float t = ((px-a.x)*abx + (py-a.y)*aby + (pz-a.z)*abz) / denom;
    t = std::clamp(t, 0.0f, 1.0f);
    const float qx = a.x + abx*t;
    const float qy = a.y + aby*t;
    const float qz = a.z + abz*t;
    const float dx = px-qx, dy = py-qy, dz = pz-qz;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

bool HkSuppressAuthoredWeldGhostQ801(const HkProxyContactQ800& contact,
                                     float feetY) {
    if (contact.triangleIndex >= gWorldTriangles.size()) return false;
    EnsureHkWeldAdjacencyQ801();
    if (contact.triangleIndex >= gHkWeldNeighboursQ801.size()) return false;

    const CollisionTriangle& tri = gWorldTriangles[contact.triangleIndex];
    if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) return false;

    const std::array<std::pair<Vec3,Vec3>,3> edges{{
        {tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}
    }};
    const float edgeTolerance = std::max(0.018f, Q800_KEEP_CONTACT_TOLERANCE * 0.90f);
    const float maxReachY = feetY + StepHeightQ78B() + CollisionRadiusQ712() * 0.25f;
    const float minReachY = feetY - 0.20f;

    for (int edge = 0; edge < 3; ++edge) {
        const int32_t neighbourIndex = gHkWeldNeighboursQ801[contact.triangleIndex][edge];
        if (neighbourIndex < 0 || static_cast<size_t>(neighbourIndex) >= gWorldTriangles.size()) continue;
        const CollisionTriangle& neighbour = gWorldTriangles[static_cast<size_t>(neighbourIndex)];

        // This is the key OG-data gate: no authored welding on either side means
        // this is a normal mesh boundary and must remain solid.
        if (tri.weldingInfoQ801 == 0u && neighbour.weldingInfoQ801 == 0u) continue;
        if (std::fabs(neighbour.normal.y) < WalkableNormalThresholdQ714(neighbour)) continue;

        const float edgeDistance = HkPointSegmentDistanceQ801(
            contact.pointX, contact.pointY, contact.pointZ,
            edges[edge].first, edges[edge].second);
        if (edgeDistance > edgeTolerance) continue;

        const float edgeMinY = std::min(edges[edge].first.y, edges[edge].second.y);
        const float edgeMaxY = std::max(edges[edge].first.y, edges[edge].second.y);
        if (edgeMaxY > maxReachY || edgeMaxY < minReachY) continue;

        if (gHkWeldPassLogsQ801 < 160u || (gResolveCounter % 240u) == 0u) {
            ++gHkWeldPassLogsQ801;
            Q6G_LOGI("Q8.1 HAVOK WELD EDGE PASS: tri=%zu neighbour=%d weld=0x%04X neighbourWeld=0x%04X edge=%d edgeDist=%.4f edgeRise=(%.3f..%.3f) neighbourNormalY=%.3f",
                     contact.triangleIndex, neighbourIndex,
                     static_cast<unsigned>(tri.weldingInfoQ801),
                     static_cast<unsigned>(neighbour.weldingInfoQ801),
                     edge, edgeDistance, edgeMinY-feetY, edgeMaxY-feetY,
                     neighbour.normal.y);
        }
        return true;
    }
    return false;
}

]=])

string(REPLACE
    "void GatherHkProxyContactsQ800(float x, float z, float feetY, float tolerance,"
    "${Q801_WELD_SOURCE}void GatherHkProxyContactsQ800(float x, float z, float feetY, float tolerance,"
    Q801_CONTROLLER_SOURCE "${Q801_CONTROLLER_SOURCE}")

string(REPLACE
    "        candidate.platform = tri.platformQ714;\n\n        // Havok manifolds contain contact points/planes, not one entry per model."
    "        candidate.platform = tri.platformQ714;\n\n        // Q8.1: apply Fallout's authored packed-triangle welding before this\n        // contact becomes a proxy constraint. This removes ghost side/end-edge\n        // planes while leaving unwelded/tall boundaries solid.\n        if (HkSuppressAuthoredWeldGhostQ801(candidate, feetY)) continue;\n\n        // Havok manifolds contain contact points/planes, not one entry per model."
    Q801_CONTROLLER_SOURCE "${Q801_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=fo3-havok-proxy-cleanroom+linear-cast+persistent-point-manifold+simplex+support-check persistentOverlap=keep-distance"
    "mode=fo3-havok-proxy-cleanroom+authored-welding+linear-cast+persistent-point-manifold+simplex+support-check persistentOverlap=keep-distance"
    Q801_CONTROLLER_SOURCE "${Q801_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q801.inc"
     "${Q801_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q800.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q801.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
