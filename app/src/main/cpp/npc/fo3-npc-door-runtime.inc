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
static bool Q230MovingDoorBlocks(const std::array<float,3>& origin,const std::array<float,3>& direction,float step) {
  bool moving=false;for(const auto& entry:gQ2400InteriorDoors)if(Q230DoorMoving(entry.second)){moving=true;break;}
  if(!moving)return false;
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
    float hit=0;if(state!=gQ2400InteriorDoors.end()&&Q230DoorMoving(state->second)&&fo3interaction::Box(origin,direction,bounds.lo,bounds.hi,step,hit))return true;
  }
  return false;
}
static bool Q230PathBlocked(Q230ActorVisual& actor,float dx,float dy,float dz,float step,float segment) {
  Q230UpdateActor(actor);std::array<float,3> origin{};
  if(!Q230LiveBone(actor,actor.chestBone,origin)) {
    std::array<float,3> lower{},upper{};
    if(!Q230LiveBounds(actor,lower,upper))return true;
    origin=actor.runtime.position;
    origin[1]=(lower[1]+upper[1])*.5f;
  }
  if(Q230MovingDoorBlocks(origin,{dx,dy,dz},step))return true;
  auto& pending=actor.runtime.navigationDoor;
  auto trace=[&](Fo3WorldSurfaceHit& hit){
    // Interaction rays exclude a centimetre at the end. At headset frame
    // rates that can discard the entire movement step.
    return TraceFo3WorldSurface(origin[0],origin[1],origin[2],dx,dy,dz,
        std::min(segment,step+.01f),actor.source.refFormId,0,&hit);
  };
  if(pending) {
    const auto state=gQ2400InteriorDoors.find(pending);
    if(state!=gQ2400InteriorDoors.end()&&Q230DoorMoving(state->second))return true;
    pending=0;
  }
  Fo3WorldSurfaceHit hit;
  if(!trace(hit))return false;
  if(!hit.reference||!Q2400FindSwingDoorObject(hit.reference)||gExteriorWorldspaceQ1890||
     Q230DoorSettledOpen(hit.reference))return true;
  const auto door=gQ2400InteriorDoors.find(hit.reference);
  if(door!=gQ2400InteriorDoors.end()&&Q230DoorMoving(door->second))return true;
  const double now=std::max(0.,actor.aiLastUpdate);
  if(now<actor.runtime.nextDoorQuery)return true;
  actor.runtime.nextDoorQuery=now+.5;
  pending=hit.reference;
  if(!gPlayerSession||!gPlayerSession->player.CanActorOpenDoor(actor.source.refFormId,pending)) {
    Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"NPC door lock/key/script access denied ref="+std::to_string(pending));pending=0;return true;
  }
  if(Q2400ToggleInteriorDoor(pending,origin[0],origin[2])) {
    const auto ref=gPlayerSession->player.Definitions().references.find(pending);
    if(ref!=gPlayerSession->player.Definitions().references.end())fo3audio::Open(ref->second.base);
    Q6H_LOGI("NPC DOOR actor=%08X door=%08X action=open",actor.source.refFormId,pending);
  }else {Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"resident local door unavailable ref="+std::to_string(pending));pending=0;}
  return true;
}
