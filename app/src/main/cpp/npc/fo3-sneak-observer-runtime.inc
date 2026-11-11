static bool Q230InSightCone(const Q230ActorVisual& actor,const fo3sneak::Point& point,float degrees){
 const float dx=point[0]-actor.runtime.position[0],dz=point[2]-actor.runtime.position[2];
 const float bearing=std::atan2(-dx,-dz);
 const float halfCone=degrees*3.14159265359f/360;
 return std::hypot(dx,dz)<.01f||
   std::abs(fo3npc::Angle(bearing-actor.runtime.yaw-actor.runtime.headYaw))<=halfCone;
}
// Loaded player/observer evidence, on the NPC's existing think cadence.
static void Q230ObservePlayer(Q230ActorVisual& actor,bool hostile,double now){
 auto& observer=actor.playerDetection;if(now<observer.next)return;
 auto& player=gPlayerSession->player;const auto& settings=player.Definitions().sneakSettings;
 const auto* stats=player.ActorStatistics(actor.source.refFormId);if(!stats)return;
 const fo3sneak::Point point{gQ210Head[0],gQ210Head[1],gQ210Head[2]};
 const auto distance=[&](const fo3sneak::Point& source){
   const auto& pos=actor.runtime.position;
   return std::hypot(source[0]-pos[0],source[1]-pos[1],source[2]-pos[2])*FO3_UNITS_PER_METRE;
 };
 const bool exterior=gExteriorWorldspaceQ1890!=0;
 const float limit=fo3sneak::Setting(settings,"fSneakMaxDistance")*(exterior?fo3sneak::Setting(settings,"fSneakExteriorDistanceMult"):1);
 const float playerDistance=distance(point);
 const bool sight=playerDistance<limit&&
   Q230InSightCone(actor,point,fo3sneak::Setting(settings,"fDetectionViewCone"))&&Q230Sight(actor,0x14,point);
 const float actionTime=fo3sneak::Setting(settings,"fDetectionActionTimer");
 const auto available=[&](const fo3sneak::Noise& noise){
   return hostile&&noise.strength>0&&noise.time>observer.heardAt&&noise.time>=observer.evidenceAt&&
          now<noise.time+actionTime&&distance(noise.position)<limit;
 };
 const bool shot=available(gPlayerDetectionShot),step=available(gPlayerDetectionStep);
 if(!sight&&!shot&&!step){
   observer.Step(settings,-INFINITY,hostile,now,point);
   player.SetScriptActorDetectedPlayer(actor.source.refFormId,false);
   return;
 }
 fo3sneak::Evidence e;e.distance=playerDistance;
 e.exterior=exterior;e.perception=stats->perception;player.ActorAIValue(actor.source.refFormId,6,e.perception);
 e.skill=player.EffectiveSkill(size_t(fo3player::Skill::Sneak));player.ActorValue(fo3player::PlayerRef,49,e.chameleon);e.sneaking=player.IsSneaking();
 const float walkSpeed=player.Definitions().npcBaseSpeed/FO3_UNITS_PER_METRE;
 const float sneakWalk=fo3sneak::Setting(settings,"fMoveSneakMult");
 e.alert=observer.phase!=fo3sneak::Phase::Normal;
 e.otherCombat=actor.runtime.activity==fo3npc::Activity::Combat&&actor.runtime.combatTarget!=0x14;
 const auto* furniture=actor.runtime.furniture.Selected();e.sleeping=furniture&&furniture->sleep&&actor.runtime.furniture.phase==fo3furniture::Phase::Loop;
 for(const auto& stack:player.Snapshot().inventory)if(stack.equipped){const auto item=player.Definitions().items.find(stack.formId);if(item!=player.Definitions().items.end()&&item->second.kind==fo3player::ItemKind::Armour&&(item->second.bipedMask&4))e.armourWeight+=item->second.weight;}
 float sightScore=-INFINITY;
 if(sight){
   auto visual=e;visual.los=true;visual.light=Q230PlayerLightLevel(point,now);
   const float walking=walkSpeed*(visual.sneaking?sneakWalk:1);
   visual.moving=gPlayerDetectionSpeed>.05f;visual.running=visual.moving&&walking>0&&gPlayerDetectionSpeed>walking;
   sightScore=fo3sneak::Value(settings,visual);
 }
 const fo3sneak::Noise* heard=nullptr;
 float heardScore=fo3sneak::Setting(settings,"fSneakNoticedMin");
 const auto hear=[&](const fo3sneak::Noise& noise,bool footstep){
   auto audio=e;audio.distance=distance(noise.position);
   audio.sneaking=noise.sneaking;
   const float walking=walkSpeed*(audio.sneaking?sneakWalk:1);
   audio.moving=footstep;audio.running=footstep&&walking>0&&noise.strength>walking;
   audio.action=footstep?0:noise.strength;
   // Sightline affects attenuation, but sound never contributes a visual term.
   audio.soundClear=Q230Sight(actor,0x14,noise.position);
   const float score=fo3sneak::Value(settings,audio);
   if(score>heardScore){heardScore=score;heard=&noise;}
 };
 if(sightScore<=fo3sneak::Setting(settings,"fSneakNoticedMin")){
   if(shot)hear(gPlayerDetectionShot,false);
   if(step)hear(gPlayerDetectionStep,true);
 }
 observer.Step(settings,sightScore,hostile,now,point,heard);
 player.SetScriptActorDetectedPlayer(actor.source.refFormId,observer.detected);
}
