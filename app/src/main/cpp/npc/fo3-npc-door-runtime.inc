// Called only while advancing a resident NAVM path. Door animation/collision
// remains owned by the existing local-door runtime, never the player XTEL queue.
#ifndef FO3_NPC_DOOR_HOST_TEST
static bool Q230QueryPathDoor(float ox,float oy,float oz,float dx,float dy,float dz,Fo3DoorAimQ1700* aim) {
  *aim={};const GpuObject* hit=nullptr;float nearest=3.f;
  for(const auto& object:gObjects){
    if(object.baseRecordType!="DOOR"||(!object.teleport.valid&&(!object.q2400SwingDoor||gExteriorWorldspaceQ1890)))continue;
    float distance=0;if(RayAabbQ7(ox,oy,oz,dx,dy,dz,object,distance)&&distance<nearest){nearest=distance;hit=&object;}
  }
  if(!hit)return false;
  aim->valid=true;aim->sourceDoorRef=hit->refFormId;aim->distance=nearest;
  aim->localSwing=!hit->teleport.valid&&hit->q2400SwingDoor;return true;
}
#endif
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
  if(!Q230LiveBone(actor,actor.chestBone,origin))return true;
  if(Q230MovingDoorBlocks(origin,{dx,dy,dz},step))return true;
  auto& pending=actor.runtime.navigationDoor;
  if(!pending) {
    if(!HasFo3InteractionOccluder(origin[0],origin[1],origin[2],dx,dy,dz,step,actor.source.refFormId))return false;
    const double now=std::max(0.,actor.aiLastUpdate);
    if(now<actor.runtime.nextDoorQuery)return true;
    actor.runtime.nextDoorQuery=now+.5; // bounded resident obstruction query
    Fo3DoorAimQ1700 aim;
    if(!Q230QueryPathDoor(origin[0],origin[1],origin[2],dx,dy,dz,&aim)||!aim.valid||aim.distance>segment){Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"resident path obstruction without a supported door on this segment");return true;}
    if(!aim.localSwing){Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"NPC load-door traversal unavailable");return true;}
    if(HasFo3InteractionOccluder(origin[0],origin[1],origin[2],dx,dy,dz,aim.distance,aim.sourceDoorRef))return true;
    pending=aim.sourceDoorRef;
  }
  const auto state=gQ2400InteriorDoors.find(pending);
  if(state!=gQ2400InteriorDoors.end()) {
    const auto& door=state->second;
    if(door.targetOpen&&(door.q2401NifAnimation?!door.q2401Moving:door.progress>=.999f)) {
      pending=0;
      return HasFo3InteractionOccluder(origin[0],origin[1],origin[2],dx,dy,dz,step,actor.source.refFormId);
    }
    // Never toggle an opening door closed, or reverse a closing sequence.
    if(door.targetOpen||door.q2401Moving||door.progress>0)return true;
  }
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
