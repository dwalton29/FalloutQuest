"""Check final stereo colour-pass placement; this does not substitute headset QA."""
from pathlib import Path
import unittest
CPP=Path(__file__).resolve().parents[2]/'app/src/main/cpp'
class NotificationLayering(unittest.TestCase):
    def test_final_colour_pass(self):
        eye=(CPP/'core/fo3-runtime-loop.inc').read_text().split('bool RenderEye(',1)[1]
        notice=eye.index('fo3notifyui::Render(')
        for preceding in ('RenderScene(', 'fo3lootui::Render(', 'RenderFo3InteractionHud(', 'Q1350CompositeEyePostQ1350('):
            self.assertLess(eye.index(preceding),notice)
        self.assertLess(notice,eye.index('glFlush()'))
        self.assertEqual(eye.count('fo3notifyui::Render('),1)
        policy=eye[eye.rfind('if(',0,notice):notice]
        for modal in ('IsFo3LoadingVisible','Fo3DialogueFocus','Fo3ScriptMessageFocus','Fo3PipboyFocus','Fo3LockpickFocus'):
            self.assertIn(modal,policy)
    def test_warm_eyes_are_read_only(self):
        render=(CPP/'ui/interaction/fo3-item-notification-renderer.h').read_text().split('inline void Render(',1)[1]
        for write in ('Advance(', 'Prepare(', 'TakeSound(', 'LoadFalloutTexture', 'glBufferData', 'glReadPixels'):
            self.assertNotIn(write,render)
        self.assertIn('glDisable(GL_DEPTH_TEST)',render)
        self.assertIn('glDepthMask(GL_FALSE)',render)
        self.assertIn('CachedStateGuard guard',render)
if __name__=='__main__':unittest.main()
