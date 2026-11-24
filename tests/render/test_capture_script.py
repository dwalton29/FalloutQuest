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
 print('    versionCode='+os.environ.get('FAKE_ADB_CODE','350')+' minSdk=29 targetSdk=35')
 print('    versionName='+os.environ.get('FAKE_ADB_VERSION','0.195.0-cpu-profiling-O2'))
elif a[:3]==['shell','setprop','debug.falloutquest.perf_capture']:
 if a[3]!='0':
  token,_,limit=a[3].partition(':')
  limit,_,binds=limit.partition(':binds') if not limit.startswith('binds') else ('','',limit[5:])
  s={'token':token,'limit':limit,'binds':binds}
 else:s['stopped']=True
 p.write_text(json.dumps(s))
elif a[:2]==['shell','dumpsys']:
 print('Device diagnostic '+str(a[2:]))
elif a[:2]==['logcat','-d']:
 if 'token' in s and not os.environ.get('FAKE_ADB_LOST_MARKERS'):
  print('FQPerf: CAPTURE_START id='+s['token']+' gpuSupported=1')
  if s.get('stopped'):print('FQPerf: CAPTURE_END id='+s['token']+' frames=2 saved=1')
 else:print('FalloutQuest: ready')
elif a[:3]==['exec-out','run-as','com.falloutquest.app']:
 if a[-1]=='files/performance-status.txt':
  if not os.environ.get('FAKE_ADB_STATUS') or 'token' not in s:sys.exit('No status file')
  print('format=351')
  print('session='+s['token'])
  print('state='+('complete' if s.get('stopped') else 'recording'))
  print('frames=2')
  print('saved=1')
  sys.exit(0)
 if os.environ.get('FAKE_ADB_PULL_FAIL'):sys.exit('run-as: could not read files/performance.csv')
 print('# format='+('351' if os.environ.get('FAKE_ADB_STATUS') else '233')+' session='+('stale' if os.environ.get('FAKE_ADB_STALE') else s['token'])+' native=profile-O2'+(' solverIterationLimit='+os.environ.get('FAKE_ADB_LIMIT',s['limit']) if s['limit'] else '')+(' renderer=vulkan vkBufferBindReuse='+os.environ.get('FAKE_ADB_BINDS',s['binds']) if s['binds'] else ''))
 solver=os.environ.get('FAKE_ADB_SOLVER')
 print('serial,loading,blocked,submitted,workUs,threadCpuUs,rigidStepUs,cell,world,residents,draws,slowObjectRef'+(',rigidUpdateBoundsUs,rigidGroupSetupUs,rigidGroupIterationsUs,rigidGroupFinishUs,rigidConstraintRemainderUs,rigidGroupIterationsUsCalls,solverVelocityIterations' if solver else ''))
 print('1,0,0,1,40000,10000,29000,42,60,64,2000,4660'+(',200,1000,3500,500,1000,3,41' if solver else ''))
 if not os.environ.get('FAKE_ADB_TRUNCATED'):print('2,1,1,1,9000000,20000,100,42,60,64,2000,0'+(',0,0,0,0,0,2,30' if solver else ''))
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
    def run_script(self,*args):
        return subprocess.run(['pwsh','-NoProfile','-File',str(self.script),'-Seconds','10',
             '-Scene','capitol','-OutputDirectory',str(self.directory/'captures'),*args],env=self.env,
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

    def test_status_survives_missing_logcat_markers_and_writes_diagnosis(self):
        self.env.update(FAKE_ADB_STATUS='1',FAKE_ADB_LOST_MARKERS='1',FAKE_ADB_SOLVER='1',FAKE_ADB_CODE='363')
        r=self.run_script('-SolverIterations','20');self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        report, = (self.directory/'captures').glob('*-summary.txt')
        text=report.read_text()
        self.assertIn('solverIterationLimit=20',text)
        self.assertIn('v363-i20-',report.name)
        self.assertIn('Active gameplay: 1',text)
        self.assertIn('rigidStepUs | 1 | 29.000',text)
        self.assertIn('rigidGroupIterationsUs | 1 | 3.500',text)
        self.assertIn('41 velocity iterations across 3 solver group calls',text)
        self.assertNotIn('9000.000',text)
        device, = (self.directory/'captures').glob('*-device.txt')
        self.assertIn('before capture',device.read_text())
        self.assertIn('after capture',device.read_text())
        self.assertIn('thermalservice',device.read_text())

    def test_buffer_bind_control(self):
        self.env.update(FAKE_ADB_STATUS='1',FAKE_ADB_CODE='364')
        r=self.run_script('-SolverIterations','16','-BufferBindReuse','Off')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        report, = (self.directory/'captures').glob('*-summary.txt')
        self.assertIn('v364-i16-binds0-',report.name)
        self.assertIn('vkBufferBindReuse=0',report.read_text())

    def test_rejects_mismatched_buffer_bind_mode(self):
        self.env.update(FAKE_ADB_STATUS='1',FAKE_ADB_CODE='364',FAKE_ADB_BINDS='0')
        r=self.run_script('-BufferBindReuse','On')
        self.assertNotEqual(r.returncode,0)
        self.assertIn('did not confirm buffer bind reuse On',r.stdout+r.stderr)

    def test_rejects_mismatched_solver_setting(self):
        self.env.update(FAKE_ADB_STATUS='1',FAKE_ADB_CODE='363',FAKE_ADB_LIMIT='20')
        r=self.run_script('-SolverIterations','16')
        self.assertNotEqual(r.returncode,0)
        self.assertIn('did not confirm the requested solver iteration limit of 16',r.stdout+r.stderr)
        self.assertFalse(list((self.directory/'captures').glob('*.csv')))

    def test_rejects_stale_csv_but_preserves_diagnostics(self):
        self.env.update(FAKE_ADB_STATUS='1',FAKE_ADB_STALE='1')
        r=self.run_script();self.assertNotEqual(r.returncode,0)
        log, = (self.directory/'captures').glob('*-logcat.txt')
        self.assertIn('empty, stale, or from the wrong build',log.read_text())
        self.assertFalse(list((self.directory/'captures').glob('*.csv')))

    def test_rejects_truncated_csv(self):
        self.env.update(FAKE_ADB_STATUS='1',FAKE_ADB_TRUNCATED='1')
        r=self.run_script();self.assertNotEqual(r.returncode,0)
        log, = (self.directory/'captures').glob('*-logcat.txt')
        self.assertIn('CSV was truncated',log.read_text())

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
