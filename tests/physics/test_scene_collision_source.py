from pathlib import Path
s=Path('app/src/main/cpp/core/fo3-runtime.cpp').read_text()
begin=s[s.index('bool BeginFo3SceneLoad('):s.index('bool ProcessQ74TransitionRequest(')]
assert begin.index('SelectFo3AuthoredCollisionPlacements(result.placements)') < begin.index('if (Q74ShouldSkipPlacement(placement))') < begin.index('if (!BuildCpuObjects(placement, parts))')
transition=s[s.index('bool ProcessQ74TransitionRequest('):]
assert 'if (request.worldspaceFormId != 0u) ConfigureFo3CollisionPolicyQ710' not in transition
assert transition.index('ResetFo3CollisionSceneCache()') < transition.index('collisionPreparation.Start(')
assert transition.index('PublishFo3CollisionSnapshotQ1930') < transition.index('gObjects = std::move(replacement)') < transition.index('CompleteFo3CellTransitionQ74(')
assert transition.index('gSceneLoad.collisionComplete = true') < transition.index('PublishFo3CollisionSnapshotQ1930')
print('Scene collision independence and publication ordering passed')

# Reference state must be checked before every transition upload/publication.
assert transition.index('AbortFo3SceneLoad("reference-state-changed")') < transition.index('UploadCpuObject(')
assert transition.index('gFo3ScenePublication.Publish(prepared.referenceState)') > transition.index('PublishFo3CollisionSnapshotQ1930')
c=Path('app/src/main/cpp/world/fo3-cell-streaming.inc').read_text()
reset=c[c.index('void Q1900ResetCellsQ19('):c.index('bool Q1900ContextChangedQ19(')]
assert reset.index('Q2015DiscardCellUploadQ19(entry.second)') < reset.index('gWorldStreaming.cells.clear()')
assert 'gWorldStreaming.publication == gFo3ScenePublication.Stamp()' in c
assert 'task->referenceState = gFo3ScenePublication.Snapshot()' in c
assert 'task->referenceState=CaptureFo3ReferenceState()' not in c
update=c[c.index('void Q1900UpdateCellStreamingQ19('):]
assert update.index('if (!Fo3AcceptsScenePublication') < update.index('Q1900ResetCellsQ19(')
assert 'state.textureCursor = state.gpuCursor = 0u' in update
commit=c[c.index('void Q1900CommitCellVisualsQ19('):c.index('bool Q2013AdvanceCellGpuSliceQ19(')]
assert commit.index('if (!Fo3AcceptsScenePublication') < commit.index('Q1910QueueGpuDeleteQ19(object)')
collision=c[c.index('void Q1900TryPublishCollisionQ19('):c.index('void Q1900MaintainTerrainQ19(')]
assert collision.index('!Fo3AcceptsScenePublication(task->publication)') < collision.index('PublishFo3CollisionSnapshotQ1930(')
print('Scene reference epoch admission and staged-resource retirement passed')

arrival=s[s.index('void Q230PumpNpcArrivals(){'):s.index('void QActorPrepareStereoFrame()')]
assert arrival.index('!Fo3AcceptsScenePublication(work.publication)') < arrival.index('Q230UploadActorPart(')
assert arrival.index('if(!work.preparation.Ready())return') < arrival.index('Q230ResetNpcArrivals()')
assert 'pending.insert(work.requested.begin(),work.requested.end())' in arrival
assert arrival.index('auto requested=refs') < arrival.index('work.preparation.Start(')
assert 'work.requested=std::move(requested)' in arrival
print('NPC arrival epoch guard precedes part uploads and actor registration')

# GPU ownership plans must use the real shared/unique deletion path, and every
# legacy structural publication must invalidate plans before moving live slots.
assert 'using Fo3ResidentVisualPlan = fo3scene::VisualExchange<GpuObject>' in s
dispose=s[s.index('void DisposeFo3ResidentVisuals('):s.index('void DiscardFo3ResidentVisualPlan()')]
assert 'Q74DeleteGpuObjects(objects)' in dispose
deletion=s[s.index('void Q74DeleteGpuObjects('):s.index('struct Fo3SceneCpuPreparation')]
assert deletion.index('&objects == &gObjects') < deletion.index('QActorDeleteSkin(object)')
assert deletion.index('QActorDeleteSkin(object)') < deletion.index('Q2017ReleaseSharedGeometry(object)')
for source,start,end,mutation in [
    (s,'bool InitializeScene()','bool Q74ShouldSkipPlacement(', 'gObjects.push_back('),
    (s,'void Q1900CommitWindow()','struct Q1970', 'replacement.push_back(std::move(object))'),
    (s,'void Q6HDeleteFramebuffers(',None, 'Q1900DeleteGpuShape(object)'),
    (c,'void Q1900CommitCellVisualsQ19(','bool Q2013AdvanceCellGpuSliceQ19(', 'Q1910QueueGpuDeleteQ19(object)'),
    (c,'void Q1900RetireFarCellsQ19(',None, 'Q1910QueueGpuDeleteQ19(object)'),
]:
    region=source[source.index(start):]
    if end: region=region[:region.index(end)]
    assert region.index('gFo3VisualPublication.Changed()') < region.index(mutation)
for assignment in ('gObjects = std::move(replacement)', 'gObjects = std::move(work.replacement)'):
    # Whole-scene replacements destroy the previous layout through the marked
    # helper, except the separately marked Q18 retained/incoming composition.
    for part in s.split(assignment)[:-1]:
        assert ('Q74DeleteGpuObjects(gObjects)' in part[-250:] or
                'void Q1900CommitWindow()' in part and
                'gFo3VisualPublication.Changed()' in part[part.rindex('void Q1900CommitWindow()'):])
teardown=s[s.index('void Q6HDeleteFramebuffers('):]
assert teardown.index('DiscardFo3ResidentVisualPlan()') < teardown.index('Q2017ForceClearSharedGeometry()')
assert teardown.index('DiscardFo3ResidentVisualPlan()') < teardown.index('gTextureCache.clear()')
assert begin.index('DiscardFo3ResidentVisualPlan()') < begin.index('gSceneLoad.preparation.Start(')
assert 'gOpaqueOrderVisualRevision == gFo3VisualPublication.Revision()' in s
assert 'cache.visualRevision != gFo3VisualPublication.Revision()' in s
assert 'cachedVisualRevision != 0 && cachedVisualRevision == gFo3VisualPublication.Revision()' in s
print('Visual ownership epochs precede scene/CELL/retirement teardown and invalidate frame caches')

worker=s[s.index('bool PrepareFo3ResidentCpuResources('):s.index('using Fo3ResidentLane=')]
for forbidden in ('gPlayerSession','gObjects','ConfigureFo3CollisionPolicyQ710',
                  'ResetFo3CollisionSceneCache','PrimeFo3Collision','UploadCpuObject','glGen'):
    assert forbidden not in worker
assert worker.index('SelectResidentResources(') < worker.index('PrepareResidentCpuParts(')
assert worker.index('PrepareResidentAuthoredAdmission(') < worker.index('SelectResidentResources(')
assert 'resident exterior authored admission unsupported' in worker
assert s.count('cpu.authored.Matches(cpu.request.cellFormId,cpu.affected)') == 3
assert 'BuildCpuObjects(placement,parts,true)' in worker
assert worker.index('PrepareResidentCpuParts(') < worker.index('out=std::move(next)')
cpu=s[s.index('bool BuildCpuObjects('):s.index('bool ResolveDoorTeleportCachedQ1698(')]
assert 'requireComplete ? completeCache : ordinaryCache' in cpu
assert 'LoadFo3StaticNifMeshes(placement.modelPath, meshes, requireComplete)' in cpu
nif=Path('app/src/main/cpp/rendering/mesh/fo3-static-nif.cpp').read_text()
assert 'requireComplete && (!q970SelectionReady || q970Scene.fallbackAll)' in nif
assert 'requireComplete && (q1060ShapeObjectFailures || q1060ShapeLoadFailures)' in nif
print('Resident CPU packet uses value inputs, strict decode/cache admission and no live resource publication')

# Resident texture decode is completed on the worker, before packet publication.
assert worker.index('PrepareExpandedVertexStreamQ1960(') < worker.index('PrepareResidentTextures(') < worker.index('out=std::move(next)')
assert 'next.publication=publication; next.visualRevision=visualRevision' in worker
assert 'publication.references!=published->revision' in worker
for forbidden in ('gTextureCache','gCubeTextureCacheQ2050','glBind','glTexImage'):
    assert forbidden not in worker
for signature,ordinary in [('bool UploadTexture(', 'gTextureCache.find('),
                           ('bool UploadCubeTextureQ2050(', 'gCubeTextureCacheQ2050.find(')]:
    upload=s[s.index(signature):]
    assert upload.index('if(gFo3ResidentTextureUpload)') < upload.index(ordinary)
private=s[s.index('bool UploadFo3ResidentTexture('):s.index('bool UploadTexture(')]
assert 'work.decoded->cubes.count(decodedKey)' in private
assert private.index('work.decoded->images.count(decodedKey)') < private.index('global.find(key)')
for forbidden in ('LoadFallout','gTextureCache.emplace','gCubeTextureCacheQ2050.emplace','Q1900MarkGpuTextureKnown'):
    assert forbidden not in private
assert 'gpu.teleport.valid && !gFo3ResidentTextureUpload' in s
stamp=s[s.index('bool Fo3ResidentPacketCurrent('):s.index('void DeleteFo3ResidentTexture(')]
for guard in ('Fo3AcceptsScenePublication(cpu.publication)', 'gFo3VisualPublication.Revision()',
              'gCurrentCellFormId','gExteriorWorldspaceQ1890','gSceneCenterXQ1730','gSceneCenterYQ1730','gSceneFloorZQ1730'):
    assert guard in stamp
advance=s[s.index('fo3scene::GpuStageResult AdvanceFo3ResidentGpuStage('):s.index('bool ReleaseFo3ResidentAssetLane(')]
assert advance.index('!Fo3ResidentPacketCurrent(*work.cpu)') < advance.index('UploadCpuObject(')
assert advance.index('UploadCpuObject(') < advance.index('PrepareFo3ResidentVisualPlan(')
assert 'CommitFo3ResidentVisualPlan' not in advance
for region in (begin,teardown):
    assert region.index('DiscardFo3ResidentGpuStage()') < region.index('DiscardFo3ResidentVisualPlan()')
assert 'object.residentTextures.reset();' in s[s.index('void Q1900DeleteGpuShape('):s.index('void Q1900CancelPending(')]
print('Resident texture isolation, worker stamps, private upload admission and context teardown passed')

# The reservation stops ordinary consumers as well as launches. Completed work
# remains parked rather than advancing collision/visual/terrain publications.
for source,signature in [
    (s,'void UpdateFo3ExteriorStreamingQ1890('),
    (s,'void Q230PumpNpcArrivals(){'),
    (s,'bool Q1900BeginStream('),
    (s,'void Q1900AdvanceStream() {'),
    (s,'void Q1970ConsumeLodWorkersQ19() {'),
    (s,'void Q1970AdvanceLodGpuQ19() {'),
    (c,'void Q1900ConsumeWorkerQ19() {'),
    (c,'void Q1900LaunchWorkerQ19('),
    (c,'void Q1900TryPublishCollisionQ19('),
    (c,'void Q1900MaintainTerrainQ19('),
    (c,'void Q1900UpdateCellStreamingQ19('),
]:
    body=source[source.index(signature):].split('{',1)[1]
    assert 'Fo3ResidentAssetLaneBlocked()' in body[:200],signature
activity=s[s.index('fo3scene::ResidentAssetActivity CaptureFo3ResidentAssetActivity() {'):s.index('void Q1990RenderNativeLod(bool alphaPass) {')]
for busy in ('activity.cells','activity.collision','activity.lod','activity.terrain','activity.npc','activity.legacy'):
    assert busy in activity
assert 'memory_order_acquire' in activity
for forbidden in ('Consume','PublishFo3','Reset','glDelete','join('):
    assert forbidden not in activity
terrain=Path('app/src/main/cpp/rendering/terrain/fo3-terrain-stream.inc').read_text()
probe=terrain[terrain.index('bool IsFo3TerrainStreamingCpuQuiescentQ2000() {'):terrain.index('#include "fo3-terrain-parked-work.inc"')]
assert 'memory_order_acquire' in probe and 'Consume' not in probe and 'join(' not in probe
lane_start=s[s.index('bool StartFo3ResidentCpuPreparation('):s.index('bool TakeFo3ResidentCpuPreparation(')]
assert lane_start.index('gFo3ResidentLane.Exclusive(ticket)') < lane_start.index('live.reserve(gObjects.size())') < lane_start.index('gFo3ResidentLane.Start(ticket,')
immutable=lane_start[lane_start.index('[context,scope,publication,visualRevision,ticket,'):]
for forbidden in ('gObjects','gPlayerSession','gFo3ScenePublication','gFo3VisualPublication','gWorldStreaming'):
    assert forbidden not in immutable
assert 'result.assetTicket=ticket' in immutable
assert 'gFo3ResidentLane.Exclusive(cpu.assetTicket)' in stamp
cancel=s[s.index('bool CancelFo3ResidentPreparation('):s.index('// Reserved one-turn commit executor.')]
assert cancel.index('gFo3ResidentLane.Owns(ticket)') < cancel.index('DiscardFo3ResidentGpuStage()') < cancel.index('gFo3ResidentLane.Cancel(ticket)')
assert cancel.index('gFo3ResidentVisualPlan->Applied()') < cancel.index('DiscardFo3ResidentVisualPlan()')
release=s[s.index('bool ReleaseFo3ResidentAssetLane('):s.index('bool CancelFo3ResidentPreparation(')]
assert release.index('gFo3ResidentGpuWork||gFo3ResidentVisualPlan') < release.index('gFo3ResidentLane.Release(ticket)')
for region in (begin,teardown):
    assert region.index('ResetFo3ResidentPreparationLane()') < region.index('DiscardFo3ResidentGpuStage()')
assert 'gFo3LegacyMetadataWorkers.push_back(task)' in s
assert 'const auto worker=weak.lock()' in activity
print('Resident asset reservation gates consumers/launches and acquires all six worker families before CPU/GPU use')

# Candidate collision uses immutable policy/door/collection inputs and private
# model/chunk storage; global transformed/negative caches stay untouched.
physics=Path('app/src/main/cpp/physics/fo3-collision-runtime.cpp').read_text()
private_collision=physics[physics.index('bool PrepareFo3ResidentCollisionSnapshot('):physics.index('static bool Q1930CanExchangeMutation(')]
for forbidden in ('ConfigureFo3CollisionPolicyQ710','PrimeFo3CollisionPlacementCacheQ1820',
                  'ResetFo3CollisionSceneCache','gQ1990CollisionPlacementCache',
                  'gQ1820NegativeCollisionPlacementCache','gQ1820NoCollisionModels',
                  'gDoorCollisionPoses','gCollectedCollisionRefs'):
    assert forbidden not in private_collision
assert 'BuildFo3CollisionPolicyQ710(placements)' in private_collision
assert 'LoadFo3NifCollisionShapesComplete(p.modelPath' in private_collision
assert 'LoadFo3PackedMetadataQ714(p.modelPath,next.metadata,true)' in private_collision
assert 'DiscardFo3CollisionSnapshotQ1930(token)' in private_collision
assert '&published,&chunks,&policy' in private_collision
exchange=physics[physics.index('bool ExchangeFo3CollisionMutationSnapshot('):physics.index('bool PublishFo3CollisionSnapshotQ1930(')]
assert exchange.index('Q1930CanExchangeMutation(snapshot)') < exchange.index('Fo3DynamicOnlyCollisionModelsQ710().swap(snapshot.policyPayload)') < exchange.index('gWorldTriangles.swap(')
assert 'snapshot.policyApplied=!snapshot.policyApplied' in exchange
assert 'CanExchangeFo3CollisionMutationSnapshot(cpu.collision.Get())' in stamp
assert lane_start.index('CaptureFo3CollisionMutationState()') < lane_start.index('gFo3ResidentLane.Start(ticket,')
assert 'collisionScope,collisionState' in immutable
assert immutable.index('LoadFo3WorldspacePlacements(collisionScope') < immutable.index('PrepareFo3ResidentCollisionSnapshot(')
assert 'result.collision=Fo3CollisionSnapshotOwner(collisionToken)' in immutable
print('Resident collision preparation owns private policy/chunks/token and validates collision inputs before GPU admission')

# Interior light resources are decoded off-thread and exchanged as a coupled
# snapshot/placed-light bundle without reloading room environment or image space.
assert 'collisionState,lightRevision' in immutable
assert 'PrepareResidentInteriorLighting(' in immutable
for forbidden in ('gFo3InteriorLighting','gFo3PlacedLights','gFo3LightPublication'):
    assert forbidden not in immutable
assert 'cpu.lightRevision==gFo3LightPublication.Revision()' in stamp
light_plan=s[s.index('bool PrepareFo3ResidentLightPlan('):s.index('// Render-thread-only private GPU preparation.')]
assert light_plan.index('SameResidentLighting(gFo3InteriorLighting,cpu.lighting.before)') < light_plan.index('Fo3ResidentLightPlan::Prepare(')
assert 'MatchesFo3PlacedInteriorLights(gFo3InteriorLighting,gFo3PlacedLights)' in light_plan
for forbidden in ('PublishFo3InteriorLighting(','gFo3Environment=','gFo3ImageSpace=','LoadFo3CellEnvironment('):
    assert forbidden not in light_plan
assert advance.index('PrepareFo3ResidentLightPlan(') < advance.index('PrepareFo3ResidentVisualPlan(')
assert 'DiscardFo3ResidentLightPlan();DiscardFo3ResidentDoorInteractionPlan();' in advance
assert 'DiscardFo3ResidentReferencePublicationPlan();gFo3ResidentInteractionAdmission.reset();' in advance
assert 'gFo3ResidentLightPlan' in release
assert cancel.index('gFo3ResidentLightPlan->Applied()') < cancel.index('DiscardFo3ResidentLightPlan()')
for region in (begin,teardown):
    assert region.index('DiscardFo3ResidentVisualPlan()') < region.index('DiscardFo3ResidentLightPlan()')
static_lights=s[s.index('void PrepareFo3InteriorObjectLights('):s.index('void PrepareFo3InteriorSceneLights() {')]
assert 'object.interiorLightRevision==gFo3LightPublication.Revision()' in static_lights
assert 'object.interiorLightRevision=gFo3LightPublication.Revision()' in static_lights
publication=Path('app/src/main/cpp/rendering/environment/fo3-interior-publication.h').read_text()
assert publication.index('gFo3LightPublication.Changed()') < publication.index('gFo3InteriorLighting=')
visual_depth=Path('app/src/main/cpp/rendering/environment/fo3-visual-depth.h').read_text()
reset_lights=visual_depth[visual_depth.index('inline void ResetFo3PlacedLights() {'):visual_depth.index('inline bool LoadFo3PlacedLights(')]
assert reset_lights.index('gFo3LightPublication.Changed()') < reset_lights.index('gFo3PlacedLights.clear()')
print('Interior light resources capture privately, exchange without environment resets and invalidate static/actor light selection')

assert "cachedLightRevision != 0 && cachedLightRevision == gFo3LightPublication.Revision()" in s
assert "cachedLightRevision = gFo3LightPublication.Revision()" in s

# Load-door fallback anchors and prompt metadata/text share one revisioned owner.
doors=Path('app/src/main/cpp/world/interaction/fo3-door-resources.cpp').read_text()
legacy=Path('app/src/main/cpp/world/interaction/fo3-authored-door-cache.inc').read_text()
cell_world=Path('app/src/main/cpp/world/fo3-cell-world.cpp').read_text()
assert 'auto& gCellCaches = Fo3LiveDoorResources().cells' in legacy
anchor_build=legacy[legacy.index('const std::vector<Anchor>& AnchorsForCell('):]
assert anchor_build.index('cached->second.attempted') < anchor_build.index('MarkFo3DoorResourcesChanged()') < anchor_build.index('gCellCaches[cellFormId]')
assert 'QueryFo3DoorAnchorResources(Q1730Door::AnchorsForCell(cellFormId)' in legacy
assert 'auto& gDoorMetadata = Fo3LiveDoorResources().metadata' in cell_world
assert 'auto& gPrompts = Fo3LiveDoorResources().prompts' in cell_world
for signature,mutation in [('bool EnsureDoorMetadata(', 'gDoorMetadata[sourceDoorRef] = meta'),
                            ('void CacheFo3DoorPromptQ1840(', 'Q1840Prompt::gDoorMetadata[sourceDoorRef] = meta'),
                            ('bool GetFo3DoorPromptQ1840(', 'Q1840Prompt::gPrompts.emplace')]:
    region=cell_world[cell_world.index(signature):]
    assert region.index('MarkFo3DoorResourcesChanged()') < region.index(mutation)
assert 'FormatFo3DoorPrompt(doorName,destination,out)' in cell_world
assert lane_start.index('CaptureFo3DoorResources()') < lane_start.index('gFo3ResidentLane.Start(ticket,')
assert 'capturedDoors=std::move(capturedDoors)' in immutable and 'PrepareFo3ResidentDoorResources(' in immutable
for forbidden in ('Fo3LiveDoorResources','GetFo3DoorResourceRevision','gCellCaches','gDoorMetadata','gPrompts'):
    assert forbidden not in immutable
assert 'cpu.doorRevision==GetFo3DoorResourceRevision()' in stamp
assert advance.index('PrepareFo3ResidentDoorInteractionPlan(') < advance.index('PrepareFo3ResidentLightPlan(')
assert 'HasFo3ResidentDoorPlan()' in release
assert cancel.index('Fo3ResidentDoorPlanApplied()') < cancel.index('DiscardFo3ResidentDoorInteractionPlan()')
for region in (begin,teardown):
    assert region.index('ResetFo3ResidentPreparationLane()') < region.index('DiscardFo3ResidentDoorInteractionPlan()')
for signature in ('bool CommitFo3ResidentDoorInteractionPlan()', 'bool RollbackFo3ResidentDoorInteractionPlan()'):
    region=s[s.index(signature):].split('\n}',1)[0]
    assert '!gSceneLoad.active' in region and 'gFo3ResidentLane.Exclusive(gFo3ResidentDoorTicket)' in region
prepare=doors[doors.index('bool PrepareFo3ResidentDoorResources('):doors.index('bool PrepareFo3ResidentDoorPlan(')]
for forbidden in ('Fo3LiveDoorResources','Publication()','Pending()','PlayerSession','glGen'):
    assert forbidden not in prepare
print('Load-door resources capture privately, guard epochs and couple targeting/prompts through lifecycle checks')

# Transient menus/physics are rejected, never silently closed or reset to admit
# an exchange. Recheck before worker capture, every GPU step and final commits.
probe=Path('app/src/main/cpp/world/fo3-resident-interaction-runtime.inc').read_text()
for forbidden in ('Poll','Cancel','CloseFo3','EndContainer','Reset','emplace','push_back','operator[]'):
    assert forbidden not in probe
for focus in ('Fo3DialogueFocus()','Fo3BarterFocus()','Fo3PipboyFocus()',
              'Fo3ScriptMessageFocus()','Fo3LockpickFocus()','Fo3LockpickInputBlocked()',
              'GetFo3LootPanel().reference','weaponruntime::held','weaponruntime::magazine',
              'weaponruntime::charging','gShoulderGesture.reference'):
    assert focus in probe
for hazard in ('catalog.actorPlacements','catalog.actorInventories','player.Snapshot().actors',
               'gQ230NpcActors','gQ220Grab','player.IsCollected','gQ223DynamicBodies',
               'catalog.items','gQ2400InteriorDoors','catalog.pipboy.furniture'):
    assert hazard in probe
assert lane_start.index('ResidentInteractionAdmission::Check(') < lane_start.index('live.reserve(gObjects.size())')
assert 'ResidentInteractionAdmission::Check(cpu.affected' in stamp
assert 'result.affected=affected' in immutable
for forbidden in ('CaptureFo3ResidentInteractionActivity','Fo3ResidentInteractionHazard','gFo3ResidentInteractionAdmission'):
    assert forbidden not in immutable
assert advance.index('PrepareFo3ResidentInteractionAdmission(') < advance.index('PrepareFo3ResidentDoorInteractionPlan(')
for signature,exchange in [('bool CommitFo3ResidentVisualPlan()', 'gFo3ResidentVisualPlan->Commit('),
                           ('bool CommitFo3ResidentLightPlan()', 'gFo3ResidentLightPlan->Commit('),
                           ('bool CommitFo3ResidentDoorInteractionPlan()', 'CommitFo3ResidentDoorPlan()')]:
    region=s[s.index(signature):].split('\n}',1)[0]
    assert region.index('Fo3ResidentInteractionAdmissionCurrent()') < region.index(exchange)
for signature in ('bool RollbackFo3ResidentVisualPlan()', 'bool RollbackFo3ResidentLightPlan()', 'bool RollbackFo3ResidentDoorInteractionPlan()'):
    assert 'Fo3ResidentInteractionAdmissionCurrent()' not in s[s.index(signature):].split('\n}',1)[0]
assert release.index('gFo3ResidentLane.Release(ticket)') < release.index('gFo3ResidentInteractionAdmission.reset()')
assert cancel.index('gFo3ResidentLane.Owns(ticket)') < cancel.index('gFo3ResidentInteractionAdmission.reset()')
staged=s[s.index('bool PrepareFo3ResidentInteractionAdmission('):s.index('// Render-thread-only private GPU preparation.')]
assert 'object.q2400SwingDoor||object.q220LooseObject' in staged
assert 'object.baseRecordType=="FURN"' in staged
print('Resident interaction preflight rejects live/staged hazards and guards worker/GPU/commit without restricting rollback')

# The precomputed canonical rollback projection travels only as an owned,
# immutable worker value, then couples the scene publication and fast cache.
assert lane_start.index('ValidResidentReferenceProjections(') < lane_start.index('gFo3ResidentLane.Start(ticket,')
assert 'rollback=std::move(rollback)' in immutable and 'result.rollback=rollback' in immutable
assert advance.index('PrepareFo3ResidentReferencePublicationPlan(') < advance.index('PrepareFo3ResidentVisualPlan(')
reference_plan=s[s.index('bool PrepareFo3ResidentReferencePublicationPlan('):s.index('// Render-thread-only private GPU preparation.')]
assert 'cpu.published,cpu.candidate,cpu.rollback' in reference_plan
assert 'ReferenceExchange::Prepare(gFo3ScenePublication,gFo3ReferenceState' in reference_plan
for forbidden in ('CommitScriptTransaction','RollbackScriptTransaction','FlushFo3PlayerState','PublishFo3Collision'):
    assert forbidden not in reference_plan
assert 'Snapshot().referenceStateRevision' in reference_plan
assert 'gFo3ResidentReferencePublicationPlan' in release
assert cancel.index('gFo3ResidentReferencePublicationPlan->Applied()') < cancel.index('DiscardFo3ResidentReferencePublicationPlan()')
reset=s[s.index('void ResetFo3ResidentPreparationLane(){'):s.index('bool Fo3ResidentLaneContextCurrent()')]
assert reset.index('gFo3ResidentLane.Reset()') < reset.index('DiscardFo3ResidentReferencePublicationPlan()')
rollback_reference=reference_plan[reference_plan.index('bool RollbackFo3ResidentReferencePublicationPlan()'):]
assert 'Fo3ResidentInteractionAdmissionCurrent()' not in rollback_reference
print('Reference publication/cache exchange owns monotonic candidate/rollback projections and follows lane lifecycle guards')

executor=s[s.index('struct Fo3ResidentExchangeAdapter {'):s.index('// Reserved asynchronous owner.')]
for proof in ('&cpu==gFo3ResidentPreparedPacket','cpu.collision.Get()==gFo3ResidentPreparedCollision',
              'script.CandidateReferences()==cpu.candidate','script.RollbackReferences()==cpu.rollback',
              'script.AffectedReferences()==cpu.affected','CanCommitScriptTransaction(script)',
              'CanCommitFo3ResidentDoorPlan()','CanPublishFo3ResidentReferencesBeforeCommit()'):
    assert proof in executor
for forbidden in ('FlushFo3','Poll','Pump','glDraw','ExecuteQuest','DispatchReferenceEvent'):
    assert forbidden not in executor
for forward,undo in [('CommitScriptTransaction(script)','RollbackScriptTransaction(script)'),
                      ('CommitFo3ResidentLightPlan()','RollbackFo3ResidentLightPlan()'),
                      ('CommitFo3ResidentVisualPlan()','RollbackFo3ResidentVisualPlan()'),
                      ('CommitFo3ResidentDoorInteractionPlan()','RollbackFo3ResidentDoorInteractionPlan()'),
                      ('CommitFo3ResidentReferencePublicationPlan()','RollbackFo3ResidentReferencePublicationPlan()')]:
    assert forward in executor and undo in executor
assert executor.count('return ExchangeFo3CollisionMutationSnapshot(cpu.collision.Get())')==2
assert 'gPlayerSession->saveBlocked=true' in executor and 'gSceneReady=false' in executor
assert 'gFo3ResidentExchangeRecoveryFailed' in release and 'gFo3ResidentExchangeRecoveryFailed' in cancel
interaction=Path('app/src/main/cpp/world/interaction/fo3-interaction-runtime.inc').read_text()
delivery=interaction[interaction.index('void DeliverFo3ScriptSounds()'):interaction.index('fo3loot::Cursor gLootCursor;')]
assert delivery.index('Fo3ResidentFrameBlocked()') < delivery.index('PollQuestNotice')
assert advance.index('if(prepared){') < advance.index('gFo3ResidentPreparedPacket=work.cpu.get()') < advance.index('completed=std::move(work.cpu)')
for signature in ('void DiscardFo3ResidentReferencePublicationPlan()', 'void DiscardFo3ResidentVisualPlan()',
                  'void DiscardFo3ResidentLightPlan()', 'void DiscardFo3ResidentDoorInteractionPlan()'):
    body=s[s.index(signature):].split('}',1)[0]
    mutation='DiscardFo3ResidentDoorPlan()' if 'DoorInteraction' in signature else '.reset()'
    assert body.index('InvalidateFo3ResidentPreparedBinding()') < body.index(mutation)
flush=s[s.index('void FlushFo3PlayerState()'):s.index('bool DrainFo3TransitionBackgroundWork();')]
assert flush.index('gFo3ResidentExchangeRecoveryFailed') < flush.index('WeaponPersistWorld()') < flush.index('gPlayerSession->Flush(')
print('Composite executor binds canonical/prepared resources, maps all exchanges/undo and blocks success/save/effects on recovery failure')

frame_gate=s[s.index('bool Fo3ResidentFrameBlocked(){'):s.index('void ResetFo3ResidentPreparationLane(){')]
assert 'gFo3ResidentLane.Blocked()||gFo3ResidentWorkflow.Active()||gFo3ResidentExchangeExecuting||gFo3ResidentExchangeRecoveryFailed' in frame_gate
acquire=s[s.index('uint64_t AcquireFo3ResidentAssetLane()'):s.index('bool PollFo3ResidentAssetLane(')]
assert acquire.index('CaptureFo3ResidentInteractionActivity().Idle()') < acquire.index('gFo3ResidentLane.Acquire()')
assert flush.index('Fo3ResidentFrameBlocked()') < flush.index('WeaponPersistWorld()')
assert flush.index('gPlayerSession->saveBlocked') < flush.index('WeaponPersistWorld()')
assert 'gFo3ResidentSavePending=true;return;' in flush and 'FlushFo3DeferredPlayerState()' in flush
actor_frame=s[s.index('void QActorPrepareStereoFrame() {'):]
assert actor_frame.index('Fo3ResidentFrameBlocked()') < actor_frame.index('Q230SimulateActor(')
loose=s[s.index('void Q221UpdateLooseObjectsFromSolvedPalms('):s.index('std::string Q211Lower(')]
assert loose.index('Fo3ResidentFrameBlocked()') < loose.index('Q223AdvanceDynamicBodies()')
loop=Path('app/src/main/cpp/core/fo3-runtime-loop.inc').read_text()
assert 'if (!Fo3ResidentFrameBlocked() && gPlayerSession' in loop
assert 'if (!Fo3ResidentFrameBlocked() && !IsFo3LoadingVisible())' in loop
# Check actual block nesting so a later misplaced brace cannot silently ungate
# a UI, physics, script or activation call. Preparation and tracking stay live.
def guarded(call):
    prefix=loop[:loop.index(call)]
    stack=[]
    for line in prefix.splitlines():
        for ch in line:
            if ch=='{': stack.append(line)
            elif ch=='}' and stack: stack.pop()
    return any('if(!Fo3ResidentFrameBlocked())' in line for line in stack)
for call in ('UpdateFo3Pipboy(gStereoFrame,','UpdateFo3Lockpick(static_cast',
             'UpdateFo3ScriptMessage(static_cast','UpdateFo3Dialogue(static_cast',
             'WeaponUpdate(static_cast','DeliverFo3ScriptSounds();',
             'UpdateFo3InteriorDoorsQ2400();','UpdateDoorAimQ1700(static_cast',
             'ActivateFo3Interaction('):
    assert guarded(call), call
for call in ('UpdateFo3ExteriorStreamingQ1890(q1890VirtualHead.',
             'SetFo3PlayerBodyTrackingQ210(','FlushFo3DeferredPlayerState();'):
    assert not guarded(call), call
assert 'residentInputBarrier_.Allow(Fo3ResidentFrameBlocked(),neutral)' in loop
assert 'residentInputBarrier_.Allow(true,false)' in loop
assert loop.index('previousWorldClockNs=clockNs;') > loop.index('scriptPlayer.PumpQuestGameMode(seconds)')
print('Reservation excludes frame mutations/save/effects while retaining tracking and asset progress; held controls require release')

workflow=s[s.index('// Reserved asynchronous owner.'):s.index('// Dynamic NPC arrivals use')]
for binding in ('CanCommitScriptTransaction(script)','script.CandidateReferences()',
                'script.RollbackReferences()','script.AffectedReferences()',
                'PollFo3ResidentAssetLane(ticket)','StartFo3ResidentCpuPreparation(ticket,',
                'TakeFo3ResidentCpuPreparation(ticket,packet)','BeginFo3ResidentGpuStage(packet)',
                'AdvanceFo3ResidentGpuStage(packet)','CancelFo3ResidentPreparation(ticket)',
                'ExecuteFo3ResidentPreparedExchange(packet,script)'):
    assert binding in workflow,binding
assert 'gFo3ResidentWorkflow.OwnsExchange(cpu.assetTicket,cpu,script)' in executor
assert 'gFo3ResidentWorkflow.Active()' in release and 'gFo3ResidentWorkflowCleanup' in cancel
assert reset.index('gFo3ResidentLane.Reset()') < reset.index('gFo3ResidentWorkflow.ResetAfterDrain()')
pump=loop.index('PumpFo3ResidentWorkflow(sessionState_==XR_SESSION_STATE_FOCUSED)')
assert loop.index('xrBeginFrame(session_, &beginInfo)') < pump < loop.index('if (frameState.shouldRender)')
assert not guarded('PumpFo3ResidentWorkflow(')
assert loop.index('if(sessionState_!=XR_SESSION_STATE_FOCUSED)PumpFo3ResidentWorkflow(false)') < loop.index('SetFo3ShoulderInteractionFocused(')
native_pump=workflow[workflow.index('void PumpFo3ResidentWorkflow('):workflow.index('fo3scene::ResidentExchangeResult ExecuteFo3ResidentWorkflow(')]
assert 'if(!focused)' in native_pump and 'RequestCancel' in native_pump
assert 'ExecuteFo3' not in native_pump
factory=workflow[workflow.index('uint64_t BeginFo3ResidentWorkflow('):workflow.index('bool CancelFo3ResidentWorkflow(')]
assert factory.index('AcquireFo3ResidentAssetLane()') < factory.index('prepare(player)') < factory.index('gFo3ResidentWorkflow.Begin(')
assert 'const fo3player::Player&' in factory and 'CancelFo3ResidentPreparation(ticket)' in factory
for forbidden in ('FlushFo3','PollScript','PollQuest','DispatchReferenceEvent','glDraw','PumpFo3AndroidEvents'):
    assert forbidden not in workflow
print('Native workflow owns the script/packet, advances outside gameplay exclusion and retains all settled owners for finalization')

finalizer=workflow[workflow.index('bool CanFinalize('):workflow.index('uint64_t BeginFo3ResidentWorkflow(')]
for proof in ('CanFinalizeScriptTransaction(script,committed)',
              'gFo3ResidentVisualPlan->CanFinalize(', 'gFo3ResidentLightPlan->CanFinalize(',
              'CanFinalizeFo3ResidentDoorPlan(committed)', 'gFo3ResidentReferencePublicationPlan->CanFinalize(',
              'cpu.settledPublication==gFo3ScenePublication.Stamp()',
              'cpu.settledCollision==GetFo3CollisionPublicationRevision()',
              'gFo3ResidentLane.CanRelease(ticket)','CaptureFo3ResidentAssetActivity().Quiescent()',
              'CanExchangeFo3CollisionMutationSnapshot(cpu.collision.Get())'):
    assert proof in finalizer,proof
assert 'ResidentWorkflowPhase::Finalizing' in release
assert executor.index('ExecuteResidentExchange(adapter)') < executor.index('cpu.settledPublication=')
owner=Path('app/src/main/cpp/world/fo3-resident-workflow.h').read_text()
finish=owner[owner.index('bool Finalize('):owner.index('// Scene/context teardown only')]
assert finish.index('adapter.Reconcile(outcome_)') < finish.rindex('adapter.CanFinalize(') < finish.index('adapter.Retire(*packet_);') < finish.index('if(!adapter.Release(ticket))') < finish.rindex('Clear();')
reconcile=Path('app/src/main/cpp/world/fo3-resident-reconcile-runtime.inc').read_text()
body=reconcile[reconcile.index('bool ReconcileFo3ResidentParkedWork('):]
assert body.index('CaptureFo3ResidentAssetActivity().Quiescent()') < body.index('auto npcPending=') < body.index('DiscardFo3TerrainParkedWorkQ2000()') < body.index('Q230ResetNpcArrivals()')
for queues in ('gWorldStreaming.cells.clear()', 'worker.reset()', 'gWorldStreaming.collisionTask.reset()',
               'gQ2013LodUploadsQ19.clear()', 'gFo3LegacyMetadataWorkers.clear()',
               'gQ230NpcArrival.pending.swap(npcPending)'):
    assert queues in body,queues
for forbidden in ('gObjects','gQ230NpcActors','CommitFo3','PublishFo3','gPlayerSession','FlushFo3','Pump','glDraw'):
    assert forbidden not in reconcile,forbidden
terrain_cleanup=Path('app/src/main/cpp/rendering/terrain/fo3-terrain-parked-work.inc').read_text()
assert terrain_cleanup.index('IsFo3TerrainStreamingCpuQuiescentQ2000()') < terrain_cleanup.index('.join()') < terrain_cleanup.index('gQ2000Worker.reset()')
for forbidden in ('q711TerrainBatches','gQ2000GroundCells','gQ2000ResidentValid','gQ2000WindowKeys','Publish','Consume','Launch'):
    assert forbidden not in terrain_cleanup,forbidden
assert '#include "world/fo3-resident-reconcile-runtime.inc"' in s
assert '#include "fo3-terrain-parked-work.inc"' in terrain
print('Settled finalization validates epochs/canonical owners, reconciles private queues and retires before resuming; live LAND/scene/actors stay untouched')
