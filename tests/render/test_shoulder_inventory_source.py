"""Integration guards for the actual render-thread release and stereo paths."""
import pathlib
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class ShoulderIntegration(unittest.TestCase):
    def test_physical_release_preserves_shoulder_priority(self):
        runtime=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
        grab=runtime.split('void Q220UpdateLooseGrab(',1)[1].split('void Q221UpdateLooseObjectsFromSolvedPalms(',1)[0]
        self.assertLess(grab.index('CollectFo3WorldReference(ref)'),grab.index('if(grip<=RELEASE)'))
        released=grab.split('if(grip<=RELEASE){',1)[1].split('}else{',1)[0]
        self.assertIn('fo3rigid::Live().Release(handIndex)',released)
        self.assertNotIn('sampledLinearVelocity',released)
        self.assertNotIn('sampledAngularVelocity',released)
        self.assertIn('state.previousGrip=grip;return;',grab)
        self.assertLess(grab.index('const bool resident='),grab.index('if (!handValid)'))
        self.assertIn('Fo3GameplayNow()',runtime.split('void Q223AdvanceDynamicBodies()',1)[1].split('bool Q220FindNearestLooseRef(',1)[0])

    def test_physical_hand_is_not_its_own_tracking_target(self):
        runtime=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
        hand=runtime.split('void Q211UpdatePlayerRig()',1)[1].split('#include "weapons/fo3-weapon-runtime.inc"',1)[0]
        self.assertIn('fo3rigid::Live().GripPalm(h,palm)',hand)
        self.assertIn('Q218RotateQuaternion(renderQuat[0]',hand)
        self.assertIn('Q218RotateQuaternion(renderQuat[1]',hand)
        weapon=(ROOT/'app/src/main/cpp/weapons/fo3-weapon-runtime.inc').read_text()
        raw=weapon.split('Matrix RawPrimaryHand()',1)[1].split('Matrix HandPose(',1)[0]
        self.assertIn('gQ210RightHand[0]',raw)
        self.assertIn('gQ218RightHandQuat',raw)
        self.assertNotIn('gVrSolved',raw)
        self.assertIn('pose=fo3anim::Multiply(raw,rawToWeapon)',weapon)
        support=weapon.split('Matrix RawSupportHand()',1)[1].split('Matrix HandPose(',1)[0]
        self.assertIn('gQ210LeftHand[0]',support)
        self.assertIn('gQ218LeftHandQuat',support)
        self.assertNotIn('gVrSolved',support)
        self.assertIn('left=support?Point(rawSupport,rawSupportPalmLocal)',weapon)
        after_step=weapon.split('Q223AdvanceDynamicBodies();',1)[1]
        self.assertIn('physics.Pose(key,actual)',after_step)
        self.assertIn('if(!physicsAdvanced)Q223AdvanceDynamicBodies();',weapon)


    def test_body_holsters_do_not_draw_during_input_or_pick_up_from_stale_ik(self):
        weapon=(ROOT/'app/src/main/cpp/weapons/fo3-weapon-runtime.inc').read_text()
        update=weapon.split('void WeaponUpdate(double now,bool backButton){',1)[1]
        self.assertNotIn('DrawModel(',update)
        self.assertIn('const V accessPoint{gQ210RightHand[0]',update)
        pickup=update.split('if(slot)Draw(slot->id,slot->formId);',1)[1]
        self.assertIn('else if(gWeaponPalmValid[1]){',pickup)

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
