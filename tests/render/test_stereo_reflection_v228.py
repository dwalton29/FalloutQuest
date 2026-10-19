"""Guard v228 publication draw-list reuse and strictly invisible mirror LAND."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = (ROOT / "app/src/main/cpp/core/fo3-runtime.cpp").read_text()
LAND = (ROOT / "app/src/main/cpp/rendering/terrain/fo3-terrain-render.inc").read_text()


class StereoReflectionV228(unittest.TestCase):
    def test_publication_ordered_passes_and_mirror_reuse(self):
        prepare = RUNTIME.split("void Q2017PrepareOpaqueOrder()", 1)[1].split(
            "// One camera preparation per stereo frame", 1
        )[0]
        self.assertIn("gAlphaFrameOrder.push_back(&object)", prepare)
        self.assertIn("gEnvironmentFrameOrder.push_back(&object)", prepare)
        self.assertIn("gOpaqueOrderVisualRevision == gFo3VisualPublication.Revision()", prepare)
        scene = RUNTIME.split("void RenderScene(const float* mvp)", 1)[1]
        self.assertIn("for (const GpuObject* alpha : gAlphaFrameOrder)", scene)
        self.assertIn("for (const GpuObject* reflective : gEnvironmentFrameOrder)", scene)
        mirror = RUNTIME.split("bool Q2090RenderWaterReflection(", 1)[1].split(
            "struct Q1970RenderStallScopeQ19", 1
        )[0]
        self.assertIn("for (const GpuObject* alpha : gAlphaFrameOrder)", mirror)
        self.assertIn("Q2017ReflectionUseful(object)", mirror)
        self.assertIn("Q2015AabbVisible(object)", mirror)

    def test_below_water_rejection_only_during_mirror(self):
        helper = LAND.split("bool Q228EntireTerrainBatchBelowReflectionPlane(", 1)[1].split(
            "GLuint q1050TerrainShadowTexture", 1
        )[0]
        self.assertIn("q2090TerrainReflectionClipEnabled &&", helper)
        self.assertIn("std::isfinite(batch.maxY)", helper)
        self.assertIn("batch.maxY < q2090TerrainReflectionPlaneY - 0.001f", helper)
        draw = LAND.split("void RenderFo3TerrainQ76(", 1)[1]
        self.assertEqual(draw.count("if (Q228EntireTerrainBatchBelowReflectionPlane(batch))"), 2)
        self.assertIn("vPosition = aPosition;", LAND)
        self.assertIn("vPosition.y < uWaterReflectionPlaneYQ2090", LAND)
        self.assertIn("batch.boundsValid &&", helper)
        self.assertIn("culling=homogeneous-padded", LAND)
        self.assertNotIn("Q2018BuildTerrainFrustum", LAND)
        # Both publication routes must derive renderer-space bounds from the
        # very same float stream sent to GL. Streamed batches used to stay zero.
        stream = (ROOT / "app/src/main/cpp/rendering/terrain/fo3-terrain-stream.inc").read_text()
        for source in (LAND, stream):
            self.assertIn("fo3terrain::BuildBounds(cpu.vertices.data(),cpu.vertices.size())", source)
        self.assertEqual(draw.count("fo3terrain::MayIntersectClip(batch,mvp)"), 2)


if __name__ == "__main__":
    unittest.main()
