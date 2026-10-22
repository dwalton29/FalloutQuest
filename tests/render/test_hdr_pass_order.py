"""Verify production scheduling and retained water/depth consumers."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
LOOP=(ROOT/'app/src/main/cpp/core/fo3-runtime-loop.inc').read_text()
class HdrPassOrder(unittest.TestCase):
    def test_water_visibility_precedes_main_clear_without_a_reflection_pass(self):
        scene=CORE.split('void RenderScene(const float* mvp)',1)[1].split('void Q6HGenFramebuffers',1)[0]
        # Q244 keeps authored water/refraction but never submits a mirrored
        # Wasteland scene or its reflection FBO before either stereo eye.
        visibility=scene.index('IsFo3WaterPotentiallyVisibleQ209B(')
        disabled=scene.index('q2090ReflectionReady = false;')
        begin=scene.index('Q233BeginMainEyeWorld();',disabled)
        restore=scene.index('glUniformMatrix4fv(gMvpLocation,1,GL_FALSE,mvp);',begin)
        opaque=scene.index('Q1990RenderNativeLod(false);',restore)
        terrain=scene.index('RenderFo3CollisionOverlay(mvp);',opaque)
        snapshot=scene.index('Q2060ResolveEyeMsaaQ2060();',terrain)
        water=scene.index('RenderFo3WaterSurfaceQ2070(',snapshot)
        self.assertLess(visibility,disabled)
        self.assertLess(disabled,begin)
        self.assertLess(begin,restore)
        self.assertLess(restore,opaque)
        self.assertLess(opaque,terrain)
        self.assertLess(terrain,snapshot)
        self.assertLess(snapshot,water)
        self.assertNotIn('Q2090RenderWaterReflection(mvp,',scene)
        self.assertIn('q2090HavePlane && q209bExposedCells > 0u',scene[:disabled])
        self.assertIn('q2090ReflectionReady ? gWaterReflectionColorQ2090 : 0u',scene)
        self.assertIn('q2090ReflectionReady ? q2090ReflectionMvp : nullptr',scene)
        eye=LOOP.split('bool RenderEye(',1)[1].split('\nprivate:',1)[0]
        self.assertNotIn('glClear(',eye)
        self.assertNotIn('RenderFo3PcSky(',eye)
    def test_clear_honours_depth_mask_and_interior_loading_guards(self):
        begin=CORE.split('void Q233BeginMainEyeWorld()',1)[1].split('void RenderScene(',1)[0]
        self.assertLess(begin.index('glDepthMask(GL_TRUE)'),begin.index('glClear('))
        self.assertIn('glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT)',begin)
        self.assertIn('glDepthMask(depthWrite)',begin)
        self.assertIn('!IsFo3LoadingVisible() && !gFo3Environment.interior',begin)
        mirror=CORE.split('bool Q2090RenderWaterReflection(',1)[1].split('struct Q1970RenderStallScopeQ19',1)[0]
        self.assertLess(mirror.index('glDepthMask(GL_TRUE)'),mirror.index('glClear('))
        self.assertIn('std::copy(cachedReflection, cachedReflection+16, outReflectionMvp)',mirror)
        self.assertIn('glDepthMask(previousDepthMask)',mirror)
    def test_final_depth_and_water_snapshot_still_resolve(self):
        resolve=CORE.split('void Q2060ResolveEyeMsaaQ2060(GLbitfield mask) {',1)[1].split('bool Q1280AllocatePostTarget',1)[0]
        self.assertIn('void Q2060ResolveEyeMsaaQ2060(GLbitfield mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);',CORE)
        scene=CORE.split('void RenderScene(const float* mvp)',1)[1].split('void Q6HGenFramebuffers',1)[0]
        self.assertIn('q209bWaterInFrustum && q2060MsaaActive',scene)
        self.assertIn('q2080SceneSnapshotReady ? q1370PostDepth : 0u',scene)
        self.assertIn('fqperf::ContinueHdrPass()',scene)
        post=CORE.split('void Q1350CompositeEyePostQ1350(',1)[1].split('void Q1280ShutdownPost',1)[0]
        self.assertLess(post.index('Q2060ResolveEyeMsaaQ2060(q234FinalResolveMask)'),post.index('fqperf::EndHdrPass()'))
        self.assertLess(post.index('fqperf::EndHdrPass()'),post.index('Q1350UpdateExposureQ1350()'))
        self.assertIn('GL_DEPTH_BUFFER_BIT,GL_NEAREST',LOOP) # compositor depth survives
    def test_v234_reuses_only_unchanged_pre_water_depth(self):
        scene=CORE.split('void RenderScene(const float* mvp)',1)[1].split('void Q6HGenFramebuffers',1)[0]
        self.assertIn('q234PreWaterDepthResolved = true;',scene)
        self.assertLess(scene.index('Q2060ResolveEyeMsaaQ2060();'),scene.index('q234PreWaterDepthResolved = true;'))
        begin=CORE.split('bool Q1280BeginEyePostQ1280(',1)[1].split('void Q1280CompositeEyePost',1)[0]
        self.assertIn('q234PreWaterDepthResolved = false;',begin)
        post=CORE.split('void Q1350CompositeEyePostQ1350(',1)[1].split('void Q1280ShutdownPost',1)[0]
        self.assertIn('q234PreWaterDepthResolved&&!lateDepthWritesPossible',post)
        self.assertIn('(reusePreWaterDepth?0u:GL_DEPTH_BUFFER_BIT)',post)
        self.assertIn('q234PreWaterDepthResolved=false;',post)
        self.assertIn('IsFo3LoadingVisible() || Fo3LockpickFocus()',LOOP)
        self.assertIn('glDepthMask(GL_FALSE);',(ROOT/'app/src/main/cpp/rendering/water/fo3-water.cpp').read_text())
        self.assertIn('glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);',(ROOT/'app/src/main/cpp/ui/lockpick/fo3-lockpick-renderer.h').read_text())
        self.assertIn('glDepthMask(GL_FALSE)',(ROOT/'app/src/main/cpp/ui/interaction/fo3-health-bar-hud.h').read_text())
        self.assertIn('if(mask & GL_DEPTH_BUFFER_BIT)',CORE)
        self.assertIn('Q234 DEPTH REUSE:',CORE)
if __name__=='__main__':unittest.main()
