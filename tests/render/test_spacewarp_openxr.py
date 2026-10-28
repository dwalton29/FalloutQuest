"""Compile the submission adapter against the Android project's pinned OpenXR SDK."""
import hashlib
import os
import pathlib
import subprocess
import tempfile
import unittest
import urllib.request
ROOT=pathlib.Path(__file__).resolve().parents[2]
VERSION='release-1.1.61'
HASHES={
 'openxr.h':'acf5663fc45e12e78462bc572fa7c5da9841c7656abb35ccc6242fb1deb77fab',
 'openxr_platform_defines.h':'16af15bf44d88ec0e01a145e8425da47aff14586c027c748a1c9cd77e287ec6b',
}
class OpenXRSpaceWarp(unittest.TestCase):
    def test_native_adapter(self):
        with tempfile.TemporaryDirectory() as directory:
            directory=pathlib.Path(directory)
            sdk=directory/'sdk'/'openxr';sdk.mkdir(parents=True)
            for name,digest in HASHES.items():
                local=os.environ.get('FQ_TEST_OPENXR_HEADERS')
                if local:data=(pathlib.Path(local)/'openxr'/name).read_bytes()
                else:
                    url=f'https://raw.githubusercontent.com/KhronosGroup/OpenXR-SDK/{VERSION}/include/openxr/{name}'
                    with urllib.request.urlopen(url,timeout=30) as response:data=response.read()
                self.assertEqual(hashlib.sha256(data.rstrip(b'\n')).hexdigest(),digest)
                (sdk/name).write_bytes(data)
            binary=directory/'spacewarp-openxr'
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
                '-I'+str(sdk.parent),'-I'+str(ROOT/'app/src/main/cpp'),
                str(ROOT/'tests/render/spacewarp_openxr_tests.cpp'),'-o',str(binary)],check=True,capture_output=True,text=True)
            subprocess.run([str(binary)],check=True,capture_output=True,text=True)
if __name__=='__main__':unittest.main()
