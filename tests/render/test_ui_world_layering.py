"""UI stays legible through world geometry without overwriting scene depth."""
from pathlib import Path
import unittest

CPP = Path(__file__).resolve().parents[2] / 'app/src/main/cpp'


class UiWorldLayering(unittest.TestCase):
    def test_final_colour_pass(self):
        source = (CPP / 'core/fo3-runtime-loop.inc').read_text()
        eye = source.split('bool RenderEye(', 1)[1]
        composite = eye.index('Q1350CompositeEyePostQ1350(')
        for draw in ('Q230RenderHealthHud(',
                     'RenderFo3Terminal(', 'fo3menuui::Conversation(',
                     'fo3menuui::Barter(', 'fo3menuui::Message(',
                     'fo3lootui::Render(', 'fo3notifyui::Render(',
                     'RenderFo3InteractionHud('):
            self.assertGreater(eye.index(draw), composite, draw)
            self.assertLess(eye.index(draw), eye.index('glFlush()'), draw)
        # The physical Pip-Boy belongs to the world/body pass. Hands and world
        # geometry must occlude it; overlays alone bypass scene depth.
        self.assertNotIn('Q210RenderPipboyOverlay(',eye)
        runtime = (CPP / 'core/fo3-runtime.cpp').read_text()
        world = runtime.split('void Q210RenderPlayerBody(', 1)[1].split('void ', 1)[0]
        self.assertNotIn('Fo3PipboyFocus()||!gPipAnchorReady',world)
        self.assertIn('object.zBufferTestQ1200',world)
        self.assertNotIn('Q230RenderHealthHud(', runtime.split('void RenderScene(', 1)[1])

    def test_overlay_depth_policy(self):
        for path in ('ui/menu/fo3-menu-renderer.h',
                     'ui/interaction/fo3-loot-panel.h',
                     'ui/interaction/fo3-health-bar-hud.h',
                     'ui/terminal/fo3-terminal-renderer.h',
                     'ui/interaction/fo3-item-notification-renderer.h'):
            source = (CPP / path).read_text()
            self.assertIn('glDisable(GL_DEPTH_TEST)', source, path)
            self.assertIn('glDepthMask(GL_FALSE)', source, path)


if __name__ == '__main__':
    unittest.main()
