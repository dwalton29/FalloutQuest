# Q9.5: make Q9.0's coherent authored collision shapes the actual movement
# broadphase/controller source instead of repeatedly rescanning gWorldTriangles.
#
# Architecture:
#   placed Havok shapes -> coherent placed collision object -> spatial grid
#   -> nearby objects only -> triangle facets as narrowphase.
#
# A triangle remains the exact mathematical facet used for capsule distance,
# but it is no longer the world-level collision object or broadphase primitive.
# Stair stepping is topology/object aware: start from the authored object that
# blocked the capsule, follow exact shared-edge adjacency first, then search the
# rest of that same placed object for the next walkable top. Only after that may
# a nearby separate object provide the landing. Q9.1-Q9.4 step heuristics stay
# compiled for diagnostics but are not called from the movement loop.
#
# This also replaces Q8.0's full-world ground scan. The hot movement path has no
# full gWorldTriangles scan after the spatial index has been built.

file(READ "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q940.inc"
     Q950_CONTROLLER_SOURCE)

set(Q950_COHERENT_SPATIAL_SOURCE [=[
constexpr float Q950_GRID_CELL = 2.0f;
constexpr float Q950_STEP_TOLERANCE = 0.020f;
constexpr float Q950_QUERY_PAD = 0.035f;
constexpr int Q950_CAST_BISECTIONS = 5;
constexpr int Q950_MAX_CAST_SAMPLES = 12;
constexpr size_t Q950_MAX_GRID_CELLS_PER_ITEM = 64u;

struct Q950CollisionObject {
    uint64_t key = 0u;
    uint32_t refFormId = 0u;
    std::vector<uint64_t> shapeKeys;
    std::vector<size_t> triangles;
    float minX = 0.0f, maxX = 0.0f;
    float minY = 0.0f, maxY = 0.0f;
    float minZ = 0.0f, maxZ = 0.0f;
    size_t walkableTriangles = 0u;
};

struct Q950GridCell {
    std::vector<uint32_t> objects;
    std::vector<uint32_t> legacyTriangles;
};

struct Q950Candidates {
    std::vector<uint32_t> objects;
    std::vector<uint32_t> legacyTriangles;
};

struct Q950Landing {
    bool found = false;
    float y = 0.0f;
    float rise = 0.0f;
    float score = 0.0f;
    float probeAhead = 0.0f;
    size_t triangleIndex = 0u;
    uint32_t objectIndex = 0xffffffffu;
    bool topology = false;
};

std::vector<Q950CollisionObject> gQ950Objects;
std::vector<uint32_t> gQ950TriangleObject;
std::unordered_map<uint64_t,Q950GridCell> gQ950Grid;
std::vector<uint32_t> gQ950LargeObjects;
std::vector<uint32_t> gQ950LargeLegacy;
std::vector<uint32_t> gQ950ObjectStamp;
std::vector<uint32_t> gQ950TriangleStamp;
uint32_t gQ950QuerySerial = 1u;
const CollisionTriangle* gQ950Data = nullptr;
size_t gQ950TriangleCount = 0u;
bool gQ950Ready = false;
uint64_t gQ950ReadyLogs = 0u;
uint64_t gQ950StepLogs = 0u;
uint64_t gQ950LowPassLogs = 0u;
uint64_t gQ950BlockedLogs = 0u;
uint64_t gQ950SlideLogs = 0u;

uint64_t Q950CellKey(int x,int z) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32u) |
           static_cast<uint32_t>(z);
}

int Q950CellCoord(float v) {
    return static_cast<int>(std::floor(v / Q950_GRID_CELL));
}

bool Q950AabbOverlap(float minX,float maxX,float minY,float maxY,float minZ,float maxZ,
                     float bMinX,float bMaxX,float bMinY,float bMaxY,float bMinZ,float bMaxZ) {
    return !(maxX < bMinX || minX > bMaxX ||
             maxY < bMinY || minY > bMaxY ||
             maxZ < bMinZ || minZ > bMaxZ);
}

void EnsureQ950SpatialIndex() {
    const CollisionTriangle* data = gWorldTriangles.empty() ? nullptr : gWorldTriangles.data();
    if (gQ950Ready && data == gQ950Data && gQ950TriangleCount == gWorldTriangles.size()) return;

    EnsureHkCoherentShapesQ900();
    EnsureHkWeldAdjacencyQ801();

    gQ950Objects.clear();
    gQ950TriangleObject.assign(gWorldTriangles.size(),0xffffffffu);
    gQ950Grid.clear();
    gQ950LargeObjects.clear();
    gQ950LargeLegacy.clear();

    std::unordered_map<uint64_t,uint32_t> objectByKey;
    objectByKey.reserve(gHkShapesQ900.size()+32u);
    size_t shapedTriangles = 0u;

    // Merge Q9.0's per-bhk-shape meshes into one placed authored collision
    // object when they belong to the same placed reference. This keeps separate
    // Havok leaf shapes available through shapeKeys while letting a stair NIF's
    // riser and tread participate in one object-level traversal query.
    for (const HkCoherentShapeQ900& shape : gHkShapesQ900) {
        if (shape.triangles.empty()) continue;
        uint32_t refFormId = 0u;
        for (size_t triIndex : shape.triangles) {
            if (triIndex >= gWorldTriangles.size()) continue;
            const auto src = gSurfaceSourcesQ722.find(gWorldTriangles[triIndex].surfaceKeyQ714);
            if (src != gSurfaceSourcesQ722.end() && src->second.refFormId != 0u) {
                refFormId = src->second.refFormId;
                break;
            }
        }
        const uint64_t objectKey = refFormId != 0u
            ? (0x8000000000000000ULL | static_cast<uint64_t>(refFormId))
            : shape.key;

        uint32_t objectIndex = 0u;
        const auto found = objectByKey.find(objectKey);
        if (found == objectByKey.end()) {
            objectIndex = static_cast<uint32_t>(gQ950Objects.size());
            Q950CollisionObject object;
            object.key = objectKey;
            object.refFormId = refFormId;
            object.minX = shape.minX; object.maxX = shape.maxX;
            object.minY = shape.minY; object.maxY = shape.maxY;
            object.minZ = shape.minZ; object.maxZ = shape.maxZ;
            gQ950Objects.push_back(std::move(object));
            objectByKey.emplace(objectKey,objectIndex);
        } else {
            objectIndex = found->second;
        }

        Q950CollisionObject& object = gQ950Objects[objectIndex];
        object.shapeKeys.push_back(shape.key);
        object.minX = std::min(object.minX,shape.minX); object.maxX = std::max(object.maxX,shape.maxX);
        object.minY = std::min(object.minY,shape.minY); object.maxY = std::max(object.maxY,shape.maxY);
        object.minZ = std::min(object.minZ,shape.minZ); object.maxZ = std::max(object.maxZ,shape.maxZ);
        object.walkableTriangles += shape.walkableTriangles;
        for (size_t triIndex : shape.triangles) {
            if (triIndex >= gWorldTriangles.size()) continue;
            object.triangles.push_back(triIndex);
            gQ950TriangleObject[triIndex] = objectIndex;
            ++shapedTriangles;
        }
    }

    auto insertObject = [&](uint32_t objectIndex) {
        const Q950CollisionObject& object = gQ950Objects[objectIndex];
        const int minCx=Q950CellCoord(object.minX), maxCx=Q950CellCoord(object.maxX);
        const int minCz=Q950CellCoord(object.minZ), maxCz=Q950CellCoord(object.maxZ);
        const int64_t spanX=static_cast<int64_t>(maxCx)-minCx+1;
        const int64_t spanZ=static_cast<int64_t>(maxCz)-minCz+1;
        if (spanX <= 0 || spanZ <= 0 ||
            static_cast<uint64_t>(spanX*spanZ) > Q950_MAX_GRID_CELLS_PER_ITEM) {
            gQ950LargeObjects.push_back(objectIndex);
            return;
        }
        for (int cx=minCx;cx<=maxCx;++cx)
            for (int cz=minCz;cz<=maxCz;++cz)
                gQ950Grid[Q950CellKey(cx,cz)].objects.push_back(objectIndex);
    };
    for (uint32_t i=0u;i<gQ950Objects.size();++i) insertObject(i);

    size_t legacyTriangles = 0u;
    for (uint32_t triIndex=0u;triIndex<gWorldTriangles.size();++triIndex) {
        if (gQ950TriangleObject[triIndex] != 0xffffffffu) continue;
        ++legacyTriangles;
        const CollisionTriangle& tri=gWorldTriangles[triIndex];
        const int minCx=Q950CellCoord(tri.minX), maxCx=Q950CellCoord(tri.maxX);
        const int minCz=Q950CellCoord(tri.minZ), maxCz=Q950CellCoord(tri.maxZ);
        const int64_t spanX=static_cast<int64_t>(maxCx)-minCx+1;
        const int64_t spanZ=static_cast<int64_t>(maxCz)-minCz+1;
        if (spanX <= 0 || spanZ <= 0 ||
            static_cast<uint64_t>(spanX*spanZ) > Q950_MAX_GRID_CELLS_PER_ITEM) {
            gQ950LargeLegacy.push_back(triIndex);
            continue;
        }
        for (int cx=minCx;cx<=maxCx;++cx)
            for (int cz=minCz;cz<=maxCz;++cz)
                gQ950Grid[Q950CellKey(cx,cz)].legacyTriangles.push_back(triIndex);
    }

    gQ950ObjectStamp.assign(gQ950Objects.size(),0u);
    gQ950TriangleStamp.assign(gWorldTriangles.size(),0u);
    gQ950QuerySerial=1u;
    gQ950Data=data;
    gQ950TriangleCount=gWorldTriangles.size();
    gQ950Ready=true;

    if (gQ950ReadyLogs < 8u) {
        ++gQ950ReadyLogs;
        Q6G_LOGI("Q9.5 COHERENT BROADPHASE READY: objects=%zu q900Shapes=%zu shapedTriangles=%zu legacyTriangles=%zu cells=%zu largeObjects=%zu mode=placed-object-grid+shape-narrowphase fullWorldMovementScans=0",
                 gQ950Objects.size(),gHkShapesQ900.size(),shapedTriangles,legacyTriangles,
                 gQ950Grid.size(),gQ950LargeObjects.size());
    }
}

void Q950BeginQuery() {
    ++gQ950QuerySerial;
    if (gQ950QuerySerial == 0u) {
        std::fill(gQ950ObjectStamp.begin(),gQ950ObjectStamp.end(),0u);
        std::fill(gQ950TriangleStamp.begin(),gQ950TriangleStamp.end(),0u);
        gQ950QuerySerial=1u;
    }
}

void Q950AddObject(Q950Candidates& out,uint32_t index) {
    if (index>=gQ950Objects.size()) return;
    if (gQ950ObjectStamp[index]==gQ950QuerySerial) return;
    gQ950ObjectStamp[index]=gQ950QuerySerial;
    out.objects.push_back(index);
}

void Q950AddLegacy(Q950Candidates& out,uint32_t index) {
    if (index>=gWorldTriangles.size()) return;
    if (gQ950TriangleStamp[index]==gQ950QuerySerial) return;
    gQ950TriangleStamp[index]=gQ950QuerySerial;
    out.legacyTriangles.push_back(index);
}

Q950Candidates QueryQ950(float minX,float maxX,float minY,float maxY,float minZ,float maxZ) {
    EnsureQ950SpatialIndex();
    Q950BeginQuery();
    Q950Candidates result;
    result.objects.reserve(24u);
    result.legacyTriangles.reserve(48u);
    const int minCx=Q950CellCoord(minX),maxCx=Q950CellCoord(maxX);
    const int minCz=Q950CellCoord(minZ),maxCz=Q950CellCoord(maxZ);
    for (int cx=minCx;cx<=maxCx;++cx) {
        for (int cz=minCz;cz<=maxCz;++cz) {
            const auto found=gQ950Grid.find(Q950CellKey(cx,cz));
            if (found==gQ950Grid.end()) continue;
            for (uint32_t objectIndex:found->second.objects) {
                const Q950CollisionObject& object=gQ950Objects[objectIndex];
                if (Q950AabbOverlap(minX,maxX,minY,maxY,minZ,maxZ,
                                    object.minX,object.maxX,object.minY,object.maxY,object.minZ,object.maxZ))
                    Q950AddObject(result,objectIndex);
            }
            for (uint32_t triIndex:found->second.legacyTriangles) {
                const CollisionTriangle& tri=gWorldTriangles[triIndex];
                if (Q950AabbOverlap(minX,maxX,minY,maxY,minZ,maxZ,
                                    tri.minX,tri.maxX,tri.minY,tri.maxY,tri.minZ,tri.maxZ))
                    Q950AddLegacy(result,triIndex);
            }
        }
    }
    for (uint32_t objectIndex:gQ950LargeObjects) {
        const Q950CollisionObject& object=gQ950Objects[objectIndex];
        if (Q950AabbOverlap(minX,maxX,minY,maxY,minZ,maxZ,
                            object.minX,object.maxX,object.minY,object.maxY,object.minZ,object.maxZ))
            Q950AddObject(result,objectIndex);
    }
    for (uint32_t triIndex:gQ950LargeLegacy) {
        const CollisionTriangle& tri=gWorldTriangles[triIndex];
        if (Q950AabbOverlap(minX,maxX,minY,maxY,minZ,maxZ,
                            tri.minX,tri.maxX,tri.minY,tri.maxY,tri.minZ,tri.maxZ))
            Q950AddLegacy(result,triIndex);
    }
    return result;
}

bool Q950PositionBlocked(float x,float z,float feetY,float moveX,float moveZ,
                         const Q950Candidates& candidates,
                         CapsuleHitQ910& outHit,uint32_t& outObject) {
    outHit=CapsuleHitQ910{};
    outObject=0xffffffffu;
    float strongestInto=0.0f;
    bool blocked=false;

    auto testTri=[&](size_t triIndex,uint32_t objectIndex) {
        if (triIndex>=gWorldTriangles.size()) return;
        const CollisionTriangle& tri=gWorldTriangles[triIndex];
        if (IsWalkableTriangleQ910(tri)) return;
        CapsuleHitQ910 hit;
        if (!CapsuleContactTriangleQ910(x,z,feetY,triIndex,hit)) return;
        const float hLen=std::sqrt(hit.nx*hit.nx+hit.nz*hit.nz);
        if (hLen<1e-5f) return;
        const float into=moveX*(hit.nx/hLen)+moveZ*(hit.nz/hLen);
        if (into>=-Q718_DIRECTION_EPS) return;
        if (!blocked || into<strongestInto) {
            blocked=true;
            strongestInto=into;
            outHit=hit;
            outObject=objectIndex;
        }
    };

    for (uint32_t objectIndex:candidates.objects) {
        const Q950CollisionObject& object=gQ950Objects[objectIndex];
        for (size_t triIndex:object.triangles) testTri(triIndex,objectIndex);
    }
    for (uint32_t triIndex:candidates.legacyTriangles) testTri(triIndex,0xffffffffu);
    return blocked;
}

bool SweepQ950(float startX,float startZ,float targetX,float targetZ,float feetY,
               float& outX,float& outZ,CapsuleHitQ910& outHit,uint32_t& outObject) {
    outX=startX; outZ=startZ; outHit=CapsuleHitQ910{}; outObject=0xffffffffu;
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float distance=std::sqrt(dx*dx+dz*dz);
    if (distance<1e-7f) { outX=targetX; outZ=targetZ; return true; }

    const float r=CollisionRadiusQ712()+Q910_SWEEP_SKIN+Q950_QUERY_PAD;
    const Q950Candidates candidates=QueryQ950(std::min(startX,targetX)-r,std::max(startX,targetX)+r,
                                               feetY-r,feetY+PLAYER_HEIGHT+r,
                                               std::min(startZ,targetZ)-r,std::max(startZ,targetZ)+r);
    const float sampleStep=std::max(0.015f,CollisionRadiusQ712()*0.08f);
    const int samples=std::clamp(static_cast<int>(std::ceil(distance/sampleStep)),1,Q950_MAX_CAST_SAMPLES);
    float clearT=0.0f;
    for (int i=1;i<=samples;++i) {
        const float t=static_cast<float>(i)/static_cast<float>(samples);
        CapsuleHitQ910 hit; uint32_t objectIndex=0xffffffffu;
        if (!Q950PositionBlocked(startX+dx*t,startZ+dz*t,feetY,dx,dz,candidates,hit,objectIndex)) {
            clearT=t;
            continue;
        }
        float lo=clearT,hi=t;
        CapsuleHitQ910 hiHit=hit; uint32_t hiObject=objectIndex;
        for (int iteration=0;iteration<Q950_CAST_BISECTIONS;++iteration) {
            const float mid=(lo+hi)*0.5f;
            CapsuleHitQ910 midHit; uint32_t midObject=0xffffffffu;
            if (!Q950PositionBlocked(startX+dx*mid,startZ+dz*mid,feetY,dx,dz,candidates,midHit,midObject)) {
                lo=mid;
            } else {
                hi=mid; hiHit=midHit; hiObject=midObject;
            }
        }
        outX=startX+dx*lo; outZ=startZ+dz*lo;
        outHit=hiHit; outObject=hiObject;
        return false;
    }
    outX=targetX; outZ=targetZ;
    return true;
}

bool Q950SurfaceY(size_t triIndex,float x,float z,float& outY) {
    if (triIndex>=gWorldTriangles.size()) return false;
    const CollisionTriangle& tri=gWorldTriangles[triIndex];
    if (!IsWalkableTriangleQ910(tri)) return false;
    if (x<tri.minX-0.008f || x>tri.maxX+0.008f || z<tri.minZ-0.008f || z>tri.maxZ+0.008f) return false;
    float u=0.0f,v=0.0f,w=0.0f;
    if (!BarycentricXZ(tri,x,z,u,v,w)) return false;
    outY=tri.a.y*u+tri.b.y*v+tri.c.y*w;
    return true;
}

void Q950ConsiderLandingTriangle(Q950Landing& best,size_t triIndex,uint32_t objectIndex,
                                 float targetX,float targetZ,float feetY,float nx,float nz,
                                 float contactRise,bool topology) {
    if (triIndex>=gWorldTriangles.size()) return;
    const CollisionTriangle& tri=gWorldTriangles[triIndex];
    if (!IsWalkableTriangleQ910(tri)) return;
    const float maxRise=StepHeightQ78B()+Q950_STEP_TOLERANCE;
    const float r=CollisionRadiusQ712();
    const std::array<float,9> ahead{{0.0f,r*0.20f,r*0.40f,r*0.60f,r*0.80f,
                                     r*1.00f,r*1.25f,r*1.50f,r*1.85f}};
    const float lx=-nz,lz=nx;
    const std::array<float,3> lateral{{0.0f,r*0.32f,-r*0.32f}};
    for (float d:ahead) {
        for (float side:lateral) {
            const float px=targetX+nx*d+lx*side;
            const float pz=targetZ+nz*d+lz*side;
            float y=0.0f;
            if (!Q950SurfaceY(triIndex,px,pz,y)) continue;
            const float rise=y-feetY;
            if (rise<0.006f || rise>maxRise) continue;
            const float desired=std::clamp(contactRise,0.006f,maxRise);
            const float score=d*0.12f+std::fabs(rise-desired)*0.22f+std::fabs(side)*0.04f+(topology?-0.025f:0.0f);
            if (!best.found || score<best.score) {
                best.found=true; best.y=y; best.rise=rise; best.score=score;
                best.probeAhead=d; best.triangleIndex=triIndex; best.objectIndex=objectIndex;
                best.topology=topology;
            }
        }
    }

    // If the tread is shorter than our probe lattice, its centroid is still a
    // valid object-top candidate provided it lies directly ahead of the capsule.
    const float cx=(tri.a.x+tri.b.x+tri.c.x)/3.0f;
    const float cy=(tri.a.y+tri.b.y+tri.c.y)/3.0f;
    const float cz=(tri.a.z+tri.b.z+tri.c.z)/3.0f;
    const float relX=cx-targetX,relZ=cz-targetZ;
    const float forward=relX*nx+relZ*nz;
    const float side=std::fabs(relX*(-nz)+relZ*nx);
    const float rise=cy-feetY;
    if (forward>=-r*0.35f && forward<=r*2.20f && side<=r*1.20f && rise>=0.006f && rise<=maxRise) {
        const float desired=std::clamp(contactRise,0.006f,maxRise);
        const float score=std::max(0.0f,forward)*0.14f+side*0.08f+std::fabs(rise-desired)*0.24f+(topology?-0.020f:0.018f);
        if (!best.found || score<best.score) {
            best.found=true; best.y=cy; best.rise=rise; best.score=score;
            best.probeAhead=std::max(0.0f,forward); best.triangleIndex=triIndex;
            best.objectIndex=objectIndex; best.topology=topology;
        }
    }
}

Q950Landing FindQ950Landing(float startX,float startZ,float targetX,float targetZ,float feetY,
                            const CapsuleHitQ910& hit,uint32_t hitObject) {
    Q950Landing best;
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len<1e-7f) return best;
    const float nx=dx/len,nz=dz/len;
    const float contactRise=hit.pointY-feetY;

    // 1) Exact topology first. Walk a few rings of authored shared-edge
    // adjacency beginning at the facet that actually hit the capsule.
    if (hitObject!=0xffffffffu && hit.triangleIndex<gHkWeldNeighboursQ801.size()) {
        std::vector<size_t> queue;
        queue.reserve(48u);
        queue.push_back(hit.triangleIndex);
        std::vector<size_t> visited;
        visited.reserve(48u);
        for (size_t cursor=0u;cursor<queue.size() && cursor<48u;++cursor) {
            const size_t triIndex=queue[cursor];
            if (std::find(visited.begin(),visited.end(),triIndex)!=visited.end()) continue;
            visited.push_back(triIndex);
            Q950ConsiderLandingTriangle(best,triIndex,hitObject,targetX,targetZ,feetY,nx,nz,contactRise,true);
            if (triIndex>=gHkWeldNeighboursQ801.size()) continue;
            for (int edge=0;edge<3;++edge) {
                const int32_t neighbour=gHkWeldNeighboursQ801[triIndex][edge];
                if (neighbour<0) continue;
                const size_t ni=static_cast<size_t>(neighbour);
                if (ni>=gQ950TriangleObject.size() || gQ950TriangleObject[ni]!=hitObject) continue;
                if (std::find(visited.begin(),visited.end(),ni)==visited.end()) queue.push_back(ni);
            }
        }
    }

    // 2) Same placed authored object. This crosses separate bhk leaf blocks in
    // a stair NIF while still refusing unrelated world triangles.
    if (hitObject!=0xffffffffu && hitObject<gQ950Objects.size()) {
        for (size_t triIndex:gQ950Objects[hitObject].triangles)
            Q950ConsiderLandingTriangle(best,triIndex,hitObject,targetX,targetZ,feetY,nx,nz,contactRise,false);
    }
    if (best.found) return best;

    // 3) Separate modular piece immediately ahead (ramp-to-ramp etc.). Query the
    // spatial broadphase, then inspect only those nearby coherent objects.
    const float r=CollisionRadiusQ712()*2.25f;
    const float maxRise=StepHeightQ78B()+Q950_STEP_TOLERANCE;
    const Q950Candidates nearby=QueryQ950(targetX-r,targetX+r,feetY-0.05f,feetY+maxRise+0.08f,targetZ-r,targetZ+r);
    for (uint32_t objectIndex:nearby.objects) {
        if (objectIndex==hitObject) continue;
        const Q950CollisionObject& object=gQ950Objects[objectIndex];
        if (object.walkableTriangles==0u) continue;
        for (size_t triIndex:object.triangles)
            Q950ConsiderLandingTriangle(best,triIndex,objectIndex,targetX,targetZ,feetY,nx,nz,contactRise,false);
    }
    return best;
}

bool UpperBlockedQ950(float x,float z,float feetY,float moveX,float moveZ,CapsuleHitQ910& outHit) {
    const float r=CollisionRadiusQ712()+Q910_SWEEP_SKIN+Q950_QUERY_PAD;
    const Q950Candidates nearby=QueryQ950(x-r,x+r,feetY+StepHeightQ78B()-r,
                                           feetY+PLAYER_HEIGHT+r,z-r,z+r);
    outHit=CapsuleHitQ910{};
    float strongestInto=0.0f;
    bool blocked=false;
    auto testTri=[&](size_t triIndex) {
        CapsuleHitQ910 hit;
        if (!UpperBodyTriangleContactQ930(x,z,feetY,triIndex,hit)) return;
        const float hLen=std::sqrt(hit.nx*hit.nx+hit.nz*hit.nz);
        if (hLen<1e-5f) { outHit=hit; blocked=true; strongestInto=-1000.0f; return; }
        const float into=moveX*(hit.nx/hLen)+moveZ*(hit.nz/hLen);
        if (into>=-Q718_DIRECTION_EPS) return;
        if (!blocked || into<strongestInto) { blocked=true; strongestInto=into; outHit=hit; }
    };
    for (uint32_t objectIndex:nearby.objects)
        for (size_t triIndex:gQ950Objects[objectIndex].triangles) testTri(triIndex);
    for (uint32_t triIndex:nearby.legacyTriangles) testTri(triIndex);
    return blocked;
}

bool FindGroundQ950(float x,float z,float feetY,float& groundY) {
    const float probeRadius=CollisionRadiusQ712()*0.62f;
    const float d=probeRadius*0.70710678f;
    const std::array<Vec2,9> probes{{
        {0.0f,0.0f},{probeRadius,0.0f},{-probeRadius,0.0f},{0.0f,probeRadius},{0.0f,-probeRadius},
        {d,d},{d,-d},{-d,d},{-d,-d}
    }};
    const float maxUp=StepHeightQ78B()+Q950_STEP_TOLERANCE;
    const float maxDown=MAX_GROUND_DROP;
    const float r=probeRadius+0.04f;
    const Q950Candidates nearby=QueryQ950(x-r,x+r,feetY-maxDown-0.05f,feetY+maxUp+0.05f,z-r,z+r);
    bool found=false;
    float bestDistance=std::numeric_limits<float>::max();
    float bestY=feetY;
    auto testTri=[&](size_t triIndex) {
        if (triIndex>=gWorldTriangles.size() || !IsWalkableTriangleQ910(gWorldTriangles[triIndex])) return;
        for (const Vec2& p:probes) {
            float y=0.0f;
            if (!Q950SurfaceY(triIndex,x+p.x,z+p.y,y)) continue;
            const float delta=y-feetY;
            if (delta>maxUp || delta<-maxDown) continue;
            const float ad=std::fabs(delta);
            if (!found || ad<bestDistance-0.0005f || (std::fabs(ad-bestDistance)<=0.0005f && y>bestY)) {
                found=true; bestDistance=ad; bestY=y;
            }
        }
    };
    for (uint32_t objectIndex:nearby.objects)
        for (size_t triIndex:gQ950Objects[objectIndex].triangles) testTri(triIndex);
    for (uint32_t triIndex:nearby.legacyTriangles) testTri(triIndex);
    if (!found) return false;
    groundY=bestY;
    return true;
}

bool TryCoherentStepQ950(float startX,float startZ,float targetX,float targetZ,float feetY,
                         const CapsuleHitQ910& hit,uint32_t hitObject,float& outFeetY) {
    const float dx=targetX-startX,dz=targetZ-startZ;
    const float len=std::sqrt(dx*dx+dz*dz);
    if (len<1e-7f) return false;

    const Q950Landing landing=FindQ950Landing(startX,startZ,targetX,targetZ,feetY,hit,hitObject);
    if (landing.found) {
        CapsuleHitQ910 upper;
        if (!UpperBlockedQ950(targetX,targetZ,landing.y,dx,dz,upper)) {
            outFeetY=landing.y;
            if (gQ950StepLogs<120u || (gResolveCounter%360u)==0u) {
                ++gQ950StepLogs;
                const Q950CollisionObject* object=landing.objectIndex<gQ950Objects.size()?&gQ950Objects[landing.objectIndex]:nullptr;
                Q6G_LOGI("Q9.5 SHAPE STEP: hitObject=%u landingObject=%u ref=%08X contactRise=%.3f landingRise=%.3f probeAhead=%.3f topology=%d tri=%zu",
                         hitObject,landing.objectIndex,object?object->refFormId:0u,
                         hit.pointY-feetY,landing.rise,landing.probeAhead,landing.topology?1:0,landing.triangleIndex);
            }
            return true;
        }
    }

    // No raised top: preserve Q9.3's useful low-lip/wood/pipe behaviour, but
    // only when ordinary support still exists and the target has no upper-body
    // collision. This is an envelope rule, not a triangle-height rule.
    float floorY=feetY;
    CapsuleHitQ910 upper;
    if (FindGroundQ950(targetX,targetZ,feetY,floorY) &&
        std::fabs(floorY-feetY)<=0.08f &&
        !UpperBlockedQ950(targetX,targetZ,floorY,dx,dz,upper)) {
        outFeetY=floorY;
        if (gQ950LowPassLogs<60u || (gResolveCounter%480u)==0u) {
            ++gQ950LowPassLogs;
            Q6G_LOGI("Q9.5 LOW ENVELOPE PASS: hitObject=%u contactRise=%.3f floorDelta=%.3f",
                     hitObject,hit.pointY-feetY,floorY-feetY);
        }
        return true;
    }
    return false;
}

Vec2 ProjectAlongWallQ950(Vec2 move,const CapsuleHitQ910& hit) {
    const float hLen=std::sqrt(hit.nx*hit.nx+hit.nz*hit.nz);
    if (hLen<1e-6f) return Vec2{0.0f,0.0f};
    const float nx=hit.nx/hLen,nz=hit.nz/hLen;
    const float into=move.x*nx+move.y*nz;
    if (into<0.0f) { move.x-=nx*into; move.y-=nz*into; }
    return move;
}

bool MoveExteriorSubstepQ950(float& x,float& z,float& feetY,
                             float moveX,float moveZ,bool& steppedThisFrame,
                             uint32_t& contactSurfaces) {
    if (moveX*moveX+moveZ*moveZ<1e-10f) return true;
    EnsureQ950SpatialIndex();
    static bool loggedReady=false;
    if (!loggedReady) {
        loggedReady=true;
        Q6G_LOGI("Q9.5 CONTROLLER READY: mode=coherent-placed-objects+spatial-grid+topology-step+shape-narrowphase triangleSoupStepPath=DISABLED fullWorldMovementScans=0 oldQ91toQ94StepPath=disabled");
    }

    const float startX=x,startZ=z;
    const float targetX=x+moveX,targetZ=z+moveZ;
    float castX=x,castZ=z;
    CapsuleHitQ910 hit; uint32_t hitObject=0xffffffffu;
    if (SweepQ950(startX,startZ,targetX,targetZ,feetY,castX,castZ,hit,hitObject)) {
        x=targetX; z=targetZ;
        return true;
    }
    ++contactSurfaces;

    float stepY=feetY;
    if (TryCoherentStepQ950(startX,startZ,targetX,targetZ,feetY,hit,hitObject,stepY)) {
        x=targetX; z=targetZ; feetY=stepY; steppedThisFrame=true;
        return true;
    }

    x=castX; z=castZ;
    CapsuleHitQ910 wallHit=hit;
    uint32_t wallObject=hitObject;
    for (int iteration=0;iteration<2;++iteration) {
        Vec2 remaining{targetX-x,targetZ-z};
        if (remaining.x*remaining.x+remaining.y*remaining.y<0.00000025f) break;
        const Vec2 slide=ProjectAlongWallQ950(remaining,wallHit);
        if (slide.x*slide.x+slide.y*slide.y<0.00000025f) break;
        if (slide.x*remaining.x+slide.y*remaining.y<=0.0f) break;
        const float sxTarget=x+slide.x,szTarget=z+slide.y;
        float sx=x,sz=z; CapsuleHitQ910 slideHit; uint32_t slideObject=0xffffffffu;
        if (SweepQ950(x,z,sxTarget,szTarget,feetY,sx,sz,slideHit,slideObject)) {
            x=sxTarget; z=szTarget;
            if (gQ950SlideLogs<30u) {
                ++gQ950SlideLogs;
                Q6G_LOGI("Q9.5 WALL SLIDE: requested=(%.3f %.3f) projected=(%.3f %.3f)",
                         remaining.x,remaining.y,slide.x,slide.y);
            }
            return true;
        }
        float slideStepY=feetY;
        if (TryCoherentStepQ950(x,z,sxTarget,szTarget,feetY,slideHit,slideObject,slideStepY)) {
            x=sxTarget; z=szTarget; feetY=slideStepY; steppedThisFrame=true;
            return true;
        }
        x=sx; z=sz; wallHit=slideHit; wallObject=slideObject; ++contactSurfaces;
    }

    if (gQ950BlockedLogs<40u || (gResolveCounter%480u)==0u) {
        ++gQ950BlockedLogs;
        uint32_t ref=0u;
        if (wallObject<gQ950Objects.size()) ref=gQ950Objects[wallObject].refFormId;
        const CollisionTriangle* tri=(hit.hit&&hit.triangleIndex<gWorldTriangles.size())?&gWorldTriangles[hit.triangleIndex]:nullptr;
        const auto src=tri?gSurfaceSourcesQ722.find(tri->surfaceKeyQ714):gSurfaceSourcesQ722.end();
        Q6G_LOGI("Q9.5 BLOCKED: object=%u ref=%08X EDID=%s start=(%.3f %.3f) pos=(%.3f %.3f) target=(%.3f %.3f) contactRise=%.3f",
                 hitObject,ref,(src!=gSurfaceSourcesQ722.end()&&!src->second.editorId.empty())?src->second.editorId.c_str():"<none>",
                 startX,startZ,x,z,targetX,targetZ,hit.pointY-feetY);
    }
    return (x-startX)*(x-startX)+(z-startZ)*(z-startZ)>0.00000025f;
}

]=])

string(REPLACE
    "Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {"
    "${Q950_COHERENT_SPATIAL_SOURCE}Vec2 ProjectAlongWallQ910(Vec2 move,const CapsuleHitQ910& hit) {"
    Q950_CONTROLLER_SOURCE "${Q950_CONTROLLER_SOURCE}")

# Q9.5 owns the active horizontal movement path. Q9.1-Q9.4 remain compiled only
# as an A/B reference and cannot veto a successful coherent-shape step.
string(REPLACE
    "if (!MoveExteriorSubstepQ910(x, z, feetY, subDx, subDz,"
    "if (!MoveExteriorSubstepQ950(x, z, feetY, subDx, subDz,"
    Q950_CONTROLLER_SOURCE "${Q950_CONTROLLER_SOURCE}")

# Ground/support was another hidden O(worldTriangles) hot path in Q8.0.
string(REPLACE
    "bool grounded = FindHkGroundQ800(x, z, feetY, groundY);"
    "bool grounded = FindGroundQ950(x, z, feetY, groundY);"
    Q950_CONTROLLER_SOURCE "${Q950_CONTROLLER_SOURCE}")

string(REPLACE
    "mode=raised-support-step-envelope+local-candidates+target-upper-body+wall-projection persistentOverlap=none"
    "mode=coherent-placed-objects+spatial-grid+topology-step+shape-narrowphase persistentOverlap=none"
    Q950_CONTROLLER_SOURCE "${Q950_CONTROLLER_SOURCE}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q950.inc"
     "${Q950_CONTROLLER_SOURCE}")
string(REPLACE
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q940.inc\""
    "#include \"${CMAKE_CURRENT_BINARY_DIR}/fo3-collision-controller-q950.inc\""
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")