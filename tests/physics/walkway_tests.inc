// Optional user-supplied originals, exercised through scene publication,
// horizontal collision and the production gravity adapter. No asset bytes ship.
static void OriginalMegatonWalkways(const char* directory){
 unsigned total=0;
 for(const std::string model:{"scrapgroundplatesreg01.nif","scrapgroundplatesreg02.nif","megatonrampstraightreg.nif","megatonrampstraightreg02.nif","megatonrampstraightsml.nif","megatonrampturn180reg.nif","megatonrampturn45sml.nif","megatonrampturn90reg.nif","megatonrampturn90sml.nif"}){
  std::ifstream f(std::string(directory)+"/"+model,std::ios::binary);assets[model]={std::istreambuf_iterator<char>(f),{}};assert(!assets[model].empty());
  publish(prepare({placement(600,model)},true,1));StairStart();gExteriorAllBhksQ78A=true;
  const auto triangles=gWorldTriangles;unsigned count=0;
  for(const auto& t:triangles){
   if(!IsWalkableTriangleQ910(t)||t.normal.y<0)continue;
   float x=(t.a.x+t.b.x+t.c.x)/3,z=(t.a.z+t.b.z+t.c.z)/3,y=(t.a.y+t.b.y+t.c.y)/3;
   float support=y;if(!FindGroundQ950(x,z,y,support)||std::abs(support-y)>.04f)continue;
   const float dx=((x+t.a.x)*.5f-x)/30,dz=((z+t.a.z)*.5f-z)/30;ResetFo3PlayerGravity();
   for(int frame=0;frame<30;++frame){float nx,nz,ny;assert(ResolveFo3PlayerMotionWithGravity(x,z,x+dx,z+dz,y,1.f/72,&nx,&nz,&ny));x=nx;z=nz;y=ny;}
   float u,v,w;if(!BarycentricXZ(t,x,z,u,v,w))continue;
   const float expected=t.a.y*u+t.b.y*v+t.c.y*w;
   assert(y>=expected-.04f);++count;
  }
  assert(count);total+=count;std::cout<<"Original "<<model<<": "<<count<<" authored support trajectories passed\n";
 }
 assert(total>=200);std::cout<<"Original walkways: "<<total<<" moving support trajectories passed\n";
}

static void OriginalPlacedWalkways(const char* root,const char* manifest){
 assetRoot=root;std::ifstream f(manifest);assert(f);
 std::vector<Fo3WorldPlacement> placements;Fo3WorldPlacement placed;
 while(f>>placed.refFormId>>placed.baseRecordType>>placed.x>>placed.y>>placed.z>>placed.rx>>placed.ry>>placed.rz>>placed.scale){
  f.get();std::getline(f,placed.modelPath);placements.push_back(placed);
 }
 publish(prepare(placements,true,0xa74));StairStart();gExteriorAllBhksQ78A=true;SynchronizeFo3RigidWorld(0,0);
 unsigned count=0,bad=0,headBlocked=0;std::unordered_set<uint32_t> refs;
 const auto triangles=gWorldTriangles;
 for(const auto& t:triangles){
  const auto source=gSurfaceSourcesQ722.find(t.surfaceKeyQ714);if(source==gSurfaceSourcesQ722.end())continue;
  const auto& model=source->second.modelPath;
  if(model.find("megatonramp")==std::string::npos&&model.find("scrapgroundplate")==std::string::npos)continue;
  if(!IsWalkableTriangleQ910(t)||t.normal.y<0)continue;
  float x=(t.a.x+t.b.x+t.c.x)/3,z=(t.a.z+t.b.z+t.c.z)/3,y=(t.a.y+t.b.y+t.c.y)/3;
  float support=y;if(!FindGroundQ950(x,z,y,support)||std::abs(support-y)>.04f)continue;
  if(!fo3rigid::Live().CanExpandCapsule({x,y,z},CollisionRadiusQ712(),.3f,PLAYER_HEIGHT)){++headBlocked;continue;}
  const float startX=x,startY=y,startZ=z;
  for(int hz:{36,45,72}){
   x=startX;y=startY;z=startZ;const int frames=hz*5/12;
   const float dx=((x+t.a.x)*.5f-x)/frames,dz=((z+t.a.z)*.5f-z)/frames;ResetFo3PlayerGravity();
   for(int frame=0;frame<frames;++frame){
    float nx,nz,ny;assert(ResolveFo3PlayerMotionWithGravity(x,z,x+dx,z+dz,y,1.f/hz,&nx,&nz,&ny));x=nx;z=nz;y=ny;
   }
   float u,v,w;if(!BarycentricXZ(t,x,z,u,v,w))continue;
   const float expected=t.a.y*u+t.b.y*v+t.c.y*w;++count;refs.insert(source->second.refFormId);
   if(y<expected-.04f){++bad;std::cerr<<"Lost support ref="<<std::hex<<source->second.refFormId<<std::dec<<" hz="<<hz<<" feet="<<y<<" surface="<<expected<<"\n";}
  }
 }
 assert(bad==0&&count>=1000&&refs.size()>=50);
 std::cout<<"Original placed walkways: placements="<<placements.size()<<" triangles="<<triangles.size()<<" refs="<<refs.size()<<" trajectories="<<count<<" headBlocked="<<headBlocked<<" failures="<<bad<<"\n";
}
