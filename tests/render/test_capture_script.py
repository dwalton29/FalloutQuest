"""Exercise the real PowerShell collector with an adb beside the script."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
FAKE_ADB='''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
p=Path(os.environ['FAKE_ADB_STATE'])
s=json.loads(p.read_text()) if p.exists() else {}
a=sys.argv[1:]
if a==['get-state']:print('device')
elif a[:3]==['shell','dumpsys','package']:
 print('versionName='+os.environ.get('FAKE_ADB_VERSION','0.97.0-terrain-layer-fusion-O2'))
elif a[:3]==['shell','setprop','debug.falloutquest.perf_capture']:
 if a[3]!='0':s={'token':a[3]}
 else:s['stopped']=True
 p.write_text(json.dumps(s))
elif a[:2]==['logcat','-d']:
 print('FQPerf: CAPTURE_START id='+s['token']+' gpuSupported=1')
 if s.get('stopped'):print('FQPerf: CAPTURE_END id='+s['token']+' frames=100 saved=1')
elif a[:3]==['exec-out','run-as','com.falloutquest.app']:
 print('# format=233 session='+s['token']+' native=profile-O2')
 print('serial,loading,workUs')
 print('1,0,1000')
else:sys.exit('Unexpected adb operation: '+str(a))
'''
@unittest.skipUnless(shutil.which('pwsh'),'PowerShell not installed on this host')
class CaptureScript(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.directory=Path(self.tmp.name)
        self.script=self.directory/'capture-quest.ps1'
        shutil.copyfile(ROOT/'tools/performance/capture-quest.ps1',self.script)
        adb=self.directory/'adb.exe';adb.write_text(FAKE_ADB);adb.chmod(0o755)
        self.env=dict(os.environ,FAKE_ADB_STATE=str(self.directory/'state.json'))
    def run_script(self):
        return subprocess.run(['pwsh','-NoProfile','-File',str(self.script),'-Seconds','10',
             '-OutputDirectory',str(self.directory/'captures')],env=self.env,
             cwd=ROOT,capture_output=True,text=True,timeout=35)
    def test_finds_adb_next_to_script_and_exports_new_matching_session(self):
        r=self.run_script();self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        token=json.loads((self.directory/'state.json').read_text())['token']
        csv=self.directory/'captures'/f'megaton-v238-{token}.csv'
        log=self.directory/'captures'/f'megaton-v238-{token}-logcat.txt'
        self.assertIn('session='+token,csv.read_text())
        self.assertIn('CAPTURE_END id='+token,log.read_text())
    def test_rejects_debug_before_starting_recording(self):
        self.env['FAKE_ADB_VERSION']='0.97.0-terrain-layer-fusion'
        r=self.run_script();self.assertNotEqual(r.returncode,0)
        self.assertIn('Install the v238 O2 APK',r.stdout+r.stderr)
        self.assertFalse((self.directory/'state.json').exists())
if __name__=='__main__':unittest.main()
