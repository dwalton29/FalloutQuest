namespace {
// Moving authored BHKs keep their triangle slots and topology. Only their own
// bounds memberships change; all unrelated world triangles/indices stay intact.
uint64_t gAnimatedIndexEpoch=0,gCollisionTopologyEpoch=1;
std::unordered_map<uint32_t,std::vector<uint32_t>> gAnimatedReferenceTriangles;

void InvalidateAnimatedReferenceIndex() {
    if(++gCollisionTopologyEpoch==0)++gCollisionTopologyEpoch;
}

template<class Bounds,class Coord,class Visit>
void VisitAnimatedBounds(const Bounds& b,Coord coord,Visit visit) {
    const int x0=coord(b.minX),x1=coord(b.maxX),z0=coord(b.minZ),z1=coord(b.maxZ);
    const int64_t nx=int64_t(x1)-x0+1,nz=int64_t(z1)-z0+1;
    if(nx<=0||nz<=0||nx>64||nz>64||nx*nz>64){visit(true,0);return;}
    for(int x=x0;x<=x1;++x)for(int z=z0;z<=z1;++z)visit(false,Q225CellKey(x,z));
}
template<class V> void RemoveAnimatedIndex(V& values,uint32_t index) {
    values.erase(std::remove(values.begin(),values.end(),index),values.end());
}
void MoveAnimatedTriangleGrid(uint32_t index,const CollisionTriangle& old,const CollisionTriangle& next) {
    VisitAnimatedBounds(old,Q225CellCoord,[&](bool large,uint64_t key){
        if(large){RemoveAnimatedIndex(gInteractionLargeTriangles,index);return;}
        const auto cell=gQ225DynamicGrid.find(key);if(cell==gQ225DynamicGrid.end())return;
        RemoveAnimatedIndex(cell->second,index);if(cell->second.empty())gQ225DynamicGrid.erase(cell);
    });
    VisitAnimatedBounds(next,Q225CellCoord,[&](bool large,uint64_t key){
        if(large)gInteractionLargeTriangles.push_back(index);else gQ225DynamicGrid[key].push_back(index);
    });
}
template<class Bounds> void RefitAnimatedBounds(Bounds& b) {
    bool first=true;b.walkableTriangles=0;
    for(size_t index:b.triangles){const auto& t=gWorldTriangles[index];
        if(first){b.minX=t.minX;b.maxX=t.maxX;b.minY=t.minY;b.maxY=t.maxY;b.minZ=t.minZ;b.maxZ=t.maxZ;first=false;}
        else {b.minX=std::min(b.minX,t.minX);b.maxX=std::max(b.maxX,t.maxX);
            b.minY=std::min(b.minY,t.minY);b.maxY=std::max(b.maxY,t.maxY);
            b.minZ=std::min(b.minZ,t.minZ);b.maxZ=std::max(b.maxZ,t.maxZ);}
        if(std::fabs(t.normal.y)>=WalkableNormalThresholdQ714(t))++b.walkableTriangles;
    }
}
void EnsureAnimatedReferenceIndex() {
    if(gAnimatedIndexEpoch!=gCollisionTopologyEpoch){
        gAnimatedReferenceTriangles.clear();
        for(uint32_t i=0;i<gWorldTriangles.size();++i){
            const auto source=gSurfaceSourcesQ722.find(gWorldTriangles[i].surfaceKeyQ714);
            if(source!=gSurfaceSourcesQ722.end())gAnimatedReferenceTriangles[source->second.refFormId].push_back(i);
        }
        gAnimatedIndexEpoch=gCollisionTopologyEpoch;
    }
}
bool UpdateAnimatedCollisionInPlace(uint32_t reference,const std::vector<CollisionTriangle>& moved) {
    EnsureAnimatedReferenceIndex();
    const auto found=gAnimatedReferenceTriangles.find(reference);
    if(found==gAnimatedReferenceTriangles.end()||found->second.size()!=moved.size())return false;
    const auto& indices=found->second;
    // Establish topology once after residency changes, never during each pose.
    EnsureQ950SpatialIndex();
    for(size_t i=0;i<indices.size();++i){
        const auto& old=gWorldTriangles[indices[i]];const auto& next=moved[i];
        if(old.surfaceKeyQ714!=next.surfaceKeyQ714||old.meshKeyQ801!=next.meshKeyQ801||
           old.animationTargetBlock!=next.animationTargetBlock||old.vertexAQ801!=next.vertexAQ801||
           old.vertexBQ801!=next.vertexBQ801||old.vertexCQ801!=next.vertexCQ801)return false;
    }
    std::unordered_set<size_t> shapes;
    std::unordered_set<uint32_t> objects;
    for(size_t i=0;i<indices.size();++i){
        const uint32_t index=indices[i];const auto old=gWorldTriangles[index];const auto& next=moved[i];
        if(old.a.x==next.a.x&&old.a.y==next.a.y&&old.a.z==next.a.z&&
           old.b.x==next.b.x&&old.b.y==next.b.y&&old.b.z==next.b.z&&
           old.c.x==next.c.x&&old.c.y==next.c.y&&old.c.z==next.c.z)continue;
        MoveAnimatedTriangleGrid(index,old,next);
        const auto shape=gHkShapeIndexQ900.find(old.meshKeyQ801);
        if(shape!=gHkShapeIndexQ900.end())shapes.insert(shape->second);
        const uint32_t object=gQ950TriangleObject[index];
        if(object!=0xffffffffu)objects.insert(object);
        else {
            VisitAnimatedBounds(old,Q950CellCoord,[&](bool large,uint64_t key){
                if(large){RemoveAnimatedIndex(gQ950LargeLegacy,index);return;}
                const auto c=gQ950Grid.find(key);if(c==gQ950Grid.end())return;
                RemoveAnimatedIndex(c->second.legacyTriangles,index);
                if(c->second.objects.empty()&&c->second.legacyTriangles.empty())gQ950Grid.erase(c);
            });
            VisitAnimatedBounds(next,Q950CellCoord,[&](bool large,uint64_t key){
                if(large)gQ950LargeLegacy.push_back(index);else gQ950Grid[key].legacyTriangles.push_back(index);
            });
        }
        gWorldTriangles[index]=next;
    }
    for(size_t index:shapes)RefitAnimatedBounds(gHkShapesQ900[index]);
    for(uint32_t index:objects){auto& object=gQ950Objects[index];
        VisitAnimatedBounds(object,Q950CellCoord,[&](bool large,uint64_t key){
            if(large){RemoveAnimatedIndex(gQ950LargeObjects,index);return;}
            const auto c=gQ950Grid.find(key);if(c==gQ950Grid.end())return;
            RemoveAnimatedIndex(c->second.objects,index);
            if(c->second.objects.empty()&&c->second.legacyTriangles.empty())gQ950Grid.erase(c);
        });
        RefitAnimatedBounds(object);
        VisitAnimatedBounds(object,Q950CellCoord,[&](bool large,uint64_t key){
            if(large)gQ950LargeObjects.push_back(index);else gQ950Grid[key].objects.push_back(index);
        });
    }
    // Weld links use authored mesh/vertex IDs, unaffected by rigid node motion.
    gHkManifoldValidQ800=false;
    return true;
}

} // namespace
