static std::unordered_set<std::string> gQ240AiDiagnostics;
static void Q240AiLogOnce(uint32_t actor,uint32_t package,const std::string& why) {
  char prefix[40];std::snprintf(prefix,sizeof(prefix),"%08X:%08X:",actor,package);
  const std::string key=std::string(prefix)+why;
  if(gQ240AiDiagnostics.insert(key).second)
    Q6H_LOGI("NPC PACKAGE UNSUPPORTED actor=%08X package=%08X reason=%s",actor,package,why.c_str());
}
static std::array<float,3> Q240GamePosition(const Q230ActorVisual& actor) {
  return {gSceneCenterXQ1730+actor.runtime.position[0]*FO3_UNITS_PER_METRE,
          gSceneCenterYQ1730-(actor.runtime.position[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
          gSceneFloorZQ1730+(actor.runtime.position[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
}
static std::array<float,3> Q240ScenePosition(const std::array<float,3>& game) {
  return {(game[0]-gSceneCenterXQ1730)/FO3_UNITS_PER_METRE,
          FLOOR_Y+(game[2]-gSceneFloorZQ1730)/FO3_UNITS_PER_METRE,
          SCENE_FORWARD-(game[1]-gSceneCenterYQ1730)/FO3_UNITS_PER_METRE};
}
static float Q240PlanarDistance(const std::array<float,3>& a,const std::array<float,3>& b) {
  const float dx=a[0]-b[0],dy=a[1]-b[1];return std::sqrt(dx*dx+dy*dy);
}
static std::array<float,3> Q240Centroid(const Fo3NpcNavMeshQ240& mesh,size_t triangle) {
  const auto& t=mesh.triangles[triangle];std::array<float,3> out{};
  for(int v=0;v<3;++v)for(int c=0;c<3;++c)out[c]+=mesh.vertices[t.vertex[v]][c]/3.f;
  return out;
}
struct Q240Node {
  size_t mesh=SIZE_MAX,triangle=SIZE_MAX;
  bool Valid()const{return mesh!=SIZE_MAX&&triangle!=SIZE_MAX;}
};
static size_t Q240Flat(const Q240NavigationGraph& graph,const Q240Node& node) {
  return node.Valid()&&node.mesh<graph.triangleOffsets.size()?graph.triangleOffsets[node.mesh]+node.triangle:SIZE_MAX;
}
static Q240Node Q240NodeAt(const Q240NavigationGraph& graph,size_t flat) {
  if(flat>=graph.triangleCount||graph.triangleOffsets.empty())return {};
  auto it=std::upper_bound(graph.triangleOffsets.begin(),graph.triangleOffsets.end(),flat);
  const size_t mesh=it==graph.triangleOffsets.begin()?0u:size_t((it-graph.triangleOffsets.begin())-1);
  const size_t triangle=flat-graph.triangleOffsets[mesh];
  if(mesh>=graph.meshes.size()||triangle>=graph.meshes[mesh]->triangles.size())return {};
  return {mesh,triangle};
}
static std::array<float,3> Q240Centroid(const Q240NavigationGraph& graph,const Q240Node& node) {
  return Q240Centroid(*graph.meshes[node.mesh],node.triangle);
}
template<class F>
static void Q240Neighbors(const Q240NavigationGraph& graph,const Q240Node& node,F&& visit) {
  if(!node.Valid()||node.mesh>=graph.meshes.size())return;
  const auto& mesh=*graph.meshes[node.mesh];
  if(node.triangle>=mesh.triangles.size())return;
  const auto& triangle=mesh.triangles[node.triangle];
  for(int edge=0;edge<3;++edge) {
    const int link=triangle.neighbor[edge];
    if(link<0)continue;
    if((triangle.flags&(1u<<edge))!=0u) {
      if(size_t(link)>=mesh.external.size())continue;
      const auto& external=mesh.external[size_t(link)];
      const auto target=graph.byForm.find(external.navMeshFormId);
      if(target==graph.byForm.end()||target->second>=graph.meshes.size())continue;
      if(external.triangle>=graph.meshes[target->second]->triangles.size())continue;
      visit(Q240Node{target->second,external.triangle});
    } else if(size_t(link)<mesh.triangles.size()) visit(Q240Node{node.mesh,size_t(link)});
  }
}
static std::array<float,3> Q240ClosestPoint(const Fo3NpcNavMeshQ240& mesh,size_t triangle,
                                          const std::array<float,3>& point) {
  // Closest point on the triangle, including its edges. Centroid distance is
  // not surface distance: a valid point in a large triangle can be far away.
  const auto& t=mesh.triangles[triangle];
  const auto &a=mesh.vertices[t.vertex[0]],&b=mesh.vertices[t.vertex[1]],&c=mesh.vertices[t.vertex[2]];
  auto sub=[](const auto& x,const auto& y){return std::array<float,3>{x[0]-y[0],x[1]-y[1],x[2]-y[2]};};
  auto dot=[](const auto& x,const auto& y){return x[0]*y[0]+x[1]*y[1]+x[2]*y[2];};
  auto mix=[](const auto& x,const auto& y,float f){return std::array<float,3>{x[0]+(y[0]-x[0])*f,x[1]+(y[1]-x[1])*f,x[2]+(y[2]-x[2])*f};};
  const auto ab=sub(b,a),ac=sub(c,a),ap=sub(point,a);
  const float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return a;
  const auto bp=sub(point,b);const float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return b;
  const float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return mix(a,b,d1/(d1-d3));
  const auto cp=sub(point,c);const float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return c;
  const float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return mix(a,c,d2/(d2-d6));
  const float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return mix(b,c,(d4-d3)/((d4-d3)+(d5-d6)));
  const float sum=va+vb+vc;if(std::fabs(sum)<1e-8f)return a;
  const float v=vb/sum,w=vc/sum;
  return {a[0]+ab[0]*v+ac[0]*w,a[1]+ab[1]*v+ac[1]*w,a[2]+ab[2]*v+ac[2]*w};
}
// Project vertically onto this route segment's triangle. A segment can start
// above/below NAVM; interpolating its old root height toward the next portal
// preserves that error for seconds. Ground every step on the authored surface.
static std::array<float,3> Q240GroundPoint(const Fo3NpcNavMeshQ240& mesh,size_t triangle,
                                         const std::array<float,3>& point) {
  const auto& t=mesh.triangles[triangle];
  const auto &a=mesh.vertices[t.vertex[0]],&b=mesh.vertices[t.vertex[1]],&c=mesh.vertices[t.vertex[2]];
  const float det=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1]);
  if(std::fabs(det)>1e-6f) {
    const float u=((b[1]-c[1])*(point[0]-c[0])+(c[0]-b[0])*(point[1]-c[1]))/det;
    const float v=((c[1]-a[1])*(point[0]-c[0])+(a[0]-c[0])*(point[1]-c[1]))/det;
    const float w=1-u-v;
    if(u>=-1e-5f&&v>=-1e-5f&&w>=-1e-5f)return {point[0],point[1],u*a[2]+v*b[2]+w*c[2]};
  }
  float nearest=INFINITY;std::array<float,3> out=a;
  for(int edge=0;edge<3;++edge) {
    const auto &x=mesh.vertices[t.vertex[edge]],&y=mesh.vertices[t.vertex[(edge+1)%3]];
    const float dx=y[0]-x[0],dy=y[1]-x[1],length=dx*dx+dy*dy;
    const float f=length>1e-6f?std::clamp(((point[0]-x[0])*dx+(point[1]-x[1])*dy)/length,0.f,1.f):0.f;
    std::array<float,3> q{};for(int axis=0;axis<3;++axis)q[axis]=x[axis]+(y[axis]-x[axis])*f;
    const float d=(q[0]-point[0])*(q[0]-point[0])+(q[1]-point[1])*(q[1]-point[1]);
    if(d<nearest){nearest=d;out=q;}
  }
  return out;
}
static Q240Node Q240NearestNode(const Q240NavigationGraph& graph,const std::array<float,3>& point,float* distance=nullptr) {
  Q240Node best;float nearest=INFINITY;
  for(size_t mesh=0;mesh<graph.meshes.size();++mesh)for(size_t triangle=0;triangle<graph.meshes[mesh]->triangles.size();++triangle) {
    const auto c=Q240ClosestPoint(*graph.meshes[mesh],triangle,point);
    const float dx=c[0]-point[0],dy=c[1]-point[1],dz=c[2]-point[2],d=dx*dx+dy*dy+dz*dz;
    if(d<nearest){nearest=d;best={mesh,triangle};}
  }
  if(distance)*distance=std::sqrt(nearest);
  return best;
}
// Resolve the actual common edge, rather than drawing a line between centres.
// Reject malformed external links that do not join the same physical boundary.
static bool Q240Portal(const Q240NavigationGraph& graph,const Q240Node& from,const Q240Node& to,
                       std::array<float,3>& midpoint) {
  const auto& a=*graph.meshes[from.mesh];const auto& b=*graph.meshes[to.mesh];
  const auto& ta=a.triangles[from.triangle];const auto& tb=b.triangles[to.triangle];
  // Original Megaton reciprocal NVEX links differ by up to 14.274 units
  // between cell boundary vertices. Allow that authored seam (16 game units),
  // while requiring the target's reciprocal link; internal edges stay exact.
  const float tolerance=from.mesh==to.mesh?.01f:16.f;
  auto same=[&](const auto& x,const auto& y){float d=0;for(int c=0;c<3;++c)d+=(x[c]-y[c])*(x[c]-y[c]);return d<=tolerance*tolerance;};
  for(int edge=0;edge<3;++edge) {
    const int link=ta.neighbor[edge];if(link<0)continue;
    bool connects=false;
    if(ta.flags&(1u<<edge)) {
      if(size_t(link)<a.external.size()) {
        const auto& e=a.external[link];connects=e.navMeshFormId==b.formId&&e.triangle==to.triangle;
      }
    }else connects=from.mesh==to.mesh&&size_t(link)==to.triangle;
    if(!connects)continue;
    const auto &p=a.vertices[ta.vertex[edge]],&q=a.vertices[ta.vertex[(edge+1)%3]];
    for(int targetEdge=0;targetEdge<3;++targetEdge) {
      const auto &u=b.vertices[tb.vertex[targetEdge]],&v=b.vertices[tb.vertex[(targetEdge+1)%3]];
      if(from.mesh!=to.mesh) {
        const int back=tb.neighbor[targetEdge];
        if(!(tb.flags&(1u<<targetEdge))||back<0||size_t(back)>=b.external.size())continue;
        const auto& reciprocal=b.external[back];
        if(reciprocal.navMeshFormId!=a.formId||reciprocal.triangle!=from.triangle)continue;
      }
      if((same(p,u)&&same(q,v))||(same(p,v)&&same(q,u))) {
        for(int c=0;c<3;++c)midpoint[c]=(p[c]+q[c])*.5f;
        return true;
      }
    }
  }
  return false;
}
static bool Q240Path(const Q240NavigationGraph& graph,const std::array<float,3>& from,
                     const std::array<float,3>& to,std::vector<std::array<float,3>>& path,std::string* failure=nullptr,
                     std::vector<std::pair<size_t,size_t>>* surfaces=nullptr) {
  path.clear();if(surfaces)surfaces->clear();if(failure)failure->clear();
  auto fail=[&](const char* why){if(failure)*failure=why;return false;};
  float startDistance=0,goalDistance=0;
  const Q240Node start=Q240NearestNode(graph,from,&startDistance),goal=Q240NearestNode(graph,to,&goalDistance);
  if(!start.Valid()||!goal.Valid())return fail("no NAVM triangle");
  if(startDistance>350.f)return fail("actor too far from NAVM surface");
  if(goalDistance>500.f)return fail("destination too far from NAVM surface");
  const size_t startFlat=Q240Flat(graph,start),goalFlat=Q240Flat(graph,goal),n=graph.triangleCount;
  if(startFlat>=n||goalFlat>=n)return fail("invalid NAVM graph index");
  std::vector<float> cost(n,INFINITY),score(n,INFINITY);
  std::vector<size_t> previous(n,SIZE_MAX);std::vector<uint8_t> open(n,0),closed(n,0);
  cost[startFlat]=0;score[startFlat]=Q240PlanarDistance(Q240Centroid(graph,start),Q240Centroid(graph,goal));open[startFlat]=1;
  for(size_t pass=0;pass<n;++pass) {
    size_t current=SIZE_MAX;float best=INFINITY;
    for(size_t i=0;i<n;++i)if(open[i]&&score[i]<best){best=score[i];current=i;}
    if(current==SIZE_MAX)break;if(current==goalFlat)break;open[current]=0;closed[current]=1;
    const auto currentNode=Q240NodeAt(graph,current);if(!currentNode.Valid())continue;
    const auto here=Q240Centroid(graph,currentNode);
    Q240Neighbors(graph,currentNode,[&](const Q240Node& nextNode){
      const size_t next=Q240Flat(graph,nextNode);if(next>=n||closed[next])return;
      std::array<float,3> portal{};if(!Q240Portal(graph,currentNode,nextNode,portal))return;
      const auto there=Q240Centroid(graph,nextNode);
      const float tentative=cost[current]+Q240PlanarDistance(here,there);
      if(tentative>=cost[next])return;
      previous[next]=current;cost[next]=tentative;
      score[next]=tentative+Q240PlanarDistance(there,Q240Centroid(graph,goal));open[next]=1;
    });
  }
  if(startFlat!=goalFlat&&previous[goalFlat]==SIZE_MAX)return fail("no connected portal route");
  std::vector<size_t> reverse;
  for(size_t at=goalFlat;at!=startFlat&&at!=SIZE_MAX;at=previous[at])reverse.push_back(at);
  std::reverse(reverse.begin(),reverse.end());
  auto prior=start;
  for(size_t flat:reverse) {
    const auto node=Q240NodeAt(graph,flat);std::array<float,3> portal{};
    if(!node.Valid()||!Q240Portal(graph,prior,node,portal)){path.clear();return fail("invalid shared NAVM portal");}
    path.push_back(portal);if(surfaces)surfaces->push_back({prior.mesh,prior.triangle});prior=node;
  }
  // Preserve the requested destination inside the terminal triangle. Travel
  // must not declare completion just because it reached that triangle's centre.
  path.push_back(Q240GroundPoint(*graph.meshes[goal.mesh],goal.triangle,to));
  if(surfaces)surfaces->push_back({goal.mesh,goal.triangle});
  return true;
}
static bool Q240ScheduleActive(const fo3pipdata::PackageSchedule& schedule,float hour) {
  if(!schedule.valid)return true;
  if(schedule.month!=-1||schedule.weekday!=-1||schedule.date!=0)return false;
  if(schedule.hour<0)return true;
  if(schedule.duration<=0)return false;
  const float start=float(schedule.hour),end=start+float(schedule.duration);
  hour=fo3tod::WrapHour(hour);
  if(end<=24.f)return hour>=start&&hour<end;
  return hour>=start||hour<end-24.f;
}
static const fo3pipdata::PackageDefinition* Q240SelectPackage(
    Q230ActorVisual& actor,uint32_t& id,std::array<float,3>& anchor,float& radius) {
  id=0;radius=0;if(!gPlayerSession)return nullptr;
  const auto& definitions=gPlayerSession->player.Definitions().pipboy;
  const auto actorDef=definitions.dialogueActors.find(actor.source.baseFormId);
  if(actorDef==definitions.dialogueActors.end())return nullptr;
  const float hour=GetFo3TimeOfDayHour();
  for(uint32_t packageId:actorDef->second.packages) {
    const auto found=definitions.packages.find(packageId);if(found==definitions.packages.end())continue;
    const auto& package=found->second;
    if(package.type!=5&&package.type!=6&&package.type!=12){Q240AiLogOnce(actor.source.refFormId,packageId,"package type "+std::to_string(package.type));continue;}
    if(package.scripted){Q240AiLogOnce(actor.source.refFormId,packageId,"package script");continue;}
    if(!Q240ScheduleActive(package.schedule,hour))continue;
    if(!package.location.valid){Q240AiLogOnce(actor.source.refFormId,packageId,"missing PLDT");continue;}
    fo3dialogue::Context ctx;ctx.player=&gPlayerSession->player;
    const auto game=Q240GamePosition(actor);
    ctx.speaker={actor.source.refFormId,actor.source.baseFormId,gCurrentCellFormId,gExteriorWorldspaceQ1890,
      actor.runtime.package,game[0],game[1],game[2],actor.runtime.activity==fo3npc::Activity::Combat,actor.runtime.packageKnown};
    ctx.target={0x14,7,gCurrentCellFormId,gExteriorWorldspaceQ1890,0,
      gSceneCenterXQ1730+gQ210Head[0]*FO3_UNITS_PER_METRE,
      gSceneCenterYQ1730-(gQ210Head[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
      gSceneFloorZQ1730+(gQ210Head[1]-FLOOR_Y)*FO3_UNITS_PER_METRE,false,false};
    std::string error;
    if(!fo3dialogue::Conditions(package.conditions,ctx,error)) {
      if(!error.empty())Q240AiLogOnce(actor.source.refFormId,packageId,error);
      continue;
    }
    if(package.location.type==0u) {
      const auto target=definitions.targets.find(package.location.value);
      if(target==definitions.targets.end()){Q240AiLogOnce(actor.source.refFormId,packageId,"location reference unavailable");continue;}
      if(target->second.world!=gExteriorWorldspaceQ1890||
         (!gExteriorWorldspaceQ1890&&target->second.cell!=gCurrentCellFormId)) {
        Q240AiLogOnce(actor.source.refFormId,packageId,"destination requires another scene/cell");continue;
      }
      anchor={target->second.x,target->second.y,target->second.z};
    } else if(package.location.type==2u) anchor=game;
    else if(package.location.type==3u) anchor={actor.source.x,actor.source.y,actor.source.z};
    else {Q240AiLogOnce(actor.source.refFormId,packageId,"PLDT type "+std::to_string(package.location.type));continue;}
    radius=float(std::max(0,package.location.radius));id=packageId;return &package;
  }
  return nullptr;
}
static bool Q240BuildPackagePath(Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& package,
                                 const std::array<float,3>& anchor,float radius) {
  actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;if(!actor.navigationGraph||actor.navigationGraph->triangleCount==0)return false;
  const auto& graph=*actor.navigationGraph;
  const auto current=Q240GamePosition(actor);
  std::array<float,3> destination=anchor;
  if(package.type==5||package.type==12) {
    // Fallout's low NVTR flag bits mark external edges; their neighbor values
    // index NVEX, which points at a triangle in another NAVM. Traverse those
    // authored links so Wander/Sandbox can continue across the town graph.
    float startDistance=0;const auto start=Q240NearestNode(graph,current,&startDistance);
    if(!start.Valid()||startDistance>350.f) {
      Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"actor off NAVM surface distance="+std::to_string(startDistance));return false;
    }
    const size_t n=graph.triangleCount,startFlat=Q240Flat(graph,start);
    std::vector<uint8_t> reachable(n,0);std::vector<size_t> queue;
    queue.reserve(n);queue.push_back(startFlat);reachable[startFlat]=1;
    for(size_t at=0;at<queue.size();++at) {
      const auto node=Q240NodeAt(graph,queue[at]);if(!node.Valid())continue;
      Q240Neighbors(graph,node,[&](const Q240Node& nextNode){
        const size_t next=Q240Flat(graph,nextNode);
        std::array<float,3> portal{};
        if(next<n&&!reachable[next]&&Q240Portal(graph,node,nextNode,portal)){reachable[next]=1;queue.push_back(next);}
      });
    }
    std::vector<std::array<float,3>> choices;choices.reserve(queue.size());
    uint32_t hash=actor.source.refFormId*2654435761u ^ actor.aiPackage*2246822519u ^ (++actor.aiSequence)*3266489917u;
    auto random=[&](){hash=hash*1664525u+1013904223u;return float(hash>>8)/16777216.f;};
    for(size_t flat:queue) {
      const auto node=Q240NodeAt(graph,flat);if(!node.Valid())continue;
      const auto& mesh=*graph.meshes[node.mesh];const auto& tri=mesh.triangles[node.triangle];
      auto candidate=Q240GroundPoint(mesh,node.triangle,anchor);
      if(Q240PlanarDistance(candidate,anchor)>radius)continue;
      // Include a large current triangle and the portion of boundary triangles
      // inside PLDT. Centroid-only selection can exhaust a perfectly walkable area.
      for(int attempt=0;attempt<16;++attempt) {
        float u=random(),v=random();if(u+v>1){u=1-u;v=1-v;}
        std::array<float,3> point{};
        for(int axis=0;axis<3;++axis)point[axis]=mesh.vertices[tri.vertex[0]][axis]*(1-u-v)+mesh.vertices[tri.vertex[1]][axis]*u+mesh.vertices[tri.vertex[2]][axis]*v;
        if(Q240PlanarDistance(point,anchor)<=radius&&Q240PlanarDistance(point,current)>16.f){candidate=point;break;}
      }
      if(Q240PlanarDistance(candidate,current)>16.f)choices.push_back(candidate);
    }
    if(choices.empty()) {Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"no reachable destination in PLDT radius="+std::to_string(radius));return false;}
    destination=choices[hash%choices.size()];
  }
  std::string failure;
  const bool success=Q240Path(graph,current,destination,actor.aiPathGame,&failure,&actor.aiPathSurfaces);
  if(!success)Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,failure);
  if(!success||actor.aiSequence<=2u||(actor.aiSequence%8u)==0u) {
    const auto start=Q240NearestNode(graph,current),goal=Q240NearestNode(graph,destination);
    Q6H_LOGI("NPC ROUTE actor=%08X package=%08X from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f) startNav=%08X startTri=%zu goalNav=%08X goalTri=%zu waypoints=%zu success=%d reason=%s",
      actor.source.refFormId,actor.aiPackage,current[0],current[1],current[2],destination[0],destination[1],destination[2],
      start.Valid()?graph.meshes[start.mesh]->formId:0u,start.triangle,goal.Valid()?graph.meshes[goal.mesh]->formId:0u,goal.triangle,
      actor.aiPathGame.size(),success,failure.c_str());
  }
  return success;
}
static void Q240AdvanceNpcPackage(Q230ActorVisual& actor,double now) {
  if(!gPlayerSession||actor.rigs.empty()||!actor.navigationGraph||actor.navigationGraph->triangleCount==0||actor.runtime.dialogue||
     actor.runtime.activity==fo3npc::Activity::Combat||
     gPlayerSession->player.ActorHealth(actor.source.refFormId)<=0)return;
  float dt=0;if(actor.aiLastUpdate>=0)dt=std::clamp(float(now-actor.aiLastUpdate),0.f,.10f);
  actor.aiLastUpdate=now;
  uint32_t packageId=0;std::array<float,3> anchor{};float radius=0;
  const auto* package=Q240SelectPackage(actor,packageId,anchor,radius);
  if(!package) {
    actor.aiPackage=actor.runtime.package=0;actor.runtime.packageKnown=false;
    actor.runtime.activity=fo3npc::Activity::Idle;actor.runtime.speed=0;
    actor.runtime.animation=fo3npc::Animation::Idle;actor.aiPathGame.clear();return;
  }
  if(packageId!=actor.aiPackage) {
    actor.aiPackage=packageId;actor.aiAnchorGame=anchor;actor.aiSequence=0;actor.aiPathGame.clear();actor.aiPathIndex=0;actor.aiRepathAt=0;
    Q6H_LOGI("NPC PACKAGE actor=%08X package=%08X EDID=%s type=%u locationType=%u radius=%d hour=%.2f",
      actor.source.refFormId,packageId,package->editor.c_str(),unsigned(package->type),
      package->location.type,package->location.radius,GetFo3TimeOfDayHour());
  }
  if(package->location.type==2u)anchor=actor.aiAnchorGame;
  actor.runtime.package=packageId;actor.runtime.packageKnown=true;actor.runtime.activity=fo3npc::Activity::Package;
  if(actor.aiPathIndex>=actor.aiPathGame.size()) {
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;
    // Travel builds one authored route when the package becomes active, then
    // remains at the destination for the rest of that package. Wander/Sandbox
    // keep selecting new NAVM destinations after their short pause.
    if(package->type==6&&actor.aiSequence>0)return;
    if(now<actor.aiRepathAt)return;
    if(!Q240BuildPackagePath(actor,*package,anchor,radius)){
      Q240AiLogOnce(actor.source.refFormId,packageId,
        "path build failed on authored NAVM graph meshes="+std::to_string(actor.navigationGraph->meshes.size())+
        " triangles="+std::to_string(actor.navigationGraph->triangleCount));
      actor.aiRepathAt=now+2;return;
    }
    if(package->type==6)actor.aiSequence=1;
    if(actor.aiSequence<=2u||(actor.aiSequence%8u)==0u)
      Q6H_LOGI("NPC PATH actor=%08X package=%08X type=%u waypoints=%zu graphMeshes=%zu graphTriangles=%zu sequence=%u",
        actor.source.refFormId,packageId,unsigned(package->type),actor.aiPathGame.size(),
        actor.navigationGraph->meshes.size(),actor.navigationGraph->triangleCount,actor.aiSequence);
    actor.aiRepathAt=now+1;
  }
  if(dt<=0||actor.aiPathIndex>=actor.aiPathGame.size())return;
  const auto targetScene=Q240ScenePosition(actor.aiPathGame[actor.aiPathIndex]);
  actor.runtime.destination=targetScene;
  const float dx=targetScene[0]-actor.runtime.position[0],dy=targetScene[1]-actor.runtime.position[1],
              dz=targetScene[2]-actor.runtime.position[2];
  const float distance=std::sqrt(dx*dx+dz*dz);
  if(distance<.08f) {
    actor.runtime.position=targetScene;++actor.aiPathIndex;actor.runtime.speed=0;
    actor.runtime.animation=fo3npc::Animation::Idle;actor.aiRepathAt=now+.35;return;
  }
  const float desired=std::atan2(-dx,-dz),error=fo3npc::Angle(desired-actor.runtime.yaw);
  actor.runtime.yaw=fo3npc::Angle(actor.runtime.yaw+
    std::clamp(error,-fo3npc::DialoguePolicy::BodySpeed*dt,fo3npc::DialoguePolicy::BodySpeed*dt));
  actor.runtime.returnYaw=actor.runtime.yaw;
  actor.runtime.headYaw+=std::clamp(-actor.runtime.headYaw,
    -fo3npc::DialoguePolicy::HeadSpeed*dt,fo3npc::DialoguePolicy::HeadSpeed*dt);
  if(std::fabs(error)>.70f) {
    actor.runtime.speed=0;
    actor.runtime.animation=error>0?fo3npc::Animation::TurnLeft:fo3npc::Animation::TurnRight;return;
  }
  // Fallout's movement-speed actor values are not yet decoded. 1.05 m/s is an
  // explicit standalone-VR locomotion bridge; path, schedule, radius and clip
  // remain authored data.
  constexpr float WALK_METRES_PER_SECOND=1.05f;
  const float step=std::min(distance,WALK_METRES_PER_SECOND*dt),fraction=step/std::max(distance,.001f);
  actor.runtime.position[0]+=dx*fraction;actor.runtime.position[2]+=dz*fraction;
  actor.runtime.position[1]+=dy*fraction;
  if(actor.aiPathIndex<actor.aiPathSurfaces.size()) {
    const auto surface=actor.aiPathSurfaces[actor.aiPathIndex];
    actor.runtime.position=Q240ScenePosition(Q240GroundPoint(*actor.navigationGraph->meshes[surface.first],surface.second,Q240GamePosition(actor)));
  }
  actor.runtime.speed=WALK_METRES_PER_SECOND;
  actor.runtime.animation=fo3npc::Animation::Walk;
}
void Q240UpdateNpcPackage(Q230ActorVisual& actor,double now) {
  const auto position=actor.runtime.position;
  const auto yaw=actor.runtime.yaw;
  const auto animation=actor.runtime.animation;
  Q240AdvanceNpcPackage(actor,now);
  // Interaction bounds can sample the skin earlier in this stereo frame.
  // A changed package root/clip must still be submitted before world drawing.
  if(position!=actor.runtime.position||yaw!=actor.runtime.yaw||animation!=actor.runtime.animation)
    actor.lastFrame=UINT64_MAX;
}
