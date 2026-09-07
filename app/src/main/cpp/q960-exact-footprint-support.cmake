# Q9.6: keep Q9.5's coherent-object spatial broadphase, but replace the stair
# landing probe lattice with an exact circular-footprint/triangle overlap query.
#
# The Q9.5 log proved the architecture/performance work: full-world movement
# scans are gone and modular catwalk seams traverse correctly. The remaining
# stair failure is support transfer. A riser can contact the capsule at one
# height while the reachable tread lies under the capsule footprint at another.
# Q9.6 therefore ignores contact height as a step gate and asks the coherent
# collision object directly for the highest walkable support intersecting the
# destination capsule footprint, bounded only by the controller step budget.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q950.inc"
     Q960_CONTROLLER_SOURCE)

set(Q960_EXACT_FOOTPRINT_SOURCE [=[
uint64_t gQ960LandingLogs = 0u;
uint64_t gQ960StepLogs = 0u;
uint64_t gQ960RejectLogs = 0u;

struct Q960FootprintHit {
    bool found = false;
    float y = 0.0f;
    float radial = 0.0f;
    float forward = 0.0f;
};

bool Q960FootprintSupportOnTriangle(size_t triIndex,
                                    float cx,float cz,float radius,
                                    float feetY,float maxRise,
                                    float nx,float nz,
                                    Q960FootprintHit& out) {
    if (triIndex>=gWorldTriangles.size()) return false;
    const CollisionTriangle& tri=gWorldTriangles[triIndex];
    if (!IsWalkableTriangleQ910(tri)) return false;
    if (cx < tri.minX-radius-0.004f || cx > tri.maxX+radius+0.004f ||
        cz < tri.minZ-radius-0.004f || cz > tri.maxZ+radius+0.004f) return false;

    const float r2=radius*radius;
    bool found=false;
    float bestY=-std::numeric_limits<float>::infinity();
    float bestRadial=std::numeric_limits<float>::infinity();
    float bestForward=-std::numeric_limits<float>::infinity();

    auto consider=[&](float px,float pz) {
        const float ox=px-cx,oz=pz-cz;
        const float d2=ox*ox+oz*oz;
        if (d2>r2+0.000025f) return;
        float u=0.0f,v=0.0f,w=0.0f;
        if (!BarycentricXZ(tri,px,pz,u,v,w)) return;
        // Numerical edge tolerance only. BarycentricXZ already performs the
        // actual projected-triangle containment test.
        if (u < -0.0025f || v < -0.0025f || w < -0.0025f) return;
        const float y=tri.a.y*u+tri.b.y*v+tri.c.y*w;
        const float rise=y-feetY;
        if (rise<0.006f || rise>maxRise) return;
        const float forward=ox*nx+oz*nz;
        // Ignore support that exists only on the far rear edge of the capsule.
        // Center/side overlap and any forward overlap remain valid.
        if (forward < -radius*0.45f) return;
        const float radial=std::sqrt(std::max(0.0f,d2));
        if (!found || y>bestY+0.0005f ||
            (std::fabs(y-bestY)<=0.0005f && radial<bestRadial-0.0005f) ||
            (std::fabs(y-bestY)<=0.0005f && std::fabs(radial-bestRadial)<=0.0005f && forward>bestForward)) {
            found=true;
            bestY=y;
            bestRadial=radial;
            bestForward=forward;
        }
    };

    // Circle centre inside the projected tread.
    consider(cx,cz);

    const std::array<Vec2,3> p{{
        {tri.a.x,tri.a.z},{tri.b.x,tri.b.z},{tri.c.x,tri.c.z}
    }};
    for (const Vec2& v:p) consider(v.x,v.y);

    // Exact circle/segment intersection plus the closest segment point. The
    // union of these candidates contains extrema of a linear height field over
    // triangle-projection intersect circle, which is exactly the footprint
    // support region for a walkable facet.
    for (int edge=0;edge<3;++edge) {
        const Vec2& a=p[edge];
        const Vec2& b=p[(edge+1)%3];
        const float ex=b.x-a.x,ez=b.y-a.y;
        const float len2=ex*ex+ez*ez;
        if (len2<1e-10f) continue;

        const float tClosest=std::clamp(((cx-a.x)*ex+(cz-a.y)*ez)/len2,0.0f,1.0f);
        consider(a.x+ex*tClosest,a.y+ez*tClosest);

        const float fx=a.x-cx,fz=a.y-cz;
        const float A=len2;
        const float B=2.0f*(fx*ex+fz*ez);
        const float C=fx*fx+fz*fz-r2;
        const float disc=B*B-4.0f*A*C;
        if (disc>=0.0f) {
            const float root=std::sqrt(std::max(0.0f,disc));
            const float inv=0.5f/A;
            const float t0=(-B-root)*inv;
            const float t1=(-B+root)*inv;
            if (t0>=0.0f && t0<=1.0f) consider(a.x+ex*t0,a.y+ez*t0);
            if (t1>=0.0f && t1<=1.0f && std::fabs(t1-t0)>1e-6f)
                consider(a.x+ex*t1,a.y+ez*t1);
        }
    }

    if (!found) return false;
    out.found=true;
    out.y=bestY;
    out.radial=bestRadial;
    out.forward=bestForward;
    return true;
}

void Q960ConsiderObjectFootprint(Q950Landing& best,uint32_t objectIndex,
                                 float targetX,float targetZ,float feetY,
                                 float nx,float nz,bool sameObject) {
    if (objectIndex>=gQ950Objects.size()) return;
    const float maxRise=StepHeightQ78B()+Q950_STEP_TOLERANCE;
    // The blocking sweep stops the centre about one capsule radius from a riser.
    // Use the real collision footprint plus a tiny numerical skin so the tread
    // touching that circle is considered immediately, without a probe lattice.
    const float radius=CollisionRadiusQ712()+Q910_SWEEP_SKIN+0.006f;
    const Q950CollisionObject& object=gQ950Objects[objectIndex];
    for (size_t triIndex:object.triangles) {
        Q960FootprintHit hit;
        if (!Q960FootprintSupportOnTriangle(triIndex,targetX,targetZ,radius,
                                            feetY,maxRise,nx,nz,hit)) continue;
        const float rise=hit.y-feetY;
        // Highest reachable support wins. This naturally chooses the first
        // stair tread because the next tread is > one step above current feet.
        // On irregular scrap it selects the real ~0.318 m top even if the
        // capsule contact point on the side face is ~0.35 m high.
        const float score=-rise + hit.radial*0.0005f + (sameObject?-0.001f:0.001f);
        if (!best.found || score<best.score) {
            best.found=true;
            best.y=hit.y;
            best.rise=rise;
            best.score=score;
            best.probeAhead=std::max(0.0f,hit.forward);
            best.triangleIndex=triIndex;
            best.objectIndex=objectIndex;
            best.topology=sameObject;
        }
    }
}

Q950Landing FindQ960FootprintLanding(float startX,float startZ,
                                     float targetX,float targetZ,float feetY,
                                     const CapsuleHitQ910& hit,uint32_t hitObject) {
    Q950Landing best;
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len<1e-7f) return best;
    const float nx=dx/len,nz=dz/len;

    // 1) The coherent object that actually blocked the capsule owns the first
    // chance to provide support. No contactRise comparison and no point probes.
    if (hitObject!=0xffffffffu && hitObject<gQ950Objects.size())
        Q960ConsiderObjectFootprint(best,hitObject,targetX,targetZ,feetY,nx,nz,true);

    // 2) If the blocking object has no reachable top, allow an immediately
    // adjacent coherent modular object to provide it (catwalk/ramp transitions).
    if (!best.found) {
        const float radius=CollisionRadiusQ712()+Q910_SWEEP_SKIN+0.020f;
        const float maxRise=StepHeightQ78B()+Q950_STEP_TOLERANCE;
        const Q950Candidates nearby=QueryQ950(targetX-radius,targetX+radius,
                                               feetY-0.03f,feetY+maxRise+0.05f,
                                               targetZ-radius,targetZ+radius);
        for (uint32_t objectIndex:nearby.objects) {
            if (objectIndex==hitObject) continue;
            if (objectIndex>=gQ950Objects.size() || gQ950Objects[objectIndex].walkableTriangles==0u) continue;
            Q960ConsiderObjectFootprint(best,objectIndex,targetX,targetZ,feetY,nx,nz,false);
        }

        // LAND/legacy triangles are not authored Q9.5 objects, but may still be
        // the receiving support at an object boundary. Test only the already
        // spatially-filtered nearby legacy set, never the whole world.
        const float supportRadius=CollisionRadiusQ712()+Q910_SWEEP_SKIN+0.006f;
        for (uint32_t triIndex:nearby.legacyTriangles) {
            Q960FootprintHit fh;
            if (!Q960FootprintSupportOnTriangle(triIndex,targetX,targetZ,supportRadius,
                                                feetY,maxRise,nx,nz,fh)) continue;
            const float rise=fh.y-feetY;
            const float score=-rise+fh.radial*0.0005f+0.002f;
            if (!best.found || score<best.score) {
                best.found=true; best.y=fh.y; best.rise=rise; best.score=score;
                best.probeAhead=std::max(0.0f,fh.forward); best.triangleIndex=triIndex;
                best.objectIndex=0xffffffffu; best.topology=false;
            }
        }
    }

    if (best.found && (gQ960LandingLogs<180u || (gResolveCounter%360u)==0u)) {
        ++gQ960LandingLogs;
        const Q950CollisionObject* object=best.objectIndex<gQ950Objects.size()?&gQ950Objects[best.objectIndex]:nullptr;
        Q6G_LOGI("Q9.6 FOOTPRINT LANDING: hitObject=%u landingObject=%u ref=%08X contactRise=%.3f landingRise=%.3f overlapForward=%.3f sameObject=%d tri=%zu exactCircleTriangle=1",
                 hitObject,best.objectIndex,object?object->refFormId:0u,
                 hit.pointY-feetY,best.rise,best.probeAhead,best.topology?1:0,best.triangleIndex);
    }
    return best;
}

bool TryCoherentStepQ960(float startX,float startZ,float targetX,float targetZ,float feetY,
                         const CapsuleHitQ910& hit,uint32_t hitObject,float& outFeetY) {
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len<1e-7f) return false;

    const Q950Landing landing=FindQ960FootprintLanding(startX,startZ,targetX,targetZ,feetY,hit,hitObject);
    if (landing.found) {
        CapsuleHitQ910 upper;
        if (!UpperBlockedQ950(targetX,targetZ,landing.y,dx,dz,upper)) {
            outFeetY=landing.y;
            if (gQ960StepLogs<180u || (gResolveCounter%360u)==0u) {
                ++gQ960StepLogs;
                const Q950CollisionObject* object=landing.objectIndex<gQ950Objects.size()?&gQ950Objects[landing.objectIndex]:nullptr;
                Q6G_LOGI("Q9.6 SHAPE STEP: hitObject=%u landingObject=%u ref=%08X contactRise=%.3f landingRise=%.3f sameObject=%d tri=%zu supportMode=exact-footprint",
                         hitObject,landing.objectIndex,object?object->refFormId:0u,
                         hit.pointY-feetY,landing.rise,landing.topology?1:0,landing.triangleIndex);
            }
            return true;
        }
        // A real raised support was found. If the upper capsule cannot occupy
        // the resulting pose, do NOT silently fall back to old-floor traversal;
        // that was the Q9.5 stair behaviour we are removing.
        if (gQ960RejectLogs<120u || (gResolveCounter%360u)==0u) {
            ++gQ960RejectLogs;
            Q6G_LOGI("Q9.6 STEP REJECT: phase=UPPER hitObject=%u contactRise=%.3f landingRise=%.3f upperHitRise=%.3f tri=%zu",
                     hitObject,hit.pointY-feetY,landing.rise,upper.pointY-landing.y,upper.triangleIndex);
        }
        return false;
    }

    if (gQ960RejectLogs<120u || (gResolveCounter%360u)==0u) {
        ++gQ960RejectLogs;
        Q6G_LOGI("Q9.6 STEP REJECT: phase=LANDING hitObject=%u contactRise=%.3f reason=no-footprint-support",
                 hitObject,hit.pointY-feetY);
    }

    // Preserve Q9.5's proven low-lip/catwalk behaviour only when there is no
    // raised footprint support at all.
    float floorY=feetY;
    CapsuleHitQ910 upper;
    if (FindGroundQ950(targetX,targetZ,feetY,floorY) &&
        std::fabs(floorY-feetY)<=0.08f &&
        !UpperBlockedQ950(targetX,targetZ,floorY,dx,dz,upper)) {
        outFeetY=floorY;
        return true;
    }
    return false;
}

]=])

string(REPLACE
    "bool TryCoherentStepQ950(float startX,float startZ,float targetX,float targetZ,float feetY,"
    "${Q960_EXACT_FOOTPRINT_SOURCE}bool TryCoherentStepQ950(float startX,float startZ,float targetX,float targetZ,float feetY,"
    Q960_CONTROLLER_SOURCE "${Q960_CONTROLLER_SOURCE}")

# Only replace the two active Q9.5 movement call sites. The old Q9.5 function
# remains compiled for reference but cannot decide an active step.
string(REPLACE
    "if (TryCoherentStepQ950(startX,startZ,targetX,targetZ,feetY,hit,hitObject,stepY))"
    "if (TryCoherentStepQ960(startX,startZ,targetX,targetZ,feetY,hit,hitObject,stepY))"
    Q960_CONTROLLER_SOURCE "${Q960_CONTROLLER_SOURCE}")
string(REPLACE
    "if (TryCoherentStepQ950(x,z,sxTarget,szTarget,feetY,slideHit,slideObject,slideStepY))"
    "if (TryCoherentStepQ960(x,z,sxTarget,szTarget,feetY,slideHit,slideObject,slideStepY))"
    Q960_CONTROLLER_SOURCE "${Q960_CONTROLLER_SOURCE}")

string(REPLACE
    "Q9.5 CONTROLLER READY: mode=coherent-placed-objects+spatial-grid+topology-step+shape-narrowphase triangleSoupStepPath=DISABLED fullWorldMovementScans=0 oldQ91toQ94StepPath=disabled"
    "Q9.6 CONTROLLER READY: mode=coherent-placed-objects+spatial-grid+exact-footprint-support+shape-narrowphase triangleSoupStepPath=DISABLED probeLatticeStepPath=DISABLED fullWorldMovementScans=0 oldQ91toQ94StepPath=disabled"
    Q960_CONTROLLER_SOURCE "${Q960_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q960.inc"
     "${Q960_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q950.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q960.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
