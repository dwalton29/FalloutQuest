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
  for (const auto &source : actors) {
    if (cancel.load(std::memory_order_acquire))
      return;
    Q230ActorVisual visual;
    if (Q230BuildNpcActor(source, visual))
      out.push_back(std::move(visual));
  }
}

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
    if(QActorCreateSkin(gpu,rig.skin,indices,weights,rig.bones.size(),false,part.q1960ExpandedVertices))
      actor.rigs.push_back(std::move(rig));
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
  if(!fo3anim::Sample(actor.skeleton,actor.clip,elapsed,actor.pose,&timings)) return;
  fqactor::npc.clip+=timings.clipUs;fqactor::npc.pose+=timings.skeletonUs;
  for(auto& rig:actor.rigs) {
    const auto started=fqopaque::Clock::now();
    for(size_t slot=0;slot<rig.skin.mapping.sources.size();++slot) {
      const int index=rig.skin.mapping.sources[slot];
      const int bone=index<static_cast<int>(rig.bones.size()) ? rig.bones[index] : -1;
      auto delta=fqskin::Identity();
      if(bone>=0) delta=fo3anim::Multiply(rig.scenePlacement,fo3anim::Multiply(actor.pose.delta[bone],rig.inverseScenePlacement));
      // Uniform game->scene scale cancels in this conjugation; direction w=0.
      fqskin::Pack(rig.skin.palette[slot+1],delta,delta);
    }
    fqactor::npc.bones+=fqopaque::Micros(started);
    QActorUploadSkin(actor.objects[rig.gpuIndex],rig.skin,fqactor::npc,false);
  }
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
