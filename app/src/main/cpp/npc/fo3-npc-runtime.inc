void Q230CacheDialogueAnimations(Q230ActorVisual& actor,const fo3pipdata::Definitions* d) {
  actor.animations[0]=actor.clip;
  const auto root=actor.source.skeletonModel.substr(0,actor.source.skeletonModel.find_last_of("\\/")+1);
  std::array<std::string,size_t(fo3npc::Animation::Count)> paths{"",root+"locomotion\\mtturnleft.kf",root+"locomotion\\mtturnright.kf","",
    root+"locomotion\\"+(actor.source.female?std::string("female\\"):std::string("male\\"))+"mtforward.kf",
    ""};
  if(d)for(const auto& entry:std::array<std::pair<size_t,const char*>,2>{{{3,fo3npc::DialoguePolicy::ConversationIdle},{5,fo3npc::DialoguePolicy::SpeakingIdle}}}) {
    std::string name=entry.second;for(auto& c:name)c=char(std::tolower((unsigned char)c));
    const auto named=d->formNames.find(name);if(named==d->formNames.end())continue;
    const auto model=d->idleModels.find(named->second);if(model!=d->idleModels.end())paths[entry.first]=model->second;
  }
  paths[size_t(fo3npc::Animation::Run)]=root+"locomotion\\"+
      (actor.source.female?std::string("female\\"):std::string("male\\"))+
      "mtfastforward.kf";
  for(size_t i=1;i<paths.size();++i){std::vector<uint8_t> bytes;
    const bool ready=!paths[i].empty()&&LoadFalloutMeshFile(paths[i],bytes)&&fo3anim::DecodeClip(bytes,actor.animations[i]);
    (void)ready;Q6H_LOGI("NPC CLIP actor=%08X slot=%zu ready=%d path=%s",actor.source.refFormId,i,ready,paths[i].c_str());
  }
  actor.headBone=fo3anim::FindBone(actor.skeleton,"Bip01 Head");
  actor.chestBone=fo3anim::FindBone(actor.skeleton,"Bip01 Spine2");
  actor.blendFrom.resize(actor.skeleton.bones.size());
}

#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
void Q230CacheCombat(Q230ActorVisual& actor,const fo3player::Catalog* catalog) {
  if(!catalog)return;
  const auto root=actor.source.skeletonModel.substr(0,actor.source.skeletonModel.find_last_of("\\/")+1);
  const auto candidates=fo3npc::CombatWeaponCandidates(actor.source,*catalog);
  Q6H_LOGI("NPC COMBAT CANDIDATES actor=%08X source=%zu weapons=%zu",
    actor.source.refFormId,actor.source.inventory.size(),candidates.size());
  for(const uint32_t form:candidates) {
    if(actor.combatWeapons.count(form))continue;
    const auto item=catalog->items.find(form);if(item==catalog->items.end())continue;
    const auto& d=item->second.weapon;
    const bool melee=d.valid&&(d.animation==1||d.animation==2);
    if(!melee&&!d.Firearm())continue;
    if((d.flags2&0x41)||d.animation>=8)continue;
    const auto proj=catalog->weapons.projectiles.find(d.projectile);
    if(!melee&&(proj==catalog->weapons.projectiles.end()||proj->second.type!=1||proj->second.explosion||proj->second.range<=0))continue;
    const std::string group=d.animation==1?"1hm":d.animation==2?"2hm":d.animation<=4?"1hp":d.animation==6?"2ha":"2hr";
    Q230CombatWeapon ready;ready.definition=d;std::vector<uint8_t> bytes;
    // WEAP DNAM byte 41 is the authored animation-group enum (xEdit FO3).
    std::string attack;
    switch(d.attackAnimation){case 26:attack="AttackLeft";break;case 32:attack="AttackRight";break;case 38:attack="Attack3";break;case 44:attack="Attack4";break;case 50:attack="Attack5";break;case 56:attack="Attack6";break;case 62:attack="Attack7";break;case 68:attack="Attack8";break;case 74:attack="AttackLoop";break;case 80:attack="AttackSpin";break;case 86:attack="AttackSpin2";break;case 255:if(melee)attack="AttackRight_A";break;}
    if(attack.empty())continue; // Undecoded engine default/group, never fake it.
    std::string attackPath=attack;for(auto& ch:attackPath)ch=char(std::tolower((unsigned char)ch));
    const std::array<std::string,4> names{group+"aim.kf",group+attackPath+".kf",
      d.reload<26?group+"reload"+char('a'+d.reload)+".kf":"",group+"recoil.kf"};
    bool valid=true;
    for(size_t i=0;i<names.size();++i){
      bool ok=!names[i].empty()&&LoadFalloutMeshFile(root+names[i],bytes)&&fo3anim::DecodeClip(bytes,ready.clips[i]);
      if(ok&&i==1)ok=ready.clips[i].name==attack;
      if(ok&&i==0)ok=ready.clips[i].name=="Aim";
      if(ok&&i==2)ok=ready.clips[i].name==std::string("Reload")+char('A'+d.reload);
      if(i<2||(!melee&&i==2))valid=valid&&ok;
      if(ok)for(const auto& key:ready.clips[i].textKeys)if(key.text.rfind("Sound: ",0)==0)fo3audio::PreloadSound(key.text.substr(7));
    }
    if(!valid||!LoadFalloutMeshFile(item->second.model,bytes)||!fo3anim::DecodeModelHierarchy(bytes,ready.model,ready.modelBlocks)) {
      Q6H_LOGI("NPC EQUIP UNSUPPORTED actor=%08X weapon=%08X reason=missing original animation/model",actor.source.refFormId,form);continue;
    }
    if(!melee){const int muzzle=fo3anim::FindBone(ready.model,"ProjectileNode");if(muzzle<0)continue;ready.muzzle=fo3anim::Point(ready.model.bindGlobal[muzzle],{});}
    if(fo3anim::FindBone(actor.skeleton,"Weapon")<0)continue;
    Fo3WorldPlacement placement;placement.modelPath=item->second.model;placement.baseFormId=form;
    placement.baseRecordType="WEAP";placement.scale=1;
    std::vector<CpuObject> parts;if(!BuildCpuObjects(placement,parts))continue;
    for(auto& part:parts)actor.parts.push_back(std::move(part));
    ready.ready=true;actor.combatWeapons.emplace(form,std::move(ready));
    fo3audio::PreloadSound(d.fire3D);fo3audio::PreloadSound(d.equip);
  }
  // All original A/B/C hit IDLEs. Skip missing clips without inventing
  // stand-ins; choose among loaded siblings on an actual nonfatal impact.
  for(size_t slot=0;slot<fo3npc::HitIdleNames.size();++slot)
    for(size_t variant=0;variant<fo3npc::HitIdleNames[slot].size();++variant){
      const auto named=catalog->pipboy.formNames.find(fo3npc::HitIdleNames[slot][variant]);
      if(named==catalog->pipboy.formNames.end())continue;
      const auto model=catalog->pipboy.idleModels.find(named->second);
      if(model==catalog->pipboy.idleModels.end())continue;
      std::vector<uint8_t> bytes;
      auto& clip=actor.authoredHitClips[slot][variant];
      if(LoadFalloutMeshFile(model->second,bytes)&&fo3anim::DecodeClip(bytes,clip)&&
         !clip.tracks.empty())
        Q6H_LOGI("NPC HIT CLIP actor=%08X region=%zu variant=%zu form=%08X path=%s",
          actor.source.refFormId,slot,variant,named->second,model->second.c_str());
    }
  for(const auto& idle:catalog->pipboy.idleModels) {
    std::string path=idle.second;for(auto& c:path)c=char(std::tolower((unsigned char)c));
    if(path.find("deathpose1.kf")==std::string::npos)continue;
    std::vector<uint8_t> bytes;
    if(LoadFalloutMeshFile(idle.second,bytes))fo3anim::DecodeClip(bytes,actor.animations[size_t(fo3npc::Animation::Death)]);
    break;
  }
}
#endif

bool Q230LiveBone(const Q230ActorVisual& actor,int bone,std::array<float,3>& world) {
  if(bone<0||size_t(bone)>=actor.pose.global.size()||actor.rigs.empty())return false;
  world=fo3anim::Point(actor.rigs[0].scenePlacement,
    {actor.pose.global[bone][12],actor.pose.global[bone][13],actor.pose.global[bone][14]});
  world=fo3anim::Point(actor.dialogueRoot,world);return true;
}

// Render-thread ownership and CPU preparation bridge for authored actors.
// Included by the runtime; all CPU data is request-local until scene commit.
// Built only on the scene preparation worker. Atomic shared_ptr publication
// prevents render-frame ESM scans and races with live package evaluation.
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
static std::shared_ptr<const fo3xtel::Index> gQ230NpcXtelIndex;
static std::mutex gQ230NpcXtelMutex;
static std::shared_ptr<const fo3xtel::Index> Q230NpcXtelIndex(){
  return std::atomic_load(&gQ230NpcXtelIndex);
}
#endif
void Q230PrepareActors(uint32_t cell, uint32_t worldspace,
                       std::vector<Q230ActorVisual> &out,
                       const std::atomic<bool> &cancel
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
                       ,const std::unordered_map<uint32_t,fo3player::ActorState>* savedOverride,
                       const std::unordered_set<uint32_t>* onlyRefs
#endif
                       ) {
  // Wasteland roaming/persistent actor residency requires the AI/streaming
  // milestone. Megaton and explicit interior CELLs can be represented now.
  if (worldspace != 0u && worldspace != 0x00000A74u)
    return;
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
  if(!Q230NpcXtelIndex()){
    std::lock_guard<std::mutex> lock(gQ230NpcXtelMutex);
    if(!Q230NpcXtelIndex()){
      auto graph=std::make_shared<fo3xtel::Index>();std::string reason;
      if(graph->Build(fo3assets::FalloutMasterPath(),reason)){
        std::shared_ptr<const fo3xtel::Index> ready=graph;
        std::atomic_store(&gQ230NpcXtelIndex,ready);
        Q6H_LOGI("NPC XTEL INDEX READY edges=%zu",graph->Size());
      }else Q6H_LOGI("NPC XTEL INDEX FAILED reason=%s",reason.c_str());
    }
  }
  // Include original ACHR records moved through XTEL into any grid CELL
  // belonging to Megaton's fully resident exterior worldspace.
  // For background arrival preparation use an immutable actor-state snapshot,
  // rather than reading mutable Player actor states from the worker thread.
  const auto* savedActors=savedOverride?savedOverride:
      (gPlayerSession?&gPlayerSession->player.Snapshot().actors:nullptr);
  const auto* defs=gPlayerSession?&gPlayerSession->player.Definitions().pipboy:nullptr;
  std::unordered_set<uint32_t> arrivals;
  if(savedActors&&defs)for(const auto& entry:*savedActors){
    const auto& saved=entry.second;
    if(!saved.dead&&fo3npc::SceneOwnsCell(cell,worldspace,saved.cell,saved.world,defs->cellWorlds) &&
       (!onlyRefs||onlyRefs->count(entry.first)))
      arrivals.insert(entry.first);
  }
  std::vector<Fo3NpcActorQ230> actors;
  if(!LoadFo3CellActors(cell,actors,{},arrivals.empty()?nullptr:&arrivals))return;
#else
  std::vector<Fo3NpcActorQ230> actors;
  if(!LoadFo3CellActors(cell,actors))return;
#endif
  std::vector<Fo3NpcNavMeshQ240> rawNavigation;
  LoadFo3NpcNavigationQ240(cell,worldspace,rawNavigation);
  auto navigation=std::make_shared<Q240NavigationGraph>();
  navigation->meshes.reserve(rawNavigation.size());
  navigation->triangleOffsets.reserve(rawNavigation.size());
  for(auto& mesh:rawNavigation) {
    auto stored=std::make_shared<const Fo3NpcNavMeshQ240>(std::move(mesh));
    navigation->triangleOffsets.push_back(navigation->triangleCount);
    navigation->triangleCount+=stored->triangles.size();
    navigation->byForm[stored->formId]=navigation->meshes.size();
    navigation->meshes.push_back(std::move(stored));
  }
  std::shared_ptr<fo3furniture::Scene> furniture;
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
  if(gPlayerSession)furniture=fo3furniture::BuildScene(gPlayerSession->player.Definitions().pipboy,cell,worldspace,[](const std::string& path,std::vector<uint8_t>& bytes){return LoadFalloutMeshFile(path,bytes);},true);
#endif
  for(auto source:actors) {
    if(cancel.load(std::memory_order_acquire))return;
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
    if(onlyRefs&&!onlyRefs->count(source.refFormId))continue;
    if(savedActors){
      const auto it=savedActors->find(source.refFormId);
      if(it!=savedActors->end()){
        const auto& pose=it->second;
        if(pose.dead||
           (defs&&!fo3npc::SceneOwnsCell(cell,worldspace,pose.cell,pose.world,defs->cellWorlds)))
          continue;
        // Use the canonical arrival root when constructing the new rig;
        // later Q230SimulateActor restores its package and remaining state.
        source.x=pose.position[0];source.y=pose.position[1];source.z=pose.position[2];
        source.rz=pose.yaw;
      }
    }
#endif
    Q230ActorVisual visual;
    if (Q230BuildNpcActor(source, visual)) {
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
      // Original differential targets are decoded on the scene worker, like
      // the existing FaceGen/animation preparation, never during a draw.
      for(const auto& part:visual.parts) {
        const auto& path=part.placement.modelPath;
        if(!fo3appearance::HeadPart(source,path)||visual.facialAssets.count(path))continue;
        const auto dot=path.rfind('.');if(dot==std::string::npos)continue;
        std::vector<uint8_t> bytes;auto tri=std::make_shared<fo3face::Tri>();std::string error;
        if(LoadFalloutMeshFile(path.substr(0,dot)+".tri",bytes)&&fo3face::DecodeTri(bytes,*tri,error)) {
          Q6H_LOGI("NPC TRI actor=%08X path=%s vertices=%u morphs=%zu",source.refFormId,path.c_str(),tri->vertices,tri->morphs.size());
          visual.facialAssets.emplace(path,std::move(tri));
        }else Q6H_LOGI("NPC TRI MISSING actor=%08X path=%s reason=%s",source.refFormId,path.c_str(),error.c_str());
      }
#endif
      float nearest=INFINITY;
      for(const auto& mesh:navigation->meshes) {
        float candidate=INFINITY;
        for(const auto& vertex:mesh->vertices) {
          const float dx=vertex[0]-source.x,dy=vertex[1]-source.y,dz=vertex[2]-source.z;
          candidate=std::min(candidate,dx*dx+dy*dy+dz*dz);
        }
        if(candidate<nearest){nearest=candidate;visual.navigation=mesh;}
      }
      visual.navigationGraph=navigation;visual.runtime.furniture.scene=furniture;
      if(furniture){
        if(visual.renderEnvelope.empty())visual.renderEnvelope=fo3anim::ClipEnvelope(visual.skeleton,visual.clip);
        for(const auto& slot:furniture->slots)if(slot.program)for(const auto* clip:{&slot.program->enter,&slot.program->loop,&slot.program->exit}){
          const auto envelope=fo3anim::ClipEnvelope(visual.skeleton,*clip);
          for(size_t i=0;i<envelope.size()&&i<visual.renderEnvelope.size();++i){visual.renderEnvelope[i].translation=std::max(visual.renderEnvelope[i].translation,envelope[i].translation);visual.renderEnvelope[i].scale=std::max(visual.renderEnvelope[i].scale,envelope[i].scale);}
        }
      }
      out.push_back(std::move(visual));
    }
  }
}


#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
static bool Q230PathBlocked(Q230ActorVisual&,float,float,float,float,float);
#define FO3_NPC_ACCOMPANY_TARGET(reference,point) Q240ResidentAccompanyTarget(gQ230NpcActors,reference,point)
#define FO3_NPC_PACKAGE_ACTOR_TARGET(reference,point) Q240ResidentPackageTarget(gQ230NpcActors,reference,point)
#define FO3_NPC_PATH_BLOCKED Q230PathBlocked
#define FO3_NPC_STAGGER_ROUTING
#define FO3_NPC_WORLD_GROUND(x,z,referenceY,outY) SampleFo3ActorGroundQ240(x,z,referenceY,outY)
#define FO3_NPC_FURNITURE_LOADING IsFo3LoadingVisible()
#define FO3_NPC_XTEL_INDEX() Q230NpcXtelIndex()
#define FO3_NPC_CHAT_AVAILABLE(actor,reference) Q230ChatEligible(actor,reference)
#define FO3_NPC_CHAT_REQUEST(speaker,listener,now) Q230ChatBegin(speaker,listener,now)
#include "fo3-npc-package-runtime.inc"
#undef FO3_NPC_CHAT_REQUEST
#undef FO3_NPC_CHAT_AVAILABLE
#undef FO3_NPC_XTEL_INDEX
#undef FO3_NPC_FURNITURE_LOADING
#undef FO3_NPC_WORLD_GROUND
#undef FO3_NPC_PATH_BLOCKED
#undef FO3_NPC_STAGGER_ROUTING
#undef FO3_NPC_ACCOMPANY_TARGET
#undef FO3_NPC_PACKAGE_ACTOR_TARGET
#include "fo3-npc-door-runtime.inc"
#endif

fo3anim::Matrix Q230PlacementMatrix(const Fo3WorldPlacement &placement) {
  auto matrix = fo3anim::Identity();
  const Vec3 axes[3] = {ApplyEsmRotation({placement.scale, 0, 0}, placement),
                        ApplyEsmRotation({0, placement.scale, 0}, placement),
                        ApplyEsmRotation({0, 0, placement.scale}, placement)};
  for (int c = 0; c < 3; ++c) {
    matrix[c * 4] = axes[c].x;
    matrix[c * 4 + 1] = axes[c].y;
    matrix[c * 4 + 2] = axes[c].z;
  }
  matrix[12] = placement.x;
  matrix[13] = placement.y;
  matrix[14] = placement.z;
  return matrix;
}

bool Q230UploadActorPart(Q230ActorVisual &actor, CpuObject &part, float centerX,
                         float centerY, float floorZ) {
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
  if(part.placement.baseRecordType=="WEAP") {
    GpuObject gpu;if(!UploadCpuObject(part,0,0,0,gpu))return false;
    gpu.q230NpcActor=true;gpu.physicalWeapon=true;
    actor.combatWeapons.at(part.placement.baseFormId).gpu.push_back(actor.objects.size());
    actor.objects.push_back(std::move(gpu));return true;
  }
#endif
  Q230RigPart rig;
  rig.centerX = centerX;
  rig.centerY = centerY;
  rig.floorZ = floorZ;
  const bool animationReady =
      !actor.skeleton.bones.empty() && !actor.clip.tracks.empty();
  if (animationReady) {
    rig.placement = Q230PlacementMatrix(part.placement);
    if (!fo3anim::Inverse(rig.placement, rig.inversePlacement))
      return false;
    for (const auto &bone : part.mesh.skinBones)
      rig.bones.push_back(fo3anim::FindBone(actor.skeleton, bone.name));
    if (!part.mesh.skinned) {
      const bool headPart = fo3appearance::HeadPart(actor.source, part.placement.modelPath);
      if (headPart)
        rig.rigidBone = fo3anim::FindBone(actor.skeleton, "Bip01 Head");
    }
  }
  const bool deform =
      animationReady && (part.mesh.skinned || rig.rigidBone >= 0);
  if (deform) {
    auto scene=fo3anim::Identity();
    scene[0]=1.0f/FO3_UNITS_PER_METRE;scene[5]=0;scene[10]=0;
    scene[6]=-1.0f/FO3_UNITS_PER_METRE;scene[9]=1.0f/FO3_UNITS_PER_METRE;
    scene[12]=-rig.centerX/FO3_UNITS_PER_METRE;
    scene[13]=FLOOR_Y-rig.floorZ/FO3_UNITS_PER_METRE;
    scene[14]=SCENE_FORWARD+rig.centerY/FO3_UNITS_PER_METRE;
    fo3anim::Matrix inverseScene{};fo3anim::Inverse(scene,inverseScene);
    rig.scenePlacement=fo3anim::Multiply(scene,rig.placement);
    rig.inverseScenePlacement=fo3anim::Multiply(rig.inversePlacement,inverseScene);
    if (!PrepareExpandedVertexStreamQ1960(part, centerX, centerY, floorZ))
      return false;
    // FaceGen/morphed bind geometry is uploaded once, never rebuilt in update.

  }
  // UploadCpuObject consumes the expanded stream. Preserve a setup-time copy
  // until CreateSkin has selected GPU mode or the explicit CPU fallback.
  const std::vector<float> bindExpanded = deform ? part.q1960ExpandedVertices : std::vector<float>{};
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
  const auto facial=actor.facialAssets.find(part.placement.modelPath);
  if(deform&&facial!=actor.facialAssets.end()&&facial->second->vertices==part.positionsGame.size()) {
    fo3anim::Matrix headAttach=fo3anim::Identity();
    if(!part.mesh.skinned&&rig.rigidBone>=0)
      fo3appearance::HeadBindTransform(actor.skeleton,Q230PlacementMatrix(part.placement),headAttach,
        fo3appearance::BoneLocalFacePart(actor.source,part.placement.modelPath));
    for(size_t channel=0;channel<fo3face::MorphNames.size();++channel) {
      const auto morph=facial->second->morphs.find(fo3face::MorphNames[channel]);
      if(morph==facial->second->morphs.end())continue; // No invented target aliases.
      auto& expanded=rig.facialDeltas[channel];expanded.reserve(part.mesh.indices.size());
      const auto* m=part.mesh.geometryDeltaToModel;
      for(auto vertex:part.mesh.indices) {
        const auto& d=morph->second[vertex];
        Vec3 model{m[0]*d[0]+m[1]*d[1]+m[2]*d[2],m[3]*d[0]+m[4]*d[1]+m[5]*d[2],m[6]*d[0]+m[7]*d[1]+m[8]*d[2]};
        model=ApplyEsmRotation(Q211Mul(model,part.placement.scale),part.placement);
        const auto game=fo3anim::Point(headAttach,{model.x,model.y,model.z},true);
        expanded.push_back({game[0]/FO3_UNITS_PER_METRE,game[2]/FO3_UNITS_PER_METRE,-game[1]/FO3_UNITS_PER_METRE});
      }
    }
    if(std::any_of(rig.facialDeltas.begin(),rig.facialDeltas.end(),[](const auto& d){return !d.empty();})) {
      rig.facialBind=rig.facialWork=bindExpanded;
      rig.facialIndices.assign(part.mesh.indices.begin(),part.mesh.indices.end());
    }
  }
#endif
  GpuObject gpu;
  if (!UploadCpuObject(part, centerX, centerY, floorZ, gpu))
    return false;
  gpu.q230NpcActor = true;
  rig.gpuIndex = actor.objects.size();
  if (deform) {
    std::vector<uint16_t> indices;
    std::vector<float> weights;
    indices.reserve(part.mesh.indices.size()*4);weights.reserve(part.mesh.indices.size()*4);
    if(!part.mesh.skinned && rig.rigidBone>=0) rig.bones={rig.rigidBone};
    for(uint32_t vertex:part.mesh.indices) for(size_t j=0;j<4;++j) {
      const size_t at=static_cast<size_t>(vertex)*4+j;
      const bool weighted=part.mesh.skinned && at<part.mesh.skinBoneIndices.size() && at<part.mesh.skinBoneWeights.size();
      indices.push_back(weighted ? part.mesh.skinBoneIndices[at] : 0);
      weights.push_back(weighted ? part.mesh.skinBoneWeights[at] : (rig.rigidBone>=0 && j==0 ? 1.0f : 0.0f));
    }
    if(QActorCreateSkin(gpu,rig.skin,indices,weights,rig.bones.size(),false,bindExpanded)) {
      // Retain only position triplets for weapon-hit skinning, not a duplicate VBO.
      rig.hitVertices.reserve(part.mesh.indices.size());
      for(uint32_t index:part.mesh.indices){const auto&v=part.positionsGame[index];rig.hitVertices.push_back({(v.x-centerX)/FO3_UNITS_PER_METRE,FLOOR_Y+(v.z-floorZ)/FO3_UNITS_PER_METRE,SCENE_FORWARD-(v.y-centerY)/FO3_UNITS_PER_METRE});}
      for(size_t v=0;v+2<rig.hitVertices.size();v+=3){
        std::array<float,7> regions{};
        for(size_t corner=0;corner<3;++corner)for(size_t j=0;j<4;++j){
          const auto* a=rig.skin.mapping.attributes.data()+(v+corner)*8;const size_t slot=size_t(a[4+j]);
          if(a[j]<=0||slot==0||slot>rig.skin.mapping.sources.size())continue;
          const int source=rig.skin.mapping.sources[slot-1];if(source<0||size_t(source)>=rig.bones.size())continue;
          const int bone=rig.bones[source];if(bone<0||size_t(bone)>=actor.skeleton.bones.size())continue;
          regions[size_t(fo3weapon::BoneRegion(actor.skeleton.bones[bone].name))]+=a[j];
        }
        rig.hitRegions.push_back(uint8_t(std::max_element(regions.begin(),regions.end())-regions.begin()));
      }
      rig.interactionBounds.resize(rig.skin.palette.size(),{{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY}});
      for(size_t v=0;v<rig.hitVertices.size();++v)for(size_t j=0;j<4;++j) {
        const auto* attribute=rig.skin.mapping.attributes.data()+v*8;
        if(attribute[j]<=0)continue;
        const size_t slot=size_t(attribute[4+j]);
        if(slot>=rig.interactionBounds.size())continue;
        for(size_t c=0;c<3;++c){auto& bounds=rig.interactionBounds[slot];bounds.first[c]=std::min(bounds.first[c],rig.hitVertices[v][c]);bounds.second[c]=std::max(bounds.second[c],rig.hitVertices[v][c]);}
      }
      if(actor.rigs.empty()) {
        actor.runtime.authoredYaw=actor.runtime.yaw=actor.runtime.returnYaw=std::atan2(-rig.scenePlacement[4],-rig.scenePlacement[6]);
        actor.runtime.position={rig.scenePlacement[12],rig.scenePlacement[13],rig.scenePlacement[14]};
      }
      actor.rigs.push_back(std::move(rig));
    }
  }
  GpuObject bounds=gpu;
  if(deform && !actor.rigs.empty() && actor.rigs.back().gpuIndex==rig.gpuIndex) {
    if(actor.renderEnvelope.empty()) actor.renderEnvelope=fo3anim::ClipEnvelope(actor.skeleton,actor.clip);
    double bindRadius=0;
    for(const auto& vertex:part.positionsGame) {
      const auto local=fo3anim::Point(actor.rigs.back().inversePlacement,{vertex.x,vertex.y,vertex.z});
      bindRadius=std::max(bindRadius,std::sqrt(double(local[0])*local[0]+double(local[1])*local[1]+double(local[2])*local[2]));
    }
    // rig was moved into actor.rigs; use its stable, cached bone association.
    const auto& readyRig=actor.rigs.back();
    double radius=bindRadius;
    for(int bone:readyRig.bones) radius=std::max(radius,fo3anim::DeltaRadius(actor.skeleton,actor.renderEnvelope,bone,bindRadius));
    radius=radius*fo3anim::LinearNormBound(readyRig.placement)/FO3_UNITS_PER_METRE;
    const float x=(readyRig.placement[12]-centerX)/FO3_UNITS_PER_METRE;
    const float y=FLOOR_Y+(readyRig.placement[14]-floorZ)/FO3_UNITS_PER_METRE;
    const float z=SCENE_FORWARD-(readyRig.placement[13]-centerY)/FO3_UNITS_PER_METRE;
    if(!std::isfinite(radius)) {
      bounds.minX=bounds.minY=bounds.minZ=-1e30f;bounds.maxX=bounds.maxY=bounds.maxZ=1e30f;
    } else {
      const float r=static_cast<float>(radius)+0.01f;
      bounds.minX=std::min(bounds.minX,x-r);bounds.maxX=std::max(bounds.maxX,x+r);
      bounds.minY=std::min(bounds.minY,y-r);bounds.maxY=std::max(bounds.maxY,y+r);
      bounds.minZ=std::min(bounds.minZ,z-r);bounds.maxZ=std::max(bounds.maxZ,z+r);
    }
  }
  if(!actor.renderBoundsReady) { actor.renderBounds=bounds;actor.renderBoundsReady=true; }
  else {
    auto& a=actor.renderBounds;
    a.minX=std::min(a.minX,bounds.minX);a.maxX=std::max(a.maxX,bounds.maxX);
    a.minY=std::min(a.minY,bounds.minY);a.maxY=std::max(a.maxY,bounds.maxY);
    a.minZ=std::min(a.minZ,bounds.minZ);a.maxZ=std::max(a.maxZ,bounds.maxZ);
  }
  actor.objects.push_back(std::move(gpu));
  return true;
}

#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
// Only TRI-bearing head parts upload differential bind geometry. Body skinning
// continues to update bone palettes without rebuilding its vertices.
void Q230UpdateFacialPart(Q230ActorVisual& actor,Q230RigPart& rig) {
    if(!rig.facialBind.empty()&&rig.facialPublished!=actor.facialWeights) {
      if(!fo3face::BlendExpanded(rig.facialBind,rig.facialDeltas,actor.facialWeights,rig.facialWork))return;
      if(!fo3face::UpdateDirections(rig.facialBind,rig.facialIndices,rig.facialWork))return;
      auto& object=actor.objects[rig.gpuIndex];
      if(object.skinMode) {
        glBindBuffer(GL_ARRAY_BUFFER,object.vbo);
        glBufferSubData(GL_ARRAY_BUFFER,0,rig.facialWork.size()*sizeof(float),rig.facialWork.data());
        glBindBuffer(GL_ARRAY_BUFFER,0);
      }else rig.skin.bind=rig.facialWork;
      rig.facialPublished=actor.facialWeights;
    }
}
#endif

void Q230UpdateActor(Q230ActorVisual &actor) {
  if (actor.lastFrame == gStereoFrame || actor.rigs.empty()) return;
  actor.lastFrame=gStereoFrame;
  ++fqactor::npc.actors;
  const double elapsed=std::chrono::duration<double>(fqopaque::Clock::now()-actor.animationStart).count();
  fo3anim::SampleTimings timings;
  int selected=int(actor.runtime.animation);
  if(selected<0||size_t(selected)>=actor.animations.size())selected=0;
  if(actor.animations[selected].tracks.empty())selected=0;
  if(selected!=actor.activeAnimation
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
     ||actor.animationSerial!=actor.runtime.actionSerial
#endif
     ) {
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
    actor.animationSerial=actor.runtime.actionSerial;
#endif
    actor.blendFrom=actor.pose.local;actor.blendStart=elapsed;actor.activeAnimation=selected;
    fo3anim::BindClip(actor.skeleton,actor.animations[selected],actor.pose);
    if(actor.runtime.activity!=fo3npc::Activity::Combat)Q6H_LOGI("NPC ANIM actor=%08X clip=%s",actor.source.refFormId,actor.animations[selected].name.c_str());
  }
  const auto& clip=actor.animations[actor.activeAnimation].tracks.empty()?actor.clip:actor.animations[actor.activeAnimation];
  const double clipTime=actor.blendStart<0?elapsed:elapsed-actor.blendStart;
  const double sampleTime=actor.runtime.activity==fo3npc::Activity::Dead?std::max(0.f,clip.stop-clip.start):clipTime;
  const auto& furniture=actor.runtime.furniture;
  const auto* slot=furniture.Selected();
  const bool furniturePose=slot&&(furniture.phase==fo3furniture::Phase::Enter||furniture.phase==fo3furniture::Phase::Loop||furniture.phase==fo3furniture::Phase::Exit);
  if(furniturePose){
    const auto& authored=slot->idleMarker?slot->idles[furniture.idleIndex].clip:furniture.phase==fo3furniture::Phase::Enter?slot->program->enter:furniture.phase==fo3furniture::Phase::Exit?slot->program->exit:slot->program->loop;
    const double now=std::chrono::duration<double>(fqopaque::Clock::now().time_since_epoch()).count();
    fo3anim::BindClip(actor.skeleton,authored,actor.pose);
    if(!fo3anim::Sample(actor.skeleton,authored,now-furniture.started,actor.pose,&timings,slot->idleMarker?fo3anim::RootPolicy::Locomotion:fo3anim::RootPolicy::Furniture,&furniture.retainedRoot))return;
  }else if(!fo3anim::Sample(actor.skeleton,clip,sampleTime,actor.pose,&timings)) return;
  if(!furniturePose&&actor.blendStart>=0&&elapsed-actor.blendStart<fo3npc::DialoguePolicy::BlendSeconds&&actor.blendFrom.size()==actor.pose.local.size()) {
    const float mix=float((elapsed-actor.blendStart)/fo3npc::DialoguePolicy::BlendSeconds);
    for(size_t i=0;i<actor.pose.local.size();++i) {
      auto& to=actor.pose.local[i];const auto& from=actor.blendFrom[i];
      for(int j=0;j<3;++j)to.translation[j]=from.translation[j]*(1-mix)+to.translation[j]*mix;
      float dot=0;for(int j=0;j<4;++j)dot+=from.rotation[j]*to.rotation[j];
      float norm=0;for(int j=0;j<4;++j){to.rotation[j]=from.rotation[j]*(1-mix)+(dot<0?-1:1)*to.rotation[j]*mix;norm+=to.rotation[j]*to.rotation[j];}
      norm=std::sqrt(norm);if(norm>1e-6f)for(auto& v:to.rotation)v/=norm;
      to.scale=from.scale*(1-mix)+to.scale*mix;
    }
    // Blending must preserve Sample()'s in-place accumulation-root invariant.
    // Otherwise a previous clip's root motion can leak back for the blend.
    if(actor.pose.accumulation>=0&&size_t(actor.pose.accumulation)<actor.pose.local.size())
      actor.pose.local[size_t(actor.pose.accumulation)]=fo3anim::Transform{};
    fo3anim::ComposePose(actor.skeleton,actor.pose);
  }
  if(!furniturePose)fo3anim::LookYaw(actor.skeleton,actor.pose,actor.headBone,actor.runtime.headYaw);
  auto root=fo3anim::Identity();const float yaw=actor.runtime.yaw-actor.runtime.authoredYaw;
  root[0]=root[10]=std::cos(yaw);root[2]=-std::sin(yaw);root[8]=-root[2];
  if(!actor.rigs.empty()) {
    const auto& placement=actor.rigs[0].scenePlacement;const float x=placement[12],y=placement[13],z=placement[14];
    root[12]=actor.runtime.position[0]-root[0]*x-root[8]*z;
    root[13]=actor.runtime.position[1]-y;
    root[14]=actor.runtime.position[2]-root[2]*x-root[10]*z;
  }
  actor.dialogueRoot=root;
  fqactor::npc.clip+=timings.clipUs;fqactor::npc.pose+=timings.skeletonUs;
  for(auto& rig:actor.rigs) {
    const auto started=fqopaque::Clock::now();
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
    Q230UpdateFacialPart(actor,rig);
#endif
    fqskin::Pack(rig.skin.palette[0],actor.dialogueRoot,actor.dialogueRoot);
    for(size_t slot=0;slot<rig.skin.mapping.sources.size();++slot) {
      const int index=rig.skin.mapping.sources[slot];
      const int bone=index<static_cast<int>(rig.bones.size()) ? rig.bones[index] : -1;
      auto delta=fqskin::Identity();
      if(bone>=0) delta=fo3anim::Multiply(rig.scenePlacement,fo3anim::Multiply(actor.pose.delta[bone],rig.inverseScenePlacement));
      delta=fo3anim::Multiply(actor.dialogueRoot,delta);
      // Uniform game->scene scale cancels in this conjugation; direction w=0.
      fqskin::Pack(rig.skin.palette[slot+1],delta,delta);
    }
    fqactor::npc.bones+=fqopaque::Micros(started);
    QActorUploadSkin(actor.objects[rig.gpuIndex],rig.skin,fqactor::npc,false);
  }
}

bool Q230LiveBounds(Q230ActorVisual& actor,std::array<float,3>& lo,std::array<float,3>& hi) {
  if(actor.runtime.offScene)return false;
  Q230UpdateActor(actor);lo={INFINITY,INFINITY,INFINITY};hi={-INFINITY,-INFINITY,-INFINITY};
  for(const auto& rig:actor.rigs)for(size_t slot=0;slot<rig.interactionBounds.size();++slot) {
    const auto& b=rig.interactionBounds[slot];if(!std::isfinite(b.first[0]))continue;
    for(int corner=0;corner<8;++corner) {
      const std::array<float,3> p{corner&1?b.second[0]:b.first[0],corner&2?b.second[1]:b.first[1],corner&4?b.second[2]:b.first[2]};
      const auto point=fqskin::Transform(rig.skin.palette[slot].data(),p.data(),false);
      for(size_t c=0;c<3;++c){lo[c]=std::min(lo[c],point[c]);hi[c]=std::max(hi[c],point[c]);}
    }
  }
  return std::isfinite(lo[0]);
}

#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
void Q230ActorHitReaction(uint32_t target,uint8_t region) {
  if(!gPlayerSession||gPlayerSession->player.ActorHealth(target)<=0)return;
  const double now=std::chrono::duration<double>(
    fqopaque::Clock::now().time_since_epoch()).count();
  for(auto& actor:gQ230NpcActors) {
    if(actor.source.refFormId!=target||actor.runtime.offScene||
       !actor.runtime.Alive()||now<actor.runtime.hitUntil)continue;
    // fo3weapon::Region is source geometry, never a guessed hit location.
    const size_t slot=fo3npc::HitRegionSlot(region);
    auto& s=actor.runtime;
    // A hit cannot indefinitely cancel a wind-up or restart a reload.
    // Let the original combat event finish; the damage and hostility still apply.
    if(!fo3npc::MayPlayHitReaction(s)){
      Q6H_LOGI("NPC HIT REACTION SKIP actor=%08X reason=attack-or-reload",target);
      return;
    }
    std::array<bool,3> available{};
    for(size_t i=0;i<3;++i)available[i]=!actor.authoredHitClips[slot][i].tracks.empty();
    const int variant=fo3npc::HitVariant(available,s.hitSequence++);
    if(variant<0)return;
    const auto& clip=actor.authoredHitClips[slot][size_t(variant)];
    const double duration=(clip.stop-clip.start)/std::max(.01f,clip.frequency);
    if(!std::isfinite(duration)||duration<=0)return;
    actor.animations[size_t(fo3npc::Animation::Hit)]=clip;
    s.furniture.Clear();s.speed=0;
    // Outside combat the reaction may interrupt movement, not erase damage
    // attribution or a paused combat reload.
    s.hitUntil=now+std::min(std::clamp(duration,.08,1.5),
                             s.activity==fo3npc::Activity::Combat?.15:.35);
    s.animation=fo3npc::Animation::Hit;++s.actionSerial;actor.lastFrame=UINT64_MAX;
    Q6H_LOGI("NPC HIT REACTION actor=%08X region=%u variant=%d duration=%.3f clip=%s",
      target,unsigned(region),variant,s.hitUntil-now,clip.name.c_str());
    return;
  }
}
#include "fo3-npc-combat-runtime.inc"
#endif
void Q230RenderNpcActors(bool alphaPass) {
  const auto drawStarted=fqopaque::Clock::now();
  for (auto &actor : gQ230NpcActors) {
    if(!actor.renderVisible) continue;
    for (size_t objectIndex=0;objectIndex<actor.objects.size();++objectIndex) {
      auto& object=actor.objects[objectIndex];
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
      bool weaponPart=false;for(const auto& w:actor.combatWeapons)
        if(std::find(w.second.gpu.begin(),w.second.gpu.end(),objectIndex)!=w.second.gpu.end()){weaponPart=true;break;}
      if(weaponPart)continue;
#endif
      if (object.alphaBlend != alphaPass)
        continue;
      if (object.zBufferTestQ1200)
        glEnable(GL_DEPTH_TEST);
      else
        glDisable(GL_DEPTH_TEST);
      glDepthMask(object.zBufferWriteQ1200 ? GL_TRUE : GL_FALSE);
      if (alphaPass) {
        glEnable(GL_BLEND);
        glBlendFunc(Q1150BlendFactor(object.alphaSourceBlend, true),
                    Q1150BlendFactor(object.alphaDestBlend, false));
      }
      DrawSceneObject(object);
    }
#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
    Q230RenderWeapon(actor,alphaPass);
#endif
  }
  fqactor::npc.draw+=fqopaque::Micros(drawStarted);
}
