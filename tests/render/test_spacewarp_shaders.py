"""Compile the native GLES SpaceWarp static-motion shader pair."""
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class SpaceWarpShaders(unittest.TestCase):
    def test_compile_and_link(self):
        compiler=shutil.which('glslangValidator')
        if not compiler:self.skipTest('glslangValidator unavailable')
        for filename,prefix in [('fo3-motion-vectors.h','Static'),('fo3-dynamic-motion.h','Dynamic')]:
            source=(ROOT/'app/src/main/cpp/rendering/spacewarp'/filename).read_text()
            stages=re.findall(prefix+r'(?:Vertex|Fragment)=R"GLSL\((.*?)\)GLSL"',source,re.S)
            self.assertEqual(len(stages),2)
            with tempfile.TemporaryDirectory() as directory:
                paths=[]
                for suffix,text in zip(('vert','frag'),stages):
                    path=pathlib.Path(directory)/('motion.'+suffix);path.write_text(text);paths.append(str(path))
                subprocess.run([compiler,'-l',*paths],check=True,capture_output=True,text=True)
if __name__=='__main__':unittest.main()
