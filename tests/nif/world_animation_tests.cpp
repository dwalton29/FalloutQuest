#include "rendering/fo3-visual-admission.h"
#include "../../app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp"
#include "world/interaction/fo3-door-nif-animation.h"
#include <cassert>
#include <fstream>
#include <iostream>
static std::vector<uint8_t> overrideBytes;
bool LoadFalloutMeshFile(const std::string& path,std::vector<uint8_t>& bytes,std::string* resolved){if(!overrideBytes.empty()){bytes=overrideBytes;return true;}std::ifstream file(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});if(resolved)*resolved=path;return !bytes.empty();}
// Synthetic graph tests exercise admission without shipping original NIF bytes.
static void VisibilityAdmissionTests(){
 auto u32=[](std::vector<uint8_t>& b,uint32_t n){for(int i=0;i<4;++i)b.push_back(uint8_t(n>>(8*i)));};
 auto base=[&](bool hidden){std::vector<uint8_t> b;u32(b,INVALID_REF);u32(b,0);u32(b,INVALID_REF);
   u32(b,hidden?1:0);for(int i=0;i<13;++i)u32(b,0);u32(b,0);u32(b,INVALID_REF);return b;};
 auto node=[&](bool hidden,bool sw,uint32_t active){auto b=base(hidden);u32(b,2);u32(b,1);u32(b,2);u32(b,0);
   if(sw){b.push_back(0);b.push_back(0);u32(b,active);}return b;};
 auto check=[&](bool hidden,bool sw,uint32_t active,bool includeHidden,size_t expected){
   NifHeader h;h.userVersion=11;h.bsVersion=34;h.numBlocks=3;h.blockTypes={sw?"NiSwitchNode":"NiNode","NiTriShape"};h.blockTypeIndices={0,1,1};
   std::vector<uint8_t> bytes;for(const auto& b:{node(hidden,sw,active),base(false),base(false)}){h.blockOffsets.push_back(bytes.size());h.blockSizes.push_back(b.size());bytes.insert(bytes.end(),b.begin(),b.end());}
   std::vector<uint8_t> selected;Q970SceneStats stats;assert(Q970BuildRenderableSet(bytes,h,selected,stats,includeHidden));
   assert(stats.selectedShapes==expected&&!stats.fallbackAll);
 };
 check(true,false,0,false,0);check(false,true,INVALID_REF,false,0);
 check(false,true,0,false,1);check(false,true,1,false,1);
 check(true,false,0,true,2);check(false,true,0,true,1);check(false,true,INVALID_REF,true,0);
 assert(Fo3IsTriggerHelperModel("Triggers\\TrigActorBox03.NIF"));
 assert(Fo3IsTriggerHelperModel("TRIGGERS/TrigActorBox03.nif"));
 assert(!Fo3IsTriggerHelperModel("Architecture\\Wasteland\\SuperDuperMart01.NIF"));
}
int main(int argc,char** argv){
  VisibilityAdmissionTests();
  using namespace fo3anim;
  Skeleton s;Bone root;root.name="root";s.bones.push_back(root);assert(FinalizeSkeleton(s));
  Clip c;c.stop=2;c.cycle=2;PropertyTrack property;property.bone="root";property.kind=PropertyKind::Visibility;
  property.base[0]=0;property.values.dimensions=1;property.values.keys={{0,{0,0,0,0},{},{}},{1,{1,0,0,0},{},{}}};c.properties.push_back(property);
  Pose p;assert(Sample(s,c,.99,p)&&p.properties[0].value[0]==0);assert(Sample(s,c,1,p)&&p.properties[0].value[0]==1);
  c.properties[0].kind=PropertyKind::Alpha;c.properties[0].values.interpolation=1;
  assert(Sample(s,c,.5,p)&&std::abs(p.properties[0].value[0]-.5)<1e-5);
  // Optional integration with the unmodified original multi-controller NIF.
  if(argc>1){std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    std::vector<int> mapping;std::vector<Clip> clips;assert(DecodeObjectAnimation(bytes,s,mapping,clips));
    const Clip *opening=nullptr,*idle=nullptr;for(const auto& clip:clips){assert(!clip.ignoredControllers);if(clip.name=="Forward")opening=&clip;if(clip.name=="Left")idle=&clip;}
    assert(opening&&idle&&opening->cycle==2&&idle->cycle==0&&opening->stop-opening->start==15);
    Pose first,middle,last,settled;assert(Sample(s,*opening,0,first)&&Sample(s,*opening,7.5,middle)&&Sample(s,*opening,15,last)&&Sample(s,*idle,0,settled));
    size_t moved=0;for(size_t i=0;i<first.global.size();++i){if(first.global[i]!=last.global[i])++moved;}
    assert(moved>5);bool vis=false,alpha=false,emissive=false,emitter=false;
    for(const auto& channel:opening->properties){vis|=channel.kind==PropertyKind::Visibility;alpha|=channel.kind==PropertyKind::Alpha;emissive|=channel.kind==PropertyKind::Emissive;emitter|=channel.kind==PropertyKind::EmitterActive;}
    assert(vis&&alpha&&emissive&&emitter);
    std::vector<Fo3StaticNifMesh> ordinary,animated;
    assert(LoadFo3StaticNifMeshes(argv[1],ordinary,false,true));
    assert(LoadFo3StaticNifMeshes(argv[1],animated,false,true,true));
    assert(animated.size()>=ordinary.size());
    // Perturb one shape's hidden flag in memory to verify the static loader
    // retains a resource that a later visibility controller may reveal.
    NifHeader header;assert(ParseHeader(bytes,header));const auto block=ordinary.front().shapeBlock;
    Cursor objectNet(BlockData(bytes,header,block),header.blockSizes[block]);assert(ParseObjectNetPrefix(objectNet));
    overrideBytes=bytes;overrideBytes[header.blockOffsets[block]+objectNet.offset()]|=1;
    assert(LoadFo3StaticNifMeshes(argv[1],ordinary,false,true));
    assert(LoadFo3StaticNifMeshes(argv[1],animated,false,true,true));
    assert(animated.size()>ordinary.size());overrideBytes.clear();
    assert(!DecodeUiAnimation(bytes,s,mapping,clips)); // UI adapters remain strict.
    std::cout<<"Original world NIF transform/material/visibility/emitter channels passed\n";
  }
  std::cout<<"World controller sampling passed\n";
}
