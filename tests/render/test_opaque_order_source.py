"""Q235 integration: stereo-stable depth ordering and fixed authored barriers."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
HEADER=(ROOT/'app/src/main/cpp/rendering/opaque-front-to-back.h').read_text()
class OpaqueOrderIntegration(unittest.TestCase):
    def test_sort_during_rebuild_only_and_exclude_mirror(self):
        fn=CORE.split('void Q2017RenderOpaqueDetailedInstanced()',1)[1].split('bool Q1030InitializeRenderProgramOnly()',1)[0]
        rebuild=fn.split('if (rebuild) {',1)[1].split('if (!rebuild &&',1)[0]
        self.assertIn('if(!gWaterReflectionPassQ2090&&fqopaque::Options().frontToBack)',rebuild)
        self.assertIn('fqopaqueorder::SortSafeRuns(cache.ordered);',rebuild)
        self.assertIn('cache.ordered.clear();',rebuild)
        self.assertIn('for(const Q235OpaquePacket& packet:cache.ordered)',fn)
        self.assertIn('gInstancedDrawActiveQ2016=true;',fn)
    def test_opaque_and_authored_boundaries(self):
        helper=CORE.split('fqopaqueorder::SortKey Q235OpaqueKey(',1)[1].split('bool Q2017ReflectionUseful(',1)[0]
        self.assertIn('object.zBufferTestQ1200&&object.zBufferWriteQ1200',helper)
        self.assertIn('!object.decalQ1170&&!object.noLightingFalloff',helper)
        self.assertIn('if(!packets[first].key.safe)',HEADER)
        self.assertIn('while(end<packets.size()&&packets[end].key.safe)',HEADER)
        self.assertIn('if (object.alphaBlend ||',CORE)
        self.assertIn('gAlphaFrameOrder',CORE)
        self.assertIn('gEnvironmentFrameOrder',CORE)
    def test_original_draw_count_and_frame_shared_buffer(self):
        fn=CORE.split('void Q2017RenderOpaqueDetailedInstanced()',1)[1].split('bool Q1030InitializeRenderProgramOnly()',1)[0]
        self.assertIn('actualInstancedDraws+=fqgl::counters.draws-before;',fn)
        self.assertIn('fallbackDraws+=fqgl::counters.draws-before;',fn)
        self.assertIn('gInstanceBufferQ2016 = cache.buffer',fn)
        self.assertIn('cache.frame != gStereoFrame',fn)
        self.assertIn('Q235 OPAQUE EARLYZ:',fn)
if __name__=='__main__':unittest.main()
