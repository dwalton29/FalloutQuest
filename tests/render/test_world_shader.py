"""Compile/link the actual world shader, including its diagnostic branch."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'app/src/main/cpp/core/fo3-runtime.cpp').read_text()
program = source.split('GLuint CreateQ6HProgram()', 1)[1].split('void Q1070ApplyStaticAnisotropy', 1)[0]
with tempfile.TemporaryDirectory() as directory:
    paths = []
    for name, suffix in [('vertexSource', 'vert'), ('fragmentSource', 'frag')]:
        shader = re.search(rf'{name}\s*=\s*R"\((.*?)\)";', program, re.S).group(1)
        path = Path(directory) / f'world.{suffix}'
        path.write_text(shader.lstrip())
        paths.append(str(path))
    subprocess.run(['glslangValidator', '-l', *paths], check=True)
