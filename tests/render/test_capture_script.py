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
if os.environ.get('FAKE_ADB_UNAUTHORIZED'):
 sys.exit('adb: device unauthorized')
if a==['get-state']:print('device')
elif a[:3]==['shell','dumpsys','package']:
 print('    versionCode=350 minSdk=29 targetSdk=35')
 print('    versionName='+os.environ.get('FAKE_ADB_VERSION','0.195.0-cpu-profiling-O2'))
elif a[:3]==['shell','setprop','debug.falloutquest.perf_capture']:
 if a[3]!='0':s={'token':a[3]}
 else:s['stopped']=True
 p.write_text(json.dumps(s))
elif a[:2]==['logcat','-d']:
 if 'token' in s:
  print('FQPerf: CAPTURE_START id='+s['token']+' gpuSupported=1')
  if s.get('stopped'):print('FQPerf: CAPTURE_END id='+s['token']+' frames=100 saved=1')
 else:print('FalloutQuest: ready')
elif a[:3]==['exec-out','run-as','com.falloutquest.app']:
 if os.environ.get('FAKE_ADB_PULL_FAIL'):sys.exit('run-as: could not read files/performance.csv')
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
             '-Scene','capitol','-OutputDirectory',str(self.directory/'captures')],env=self.env,
             cwd=ROOT,capture_output=True,text=True,timeout=35)
    def test_finds_adb_next_to_script_and_exports_new_matching_session(self):
        r=self.run_script();self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        token=json.loads((self.directory/'state.json').read_text())['token']
        csv=self.directory/'captures'/f'capitol-v350-{token}.csv'
        log=self.directory/'captures'/f'capitol-v350-{token}-logcat.txt'
        self.assertIn('session='+token,csv.read_text())
        self.assertIn('versionCode=350',log.read_text())
        self.assertIn('CAPTURE_END id='+token,log.read_text())
        self.assertIn(str(log),r.stdout)
    def test_rejects_debug_before_starting_recording(self):
        self.env['FAKE_ADB_VERSION']='0.195.0-cpu-profiling'
        r=self.run_script();self.assertNotEqual(r.returncode,0)
        self.assertIn('Install the FalloutQuest O2 profile APK',r.stdout+r.stderr)
        self.assertFalse((self.directory/'state.json').exists())
        log, = (self.directory/'captures').glob('capitol-v350-*-logcat.txt')
        self.assertIn('Capture failed:',log.read_text())
        self.assertIn('FalloutQuest: ready',log.read_text())

    def test_preserves_log_when_csv_pull_fails(self):
        self.env['FAKE_ADB_PULL_FAIL']='1'
        r=self.run_script();self.assertNotEqual(r.returncode,0)
        state=json.loads((self.directory/'state.json').read_text())
        self.assertTrue(state['stopped'])
        log, = (self.directory/'captures').glob('capitol-v350-*-logcat.txt')
        self.assertIn('adb exec-out failed:',log.read_text())
        self.assertIn('CAPTURE_END id='+state['token'],log.read_text())
        self.assertFalse(list((self.directory/'captures').glob('*.csv')))

    def test_writes_connection_error_when_adb_cannot_read_logcat(self):
        self.env['FAKE_ADB_UNAUTHORIZED']='1'
        r=self.run_script();self.assertNotEqual(r.returncode,0)
        log, = (self.directory/'captures').glob('capitol-*-logcat.txt')
        self.assertIn('Capture failed: adb get-state failed: adb: device unauthorized',log.read_text())
        self.assertIn('Could not read headset logcat',log.read_text())
        self.assertFalse(list((self.directory/'captures').glob('*.csv')))

if __name__=='__main__':unittest.main()
