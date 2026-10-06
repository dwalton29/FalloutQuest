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
  for(size_t i=1;i<paths.size();++i){std::vector<uint8_t> bytes;
    const bool ready=!paths[i].empty()&&LoadFalloutMeshFile(paths[i],bytes)&&fo3anim::DecodeClip(bytes,actor.animations[i]);
    (void)ready;Q6H_LOGI("NPC CLIP actor=%08X slot=%zu ready=%d path=%s",actor.source.refFormId,i,ready,paths[i].c_str());
  }
  actor.headBone=fo3anim::FindBone(actor.skeleton,"Bip01 Head");
  actor.chestBone=fo3anim::FindBone(actor.skeleton,"Bip01 Spine2");
  actor.blendFrom.resize(actor.skeleton.bones.size());
}

bool Q230LiveBone(const Q230ActorVisual& actor,int bone,std::array<float,3>& world) {
  if(bone<0||size_t(bone)>=actor.pose.global.size()||actor.rigs.empty())return false;
  world=fo3anim::Point(actor.rigs[0].scenePlacement,
    {actor.pose.global[bone][12],actor.pose.global[bone][13],actor.pose.global[bone][14]});
  world=fo3anim::Point(actor.dialogueRoot,world);return true;
}

// Render-thread ownership and CPU preparation bridge for authored actors.
// Included by the runtime; all CPU data is request-local until scene commit.
void Q230PrepareActors(uint32_t cell, uint32_t worldspace,
                       std::vector<Q230ActorVisual> &out,
                       const std::atomic<bool> &cancel) {
  // Wasteland roaming/persistent actor residency requires the AI/streaming
  // milestone. Megaton and explicit interior CELLs can be represented now.
  if (worldspace != 0u && worldspace != 0x00000A74u)
    return;
  std::vector<Fo3NpcActorQ230> actors;
  if (!LoadFo3CellActors(cell, actors))
    return;
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
  for (const auto &source : actors) {
    if (cancel.load(std::memory_order_acquire))
      return;
    Q230ActorVisual visual;
    if (Q230BuildNpcActor(source, visual)) {
      float nearest=INFINITY;
      for(const auto& mesh:navigation->meshes) {
        float candidate=INFINITY;
        for(const auto& vertex:mesh->vertices) {
          const float dx=vertex[0]-source.x,dy=vertex[1]-source.y,dz=vertex[2]-source.z;
          candidate=std::min(candidate,dx*dx+dy*dy+dz*dz);
        }
        if(candidate<nearest){nearest=candidate;visual.navigation=mesh;}
      }
      visual.navigationGraph=navigation;
      out.push_back(std::move(visual));
    }
  }
}


#ifndef FO3_ACTOR_RUNTIME_HOST_TEST
#include "fo3-npc-package-runtime.inc"
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

void Q230UpdateActor(Q230ActorVisual &actor) {
  if (actor.lastFrame == gStereoFrame || actor.rigs.empty()) return;
  actor.lastFrame=gStereoFrame;
  ++fqactor::npc.actors;
  const double elapsed=std::chrono::duration<double>(fqopaque::Clock::now()-actor.animationStart).count();
  fo3anim::SampleTimings timings;
  int selected=int(actor.runtime.animation);
  if(selected<0||size_t(selected)>=actor.animations.size())selected=0;
  if(actor.animations[selected].tracks.empty())selected=0;
  if(selected!=actor.activeAnimation) {
    actor.blendFrom=actor.pose.local;actor.blendStart=elapsed;actor.activeAnimation=selected;
    fo3anim::BindClip(actor.skeleton,actor.animations[selected],actor.pose);
    Q6H_LOGI("NPC ANIM actor=%08X clip=%s",actor.source.refFormId,actor.animations[selected].name.c_str());
  }
  const auto& clip=actor.animations[actor.activeAnimation].tracks.empty()?actor.clip:actor.animations[actor.activeAnimation];
  const double clipTime=actor.blendStart<0?elapsed:elapsed-actor.blendStart;
  if(!fo3anim::Sample(actor.skeleton,clip,clipTime,actor.pose,&timings)) return;
  if(actor.blendStart>=0&&elapsed-actor.blendStart<fo3npc::DialoguePolicy::BlendSeconds&&actor.blendFrom.size()==actor.pose.local.size()) {
    const float mix=float((elapsed-actor.blendStart)/fo3npc::DialoguePolicy::BlendSeconds);
    for(size_t i=0;i<actor.pose.local.size();++i) {
      auto& to=actor.pose.local[i];const auto& from=actor.blendFrom[i];
      for(int j=0;j<3;++j)to.translation[j]=from.translation[j]*(1-mix)+to.translation[j]*mix;
      float dot=0;for(int j=0;j<4;++j)dot+=from.rotation[j]*to.rotation[j];
      float norm=0;for(int j=0;j<4;++j){to.rotation[j]=from.rotation[j]*(1-mix)+(dot<0?-1:1)*to.rotation[j]*mix;norm+=to.rotation[j]*to.rotation[j];}
      norm=std::sqrt(norm);if(norm>1e-6f)for(auto& v:to.rotation)v/=norm;
      to.scale=from.scale*(1-mix)+to.scale*mix;
    }
    fo3anim::ComposePose(actor.skeleton,actor.pose);
  }
  fo3anim::LookYaw(actor.skeleton,actor.pose,actor.headBone,actor.runtime.headYaw);
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

void Q230RenderNpcActors(bool alphaPass) {
  const auto drawStarted=fqopaque::Clock::now();
  for (auto &actor : gQ230NpcActors) {
    if(!actor.renderVisible) continue;
    for (const auto &object : actor.objects) {
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
  }
  fqactor::npc.draw+=fqopaque::Micros(drawStarted);
}
