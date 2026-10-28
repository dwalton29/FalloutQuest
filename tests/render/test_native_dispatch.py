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
        source+=(ROOT/'rendering/vulkan/fo3-vulkan-spacewarp.inc').read_text()
        source+=(ROOT/'rendering/vulkan/fo3-vulkan-composition.inc').read_text()
        self.assertNotIn('gl-state-cache.h',source)
        self.assertEqual(re.findall(r'\bgl[A-Z]\w*\s*\(',source),[])
        self.assertNotIn('fqgl::state',source)
    def test_native_motion_is_submitted_before_runtime_release(self):
        native=(ROOT/'rendering/vulkan/fo3-vulkan-renderer.cpp').read_text()
        end=native.split('void End(PostTarget*',1)[1].split('void ReportFrame(',1)[0]
        self.assertLess(end.index('RenderMotion()'),end.index('vkQueueSubmit('))
        present=native.split('bool Present(const XrView*',1)[1].split('~Renderer()',1)[0]
        self.assertLess(present.index('End(&xrTargets'),present.index('ReleaseMotion()'))
        loop=(ROOT/'core/fo3-runtime-loop.inc').read_text()
        resources=loop.split('if(spacewarp_.Requested()&&spacewarp_.Backend()!=fqspacewarp::Api::None)',1)[1].split('CreateSceneRenderer',1)[0]
        self.assertIn('VK_FORMAT_R16G16B16A16_SFLOAT',resources)
        self.assertIn('VK_FORMAT_D32_SFLOAT',resources)
        self.assertIn('fqvk::InitializeSpacewarp(targets)',resources)
        frame=loop.split('rendered=fqvk::PresentGameplay(',1)[1]
        self.assertLess(frame.index('spacewarpHistory_.EyeRendered(1)'),frame.index('spacewarpHistory_.CanSynthesize'))
        shutdown=loop.split('void Shutdown()',1)[1].split('private:',1)[0]
        self.assertLess(shutdown.index('fqvk::ShutdownSpacewarp()'),shutdown.index('spacewarp_.Destroy()'))
        motion=(ROOT/'rendering/vulkan/fo3-vulkan-spacewarp.inc').read_text().split('void RenderMotion()',1)[1]
        self.assertNotIn('vkWaitForFences',motion)
        self.assertNotIn('vkDeviceWaitIdle',motion)
        self.assertNotIn('Readback',motion)
if __name__=='__main__':unittest.main()
