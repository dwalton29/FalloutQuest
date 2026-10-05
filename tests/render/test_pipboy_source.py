"""Integration contracts that pure device policy and GL tests cannot observe."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[2]/'app/src/main/cpp'
class Pipboy(unittest.TestCase):
    def test_input_precedes_world_activation_and_eyes(self):
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        frame=loop.split('void RenderFrame()',1)[1].split('bool RenderEye(',1)[0]
        self.assertLess(frame.index('UpdateFo3Pipboy('),frame.index('gPipInput.WorldA('))
        self.assertLess(frame.index('fo3pipui::Render('),frame.index('rendered = RenderEye(eye'))
        self.assertEqual(frame.count('UpdateFo3Pipboy('),1)
        self.assertIn('Fo3PipboyFocus()?0:lootScrollY_',loop)
        self.assertIn('gPipActivation.phase==fo3pip::Phase::Candidate',loop)
    def test_authoritative_solved_mount(self):
        core=(ROOT/'core/fo3-runtime.cpp').read_text()
        self.assertIn('UpdateFo3PipboyMount(q213LeftPose);',core)
        mount=core.split('void UpdateFo3PipboyMount(',1)[1].split('void UpdateFo3Pipboy(',1)[0]
        self.assertIn('pose.foreTwist',mount)
        self.assertNotIn('pose.handDelta',mount)
        self.assertNotIn('gQ218LeftHandQuat',mount)
        self.assertIn('gPipBind',mount);self.assertIn('gQ210PlayerRoot',mount)
    def test_math_helpers_are_wired_to_xr(self):
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        self.assertIn('pipLeftGripTracked_=gripTracking.tracked',loop)
        self.assertIn('gVrPipTracking.Step(',loop)
        self.assertIn('fo3vr::CentreOrientation(',loop)
        self.assertIn('MatrixFromPose(q210VirtualHead)',loop)
        sync=loop.split('void SyncInput(',1)[1].split('void UpdatePlayer(',1)[0]
        self.assertLess(sync.index('pipLeftGripTracked_=pipLeftAimTracked_=false'),sync.index('xrSyncActions('))
        self.assertIn('handPoseValid_[hand]=gripHandPoseValid_[hand]=false',sync)
    def test_bsa_prefixed_pipboy_path_is_classified(self):
        core=(ROOT/'core/fo3-runtime.cpp').read_text()
        self.assertIn('const bool pipboy=Q210EndsWithInsensitive(',core)
        self.assertIn('"pipboy3000\\\\pipboyarm.nif"',core)
        self.assertNotIn('const bool pipboy=fo3appearance::SameModel(path,"PipBoy3000/PipBoyArm.NIF")',core)
    def test_no_steady_queries_or_pause(self):
        ui=(ROOT/'ui/pipboy/fo3-pipboy-renderer.h').read_text()
        render=ui.split('inline void Render(',1)[1].split('inline void Shutdown(',1)[0]
        self.assertNotRegex(render,r'\bgl(?:Get\w*|IsEnabled)\s*\(')
        self.assertIn('menu.NeedsRedraw(focus)',render)
        self.assertNotIn('glGen',render)
        self.assertIn('r.renderQueries = fqgl::counters.queries - queries',render)
        state=(ROOT/'ui/pipboy/fo3-pipboy-state.h').read_text()
        self.assertNotIn('paused',state.lower())
    def test_original_semantics_not_block_selector(self):
        mesh=(ROOT/'ui/pipboy/fo3-pipboy-mesh.h').read_text()
        self.assertIn('pipboyscreen:0',mesh);self.assertIn('screen.dds',mesh)
        self.assertNotRegex(mesh,r'shapeBlock\s*==')
        self.assertIn('Player', (ROOT/'ui/pipboy/fo3-pipboy-state.h').read_text())
if __name__=='__main__':unittest.main()
