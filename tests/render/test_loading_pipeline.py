"""Guard the XR/loading ownership boundaries that host GLES mocks cannot model."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]/'app/src/main/cpp'

class LoadingPipeline(unittest.TestCase):
    def test_v245_wasteland_warmup_only_waits_for_requested_lod(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text()
        warmup=runtime.split('void Q2024GetAuthoredWarmupState(',2)[2].split(
            'uint64_t gQ1990NativeLodSerial',1)[0]
        # v244 made High VWD selection distance-sensitive. The loading gate
        # must no longer count all authored High VWD in the entire archive.
        self.assertIn('highReady = 0u;',warmup)
        self.assertIn('highTarget = 0u;',warmup)
        self.assertNotIn('gQ2023HighObjectBlocks.size()',warmup)
        self.assertIn('Q244HighObjectDesired(high.blockX,high.blockY,px,py)',warmup)
        self.assertIn('Q244HighObjectDesired(asset.blockX,asset.blockY,px,py)',warmup)
        self.assertIn('fqquality::CoarseDesired(16,tile.blockX,tile.blockY,px,py)',warmup)
        self.assertIn('fqquality::CoarseDesired(16,asset.blockX,asset.blockY,px,py)',warmup)
        startup=runtime.split('const bool nearLodComplete = lodReady >= 9u;',1)[1].split(
            'const uint64_t warmupUs =',1)[0]
        self.assertIn('gQ2023LodAssetsDiscovered &&',startup)
        self.assertIn('level32Ready >= level32Target &&',startup)
        self.assertIn('highReady >= highTarget;',startup)
        self.assertNotIn('highTarget > 0u',startup)
        self.assertNotIn('level32Target > 0u',startup)
        self.assertIn('Fo3LoadingExteriorReady(detailComplete, nearLodComplete, horizonComplete)',
                      runtime)
        # Actual admission and render checks must share the same cutoff.
        self.assertIn('return Q244HighObjectDesired(asset.blockX, asset.blockY,',runtime)
        self.assertIn('if (!Q244HighObjectDesired(high.blockX, high.blockY,',runtime)

    def test_arrival_heading_comes_from_commit(self):
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        facing=loop.split('    void ApplyAuthoredDoorFacingQ1810(',1)[1].split('    void RenderFrame()',1)[0]
        self.assertIn('GetFo3ArrivalFacingQ74()',facing)
        self.assertNotIn('doorAim',facing)
        self.assertNotIn('IsFo3LoadingVisible()',facing)
        world=(ROOT/'world/fo3-cell-world.cpp').read_text()
        commit=world.split('void CompleteFo3CellTransitionQ74(',1)[1].split('bool ConsumeFo3PlayerResetQ74()',1)[0]
        self.assertIn('gArrivalFacingQ74.Publish(gPendingTransitionQ74.destinationDoorRef, gPendingTransitionQ74.rz)',commit)
        interaction=loop.split('if (!dialogueControls &&',1)[1].split('FlushFo3DeferredPlayerState();',1)[0]
        self.assertNotIn('playerYaw_ = 0.0f',interaction)
        self.assertNotIn('playerPosition_ =',interaction)

    def test_loading_eye_has_no_world_or_post_work(self):
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        eye=loop.split('    bool RenderEye(',1)[1]
        loading=eye.split('            if(loadingEye) {',1)[1].split('            } else {',1)[0]
        self.assertIn('ViewFromPose(view.pose)',loading)
        self.assertIn('RenderFo3LoadingVr(',loading)
        for operation in ('RenderScene(', 'Q1280BeginEyePost', 'ToVirtualPose(', 'UpdateFo3VisualEye'):
            self.assertNotIn(operation,loading)
        preparation=loop.split('                // All loading work advances',1)[1].split('                SyncInput(',1)[0]
        self.assertIn('ProcessQ74TransitionRequest();',preparation)
        self.assertIn('fo3loadingvr::AdvanceAssets();',preparation)
        self.assertIn('if(!IsFo3LoadingVisible()) {',loop)
    def test_presentation_is_outside_legacy_gl_overrides(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text()
        self.assertLess(runtime.index('#include "ui/loading/fo3-loading-vr.h"'),
                        runtime.index('#define glDeleteFramebuffers Q6HDeleteFramebuffers'))
        render=(ROOT/'ui/loading/fo3-loading-vr.h').read_text().split('inline void Render(',1)[1].split('inline void Shutdown()',1)[0]
        self.assertNotIn('AdvanceAssets()',render)
    def test_collision_overlaps_gpu_and_publication_does_not_yield(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text().split('bool ProcessQ74TransitionRequest() {',1)[1].split('GLenum Q1150BlendFactor',1)[0]
        self.assertLess(runtime.index('gSceneLoad.collisionPreparation.Start('),runtime.index('UploadCpuObject('))
        commit=runtime.split('PublishFo3CollisionSnapshotQ1930(',1)[1].split('const auto finalizationStarted',1)[0]
        self.assertNotIn('fqloading::Allow()',commit)
        self.assertIn('superseded-generation',runtime)
        self.assertIn('reference-state-changed',runtime)
    def test_no_fixed_dwell_and_readiness_is_unchanged(self):
        state=(ROOT/'ui/loading/fo3-loading-state.h').read_text()
        self.assertNotIn('FO3_LOADING_MIN_PRESENT_FRAMES',state)
        self.assertIn('return detail && nearLod && horizon;',state)
    def test_scene_optional_visuals_cannot_abort_the_world(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text()
        scene=runtime.split('bool ProcessQ74TransitionRequest() {',1)[1].split(
            'GLenum Q1150BlendFactor',1)[0]
        # The v216 staging lane made any unsupported optional visual fatal.
        # A source-authored mesh/actor part is allowed to fail individually;
        # the scene readiness and collision checks remain mandatory.
        self.assertIn('SCENE OPTIONAL MESH SKIPPED',scene)
        self.assertIn('SCENE OPTIONAL NPC PART SKIPPED',scene)
        self.assertNotIn('AbortFo3SceneLoad("geometry-upload")',scene)
        self.assertNotIn('AbortFo3SceneLoad("npc-part-upload")',scene)
        self.assertIn('gpu-upload-too-small',scene)
        self.assertIn('collision-publication',scene)
        # Safe CPU source asset predecode, not a per-texture modal GL transfer.
        self.assertNotIn('PrepareFo3SceneImage(',runtime)
        self.assertIn('Q1900PrepareTextureCpuQ19(path);',runtime)
        self.assertIn('gQ1900PreparedTexturesQ19',runtime)

    def test_loading_profiler_records_bottlenecks_and_failed_publication(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text()
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        logger=(ROOT/'rendering/loading-diagnostics.h').read_text()

        # The profiler survives cooperative aborts, but does not change what
        # is required to publish the original scene.
        self.assertIn('fqloadprofile::Log("ABORT',runtime)
        self.assertIn('fqloadprofile::Log("CPU_READY',runtime)
        self.assertIn('fqloadprofile::Log("SCENE_READY',runtime)
        self.assertIn('fqloadprofile::Log("PROGRESS',runtime)
        self.assertIn('fqloadprofile::Log("COLLISION_READY',runtime)
        self.assertIn('fqloadprofile::Log("SLOW_MESH',runtime)
        self.assertIn('fqloadprofile::Log("SLOW_NPC',runtime)
        self.assertIn('result.meshUs=Fo3SceneElapsedUs(meshStarted)',runtime)
        self.assertIn('result.actorUs=Fo3SceneElapsedUs(actorStarted)',runtime)
        self.assertIn('result.worstTextureDecodeUs',runtime)
        self.assertIn('gSceneLoad.slowObjectUploads<=12u',runtime)

        self.assertIn('fqloadprofile::Log("UI_FRAMES',loop)
        self.assertIn('fqloadprofile::Log("PRESENTATION_COMPLETE',loop)
        self.assertIn('fqloadprofile::Log("PLAYABLE_FIRST_FRAME',loop)
        self.assertIn('fqloadprofile::Log("BLANK_WORLD',loop)
        self.assertIn('fqloadprofile::Log("LOADING_UI_CATALOG',loop)
        self.assertIn('fqloadprofile::Log("SLOW_LOADING_UI_ASSETS',loop)

        self.assertIn('com.falloutquest.app/files/loading-profile.log',logger)
        self.assertIn('FQLoadProfile',logger)
        self.assertIn('std::fflush(file)',logger)
        self.assertIn('2L*1024L*1024L',logger)
        self.assertIn('std::mutex',logger)

    def test_authored_door_cache_is_parallel_cancellable_and_committed_once(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text()
        cache=(ROOT/'world/interaction/fo3-authored-door-cache.inc').read_text()
        interior=(ROOT/'world/fo3-cell-interior.inc').read_text()
        worker=cache.split('bool PrepareWorker(',1)[1].split('const std::vector<Anchor>& AnchorsForCell(',1)[0]
        scene=runtime.split('bool ProcessQ74TransitionRequest() {',1)[1].split(
            'GLenum Q1150BlendFactor',1)[0]

        # Scan remains the original two ESM passes and XESP parent resolution.
        self.assertIn('Q1700Interior::CollectCellReferences(cellFormId, raw, &cancel)',worker)
        self.assertIn('Q1700Interior::ResolveEnabled(',worker)
        self.assertIn('Fo3DoorTeleport teleport;',worker)
        self.assertIn('ownerCells.find(teleport.destinationDoorRefFormId)',worker)
        self.assertIn('out.anchors.push_back(anchor)',worker)
        # No render-thread cache mutation, XR pumping or GL calls in worker.
        for forbidden in ('gCellCaches', 'MarkFo3DoorResourcesChanged',
                          'PumpFo3AndroidEventsQ1860(', 'glBind', 'glGen'):
            self.assertNotIn(forbidden,worker)
        self.assertIn('cancel.load(std::memory_order_acquire)',worker)
        self.assertIn('cancelled->load(std::memory_order_acquire)',interior)
        self.assertIn('gSceneLoad.doorPreparation.RequestCancel()',runtime)
        self.assertIn('!gSceneLoad.doorPreparation.Ready()',runtime)

        # Start in parallel with CPU preparation; publish on render thread
        # with original live-door fallback only after the packet is ready.
        begin=runtime.split('bool BeginFo3SceneLoad(',1)[1].split(
            'bool ProcessQ74TransitionRequest(',1)[0]
        self.assertLess(begin.index('StartFo3SceneDoorWorker(request.cellFormId);'),
                        begin.index('gSceneLoad.preparation.Start('))
        self.assertIn('StartFo3SceneDoorWorker(prepared.request.cellFormId);',scene)
        self.assertIn('if(!cellFormId || gSceneLoad.doorWorkerAttempted)return;',runtime)
        self.assertLess(scene.index('PublishFo3AuthoredDoorAnchorsQ2190('),
                        scene.index('PrimeFo3AuthoredDoorAnchorsQ1870('))
        self.assertIn('MarkFo3DoorResourcesChanged()',cache)
        self.assertIn('FINALIZE_STEP generation=',runtime)
        self.assertIn('DOOR_WORKER_READY generation=',runtime)

    def test_water_catalog_prefetch_is_cpu_only_and_generation_safe(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text()
        header=(ROOT/'rendering/water/fo3-water.h').read_text()
        water=(ROOT/'rendering/water/fo3-water.cpp').read_text()
        worker=water.split('bool PrepareFo3WaterCatalogQ2200(',1)[1].split('bool PublishFo3WaterCatalogQ2200(',1)[0]
        scene=runtime.split('bool ProcessQ74TransitionRequest() {',1)[1].split('GLenum Q1150BlendFactor',1)[0]
        self.assertIn('std::atomic<bool>& cancel',header)
        self.assertIn('ReadWorldDefaults(',worker)
        self.assertIn('CollectWaterCells(',worker)
        self.assertIn('ResolveWaterTypes(',worker)
        self.assertIn('cancel.load(',worker)
        for forbidden in ('gWaterCatalogWorldspace', 'gWaterCatalogCells',
                          'gWaterCells', 'glBind', 'glGen'):
            self.assertNotIn(forbidden,worker)
        self.assertIn('gSceneLoad.waterPreparation.RequestCancel()',runtime)
        self.assertIn('!gSceneLoad.waterPreparation.Ready()',scene)
        self.assertIn('StartFo3SceneWaterWorker(request.worldspaceFormId);',runtime)
        self.assertIn('StartFo3SceneWaterWorker(prepared.request.worldspaceFormId);',scene)
        self.assertLess(scene.index('PublishFo3WaterCatalogQ2200('),
                        scene.index('LoadFo3WaterSceneQ2070('))
        self.assertIn('WATER_WORKER_READY generation=',runtime)
        self.assertIn('WATER_WORKER_FAILED generation=',runtime)
        self.assertIn('Q22.0 TRANSITION_STEP', (ROOT/'world/fo3-cell-world.cpp').read_text())

    def test_v221_environment_prefetch_and_original_weather_preserved(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text()
        env=(ROOT/'rendering/environment/fo3-environment.h').read_text()
        xemi=(ROOT/'rendering/environment/fo3-external-emittance.h').read_text()
        weather=(ROOT/'rendering/environment/fo3-weather-imagespace.h').read_text()
        tod=(ROOT/'rendering/environment/fo3-time-of-day.h').read_text()
        scene=runtime.split('bool ProcessQ74TransitionRequest() {',1)[1].split('GLenum Q1150BlendFactor',1)[0]
        idxworker=env.split('inline bool PrepareRecordIndexQ2210(',1)[1].split('inline bool PublishRecordIndexQ2210(',1)[0]
        xworker=xemi.split('inline bool PrepareFo3ExternalEmittanceQ2210(',1)[1].split('inline bool PublishFo3ExternalEmittanceQ2210(',1)[0]
        self.assertIn('fo3esm::ReadPayloadCurrent(cached,match->second.size',env)
        self.assertIn('std::atomic_load_explicit(&gRecordIndexQ2210',env)
        self.assertIn('std::atomic_store_explicit(&gRecordIndexQ2210',env)
        self.assertIn('cancel.load(',idxworker)
        self.assertNotIn('gRecordIndexQ2210',idxworker)
        self.assertIn('cancel->load(',xworker)
        self.assertIn('prepared.entries[raw.refFormId]',xworker)
        self.assertNotIn('gFo3ExternalEmittance[raw.refFormId]',xworker)
        self.assertIn('StartFo3SceneEnvironmentIndexWorker();',runtime)
        self.assertIn('StartFo3SceneEmittanceWorker(prepared.request.worldspaceFormId);',scene)
        self.assertIn('gSceneLoad.environmentIndexPreparation.RequestCancel()',runtime)
        self.assertIn('gSceneLoad.emittancePreparation.RequestCancel()',runtime)
        self.assertIn('!gSceneLoad.environmentIndexPreparation.Ready()',scene)
        self.assertIn('!gSceneLoad.emittancePreparation.Ready()',scene)
        self.assertLess(scene.index('PublishRecordIndexQ2210('),scene.index('CompleteFo3CellTransitionQ74('))
        self.assertLess(scene.index('PublishFo3ExternalEmittanceQ2210('),scene.index('CompleteFo3CellTransitionQ74('))
        self.assertIn('ApplyWeatherImad(runtime.endpointImage[i],modifier);',tod)
        self.assertIn('inline bool ApplyWeatherImad(Fo3ImageSpace& image,const WeatherImad& modifier)',weather)
        self.assertIn('TRANSITION_STEP cell=%08X',(ROOT/'world/fo3-cell-world.cpp').read_text())

    def test_v222_no_emittance_esm_scan_in_mesh_upload_path(self):
        runtime=(ROOT/'core/fo3-runtime.cpp').read_text()
        xemi=(ROOT/'rendering/environment/fo3-external-emittance.h').read_text()
        # Stop at this function's outer closing brace, before InitializeScene.
        upload=runtime.split('bool UploadCpuObject(',1)[1].split('\n}\n',1)[0]
        scene=runtime.split('bool ProcessQ74TransitionRequest() {',1)[1].split('GLenum Q1150BlendFactor',1)[0]
        self.assertIn('TryResolvePublishedFo3ExternalEmittanceQ2220(',upload)
        self.assertNotIn('ResolveFo3ExternalEmittance(0x00000A74u',upload)
        self.assertNotIn('LoadFo3ExternalEmittance(',upload)
        self.assertIn('gQ2220UploadWorldspace=request.worldspaceFormId;',scene)
        helper=xemi.split('inline bool TryResolvePublishedFo3ExternalEmittanceQ2220(',1)[1].split('inline bool ResolveFo3ExternalEmittance(',1)[0]
        self.assertNotIn('LoadFo3ExternalEmittance(',helper)
        self.assertIn('gFo3ExternalEmittanceWorld!=worldspaceFormId',helper)
        self.assertIn('RebindFo3PublishedEmittanceQ2220(request.worldspaceFormId)',scene)
        self.assertIn('XEMI_GPU_REBIND generation=',scene)
        self.assertIn('XEMI_SCENE_FALLBACK generation=',scene)
        self.assertLess(scene.index('PublishFo3ExternalEmittanceQ2210('),
                        scene.index('RebindFo3PublishedEmittanceQ2220('))

    def test_v222_terrain_parallel_dds_and_detailed_steps(self):
        terrain=(ROOT/'rendering/terrain/fo3-terrain-render.inc').read_text()
        scene=terrain.split('bool InitializeFo3TerrainRenderQ76(',1)[1].split('void RenderFo3Terrain',1)[0]
        self.assertIn('Q2220_MAX_STAGED_BYTES=96u*1024u*1024u',scene)
        self.assertIn('q2220Workers.emplace_back(q2220Decode)',scene)
        self.assertIn('q2220Workers)if(worker.joinable())worker.join()',scene)
        self.assertIn('q2220Decode();',scene)
        self.assertIn('LoadFalloutTextureRgba(q2220Paths[i],image)',scene)
        self.assertIn('Q711UploadTexture(cpu.texturePath,',scene)
        self.assertIn('Q711UploadTexture(cpu.normalPath,',scene)
        for stage in ('load-authored-land','stitch-authored-heights',
                      'build-authored-geometry','parallel-dds-decode',
                      'compile-terrain-shader','gpu-textures-and-geometry'):
            self.assertIn('q2220Mark("'+stage+'"',scene)
        self.assertIn('const GLenum error = glGetError()',scene)
        self.assertIn('Q7.20 TERRAIN GPU READY',scene)
        self.assertIn('TERRAIN_INIT_STEP worldspace=%08X',scene)
        self.assertIn('InitializeFo3TerrainRenderQ76(', (ROOT/'world/fo3-cell-world.cpp').read_text())

    def test_gpu_backlog_never_blocks_waiting_for_completion(self):
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        self.assertIn('glClientWaitSync(gLoadingUploadFence,0,0)',loop)
        self.assertNotIn('glFinish(',loop)
        self.assertIn('ShutdownFo3LoadingVr();',loop)

if __name__=='__main__': unittest.main()
