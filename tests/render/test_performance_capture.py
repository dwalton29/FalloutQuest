"""Capture schema/statistics parity and retained runtime safety boundaries."""
import csv
import sys
sys.dont_write_bytecode = True
import importlib.util
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('summary', ROOT/'tools/performance/summarize_capture.py')
summary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summary)

class CaptureTests(unittest.TestCase):
    def test_statistics(self):
        s = summary.stats([1000, 10000, 14000, 20000])
        self.assertEqual(s['mean'], 11250)
        self.assertEqual(s['median'], 12000)
        self.assertAlmostEqual(s['p95'], 19100)
        self.assertAlmostEqual(s['p99'], 19820)
        self.assertEqual(s['over72'], 50)
        self.assertEqual(s['over90'], 50)

    def test_csv_written_by_production_header(self):
        with open('/tmp/fq-perf-test.csv') as f:
            reader = csv.DictReader(line for line in f if not line.startswith('#'))
            rows = list(reader)
        self.assertEqual(len(rows), 4096)
        self.assertNotIn(None, rows[0])
        self.assertEqual(float(rows[0]['gpuEyesUs']), 250)
        self.assertEqual(float(rows[0]['gpuEye0Us']), 100)
        self.assertEqual(float(rows[0]['workUs']), 1000)
        self.assertEqual(float(rows[-1]['workUs']), 5095)

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
