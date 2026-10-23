"""Compile/link the canonical production material ports and check their ABI."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('vkbuild',ROOT/'tools/build_vulkan_shaders.py')
vkbuild=importlib.util.module_from_spec(spec);spec.loader.exec_module(vkbuild)
class VulkanShaders(unittest.TestCase):
    def test_material_abi_and_stereo(self):
        for name,pair in vkbuild.sources().items():
            converted,fields,samplers,size=vkbuild.port(pair,name)
            self.assertLessEqual(size,16384)
            last=0
            for field,offset,length,typ,count in fields:
                self.assertEqual(offset%16,0)
                self.assertGreaterEqual(offset,last);last=offset+length
            self.assertEqual(last,size)
            if name=='water':
                self.assertIn('float(gl_ViewIndex)',converted[1])
                self.assertIn('sampler2DArray uSceneDepth',converted[1])
                self.assertIn('vec2(displacedNdc.x, -displacedNdc.y)',converted[1])
                self.assertIn('fqEye[gl_ViewIndex].xyz',converted[1])
                self.assertIn('(fqClip.z+fqClip.w)*0.5',converted[0]);continue
            if name=='sky':
                self.assertIn('fqSkyViewProjection[gl_ViewIndex]',converted[0]);continue
            self.assertIn('fqViewProjection[gl_ViewIndex]',converted[0])
            self.assertIn('(q1532Clip.z+q1532Clip.w)*0.5',converted[0])
            self.assertIn('-q1532Clip.y',converted[0])
            self.assertIn('fqEye[gl_ViewIndex].xyz',converted[1])
            self.assertTrue(samplers)
            # Original per-vertex FO3 fog stays in GL clip space, before remapping.
            self.assertIn('length(q1532Clip.xyz)',converted[0])
            if name=='terrain':self.assertNotIn('uPaintCount',converted[1])
    def test_compile_link_validate(self):
        compiler=os.environ.get('FQ_SHADER_COMPILER') or shutil.which('glslangValidator')
        self.assertIsNotNone(compiler,'Set FQ_SHADER_COMPILER or install glslang-tools')
        with tempfile.TemporaryDirectory() as temp:
            out=Path(temp);vkbuild.build(out,compiler)
            for name in vkbuild.sources():
                for stage in ['vert','frag']:
                    file=out/f'{name}.{stage}.spv'
                    self.assertEqual(file.read_bytes()[:4],b'\x03\x02\x23\x07')
                    if shutil.which('spirv-val'):
                        subprocess.run(['spirv-val','--target-env','vulkan1.1',str(file)],check=True)
                subprocess.run([compiler,'-V','--target-env','vulkan1.1','-l',str(out/f'{name}.vert'),str(out/f'{name}.frag'),'-o',str(out/f'{name}-linked.spv')],check=True)
if __name__=='__main__':unittest.main()
