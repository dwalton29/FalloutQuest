#include <limits>
#include "fo3-package-schedule.h"
#include <queue>
#include <functional>
static std::unordered_set<std::string> gQ240AiDiagnostics;
// Frame diagnostics only; the actual topology and algorithm remain pure.
static uint32_t gQ240PathsPlanned=0,gQ240PathNodesExpanded=0;
static void Q240AiLogOnce(uint32_t actor,uint32_t package,const std::string& why) {
  char prefix[40];std::snprintf(prefix,sizeof(prefix),"%08X:%08X:",actor,package);
  const std::string key=std::string(prefix)+why;
  if(gQ240AiDiagnostics.insert(key).second)
    Q6H_LOGI("NPC PACKAGE UNSUPPORTED actor=%08X package=%08X reason=%s",actor,package,why.c_str());
}
static void Q240AccompanyOnce(uint32_t actor,uint32_t package,uint32_t target,const char* event,const std::string& reason) {
  const std::string key="Accompany:"+std::to_string(actor)+":"+std::to_string(package)+":"+event+":"+reason;
  if(gQ240AiDiagnostics.insert(key).second)
    Q6H_LOGI("NPC ACCOMPANY %s actor=%08X package=%08X target=%08X reason=%s",event,actor,package,target,reason.c_str());
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
                       std::array<float,3>& midpoint,
                       std::array<std::array<float,3>,2>* endpoints=nullptr) {
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
        if(endpoints){(*endpoints)[0]=p;(*endpoints)[1]=q;}
        return true;
      }
    }
  }
  return false;
}
static bool Q240Path(const Q240NavigationGraph& graph,const std::array<float,3>& from,
                     const std::array<float,3>& to,std::vector<std::array<float,3>>& path,std::string* failure=nullptr,
                     std::vector<std::pair<size_t,size_t>>* surfaces=nullptr) {
  ++gQ240PathsPlanned;
  path.clear();if(surfaces)surfaces->clear();if(failure)failure->clear();
  auto fail=[&](const char* why){if(failure)*failure=why;return false;};
  float startDistance=0,goalDistance=0;
  const Q240Node start=Q240NearestNode(graph,from,&startDistance),goal=Q240NearestNode(graph,to,&goalDistance);
  if(!start.Valid()||!goal.Valid())return fail("no NAVM triangle");
  if(startDistance>350.f)return fail("actor too far from NAVM surface");
  if(goalDistance>500.f)return fail("destination too far from NAVM surface");
  const size_t startFlat=Q240Flat(graph,start),goalFlat=Q240Flat(graph,goal),n=graph.triangleCount;
  if(startFlat>=n||goalFlat>=n)return fail("invalid NAVM graph index");
  // The original full-frontier scan visited every NAVM triangle for each A*
  // expansion (quadratic work on Megaton's connected meshes). Keep the same
  // centroids, portal validation, costs and endpoint, but use a min-heap.
  using FrontierEntry=std::pair<float,size_t>;
  std::priority_queue<FrontierEntry,std::vector<FrontierEntry>,std::greater<FrontierEntry>> frontier;
  std::vector<float> cost(n,INFINITY),score(n,INFINITY);
  std::vector<size_t> previous(n,SIZE_MAX);std::vector<uint8_t> closed(n,0);
  cost[startFlat]=0;score[startFlat]=Q240PlanarDistance(Q240Centroid(graph,start),Q240Centroid(graph,goal));
  frontier.emplace(score[startFlat],startFlat);
  while(!frontier.empty()) {
    const auto [priority,current]=frontier.top();frontier.pop();
    if(closed[current]||priority>score[current])continue; // stale queued priority
    if(current==goalFlat)break;
    ++gQ240PathNodesExpanded;
    closed[current]=1;
    const auto currentNode=Q240NodeAt(graph,current);if(!currentNode.Valid())continue;
    const auto here=Q240Centroid(graph,currentNode);
    Q240Neighbors(graph,currentNode,[&](const Q240Node& nextNode){
      const size_t next=Q240Flat(graph,nextNode);if(next>=n||closed[next])return;
      std::array<float,3> portal{};if(!Q240Portal(graph,currentNode,nextNode,portal))return;
      const auto there=Q240Centroid(graph,nextNode);
      const float tentative=cost[current]+Q240PlanarDistance(here,there);
      if(tentative>=cost[next])return;
      previous[next]=current;cost[next]=tentative;
      score[next]=tentative+Q240PlanarDistance(there,Q240Centroid(graph,goal));
      frontier.emplace(score[next],next);
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
// Shared bounded escape endpoint search. Only reciprocal authored NAVM portals
// are visited. For a package, prefer a nearby surface that meets its actual
// PTDT distance; if none exists, retain the farthest reachable improvement.
// Combat keeps its existing farthest-endpoint policy (minimum = infinity).
static bool Q240FleeDestination(const Q240NavigationGraph& graph,const std::array<float,3>& current,
                                const std::array<float,3>& threat,float minimum,std::array<float,3>& destination) {
  float surfaceDistance=0;const auto start=Q240NearestNode(graph,current,&surfaceDistance);
  if(!start.Valid()||surfaceDistance>350.f)return false;
  destination=current;float farthest=Q240PlanarDistance(current,threat),nearestSafe=INFINITY;
  std::vector<uint8_t> seen(graph.triangleCount,0);std::vector<Q240Node> nodes{start};seen[Q240Flat(graph,start)]=1;
  for(size_t at=0;at<nodes.size();++at) {
    const auto point=Q240Centroid(graph,nodes[at]);const float separation=Q240PlanarDistance(point,threat);
    const float travel=Q240PlanarDistance(current,point);
    if(separation>=minimum&&travel<nearestSafe){nearestSafe=travel;destination=point;}
    else if(!std::isfinite(nearestSafe)&&separation>farthest){farthest=separation;destination=point;}
    Q240Neighbors(graph,nodes[at],[&](Q240Node next){const auto flat=Q240Flat(graph,next);std::array<float,3> portal{};
      if(flat<seen.size()&&!seen[flat]&&Q240Portal(graph,nodes[at],next,portal)){seen[flat]=1;nodes.push_back(next);}
    });
  }
  return destination!=current;
}
template<class ResidentActors>
static bool Q240ResidentPackageTarget(const ResidentActors& actors,uint32_t reference,std::array<float,3>& point) {
  for(const auto& target:actors)if(target.source.refFormId==reference&&target.runtime.Alive()&&!target.rigs.empty()) {
    const auto& saved=gPlayerSession->player.Snapshot().actors;const auto state=saved.find(reference);
    if(state!=saved.end()&&(state->second.world!=gExteriorWorldspaceQ1890||(!gExteriorWorldspaceQ1890&&state->second.cell!=gCurrentCellFormId)))return false;
    // A follower can update before its leader's first simulation this frame.
    // Restore location from canonical state until that resident root is ready.
    point=!target.stateRestored&&state!=saved.end()?state->second.position:Q240GamePosition(target);return true;
  }
  return false;
}
// Accompany knows a resident target's current package route destination,
// whereas Follow resolves only its current root. Never predict player intent.
template<class ResidentActors>
static bool Q240ResidentAccompanyTarget(const ResidentActors& actors,uint32_t reference,std::array<float,3>& goal) {
  std::array<float,3> current{};if(!Q240ResidentPackageTarget(actors,reference,current))return false;
  for(const auto& target:actors)if(target.source.refFormId==reference){
    goal=target.runtime.activity==fo3npc::Activity::Package&&target.aiPathIndex<target.aiPathGame.size()?target.aiPathGame.back():current;
    return std::all_of(goal.begin(),goal.end(),[](float c){return std::isfinite(c);});
  }return false;
}
static bool Q240PackageActorTarget(const Q230ActorVisual& actor,uint32_t reference,std::array<float,3>& point) {
  if(!gPlayerSession||reference==actor.source.refFormId||gPlayerSession->player.ActorHealth(reference)<=0)return false;
  if(reference==0x14)point={gSceneCenterXQ1730+gQ210Head[0]*FO3_UNITS_PER_METRE,
    gSceneCenterYQ1730-(gQ210Head[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
    gSceneFloorZQ1730+(gQ210Head[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
  else {
#ifdef FO3_NPC_PACKAGE_ACTOR_TARGET
    if(!FO3_NPC_PACKAGE_ACTOR_TARGET(reference,point))return false;
#else
    return false; // No resident runtime adapter; never use an authored spawn.
#endif
  }
  return std::all_of(point.begin(),point.end(),[](float coordinate){return std::isfinite(coordinate);});
}
static bool Q240AccompanyDestination(const Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& p,std::array<float,3>& current,std::array<float,3>& goal) {
  if(!Q240PackageActorTarget(actor,p.target.value,current)||p.target.value==0x14)return false;
#ifdef FO3_NPC_ACCOMPANY_TARGET
  return FO3_NPC_ACCOMPANY_TARGET(p.target.value,goal);
#else
  (void)goal;return false;
#endif
}
static bool Q240ScheduleActive(const fo3pipdata::PackageSchedule& schedule,float hour) {
  fo3schedule::Calendar calendar{};
  if(gPlayerSession) {
    const auto& state=gPlayerSession->player.Snapshot();
    calendar={int(state.gameYear),int(state.gameMonth),int(state.gameDay)};
  }
  return fo3schedule::Active(schedule,hour,calendar);
}
static const std::vector<fo3pipdata::PatrolPoint>* Q240PatrolRoute(const Q230ActorVisual& actor,uint32_t id,const fo3pipdata::PackageDefinition& package,bool* circular=nullptr,std::string* error=nullptr) {
  if(package.location.type!=6){if(circular)*circular=package.patrolCircular;if(error)*error=package.patrolUnsupported;return &package.patrol;}
  const auto& d=gPlayerSession->player.Definitions().pipboy;const uint64_t key=(uint64_t(actor.source.refFormId)<<32)|id;
  const auto i=d.actorPatrols.find(key);if(circular)*circular=d.actorPatrolCircular.count(key)!=0;
  if(error){const auto e=d.actorPatrolUnsupported.find(key);*error=e==d.actorPatrolUnsupported.end()?"":e->second;}
  return i==d.actorPatrols.end()?nullptr:&i->second;
}
// Resolve only original resident furniture. A missing or contested bed yields
// to lower-priority authored packages through the existing retry mechanism.
static size_t Q240FurnitureTarget(const Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& p) {
  const auto& f=actor.runtime.furniture;if(!f.scene)return SIZE_MAX;
  const auto& d=gPlayerSession->player.Definitions().pipboy;
  const auto* def=fo3pipdata::ActorCategory(d,actor.source.baseFormId,1);
  if(def&&d.childRaces.count(def->race))return SIZE_MAX; // Child IDLE branch requires its own authored program.
  const auto here=Q240GamePosition(actor);std::array<float,3> anchor=here;bool exact=false;
  if(p.location.type==0){auto t=d.targets.find(p.location.value);if(t==d.targets.end())return SIZE_MAX;anchor={t->second.x,t->second.y,t->second.z};exact=d.furniture.count(t->second.base)!=0;}
  else if(p.location.type==1){if(p.location.value!=gCurrentCellFormId)return SIZE_MAX;}
  else if(p.location.type==3)anchor={actor.source.x,actor.source.y,actor.source.z};
  else return SIZE_MAX;
  size_t best=SIZE_MAX;float distance=INFINITY;
  for(size_t i=0;i<f.scene->slots.size();++i){const auto& slot=f.scene->slots[i];if(slot.idleMarker||slot.sleep!=(p.type==4))continue;
    if(exact&&slot.reference!=p.location.value)continue;
    if(!exact&&p.location.type!=1&&Q240PlanarDistance(anchor,{slot.placement.x,slot.placement.y,slot.placement.z})>std::max(0,p.location.radius))continue;
    if(slot.placement.owner&&slot.placement.owner!=actor.source.baseFormId&&(!def||!def->factions.count(slot.placement.owner)))continue;
    const uint64_t key=uint64_t(slot.reference)<<32;const auto owner=f.scene->reservations->owners.find(key);
    if(owner!=f.scene->reservations->owners.end()&&owner->second!=actor.source.refFormId)continue;
    const float candidate=Q240PlanarDistance(here,{slot.alignment[12],slot.alignment[13],slot.alignment[14]});if(candidate<distance){distance=candidate;best=i;}
  }return best;
}
// Canonical CTDA evaluation includes every parent IDLE gate. Unsupported
// conditions exclude that animation rather than treating it as unconditional.
static bool Q240MarkerIdleEligible(const Q230ActorVisual& actor,uint32_t idle) {
  const auto& d=gPlayerSession->player.Definitions().pipboy;
  fo3dialogue::Context ctx;ctx.player=&gPlayerSession->player;const auto game=Q240GamePosition(actor);
  ctx.speaker={actor.source.refFormId,actor.source.baseFormId,gCurrentCellFormId,gExteriorWorldspaceQ1890,actor.aiPackage,game[0],game[1],game[2],false,true};
  std::unordered_set<uint32_t> seen;
  while(idle){if(!seen.insert(idle).second||seen.size()>64)return false;
    auto c=d.idleConditions.find(idle);std::string error;
    if(c!=d.idleConditions.end()&&(!fo3dialogue::Conditions(c->second,ctx,error)||!error.empty()))return false;
    const auto parent=d.idleParents.find(idle);idle=parent==d.idleParents.end()?0:parent->second;
  }return true;
}
static size_t Q240SandboxTarget(const Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& p,const std::array<float,3>& anchor,float radius) {
  const auto& f=actor.runtime.furniture;if(!f.scene)return SIZE_MAX;
  const auto& d=gPlayerSession->player.Definitions().pipboy;const auto* def=fo3pipdata::ActorCategory(d,actor.source.baseFormId,1);
  std::vector<size_t> choices;
  for(size_t i=0;i<f.scene->slots.size();++i){const auto& slot=f.scene->slots[i];
    if(slot.sleep||(slot.idleMarker?(p.typeFlags&8u)||(p.flags&0x01000000u):(p.typeFlags&16u)))continue;
    if(!slot.idleMarker&&def&&d.childRaces.count(def->race))continue;
    if(Q240PlanarDistance(anchor,{slot.placement.x,slot.placement.y,slot.placement.z})>radius)continue;
    if(slot.placement.owner&&slot.placement.owner!=actor.source.baseFormId&&(!def||!def->factions.count(slot.placement.owner)))continue;
    const auto owner=f.scene->reservations->owners.find(uint64_t(slot.reference)<<32);
    if(owner!=f.scene->reservations->owners.end()&&owner->second!=actor.source.refFormId)continue;
    if(slot.idleMarker&&!std::any_of(slot.idles.begin(),slot.idles.end(),[&](const auto& idle){return Q240MarkerIdleEligible(actor,idle.form);}))continue;
    choices.push_back(i);
  }
  if(choices.size()>1)choices.erase(std::remove_if(choices.begin(),choices.end(),[&](size_t i){return f.scene->slots[i].reference==actor.runtime.sandboxLastTarget;}),choices.end());
  if(choices.empty())return SIZE_MAX;
  const uint32_t hash=actor.source.refFormId*2654435761u^actor.aiPackage*2246822519u^actor.runtime.sandboxCycles*3266489917u;
  // Bound route probes on the existing activity-selection cadence. A bad marker
  // cannot monopolise selection or strand actors that have other activities.
  for(size_t attempt=0;attempt<std::min(size_t(8),choices.size());++attempt){const size_t i=choices[(hash+attempt)%choices.size()];const auto& a=f.scene->slots[i].alignment;
    std::vector<std::array<float,3>> route;
    if(actor.navigationGraph&&Q240Path(*actor.navigationGraph,Q240GamePosition(actor),{a[12],a[13],a[14]},route)&&!route.empty()&&Q240PlanarDistance(route.back(),{a[12],a[13],a[14]})<=8.f)return i;
  }return SIZE_MAX;
}
// Use the immutable v185 graph to find an accessible original load door
// toward a remote PLDT reference. NPCs never call the player XTEL queue.
static bool Q240NpcRemoteDoor(const Q230ActorVisual& actor,uint32_t targetCell,
    uint32_t& doorRef,std::array<float,3>& doorPosition){
#ifndef FO3_NPC_XTEL_INDEX
  (void)actor;(void)targetCell;(void)doorRef;(void)doorPosition;return false;
#else
  const auto index=FO3_NPC_XTEL_INDEX();
  if(!index||!gPlayerSession||!actor.navigationGraph||!targetCell)return false;
  const auto& d=gPlayerSession->player.Definitions().pipboy;
  std::vector<uint32_t> doors;
  if(gExteriorWorldspaceQ1890==0x00000A74u){
    // The Megaton scene is loaded via its persistent WRLD CELL, but authored
    // XTEL doors actually belong to individual exterior grid CELLs.
    for(const auto& cell:d.cellWorlds)if(cell.second==gExteriorWorldspaceQ1890){
      if(const auto* exits=index->Outgoing(cell.first))
        doors.insert(doors.end(),exits->begin(),exits->end());
    }
  } else if(const auto* exits=index->Outgoing(gCurrentCellFormId)){
    doors=*exits;
  }
  if(doors.empty())return false;
  const auto here=Q240GamePosition(actor);
  size_t minLegs=SIZE_MAX;float minDistance=INFINITY;
  for(uint32_t ref:doors){
    const auto* edge=index->Find(ref);
    if(!edge||edge->sourceWorld!=gExteriorWorldspaceQ1890||
       (edge->sourceRecordFlags&0x820u)||(edge->destinationRecordFlags&0x820u))
      continue;
    const auto placement=d.targets.find(ref);
    if(placement==d.targets.end()||(placement->second.flags&0x820u)||
       placement->second.parent||!gPlayerSession->player.CanActorOpenDoor(actor.source.refFormId,ref))
      continue;
    std::vector<uint32_t> remainder;
    if(!index->CellRoute(edge->destinationCell,targetCell,remainder))continue;
    std::array<float,3> point{placement->second.x,placement->second.y,placement->second.z};
    std::vector<std::array<float,3>> route;
    if(!Q240Path(*actor.navigationGraph,here,point,route)||route.empty()||
       Q240PlanarDistance(route.back(),point)>96.f)continue;
    const float distance=Q240PlanarDistance(here,point);
    if(remainder.size()+1<minLegs||(remainder.size()+1==minLegs&&distance<minDistance)){
      minLegs=remainder.size()+1;minDistance=distance;doorRef=ref;doorPosition=point;
    }
  }
  return doorRef!=0;
#endif
}
static const fo3pipdata::PackageDefinition* Q240SelectPackage(
    Q230ActorVisual& actor,uint32_t& id,std::array<float,3>& anchor,float& radius) {
  id=0;radius=0;actor.runtime.xtelDoor=0;actor.runtime.xtelCell=0;
  if(!gPlayerSession)return nullptr;
  const auto& definitions=gPlayerSession->player.Definitions().pipboy;
  const auto* actorDef=fo3pipdata::ActorCategory(definitions,actor.source.baseFormId,16);
  if(!actorDef){Q240AiLogOnce(actor.source.refFormId,0,"invalid AI package template");return nullptr;}
  const float hour=GetFo3TimeOfDayHour();
  for(uint32_t packageId:actorDef->packages) {
    const auto retry=actor.runtime.packageRetryAfter.find(packageId);
    if(retry!=actor.runtime.packageRetryAfter.end()&&actor.aiLastUpdate<retry->second)continue;
    const auto found=definitions.packages.find(packageId);if(found==definitions.packages.end())continue;
    const auto& package=found->second;
    if(package.type!=7&&package.type!=3&&package.type!=4&&package.type!=1&&package.type!=2&&package.type!=6&&package.type!=14&&package.type!=13&&package.type!=5&&package.type!=12&&package.type!=10&&package.type!=8&&package.type!=15){Q240AiLogOnce(actor.source.refFormId,packageId,"package type "+std::to_string(package.type));continue;}
    if(package.type==13){std::string error;const auto* route=Q240PatrolRoute(actor,packageId,package,nullptr,&error);if(!route||route->empty()||!error.empty()){Q240AiLogOnce(actor.source.refFormId,packageId,error.empty()?"Patrol route unavailable":error);continue;}}
    if(package.type==2&&((package.location.type!=0&&package.location.type!=3)||!package.escortDistanceValid||!package.target.valid||package.target.type!=0||package.target.value!=0x14)){
      Q240AiLogOnce(actor.source.refFormId,packageId,"Escort requires authored distance and supported player reference target");continue;
    }
    if(package.type==7&&(package.location.valid||package.location2.valid||!package.target.valid||package.target.type!=0||package.target.radius<=0||package.target2.valid||package.typeFlags||package.procedureActions)){
      Q240AccompanyOnce(actor.source.refFormId,packageId,package.target.value,"UNSUPPORTED","requires one resident actor reference and positive distance without location/action");continue;
    }
    // Verified Use Item At subset: a placed TERM with Sit Down, plus a
    // resident authored chair. Other item/activator semantics remain unsupported.
    if(package.type==8&&(!package.location.valid||package.location.type!=0||package.location2.valid||
       !package.target.valid||package.target.type!=0||package.target2.valid||
       package.typeFlags!=2||package.procedureActions||package.otherProcedureScript||
       (package.scripted&&package.onBeginScript.empty()))){
      Q240AiLogOnce(actor.source.refFormId,packageId,"Use Item At requires original seated reference target with supported OnBegin only");continue;
    }
    if(package.type==10&&(package.location.valid||!package.target.valid||package.target.type!=0||package.target.radius<=0||package.procedureActions)){
      Q240AiLogOnce(actor.source.refFormId,packageId,"Flee requires an actor reference, positive minimum distance, no start location or procedure action");continue;
    }
    if(package.type==15&&(!package.target.valid||package.target.type!=0||package.target.value==0x14||!package.target.value||package.target2.valid||package.location2.valid||package.procedureActions||package.otherProcedureScript)){
      Q240AiLogOnce(actor.source.refFormId,packageId,"Dialogue AI requires one real actor target and no unsupported procedure actions");continue;
    }
    if(!package.combatStyleValid){Q240AiLogOnce(actor.source.refFormId,packageId,"malformed package combat style");continue;}
    if(package.scripted&&package.type!=8){Q240AiLogOnce(actor.source.refFormId,packageId,"package script");continue;}
    if(!Q240ScheduleActive(package.schedule,hour))continue;
    if(!package.location.valid&&package.type!=1&&package.type!=7&&package.type!=10&&package.type!=15){Q240AiLogOnce(actor.source.refFormId,packageId,"missing PLDT");continue;}
    fo3dialogue::Context ctx;ctx.player=&gPlayerSession->player;
    const auto game=Q240GamePosition(actor);
    ctx.speaker={actor.source.refFormId,actor.source.baseFormId,gCurrentCellFormId,gExteriorWorldspaceQ1890,
      actor.runtime.package,game[0],game[1],game[2],actor.runtime.activity==fo3npc::Activity::Combat,actor.runtime.packageKnown};
    ctx.target={0x14,7,gCurrentCellFormId,gExteriorWorldspaceQ1890,0,
      gSceneCenterXQ1730+gQ210Head[0]*FO3_UNITS_PER_METRE,
      gSceneCenterYQ1730-(gQ210Head[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
      gSceneFloorZQ1730+(gQ210Head[1]-FLOOR_Y)*FO3_UNITS_PER_METRE,false,false};
    // For actor-to-actor packages the actual interlocutor, NOT the player,
    // is the CTDA target. Never accidentally satisfy run-on-target quests
    // using the player as a substitute.
    if(package.type==15){
      const auto placement=definitions.targets.find(package.target.value);
      std::array<float,3> listener{};
      if(placement==definitions.targets.end()||
         !Q240PackageActorTarget(actor,package.target.value,listener)||
         placement->second.base==0){
        Q240AiLogOnce(actor.source.refFormId,packageId,"Dialogue target not alive and resident");continue;
      }
      ctx.target={package.target.value,placement->second.base,gCurrentCellFormId,gExteriorWorldspaceQ1890,0,
        listener[0],listener[1],listener[2],false,false};
    }
    // Quest policy: a stable Accompany random gate per actor/package/game minute.
    if(package.type==7)ctx.randomPercent=float((actor.source.refFormId*2654435761u^packageId*2246822519u^uint32_t(fo3tod::WrapHour(hour)*60)*3266489917u)%100);
    std::string error;
    // GECK: Flee conditions gate entry but do not terminate an active package.
    const bool continuingFlee=package.type==10&&actor.aiPackage==packageId&&actor.aiSequence==1;
    const bool conditionsPass=fo3dialogue::Conditions(package.conditions,ctx,error);
    if(!error.empty()||(!continuingFlee&&!conditionsPass)) {
      if(!error.empty())Q240AiLogOnce(actor.source.refFormId,packageId,error);
      continue;
    }
    // Only location-directed, locally executable package families may cross
    // XTEL in this checkpoint. Target/follower and script procedures require
    // a separate unloaded-scheduler implementation.
    const bool locationProcedure=package.type==3||package.type==4||
      package.type==5||package.type==6||package.type==8||
      package.type==12||package.type==14;
    if(locationProcedure&&package.location.valid&&package.location.type==0){
      const auto remote=definitions.targets.find(package.location.value);
      if(remote!=definitions.targets.end()&&
         (remote->second.world!=0||remote->second.cell!=0)&&
         (remote->second.world!=gExteriorWorldspaceQ1890||
          (!gExteriorWorldspaceQ1890&&remote->second.cell!=gCurrentCellFormId))){
        uint32_t door=0;std::array<float,3> approach{};
        if(!Q240NpcRemoteDoor(actor,remote->second.cell,door,approach)){
          Q240AiLogOnce(actor.source.refFormId,packageId,"remote PLDT needs an accessible reachable XTEL door");
          continue;
        }
        actor.runtime.xtelDoor=door;actor.runtime.xtelCell=remote->second.cell;
        Q6H_LOGI("NPC XTEL INTENT actor=%08X package=%08X sourceCell=%08X targetCell=%08X door=%08X hour=%.2f",
          actor.source.refFormId,packageId,gCurrentCellFormId,remote->second.cell,door,hour);
        id=packageId;anchor=approach;radius=0;
        return &package;
      }
    }
    if(package.type==15){
      if(package.location.valid&&package.location.type!=2u&&package.location.type!=3u){
        Q240AiLogOnce(actor.source.refFormId,packageId,"Dialogue start-location mode unsupported");continue;
      }
#ifndef FO3_NPC_CHAT_AVAILABLE
      Q240AiLogOnce(actor.source.refFormId,packageId,"NPC conversation director unavailable");continue;
#else
      if(!FO3_NPC_CHAT_AVAILABLE(actor,package.target.value)){continue;}
      if(!Q240PackageActorTarget(actor,package.target.value,anchor))continue;
#endif
    }
    else if(package.type==7){std::array<float,3> current{};if(!Q240AccompanyDestination(actor,package,current,anchor)){Q240AccompanyOnce(actor.source.refFormId,packageId,package.target.value,"UNSUPPORTED","target/destination unavailable in resident scene; cross-cell continuation requires XTEL milestone");continue;}}
    else if(package.type==1||package.type==10) {
      if(!package.target.valid||package.target.type!=0){Q240AiLogOnce(actor.source.refFormId,packageId,"actor target resolver unsupported");continue;}
      if(!Q240PackageActorTarget(actor,package.target.value,anchor)){Q240AiLogOnce(actor.source.refFormId,packageId,"actor target is not alive and resident");continue;}
    } else if(package.type==13&&package.location.type==6u) {
      const auto& p=Q240PatrolRoute(actor,packageId,package)->front().placement;
      if(p.world!=gExteriorWorldspaceQ1890||(!p.world&&p.cell!=gCurrentCellFormId)){Q240AiLogOnce(actor.source.refFormId,packageId,"Patrol requires another resident scene");continue;}
      anchor={p.x,p.y,p.z};
    } else if(package.type==3||package.type==4||package.type==8) {
      if(package.type==8){
        const auto target=definitions.targets.find(package.target.value);
        if(target==definitions.targets.end()||!definitions.terminalBases.count(target->second.base)||
            (target->second.flags&0x820u)||target->second.parent||
            target->second.world!=gExteriorWorldspaceQ1890||
            (!gExteriorWorldspaceQ1890&&target->second.cell!=gCurrentCellFormId)){
          Q240AiLogOnce(actor.source.refFormId,packageId,"Use Item At terminal target unavailable in resident cell");continue;
        }
      }
      const size_t slot=Q240FurnitureTarget(actor,package);if(slot==SIZE_MAX){Q240AiLogOnce(actor.source.refFormId,packageId,"no available verified resident furniture marker");continue;}
      const auto& a=actor.runtime.furniture.scene->slots[slot].alignment;anchor={a[12],a[13],a[14]};
    } else if(package.location.type==0u) {
      const auto target=definitions.targets.find(package.location.value);
      if(target==definitions.targets.end()){Q240AiLogOnce(actor.source.refFormId,packageId,"location reference unavailable");continue;}
      if(target->second.world!=gExteriorWorldspaceQ1890||
         (!gExteriorWorldspaceQ1890&&target->second.cell!=gCurrentCellFormId)) {
        Q240AiLogOnce(actor.source.refFormId,packageId,"destination requires another scene/cell");continue;
      }
      anchor={target->second.x,target->second.y,target->second.z};
    } else if(package.location.type==1u&&package.type==12&&package.location.value==gCurrentCellFormId) anchor=game;
    else if(package.location.type==2u) anchor=game;
    else if(package.location.type==3u) anchor={actor.source.x,actor.source.y,actor.source.z};
    else {Q240AiLogOnce(actor.source.refFormId,packageId,"PLDT type "+std::to_string(package.location.type));continue;}
    radius=package.type==12&&package.location.type==1?std::numeric_limits<float>::max():float(std::max(0,package.location.radius));id=packageId;return &package;
  }
  return nullptr;
}
static void Q240PackageRouteFailed(Q230ActorVisual& actor,double now,const char* reason,double delay=2) {
  actor.runtime.furniture.Clear();
  actor.runtime.packageRetryAfter[actor.aiPackage]=now+delay;
  actor.runtime.nextPackageEvaluation=0;actor.runtime.blockedSince=-1;
  actor.runtime.procedure=fo3npc::Procedure::RouteFailed;
  actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;
  actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
  actor.runtime.navigationDoor=0;actor.aiRepathAt=now+delay;
  Q6H_LOGI("NPC PACKAGE FAIL actor=%08X package=%08X reason=%s retry=%.2f selectorContinues=1",actor.source.refFormId,actor.aiPackage,reason,delay);
}
static void Q240SuspendPackageWait(Q230ActorVisual& actor,double now) {
  actor.runtime.suspendedPackageWait=0;
  if(!gPlayerSession)return;
  const auto& packages=gPlayerSession->player.Definitions().pipboy.packages;
  const auto found=packages.find(actor.aiPackage);
  if(found!=packages.end()&&found->second.type==13&&actor.aiPathGame.empty())
    actor.runtime.suspendedPackageWait=float(std::max(0.,actor.aiRepathAt-now));
}
static void Q240ResumePackageWait(Q230ActorVisual& actor,double now) {
  actor.aiRepathAt=now+actor.runtime.suspendedPackageWait;
  actor.runtime.suspendedPackageWait=0;actor.aiLastUpdate=now;
  actor.runtime.nextPackageEvaluation=0;
}
// One package owner for ordinary updates and a pre-movement combat override.
// Adopting a newly selected package changes its intent/progress, never its root.
static void Q240AdoptPackage(Q230ActorVisual& actor,uint32_t packageId,const fo3pipdata::PackageDefinition& package,
                             const std::array<float,3>& anchor) {
  if(packageId!=actor.aiPackage) {
    const auto previous=gPlayerSession->player.Definitions().pipboy.packages.find(actor.aiPackage);
    if(previous!=gPlayerSession->player.Definitions().pipboy.packages.end()&&previous->second.type==7)
      Q6H_LOGI("NPC ACCOMPANY COMPLETE actor=%08X package=%08X target=%08X reason=reselection",actor.source.refFormId,actor.aiPackage,previous->second.target.value);
    if(package.type==7){
      Q6H_LOGI("NPC ACCOMPANY BEGIN actor=%08X package=%08X target=%08X",actor.source.refFormId,packageId,package.target.value);
      Q6H_LOGI("NPC ACCOMPANY TARGET actor=%08X package=%08X target=%08X distance=%d",actor.source.refFormId,packageId,package.target.value,package.target.radius);
    }
    actor.runtime.blockedSince=-1;actor.runtime.procedure=fo3npc::Procedure::Executing;
    actor.runtime.sandboxLastTarget=0;actor.runtime.sandboxCycles=0;actor.runtime.sandboxNext=0;
    actor.runtime.hasLastRoamDestination=false;
    actor.aiPackage=packageId;actor.aiAnchorGame=anchor;actor.aiSequence=0;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;actor.aiRepathAt=0;
#ifdef FO3_NPC_STAGGER_ROUTING
    // Megaton's 10:00 schedules may all change on the same stereo frame.
    // Delay only the first locally-planned civilian route by <= 0.5 seconds;
    // never defer authored package selection, combat or dialogue decisions.
    if(package.type==5||package.type==6||package.type==12||package.type==14)
      actor.aiRepathAt=actor.aiLastUpdate+
        .04*double((actor.source.refFormId*2654435761u)%13u);
#endif
    Q6H_LOGI("NPC PACKAGE actor=%08X package=%08X EDID=%s type=%u locationType=%u radius=%d flags=%08X style=%08X hour=%.2f",
      actor.source.refFormId,packageId,package.editor.c_str(),unsigned(package.type),
      package.location.type,package.location.radius,package.flags,package.combatStyle,GetFo3TimeOfDayHour());
  }
  actor.runtime.package=packageId;actor.runtime.packageKnown=true;
}
static bool Q240BuildPackagePath(Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& package,
                                 const std::array<float,3>& anchor,float radius) {
  actor.runtime.nextDoorQuery=0;actor.runtime.navigationDoor=0;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;if(!actor.navigationGraph||actor.navigationGraph->triangleCount==0)return false;
  const auto& graph=*actor.navigationGraph;
  const auto current=Q240GamePosition(actor);
  std::array<float,3> destination=anchor;
  if((package.type==5&&radius>0)||package.type==12) {
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
    // Avoid repeating the previous roaming endpoint when the authored radius
    // includes alternatives. Never expand PLDT or introduce fake destinations.
    std::vector<size_t> fresh;
    for(size_t i=0;i<choices.size();++i)
      if(!actor.runtime.hasLastRoamDestination||
         Q240PlanarDistance(choices[i],actor.runtime.lastRoamDestination)>96.f)
        fresh.push_back(i);
    const auto& pool=fresh;
    size_t selected=pool.empty()?hash%choices.size():pool[hash%pool.size()];
#ifdef FO3_NPC_ROAM_ENDPOINT_OCCUPIED
    // Prefer available points INSIDE the authored Sandbox/Wander radius when
    // other residents occupy/approach this one. Eight bounded probes avoid
    // a population-sized search on every reachable NAVM triangle.
    const size_t probes=std::min(size_t(8),pool.empty()?choices.size():pool.size());
    for(size_t attempt=0;attempt<probes;++attempt){
      const size_t candidate=pool.empty()?(hash+attempt)%choices.size():
        pool[(hash+attempt)%pool.size()];
      if(!FO3_NPC_ROAM_ENDPOINT_OCCUPIED(actor.source.refFormId,choices[candidate])){
        selected=candidate;break;
      }
    }
#endif
    destination=choices[selected];
  }
  std::string failure;
  const bool success=Q240Path(graph,current,destination,actor.aiPathGame,&failure,&actor.aiPathSurfaces);
  if(success && ((package.type==5&&radius>0)||package.type==12)) {
    actor.runtime.lastRoamDestination=destination;
    actor.runtime.hasLastRoamDestination=true;
    // Non-authoritative crowd variety: choose slightly different positions on
    // the SAME original NAVM portals for roaming only, not fixed patrols,
    // travel destinations, XTEL approaches, furniture or combat.
    for(size_t i=0;i+1<actor.aiPathSurfaces.size()&&i<actor.aiPathGame.size();++i) {
      const auto from=actor.aiPathSurfaces[i],to=actor.aiPathSurfaces[i+1];
      const Q240Node a{from.first,from.second},b{to.first,to.second};
      std::array<float,3> midpoint{};
      std::array<std::array<float,3>,2> edge{};
      if(!Q240Portal(graph,a,b,midpoint,&edge))continue;
      const uint32_t seed=actor.source.refFormId*2654435761u ^
        uint32_t(i+1)*2246822519u ^ actor.aiSequence*3266489917u;
      const float f=.35f+float(seed%31u)*.01f;
      for(int axis=0;axis<3;++axis)
        actor.aiPathGame[i][axis]=edge[0][axis]*(1.f-f)+edge[1][axis]*f;
    }
  }
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
// A physical wall or furniture may block the middle of a valid original
// NAVM portal. Try its other authored crossing positions, rather than
// repeatedly issuing the same blocked midpoint route. The fractions are
// explicitly a bounded Quest avoidance policy; vertices are original NAVM.
static bool Q240AlternatePortalWaypoint(Q230ActorVisual& actor) {
  const size_t i=actor.aiPathIndex;
  if(!actor.navigationGraph||i+1>=actor.aiPathSurfaces.size()||
     i>=actor.aiPathGame.size())return false;
  const auto& graph=*actor.navigationGraph;
  const auto from=actor.aiPathSurfaces[i],to=actor.aiPathSurfaces[i+1];
  const Q240Node a{from.first,from.second},b{to.first,to.second};
  if(!a.Valid()||!b.Valid())return false;
  std::array<float,3> middle{};
  std::array<std::array<float,3>,2> edge{};
  if(!Q240Portal(graph,a,b,middle,&edge))return false;
  const auto& current=actor.aiPathGame[i];
  std::array<float,3> next{};
  float fraction=0.0f;
  std::array<float,3> first{};
  for(int axis=0;axis<3;++axis)first[axis]=edge[0][axis]*.75f+edge[1][axis]*.25f;
  if(Q240PlanarDistance(current,middle)<.01f)fraction=.25f;
  else if(Q240PlanarDistance(current,first)<.01f)fraction=.75f;
  else {
    // A roaming actor may start at its own seeded portal crossing, rather
    // than the midpoint. Recovery must still offer the two validated edges.
    std::array<float,3> last{};
    for(int axis=0;axis<3;++axis)last[axis]=edge[0][axis]*.25f+edge[1][axis]*.75f;
    if(Q240PlanarDistance(current,last)<.01f)return false;
    fraction=.25f;
  }
  for(int axis=0;axis<3;++axis)next[axis]=edge[0][axis]*(1-fraction)+edge[1][axis]*fraction;
  actor.aiPathGame[i]=next;
  Q6H_LOGI("NPC PORTAL RECOVERY actor=%08X package=%08X waypoint=%zu portal=(%08X:%zu -> %08X:%zu) fraction=%.2f",
    actor.source.refFormId,actor.aiPackage,i,
    graph.meshes[a.mesh]->formId,a.triangle,graph.meshes[b.mesh]->formId,b.triangle,fraction);
  return true;
}
static void Q240GroundPathRoot(Q230ActorVisual& actor) {
  if(!actor.navigationGraph||actor.aiPathIndex>=actor.aiPathSurfaces.size())return;
  const auto surface=actor.aiPathSurfaces[actor.aiPathIndex];
  if(surface.first>=actor.navigationGraph->meshes.size()||
     surface.second>=actor.navigationGraph->meshes[surface.first]->triangles.size())return;
  // NAVM supplies a stable vertical neighbourhood for stacked Megaton
  // geometry, but the exact visible floor comes from resident collision/LAND.
  // Do not seed the physical query from an already-wrong actor root.
  const auto game=Q240GamePosition(actor);
  const auto navScene=Q240ScenePosition(
    Q240GroundPoint(*actor.navigationGraph->meshes[surface.first],
                    surface.second,game));
#ifdef FO3_NPC_WORLD_GROUND
  float physicalY=navScene[1];
  if(FO3_NPC_WORLD_GROUND(actor.runtime.position[0],actor.runtime.position[2],
                          navScene[1],&physicalY)) {
    const float oldY=actor.runtime.position[1];
    actor.runtime.position[1]=physicalY;
    if(std::fabs(physicalY-oldY)>.05f)
      Q6H_LOGI("NPC GROUND actor=%08X oldY=%.3f navY=%.3f physicalY=%.3f",
        actor.source.refFormId,oldY,navScene[1],physicalY);
    return;
  }
#endif
  // Host/no-collision fallback: use the current route surface rather than
  // preserving an authored ACHR height error.
  actor.runtime.position[1]=navScene[1];
}
// One-time/static standing root and authored idle-marker alignment. Existing
// locomotion already grounds each NAVM step. Original beds/chairs keep their
// FURN matrices and must NEVER be snapped to standing collision.
static void Q240GroundStandingRoot(Q230ActorVisual& actor,const char* reason){
#ifdef FO3_NPC_WORLD_GROUND
  if(!actor.navigationGraph||!actor.navigationGraph->triangleCount)return;
  const auto game=Q240GamePosition(actor);float nearest=INFINITY;
  const auto node=Q240NearestNode(*actor.navigationGraph,game,&nearest);
  // Stay on the closest plausible authored NAVM layer; do not pull an actor
  // down to the ground below a legitimate upper Megaton walkway.
  if(!node.Valid()||nearest>160.f)return;
  const auto nav=Q240GroundPoint(*actor.navigationGraph->meshes[node.mesh],
    node.triangle,game);
  const auto scene=Q240ScenePosition(nav);
  float physicalY=scene[1];
  if(!FO3_NPC_WORLD_GROUND(actor.runtime.position[0],actor.runtime.position[2],
                           scene[1],&physicalY) || !std::isfinite(physicalY))return;
  const float delta=physicalY-actor.runtime.position[1];
  if(std::fabs(delta)<.015f||std::fabs(delta)>2.f)return;
  Q6H_LOGI("NPC STANCE GROUND actor=%08X reason=%s oldY=%.3f navY=%.3f physicalY=%.3f",
    actor.source.refFormId,reason,actor.runtime.position[1],scene[1],physicalY);
  actor.runtime.position[1]=physicalY;
#else
  (void)actor;(void)reason;
#endif
}
static void Q240AdvancePath(Q230ActorVisual& actor,float dt,double now) {
  if(dt<=0||actor.aiPathIndex>=actor.aiPathGame.size())return;
  // Ground before turn-in-place. Previously an off-surface ACHR root remained
  // untouched until the first forward step, causing the visible delayed snap.
  Q240GroundPathRoot(actor);
  const auto targetScene=Q240ScenePosition(actor.aiPathGame[actor.aiPathIndex]);
  actor.runtime.destination={targetScene[0],actor.runtime.position[1],targetScene[2]};
  const float dx=targetScene[0]-actor.runtime.position[0],
              dz=targetScene[2]-actor.runtime.position[2];
  const float distance=std::sqrt(dx*dx+dz*dz);
  const auto blocked=[&](float step){
#ifdef FO3_NPC_PATH_BLOCKED
    return distance>0&&FO3_NPC_PATH_BLOCKED(actor,dx/distance,0,dz/distance,step,distance);
#else
    (void)step;return false;
#endif
  };
  const auto waitOrRecover=[&]() {
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;
    actor.runtime.procedure=fo3npc::Procedure::Blocked;
    if(actor.runtime.blockedSince<0)actor.runtime.blockedSince=now;
    // The original connected NAVM corridor may still offer another position
    // on the same authenticated portal. Do not detour while opening a door.
    if(!actor.runtime.navigationDoor&&now-actor.runtime.blockedSince>=.5 &&
       Q240AlternatePortalWaypoint(actor)){
      actor.runtime.blockedSince=now;
      return;
    }
    // Give local authored door animations time to finish. A persistent wall,
    // unavailable door or transient blocker must not hold this route forever.
    if(now-actor.runtime.blockedSince>=3) {
      Q240PackageRouteFailed(actor,now,"blocked-route",1);
      const auto& definitions=gPlayerSession->player.Definitions().pipboy;
      const auto p=definitions.packages.find(actor.aiPackage);
      if(p!=definitions.packages.end()&&(p->second.type==6||p->second.type==14))actor.aiSequence=0;
    }
  };
  if(distance<.08f) {
    if(blocked(distance)){waitOrRecover();return;}
    actor.runtime.blockedSince=-1;actor.runtime.procedure=fo3npc::Procedure::Executing;
    actor.runtime.position[0]=targetScene[0];actor.runtime.position[2]=targetScene[2];
    Q240GroundPathRoot(actor);
    ++actor.aiPathIndex;actor.runtime.speed=0;
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
  constexpr float WALK_METRES_PER_SECOND=1.05f;
  const bool combatEscape=actor.runtime.activity==fo3npc::Activity::Combat&&
                          actor.runtime.action==fo3npc::CombatAction::Flee;
  const float velocity=WALK_METRES_PER_SECOND*
      (combatEscape?gPlayerSession->player.Definitions().npcRunMultiplier:1.f);
  const float step=std::min(distance,velocity*dt),fraction=step/std::max(distance,.001f);
  if(blocked(step)){
    waitOrRecover();return;
  }
  actor.runtime.blockedSince=-1;actor.runtime.procedure=fo3npc::Procedure::Executing;
  actor.runtime.position[0]+=dx*fraction;actor.runtime.position[2]+=dz*fraction;
  Q240GroundPathRoot(actor);
  actor.runtime.speed=velocity;
  actor.runtime.animation=combatEscape&&!actor.animations[size_t(fo3npc::Animation::Run)].tracks.empty()?
      fo3npc::Animation::Run:fo3npc::Animation::Walk;
}
// A door handoff is an atomic canonical state change. The actor stays
// frozen/hidden in the source scene until that scene is torn down. On the next
// scene load its original ACHR is restored in the target CELL (no teleport of
// the player, no loading screen, no off-scene AI simulation).
static void Q240AdvanceNpcXtel(Q230ActorVisual& actor,float dt,double now){
#ifndef FO3_NPC_XTEL_INDEX
  (void)actor;(void)dt;(void)now;return;
#else
  auto index=FO3_NPC_XTEL_INDEX();auto& state=actor.runtime;
  const auto* edge=index?index->Find(state.xtelDoor):nullptr;
  if(!edge||edge->sourceCell!=gCurrentCellFormId||
     edge->sourceWorld!=gExteriorWorldspaceQ1890||
     edge->destinationCell!=state.xtelCell||
     (edge->sourceRecordFlags&0x820u)||(edge->destinationRecordFlags&0x820u)){
    Q240PackageRouteFailed(actor,now,"XTEL link no longer valid");return;
  }
  auto& player=gPlayerSession->player;const auto& defs=player.Definitions().pipboy;
  const auto original=defs.targets.find(edge->sourceRef);
  if(original==defs.targets.end()||(original->second.flags&0x820u)||
     original->second.parent||!player.CanActorOpenDoor(actor.source.refFormId,edge->sourceRef)){
    Q240PackageRouteFailed(actor,now,"XTEL door disabled, locked or scripted");return;
  }
  const std::array<float,3> target{original->second.x,original->second.y,original->second.z};
  const auto here=Q240GamePosition(actor);
  if(!actor.aiPathGame.empty()&&actor.aiPathIndex>0&&
     Q240PlanarDistance(here,target)<=96.f){
    fo3player::ActorState moved;
    const auto saved=player.Snapshot().actors.find(actor.source.refFormId);
    if(saved!=player.Snapshot().actors.end())moved=saved->second;
    moved.cell=edge->destinationCell;moved.world=edge->destinationWorld;
    moved.position=edge->arrival;moved.yaw=edge->rotation[2];
    moved.package=actor.aiPackage;moved.sequence=0;moved.packageWaitSeconds=0;
    if(!player.UpdateActor(actor.source.refFormId,moved)){
      Q240PackageRouteFailed(actor,now,"XTEL canonical actor transfer rejected");return;
    }
    actor.runtime.furniture.Clear();actor.runtime.offScene=true;
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
    actor.runtime.navigationDoor=0;
    Q6H_LOGI("NPC XTEL TRANSFER actor=%08X package=%08X door=%08X toDoor=%08X cell=%08X world=%08X",
      actor.source.refFormId,actor.aiPackage,edge->sourceRef,edge->destinationRef,
      edge->destinationCell,edge->destinationWorld);
    return;
  }
  if(actor.aiPathGame.empty()||actor.aiPathIndex>=actor.aiPathGame.size()){
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
    if(now<actor.aiRepathAt)return;
    std::string failure;
    if(!Q240Path(*actor.navigationGraph,here,target,actor.aiPathGame,&failure,&actor.aiPathSurfaces)||
       actor.aiPathGame.empty()||
       Q240PlanarDistance(actor.aiPathGame.back(),target)>96.f){
      Q240PackageRouteFailed(actor,now,"XTEL NAVM approach unavailable");return;
    }
    actor.aiRepathAt=now+1.0;
    Q6H_LOGI("NPC XTEL APPROACH actor=%08X package=%08X door=%08X cell=%08X waypoints=%zu",
      actor.source.refFormId,actor.aiPackage,edge->sourceRef,edge->destinationCell,
      actor.aiPathGame.size());
  }
  Q240AdvancePath(actor,dt,now);
#endif
}
// Script-free player Escort: approach the escorted player, then travel to the
// authored destination while respecting PKE2 distance. Sequence is a persistent
// procedure phase: 0 approach, 1 lead, 2 arrived. Other target families reject.
static void Q240AdvancePlayerEscort(Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& package,
                                    const std::array<float,3>& destination,float dt,double now) {
  const auto current=Q240GamePosition(actor);
  const std::array<float,3> player{gSceneCenterXQ1730+gQ210Head[0]*FO3_UNITS_PER_METRE,
    gSceneCenterYQ1730-(gQ210Head[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,
    gSceneFloorZQ1730+(gQ210Head[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
  const float separation=Q240PlanarDistance(current,player);
  if(actor.aiSequence==0&&separation<=package.escortDistance){actor.aiSequence=1;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;actor.aiRepathAt=0;}
  if(actor.aiSequence>=2) {
    // The phase is persisted, the route is not. Combat/relocation can also
    // move a previously arrived actor away from its authored destination.
    if(!actor.aiPathGame.empty()&&Q240PlanarDistance(current,actor.aiPathGame.back())<=8.f) {
      actor.runtime.procedure=fo3npc::Procedure::Completed;
      actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;return;
    }
    actor.aiSequence=1;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
    actor.runtime.navigationDoor=0;actor.aiRepathAt=0;
    actor.runtime.procedure=fo3npc::Procedure::Executing;
  }
  if(actor.aiSequence==1&&separation>package.escortDistance&&Q240PlanarDistance(player,destination)>=Q240PlanarDistance(current,destination)) {
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;return;
  }
  const auto target=actor.aiSequence==0?player:destination;
  if(actor.aiSequence==0&&Q240PlanarDistance(actor.runtime.pathDestinationGame,target)>16.f&&now>=actor.aiRepathAt){actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;}
  if(actor.aiPathIndex>=actor.aiPathGame.size()) {
    if(actor.aiSequence==1&&!actor.aiPathGame.empty()&&Q240PlanarDistance(current,actor.aiPathGame.back())<=8.f){actor.aiSequence=2;actor.runtime.procedure=fo3npc::Procedure::Completed;actor.runtime.nextPackageEvaluation=0;actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;return;}
    if(now<actor.aiRepathAt)return;
    if(!Q240BuildPackagePath(actor,package,target,0)){Q240PackageRouteFailed(actor,now,"Escort route unavailable");return;}
    actor.runtime.pathDestinationGame=target;actor.aiRepathAt=now+.75;
    Q6H_LOGI("NPC PATH actor=%08X package=%08X type=Escort phase=%u waypoints=%zu",actor.source.refFormId,actor.aiPackage,actor.aiSequence,actor.aiPathGame.size());
  }
  Q240AdvancePath(actor,dt,now);
}
// Script-free Flee From without a start location. Staying out of combat is
// package ownership, not immunity: perception/damage may still override it.
static void Q240AdvanceFleePackage(Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& package,
                                   const std::array<float,3>& threat,float dt,double now) {
  if(actor.aiSequence==0){actor.aiSequence=1;Q6H_LOGI("NPC FLEE actor=%08X package=%08X target=%08X minimum=%d",actor.source.refFormId,actor.aiPackage,package.target.value,package.target.radius);}
  const auto current=Q240GamePosition(actor);
  auto stop=[&](){actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;};
  if(Q240PlanarDistance(current,threat)>=package.target.radius) {
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;stop();return;
  }
  if(now>=actor.aiRepathAt&&(actor.aiPathIndex>=actor.aiPathGame.size()||Q240PlanarDistance(actor.runtime.pathDestinationGame,threat)>16.f)) {
    actor.aiRepathAt=now+.75;actor.runtime.pathDestinationGame=threat;
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;actor.runtime.nextDoorQuery=0;
    std::array<float,3> destination{};std::string failure;
    if(!Q240FleeDestination(*actor.navigationGraph,current,threat,float(package.target.radius),destination)||
       !Q240Path(*actor.navigationGraph,current,destination,actor.aiPathGame,&failure,&actor.aiPathSurfaces)) {
      Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,failure.empty()?"no farther reachable Flee endpoint":failure);
      Q240PackageRouteFailed(actor,now,"Flee route unavailable");return;
    }
    Q6H_LOGI("NPC PATH actor=%08X package=%08X target=%08X type=Flee waypoints=%zu",actor.source.refFormId,actor.aiPackage,package.target.value,actor.aiPathGame.size());
  }
  if(actor.aiPathIndex>=actor.aiPathGame.size()){stop();return;}
  Q240AdvancePath(actor,dt,now);
}
static void Q240ClearAccompanyRoute(Q230ActorVisual& actor) {
  actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
  actor.runtime.navigationDoor=0;actor.runtime.nextDoorQuery=0;actor.runtime.blockedSince=-1;
  actor.aiRepathAt=0;actor.runtime.nextPackageEvaluation=0;
}
static void Q240AdvanceAccompany(Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& p,float dt,double now) {
  if(actor.runtime.procedure==fo3npc::Procedure::Interrupted){Q240ClearAccompanyRoute(actor);actor.runtime.procedure=fo3npc::Procedure::Executing;}
  std::array<float,3> leader{},goal{};
  if(!Q240AccompanyDestination(actor,p,leader,goal)){Q240PackageRouteFailed(actor,now,"Accompany target/destination unavailable");return;}
  const auto here=Q240GamePosition(actor);const float distance=float(p.target.radius);
  // Quest policy: clip destination intent to the authored distance envelope
  // around the leader. Outside it, first catch up to the leader's live root.
  auto destination=goal;const float separation=Q240PlanarDistance(here,leader),intent=Q240PlanarDistance(goal,leader);
  if(separation>distance)destination=leader;
  else if(intent>distance){const float f=distance/intent;for(int i=0;i<3;++i)destination[i]=leader[i]+(goal[i]-leader[i])*f;}
  if(Q240PlanarDistance(here,destination)<=8.f||(intent<=8.f&&separation<=distance)){
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;actor.runtime.procedure=fo3npc::Procedure::Waiting;
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;actor.runtime.blockedSince=-1;return;
  }
  if(now>=actor.aiRepathAt&&(actor.aiPathIndex>=actor.aiPathGame.size()||Q240PlanarDistance(actor.runtime.pathDestinationGame,destination)>16.f)){
    const bool firstRoute=actor.aiRepathAt==0;
    actor.aiRepathAt=now+.75;actor.runtime.pathDestinationGame=destination;actor.runtime.navigationDoor=0;actor.runtime.nextDoorQuery=0;actor.aiPathIndex=0;
    std::string failure;
    if(!Q240Path(*actor.navigationGraph,here,destination,actor.aiPathGame,&failure,&actor.aiPathSurfaces)||actor.aiPathGame.empty()||Q240PlanarDistance(actor.aiPathGame.back(),destination)>8.f){Q240AccompanyOnce(actor.source.refFormId,actor.aiPackage,p.target.value,"UNSUPPORTED","NAVM endpoint unavailable");Q240PackageRouteFailed(actor,now,"Accompany NAVM endpoint unavailable");return;}
    Q6H_LOGI("NPC ACCOMPANY %s actor=%08X package=%08X target=%08X destination=(%.1f,%.1f,%.1f) distance=%d",firstRoute?"ROUTE":"REPATH",actor.source.refFormId,actor.aiPackage,p.target.value,destination[0],destination[1],destination[2],p.target.radius);
  }
  Q240AdvancePath(actor,dt,now);
}
// Conversation AI: approach the ORIGINAL PTDT actor via local NAVM, then
// request a mutually reserved single-voice HELLO from the resident director.
// No quest scripts/INFO results are executed by this path.
static void Q240AdvanceNpcTalk(Q230ActorVisual& actor,const fo3pipdata::PackageDefinition& p,float dt,double now) {
  std::array<float,3> listener{};
  if(!Q240PackageActorTarget(actor,p.target.value,listener)){
    Q240PackageRouteFailed(actor,now,"Dialogue partner no longer resident");return;
  }
  constexpr float talkingDistance=128.f; // proximity policy in FO3 game units
  const auto here=Q240GamePosition(actor);
  if(Q240PlanarDistance(here,listener)<=talkingDistance){
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
    actor.runtime.navigationDoor=0;actor.runtime.speed=0;
    actor.runtime.animation=fo3npc::Animation::Conversation;
    actor.runtime.procedure=fo3npc::Procedure::Waiting;
    if(now<actor.aiRepathAt)return;
#ifdef FO3_NPC_CHAT_REQUEST
    if(FO3_NPC_CHAT_REQUEST(actor.source.refFormId,p.target.value,now)){
      Q6H_LOGI("NPC CONVERSATION BEGIN actor=%08X package=%08X listener=%08X",
        actor.source.refFormId,actor.aiPackage,p.target.value);
      return;
    }
#endif
    actor.aiRepathAt=now+2.;return;
  }
  if(now>=actor.aiRepathAt&&(actor.aiPathIndex>=actor.aiPathGame.size()||
      Q240PlanarDistance(actor.runtime.pathDestinationGame,listener)>32.f)){
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
    actor.runtime.navigationDoor=0;actor.runtime.pathDestinationGame=listener;
    actor.aiRepathAt=now+.75;
    std::string error;
    if(!actor.navigationGraph||!Q240Path(*actor.navigationGraph,here,listener,actor.aiPathGame,
          &error,&actor.aiPathSurfaces)||actor.aiPathGame.empty()||
       Q240PlanarDistance(actor.aiPathGame.back(),listener)>talkingDistance){
      Q240PackageRouteFailed(actor,now,"Dialogue partner NAVM approach unavailable");return;
    }
  }
  if(actor.aiPathIndex<actor.aiPathGame.size())Q240AdvancePath(actor,dt,now);
}
static void Q240FurnitureExit(Q230ActorVisual& actor,double now) {
  auto& f=actor.runtime.furniture;const auto* slot=f.Selected();if(!slot){f.Clear();return;}
  if(slot->idleMarker){f.Clear();++actor.runtime.actionSerial;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;actor.runtime.sandboxNext=now+.5;actor.lastFrame=UINT64_MAX;return;}
  if(f.phase==fo3furniture::Phase::Approach){f.Clear();return;}
  if(f.phase==fo3furniture::Phase::Enter){
    // Finish entry before using the exit's authored accumulated starting pose.
    const double duration=(slot->program->enter.stop-slot->program->enter.start)/slot->program->enter.frequency;
    if(now-f.started<duration){f.interrupted=true;return;}
    f.retainedRoot=fo3furniture::Root(slot->program->enter,duration);
  }
  if(f.phase!=fo3furniture::Phase::Exit){f.phase=fo3furniture::Phase::Exit;f.started=now;actor.lastFrame=UINT64_MAX;}
}
static bool Q240AdvanceFurniture(Q230ActorVisual& actor,const fo3pipdata::PackageDefinition* p,uint32_t id,float dt,double now) {
  auto& f=actor.runtime.furniture;
  if(f.Active()&&(f.interrupted||!p||id!=f.package))Q240FurnitureExit(actor,now);
  if(f.interrupted&&f.phase==fo3furniture::Phase::Enter){actor.runtime.speed=0;actor.lastFrame=UINT64_MAX;return true;}
  if(f.phase==fo3furniture::Phase::Exit){
    const auto* slot=f.Selected();if(!slot){f.Clear();return false;}
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;actor.lastFrame=UINT64_MAX;
    const double duration=(slot->program->exit.stop-slot->program->exit.start)/slot->program->exit.frequency;
    if(now-f.started>=duration){
      // Exit's residual is authored. Bake it once into the world root before
      // ordinary locomotion resumes; every new cycle reanchors to FRN.
      const auto end=fo3furniture::Root(slot->program->exit,duration,&f.retainedRoot);
      const auto endpoint=fo3anim::Multiply(slot->alignment,fo3anim::ToMatrix(end));
      actor.runtime.position=Q240ScenePosition({endpoint[12],endpoint[13],endpoint[14]});actor.runtime.yaw=actor.runtime.returnYaw=std::atan2(-endpoint[4],endpoint[5]);
      f.Clear();++actor.runtime.actionSerial;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;actor.aiSequence=0;actor.aiRepathAt=0;actor.runtime.nextPackageEvaluation=0;actor.aiLastUpdate=now;actor.lastFrame=UINT64_MAX;
    }return true;
  }
  if(!p||(p->type!=4&&p->type!=3&&p->type!=8&&p->type!=12))return false;
  if(!f.Active()){
    const size_t target=p->type==12?Q240SandboxTarget(actor,*p,actor.runtime.evaluatedAnchor,actor.runtime.evaluatedRadius):Q240FurnitureTarget(actor,*p);if(target==SIZE_MAX){Q240PackageRouteFailed(actor,now,"furniture unavailable");return true;}
    const auto& slot=f.scene->slots[target];auto lease=std::make_shared<fo3furniture::Lease>();
    if(!lease->Acquire(f.scene->reservations,slot.reference,slot.index,actor.source.refFormId)){Q240PackageRouteFailed(actor,now,"furniture reservation conflict");return true;}
    f.slot=target;f.lease=std::move(lease);f.phase=fo3furniture::Phase::Approach;f.started=now;f.package=id;
    if(slot.idleMarker){
      // The marker may reference several original Bethesda IDLE sequences.
      // Select among those passing their CTDA gates instead of always using
      // the first eligible animation on every Sandbox visit.
      std::vector<size_t> eligible;
      for(size_t i=0;i<slot.idles.size();++i)
        if(Q240MarkerIdleEligible(actor,slot.idles[i].form))eligible.push_back(i);
      if(eligible.empty()){f.Clear();Q240PackageRouteFailed(actor,now,"sandbox idle conditions no longer eligible");return true;}
      const uint32_t choice=actor.source.refFormId*2654435761u ^
        slot.reference*2246822519u ^ actor.runtime.sandboxCycles*3266489917u;
      f.idleIndex=eligible[choice%eligible.size()];
    }
    Q6H_LOGI("NPC ACTIVITY SELECT actor=%08X package=%08X target=%08X marker=%d reservation=1",actor.source.refFormId,id,slot.reference,slot.idleMarker);
    f.anchor={slot.alignment[12],slot.alignment[13],slot.alignment[14]};f.yaw=std::atan2(-slot.alignment[4],slot.alignment[5]);
    std::string failure;
    if(!Q240Path(*actor.navigationGraph,Q240GamePosition(actor),f.anchor,actor.aiPathGame,&failure,&actor.aiPathSurfaces)){Q240PackageRouteFailed(actor,now,"furniture NAVM approach unavailable");return true;}
    actor.aiPathIndex=0;actor.runtime.blockedSince=-1;
  }
  if(f.phase==fo3furniture::Phase::Approach){
    if(now-f.started>60){Q240PackageRouteFailed(actor,now,"furniture approach timeout");return true;}
    if(actor.aiPathIndex<actor.aiPathGame.size()){Q240AdvancePath(actor,dt,now);return true;}
    // NAVM endpoint projection must actually reach the authored marker.
    if(Q240PlanarDistance(Q240GamePosition(actor),f.anchor)>8.f){Q240PackageRouteFailed(actor,now,"furniture marker outside reachable NAVM");return true;}
    const float angle=fo3npc::Angle(f.yaw-actor.runtime.yaw);
    actor.runtime.yaw=fo3npc::Angle(actor.runtime.yaw+std::clamp(angle,-fo3npc::DialoguePolicy::BodySpeed*dt,fo3npc::DialoguePolicy::BodySpeed*dt));actor.runtime.returnYaw=actor.runtime.yaw;
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;
    if(std::fabs(angle)>.01f)return true;
    actor.runtime.position=Q240ScenePosition(f.anchor);actor.runtime.yaw=actor.runtime.returnYaw=f.yaw;actor.runtime.headYaw=0;
    const auto* selected=f.Selected();
    if(selected&&selected->idleMarker)
      Q240GroundStandingRoot(actor,"original-idle-marker");
    f.phase=selected->idleMarker?fo3furniture::Phase::Loop:fo3furniture::Phase::Enter;f.started=now;actor.lastFrame=UINT64_MAX;
    if(p->type==12){const uint32_t hash=actor.source.refFormId*2654435761u^++actor.runtime.sandboxCycles*3266489917u;
      f.until=now+(selected->idleMarker?std::clamp(double(selected->idleTimer),1.,60.):12.+double(hash%13));
      if(selected->idleMarker){const auto& clip=selected->idles[f.idleIndex].clip;const double cycle=(clip.stop-clip.start)/clip.frequency;f.until=now+std::ceil((f.until-now)/cycle)*cycle;}
      actor.runtime.sandboxLastTarget=selected->reference;
    }
    Q6H_LOGI("NPC ACTIVITY BEGIN actor=%08X package=%08X target=%08X",actor.source.refFormId,id,selected->reference);
    if(p->type==8)Q6H_LOGI("NPC USEITEM ACTION actor=%08X package=%08X item=%08X furniture=%08X action=authored-sit",
      actor.source.refFormId,id,p->target.value,selected->reference);
  }
  const auto* slot=f.Selected();if(!slot){f.Clear();return true;}
  actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;actor.lastFrame=UINT64_MAX;
  if(p->type==12&&f.until>0&&now>=f.until){
    actor.runtime.sandboxNext=now+1.;Q6H_LOGI("NPC ACTIVITY END actor=%08X package=%08X target=%08X",actor.source.refFormId,id,slot->reference);
    Q240FurnitureExit(actor,now);return true;
  }
  if(slot->idleMarker){actor.runtime.procedure=fo3npc::Procedure::Waiting;return true;}
  if(f.phase==fo3furniture::Phase::Enter){const double duration=(slot->program->enter.stop-slot->program->enter.start)/slot->program->enter.frequency;
    if(now-f.started>=duration){f.retainedRoot=fo3furniture::Root(slot->program->enter,duration);f.phase=fo3furniture::Phase::Loop;f.started=now;}
  }
  actor.runtime.procedure=f.phase==fo3furniture::Phase::Loop?fo3npc::Procedure::Waiting:fo3npc::Procedure::Executing;return true;
}
static void Q240AdvanceNpcPackage(Q230ActorVisual& actor,double now) {
  if(!gPlayerSession||actor.rigs.empty()||actor.runtime.offScene)return;
#ifdef FO3_NPC_FURNITURE_LOADING
  if(FO3_NPC_FURNITURE_LOADING||gPlayerSession->saveBlocked){
    const auto active=gPlayerSession->player.Definitions().pipboy.packages.find(actor.aiPackage);
    if(active!=gPlayerSession->player.Definitions().pipboy.packages.end()&&active->second.type==7){
      Q240AccompanyOnce(actor.source.refFormId,actor.aiPackage,active->second.target.value,"INTERRUPT","loading/save suspension");
      Q240ClearAccompanyRoute(actor);actor.runtime.procedure=fo3npc::Procedure::Interrupted;actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;actor.aiLastUpdate=now;
    }
    if(actor.runtime.furniture.Active()){actor.runtime.furniture.Clear();++actor.runtime.actionSerial;actor.runtime.nextPackageEvaluation=0;actor.lastFrame=UINT64_MAX;}return;}
#endif
  if(!actor.runtime.Alive()){
    const auto active=gPlayerSession->player.Definitions().pipboy.packages.find(actor.aiPackage);
    if(active!=gPlayerSession->player.Definitions().pipboy.packages.end()&&active->second.type==7)Q240ClearAccompanyRoute(actor);
    actor.runtime.furniture.Clear();return;
  }
  if(now<actor.runtime.hitUntil){actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Hit;actor.aiLastUpdate=now;return;}
  if(actor.runtime.dialogue||actor.runtime.activity==fo3npc::Activity::Combat){
    const auto active=gPlayerSession->player.Definitions().pipboy.packages.find(actor.aiPackage);
    if(active!=gPlayerSession->player.Definitions().pipboy.packages.end()&&active->second.type==7)
      Q240AccompanyOnce(actor.source.refFormId,actor.aiPackage,active->second.target.value,"INTERRUPT",actor.runtime.dialogue?"dialogue":"combat");
    if(actor.runtime.furniture.Active()){actor.runtime.furniture.RequestExit();Q240AdvanceFurniture(actor,nullptr,0,0,now);}return;
  }
  // ACHR z and original marker coordinates may not land exactly on the
  // rendered floor. Only on first resident update: no per-frame NAVM scan.
  if(actor.aiLastUpdate<0&&!actor.runtime.furniture.Active())
    Q240GroundStandingRoot(actor,"initial-standing");
  if(!actor.navigationGraph||actor.navigationGraph->triangleCount==0) {
    actor.runtime.furniture.Clear();
    Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"resident actor has no authored NAVM graph");
    actor.runtime.procedure=fo3npc::Procedure::Unsupported;
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;return;
  }
  const float health=gPlayerSession->player.ActorHealth(actor.source.refFormId);
  if(health<=0){if(health<0)Q240AiLogOnce(actor.source.refFormId,0,"actor statistics unresolved (including levelled templates)");return;}
  float dt=0;if(actor.aiLastUpdate>=0)dt=std::clamp(float(now-actor.aiLastUpdate),0.f,.10f);
  actor.aiLastUpdate=now;
  uint32_t packageId=0;std::array<float,3> anchor{};float radius=0;
  auto& state=actor.runtime;
  const auto revision=gPlayerSession->player.PackageRevision();
  const int minute=int(fo3tod::WrapHour(GetFo3TimeOfDayHour())*60);
  const fo3pipdata::PackageDefinition* package=nullptr;
  if(now>=state.nextPackageEvaluation||state.packageRevision!=revision||state.packageMinute!=minute||state.evaluatedPackage!=actor.aiPackage) {
    package=Q240SelectPackage(actor,packageId,anchor,radius);
    state.evaluatedPackage=packageId;state.evaluatedAnchor=anchor;state.evaluatedRadius=radius;
    state.packageRevision=revision;state.packageMinute=minute;state.nextPackageEvaluation=now+.25;
  } else if(state.evaluatedPackage) {
    const auto& definitions=gPlayerSession->player.Definitions().pipboy;
    const auto found=definitions.packages.find(state.evaluatedPackage);
    if(found!=definitions.packages.end()){package=&found->second;packageId=found->first;anchor=state.evaluatedAnchor;radius=state.evaluatedRadius;}
  }
  // Actor targets remain live between condition scans, including death/unload.
  if(package&&(package->type==1||package->type==7||package->type==10)&&!Q240PackageActorTarget(actor,package->target.value,anchor)) {
    state.nextPackageEvaluation=0;state.procedure=fo3npc::Procedure::InvalidTarget;package=nullptr;
  }
  if(actor.runtime.furniture.Active()&&(actor.runtime.furniture.interrupted||!package||packageId!=actor.runtime.furniture.package)){
    if(Q240AdvanceFurniture(actor,package,packageId,dt,now))return;
  }
  if(!package) {
    const auto previous=gPlayerSession->player.Definitions().pipboy.packages.find(actor.aiPackage);
    if(previous!=gPlayerSession->player.Definitions().pipboy.packages.end()&&previous->second.type==7)
      Q6H_LOGI("NPC ACCOMPANY COMPLETE actor=%08X package=%08X target=%08X reason=ineligible-or-unavailable",actor.source.refFormId,actor.aiPackage,previous->second.target.value);
    if(actor.runtime.procedure!=fo3npc::Procedure::Unsupported)
      Q6H_LOGI("NPC AI IDLE actor=%08X base=%08X reason=no-executable-package",actor.source.refFormId,actor.source.baseFormId);
    actor.runtime.procedure=fo3npc::Procedure::Unsupported;
    actor.aiPackage=actor.runtime.package=0;actor.runtime.packageKnown=false;
    actor.runtime.activity=fo3npc::Activity::Idle;actor.runtime.speed=0;
    actor.runtime.animation=fo3npc::Animation::Idle;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;return;
  }
  const bool newlySelected=actor.aiPackage!=packageId;
  Q240AdoptPackage(actor,packageId,*package,anchor);
  if(state.xtelDoor){
    state.activity=fo3npc::Activity::Package;state.package=packageId;
    state.packageKnown=true;
    Q240AdvanceNpcXtel(actor,dt,now);
    return;
  }
  if(newlySelected&&package->type==8){
    // The original POBA script is executed exactly once per adoption through
    // the canonical, atomic dialogue-result interpreter. Failed scripts do not
    // simulate an item-use success or apply guessed quest effects.
    if(!package->onBeginScript.empty()){
      fo3pipdata::ResultScript begin;begin.source=package->onBeginScript;
      std::string error;
      if(!gPlayerSession->player.ExecuteDialogueResult(begin,error)){
        Q240AiLogOnce(actor.source.refFormId,packageId,"Use Item At OnBegin: "+error);
        Q240PackageRouteFailed(actor,now,"unsupported Use Item At OnBegin");return;
      }
    }
    Q6H_LOGI("NPC USEITEM BEGIN actor=%08X package=%08X item=%08X location=%08X",
      actor.source.refFormId,packageId,package->target.value,package->location.value);
    state.nextPackageEvaluation=0; // OnBegin may change its own CTDA gates.
  }
  if(package->type==13) {
    bool circular=false;const auto& points=*Q240PatrolRoute(actor,packageId,*package,&circular);const size_t n=points.size();
    // Sequence is the current leg (one-based), and is saved in the existing
    // canonical actor progress record. Open repeat routes reverse at each end.
    if(actor.aiSequence==0) {
      size_t first=0;
      if(package->patrolRepeat){float nearest=INFINITY;const auto here=Q240GamePosition(actor);for(size_t i=0;i<n;++i){const auto& p=points[i].placement;const float d=Q240PlanarDistance(here,{p.x,p.y,p.z});if(d<nearest){nearest=d;first=i;}}}
      actor.aiSequence=uint32_t(first+1);
    }
    const auto index=[&](uint32_t sequence){const size_t leg=sequence-1;if(!package->patrolRepeat)return std::min(leg,n-1);if(circular||n==1)return leg%n;const size_t at=leg%(2*n-2);return at<n?at:2*n-2-at;};
    if(!actor.aiPathGame.empty()&&actor.aiPathIndex>=actor.aiPathGame.size()) {
      // A reached leg index can survive displacement, but is not evidence
      // that the actor is still at this marker. Rebuild the same leg instead
      // of advancing and silently skipping its authored wait/action.
      if(Q240PlanarDistance(Q240GamePosition(actor),actor.aiPathGame.back())>8.f) {
        actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
        actor.runtime.navigationDoor=0;actor.aiRepathAt=0;
      } else {
        const size_t reached=!package->patrolRepeat&&circular&&actor.aiSequence==n+1?0:index(actor.aiSequence);
        const auto& arrived=points[reached].placement;
        actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
        actor.aiRepathAt=now+arrived.patrolWait;++actor.aiSequence;
        actor.runtime.procedure=fo3npc::Procedure::Waiting;
        actor.runtime.nextPackageEvaluation=0;
        Q6H_LOGI("NPC PACKAGE WAIT actor=%08X package=%08X marker=%08X seconds=%.2f",actor.source.refFormId,packageId,points[reached].reference,double(arrived.patrolWait));
      }
    }
    actor.runtime.package=packageId;actor.runtime.packageKnown=true;actor.runtime.activity=fo3npc::Activity::Package;
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;
    if(now<actor.aiRepathAt)return;
    const size_t legs=n+(circular?1:0);
    if(!package->patrolRepeat&&actor.aiSequence>legs){if(package->schedule.duration>0)return;actor.aiSequence=1;}
    size_t next=index(actor.aiSequence);
    if(!package->patrolRepeat&&circular&&actor.aiSequence==n+1)next=0;
    const auto& point=points[next].placement;anchor={point.x,point.y,point.z};
  }
  if(package->location.type==2u&&package->type!=1&&package->type!=10)anchor=actor.aiAnchorGame;
  actor.runtime.package=packageId;actor.runtime.packageKnown=true;actor.runtime.activity=fo3npc::Activity::Package;
  if(package->type==12){
    if(actor.runtime.furniture.Active()){Q240AdvanceFurniture(actor,package,packageId,dt,now);return;}
    if(now<state.sandboxNext||(actor.aiPathGame.empty()&&now<actor.aiRepathAt)){
      actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;return;
    }
    if(actor.aiPathIndex>=actor.aiPathGame.size()&&now>=actor.aiRepathAt){
      const bool wander=(package->typeFlags&32u)==0;
      if((!wander||(state.sandboxCycles%2)==0)&&Q240SandboxTarget(actor,*package,anchor,radius)!=SIZE_MAX){Q240AdvanceFurniture(actor,package,packageId,dt,now);return;}
      if(!wander){Q240PackageRouteFailed(actor,now,"Sandbox has no available permitted resident activity",2);return;}
      ++state.sandboxCycles;
    }
  }
  if(package->type==4||package->type==3||package->type==8){Q240AdvanceFurniture(actor,package,packageId,dt,now);return;}
  if(package->type==7){Q240AdvanceAccompany(actor,*package,dt,now);return;}
  if(package->type==15){Q240AdvanceNpcTalk(actor,*package,dt,now);return;}
  if(package->type==10){Q240AdvanceFleePackage(actor,*package,anchor,dt,now);return;}
  if(package->type==2){Q240AdvancePlayerEscort(actor,*package,anchor,dt,now);return;}
  if(package->type==1&&Q240PlanarDistance(Q240GamePosition(actor),anchor)<=std::max(0,package->target.radius)){
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;actor.runtime.procedure=fo3npc::Procedure::Waiting;
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;actor.runtime.blockedSince=-1;return;
  }
  if(package->type==1&&Q240PlanarDistance(actor.aiAnchorGame,anchor)>16.f&&now>=actor.aiRepathAt){actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.navigationDoor=0;actor.runtime.blockedSince=-1;actor.aiAnchorGame=anchor;}
  actor.runtime.package=packageId;actor.runtime.packageKnown=true;actor.runtime.activity=fo3npc::Activity::Package;
  if(actor.aiPathIndex>=actor.aiPathGame.size()) {
    actor.runtime.speed=0;actor.runtime.animation=fo3npc::Animation::Idle;
    // Travel builds one authored route when the package becomes active, then
    // remains at the destination for the rest of that package. Wander/Sandbox
    // keep selecting new NAVM destinations after their short pause.
    const bool staysAtDestination=package->type==6||package->type==14||(package->type==5&&radius==0);
    if(staysAtDestination&&actor.aiSequence>0) {
      // Sequence alone is not proof of arrival after save restore, combat or
      // relocation. Only a reached route endpoint can keep this actor waiting.
      if(!actor.aiPathGame.empty()&&Q240PlanarDistance(Q240GamePosition(actor),actor.aiPathGame.back())<=8.f) {
        if(actor.runtime.procedure!=fo3npc::Procedure::Completed)
          Q6H_LOGI("NPC PACKAGE COMPLETE actor=%08X package=%08X type=%u scheduleReevaluated=1",actor.source.refFormId,packageId,unsigned(package->type));
        actor.runtime.procedure=fo3npc::Procedure::Completed;return;
      }
      actor.aiSequence=0;actor.aiRepathAt=0;actor.runtime.procedure=fo3npc::Procedure::Executing;
      Q6H_LOGI("NPC PACKAGE RESUME actor=%08X package=%08X reason=arrival-needs-route",actor.source.refFormId,packageId);
    }
    if(now<actor.aiRepathAt)return;
    if(!Q240BuildPackagePath(actor,*package,anchor,radius)){
      Q240AiLogOnce(actor.source.refFormId,packageId,
        "path build failed on authored NAVM graph meshes="+std::to_string(actor.navigationGraph->meshes.size())+
        " triangles="+std::to_string(actor.navigationGraph->triangleCount));
      Q240PackageRouteFailed(actor,now,"authored NAVM route unavailable");return;
    }
    if(staysAtDestination)actor.aiSequence=1;
    if(actor.aiSequence<=2u||(actor.aiSequence%8u)==0u)
      Q6H_LOGI("NPC PATH actor=%08X package=%08X type=%u waypoints=%zu graphMeshes=%zu graphTriangles=%zu sequence=%u",
        actor.source.refFormId,packageId,unsigned(package->type),actor.aiPathGame.size(),
        actor.navigationGraph->meshes.size(),actor.navigationGraph->triangleCount,actor.aiSequence);
    actor.aiRepathAt=now+1;
  }
  Q240AdvancePath(actor,dt,now);
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
