# Q9.4: keep Q9.3's low-lip/seam traversal, but fix real stairs and the
# movement-time stutter it introduced.
#
# Two changes are intentionally coupled here:
#   1) a positive riser searches for the NEXT raised walkable support first,
#      including a short look-ahead. The old/current floor may only be used as
#      fallback when no raised tread exists. This is the stair fix.
#   2) step validation builds one local triangle candidate list and reuses it
#      for landing + final upper-body occupancy. Q9.3's sampled full-world
#      upper-body sweep is no longer on the active step path.
#
# Exterior scrap steps measured around 0.307-0.312 m, so the effective rise
# budget is the authored controller step height plus 0.020 m tolerance.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q930.inc"
     Q940_CONTROLLER_SOURCE)
string(REPLACE "Q9.3" "Q9.4"
       Q940_CONTROLLER_SOURCE "${Q940_CONTROLLER_SOURCE}")

set(Q940_RAISED_SUPPORT_SOURCE [=[
constexpr float Q940_STEP_TOLERANCE = 0.020f;
constexpr float Q940_RAISED_MIN = 0.008f;
constexpr float Q940_LOCAL_PAD = 0.030f;
constexpr float Q940_LOOKAHEAD_MULT = 1.50f;
uint64_t gQ940LandingLogs = 0u;
uint64_t gQ940LocalLogs = 0u;
uint64_t gQ940PassLogs = 0u;
uint64_t gQ940FailLogs = 0u;

struct LocalCandidatesQ940 {
    std::vector<size_t> tris;
};

LocalCandidatesQ940 CollectLocalCandidatesQ940(float startX, float startZ,
                                                float targetX, float targetZ,
                                                float feetY,
                                                float nx, float nz) {
    LocalCandidatesQ940 result;
    const float r = CollisionRadiusQ712();
    const float ahead = r * Q940_LOOKAHEAD_MULT;
    const float ax = targetX + nx * ahead;
    const float az = targetZ + nz * ahead;
    const float minX = std::min({startX, targetX, ax}) - r - Q940_LOCAL_PAD;
    const float maxX = std::max({startX, targetX, ax}) + r + Q940_LOCAL_PAD;
    const float minZ = std::min({startZ, targetZ, az}) - r - Q940_LOCAL_PAD;
    const float maxZ = std::max({startZ, targetZ, az}) + r + Q940_LOCAL_PAD;
    const float minY = feetY - Q910_MAX_STEP_DROP - r - 0.05f;
    const float maxY = feetY + PLAYER_HEIGHT + r + Q940_STEP_TOLERANCE;

    result.tris.reserve(192u);
    for (size_t i = 0u; i < gWorldTriangles.size(); ++i) {
        const CollisionTriangle& tri = gWorldTriangles[i];
        if (tri.maxX < minX || tri.minX > maxX ||
            tri.maxZ < minZ || tri.minZ > maxZ ||
            tri.maxY < minY || tri.minY > maxY) continue;
        result.tris.push_back(i);
    }

    if (gQ940LocalLogs < 80u || (gResolveCounter % 360u) == 0u) {
        ++gQ940LocalLogs;
        Q6G_LOGI("Q9.4 LOCAL CANDIDATES: world=%zu local=%zu ahead=%.3f",
                 gWorldTriangles.size(), result.tris.size(), ahead);
    }
    return result;
}

int LocalFootprintSupportQ940(const LocalCandidatesQ940& local,
                              float x, float z, float y,
                              float nx, float nz) {
    const float r = CollisionRadiusQ712();
    const float lx = -nz, lz = nx;
    const float side = r * 0.52f;
    const float fore = r * 0.82f;
    const std::array<Vec2,7> probes{{
        {0.0f,0.0f},
        {lx*side,lz*side},{-lx*side,-lz*side},
        {nx*fore,nz*fore},{-nx*fore,-nz*fore},
        {nx*fore+lx*side,nz*fore+lz*side},
        {nx*fore-lx*side,nz*fore-lz*side}
    }};

    int support = 0;
    for (const Vec2& p : probes) {
        const float px = x + p.x;
        const float pz = z + p.y;
        bool thisProbe = false;
        for (size_t triIndex : local.tris) {
            const CollisionTriangle& tri = gWorldTriangles[triIndex];
            if (!IsWalkableTriangleQ910(tri)) continue;
            if (px < tri.minX-0.008f || px > tri.maxX+0.008f ||
                pz < tri.minZ-0.008f || pz > tri.maxZ+0.008f) continue;
            float u=0.0f,v=0.0f,w=0.0f;
            if (!BarycentricXZ(tri,px,pz,u,v,w)) continue;
            const float sy = tri.a.y*u + tri.b.y*v + tri.c.y*w;
            if (std::fabs(sy-y) <= Q910_LANDING_TOLERANCE) {
                thisProbe = true;
                break;
            }
        }
        if (thisProbe) ++support;
    }
    return support;
}

DownSupportQ910 FindLocalSupportQ940(const LocalCandidatesQ940& local,
                                     float x, float z, float feetY,
                                     float nx, float nz,
                                     bool raisedOnly) {
    DownSupportQ910 result;
    const float maxRise = StepHeightQ78B() + Q940_STEP_TOLERANCE;
    const float minY = feetY - Q910_MAX_STEP_DROP;
    const float maxY = feetY + maxRise;
    const float r = CollisionRadiusQ712();
    const float lx = -nz, lz = nx;
    const float side = r * 0.52f;
    const float fore = r * 0.82f;
    const std::array<Vec2,7> probes{{
        {0.0f,0.0f},
        {lx*side,lz*side},{-lx*side,-lz*side},
        {nx*fore,nz*fore},{-nx*fore,-nz*fore},
        {nx*fore+lx*side,nz*fore+lz*side},
        {nx*fore-lx*side,nz*fore-lz*side}
    }};

    struct CandidateQ940 { float y; size_t tri; };
    std::vector<CandidateQ940> candidates;
    candidates.reserve(48u);
    for (size_t triIndex : local.tris) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        if (!IsWalkableTriangleQ910(tri)) continue;
        if (tri.maxY < minY-0.05f || tri.minY > maxY+0.05f) continue;
        for (const Vec2& p : probes) {
            const float px = x+p.x, pz = z+p.y;
            if (px < tri.minX-0.008f || px > tri.maxX+0.008f ||
                pz < tri.minZ-0.008f || pz > tri.maxZ+0.008f) continue;
            float u=0.0f,v=0.0f,w=0.0f;
            if (!BarycentricXZ(tri,px,pz,u,v,w)) continue;
            const float y = tri.a.y*u + tri.b.y*v + tri.c.y*w;
            const float rise = y-feetY;
            if (y < minY || y > maxY) continue;
            if (raisedOnly && rise < Q940_RAISED_MIN) continue;
            candidates.push_back({y,triIndex});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [feetY,raisedOnly](const CandidateQ940& a,const CandidateQ940& b) {
        if (raisedOnly) return a.y < b.y; // nearest next tread, never jump a flight.
        return std::fabs(a.y-feetY) < std::fabs(b.y-feetY);
    });

    float lastY = std::numeric_limits<float>::infinity();
    for (const CandidateQ940& candidate : candidates) {
        if (std::fabs(candidate.y-lastY) < 0.004f) continue;
        lastY = candidate.y;
        const int support = LocalFootprintSupportQ940(local,x,z,candidate.y,nx,nz);
        if (support < 2) continue;
        result.found = true;
        result.y = candidate.y;
        result.supportPoints = support;
        result.triangleIndex = candidate.tri;
        return result;
    }
    return result;
}

DownSupportQ910 FindRaisedLandingAheadQ940(const LocalCandidatesQ940& local,
                                           float targetX, float targetZ,
                                           float feetY, float nx, float nz,
                                           float& outAhead) {
    const float r = CollisionRadiusQ712();
    const std::array<float,7> ahead{{
        0.0f, r*0.25f, r*0.50f, r*0.75f,
        r*1.00f, r*1.25f, r*1.50f
    }};
    for (float d : ahead) {
        const DownSupportQ910 support = FindLocalSupportQ940(
            local, targetX+nx*d, targetZ+nz*d, feetY, nx, nz, true);
        if (!support.found) continue;
        outAhead = d;
        return support;
    }
    return DownSupportQ910{};
}

bool LocalUpperBodyBlockedQ940(const LocalCandidatesQ940& local,
                               float x, float z, float feetY,
                               float moveX, float moveZ,
                               CapsuleHitQ910& outHit) {
    outHit = CapsuleHitQ910{};
    float strongestInto = 0.0f;
    bool blocked = false;
    for (size_t triIndex : local.tris) {
        CapsuleHitQ910 hit;
        if (!UpperBodyTriangleContactQ930(x,z,feetY,triIndex,hit)) continue;
        const float hLen = std::sqrt(hit.nx*hit.nx + hit.nz*hit.nz);
        if (hLen < 1e-5f) {
            outHit = hit; // ceiling/overhang overlap.
            return true;
        }
        const float into = moveX*(hit.nx/hLen) + moveZ*(hit.nz/hLen);
        if (into >= -Q718_DIRECTION_EPS) continue;
        if (!blocked || into < strongestInto) {
            blocked = true;
            strongestInto = into;
            outHit = hit;
        }
    }
    return blocked;
}

bool TryRaisedStepEnvelopeQ940(float startX, float startZ,
                               float targetX, float targetZ, float feetY,
                               const CapsuleHitQ910& initialHit,
                               float& outFeetY) {
    if (!initialHit.hit || initialHit.triangleIndex >= gWorldTriangles.size()) return false;
    const float dx = targetX-startX;
    const float dz = targetZ-startZ;
    const float len = std::sqrt(dx*dx+dz*dz);
    if (len < 1e-7f) return false;
    const float nx = dx/len, nz = dz/len;
    const float contactRise = initialHit.pointY-feetY;
    const float maxRise = StepHeightQ78B() + Q940_STEP_TOLERANCE;

    const LocalCandidatesQ940 local = CollectLocalCandidatesQ940(
        startX,startZ,targetX,targetZ,feetY,nx,nz);

    bool usedRaised = false;
    float probeAhead = 0.0f;
    DownSupportQ910 landing;

    // Positive riser: the old floor is NOT allowed to win first. Search for the
    // nearest supported raised tread, including a short look-ahead. This is the
    // specific failure seen on ShackStairs and ScrapGroundPlates.
    if (contactRise > 0.015f) {
        landing = FindRaisedLandingAheadQ940(local,targetX,targetZ,feetY,nx,nz,probeAhead);
        if (landing.found) usedRaised = true;
    }

    // No raised tread means this may simply be a low lip/pipe/seam. Preserve the
    // Q9.3 win by allowing ordinary current-floor support as fallback. A real
    // wall is still rejected by the local upper-body occupancy check below.
    if (!landing.found) {
        landing = FindLocalSupportQ940(local,targetX,targetZ,feetY,nx,nz,false);
    }
    if (!landing.found) {
        if (gQ940FailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ940FailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=SUPPORT reason=no-support contactRise=%.3f local=%zu",
                     contactRise, local.tris.size());
        }
        return false;
    }

    const float rise = landing.y-feetY;
    if (rise > maxRise || rise < -Q910_MAX_STEP_DROP) {
        if (gQ940FailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ940FailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=SUPPORT reason=rise-range contactRise=%.3f landingRise=%.3f maxRise=%.3f",
                     contactRise,rise,maxRise);
        }
        return false;
    }

    if (usedRaised && (gQ940LandingLogs < 240u || (gResolveCounter % 240u) == 0u)) {
        ++gQ940LandingLogs;
        Q6G_LOGI("Q9.4 RAISED LANDING SELECT: probeAhead=%.3f contactRise=%.3f landingRise=%.3f support=%d tri=%zu local=%zu maxRise=%.3f",
                 probeAhead,contactRise,rise,landing.supportPoints,
                 landing.triangleIndex,local.tris.size(),maxRise);
    }

    // Q9.3 sampled the whole world repeatedly along the move. Q9.4 performs one
    // final exact upper-body test against only nearby candidates, after the new
    // support height is known. A next stair riser is therefore below the new
    // envelope; a wall/rail/ceiling still intersects the upper body.
    CapsuleHitQ910 upperHit;
    if (LocalUpperBodyBlockedQ940(local,targetX,targetZ,landing.y,dx,dz,upperHit)) {
        if (gQ940FailLogs < 160u || (gResolveCounter % 240u) == 0u) {
            ++gQ940FailLogs;
            Q6G_LOGI("Q9.4 STEP ENVELOPE FAIL: phase=UPPER reason=local-target-block contactRise=%.3f landingRise=%.3f upperHitRise=%.3f tri=%zu local=%zu",
                     contactRise,rise,upperHit.pointY-landing.y,
                     upperHit.triangleIndex,local.tris.size());
        }
        return false;
    }

    outFeetY = landing.y;
    if (gQ940PassLogs < 360u || (gResolveCounter % 240u) == 0u) {
        ++gQ940PassLogs;
        Q6G_LOGI("Q9.4 STEP ENVELOPE PASS: forward=%.3f contactRise=%.3f landingRise=%.3f mode=%s probeAhead=%.3f support=%d local=%zu maxRise=%.3f fullWorldUpperSweep=0",
                 len,contactRise,rise,usedRaised?"RAISED":"FLOOR",
                 probeAhead,landing.supportPoints,local.tris.size(),maxRise);
    }
    return true;
}

]=])

string(REPLACE
    "Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {"
    "${Q940_RAISED_SUPPORT_SOURCE}Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {"
    Q940_CONTROLLER_SOURCE "${Q940_CONTROLLER_SOURCE}")

# Q9.4 owns both active step decisions. Q9.3 remains compiled for comparison
# only; replace only the two movement call sites, not Q9.3's function definition.
string(REPLACE
    "if (TryLocalStepEnvelopeQ930(startX,startZ,targetX,targetZ,feetY,hit,stepY))"
    "if (TryRaisedStepEnvelopeQ940(startX,startZ,targetX,targetZ,feetY,hit,stepY))"
    Q940_CONTROLLER_SOURCE "${Q940_CONTROLLER_SOURCE}")
string(REPLACE
    "if (TryLocalStepEnvelopeQ930(x,z,slideTargetX,slideTargetZ,feetY,\n                                   slideHit,slideStepY))"
    "if (TryRaisedStepEnvelopeQ940(x,z,slideTargetX,slideTargetZ,feetY,\n                                   slideHit,slideStepY))"
    Q940_CONTROLLER_SOURCE "${Q940_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=local-step-envelope+upper-body-sweep+post-move-support+wall-projection"
    "mode=raised-support-step-envelope+local-candidates+target-upper-body+wall-projection"
    Q940_CONTROLLER_SOURCE "${Q940_CONTROLLER_SOURCE}")
string(REPLACE
    "mode=local-step-envelope+upper-body-sweep+post-move-support+wall-projection persistentOverlap=none"
    "mode=raised-support-step-envelope+local-candidates+target-upper-body+wall-projection persistentOverlap=none"
    Q940_CONTROLLER_SOURCE "${Q940_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q940.inc"
     "${Q940_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q930.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q940.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
