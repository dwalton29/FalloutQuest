"""v229 source invariants: frame work sharing, authentic LAND order and NPC worker ownership."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"app/src/main/cpp/core/fo3-runtime.cpp").read_text()
NPC=(ROOT/"app/src/main/cpp/npc/fo3-npc-runtime.inc").read_text()
LAND=(ROOT/"app/src/main/cpp/rendering/terrain/fo3-terrain-render.inc").read_text()
STREAM=(ROOT/"app/src/main/cpp/rendering/terrain/fo3-terrain-stream.inc").read_text()

class SustainedFrameV229(unittest.TestCase):
    def test_worker_owns_cpu_expansion_and_decode(self):
        worker=CORE.split('if(work.preparation.Start([cell,world,centreX',1)[1].split('})){',1)[0]
        self.assertIn('PrepareExpandedVertexStreamQ1960(part,centreX,centreY,floorZ)',worker)
        self.assertIn('LoadFalloutTextureRgba(*path,decoded.texture)',worker)
        self.assertIn('gQ2012TextureDecodeMutexQ19',worker)
        self.assertIn('maxStagedBytes=32u*1024u*1024u',worker)
        self.assertIn('cancelled.load(std::memory_order_acquire)',worker)
        self.assertNotIn('glTexImage2D(',worker)
        self.assertNotIn('glBufferData(',worker)

    def test_gl_upload_retained_on_render_thread(self):
        main=CORE.split('void Q230PumpNpcArrivals(){',1)[1].split('#include "world/fo3-resident-reconcile-runtime.inc"',1)[0]
        self.assertIn('Q230UploadActorPart(actor,part,gSceneCenterXQ1730,',main)
        self.assertIn('gQ1900PreparedTexturesQ19.emplace(key,std::move(found->second))',main)
        self.assertIn('staged.erase(found)',main)
        self.assertIn('return; // Bound GPU work to one NPC part per frame.',main)

    def test_base_paint_order_and_mutation_invalidation(self):
        land_draw=LAND.split('void RenderFo3TerrainQ76(',1)[1]
        self.assertLess(land_draw.index('q229TerrainBaseBatches'),land_draw.index('q229TerrainPaintBatches'))
        self.assertIn('glDepthFunc(GL_LEQUAL);',land_draw)
        self.assertIn('glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);',land_draw)
        self.assertIn('Q228EntireTerrainBatchBelowReflectionPlane(batch)',land_draw)
        self.assertNotIn('Q2018BuildTerrainFrustum(mvp)',land_draw)
        self.assertIn('Q229PrepareTerrainDrawLists();',land_draw)
        self.assertEqual(STREAM.count('q711TerrainBatches.resize(write);'),2)
        self.assertEqual(STREAM.count('Q229TouchTerrainBatches();'),3)
        self.assertEqual(LAND.count('Q229TouchTerrainBatches();'),2)

    def test_palette_preserves_individual_bones_and_unbound_slots(self):
        self.assertIn('const auto rootScene=fo3anim::Multiply(actor.dialogueRoot,rig.scenePlacement);',NPC)
        self.assertIn('auto delta=actor.dialogueRoot;',NPC)
        self.assertIn('fo3anim::Multiply(actor.pose.delta[bone],rig.inverseScenePlacement)',NPC)
        self.assertIn('fqskin::Pack(rig.skin.palette[slot+1],delta,delta);',NPC)

if __name__=="__main__":
    unittest.main()
