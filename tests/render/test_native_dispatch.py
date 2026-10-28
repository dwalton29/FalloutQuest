"""Prevent a new legacy entry point from bypassing the native CPU frontend."""
from pathlib import Path
import re
import unittest
ROOT=Path(__file__).resolve().parents[2]/'app/src/main/cpp'
class NativeDispatch(unittest.TestCase):
    def test_all_legacy_calls_have_a_dispatch_boundary(self):
        adapters=[ROOT/'rendering/gl-state-cache.h',ROOT/'rendering/native-scene-frontend.h']
        covered=set(re.findall(r'^#define\s+(gl\w+)', '\n'.join(p.read_text() for p in adapters),re.M))
        # This is a backend implementation, compiled before legacy dispatch.
        # spacewarp_gles_tests exercises its native-mode no-driver boundary.
        backend=ROOT/'rendering/spacewarp/fo3-motion-gles.h'
        used={name for path in ROOT.rglob('*') if path.suffix in ('.h','.cpp','.inc') and path not in adapters and path!=backend
              for name in re.findall(r'\b(gl[A-Z]\w*)\s*\(',path.read_text())}
        self.assertEqual(used-covered,set())
    def test_native_gpu_backend_has_no_gles_driver_calls_or_state_cache(self):
        source=(ROOT/'rendering/vulkan/fo3-vulkan-renderer.cpp').read_text()
        self.assertNotIn('gl-state-cache.h',source)
        self.assertEqual(re.findall(r'\bgl[A-Z]\w*\s*\(',source),[])
        self.assertNotIn('fqgl::state',source)
if __name__=='__main__':unittest.main()
