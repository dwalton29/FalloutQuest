// Loaded player/observer evidence, on the NPC's existing think cadence.
static void Q230ObservePlayer(Q230ActorVisual& actor,bool hostile,double now){
 auto& observer=actor.playerDetection;if(now<observer.next)return;
 auto& player=gPlayerSession->player;const auto& settings=player.Definitions().sneakSettings;
 const auto* stats=player.ActorStatistics(actor.source.refFormId);if(!stats)return;
 const std::array<float,3> point{gQ210Head[0],gQ210Head[1],gQ210Head[2]};
 const auto& pos=actor.runtime.position;const float dx=point[0]-pos[0],dy=point[1]-pos[1],dz=point[2]-pos[2];
 const float planar=std::hypot(dx,dz),bearing=std::atan2(-dx,-dz);
 const float cone=fo3sneak::Setting(settings,"fDetectionViewCone")*.5f*3.14159265359f/180;
 const bool front=planar<.01f||std::abs(fo3npc::Angle(bearing-actor.runtime.yaw-actor.runtime.headYaw))<=cone;
 const bool los=front&&Q230Sight(actor,0x14,point);
 fo3sneak::Evidence e;e.distance=std::sqrt(dx*dx+dy*dy+dz*dz)*FO3_UNITS_PER_METRE;
 e.exterior=gExteriorWorldspaceQ1890!=0;e.los=los;e.light=Q230PlayerLightLevel(point);e.perception=stats->perception;
 e.skill=player.Snapshot().skills[size_t(fo3player::Skill::Sneak)];e.sneaking=player.IsSneaking();e.moving=gPlayerDetectionSpeed>.05f;
 const float walking=player.Definitions().npcBaseSpeed/FO3_UNITS_PER_METRE*(e.sneaking?fo3sneak::Setting(settings,"fMoveSneakMult"):1);
 e.running=e.moving&&walking>0&&gPlayerDetectionSpeed>walking;e.alert=observer.phase!=fo3sneak::Phase::Normal;
 e.otherCombat=actor.runtime.activity==fo3npc::Activity::Combat&&actor.runtime.combatTarget!=0x14;
 const auto* furniture=actor.runtime.furniture.Selected();e.sleeping=furniture&&furniture->sleep&&actor.runtime.furniture.phase==fo3furniture::Phase::Loop;
 for(const auto& stack:player.Snapshot().inventory)if(stack.equipped){const auto item=player.Definitions().items.find(stack.formId);if(item!=player.Definitions().items.end()&&item->second.kind==fo3player::ItemKind::Armour&&(item->second.bipedMask&4))e.armourWeight+=item->second.weight;}
 e.action=now<=gPlayerDetectionActionUntil?gPlayerDetectionAction:0;
 observer.Step(settings,fo3sneak::Value(settings,e),hostile,now,point);
 if(hostile&&observer.phase!=fo3sneak::Phase::Normal&&(observer.detected||observer.value>fo3sneak::Setting(settings,"fSneakNoticedMin")))actor.runtime.lastThreat=observer.lastKnown;
}
