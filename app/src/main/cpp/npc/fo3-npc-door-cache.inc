// Immutable CELL/NAVM topology only. Locks, actor keys, PACK conditions, door
// ranking and the crowd-aware movement route are always evaluated live.
struct Q280XtelDistanceCache {
  std::weak_ptr<const fo3xtel::Index> owner;
  std::unordered_map<uint32_t,std::unordered_map<uint32_t,size_t>> targets;
};
static Q280XtelDistanceCache gQ280XtelDistances;
static const std::unordered_map<uint32_t,size_t>& Q280CellDistances(
    const std::shared_ptr<const fo3xtel::Index>& index,uint32_t target) {
  if(gQ280XtelDistances.owner.lock()!=index) {
    gQ280XtelDistances.targets.clear();gQ280XtelDistances.owner=index;
  }
  auto found=gQ280XtelDistances.targets.find(target);
  if(found!=gQ280XtelDistances.targets.end()){++gQ280DoorCacheHits;return found->second;}
  if(gQ280XtelDistances.targets.size()>=16)gQ280XtelDistances.targets.clear();
  ++gQ280CellRouteBuilds;
  return gQ280XtelDistances.targets.emplace(target,index->CellDistancesTo(target)).first->second;
}
struct Q280DoorTopology {
  std::weak_ptr<const Q240NavigationGraph> owner;
  size_t triangles=0,meshes=0;
  std::vector<size_t> offsets,reverse;
  struct Destination {std::array<float,3> point{};size_t flat=SIZE_MAX;bool valid=false;};
  std::unordered_map<uint32_t,Destination> destinations;
  struct Reachability {size_t goal;std::vector<uint8_t> reaches;};
  std::vector<Reachability> goals;
  void Build(const std::shared_ptr<const Q240NavigationGraph>& graph) {
    owner=graph;triangles=graph->triangleCount;meshes=graph->meshes.size();
    offsets.assign(triangles+1,0);reverse.clear();destinations.clear();goals.clear();
    std::vector<std::pair<size_t,size_t>> edges;
    for(size_t from=0;from<triangles;++from) {
      const auto node=Q240NodeAt(*graph,from);
      Q240Neighbors(*graph,node,[&](Q240Node next){
        const auto to=Q240Flat(*graph,next);std::array<float,3> portal{};
        if(to<triangles&&Q240Portal(*graph,node,next,portal)) {
          edges.emplace_back(from,to);++offsets[to+1];
        }
      });
    }
    for(size_t i=1;i<offsets.size();++i)offsets[i]+=offsets[i-1];
    reverse.resize(edges.size());auto cursor=offsets;
    for(const auto& edge:edges)reverse[cursor[edge.second]++]=edge.first;
    ++gQ280DoorGraphBuilds;
  }
  const Destination& Goal(const Q240NavigationGraph& graph,uint32_t ref,const std::array<float,3>& point) {
    auto found=destinations.find(ref);
    if(found!=destinations.end()&&found->second.point==point){++gQ280DoorCacheHits;return found->second;}
    // Bound retained placement queries, including destinations outside NAVM.
    if(destinations.size()>=128)destinations.clear();
    Destination result;result.point=point;float distance=0;
    const auto node=Q240NearestNode(graph,point,&distance);result.flat=Q240Flat(graph,node);
    result.valid=node.Valid()&&result.flat<triangles&&distance<=500.f&&
      Q240PlanarDistance(Q240GroundPoint(*graph.meshes[node.mesh],node.triangle,point),point)<=96.f;
    ++gQ280DoorGoalScans;
    return destinations.insert_or_assign(ref,result).first->second;
  }
  bool Reaches(const Q240NavigationGraph& graph,size_t start,size_t goal) {
    (void)graph;
    for(const auto& cached:goals)if(cached.goal==goal){++gQ280DoorCacheHits;return cached.reaches[start]!=0;}
    // Reverse traversal preserves directed links: a start belongs to this set
    // exactly when it has a validated forward route to the selected door.
    Reachability result{goal,std::vector<uint8_t>(triangles,0)};
    std::vector<size_t> queue{goal};result.reaches[goal]=1;
    for(size_t at=0;at<queue.size();++at)
      for(size_t i=offsets[queue[at]];i<offsets[queue[at]+1];++i) {
        const auto next=reverse[i];if(!result.reaches[next]){result.reaches[next]=1;queue.push_back(next);}
      }
    ++gQ280DoorFloods;const bool reachable=result.reaches[start]!=0;
    if(goals.size()>=16)goals.erase(goals.begin());
    goals.push_back(std::move(result));return reachable;
  }
};
static std::vector<Q280DoorTopology> gQ280DoorTopologies;
static Q280DoorTopology& Q280DoorGraph(const std::shared_ptr<const Q240NavigationGraph>& graph) {
  for(auto i=gQ280DoorTopologies.begin();i!=gQ280DoorTopologies.end();) {
    const auto owner=i->owner.lock();
    if(!owner){i=gQ280DoorTopologies.erase(i);continue;}
    if(owner==graph) {
      if(i->triangles!=graph->triangleCount||i->meshes!=graph->meshes.size())i->Build(graph);
      return *i;
    }
    ++i;
  }
  // Graph publications are immutable; weak ownership cannot extend residency
  // or confuse a recycled address with an earlier scene's topology.
  if(gQ280DoorTopologies.size()>=4)gQ280DoorTopologies.erase(gQ280DoorTopologies.begin());
  gQ280DoorTopologies.emplace_back();gQ280DoorTopologies.back().Build(graph);
  return gQ280DoorTopologies.back();
}
