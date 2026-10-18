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
    def test_gpu_backlog_never_blocks_waiting_for_completion(self):
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        self.assertIn('glClientWaitSync(gLoadingUploadFence,0,0)',loop)
        self.assertNotIn('glFinish(',loop)
        self.assertIn('ShutdownFo3LoadingVr();',loop)

if __name__=='__main__': unittest.main()
