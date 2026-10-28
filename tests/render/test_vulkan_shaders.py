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
    def test_solid_variants_preserve_material_and_remove_only_inactive_guards(self):
        for name in ('world','terrain'):
            original=vkbuild.port(vkbuild.sources()[name],name)[0][1]
            solid=vkbuild.no_discard_fragment(original,name)
            self.assertNotIn('discard;',solid)
            self.assertIn('texture(uNormalGloss',solid)
            self.assertIn('uLocalLightPosRadius',solid)
            self.assertIn('lit = mix(lit, uFogColor, fogFactor)',solid)
            self.assertEqual(original.split('void main()')[0],solid.split('void main()')[0])
            with self.assertRaises(ValueError):
                vkbuild.no_discard_fragment(original+'\nvoid unexpected(){discard;}',name)
    def test_material_abi_and_stereo(self):
        for name,pair in vkbuild.sources().items():
            converted,fields,samplers,size=vkbuild.port(pair,name)
            self.assertLessEqual(size,16384)
            last=0
            for field,offset,length,typ,count in fields:
                self.assertEqual(offset%16,0)
                self.assertGreaterEqual(offset,last);last=offset+length
            self.assertEqual(max(16,last),size)
            if name in ('ui','notification','loading'):
                self.assertIn('fqClipFromLeft[gl_ViewIndex]*uMvp',converted[0])
                self.assertIn('(fqClip.z+fqClip.w)*0.5',converted[0]);continue
            if name in ('adapt','copy','bright','blur','post'):
                self.assertIn('gl_VertexIndex',converted[0])
                self.assertNotIn('gl_VertexID',converted[0])
                self.assertIn('sampler2DArray',converted[1])
                if name=='adapt':self.assertIn('vec3(uv,0.0)',converted[1])
                else:self.assertIn('float(gl_ViewIndex)',converted[1])
                continue
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
            for name in ('world','terrain'):
                if shutil.which('spirv-val'):
                    subprocess.run(['spirv-val','--target-env','vulkan1.1',str(out/f'{name}_solid.frag.spv')],check=True)
                subprocess.run([compiler,'-V','--target-env','vulkan1.1','-l',str(out/f'{name}.vert'),str(out/f'{name}_solid.frag'),'-o',str(out/f'{name}-solid-linked.spv')],check=True)
                if shutil.which('spirv-dis'):
                    dis=subprocess.check_output(['spirv-dis',str(out/f'{name}_solid.frag.spv')],text=True)
                    self.assertNotIn('OpKill',dis)
            for name in ('static','dynamic'):
                paths=[out/f'motion_{name}_{stage}.{stage}' for stage in ('vert','frag')]
                for path in paths:
                    if shutil.which('spirv-val'):
                        subprocess.run(['spirv-val','--target-env','vulkan1.1',str(path)+'.spv'],check=True)
                subprocess.run([compiler,'-V','--target-env','vulkan1.1','-l',*map(str,paths),'-o',str(out/f'motion-{name}-linked.spv')],check=True)
if __name__=='__main__':unittest.main()
