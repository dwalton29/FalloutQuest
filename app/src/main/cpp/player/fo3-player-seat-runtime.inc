// Original FURN marker and KF data is prepared with the scene, never loaded
// from an aiming ray or the per-eye draw path.
bool Fo3PlayerSeatBlocked(){return gPlayerSeat.Busy();}
uint32_t Fo3PlayerSeatReference(){return gPlayerSeat.seated?gPlayerSeat.reference:0;}
static size_t Fo3PlayerSeatSlot(uint32_t reference){
 if(!gPlayerSeat.scene)return SIZE_MAX;
 for(size_t i=0;i<gPlayerSeat.scene->slots.size();++i){const auto& s=gPlayerSeat.scene->slots[i];
  if(s.reference==reference&&s.program&&!s.sleep&&!s.idleMarker&&(s.marker.reference==11||s.marker.reference==12||s.marker.reference==14))return i;
 }return SIZE_MAX;
}
bool CanFo3PlayerSit(uint32_t reference){
 const auto index=Fo3PlayerSeatSlot(reference);if(index==SIZE_MAX||!gPlayerSession||gPlayerSession->saveBlocked||gPlayerSeat.Busy())return false;
 if(gPlayerSeat.seated)return reference==gPlayerSeat.reference;
 const auto& p=gPlayerSession->player;if(!IsFo3ReferenceEnabled(p,reference))return false;
 const auto owner=p.ResolveOwnership(reference);if(!owner.known||(!owner.permitted&&!owner.inherited))return false;
 if(gPlayerSeat.scene->reservations->owners.count(uint64_t(reference)<<32))return false;
 for(const auto& actor:gQ230NpcActors){const auto* slot=actor.runtime.furniture.Selected();if(actor.runtime.Alive()&&actor.runtime.furniture.Active()&&slot&&slot->reference==reference)return false;}
 return gQ210PlayerBodyReady;
}
bool HasFo3PlayerSeat(uint32_t reference){return Fo3PlayerSeatSlot(reference)!=SIZE_MAX;}
bool BeginFo3PlayerSit(uint32_t reference){
 if(!CanFo3PlayerSit(reference))return false;
 const double now=std::chrono::duration<double>(Fo3GameplayNow().time_since_epoch()).count();
 if(gPlayerSeat.seated){gPlayerSeat.Leave(now);return true;}
 const auto index=Fo3PlayerSeatSlot(reference);Vec3 eye;if(!Q211FindAvatarHeadAnchor(eye))return false;
 fo3seat::Pose sit,stand;
 if(!fo3seat::TargetsFromSceneEye(gPlayerSeat.scene->slots[index],gVrSkeleton,{eye.x,eye.y,eye.z},SCENE_FORWARD,FLOOR_Y,FO3_UNITS_PER_METRE,sit,stand))return false;
 sit.eye=Q240ScenePosition(sit.eye);stand.eye=Q240ScenePosition(stand.eye);sit.bodyDrop/=FO3_UNITS_PER_METRE;
 return gPlayerSeat.Begin(reference,index,sit,stand,now);
}
