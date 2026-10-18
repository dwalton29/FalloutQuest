"""Guard the XR/loading ownership boundaries that host GLES mocks cannot model."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]/'app/src/main/cpp'

class LoadingPipeline(unittest.TestCase):
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
        self.assertLess(begin.index('gSceneLoad.doorPreparation.Start('),
                        begin.index('gSceneLoad.preparation.Start('))
        self.assertLess(scene.index('PublishFo3AuthoredDoorAnchorsQ2190('),
                        scene.index('PrimeFo3AuthoredDoorAnchorsQ1870('))
        self.assertIn('MarkFo3DoorResourcesChanged()',cache)
        self.assertIn('FINALIZE_STEP generation=',runtime)
        self.assertIn('DOOR_WORKER_READY generation=',runtime)

    def test_gpu_backlog_never_blocks_waiting_for_completion(self):
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        self.assertIn('glClientWaitSync(gLoadingUploadFence,0,0)',loop)
        self.assertNotIn('glFinish(',loop)
        self.assertIn('ShutdownFo3LoadingVr();',loop)

if __name__=='__main__': unittest.main()
