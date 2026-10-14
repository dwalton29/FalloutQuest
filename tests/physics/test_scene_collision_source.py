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

worker=s[s.index('bool PrepareFo3ResidentCpuResources('):s.index('// Render-thread-only private GPU preparation.')]
for forbidden in ('gPlayerSession','gObjects','ConfigureFo3CollisionPolicyQ710',
                  'ResetFo3CollisionSceneCache','PrimeFo3Collision','UploadCpuObject','glGen'):
    assert forbidden not in worker
assert worker.index('SelectResidentResources(') < worker.index('PrepareResidentCpuParts(')
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
advance=s[s.index('fo3scene::GpuStageResult AdvanceFo3ResidentGpuStage('):s.index('// Dynamic NPC arrivals')]
assert advance.index('!Fo3ResidentPacketCurrent(*work.cpu)') < advance.index('UploadCpuObject(')
assert advance.index('UploadCpuObject(') < advance.index('PrepareFo3ResidentVisualPlan(')
assert 'CommitFo3ResidentVisualPlan' not in advance
for region in (begin,teardown):
    assert region.index('DiscardFo3ResidentGpuStage()') < region.index('DiscardFo3ResidentVisualPlan()')
assert 'object.residentTextures.reset();' in s[s.index('void Q1900DeleteGpuShape('):s.index('void Q1900CancelPending(')]
print('Resident texture isolation, worker stamps, private upload admission and context teardown passed')
