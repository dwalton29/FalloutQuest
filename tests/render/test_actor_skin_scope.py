from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]/'app/src/main/cpp'
class Scope(unittest.TestCase):
    def test_draws_do_not_prepare_poses(self):
        core=(ROOT/'core/fo3-runtime.cpp').read_text()
        player=core.split('void Q210RenderPlayerBody(',1)[1].split('void QActorPrepareStereoFrame()',1)[0]
        self.assertNotIn('Q211UpdatePlayerRig()',player)
        self.assertNotIn('Q210EnsurePlayerBody()',player)
        npc=(ROOT/'npc/fo3-npc-runtime.inc').read_text().split('void Q230RenderNpcActors(',1)[1]
        self.assertNotIn('Q230UpdateActor(',npc)
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        self.assertLess(loop.index('QActorPrepareStereoFrame();'),loop.index('rendered = RenderEye(eye'))
    def test_gpu_updates_only_palettes(self):
        helper=(ROOT/'rendering/actor-gpu-skin.inc').read_text()
        branch=helper.split('void QActorUploadSkin(',1)[1].split('} else {',1)[0]
        self.assertIn('glTexSubImage2D(',branch)
        self.assertNotIn('glBufferSubData',branch)
        self.assertNotIn('skin.bind',branch)
        player=(ROOT/'core/fo3-runtime.cpp').read_text().split('void Q211UpdatePlayerRig()',1)[1].split('void Q210DeletePlayerBody',1)[0]
        self.assertNotIn('for (size_t v',player)
        self.assertNotIn('glBufferSubData',player)
        npc=(ROOT/'npc/fo3-npc-runtime.inc').read_text().split('void Q230UpdateActor(',1)[1].split('void Q230RenderNpcActors',1)[0]
        self.assertNotIn('positionsGame',npc)
        self.assertNotIn('glBufferSubData',npc)
    def test_initial_static_sharing_is_scene_independent(self):
        core=(ROOT/'core/fo3-runtime.cpp').read_text()
        sharing=core.split('bool Q2017CanShareGeometry(',1)[1].split('bool Q2017TryReuseSharedGeometry',1)[0]
        self.assertNotIn('gExteriorStreamingActiveQ1890',sharing)
        for guard in ('"STAT"','"SCOL"','"TREE"','!gpu.alphaBlend','!gpu.decalQ1170','!gpu.externalEmittanceFlagQ1380','!gpu.q1990NativeLod'):
            self.assertIn(guard,sharing)
        eligibility=core.split('bool Q2017EligibleForInstancing(',1)[1].split('struct Q2017InstanceBatch',1)[0]
        self.assertIn('gExteriorStreamingActiveQ1890',eligibility)
        self.assertIn('!object.q220LooseObject',eligibility)
    def test_runtime_limits_and_cleanup(self):
        helper=(ROOT/'rendering/actor-gpu-skin.inc').read_text()
        for limit in ('GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS','GL_MAX_TEXTURE_SIZE','GL_MAX_VERTEX_ATTRIBS'):
            self.assertIn(limit,helper)
        self.assertIn('glDeleteBuffers(1,&object.skinAttributes)',helper)
        self.assertIn('glDeleteTextures(1,&object.skinPalette)',helper)
        self.assertIn('palette.size()',helper)
        self.assertIn('debug.falloutquest.cpu_skin',helper)
if __name__=='__main__':unittest.main()
