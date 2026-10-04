"""Check scope isolation and default policy for the temporary LOD diagnostics."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
HEADER=(ROOT/'app/src/main/cpp/rendering/opaque-telemetry.h').read_text()
class LodDiagnostics(unittest.TestCase):
    def test_default_and_debug_only(self):
        self.assertIn('lodRadius20=false, lodMinimal=false, lodClipBypass=false',HEADER)
        debug=HEADER.split('#ifndef NDEBUG',1)[1].split('#endif',1)[0]
        for prop in ('lod_radius20','lod_minimal','lod_clip_bypass'):
            self.assertIn('debug.falloutquest.'+prop,debug)
        self.assertIn('fqopaque::Options().lodRadius20 ? 20 : 31',SRC)
        self.assertIn('constexpr size_t Q1840_DISTANT_CACHE_BLOCKS = 324u',SRC)
        selection=SRC.split('bool Q1970ChooseMissingLodQ19(',1)[1].split('bool Q2023ChooseMissingAuthoredLodQ19',1)[0]
        self.assertIn('Q2024ObjectBlockRadius()',selection)
    def test_queries_are_serial_and_phases_split(self):
        scene=SRC.split('void RenderScene(',1)[1]
        for phase,call in [('NativeLod','Q1990RenderNativeLod(false)'),('DetailedWorld','Q2017RenderOpaqueDetailedInstanced()'),('Npc','Q230RenderNpcActors(false)'),('Player','Q210RenderPlayerBody(false)')]:
            self.assertIn(f'{{ fqopaque::PhaseScope scope(fqopaque::{phase},gStereoFrame); {call}; }}',scene)
        begin=HEADER.split('inline void Begin(uint64_t frame)',1)[1].split('inline void End',1)[0]
        self.assertNotIn('timer.Begin(',begin) # No outer query enclosing inner queries.
    def test_material_and_clip_are_native_only(self):
        draw=SRC.split('void DrawSceneObject(',1)[1].split('bool Q2017EligibleForInstancing',1)[0]
        self.assertIn('if (fqopaque::collectingNative)',draw)
        self.assertIn('fqopaque::nativeObjectMaterial && fqopaque::Options().lodMinimal',draw)
        self.assertIn('fqopaque::collectingNative && fqopaque::Options().lodClipBypass',draw)
        native=SRC.split('void Q1990RenderNativeLod(bool alphaPass)',1)[1].split('void SetFo3WaterSkyMvpQ2090',1)[0]
        self.assertIn('fqopaque::measuring && !alphaPass && !gWaterReflectionPassQ2090',native)
        self.assertIn('submit(object, true, work.level4Objects)',native)
        self.assertIn('submit(object, true, work.high)',native)
        self.assertIn('submit(object, false, work.level4Terrain)',native)
        self.assertIn('fqopaque::collectingNative = false;',native)
    def test_minimal_precedes_expensive_fragment_work(self):
        shader=SRC.split('GLuint CreateQ6HProgram()',1)[1].split('void Q1070ApplyStaticAnisotropy',1)[0]
        self.assertLess(shader.index('if (uFastMaterial > 0.5)'),shader.index('vec4 normalGloss = texture('))
        self.assertIn('mix(baseColor, uFogColor, vFogFactorQ1532)',shader)
        self.assertIn('q1900I < 25',shader)
if __name__=='__main__': unittest.main()
