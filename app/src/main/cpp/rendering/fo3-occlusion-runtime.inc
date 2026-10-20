// Included after the stereo camera and scene lists are defined.
namespace q241 {
using Clock=std::chrono::steady_clock;
constexpr uint32_t MaxTests=40000;
constexpr double BuildBudgetUs=900,TotalBudgetUs=1500;
fo3occ::Buffer eyes[2];
uint64_t generation=0,frame=~uint64_t{0},revision=0;
const GpuObject* storage=nullptr;
size_t objectCount=0;
float matrices[2][16]{};
uint32_t world=0,tests=0,tested=0,hidden=0,candidates=0,cooldown=0;
uint64_t hiddenCorners=0;
double buildUs=0,testUs=0;
bool active=false,stopped=false;
bool Enabled() {
    static const bool enabled=[] {
        char value[PROP_VALUE_MAX]{};
        __system_property_get("debug.falloutquest.occlusion",value);
        return !value[0]||std::atoi(value)!=0;
    }();
    return enabled;
}
double Us(Clock::time_point start) {
    return std::chrono::duration<double,std::micro>(Clock::now()-start).count();
}
bool Static(const GpuObject& o) {
    return !o.q220LooseObject&&!o.physicalWeapon&&!o.q2400SwingDoor&&
           !o.q210PlayerBody&&!o.q230NpcActor&&!o.skinMode&&
           (o.q1990NativeLod||o.baseRecordType=="STAT"||o.baseRecordType=="SCOL"||o.baseRecordType=="TREE");
}
void Add(const fo3occ::Point* points,unsigned n,bool twoSided=false,bool clockwise=false) {
    ++candidates;
    for(unsigned eye=0;eye<2;++eye) {
        fo3occ::Polygon p;
        if(fo3occ::ProjectPolygon(gStereoMvp[eye],points,n,twoSided,clockwise,p))eyes[eye].Add(p);
    }
}
void Prepare() {
    const auto rev=gFo3VisualPublication.Revision();
    if(frame==gStereoFrame&&revision==rev&&storage==gObjects.data()&&
       objectCount==gObjects.size()&&world==gExteriorWorldspaceQ1890&&
       std::memcmp(matrices,gStereoMvp,sizeof(matrices))==0)return;
    const bool newFrame=frame!=gStereoFrame;
    if(newFrame) {
        if(buildUs>0) {
            if(buildUs+testUs>TotalBudgetUs)cooldown=30;
            else if(hiddenCorners<10000)cooldown=7;
            static uint64_t lastReport=0;
            if(gStereoFrame-lastReport>=60) {
                lastReport=gStereoFrame;
                Q6H_LOGI("Q241 OCCLUSION: frame=%llu candidates=%u polygons=%zu/%zu testedShapes=%u hiddenShapes=%u hiddenCorners=%llu polygonTests=%u buildUs=%.1f testUs=%.1f stopped=%d cooldown=%u scope=main-stereo-static mirrors=independent distanceCut=0 gpuReadbacks=0",
                    (unsigned long long)frame,candidates,eyes[0].count,eyes[1].count,tested,hidden,
                    (unsigned long long)hiddenCorners,tests,buildUs,testUs,stopped,cooldown);
            }
        }
    }
    frame=gStereoFrame;revision=rev;storage=gObjects.data();objectCount=gObjects.size();world=gExteriorWorldspaceQ1890;
    std::memcpy(matrices,gStereoMvp,sizeof(matrices));
    ++generation;active=false;stopped=false;eyes[0].Clear();eyes[1].Clear();
    tests=tested=hidden=candidates=0;hiddenCorners=0;buildUs=testUs=0;
    if(!Enabled()||!rev||!gStereoMvpReady||gFo3Environment.interior||!gExteriorStreamingActiveQ1890)return;
    if(newFrame&&cooldown){--cooldown;return;}
    const auto start=Clock::now();
    // At most 64 static source shapes, each with <=16 original triangles.
    // The pointer list is rebuilt only with normal visual publication.
    size_t selected=0,visited=0;
    for(const auto* source:gOccluderFrameOrder) {
        if((++visited%32)==0&&Us(start)>BuildBudgetUs)break;
        const auto& o=*source;
        if(!Static(o)||!o.occluders||!Q1970ShouldRenderFullDetail(o)||!Q2015AabbVisible(o))continue;
        if(gPlayerSession&&gPlayerSession->player.IsCollected(o.refFormId))continue;
        if(world==0x3Cu&&gQ2021PlayerSceneValid) {
            // Alpha-to-coverage LOD fades are holes, not solid occluders.
            const float dx=std::max(std::fabs(o.minX-gQ2021PlayerSceneX),std::fabs(o.maxX-gQ2021PlayerSceneX));
            const float dz=std::max(std::fabs(o.minZ-gQ2021PlayerSceneZ),std::fabs(o.maxZ-gQ2021PlayerSceneZ));
            const float limit=o.q2025LandscapeRock&&!o.q2025VisibleWhenDistant ? Q2025_ROCK_FADE_START_M : Q2021_DETAIL_FADE_START_M;
            if(dx*dx+dz*dz>=(limit-0.5f)*(limit-0.5f))continue;
        }
        const bool two=o.stencilDrawModePresent&&o.stencilDrawMode==3;
        const bool clockwise=o.stencilDrawModePresent&&o.stencilDrawMode==2;
        for(const auto& t:*o.occluders)Add(t.p,t.count,two,clockwise);
        if(++selected>=64||Us(start)>BuildBudgetUs)break;
    }
    // Published LAND only. Two closest resident CELLs, interior quads only:
    // border heights can be stitched differently from grounding heights.
    struct Cell {const Fo3TerrainCellQ76* land=nullptr;float distance=1e30f;};
    std::array<Cell,2> nearest{};
    const float gx=gExteriorOriginXQ1890+gFo3EyePosition[0]*70.f;
    const float gy=gExteriorOriginYQ1890-gFo3EyePosition[2]*70.f;
    const int cx=int(std::floor(gx/4096.f)),cy=int(std::floor(gy/4096.f));
    for(int y=cy-1;y<=cy+1&&Us(start)<=BuildBudgetUs;++y)for(int x=cx-1;x<=cx+1;++x) {
        const auto* cell=GetFo3TerrainOcclusionCellQ241(world,x,y,
            gExteriorOriginXQ1890,gExteriorOriginYQ1890,gExteriorOriginZQ1890);
        if(!cell)continue;
        const float dx=(x+.5f)*4096.f-gx,dy=(y+.5f)*4096.f-gy,d=dx*dx+dy*dy;
        if(d<nearest[0].distance){nearest[1]=nearest[0];nearest[0]={cell,d};}
        else if(d<nearest[1].distance)nearest[1]={cell,d};
    }
    for(const auto& entry:nearest) {
        if(!entry.land||Us(start)>BuildBudgetUs)break;
        const auto& cell=*entry.land;
        auto point=[&](int x,int y) -> fo3occ::Point {
            return {(cell.gridX*4096.f+x*128.f-gExteriorOriginXQ1890)/70.f,
                    FLOOR_Y+(cell.heights[y*33+x]-gExteriorOriginZQ1890)/70.f,
                    SCENE_FORWARD-(cell.gridY*4096.f+y*128.f-gExteriorOriginYQ1890)/70.f};
        };
        bool finished=false;
        for(int y=1;y<31&&!finished;++y)for(int x=1;x<31;++x) {
            if((candidates%32)==0&&Us(start)>BuildBudgetUs){finished=true;break;}
            const unsigned q=(y>=16?2:0)+(x>=16?1:0);
            if(cell.forceHideMask&(1u<<q))continue;
            const fo3occ::Point p[]{point(x,y),point(x+1,y),point(x+1,y+1),point(x,y+1)};
            Add(p,4);
        }
    }
    buildUs=Us(start);active=eyes[0].count&&eyes[1].count;
}
}

bool Q241Hidden(const GpuObject& object) {
    using namespace q241;
    if(!active||frame!=gStereoFrame||world!=gExteriorWorldspaceQ1890||
       IsFo3LoadingVisible()||gWaterReflectionPassQ2090||!Static(object))return false;
    if(object.occlusionGeneration==generation)return object.occlusionHidden;
    object.occlusionGeneration=generation;object.occlusionHidden=false;
    if(stopped||tests>=MaxTests||buildUs+testUs>TotalBudgetUs){stopped=true;return false;}
    const auto start=Clock::now();++tested;
    const questrender::Bounds b{object.minX,object.maxX,object.minY,object.maxY,object.minZ,object.maxZ};
    bool covered=true;
    for(unsigned eye=0;eye<2&&covered;++eye) {
        fo3occ::Rect r;
        covered=fo3occ::ProjectBounds(gStereoMvp[eye],b,r)&&eyes[eye].Hidden(r,tests,MaxTests);
    }
    if(covered){object.occlusionHidden=true;++hidden;hiddenCorners+=object.vertexCount;}
    testUs+=Us(start);return covered;
}
