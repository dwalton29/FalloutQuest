# Q9.1: fresh, conventional character-controller movement path.
#
# This deliberately does NOT use Q8.x/Q9.0's low-obstacle, coherent-ledge,
# persistent-manifold or simplex logic to decide a step. The parser/world
# collision data remain shared, but movement is the boring standard algorithm:
#
#   horizontal capsule sweep
#       -> blocked: raise capsule by max step
#       -> sweep the ORIGINAL requested horizontal displacement
#       -> sweep/query down for the highest walkable support under the capsule
#       -> commit the full horizontal move + landing height
#       -> otherwise slide against the wall
#
# Critically, the step attempt happens BEFORE moving to time-of-impact, so a
# climbable ledge cannot create several frames of resistance before snapping up.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q900.inc"
     Q910_CONTROLLER_SOURCE)
string(REPLACE "Q9.0" "Q9.1"
       Q910_CONTROLLER_SOURCE "${Q910_CONTROLLER_SOURCE}")

set(Q910_STANDARD_CONTROLLER_SOURCE [=[
constexpr float Q910_SWEEP_SKIN = 0.004f;
constexpr float Q910_LANDING_TOLERANCE = 0.050f;
constexpr float Q910_MAX_STEP_DROP = 0.18f;
constexpr int Q910_CAST_BISECTIONS = 7;

struct CapsuleHitQ910 {
    bool hit = false;
    size_t triangleIndex = 0u;
    float pointX = 0.0f, pointY = 0.0f, pointZ = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
};

struct DownSupportQ910 {
    bool found = false;
    float y = 0.0f;
    int supportPoints = 0;
    size_t triangleIndex = 0u;
};

uint64_t gQ910ReadyLogs = 0u;
uint64_t gQ910StepLogs = 0u;
uint64_t gQ910StepFailLogs = 0u;
uint64_t gQ910SlideLogs = 0u;
uint64_t gQ910BlockedLogs = 0u;

bool IsWalkableTriangleQ910(const CollisionTriangle& tri) {
    return std::fabs(tri.normal.y) >= WalkableNormalThresholdQ714(tri);
}

bool CapsuleContactTriangleQ910(float x, float z, float feetY,
                                size_t triIndex, CapsuleHitQ910& out) {
    if (triIndex >= gWorldTriangles.size()) return false;
    const CollisionTriangle& tri = gWorldTriangles[triIndex];
    const float radius = CollisionRadiusQ712();
    const float queryRadius = radius + Q910_SWEEP_SKIN;
    if (tri.maxY < feetY - queryRadius ||
        tri.minY > feetY + PLAYER_HEIGHT + queryRadius) return false;
    if (x < tri.minX - queryRadius || x > tri.maxX + queryRadius ||
        z < tri.minZ - queryRadius || z > tri.maxZ + queryRadius) return false;

    const Vec3 segA{x, feetY + radius, z};
    const Vec3 segB{x, feetY + PLAYER_HEIGHT - radius, z};
    Vec3 capsulePoint;
    Vec3 trianglePoint;
    const float d2 = ClosestCapsuleSegmentTriangleQ719(segA, segB, tri,
                                                        capsulePoint, trianglePoint);
    if (!std::isfinite(d2) || d2 > queryRadius * queryRadius) return false;

    Vec3 separation = Q719Sub(capsulePoint, trianglePoint);
    float sepLen = std::sqrt(Q719LengthSq(separation));
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    if (sepLen > 1e-5f) {
        nx = separation.x / sepLen;
        ny = separation.y / sepLen;
        nz = separation.z / sepLen;
    } else {
        nx = tri.normal.x; ny = tri.normal.y; nz = tri.normal.z;
        const float cx = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
        const float cy = (tri.a.y + tri.b.y + tri.c.y) / 3.0f;
        const float cz = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
        const float toward = nx*(x-cx) + ny*((feetY+PLAYER_HEIGHT*0.5f)-cy) + nz*(z-cz);
        if (toward < 0.0f) { nx=-nx; ny=-ny; nz=-nz; }
    }

    out.hit = true;
    out.triangleIndex = triIndex;
    out.pointX = trianglePoint.x;
    out.pointY = trianglePoint.y;
    out.pointZ = trianglePoint.z;
    out.nx = nx; out.ny = ny; out.nz = nz;
    return true;
}

bool CapsulePositionBlockedQ910(float x, float z, float feetY,
                                float moveX, float moveZ,
                                CapsuleHitQ910& outHit) {
    outHit = CapsuleHitQ910{};
    float strongestInto = 0.0f;
    bool blocked = false;

    for (size_t triIndex = 0u; triIndex < gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri = gWorldTriangles[triIndex];
        // Floors/ramps are support, never horizontal wall constraints.
        if (IsWalkableTriangleQ910(tri)) continue;

        CapsuleHitQ910 hit;
        if (!CapsuleContactTriangleQ910(x, z, feetY, triIndex, hit)) continue;
        const float hLen = std::sqrt(hit.nx*hit.nx + hit.nz*hit.nz);
        if (hLen < 1e-5f) continue;
        const float hx = hit.nx / hLen;
        const float hz = hit.nz / hLen;
        const float into = moveX*hx + moveZ*hz;
        if (into >= -Q718_DIRECTION_EPS) continue;

        if (!blocked || into < strongestInto) {
            blocked = true;
            strongestInto = into;
            outHit = hit;
        }
    }
    return blocked;
}

bool SweepCapsuleHorizontalQ910(float startX, float startZ,
                                float targetX, float targetZ,
                                float feetY,
                                float& outX, float& outZ,
                                CapsuleHitQ910& outHit) {
    outX = startX; outZ = startZ; outHit = CapsuleHitQ910{};
    const float dx = targetX-startX, dz = targetZ-startZ;
    const float distance = std::sqrt(dx*dx + dz*dz);
    if (distance < 1e-7f) { outX=targetX; outZ=targetZ; return true; }

    const float sampleStep = std::max(0.008f, CollisionRadiusQ712()*0.05f);
    const int samples = std::clamp(static_cast<int>(std::ceil(distance/sampleStep)), 1, 24);
    float clearT = 0.0f;

    for (int i=1; i<=samples; ++i) {
        const float t = static_cast<float>(i)/static_cast<float>(samples);
        CapsuleHitQ910 hit;
        if (!CapsulePositionBlockedQ910(startX+dx*t, startZ+dz*t, feetY,
                                        dx, dz, hit)) {
            clearT = t;
            continue;
        }

        float lo=clearT, hi=t;
        CapsuleHitQ910 hiHit=hit;
        for (int iteration=0; iteration<Q910_CAST_BISECTIONS; ++iteration) {
            const float mid=(lo+hi)*0.5f;
            CapsuleHitQ910 midHit;
            if (!CapsulePositionBlockedQ910(startX+dx*mid, startZ+dz*mid, feetY,
                                            dx, dz, midHit)) {
                lo=mid;
            } else {
                hi=mid;
                hiHit=midHit;
            }
        }
        outX=startX+dx*lo;
        outZ=startZ+dz*lo;
        outHit=hiHit;
        return false;
    }

    outX=targetX; outZ=targetZ;
    return true;
}

bool WalkableSurfaceAtQ910(float x, float z, float targetY, float tolerance) {
    for (const CollisionTriangle& tri : gWorldTriangles) {
        if (!IsWalkableTriangleQ910(tri)) continue;
        if (x < tri.minX-0.008f || x > tri.maxX+0.008f ||
            z < tri.minZ-0.008f || z > tri.maxZ+0.008f) continue;
        float u=0.0f,v=0.0f,w=0.0f;
        if (!BarycentricXZ(tri,x,z,u,v,w)) continue;
        const float y=tri.a.y*u+tri.b.y*v+tri.c.y*w;
        if (std::fabs(y-targetY) <= tolerance) return true;
    }
    return false;
}

int LandingFootprintSupportQ910(float x, float z, float y,
                                float nx, float nz) {
    const float r=CollisionRadiusQ712();
    const float lx=-nz, lz=nx;
    const float side=r*0.52f;
    const float fore=r*0.82f;
    const std::array<Vec2,7> probes{{
        {0.0f,0.0f},
        {lx*side,lz*side},{-lx*side,-lz*side},
        {nx*fore,nz*fore},{-nx*fore,-nz*fore},
        {nx*fore+lx*side,nz*fore+lz*side},
        {nx*fore-lx*side,nz*fore-lz*side}
    }};
    int support=0;
    for (const Vec2& p : probes) {
        if (WalkableSurfaceAtQ910(x+p.x,z+p.y,y,Q910_LANDING_TOLERANCE)) ++support;
    }
    return support;
}

DownSupportQ910 SweepDownForSupportQ910(float x, float z,
                                        float raisedFeetY, float originalFeetY,
                                        float nx, float nz) {
    DownSupportQ910 result;
    const float maxY = originalFeetY + StepHeightQ78B() + PLAYER_SKIN + 0.012f;
    const float minY = originalFeetY - Q910_MAX_STEP_DROP;
    const float r=CollisionRadiusQ712();
    const float lx=-nz,lz=nx;
    const float side=r*0.52f, fore=r*0.82f;
    const std::array<Vec2,7> probes{{
        {0.0f,0.0f},
        {lx*side,lz*side},{-lx*side,-lz*side},
        {nx*fore,nz*fore},{-nx*fore,-nz*fore},
        {nx*fore+lx*side,nz*fore+lz*side},
        {nx*fore-lx*side,nz*fore-lz*side}
    }};

    struct CandidateQ910 { float y; size_t tri; };
    std::vector<CandidateQ910> candidates;
    candidates.reserve(32u);
    for (size_t triIndex=0u; triIndex<gWorldTriangles.size(); ++triIndex) {
        const CollisionTriangle& tri=gWorldTriangles[triIndex];
        if (!IsWalkableTriangleQ910(tri)) continue;
        if (tri.maxY < minY-0.05f || tri.minY > raisedFeetY+0.02f) continue;
        for (const Vec2& p : probes) {
            const float px=x+p.x,pz=z+p.y;
            if (px < tri.minX-0.008f || px > tri.maxX+0.008f ||
                pz < tri.minZ-0.008f || pz > tri.maxZ+0.008f) continue;
            float u=0.0f,v=0.0f,w=0.0f;
            if (!BarycentricXZ(tri,px,pz,u,v,w)) continue;
            const float y=tri.a.y*u+tri.b.y*v+tri.c.y*w;
            if (y > maxY || y < minY || y > raisedFeetY+0.01f) continue;
            candidates.push_back({y,triIndex});
        }
    }
    std::sort(candidates.begin(),candidates.end(),[](const CandidateQ910& a,const CandidateQ910& b){
        return a.y>b.y;
    });

    float lastY=std::numeric_limits<float>::infinity();
    for (const CandidateQ910& candidate : candidates) {
        if (std::fabs(candidate.y-lastY) < 0.004f) continue;
        lastY=candidate.y;
        const int support=LandingFootprintSupportQ910(x,z,candidate.y,nx,nz);
        if (support < 2) continue;
        result.found=true;
        result.y=candidate.y;
        result.supportPoints=support;
        result.triangleIndex=candidate.tri;
        return result; // first surface hit by the downward sweep = highest valid support.
    }
    return result;
}

bool TryStandardStepQ910(float startX,float startZ,
                         float targetX,float targetZ,float feetY,
                         const CapsuleHitQ910& initialHit,
                         float& outFeetY) {
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len < 1e-7f) return false;
    const float nx=dx/len,nz=dz/len;
    const float raisedFeetY=feetY+StepHeightQ78B()+PLAYER_SKIN+0.008f;

    // UP. This is a real character-controller clearance check, not a ledge
    // classification. If the capsule cannot occupy the raised position, no step.
    if (HasOverheadBlock(startX,startZ,raisedFeetY)) {
        if (gQ910StepFailLogs < 120u) {
            ++gQ910StepFailLogs;
            Q6G_LOGI("Q9.1 STANDARD STEP FAIL: phase=UP reason=overhead contactRise=%.3f",
                     initialHit.pointY-feetY);
        }
        return false;
    }

    // FORWARD. Sweep only the user's ORIGINAL requested displacement while the
    // capsule is raised. Tall walls remain walls; a <= step-height riser falls
    // below the raised capsule and therefore disappears naturally.
    float raisedX=startX,raisedZ=startZ;
    CapsuleHitQ910 raisedHit;
    if (!SweepCapsuleHorizontalQ910(startX,startZ,targetX,targetZ,raisedFeetY,
                                    raisedX,raisedZ,raisedHit)) {
        if (gQ910StepFailLogs < 120u) {
            ++gQ910StepFailLogs;
            Q6G_LOGI("Q9.1 STANDARD STEP FAIL: phase=FORWARD reason=blocked contactRise=%.3f raisedHitY=%.3f",
                     initialHit.pointY-feetY,raisedHit.pointY-feetY);
        }
        return false;
    }

    // DOWN. The first/highest walkable support under the raised capsule wins.
    // This is why the old floor cannot hide the next stair tread.
    const DownSupportQ910 landing=SweepDownForSupportQ910(targetX,targetZ,
                                                           raisedFeetY,feetY,nx,nz);
    if (!landing.found) {
        if (gQ910StepFailLogs < 120u) {
            ++gQ910StepFailLogs;
            Q6G_LOGI("Q9.1 STANDARD STEP FAIL: phase=DOWN reason=no-support contactRise=%.3f",
                     initialHit.pointY-feetY);
        }
        return false;
    }

    const float rise=landing.y-feetY;
    if (rise > StepHeightQ78B()+PLAYER_SKIN+0.012f || rise < -Q910_MAX_STEP_DROP) return false;
    // For an actual upward blocker, do not accept the old floor as the landing.
    if (initialHit.pointY-feetY > 0.015f && rise < 0.006f) return false;

    outFeetY=landing.y;
    if (gQ910StepLogs < 240u || (gResolveCounter%240u)==0u) {
        ++gQ910StepLogs;
        Q6G_LOGI("Q9.1 STANDARD STEP PASS: up=%.3f forward=%.3f down=%.3f landingRise=%.3f support=%d tri=%zu contactRise=%.3f",
                 raisedFeetY-feetY,len,raisedFeetY-landing.y,rise,
                 landing.supportPoints,landing.triangleIndex,initialHit.pointY-feetY);
    }
    return true;
}

Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {
    const float hLen=std::sqrt(hit.nx*hit.nx+hit.nz*hit.nz);
    if (hLen < 1e-6f) return Vec2{0.0f,0.0f};
    const float nx=hit.nx/hLen,nz=hit.nz/hLen;
    const float into=move.x*nx+move.y*nz;
    if (into < 0.0f) {
        move.x-=nx*into;
        move.y-=nz*into;
    }
    // Never allow collision response to reverse the requested direction.
    return move;
}

bool MoveExteriorSubstepQ910(float& x,float& z,float& feetY,
                             float moveX,float moveZ,bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX*moveX+moveZ*moveZ < 1e-10f) return true;
    if (gQ910ReadyLogs < 1u) {
        ++gQ910ReadyLogs;
        Q6G_LOGI("Q9.1 STANDARD CONTROLLER READY: mode=raw-capsule-sweep+up-forward-down+wall-projection oldStepPath=disabled oldSimplex=disabled");
    }

    const float startX=x,startZ=z;
    const float targetX=x+moveX,targetZ=z+moveZ;
    float castX=x,castZ=z;
    CapsuleHitQ910 hit;
    if (SweepCapsuleHorizontalQ910(startX,startZ,targetX,targetZ,feetY,
                                   castX,castZ,hit)) {
        x=targetX; z=targetZ;
        return true;
    }
    ++contactSurfaces;

    // STEP IS TRIED BEFORE COMMITTING TO TIME-OF-IMPACT. This is the key change:
    // a valid ledge never gets one or more frames of resisted movement first.
    float stepY=feetY;
    if (TryStandardStepQ910(startX,startZ,targetX,targetZ,feetY,hit,stepY)) {
        x=targetX; z=targetZ; feetY=stepY;
        steppedThisFrame=true;
        return true;
    }

    // Not a step: now, and only now, move to impact and perform ordinary wall
    // sliding by removing the into-wall component. No manifold/simplex solver.
    x=castX; z=castZ;
    CapsuleHitQ910 wallHit=hit;
    for (int iteration=0; iteration<3; ++iteration) {
        Vec2 remaining{targetX-x,targetZ-z};
        if (remaining.x*remaining.x+remaining.y*remaining.y < 0.00000025f) break;
        const Vec2 slide=ProjectAlongWallQ910(remaining,wallHit);
        if (slide.x*slide.x+slide.y*slide.y < 0.00000025f) break;
        if (slide.x*remaining.x+slide.y*remaining.y <= 0.0f) break;

        const float slideTargetX=x+slide.x,slideTargetZ=z+slide.y;
        float sx=x,sz=z;
        CapsuleHitQ910 slideHit;
        if (SweepCapsuleHorizontalQ910(x,z,slideTargetX,slideTargetZ,feetY,
                                       sx,sz,slideHit)) {
            if (gQ910SlideLogs < 160u || (gResolveCounter%240u)==0u) {
                ++gQ910SlideLogs;
                Q6G_LOGI("Q9.1 WALL SLIDE: requested=(%.3f %.3f) projected=(%.3f %.3f)",
                         remaining.x,remaining.y,slide.x,slide.y);
            }
            x=slideTargetX; z=slideTargetZ;
            return true;
        }

        // A second obstacle encountered while sliding gets the same standard
        // step opportunity before its collision response is accepted.
        float slideStepY=feetY;
        if (TryStandardStepQ910(x,z,slideTargetX,slideTargetZ,feetY,
                                slideHit,slideStepY)) {
            x=slideTargetX; z=slideTargetZ; feetY=slideStepY;
            steppedThisFrame=true;
            return true;
        }
        x=sx; z=sz; wallHit=slideHit; ++contactSurfaces;
    }

    if (gQ910BlockedLogs < 160u || (gResolveCounter%240u)==0u) {
        ++gQ910BlockedLogs;
        const CollisionTriangle* tri=(hit.hit && hit.triangleIndex<gWorldTriangles.size())
                                     ? &gWorldTriangles[hit.triangleIndex] : nullptr;
        uint64_t surface=tri?tri->surfaceKeyQ714:0u;
        const auto sourceIt=gSurfaceSourcesQ722.find(surface);
        if (sourceIt!=gSurfaceSourcesQ722.end()) {
            Q6G_LOGI("Q9.1 BLOCKER SOURCE: surface=%llu ref=%08X EDID=%s model=%s",
                     static_cast<unsigned long long>(surface),sourceIt->second.refFormId,
                     sourceIt->second.editorId.empty()?"<none>":sourceIt->second.editorId.c_str(),
                     sourceIt->second.modelPath.c_str());
        }
        Q6G_LOGI("Q9.1 BLOCKED: start=(%.3f %.3f) pos=(%.3f %.3f) target=(%.3f %.3f) contactRise=%.3f",
                 startX,startZ,x,z,targetX,targetZ,hit.pointY-feetY);
    }
    return (x-startX)*(x-startX)+(z-startZ)*(z-startZ)>0.00000025f;
}

]=])

# Inject the fresh controller alongside the old one for easy A/B diagnostics.
string(REPLACE
    "bool FindHkGroundQ800(float x, float z, float feetY, float& groundY) {"
    "${Q910_STANDARD_CONTROLLER_SOURCE}bool FindHkGroundQ800(float x, float z, float feetY, float& groundY) {"
    Q910_CONTROLLER_SOURCE "${Q910_CONTROLLER_SOURCE}")

# Route exterior substeps ONLY through Q9.1. Q8/Q9 ledge + simplex code remains
# compiled for diagnostics/reference but is unreachable from the movement loop.
string(REPLACE
    "if (!MoveExteriorSubstepQ800(x, z, feetY, subDx, subDz,"
    "if (!MoveExteriorSubstepQ910(x, z, feetY, subDx, subDz,"
    Q910_CONTROLLER_SOURCE "${Q910_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=coherent-bhk-shapes+shape-ledges+authored-welding+linear-cast+simplex persistentOverlap=keep-distance"
    "mode=standard-capsule-controller+up-forward-down+wall-projection persistentOverlap=none"
    Q910_CONTROLLER_SOURCE "${Q910_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q910.inc"
     "${Q910_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q900.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q910.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
