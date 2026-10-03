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
      bool headPart = part.placement.modelPath == actor.source.hairModel;
      for (const auto &path : actor.source.raceHeadModels)
        headPart |= path == part.placement.modelPath;
      for (const auto &path : actor.source.headPartModels)
        headPart |= path == part.placement.modelPath;
      if (headPart)
        rig.rigidBone = fo3anim::FindBone(actor.skeleton, "Bip01 Head");
    }
  }
  const bool deform =
      animationReady && (part.mesh.skinned || rig.rigidBone >= 0);
  if (deform) {
    if (!PrepareExpandedVertexStreamQ1960(part, centerX, centerY, floorZ))
      return false;
    rig.work = part.q1960ExpandedVertices;
    rig.posed.resize(part.positionsGame.size());
    rig.gameDeltas.resize(actor.skeleton.bones.size());
    rig.bind = part; // retain morphed bind vertices, normals, weights and
                     // material data
    rig.bind.q1960ExpandedVertices.clear();
    rig.bind.q1960ExpandedReady = false;
  }
  GpuObject gpu;
  if (!UploadCpuObject(part, centerX, centerY, floorZ, gpu))
    return false;
  gpu.q230NpcActor = true;
  rig.gpuIndex = actor.objects.size();
  if (deform) {
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(rig.work.size() * sizeof(float)),
                 rig.work.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    actor.rigs.push_back(std::move(rig));
  }
  actor.objects.push_back(std::move(gpu));
  return true;
}

void Q230UpdateActor(Q230ActorVisual &actor) {
  if (actor.lastFrame == gSceneLoadFrame || actor.rigs.empty())
    return;
  actor.lastFrame = gSceneLoadFrame;
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                    actor.animationStart)
          .count();
  if (!fo3anim::Sample(actor.skeleton, actor.clip, elapsed, actor.pose))
    return;
  for (auto &rig : actor.rigs) {
    const auto &cpu = rig.bind;
    // Form game-space deformation matrices once per bone, then skin each
    // source vertex once. Triangle expansion copies the already posed stream.
    for (size_t bone = 0; bone < actor.pose.delta.size(); ++bone)
      rig.gameDeltas[bone] = fo3anim::Multiply(
          rig.placement,
          fo3anim::Multiply(actor.pose.delta[bone], rig.inversePlacement));
    const auto identity = fo3anim::Identity();
    for (size_t vertex = 0; vertex < cpu.positionsGame.size(); ++vertex) {
      fo3anim::Matrix skin{};
      float total = 0;
      if (cpu.mesh.skinned &&
          cpu.mesh.skinBoneIndices.size() > vertex * 4u + 3u &&
          cpu.mesh.skinBoneWeights.size() > vertex * 4u + 3u) {
        for (int influence = 0; influence < 4; ++influence) {
          const size_t at = vertex * 4u + influence;
          const uint16_t index = cpu.mesh.skinBoneIndices[at];
          const float weight = cpu.mesh.skinBoneWeights[at];
          if (weight <= 0)
            continue;
          const int bone = index < rig.bones.size() ? rig.bones[index] : -1;
          const auto &delta = bone >= 0 ? rig.gameDeltas[bone] : identity;
          for (size_t i = 0; i < skin.size(); ++i)
            skin[i] += delta[i] * weight;
          total += weight;
        }
      } else if (rig.rigidBone >= 0) {
        skin = rig.gameDeltas[rig.rigidBone];
        total = 1;
      }
      if (total <= 1e-8f)
        skin = identity;
      else
        for (float &value : skin)
          value /= total;
      const Vec3 vectors[4] = {
          cpu.positionsGame[vertex], cpu.normalsGame[vertex],
          cpu.tangentsGame[vertex], cpu.bitangentsGame[vertex]};
      for (int component = 0; component < 4; ++component) {
        const auto game = fo3anim::Point(
            skin,
            {vectors[component].x, vectors[component].y, vectors[component].z},
            component != 0);
        Vec3 xr;
        if (component == 0)
          xr = {(game[0] - rig.centerX) / FO3_UNITS_PER_METRE,
                FLOOR_Y + (game[2] - rig.floorZ) / FO3_UNITS_PER_METRE,
                SCENE_FORWARD - (game[1] - rig.centerY) / FO3_UNITS_PER_METRE};
        else
          xr = GameDirectionToOpenXr(Normalize({game[0], game[1], game[2]}));
        rig.posed[vertex][component * 3] = xr.x;
        rig.posed[vertex][component * 3 + 1] = xr.y;
        rig.posed[vertex][component * 3 + 2] = xr.z;
      }
    }
    for (size_t expanded = 0; expanded < cpu.mesh.indices.size(); ++expanded)
      std::copy_n(rig.posed[cpu.mesh.indices[expanded]].begin(), 12,
                  rig.work.begin() + expanded * 18u);
    const auto &object = actor.objects[rig.gpuIndex];
    glBindBuffer(GL_ARRAY_BUFFER, object.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(rig.work.size() * sizeof(float)),
                    rig.work.data());
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Q230RenderNpcActors(bool alphaPass) {
  for (auto &actor : gQ230NpcActors) {
    Q230UpdateActor(actor);
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
}
