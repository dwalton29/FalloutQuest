"""Guard scene startup against removal of prototype draw interception."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class RenderDispatchTests(unittest.TestCase):
    def test_scene_dispatch_without_debug_geometry_or_controllers(self):
        loop = (ROOT / "app/src/main/cpp/core/fo3-runtime-loop.inc").read_text()
        eye = loop[loop.index("    void RenderEye("):loop.index("\nprivate:", loop.index("    void RenderEye("))]
        before_controllers = eye[:eye.index("for (uint32_t hand")]
        self.assertIn("RenderScene(viewProjection.m);", before_controllers)
        self.assertNotIn("if (!IsFo3LoadingVisible()) RenderScene", before_controllers)
        self.assertIn("RenderFo3LoadingVr(", eye)
        self.assertNotIn("glDrawArrays(GL_TRIANGLES, 6, 3)", eye)
        runtime = (ROOT / "app/src/main/cpp/core/fo3-runtime.cpp").read_text()
        self.assertNotIn("#define glDrawArrays", runtime)
        self.assertNotIn("Q6HDrawArrays", runtime)
        scene = runtime[runtime.index("void RenderScene(const float* mvp)"):]
        self.assertLess(scene.index("Q1030BootMegatonOnRender();"),
                        scene.index("if (IsFo3LoadingVisible()) return;"))


if __name__ == "__main__":
    unittest.main()
