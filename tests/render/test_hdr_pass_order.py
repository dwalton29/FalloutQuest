"""Verify production scheduling and retained water/depth consumers."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
LOOP=(ROOT/'app/src/main/cpp/core/fo3-runtime-loop.inc').read_text()
class HdrPassOrder(unittest.TestCase):
    def test_reflection_finishes_before_main_clear_sky_and_world(self):
        scene=CORE.split('void RenderScene(const float* mvp)',1)[1].split('void Q6HGenFramebuffers',1)[0]
        prepass=scene.index('Q2090RenderWaterReflection(mvp,')
        begin=scene.index('Q233BeginMainEyeWorld();',prepass)
        restore=scene.index('glUniformMatrix4fv(gMvpLocation,1,GL_FALSE,mvp);',begin)
        opaque=scene.index('Q1990RenderNativeLod(false);',restore)
        terrain=scene.index('RenderFo3CollisionOverlay(mvp);',opaque)
        snapshot=scene.index('Q2060ResolveEyeMsaaQ2060();',terrain)
        water=scene.index('RenderFo3WaterSurfaceQ2070(',snapshot)
        self.assertLess(prepass,begin)
        self.assertLess(begin,restore)
        self.assertLess(restore,opaque)
        self.assertLess(opaque,terrain)
        self.assertLess(terrain,snapshot)
        self.assertLess(snapshot,water)
        self.assertEqual(scene.count('Q2090RenderWaterReflection(mvp,'),1)
        before=scene[:prepass]
        self.assertIn('IsFo3WaterPotentiallyVisibleQ209B(',before)
        self.assertIn('q2090HavePlane && q209bWaterInFrustum &&',before)
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
        resolve=CORE.split('void Q2060ResolveEyeMsaaQ2060() {',1)[1].split('bool Q1280AllocatePostTarget',1)[0]
        self.assertIn('GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT',resolve)
        scene=CORE.split('void RenderScene(const float* mvp)',1)[1].split('void Q6HGenFramebuffers',1)[0]
        self.assertIn('q209bWaterInFrustum && q2060MsaaActive',scene)
        self.assertIn('q2080SceneSnapshotReady ? q1370PostDepth : 0u',scene)
        self.assertIn('fqperf::ContinueHdrPass()',scene)
        post=CORE.split('void Q1350CompositeEyePostQ1350(',1)[1].split('void Q1280ShutdownPost',1)[0]
        self.assertLess(post.index('Q2060ResolveEyeMsaaQ2060()'),post.index('fqperf::EndHdrPass()'))
        self.assertLess(post.index('fqperf::EndHdrPass()'),post.index('Q1350UpdateExposureQ1350()'))
        self.assertIn('GL_DEPTH_BUFFER_BIT,GL_NEAREST',LOOP) # compositor depth survives
if __name__=='__main__':unittest.main()
