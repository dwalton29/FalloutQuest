"""Capture schema/statistics parity and retained runtime safety boundaries."""
import csv
import sys
sys.dont_write_bytecode = True
import importlib.util
from pathlib import Path
import unittest
import contextlib
import io
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('summary', ROOT/'tools/performance/summarize_capture.py')
summary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summary)

class CaptureTests(unittest.TestCase):
    def test_v227_npc_duplicate_admission_and_weapon_hotpath(self):
        source=(ROOT/'app/src/main/cpp/npc/fo3-npc-runtime.inc').read_text()
        self.assertIn('q227PreparedRefs.insert(source.refFormId)',source)
        self.assertIn('if(object.physicalWeapon)continue;',source)
        self.assertNotIn('std::find(w.second.gpu.begin()',source)
        self.assertIn('visual=%p frame=%llu',source)

    def test_statistics(self):
        s = summary.stats([1000, 10000, 14000, 20000])
        self.assertEqual(s['mean'], 11250)
        self.assertEqual(s['median'], 12000)
        self.assertAlmostEqual(s['p95'], 19100)
        self.assertAlmostEqual(s['p99'], 19820)
        self.assertEqual(s['over72'], 50)
        self.assertEqual(s['over90'], 50)

    def test_wrapped_gpu_results(self):
        self.assertFalse(summary.valid_gpu_us(18446744073695728.0))
        self.assertFalse(summary.valid_gpu_us(float('nan')))
        self.assertTrue(summary.valid_gpu_us(13888.888))

    def test_cadence_excludes_loading_focus_and_capture_boundaries(self):
        rows = [dict(serial=str(i), loading=loading, cadenceUs=str(us))
                for i, loading, us in [(1, '1', 0), (2, '1', 14000),
                                      (3, '0', 22000000), (4, '0', 30000),
                                      (6, '0', 900000), (7, '0', 40000)]]
        self.assertEqual(summary.cadence_values(rows, lambda r: r['loading']=='0'),
                         [30000, 40000])

    def test_csv_written_by_production_header(self):
        with open('/tmp/fq-perf-test.csv') as f:
            reader = csv.DictReader(line for line in f if not line.startswith('#'))
            rows = list(reader)
        self.assertEqual(len(rows), 4096)
        self.assertNotIn(None, rows[0])
        fields = ('motionRejectReason', 'motionRejectObject', 'motionRejectRef', 'motionRejectMesh',
                  'motionRejectObjects', 'motionRejectUsedBytes', 'motionRejectRecordBytes')
        self.assertEqual([rows[0][key] for key in fields],
                         ['byte-limit', '4294967297', '4660', '7', '12', '8000000', '400000'])
        self.assertEqual([rows[1][key] for key in fields], ['none'] + ['0'] * 6)
        sample = [dict(r, focused='1', submitted='1') for r in rows[:2]]
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            summary.diagnostic_summary(sample)
        self.assertIn('Slowest player movement: frame', out.getvalue())
        self.assertIn('rigidFullRefreshUs: 0.000 ms, 1 calls', out.getvalue())
        self.assertIn('rigidConstraintsUs 1.500', out.getvalue())
        self.assertIn('rigidGroupIterationsUs 0.875', out.getvalue())
        self.assertIn('rigidConstraintRemainderUs 0.250', out.getvalue())
        self.assertIn('41 velocity iterations across 3 solver group calls', out.getvalue())
        self.assertEqual(float(rows[1]['rigidConstraintRemainderUs']), 0)
        self.assertIn('800 reused and 50 built', out.getvalue())
        self.assertIn('1/2 gameplay frames', out.getvalue())
        self.assertIn('object 4294967297, ref 00001234, mesh 7', out.getvalue())
        self.assertEqual(float(rows[0]['gpuEyesUs']), 250)
        self.assertEqual(float(rows[0]['gpuEye0Us']), 100)
        self.assertEqual(float(rows[0]['gpu0_terrainUs']), 150)
        self.assertEqual(float(rows[0]['gpu1_reflectionUs']), -1)
        self.assertEqual(float(rows[0]['workUs']), 1000)
        self.assertEqual(float(rows[0]['vrInteractionUpdateUs']), 48000)
        self.assertEqual(float(rows[0]['rigidStepUs']), 29000)
        self.assertEqual(int(rows[0]['hdrRttEyes']), 2)
        self.assertEqual(int(rows[0]['implicitColorResolvePixels']), 123456)
        self.assertEqual(float(rows[-1]['workUs']), 5095)
        self.assertEqual(float(rows[0]['vkWorldGpuUs']), -1)
        self.assertEqual(float(rows[0]['vk_sceneUs']), -1)
        self.assertEqual(int(rows[0]['npcAnimationUsCalls']), 0)

    def test_diagnosis_excludes_loading_and_identifies_slow_reference(self):
        def row(**kw):
            base=dict(serial='1',loading='0',blocked='0',submitted='1',workUs='40000',
                      threadCpuUs='10000',cell='42',world='60',residents='64',draws='2000',
                      rigidStepUs='29000',rigidStepUsCalls='3',rigidStepUsMaxUs='20000',
                      slowObjectRef='4660',slowObjectUs='12000')
            return dict(base,**kw)
        out=io.StringIO()
        with contextlib.redirect_stdout(out):
            summary.diagnostic_summary([row(),row(serial='2',loading='1',workUs='9000000')])
        text=out.getvalue()
        self.assertIn('25.0%',text)
        self.assertIn('rigidStepUs | 29.000',text)
        self.assertIn('00001234',text)
        self.assertNotIn('9000.000',text)

    def test_unfocused_frames_are_not_active_gameplay(self):
        self.assertFalse(summary.gameplay(dict(loading='0',blocked='0',submitted='1',focused='0')))
        self.assertTrue(summary.gameplay(dict(loading='0',blocked='0',submitted='1',focused='1')))

    def test_v226_npc_movement_and_arrival_attribution(self):
        header=(ROOT/'app/src/main/cpp/rendering/performance-capture.h').read_text()
        packages=(ROOT/'app/src/main/cpp/npc/fo3-npc-package-runtime.inc').read_text()
        core=(ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
        for field in ('npcGroundUs','npcPortalUs','npcObstructionUs',
                      'npcCrowdUs','npcArrivalPartUs'):
            self.assertIn('"'+field+'"',header)
        for scope in ('NpcGround','NpcPortal','NpcObstruction','NpcCrowd'):
            self.assertIn('FQ_PERF_SCOPE('+scope+')',packages)
        self.assertIn('FQ_PERF_SCOPE(NpcArrivalPart)',core)
        self.assertIn('Q226 NPC ARRIVAL PART SLOW',core)

    def test_world_order_and_actor_bounds_do_not_copy_or_sort_each_frame(self):
        core = (ROOT/'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
        order = core.split('void Q2017PrepareOpaqueOrder()', 1)[1].split('gOpaqueOrderFrame =', 1)[0]
        for guard in ('gOpaqueOrderStorage == gObjects.data()', 'gOpaqueOrderCount == gObjects.size()',
                      'gOpaqueOrderVisualRevision == gFo3VisualPublication.Revision()'):
            self.assertIn(guard, order)
        self.assertNotIn('gOpaqueOrderFrame == gStereoFrame', order)
        self.assertNotIn('GpuObject bounds=actor.renderBounds', core)
        stereo = core.split('bool Q223StereoBoundsVisible(', 1)[1].split('bool Q2017ReflectionUseful', 1)[0]
        self.assertIn('std::memcmp(matrices,gStereoMvp,sizeof(matrices))', stereo)
        self.assertNotIn('Q2015FrustumCullScope scope(mvp)', stereo)

if __name__ == '__main__':
    unittest.main()
