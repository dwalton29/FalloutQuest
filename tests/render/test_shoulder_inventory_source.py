"""Integration guards for the actual render-thread release and stereo paths."""
import pathlib
import hashlib
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class ShoulderIntegration(unittest.TestCase):
    def test_normal_throw_preserved(self):
        path='app/src/main/cpp/core/fo3-runtime.cpp'
        after=(ROOT/path).read_text()
        def throw(s):
            start=s.index('            const uint32_t releasedRef =',s.index('void Q220UpdateLooseGrab('))
            end=s.index('        } else {',start)
            return s[start:end]
        # v154 ordinary throw/drop block; hash avoids requiring git history in CI.
        self.assertEqual(hashlib.sha256(throw(after).encode()).hexdigest(),"d801a612efb27e313c11940bbd654c898d844c4ec16b9f686159a9de147cdb11")
        f=after[after.index('void Q220UpdateLooseGrab('):after.index('void Q221UpdateLooseObjectsFromSolvedPalms(')]
        self.assertLess(f.index('CollectFo3WorldReference(ref)'),f.index('const uint32_t releasedRef'))
        self.assertIn('state.previousGrip=grip;return;',f)
        self.assertLess(f.index('const bool resident='),f.index('if (!handValid)'))
    def test_prompt_renderer_has_no_driver_snapshot(self):
        prompt=(ROOT/'app/src/main/cpp/ui/interaction/fo3-interaction-hud-renderer.h').read_text()
        render=prompt[prompt.index('inline void Render('):prompt.index('inline void Shutdown()')]
        self.assertIn('GlStateGuard guard;',render)
        self.assertIn('using GlStateGuard = CachedStateGuard;',prompt)
        self.assertNotIn('glGetIntegerv(',prompt)
        self.assertNotIn('glGetBooleanv(',prompt)
        self.assertNotIn('glIsEnabled(',prompt)

    def test_stereo_render_is_read_only(self):
        renderer=(ROOT/'app/src/main/cpp/ui/interaction/fo3-item-notification-renderer.h').read_text()
        render=renderer[renderer.index('inline void Render('):]
        for forbidden in ['Advance(', 'EnsureResources(', 'glGet', 'glIsEnabled', 'glBufferData', 'AppendText']:
            self.assertNotIn(forbidden,render)
        loop=(ROOT/'app/src/main/cpp/core/fo3-runtime-loop.inc').read_text()
        self.assertEqual(loop.count('fo3notifyui::Prepare('),1)
        self.assertLess(loop.index('fo3notifyui::Prepare('),loop.index('rendered = RenderEye('))
        guard=(ROOT/'app/src/main/cpp/ui/interaction/fo3-hud-cached-state.h').read_text()
        self.assertNotIn('glGet',guard)
        self.assertNotIn('glIsEnabled',guard)
    def test_authored_master_anchor(self):
        runtime=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
        self.assertIn('q213RightPose.solved?q213RightPose.shoulder:RuntimePoint(gVrArms[1].shoulder)',runtime)
        self.assertIn('gShoulderZone.rear={gQ210PlayerRoot[8],0,gQ210PlayerRoot[10]}',runtime)
if __name__=='__main__':unittest.main()
