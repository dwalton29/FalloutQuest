"""Validate production shader branches and hidden scene publication ordering."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
CPP=ROOT/'app/src/main/cpp'
class Interior(unittest.TestCase):
    def test_commit_and_mode_lifecycle(self):
        core=(CPP/'core/fo3-runtime.cpp').read_text()
        prep=core.split('bool BeginFo3SceneLoad(',1)[1].split('bool ProcessQ74TransitionRequest()',1)[0]
        self.assertIn('fo3interior::Load(',prep)
        self.assertNotIn('PublishFo3InteriorLighting(',prep)
        commit=core.split('CompleteFo3CellTransitionQ74(request.cellFormId);',1)[1].split('gSceneLoad.active = false;',1)[0]
        self.assertIn('if (request.worldspaceFormId != 0u)',commit)
        self.assertIn('gFo3InteriorLighting = {};',commit)
        self.assertIn('PublishFo3InteriorLighting(std::move(prepared.interiorLighting))',commit)
        self.assertIn('LoadFo3CellEnvironment(',commit)
        self.assertLess(commit.index('PublishFo3InteriorLighting'),commit.index('PrepareFo3InteriorSceneLights();'))
        self.assertLess(commit.index('PublishFo3InteriorLighting'),commit.index('loading=' ) if 'loading=' in commit else len(commit))
        world=(CPP/'world/fo3-cell-world.cpp').read_text().split('void CompleteFo3CellTransitionQ74(',1)[1]
        self.assertNotIn('ResetFo3Environment();',world)
        loop=(CPP/'core/fo3-runtime-loop.inc').read_text()
        self.assertIn('!gFo3Environment.interior) UpdateFo3TimeOfDay',loop)
        self.assertIn('!gFo3Environment.interior) RenderFo3PcSky',loop)
        shader=core.split('GLuint CreateQ6HProgram()',1)[1].split('void Q1070ApplyStaticAnisotropy',1)[0]
        self.assertIn('uniform int uInteriorMode;',shader)
        self.assertIn('dot(toLight,toLight)/(radius*radius)',shader)
        self.assertIn('q1540Sp17Lambert;',shader)
        self.assertIn('uSpecularEnabled',shader)
        draw=core.split('void DrawSceneObject(',1)[1].split('GLenum',1)[0]
        self.assertNotIn('fo3interior::Select(',draw)
        self.assertNotIn('fo3interior::Load(',draw)
        self.assertIn('object.interiorLights.count',draw)
        self.assertIn('skin->palette[bone]',core)
        self.assertIn('gQ210PlayerRoot',core.split('void PrepareFo3InteriorObjectLights',1)[1])
    def test_post_programs_compile(self):
        source=(CPP/'core/fo3-runtime.cpp').read_text()
        vertex='#version 300 es\nlayout(location=0) in vec2 aPosition; out vec2 vUv; void main(){vUv=aPosition;gl_Position=vec4(aPosition,0,1);}'
        with tempfile.TemporaryDirectory() as directory:
            for name in ['adaptFragment','brightVerticalFragment','horizontalFragment','fragmentSource']:
                region=source.split('bool Q1280EnsurePostProgramQ1280()',1)[-1] if name=='fragmentSource' else source
                # final post fragment rather than world shader
                if name=='fragmentSource':
                    matches=re.findall(r'fragmentSource\s*=\s*R"\((.*?)\)";',region,re.S)
                    fragment=next(m for m in matches if 'uPcBloomQ1670' in m)
                else:fragment=re.search(rf'{name}\s*=\s*R"\((.*?)\)";',region,re.S).group(1)
                vp=Path(directory)/'post.vert';fp=Path(directory)/'post.frag'
                vp.write_text(vertex);fp.write_text(fragment.lstrip())
                subprocess.run(['glslangValidator','-l',str(vp),str(fp)],check=True,capture_output=True)
if __name__=='__main__':unittest.main()
