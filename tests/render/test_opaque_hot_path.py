"""Guard the driver-query-free object path and sequential stereo contract."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]

class OpaqueHotPathTests(unittest.TestCase):
    def test_object_path_has_no_driver_reads(self):
        source = (ROOT / 'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
        draw = source.split('void DrawSceneObject(', 1)[1].split('bool Q2017EligibleForInstancing', 1)[0]
        self.assertIsNone(re.search(r'\bgl(?:Get\w*|IsEnabled)\s*\(', draw))
        self.assertNotIn('q1900Path = object.modelPath', draw)
        self.assertNotIn('Q1900BuildNativeLodClipCells(', draw)
        self.assertNotIn('gLodFadePlayerLocationQ2021', draw)
        self.assertIn('else if (!object.q1990NativeLod)', draw)

    def test_no_multiview_or_blocking_gpu_sync(self):
        core = ROOT / 'app/src/main/cpp/core'
        sources = '\n'.join(p.read_text() for p in core.glob('fo3-runtime*'))
        for symbol in ('QMVRenderOpaque', 'GL_OVR_multiview2', 'quest-multiview.inc', 'glFinish('):
            self.assertNotIn(symbol, sources)
        self.assertIn('RenderEye(eye, views[eye], projectionViews[eye])', sources)
        self.assertIn('XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME', sources)

if __name__ == '__main__':
    unittest.main()
