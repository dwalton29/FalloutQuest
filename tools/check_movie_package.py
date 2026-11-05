#!/usr/bin/env python3
"""Verify ARM64 movie dependencies and notices in each finished APK."""
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zipfile

LIBS = ['libavformat.so', 'libavcodec.so', 'libavutil.so', 'libswscale.so']


def verify(apk):
    with zipfile.ZipFile(apk) as archive, tempfile.TemporaryDirectory() as temp:
        for lib in ['libfalloutquest.so', *LIBS]:
            data = archive.read('lib/arm64-v8a/' + lib)
            assert data[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<H', data, 18)[0] == 183, f'{lib}: expected AArch64 ELF'
            output = Path(temp) / lib
            output.write_bytes(data)
        assert b'GNU LESSER GENERAL PUBLIC LICENSE' in archive.read('assets/third_party/ffmpeg/COPYING.LGPLv2.1')
        assert b'db69d06eeeab4f46da15030a80d539efb4503ca8' in archive.read('assets/third_party/ffmpeg/NOTICE.txt')
        dynamic = subprocess.check_output(['readelf', '-d', str(Path(temp) / 'libfalloutquest.so')], text=True)
        needed = set(re.findall(r'\(NEEDED\).*?\[(.*?)\]', dynamic))
        assert set(LIBS) <= needed, f'Movie libraries not linked: {set(LIBS) - needed}'
    print(f'{apk}: ARM64 Bink libraries and LGPL notices packaged')


if __name__ == '__main__':
    for path in sys.argv[1:]:
        verify(path)
