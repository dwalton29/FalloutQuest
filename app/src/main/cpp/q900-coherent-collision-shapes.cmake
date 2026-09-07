# Q9.0: reconstruct the already-authored Fallout collision triangles into
# coherent per-bhk-shape mesh colliders for character traversal.
#
# Q8.x still queried anonymous world triangles when deciding whether a low face
# was a wall or a step. Q8.1 already preserved a stable meshKeyQ801 made from
# (placed reference, source Havok shape block), so Q9.0 uses that key to rebuild
# the shape boundary at runtime without changing the proven NIF parser.
#
# The mesh remains concave indexed triangles (NOT a convex hull). Character
# stepping now asks the coherent shape containing the blocker for a walkable top
# surface and transfers support onto that surface when it is within FO3 step
# height. Internal triangles are still retained for exact geometry/materials.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q803.inc"
     Q900_CONTROLLER_SOURCE)
string(REPLACE "Q8.3" "Q9.0"
       Q900_CONTROLLER_SOURCE "${Q900_CONTROLLER_SOURCE}")

set(Q900_COHERENT_SHAPE_SOURCE [=[
struct HkCoherentShapeQ900 {
    uint64_t key = 0u;
    std::vector<size_t> triangles;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
    size_t walkableTriangles = 0u;
};

struct HkShapeLandingQ900 {
    bool found = false;
    uint64_t shapeKey = 0u;
    size_t triangleIndex = 0u;
    float y = 0.0f;
    float probeX = 0.0f;
    float probeZ = 0.0f;
    float probeAhead = 0.0f;
    int supportPoints = 0;
};

std::vector<HkCoherentShapeQ900> gHkShapesQ900;
std::unordered_map<uint64_t, size_t> gHkShapeIndexQ900;
const CollisionTriangle* gHkShapeTriangleDataQ900 = nullptr;
size_t gHkShapeTriangleCountQ900 = 0u;
bool gHkShapesReadyQ900 = false;
uint64_t gHkShapeReadyLogsQ900 = 0u;
uint64_t gHkShapeStepLogsQ900 = 0u;
uint64_t gHkShapeNoTopLogsQ900 = 0u;
uint64_t gHkShapeBlockedLogsQ900 = 0u;

void EnsureHkCoherentShapesQ900() {
    const CollisionTriangle* data = gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();
    if (gHkShapesReadyQ900 && data == gHkShapeTriangleDataQ900 &&
        gHkShapeTriangleCountQ900 == gWorldTriangles.size()) return;

    gHkShapesQ900.clear();
    gHkShapeIndexQ900.clear();
    gHkShapeIndexQ900.reserve(gWorldTriangles.size() / 8u + 32u);

    size_t shapedTriangles = 0u;
    size_t walkableTriangles = 0u;
    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (tri.meshKeyQ801 == 0u) continue; // LAND/legacy geometry stays on existing path.
        ++shapedTriangles;

        size_t shapeIndex = 0u;
        const auto existing = gHkShapeIndexQ900.find(tri.meshKeyQ801);
        if (existing == gHkShapeIndexQ900.end()) {
            shapeIndex = gHkShapesQ900.size();
            HkCoherentShapeQ900 shape;
            shape.key = tri.meshKeyQ801;
            shape.minX = tri.minX; shape.maxX = tri.maxX;
            shape.minY = tri.minY; shape.maxY = tri.maxY;
            shape.minZ = tri.minZ; shape.maxZ = tri.maxZ;
            gHkShapesQ900.push_back(std::move(shape));
            gHkShapeIndexQ900.emplace(tri.meshKeyQ801, shapeIndex);
        } else {
            shapeIndex = existing->second;
        }

        HkCoherentShapeQ900& shape = gHkShapesQ900[shapeIndex];
        shape.triangles.push_back(triIndex);
        shape.minX = std::min(shape.minX, tri.minX); shape.maxX = std::max(shape.maxX, tri.maxX);
        shape.minY = std::min(shape.minY, tri.minY); shape.maxY = std::max(shape.maxY, tri.maxY);
        shape.minZ = std::min(shape.minZ, tri.minZ); shape.maxZ = std::max(shape.maxZ, tri.maxZ);
        if (std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri)) {
            ++shape.walkableTriangles;
            ++walkableTriangles;
        }
    }

    gHkShapeTriangleDataQ900 = data;
    gHkShapeTriangleCountQ900 = gWorldTriangles.size();
    gHkShapesReadyQ900 = true;
    if (gHkShapeReadyLogsQ900 < 8u) {
        ++gHkShapeReadyLogsQ900;
        Q6G_LOGI("Q9.0 COHERENT SHAPES READY: shapes=%zu shapedTriangles=%zu walkableTriangles=%zu worldTriangles=%zu mode=placed-bhk-shape+concave-mesh",
                 gHkShapesQ900.size(), shapedTriangles, walkableTriangles,
                 gWorldTriangles.size());
    }
}

const HkCoherentShapeQ900* FindHkShapeQ900(uint64_t key) {
    if (key == 0u) return nullptr;
    EnsureHkCoherentShapesQ900();
    const auto found = gHkShapeIndexQ900.find(key);
    if (found == gHkShapeIndexQ900.end() || found->second >= gHkShapesQ900.size()) return nullptr;
    return &gHkShapesQ900[found->second];
}

bool HkShapeSurfaceAtQ900(const HkCoherentShapeQ900& shape,
                          float x, float z, float targetY,
                          float toleranceY) {
    for (size_t triIndex : shape.triangles) {
        if (triIndex >= gWorldTriangles.size()) continue;
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
        if (x < tri.minX - 0.006f || x > tri.maxX + 0.006f ||
            z < tri.minZ - 0.006f || z > tri.maxZ + 0.006f) continue;
        float u = 0.0f, v = 0.0f, w = 0.0f;
        if (!BarycentricXZ(tri, x, z, u, v, w)) continue;
        const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
        if (std::fabs(y - targetY) <= toleranceY) return true;
    }
    return false;
}

int HkShapeFootprintSupportQ900(const HkCoherentShapeQ900& shape,
                                float x, float z, float y,
                                float nx, float nz) {
    const float r = CollisionRadiusQ712();
    const float lx = -nz, lz = nx;
    const float side = r * 0.40f;
    const float fore = r * 0.32f;
    const std::array<Vec2,5> probes{{
        {0.0f, 0.0f},
        { lx*side, lz*side}, {-lx*side,-lz*side},
        { nx*fore, nz*fore}, {-nx*fore,-nz*fore}
    }};
    int support = 0;
    for (const Vec2& p : probes) {
        if (HkShapeSurfaceAtQ900(shape, x+p.x, z+p.y, y, 0.055f)) ++support;
    }
    return support;
}

HkShapeLandingQ900 FindHkShapeLandingOnQ900(const HkCoherentShapeQ900& shape,
                                             float startX, float startZ,
                                             float targetX, float targetZ,
                                             float feetY, float contactRise) {
    HkShapeLandingQ900 result;
    if (shape.walkableTriangles == 0u) return result;
    const float dx = targetX - startX;
    const float dz = targetZ - startZ;
    const float len = std::sqrt(dx*dx + dz*dz);
    if (len < 1e-6f) return result;
    const float nx = dx / len, nz = dz / len;
    const float r = CollisionRadiusQ712();
    const float maxRise = StepHeightQ78B() + PLAYER_SKIN + 0.020f;
    const std::array<float,6> ahead{{0.0f, r*0.25f, r*0.50f,
                                     r*0.75f, r*1.00f, r*1.10f}};

    for (float distance : ahead) {
        const float px = targetX + nx*distance;
        const float pz = targetZ + nz*distance;
        if (px < shape.minX - r || px > shape.maxX + r ||
            pz < shape.minZ - r || pz > shape.maxZ + r) continue;

        HkShapeLandingQ900 best;
        float bestScore = std::numeric_limits<float>::max();
        for (size_t triIndex : shape.triangles) {
            if (triIndex >= gWorldTriangles.size()) continue;
            const CollisionTriangle& tri = gWorldTriangles[triIndex];
            if (std::fabs(tri.normal.y) < WalkableNormalThresholdQ714(tri)) continue;
            if (px < tri.minX - 0.006f || px > tri.maxX + 0.006f ||
                pz < tri.minZ - 0.006f || pz > tri.maxZ + 0.006f) continue;
            float u = 0.0f, v = 0.0f, w = 0.0f;
            if (!BarycentricXZ(tri, px, pz, u, v, w)) continue;
            const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
            const float rise = y - feetY;
            if (rise < 0.006f || rise > maxRise) continue;

            const int support = HkShapeFootprintSupportQ900(shape, px, pz, y, nx, nz);
            if (support < 2) continue;

            // Prefer the top surface whose elevation best matches the face that
            // actually blocked the capsule. This rejects the old floor while
            // naturally following ramps and stair treads on the same shape.
            const float desiredRise = std::clamp(contactRise, 0.006f, maxRise);
            const float score = std::fabs(rise - desiredRise) + distance * 0.08f;
            if (!best.found || score < bestScore - 0.0005f ||
                (std::fabs(score-bestScore) <= 0.0005f && support > best.supportPoints)) {
                best.found = true;
                best.shapeKey = shape.key;
                best.triangleIndex = triIndex;
                best.y = y;
                best.probeX = px; best.probeZ = pz;
                best.probeAhead = distance;
                best.supportPoints = support;
                bestScore = score;
            }
        }
        if (best.found) return best; // nearest probe that reaches the top wins.
    }
    return result;
}

bool HkShapeAwareLandingClearQ900(float targetX, float targetZ, float landingY,
                                  uint64_t supportShapeKey,
                                  const std::vector<uint64_t>& blockerShapeKeys,
                                  std::string& reason) {
    std::vector<HkProxyContactQ800> contacts;
    GatherHkProxyContactsQ800(targetX, targetZ, landingY,
                              Q800_KEEP_DISTANCE, contacts);
    const float lowFaceAllowance = 0.075f;
    const float upperBodyY = landingY + PLAYER_HEIGHT * 0.58f;

    auto isBlockerShape = [&](uint64_t key) {
        if (key == supportShapeKey) return true;
        return std::find(blockerShapeKeys.begin(), blockerShapeKeys.end(), key) != blockerShapeKeys.end();
    };

    for (const HkProxyContactQ800& contact : contacts) {
        if (contact.triangleIndex >= gWorldTriangles.size()) continue;
        const CollisionTriangle& tri = gWorldTriangles[contact.triangleIndex];

        // Any contact in the upper body/head region remains solid, including a
        // horizontal ceiling whose normal would otherwise look "walkable".
        if (contact.pointY >= upperBodyY) {
            reason = "upper-body";
            return false;
        }

        if (HkContactIsWalkableQ800(contact)) continue;

        // The riser/end face belonging to the shape we are climbing is below
        // the newly selected support plane, so it is an internal boundary of
        // the coherent collider, not a wall at the new character height.
        if (isBlockerShape(tri.meshKeyQ801)) {
            if (contact.pointY <= landingY + lowFaceAllowance ||
                tri.maxY <= landingY + lowFaceAllowance) continue;
        }

        // A floor lip wholly below the new support plane cannot intersect the
        // character after the support transfer.
        if (tri.maxY <= landingY + PLAYER_SKIN + 0.010f) continue;

        reason = "hard-shape";
        return false;
    }
    return true;
}

bool TryHkCoherentShapeLedgeQ900(float startX, float startZ,
                                 float targetX, float targetZ,
                                 float feetY,
                                 const std::vector<HkProxyContactQ800>& blockers,
                                 float& outFeetY) {
    if (blockers.empty()) return false;
    EnsureHkCoherentShapesQ900();

    float highestRise = -std::numeric_limits<float>::max();
    std::vector<uint64_t> blockerShapeKeys;
    blockerShapeKeys.reserve(blockers.size());
    for (const HkProxyContactQ800& blocker : blockers) {
        highestRise = std::max(highestRise, blocker.pointY - feetY);
        if (blocker.triangleIndex >= gWorldTriangles.size()) continue;
        const uint64_t key = gWorldTriangles[blocker.triangleIndex].meshKeyQ801;
        if (key != 0u && std::find(blockerShapeKeys.begin(), blockerShapeKeys.end(), key) == blockerShapeKeys.end()) {
            blockerShapeKeys.push_back(key);
        }
    }
    if (highestRise > StepHeightQ78B() + PLAYER_SKIN + 0.020f) return false;
    if (blockerShapeKeys.empty()) return false;

    HkShapeLandingQ900 landing;
    float bestError = std::numeric_limits<float>::max();
    for (uint64_t key : blockerShapeKeys) {
        const HkCoherentShapeQ900* shape = FindHkShapeQ900(key);
        if (!shape) continue;
        HkShapeLandingQ900 candidate = FindHkShapeLandingOnQ900(
            *shape, startX, startZ, targetX, targetZ, feetY, highestRise);
        if (!candidate.found) continue;
        const float error = std::fabs((candidate.y-feetY) - highestRise) + candidate.probeAhead*0.08f;
        if (!landing.found || error < bestError) {
            landing = candidate;
            bestError = error;
        }
    }

    // Adjacent modular pieces are separate placed Havok shapes. If the blocker
    // shape has no top at the forward probe (e.g. ramp -> next ramp), search
    // nearby coherent shapes, still as complete shapes rather than world-triangle soup.
    if (!landing.found) {
        const float dx = targetX-startX, dz = targetZ-startZ;
        const float len = std::sqrt(dx*dx + dz*dz);
        if (len > 1e-6f) {
            const float nx=dx/len, nz=dz/len, r=CollisionRadiusQ712();
            const float px=targetX+nx*r*0.80f, pz=targetZ+nz*r*0.80f;
            for (const HkCoherentShapeQ900& shape : gHkShapesQ900) {
                if (std::find(blockerShapeKeys.begin(), blockerShapeKeys.end(), shape.key) != blockerShapeKeys.end()) continue;
                if (shape.walkableTriangles == 0u) continue;
                if (px < shape.minX-r || px > shape.maxX+r || pz < shape.minZ-r || pz > shape.maxZ+r) continue;
                if (shape.maxY < feetY-0.18f || shape.minY > feetY+StepHeightQ78B()+0.08f) continue;
                HkShapeLandingQ900 candidate = FindHkShapeLandingOnQ900(
                    shape, startX, startZ, targetX, targetZ, feetY, highestRise);
                if (!candidate.found) continue;
                const float error=std::fabs((candidate.y-feetY)-highestRise)+candidate.probeAhead*0.08f+0.02f;
                if (!landing.found || error<bestError) { landing=candidate; bestError=error; }
            }
        }
    }

    if (!landing.found) {
        if (gHkShapeNoTopLogsQ900 < 120u || (gResolveCounter % 240u) == 0u) {
            ++gHkShapeNoTopLogsQ900;
            Q6G_LOGI("Q9.0 SHAPE LEDGE NO TOP: from=(%.3f %.3f) target=(%.3f %.3f) contactRise=%.3f blockerShapes=%zu",
                     startX,startZ,targetX,targetZ,highestRise,blockerShapeKeys.size());
        }
        return false;
    }

    std::string clearReason;
    if (!HkShapeAwareLandingClearQ900(targetX,targetZ,landing.y,
                                      landing.shapeKey,blockerShapeKeys,clearReason)) {
        if (gHkShapeBlockedLogsQ900 < 120u || (gResolveCounter % 240u) == 0u) {
            ++gHkShapeBlockedLogsQ900;
            Q6G_LOGI("Q9.0 SHAPE LEDGE BLOCKED: reason=%s shape=%llu landingRise=%.3f probeAhead=%.3f support=%d",
                     clearReason.c_str(), static_cast<unsigned long long>(landing.shapeKey),
                     landing.y-feetY, landing.probeAhead, landing.supportPoints);
        }
        return false;
    }

    outFeetY = landing.y;
    if (gHkShapeStepLogsQ900 < 180u || (gResolveCounter % 240u) == 0u) {
        ++gHkShapeStepLogsQ900;
        Q6G_LOGI("Q9.0 SHAPE LEDGE CLIMB: shape=%llu contactRise=%.3f landingRise=%.3f probeAhead=%.3f support=%d tri=%zu",
                 static_cast<unsigned long long>(landing.shapeKey), highestRise,
                 landing.y-feetY, landing.probeAhead, landing.supportPoints,
                 landing.triangleIndex);
    }
    return true;
}

]=])

string(REPLACE
    "bool TryHkLowObstacleTraverseQ802(float startX, float startZ,"
    "${Q900_COHERENT_SHAPE_SOURCE}bool TryHkLowObstacleTraverseQ802(float startX, float startZ,"
    Q900_CONTROLLER_SOURCE "${Q900_CONTROLLER_SOURCE}")

# Route both primary and slide step attempts through coherent-shape ledge
# traversal. Q8.3's old all-world-triangle traversal stays compiled only as a
# fallback implementation reference; it is no longer called by movement.
string(REPLACE
    "if (TryHkLowObstacleTraverseQ802(x, z, targetX, targetZ, feetY, blockers, stepY))"
    "if (TryHkCoherentShapeLedgeQ900(x, z, targetX, targetZ, feetY, blockers, stepY))"
    Q900_CONTROLLER_SOURCE "${Q900_CONTROLLER_SOURCE}")
string(REPLACE
    "if (TryHkLowObstacleTraverseQ802(x, z, slideTargetX, slideTargetZ,\n                                      feetY, slideBlockers, slideStepY))"
    "if (TryHkCoherentShapeLedgeQ900(x, z, slideTargetX, slideTargetZ,\n                                     feetY, slideBlockers, slideStepY))"
    Q900_CONTROLLER_SOURCE "${Q900_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=validated-low-obstacle-traversal+all-surface-landing+authored-welding+linear-cast+support-footprint+simplex persistentOverlap=keep-distance"
    "mode=coherent-bhk-shapes+shape-ledges+authored-welding+linear-cast+simplex persistentOverlap=keep-distance"
    Q900_CONTROLLER_SOURCE "${Q900_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q900.inc"
     "${Q900_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q803.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q900.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
