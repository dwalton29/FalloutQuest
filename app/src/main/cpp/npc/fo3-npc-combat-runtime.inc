// Resident actor simulation. Immutable assets are prepared in the scene worker.
// Nonresident actors retain canonical game-coordinate state and do not advance.
#ifndef FO3_NPC_COMBAT_HOST_TEST
static fo3anim::Matrix Q230WeaponMatrix(Q230ActorVisual& actor) {
  Q230UpdateActor(actor);
  const int bone=fo3anim::FindBone(actor.skeleton,"Weapon");
  if(bone<0||actor.rigs.empty())return fo3anim::Identity();
  auto inverseBasis=fo3anim::Identity();
  inverseBasis[0]=FO3_UNITS_PER_METRE;inverseBasis[5]=inverseBasis[10]=0;
  inverseBasis[6]=FO3_UNITS_PER_METRE;inverseBasis[9]=-FO3_UNITS_PER_METRE;
  auto offset=fo3anim::Identity();offset[13]=-FLOOR_Y;offset[14]=-SCENE_FORWARD;
  return fo3anim::Multiply(fo3anim::Multiply(actor.dialogueRoot,
    fo3anim::Multiply(actor.rigs[0].scenePlacement,actor.pose.global[bone])),fo3anim::Multiply(inverseBasis,offset));
}
static void Q230RenderWeapon(Q230ActorVisual& actor,bool alpha) {
  if(!gPlayerSession||actor.runtime.activity!=fo3npc::Activity::Combat)return;
  const auto* owned=gPlayerSession->player.ActorWeapon(actor.source.refFormId);if(!owned)return;
  const auto prepared=actor.combatWeapons.find(owned->formId);if(prepared==actor.combatWeapons.end())return;
  const auto transform=Q230WeaponMatrix(actor);
  for(size_t index:prepared->second.gpu){auto& o=actor.objects[index];if(o.alphaBlend!=alpha)continue;
    std::copy(transform.begin(),transform.end(),o.q220DynamicTransform);
    PrepareFo3InteriorObjectLights(o,nullptr);glEnable(GL_DEPTH_TEST);glDepthMask(o.zBufferWriteQ1200?GL_TRUE:GL_FALSE);
    if(alpha){glEnable(GL_BLEND);glBlendFunc(Q1150BlendFactor(o.alphaSourceBlend,true),Q1150BlendFactor(o.alphaDestBlend,false));}
    DrawSceneObject(o);
  }
}
#endif
static bool Q230Target(uint32_t reference,std::array<float,3>& point,uint32_t& base) {
  if(!gPlayerSession||gPlayerSession->player.ActorHealth(reference)<=0)return false;
  if(reference==0x14){base=7;
#ifdef FO3_NPC_COMBAT_HOST_TEST
    point={gQ210Head[0],gQ210Head[1],gQ210Head[2]};return true;
#else
    // VR has no rendered player head. Aim at the original posed torso bone,
    // which is inside the same authored skin used by canonical shot collision.
    for(const auto& part:gQ211PlayerRigParts)for(size_t i=0;i<part.boneNames.size();++i){
      if(part.boneNames[i]!="bip01 spine2")continue;
      const auto mapped=std::find(part.skin.mapping.sources.begin(),part.skin.mapping.sources.end(),int(i));
      if(mapped==part.skin.mapping.sources.end())continue;
      const size_t slot=size_t(mapped-part.skin.mapping.sources.begin())+1;
      if(slot>=part.skin.palette.size())continue;
      const auto bind=Q211BindBonePoint(part.bones[i]);const std::array<float,3> pivot{bind.x,bind.y,bind.z};
      const auto posed=fqskin::Transform(part.skin.palette[slot].data(),pivot.data(),false);
      const auto world=Q211TransformPoint(gQ210PlayerRoot,{posed[0],posed[1],posed[2]});
      point={world.x,world.y,world.z};return true;
    }
    // Actual tracked headset pose is a truthful perception/aiming target even
    // when the optional posed torso mapping is unavailable. Projectile
    // damage still requires collision with the canonical skinned player body.
    static bool loggedFallback=false;
    if(std::isfinite(gQ210Head[0])&&std::isfinite(gQ210Head[1])&&
       std::isfinite(gQ210Head[2])&&gSceneReady){
      point={gQ210Head[0],gQ210Head[1],gQ210Head[2]};
      if(!loggedFallback){loggedFallback=true;
        Q6H_LOGI("NPC COMBAT PLAYER TARGET source=tracked-head reason=spine2-unavailable rigParts=%zu",
                 gQ211PlayerRigParts.size());}
      return true;
    }
    static bool loggedMissing=false;
    if(!loggedMissing){loggedMissing=true;
      Q6H_LOGI("NPC COMBAT PLAYER TARGET UNAVAILABLE reason=no-posed-torso-or-tracked-head rigParts=%zu",
               gQ211PlayerRigParts.size());}
    return false;
#endif
  }
  for(auto& target:gQ230NpcActors)if(target.source.refFormId==reference&&!target.runtime.offScene&&target.runtime.Alive()){
    Q230UpdateActor(target);base=target.source.baseFormId;
    return Q230LiveBone(target,target.chestBone,point);
  }
  return false;
}
static bool Q230Sight(Q230ActorVisual& actor,uint32_t target,const std::array<float,3>& point) {
  Q230UpdateActor(actor);std::array<float,3> eye{};
  if(!Q230LiveBone(actor,actor.headBone,eye)){
    Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"combat perception: no skinned head bone");
    return false;
  }
  const float dx=point[0]-eye[0],dy=point[1]-eye[1],dz=point[2]-eye[2],length=std::sqrt(dx*dx+dy*dy+dz*dz);
  return length>.01f&&!HasFo3InteractionOccluder(eye[0],eye[1],eye[2],dx/length,dy/length,dz/length,length,target);
}
static void Q230PersistActor(Q230ActorVisual& actor) {
  if(!gPlayerSession||!actor.stateRestored||actor.runtime.offScene)return;
  auto& p=gPlayerSession->player;fo3player::ActorState saved;
  saved.cell=gCurrentCellFormId;saved.world=gExteriorWorldspaceQ1890;
  if(actor.navigationGraph&&actor.aiPathIndex<actor.aiPathSurfaces.size()){
    const auto face=actor.aiPathSurfaces[actor.aiPathIndex];
    const auto cell=actor.navigationGraph->meshes[face.first]->cellFormId;if(cell)saved.cell=cell;
  }
  saved.position=Q240GamePosition(actor);saved.yaw=actor.runtime.yaw;
  saved.package=actor.aiPackage;saved.sequence=actor.aiSequence;
  const auto package=p.Definitions().pipboy.packages.find(actor.aiPackage);
  if(package!=p.Definitions().pipboy.packages.end()&&package->second.type==13&&actor.runtime.Alive())
    saved.packageWaitSeconds=actor.runtime.activity==fo3npc::Activity::Combat?actor.runtime.suspendedPackageWait:
      (actor.aiPathGame.empty()?float(std::max(0.,actor.aiRepathAt-std::max(0.,actor.aiLastUpdate))):0);
  const auto old=p.Snapshot().actors.find(actor.source.refFormId);
  saved.hostile=old==p.Snapshot().actors.end()?0:old->second.hostile;
  saved.dead=(old!=p.Snapshot().actors.end()&&old->second.dead)||(p.ActorHealth(actor.source.refFormId)==0&&!p.Essential(actor.source.refFormId));
  const auto* weapon=p.ActorWeapon(actor.source.refFormId);saved.equippedWeapon=weapon?weapon->id:0;
  p.UpdateActor(actor.source.refFormId,saved);
}
static const fo3pipdata::PackageDefinition* Q230CurrentPackage(const Q230ActorVisual& actor) {
  const auto& packages=gPlayerSession->player.Definitions().pipboy.packages;
  const uint32_t id=actor.aiPackage?actor.aiPackage:actor.runtime.package;
  const auto found=packages.find(id);return found==packages.end()?nullptr:&found->second;
}
static const fo3pipdata::PackageDefinition* Q230PerceptionPackage(Q230ActorVisual& actor,uint32_t& id,std::array<float,3>& anchor) {
  if(actor.runtime.dialogue||actor.runtime.activity==fo3npc::Activity::Combat){id=actor.aiPackage?actor.aiPackage:actor.runtime.package;return Q230CurrentPackage(actor);}
  float radius=0;return Q240SelectPackage(actor,id,anchor,radius);
}
static const fo3weapon::Definitions::CombatStyle* Q230Style(const Q230ActorVisual& actor) {
  const auto& c=gPlayerSession->player.Definitions();const auto* package=Q230CurrentPackage(actor);
  uint32_t id=package?package->combatStyle:0;
  if(!id){const auto* traits=fo3pipdata::ActorCategory(c.pipboy,actor.source.baseFormId,1);if(!traits)return nullptr;id=traits->combatStyle;}
  if(!id)id=c.weapons.defaultCombatStyle;
  const auto style=c.weapons.styles.find(id);return style==c.weapons.styles.end()||!style->second.valid?nullptr:&style->second;
}
// Only the supported actor-reference Follow/Escort families have a leader.
// The evidence is canonical damage hostility or a resident actor currently
// targeting that leader. Ordinary faction assistance does not bypass Defensive.
static bool Q230DefendsLeader(const Q230ActorVisual& actor,const fo3pipdata::PackageDefinition* package,uint32_t attacker) {
  if(!package||(package->type!=1&&package->type!=2)||!package->target.valid||package->target.type!=0)return false;
  const uint32_t leader=package->target.value;if(!leader||leader==actor.source.refFormId||attacker==leader)return false;
  std::array<float,3> position{};if(!Q240PackageActorTarget(actor,leader,position))return false;
  const auto& states=gPlayerSession->player.Snapshot().actors;const auto state=states.find(leader);
  if(state!=states.end()&&state->second.hostile==attacker)return true;
  for(const auto& threat:gQ230NpcActors)if(threat.source.refFormId==attacker&&threat.runtime.Alive()&&
      threat.runtime.activity==fo3npc::Activity::Combat&&threat.runtime.combatTarget==leader)return true;
  return false;
}
static bool Q230SelectWeapon(Q230ActorVisual& actor,float targetRange=NAN) {
  auto& p=gPlayerSession->player;const auto* contents=p.ContainerContents(actor.source.refFormId);if(!contents)return false;
  const auto* style=Q230Style(actor);const auto* package=Q230CurrentPackage(actor);
  if(package&&(!package->combatStyleValid||(package->combatStyle&&!style))){Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"unsupported package CSTY "+std::to_string(package->combatStyle));return false;}
  const auto* current=p.ActorWeapon(actor.source.refFormId);
  if(current&&current->id==actor.runtime.equippedWeapon&&
     (actor.runtime.pendingAttack||actor.runtime.reloadUntil>0||(actor.runtime.actionUntil>0&&actor.runtime.actionUntil>actor.aiLastUpdate))){
    const auto prepared=actor.combatWeapons.find(current->formId);
    if(prepared!=actor.combatWeapons.end()&&prepared->second.ready&&!prepared->second.gpu.empty())return true;
  }
  bool bestInRange=false;
  float best=-1;uint64_t selected=0;uint32_t form=0;
  for(const auto& s:*contents){const auto prepared=actor.combatWeapons.find(s.formId);
    if(prepared==actor.combatWeapons.end()||!prepared->second.ready||prepared->second.gpu.empty()||s.condition<=0)continue;
    const auto& d=prepared->second.definition;const bool ranged=d.Firearm();
    if(style&&((style->restrictions==1&&ranged)||(style->restrictions==2&&!ranged)))continue;
    if(ranged&&!s.loadedRounds){bool reserve=false;for(const auto& ammo:*contents)if(ammo.formId==d.ammo&&ammo.count>=d.ammoUse)reserve=true;if(!reserve)continue;}
    const auto& clip=prepared->second.clips[1];
    const float duration=(clip.stop-clip.start)/std::max(.01f,d.attackMult);
    const float rate=ranged?(d.Automatic()?d.rate:d.shotsPerSecond):(duration>0?1.f/duration:1.f);
    const bool inRange=!std::isfinite(targetRange)||targetRange<=d.maxRange*(ranged&&style?style->rangeMax:1.f);
    const float score=d.damage*s.condition*std::max(rate,.01f);
    if((inRange&&!bestInRange)||(inRange==bestInRange&&score>best)){best=score;bestInRange=inRange;selected=s.id;form=s.formId;}
  }
  if(!selected)return false;
  if(actor.runtime.equippedWeapon==selected&&p.ActorWeapon(actor.source.refFormId))return true;
  if(!p.EquipActorWeapon(actor.source.refFormId,selected))return false;
  actor.runtime.equippedWeapon=selected;actor.runtime.reloadUntil=0;actor.runtime.pendingAttack=false;
  actor.runtime.nextMeleeDecision=actor.runtime.nextAttack=actor.runtime.burstUntil=actor.runtime.burstWaitUntil=actor.runtime.actionUntil=0;
  actor.runtime.action=fo3npc::CombatAction::Acquire;
  const auto& clips=actor.combatWeapons.at(form).clips;
  for(size_t i=0;i<4;++i)actor.animations[size_t(fo3npc::Animation::Aim)+i]=clips[i];
  actor.runtime.animation=fo3npc::Animation::Aim;++actor.runtime.actionSerial;actor.lastFrame=UINT64_MAX;
  fo3audio::SoundEvent(actor.combatWeapons.at(form).definition.equip);
  Q6H_LOGI("NPC EQUIP actor=%08X weapon=%08X instance=%llu",actor.source.refFormId,form,(unsigned long long)selected);return true;
}
static void Q230ActionSounds(Q230ActorVisual& actor,double now) {
  const auto slot=size_t(actor.runtime.animation!=fo3npc::Animation::Reload&&(actor.runtime.pendingAttack||now<actor.runtime.actionUntil)?fo3npc::Animation::Attack:actor.runtime.animation);if(slot>=actor.animations.size())return;
  const float previous=float(actor.runtime.lastActionTime-actor.runtime.actionStart),current=float(now-actor.runtime.actionStart);
  for(const auto& key:actor.animations[slot].textKeys)if(key.time>previous&&key.time<=current&&key.text.rfind("Sound: ",0)==0)
    fo3audio::NamedSound(key.text.substr(7));
  actor.runtime.lastActionTime=now;
}
static bool Q230CombatRoute(Q230ActorVisual& actor,const std::array<float,3>& target,bool flee,double now) {
  if(!actor.navigationGraph||actor.navigationGraph->triangleCount==0||now<actor.runtime.nextPath)return false;
  if(actor.aiPathIndex<actor.aiPathGame.size()&&Q240PlanarDistance(actor.runtime.pathDestinationGame,target)<16.f)return false;
  actor.runtime.tacticalRouteKind=0;actor.runtime.nextDoorQuery=0;actor.runtime.navigationDoor=0;actor.runtime.pathDestinationGame=target;
  actor.runtime.nextPath=now+.75; // bounded resident replanning policy
  auto destination=target;const auto current=Q240GamePosition(actor);const auto& graph=*actor.navigationGraph;
  if(flee&&!Q240FleeDestination(graph,current,target,INFINITY,destination)){
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;actor.runtime.speed=0;
    Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"no farther reachable flee endpoint");return false;
  }
  std::string failure;actor.aiPathIndex=0;
  if(!Q240ActorPath(actor,graph,current,destination,actor.aiPathGame,&failure,&actor.aiPathSurfaces)){
    Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"combat route: "+failure);return false;
  }
  Q6H_LOGI("NPC PATH actor=%08X target=%08X flee=%d waypoints=%zu",actor.source.refFormId,actor.runtime.combatTarget,flee,actor.aiPathGame.size());
  return true;
}
// Bounded Quest tactical selection over reachable original NAVM surfaces.
// Cover is tested against live geometry; no cover markers or animations are
// fabricated. Endpoint scoring is a standalone policy, not Bethesda parity.
static bool Q230TacticalRoute(Q230ActorVisual& actor,const std::array<float,3>& threat,
                             float minimum,float maximum,bool escape,bool seekCover,double now,int direction=0){
  auto& state=actor.runtime;const uint8_t routeKind=escape?1:seekCover?2:3;
  if(!actor.navigationGraph||!actor.navigationGraph->triangleCount)return false;
  if((state.action==fo3npc::CombatAction::Flee||state.combatMovement==fo3npc::CombatAction::Cover||state.combatMovement==fo3npc::CombatAction::Reposition)&&actor.aiPathIndex<actor.aiPathGame.size()&&
     state.tacticalRouteKind==routeKind&&Q240PlanarDistance(state.pathDestinationGame,threat)<70.f)return true;
  if(now<state.nextPath)return false;
  actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
  state.nextPath=now+.75;
  const auto& graph=*actor.navigationGraph;const auto current=Q240GamePosition(actor);
  const auto start=Q240NearestNode(graph,current);if(!start.Valid())return false;
  const auto& catalog=gPlayerSession->player.Definitions();const auto* style=Q230Style(actor);
  const float search=seekCover&&style&&style->coverRadius>0?style->coverRadius:
    fo3npc::CombatSetting(catalog,"fCombatFleeMaxCoverDistance",0);
  const float before=Q240PlanarDistance(current,threat);
  std::vector<uint8_t> visited(graph.triangleCount,0);std::vector<Q240Node> connected{start};
  visited[Q240Flat(graph,start)]=1;
  std::vector<std::pair<float,std::array<float,3>>> candidates;
  for(size_t i=0;i<connected.size();++i){
    const auto node=connected[i];
    Q240Neighbors(graph,node,[&](Q240Node next){const size_t flat=Q240Flat(graph,next);std::array<float,3> portal{};
      if(flat<visited.size()&&!visited[flat]&&Q240Portal(graph,node,next,portal)){visited[flat]=1;connected.push_back(next);}
    });
    const auto point=Q240Centroid(graph,node);const float travel=Q240PlanarDistance(current,point);
    if(travel<70.f||(search>0&&travel>search))continue;
    const float separation=Q240PlanarDistance(point,threat);
    if(direction){
      const float tx=threat[0]-current[0],ty=threat[1]-current[1],px=point[0]-current[0],py=point[1]-current[1];
      const float side=std::abs(direction)==1?px*ty-py*tx:px*tx+py*ty;
      if(side*(direction>0?1.f:-1.f)<=0)continue;
    }
    if(escape&&separation<=before+16.f)continue;
    if(!escape&&(separation<minimum||separation>maximum))continue;
    // Occupancy is a strict exclusion, including another selected destination.
    bool occupied=false;
    for(const auto& other:gQ230NpcActors){
      if(other.source.refFormId==actor.source.refFormId||other.runtime.offScene||!other.runtime.Alive())continue;
      const auto at=Q240GamePosition(other);
      if(std::fabs(at[2]-point[2])<90.f&&Q240PlanarDistance(at,point)<70.f){occupied=true;break;}
      if(other.aiPathIndex<other.aiPathGame.size()&&std::fabs(other.aiPathGame.back()[2]-point[2])<90.f&&Q240PlanarDistance(other.aiPathGame.back(),point)<70.f){occupied=true;break;}
    }
    if(occupied)continue;
    float score=travel;
    if(escape)score+=std::max(0.f,minimum-separation)*4;
    // Source assistance/relations permit retreat towards real living allies.
    if(escape)for(const auto& ally:gQ230NpcActors){
      if(ally.source.refFormId==actor.source.refFormId||ally.runtime.offScene||!ally.runtime.Alive())continue;
      if(fo3npc::Relationship(catalog,actor.source.baseFormId,ally.source.baseFormId)!=fo3npc::Reaction::Ally)continue;
      const float d=Q240PlanarDistance(point,Q240GamePosition(ally));
      const float maximumAlly=fo3npc::CombatSetting(catalog,"fCombatFleeMaxAllyDistance");
      const float minimumAlly=fo3npc::CombatSetting(catalog,"fCombatFleeMinAllyDistance");
      if(maximumAlly>0&&d>=minimumAlly&&d<=maximumAlly)score-=std::min(travel,minimum)*.25f;
    }
    candidates.push_back({score,point});
  }
  std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.first<b.first;});
  // Bound expensive world rays to the best geometric candidates per replan.
  // This is a Quest work budget, not an original combat-style setting.
  candidates.resize(std::min(size_t(32),candidates.size()));
  const auto enemy=Q240ScenePosition(threat);std::array<float,3> chest{};
  const bool hasChest=Q230LiveBone(actor,actor.chestBone,chest);
  std::vector<float> heights;if(hasChest)heights.push_back(chest[1]-state.position[1]);
  std::array<float,3> head{};if(Q230LiveBone(actor,actor.headBone,head))heights.push_back(head[1]-state.position[1]);
#ifndef FO3_NPC_COMBAT_HOST_TEST
  const int pelvisBone=fo3anim::FindBone(actor.skeleton,"Bip01 Pelvis");std::array<float,3> pelvis{};
  if(pelvisBone>=0&&Q230LiveBone(actor,pelvisBone,pelvis))heights.push_back(pelvis[1]-state.position[1]);
#endif
  for(auto& candidate:candidates){
    const auto scene=Q240ScenePosition(candidate.second);
    bool cover=hasChest&&!heights.empty(),firingBlocked=!hasChest;
    for(size_t i=0;i<heights.size();++i){
      // threat already contains the real target chest/head height. Raise only
      // the candidate body point; never raise the enemy origin a second time.
      const float dx=scene[0]-enemy[0],dy=scene[1]+heights[i]-enemy[1],dz=scene[2]-enemy[2];
      const float length=std::sqrt(dx*dx+dy*dy+dz*dz);
      const bool hidden=length>.01f&&HasFo3InteractionOccluder(enemy[0],enemy[1],enemy[2],dx/length,dy/length,dz/length,length,actor.source.refFormId);
      cover=cover&&hidden;if(i==0&&hasChest)firingBlocked=hidden;
    }
    if((seekCover&&!cover)||(!escape&&!seekCover&&firingBlocked))candidate.first=INFINITY;
    if(escape&&cover)candidate.first-=std::min(Q240PlanarDistance(current,candidate.second),minimum)*.5f;
  }
  std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.first<b.first;});
  for(size_t i=0;i<std::min(size_t(12),candidates.size());++i){
    if(!std::isfinite(candidates[i].first))break;
    std::vector<std::array<float,3>> route;std::vector<std::pair<size_t,size_t>> surfaces;
    if(!Q240ActorPath(actor,graph,current,candidates[i].second,route,nullptr,&surfaces)||route.empty())continue;
    // Reject escapes that initially walk toward the threat, even when their
    // endpoint is farther away. Another reachable candidate may be safer.
    if(escape&&route.size()>1&&Q240PlanarDistance(route.front(),threat)+16.f<before)continue;
    actor.aiPathGame=std::move(route);actor.aiPathSurfaces=std::move(surfaces);actor.aiPathIndex=0;
    state.tacticalRouteKind=routeKind;state.pathDestinationGame=threat;state.navigationDoor=0;state.blockedSince=-1;
    Q6H_LOGI("NPC TACTIC actor=%08X target=%08X escape=%d cover=%d waypoints=%zu",actor.source.refFormId,state.combatTarget,escape,seekCover,actor.aiPathGame.size());return true;
  }
  return false;
}
static void Q230SimulateActor(Q230ActorVisual& actor,double now) {
  if(!gPlayerSession||gPlayerSession->saveBlocked||actor.runtime.offScene||actor.rigs.empty()||IsFo3LoadingVisible())return;
  auto& p=gPlayerSession->player;const auto& c=p.Definitions();auto& s=actor.runtime;
  if(!actor.stateRestored){
    const auto saved=p.Snapshot().actors.find(actor.source.refFormId);
    const bool restored=saved!=p.Snapshot().actors.end();
    if(saved!=p.Snapshot().actors.end()){
      s.position=Q240ScenePosition(saved->second.position);s.yaw=s.returnYaw=saved->second.yaw;
      actor.aiPackage=s.package=saved->second.package;actor.aiSequence=saved->second.sequence;
      actor.aiRepathAt=now+saved->second.packageWaitSeconds;
      // Rebuild a route to the authored destination after restoration.
      const auto package=c.pipboy.packages.find(actor.aiPackage);
      if(package==c.pipboy.packages.end()||(package->second.type!=13&&package->second.type!=2&&package->second.type!=10))actor.aiSequence=0;
    }
    actor.stateRestored=true;actor.aiLastUpdate=now;p.PrepareActorInventory(actor.source.refFormId);Q230PersistActor(actor);actor.lastFrame=UINT64_MAX;
    const auto* packages=fo3pipdata::ActorCategory(c.pipboy,actor.source.baseFormId,16);
    Q6H_LOGI("NPC AI START actor=%08X base=%08X packages=%zu navTriangles=%zu health=%.1f restored=%d",
      actor.source.refFormId,actor.source.baseFormId,packages?packages->packages.size():0u,
      actor.navigationGraph?actor.navigationGraph->triangleCount:0u,p.ActorHealth(actor.source.refFormId),restored);
  }
  const float health=p.ActorHealth(actor.source.refFormId);
  if(health<0){Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"actor statistics unresolved; AI suspended, not dead");return;}
  if(health==0){
    if(s.activity!=fo3npc::Activity::Dying&&s.activity!=fo3npc::Activity::Dead){
      if(s.dialogue)EndFo3Dialogue("actor death");s.Die(now);actor.lastFrame=UINT64_MAX;
      actor.aiPathGame.clear();s.pendingAttack=false;s.reloadUntil=0;
      p.PrepareContainer(actor.source.refFormId);
      Q6H_LOGI("NPC DEATH actor=%08X essential=0",actor.source.refFormId);
    }
    if(s.activity==fo3npc::Activity::Dying&&now-s.deathAt>=std::max(.01f,actor.animations[size_t(fo3npc::Animation::Death)].stop)){
      s.activity=fo3npc::Activity::Dead;actor.lastFrame=UINT64_MAX;Q6H_LOGI("NPC CORPSE actor=%08X",actor.source.refFormId);
    }
    return;
  }
  // Preserve an authored short hit response before selecting the next combat action.
  if(now<s.hitUntil){s.speed=0;s.animation=fo3npc::Animation::Hit;return;}
  const auto ai=fo3npc::AI(c.pipboy,actor.source.baseFormId);
  if(!ai.valid){Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"unsupported AIDT");return;}
  if(now>=s.nextThink){
    s.nextThink=now+.25;
    uint32_t policyId=0;std::array<float,3> policyAnchor{};
    const auto* policy=Q230PerceptionPackage(actor,policyId,policyAnchor);
    const bool defensive=fo3npc::Defensive(policy);
    uint32_t acquire=0;std::array<float,3> point{};uint32_t base=0;
    const auto saved=p.Snapshot().actors.find(actor.source.refFormId);
    if(saved!=p.Snapshot().actors.end()&&saved->second.hostile&&Q230Target(saved->second.hostile,point,base))acquire=saved->second.hostile;
    if(!acquire&&s.activity!=fo3npc::Activity::Combat){
      float nearest=ai.radiusEnabled?float(ai.radius):c.weapons.detectionDistance;
      auto candidate=[&](uint32_t ref){
        if(ref==actor.source.refFormId||!Q230Target(ref,point,base))return;
        bool hostile=defensive?Q230DefendsLeader(actor,policy,ref):fo3npc::Acquires(ai,fo3npc::Relationship(c,actor.source.baseFormId,base));
        if(!defensive&&!hostile&&ref!=0x14){for(const auto& ally:gQ230NpcActors)if(ally.runtime.combatTarget==ref&&fo3npc::Assists(ai,fo3npc::Relationship(c,actor.source.baseFormId,ally.source.baseFormId)))hostile=true;}
        const float dx=point[0]-s.position[0],dy=point[1]-s.position[1],dz=point[2]-s.position[2];
        const float range=std::sqrt(dx*dx+dy*dy+dz*dz)*FO3_UNITS_PER_METRE;
        if(hostile&&range<nearest&&Q230Sight(actor,ref,point)){nearest=range;acquire=ref;}
      };
      candidate(0x14);for(const auto& other:gQ230NpcActors)candidate(other.source.refFormId);
    }
    if(acquire&&acquire!=s.combatTarget){
      if(policy&&s.activity!=fo3npc::Activity::Combat&&!s.dialogue)Q240AdoptPackage(actor,policyId,*policy,policyAnchor);
      if(s.dialogue)EndFo3Dialogue("combat threat");
      if(s.activity!=fo3npc::Activity::Combat)Q240SuspendPackageWait(actor,now);
      s.BeginCombat(acquire);s.lastSeen=now;s.nextPath=0;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;s.navigationDoor=0;
      Q6H_LOGI("NPC COMBAT START actor=%08X target=%08X confidence=%u assets=%zu",
        actor.source.refFormId,acquire,unsigned(ai.confidence),actor.combatWeapons.size());
    }
  }
  if(s.activity!=fo3npc::Activity::Combat)return;
  if(s.furniture.Active())return; // Original exit owns the pose before combat movement/fire.
  std::array<float,3> target{};uint32_t targetBase=0;
  const bool valid=Q230Target(s.combatTarget,target,targetBase),visible=valid&&Q230Sight(actor,s.combatTarget,target);
  if(visible){s.lastThreat=target;s.lastSeen=now;}
  // Bethesda's complete detection/search state is not decoded. This bounded
  // loaded-scene bridge searches for 15 seconds; absence/death ends immediately.
  if(!valid||(!visible&&now-s.lastSeen>15&&!(s.moraleFlee&&now<s.fleeUntil)&&s.combatMovement!=fo3npc::CombatAction::Cover)){
    const auto old=s.combatTarget;s.EndCombat();s.pendingAttack=false;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;s.navigationDoor=0;const auto package=c.pipboy.packages.find(actor.aiPackage);if(package==c.pipboy.packages.end()||(package->second.type!=13&&package->second.type!=2&&package->second.type!=10))actor.aiSequence=0;Q240ResumePackageWait(actor,now);
    p.SetActorHostile(actor.source.refFormId,0);Q6H_LOGI("NPC COMBAT END actor=%08X target=%08X",actor.source.refFormId,old);return;
  }
  const float dt=actor.aiLastUpdate<0?0:std::clamp(float(now-actor.aiLastUpdate),0.f,.1f);actor.aiLastUpdate=now;
  const float targetRange=std::hypot(target[0]-s.position[0],target[2]-s.position[2])*FO3_UNITS_PER_METRE;
  const bool armed=ai.confidence!=0&&Q230SelectWeapon(actor,targetRange);
  if(now>=s.nextTactic){
    s.nextTactic=now+1;
    const auto damage=p.Snapshot().actorDamage.find(actor.source.refFormId);
    const float maximum=health+(damage==p.Snapshot().actorDamage.end()?0:damage->second);
    const bool wantsFlee=fo3npc::ShouldFlee(ai,c,health,maximum,armed);
    if(wantsFlee&&!s.moraleFlee){
      s.moraleFlee=true;s.fleeUntil=now+fo3npc::CombatSetting(c,"fCombatFleeWaitTime");
      s.nextPath=0;actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;
      Q6H_LOGI("NPC MORALE actor=%08X confidence=%u health=%.3f max=%.3f armed=%d flee=1",actor.source.refFormId,unsigned(ai.confidence),health,maximum,armed);
    }else if(!wantsFlee&&now>=s.fleeUntil)s.moraleFlee=false;
  }
  if(s.moraleFlee&&!s.pendingAttack&&s.reloadUntil<=0){
    if(s.action!=fo3npc::CombatAction::Flee){Q6H_LOGI("NPC FLEE actor=%08X target=%08X",actor.source.refFormId,s.combatTarget);s.action=fo3npc::CombatAction::Flee;}
    const auto threat=std::array<float,3>{gSceneCenterXQ1730+s.lastThreat[0]*FO3_UNITS_PER_METRE,gSceneCenterYQ1730-(s.lastThreat[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,gSceneFloorZQ1730+(s.lastThreat[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
    const float safe=fo3npc::CombatSetting(c,"fCombatFleeNormalDistance",INFINITY);
    if(Q240PlanarDistance(Q240GamePosition(actor),threat)>=safe){
      actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;s.speed=0;s.animation=fo3npc::Animation::Idle;return;
    }
    // Source-aware escape selection; preserve the existing farthest route only
    // when the optional source distance settings are absent.
    const bool routed=std::isfinite(safe)?Q230TacticalRoute(actor,threat,safe,INFINITY,true,false,now):Q230CombatRoute(actor,threat,true,now);
    if(!routed&&actor.aiPathGame.empty())Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"combat flee has no safe reachable NAVM escape");
    Q240AdvancePath(actor,dt,now);return;
  }
  if(!armed){
    Q240AiLogOnce(actor.source.refFormId,actor.aiPackage,"no usable supported combat weapon; attempting real NAVM cover");
    const std::array<float,3> game{gSceneCenterXQ1730+target[0]*FO3_UNITS_PER_METRE,gSceneCenterYQ1730-(target[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,gSceneFloorZQ1730+(target[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
    s.action=fo3npc::CombatAction::Cover;
    Q230TacticalRoute(actor,game,0,INFINITY,false,true,now);Q240AdvancePath(actor,dt,now);return;
  }
  const auto* owned=p.ActorWeapon(actor.source.refFormId);if(!owned)return;
  const uint64_t instance=owned->id;const uint32_t form=owned->formId;
  const auto& weapon=actor.combatWeapons.at(form);const auto& d=weapon.definition;
  const auto* style=Q230Style(actor);const bool ranged=d.Firearm();
  const float dx=target[0]-s.position[0],dz=target[2]-s.position[2],distance=std::sqrt(dx*dx+dz*dz)*FO3_UNITS_PER_METRE;
  const float desired=std::atan2(-dx,-dz),error=fo3npc::Angle(desired-s.yaw);
  s.yaw=fo3npc::Angle(s.yaw+std::clamp(error,-fo3npc::MovementPolicy::TurnSpeed*dt,fo3npc::MovementPolicy::TurnSpeed*dt));s.returnYaw=s.yaw;actor.lastFrame=UINT64_MAX;
  const float maxRange=d.maxRange*(ranged&&style?style->rangeMax:1.f);
  if(s.combatMovement==fo3npc::CombatAction::Reposition&&s.manoeuvreUntil>0&&now>=s.manoeuvreUntil){
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;s.combatMovement=fo3npc::CombatAction::Acquire;s.manoeuvreUntil=0;
  }
  bool underAttack=false,recoiling=false,unarmedTarget=false;
  for(const auto& other:gQ230NpcActors)if(!other.runtime.offScene&&other.runtime.Alive()){
    if(other.runtime.pendingAttack&&other.runtime.combatTarget==actor.source.refFormId)underAttack=true;
    if(other.source.refFormId==s.combatTarget){recoiling=other.runtime.hitUntil>now;unarmedTarget=!other.runtime.equippedWeapon;}
  }
  // Decode original CSTD probabilities/timers. Mapping the selected direction
  // onto reachable NAVM endpoints is a Quest bridge, not Bethesda's solver.
  if(ranged&&visible&&style&&style->manoeuvresValid&&s.combatMovement==fo3npc::CombatAction::Acquire&&
     !s.pendingAttack&&s.reloadUntil<=0&&now>=s.nextManoeuvre&&now>=s.burstWaitUntil){
    const auto& timers=style->manoeuvreTimers;
    s.nextManoeuvre=now+std::max(.1f,fo3npc::CombatChoice(actor.source.refFormId,s.tacticSequence++,timers[6],timers[7]));
    if(fo3npc::CombatChance(actor.source.refFormId,s.tacticSequence++,fo3npc::DodgeChance(*style,underAttack))){
      const bool lateral=fo3npc::CombatChance(actor.source.refFormId,s.tacticSequence++,style->leftRightChance);
      const float positive=lateral?50.f:fo3npc::ForwardDodgeChance(*style,underAttack,s.pendingAttack);
      const int direction=(fo3npc::CombatChance(actor.source.refFormId,s.tacticSequence++,positive)?1:-1)*(lateral?1:2);
      const size_t timer=lateral?0:direction>0?2:4;
      const std::array<float,3> threat{gSceneCenterXQ1730+target[0]*FO3_UNITS_PER_METRE,gSceneCenterYQ1730-(target[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,gSceneFloorZQ1730+(target[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
      if(Q230TacticalRoute(actor,threat,d.minRange,maxRange,false,false,now,direction)){
        s.combatMovement=fo3npc::CombatAction::Reposition;s.action=s.combatMovement;
        s.manoeuvreUntil=now+std::max(.1f,fo3npc::CombatChoice(actor.source.refFormId,s.tacticSequence++,timers[timer],timers[timer+1]));
      }
    }
  }
  // Cover owns temporary loss of sight. Do not replace its route with Search.
  if(s.combatMovement==fo3npc::CombatAction::Cover&&!s.pendingAttack&&s.reloadUntil<=0){
    if(actor.aiPathIndex<actor.aiPathGame.size()){s.action=fo3npc::CombatAction::Cover;Q240AdvancePath(actor,dt,now);return;}
    if(s.coverUntil<0)s.coverUntil=now+(style?fo3npc::CombatChoice(actor.source.refFormId,s.tacticSequence++,style->waitMin,style->waitMax):0);
    if(now<s.coverUntil){s.speed=0;s.animation=fo3npc::Animation::Aim;return;}
    const std::array<float,3> threat{gSceneCenterXQ1730+s.lastThreat[0]*FO3_UNITS_PER_METRE,gSceneCenterYQ1730-(s.lastThreat[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,gSceneFloorZQ1730+(s.lastThreat[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
    s.combatMovement=fo3npc::CombatAction::Reposition;s.action=s.combatMovement;s.coverUntil=-1;s.manoeuvreUntil=0;
    actor.aiPathGame.clear();actor.aiPathSurfaces.clear();actor.aiPathIndex=0;s.nextPath=0;
    Q230TacticalRoute(actor,threat,d.minRange,maxRange,false,false,now);
  }
  bool movingFire=false;
  if(ranged&&s.combatMovement==fo3npc::CombatAction::Reposition&&actor.aiPathIndex<actor.aiPathGame.size()&&s.reloadUntil<=0){
    Q240AdvancePath(actor,dt,now);movingFire=s.speed>0;
    if(!visible){if(s.pendingAttack){s.pendingAttack=false;++s.actionSerial;}return;}
  }
  if(s.combatMovement==fo3npc::CombatAction::Reposition&&actor.aiPathIndex>=actor.aiPathGame.size())s.combatMovement=fo3npc::CombatAction::Acquire;
  if(!visible||distance>maxRange){
    // A wind-up belongs to the current line of fire. Never carry a queued
    // attack into search/pursuit and release it after regaining visibility.
    if(s.pendingAttack){s.pendingAttack=false;s.pendingHit=0;s.actionUntil=0;s.animation=fo3npc::Animation::Aim;++s.actionSerial;}
    s.combatMovement=fo3npc::CombatAction::Acquire;s.coverUntil=-1;s.manoeuvreUntil=0;
    s.action=visible?fo3npc::CombatAction::Pursue:fo3npc::CombatAction::Search;
    const auto& point=visible?target:s.lastThreat;
    const std::array<float,3> game{gSceneCenterXQ1730+point[0]*FO3_UNITS_PER_METRE,gSceneCenterYQ1730-(point[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,gSceneFloorZQ1730+(point[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
    Q230CombatRoute(actor,game,false,now);if(!movingFire)Q240AdvancePath(actor,dt,now);return;
  }
  if(!s.pendingAttack&&s.reloadUntil<=0){
    const float minimum=d.minRange*(ranged&&style?style->rangeMin:1.f);
    const std::array<float,3> game{gSceneCenterXQ1730+target[0]*FO3_UNITS_PER_METRE,gSceneCenterYQ1730-(target[2]-SCENE_FORWARD)*FO3_UNITS_PER_METRE,gSceneFloorZQ1730+(target[1]-FLOOR_Y)*FO3_UNITS_PER_METRE};
    const bool tooClose=ranged&&minimum>0&&distance<minimum;
    const bool waiting=now<s.burstWaitUntil;
    const bool active=actor.aiPathIndex<actor.aiPathGame.size()&&
      (s.combatMovement==fo3npc::CombatAction::Reposition||s.combatMovement==fo3npc::CombatAction::Cover);
    if(!active&&(s.action==fo3npc::CombatAction::Reposition||s.action==fo3npc::CombatAction::Cover))s.action=fo3npc::CombatAction::Aim;
    if(tooClose||waiting||active){
      if(!active&&now>=s.nextPath){
        const bool cover=waiting&&style&&style->coverChance>
          fo3npc::CombatChoice(actor.source.refFormId,s.tacticSequence++,0,100);
        if(Q230TacticalRoute(actor,game,minimum,maxRange,false,cover,now)){
          s.action=cover?fo3npc::CombatAction::Cover:fo3npc::CombatAction::Reposition;
          s.combatMovement=s.action;s.coverUntil=-1;s.manoeuvreUntil=0;
          Q240AdvancePath(actor,dt,now);return;
        }
      }
      if(waiting&&!active){s.speed=0;s.animation=fo3npc::Animation::Aim;s.action=fo3npc::CombatAction::Wait;return;}
    }
  }
  if(ranged&&s.action==fo3npc::CombatAction::Acquire&&style){
    s.nextAttack=std::max(s.nextAttack,now+fo3npc::CombatChoice(actor.source.refFormId,s.fireSequence++,style->waitMin,style->waitMax));
  }
  if(!movingFire)s.speed=0;
  if(s.reloadUntil>0){
    s.animation=fo3npc::Animation::Reload;Q230ActionSounds(actor,now);
    if(now>=s.reloadUntil){p.ReloadActorWeapon(actor.source.refFormId,instance);s.reloadUntil=0;s.animation=fo3npc::Animation::Aim;++s.actionSerial;
      Q6H_LOGI("NPC RELOAD END actor=%08X weapon=%08X",actor.source.refFormId,form);}
    return;
  }
  if(ranged&&owned->loadedRounds<d.ammoUse){
    if(d.reloadTime<=0)return;
    s.reloadUntil=now+std::max(d.reloadTime,weapon.clips[2].stop-weapon.clips[2].start);
    s.actionStart=s.lastActionTime=now;s.animation=fo3npc::Animation::Reload;s.action=fo3npc::CombatAction::Reload;++s.actionSerial;
    Q6H_LOGI("NPC RELOAD actor=%08X weapon=%08X seconds=%.3f",actor.source.refFormId,form,float(s.reloadUntil-now));return;
  }
  if(s.pendingAttack){
    if(!movingFire)s.animation=fo3npc::Animation::Attack;Q230ActionSounds(actor,now);
    if(now<s.pendingHit)return;
    s.pendingAttack=false;
    const auto matrix=Q230WeaponMatrix(actor);
    const auto local=std::array<float,3>{weapon.muzzle[0]/FO3_UNITS_PER_METRE,FLOOR_Y+weapon.muzzle[2]/FO3_UNITS_PER_METRE,SCENE_FORWARD-weapon.muzzle[1]/FO3_UNITS_PER_METRE};
    const auto muzzle=fo3anim::Point(matrix,local);
    const float mx=target[0]-muzzle[0],my=target[1]-muzzle[1],mz=target[2]-muzzle[2];
    const float ml=std::sqrt(mx*mx+my*my+mz*mz);
    // Revalidate the actual muzzle at the authored hit key: geometry or the
    // target may have moved since the attack wind-up began.
    if(ml<=.01f||HasFo3InteractionOccluder(muzzle[0],muzzle[1],muzzle[2],mx/ml,my/ml,mz/ml,ml,s.combatTarget))return;
    const auto aim=fo3vr::Unit(fo3vr::V{mx,my,mz});
    const float damage=p.ActorWeaponDamage(actor.source.refFormId);
    if(ranged){
      if(p.FireActorWeapon(actor.source.refFormId,instance)){
        const auto right=fo3vr::Unit(fo3vr::Cross(aim,{0,1,0}));const auto up=fo3vr::Unit(fo3vr::Cross(right,aim));
        weaponruntime::EmitShot(instance,actor.source.refFormId,form,d,{muzzle[0],muzzle[1],muzzle[2]},aim,right,up,damage);
        fo3audio::SoundEvent(d.fire3D);Q6H_LOGI("NPC FIRE actor=%08X target=%08X weapon=%08X",actor.source.refFormId,s.combatTarget,form);
      }
    }else if(visible&&distance<=d.maxRange&&p.ApplyAttack(actor.source.refFormId,form,s.combatTarget,damage))
      Q6H_LOGI("NPC HIT actor=%08X target=%08X weapon=%08X damage=%.3f",actor.source.refFormId,s.combatTarget,form,damage);
    return;
  }
  if(now<s.actionUntil){if(!movingFire)s.animation=fo3npc::Animation::Attack;Q230ActionSounds(actor,now);return;}
  if(!movingFire)s.animation=fo3npc::Animation::Aim;s.action=fo3npc::CombatAction::Aim;
  const auto matrix=Q230WeaponMatrix(actor);
  const auto muzzle=fo3anim::Point(matrix,{weapon.muzzle[0]/FO3_UNITS_PER_METRE,FLOOR_Y+weapon.muzzle[2]/FO3_UNITS_PER_METRE,SCENE_FORWARD-weapon.muzzle[1]/FO3_UNITS_PER_METRE});
  const float mx=target[0]-muzzle[0],my=target[1]-muzzle[1],mz=target[2]-muzzle[2],ml=std::sqrt(mx*mx+my*my+mz*mz);
  if(std::fabs(error)>.2f||ml<=.01f||HasFo3InteractionOccluder(muzzle[0],muzzle[1],muzzle[2],mx/ml,my/ml,mz/ml,ml,s.combatTarget))return;
  if(!ranged&&style&&style->meleeValid){
    if(now<s.nextMeleeDecision)return;
    s.nextMeleeDecision=now+std::max(.1f,fo3npc::CombatChoice(actor.source.refFormId,s.tacticSequence++,style->meleeHoldMin,style->meleeHoldMax));
    if(!fo3npc::CombatChance(actor.source.refFormId,s.tacticSequence++,fo3npc::MeleeAttackChance(*style,underAttack,recoiling,unarmedTarget))){s.action=fo3npc::CombatAction::Wait;return;}
  }
  bool ready=ranged?fo3npc::FireReady(s,d,now,style):now>=s.nextAttack;
  if(ready){
    const auto& clip=weapon.clips[1];float hit=0;for(const auto& key:clip.textKeys)if(key.text=="Hit"){hit=key.time;break;}
    s.actionStart=s.lastActionTime=now;s.pendingHit=now+hit;s.pendingAttack=true;
    s.actionUntil=now+(ranged?std::max(hit,fo3npc::ShotInterval(d,style)):(clip.stop-clip.start)/std::max(.01f,d.attackMult));
    if(!ranged)s.nextAttack=s.actionUntil;
    if(!movingFire)s.animation=fo3npc::Animation::Attack;s.action=fo3npc::CombatAction::Attack;++s.actionSerial;
  }
}
