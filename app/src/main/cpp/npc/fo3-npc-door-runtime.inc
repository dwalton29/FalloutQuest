// Called only while advancing a resident NAVM path. Door animation/collision
// remains owned by the existing local-door runtime, never the player XTEL queue.
static bool Q230DoorMoving(const Q2400InteriorDoorState&);
static bool Q230DoorSettledOpen(uint32_t reference){
  const auto state=gQ2400InteriorDoors.find(reference);
  return state!=gQ2400InteriorDoors.end()&&state->second.collisionSceneSerial==gQ2400DoorSceneSerial&&state->second.targetOpen&&!Q230DoorMoving(state->second);
}
struct Q230DoorBounds {uint32_t reference=0;std::array<float,3> lo{},hi{};};
static std::vector<Q230DoorBounds> gQ230DoorBounds;
static uint64_t gQ230DoorBoundsScene=0;
static bool Q230DoorMoving(const Q2400InteriorDoorState& state) {
  return state.collisionSceneSerial==gQ2400DoorSceneSerial&&
    (state.q2401NifAnimation?state.q2401Moving:(state.targetOpen?state.progress<.999f:state.progress>.001f));
}
static uint32_t Q230BlockingMovingDoor(const std::array<float,3>& origin,const std::array<float,3>& direction,float step) {
  bool moving=false;for(const auto& entry:gQ2400InteriorDoors)if(Q230DoorMoving(entry.second)){moving=true;break;}
  if(!moving)return 0;
  // A door activated by someone else has temporarily absent world collision
  // during its animation. Retain original closed bounds as a conservative
  // navigation barrier until settlement, including after a route replan.
  if(gQ230DoorBoundsScene!=gQ2400DoorSceneSerial){
    gQ230DoorBounds.clear();
    for(const auto& object:gObjects)if(object.q2400SwingDoor&&!object.teleport.valid)
      gQ230DoorBounds.push_back({object.refFormId,{object.minX,object.minY,object.minZ},{object.maxX,object.maxY,object.maxZ}});
    gQ230DoorBoundsScene=gQ2400DoorSceneSerial;
  }
  for(const auto& bounds:gQ230DoorBounds){const auto state=gQ2400InteriorDoors.find(bounds.reference);
    float hit=0;if(state!=gQ2400InteriorDoors.end()&&Q230DoorMoving(state->second)&&fo3interaction::Box(origin,direction,bounds.lo,bounds.hi,step,hit))return bounds.reference;
  }
  return 0;
}
static bool Q230PathBlocked(Q230ActorVisual& actor,float dx,float dy,float dz,float step,float segment) {
  Q230UpdateActor(actor);std::array<float,3> origin{};
  if(!Q230LiveBone(actor,actor.chestBone,origin)) {
    std::array<float,3> lower{},upper{};
    if(!Q230LiveBounds(actor,lower,upper))return true;
    origin=actor.runtime.position;
    origin[1]=(lower[1]+upper[1])*.5f;
  }
  // Trace the authored route from the root, independent of animated lean.
  origin[0]=actor.runtime.position[0];origin[2]=actor.runtime.position[2];
  auto& pending=actor.runtime.navigationDoor;
  if(const auto door=Q230BlockingMovingDoor(origin,{dx,dy,dz},step)){
    pending=door;
    return true;
  }
  if(pending) {
    const auto state=gQ2400InteriorDoors.find(pending);
    if(state!=gQ2400InteriorDoors.end()&&Q230DoorMoving(state->second))return true;
    pending=0;
  }
  const double now=std::max(0.,actor.aiLastUpdate);
  const bool reach=!gExteriorWorldspaceQ1890&&now>=actor.runtime.nextDoorProbe;
  if(reach)actor.runtime.nextDoorProbe=now+.25;
  // Look ahead to arm's reach along the current route. The nearest surface
  // still decides access, and a wall outside the step does not stop movement.
  const float stepTrace=step+.01f;
  const float distance=std::min(segment,reach?std::max(stepTrace,.55f):stepTrace);
  Fo3WorldSurfaceHit hit;
  const bool obstructed=TraceFo3WorldSurface(origin[0],origin[1],origin[2],dx,dy,dz,
      distance,actor.source.refFormId,0,&hit);
  const bool blocked=obstructed&&hit.distance<stepTrace;
  if(gExteriorWorldspaceQ1890||now<actor.runtime.nextDoorQuery)return blocked;
  if((!hit.reference||!Q2400FindSwingDoorObject(hit.reference))&&reach){
    // Windowed leaves may have no collision surface at chest height.
    // A second trace must hit a real resident door along the same corridor.
    const float waist=(origin[1]+actor.runtime.position[1])*.5f;
    Fo3WorldSurfaceHit lower;
    if(TraceFo3WorldSurface(origin[0],waist,origin[2],dx,dy,dz,distance,
         actor.source.refFormId,0,&lower)&&(!obstructed||lower.distance<=hit.distance+.1f))
      hit=lower;
  }
  if(!hit.reference||!Q2400FindSwingDoorObject(hit.reference)||
     Q230DoorSettledOpen(hit.reference))return blocked;
  const bool doorInStep=hit.distance<stepTrace;
  const auto door=gQ2400InteriorDoors.find(hit.reference);
  if(door!=gQ2400InteriorDoors.end()&&Q230DoorMoving(door->second))return blocked;
  if(!gPlayerSession)return blocked;
  auto& player=gPlayerSession->player;
  const auto id=actor.source.refFormId;
  actor.runtime.nextDoorQuery=now+.5;
  pending=hit.reference;
  if(player.HasReferenceEvent(pending,"OnActivate")&&player.CanScriptActivateDoor(pending,id)){
    fo3worldscript::Event(pending,"OnActivate",id,false);
    return blocked||doorInStep;
  }
  if(!player.CanActorOpenDoor(id,pending)){
    Q240AiLogOnce(id,actor.aiPackage,"NPC door access denied ref="+std::to_string(pending));
    pending=0;return blocked||doorInStep;
  }
  if(Q2400ToggleInteriorDoor(pending,origin[0],origin[2])) {
    const auto ref=gPlayerSession->player.Definitions().references.find(pending);
    if(ref!=gPlayerSession->player.Definitions().references.end())fo3audio::Open(ref->second.base);
    Q6H_LOGI("NPC DOOR actor=%08X door=%08X action=open",actor.source.refFormId,pending);
  }else {Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"resident local door unavailable ref="+std::to_string(pending));pending=0;}
  return blocked||doorInStep;
}
