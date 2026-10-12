#!/usr/bin/python3
"""Explicit supplementary failure witness + fresh runtime/export cases only.

The numerical FLU test is deliberately expected to remain FAILED. This driver
records its actual exit/output and never changes input, timestep or threshold.
"""
import sys,pathlib,json,importlib.util
r=pathlib.Path.cwd();p=r/'docs/evidence/m78-continuation/probes/windows_execute_focused_matrix.py'
# Import only its small helpers without starting its top-level execution.
s=p.read_text();helpers=s[:s.index('try:\n # The manually completed')];ns={};exec(compile(helpers,str(p),'exec'),ns)
op,collect=ns['op'],ns['collect'];ctl=ns['ctl'];reports=ns['reports'];label=ns['label']
import subprocess,time
manifest='m78-continuation-direct-combined-v2-fluid-witness.json'
def supplemented(mode):
 args=['/usr/bin/python3',str(ctl),mode,'flu001','--manifest',manifest]
 q=subprocess.run(args,capture_output=True,text=True)
 if q.returncode:raise RuntimeError(q.stderr+' '+q.stdout)
 data=json.loads(q.stdout)
 if mode=='approve':return data
 pid=data['return']['pid']
 while True:
  time.sleep(3);q=subprocess.run(['/usr/bin/python3',str(ctl),'status',str(pid)],capture_output=True,text=True)
  if q.returncode:raise RuntimeError(q.stderr)
  d=json.loads(q.stdout)['return']
  if d.get('exited'):
   if d.get('exitcode')!=0:raise RuntimeError('Supplementary tool failed '+json.dumps(d))
   return
try:
 print('BEGIN original FLU-001 expected-failure witness',flush=True);supplemented('case');collect();p=reports/('m78-'+label+'-flu001.json');d=json.loads(p.read_text(encoding='utf-8-sig'));print(json.dumps({'case':'flu001','actualExitCode':d['exitCode'],'seconds':d['seconds'],'status':'EXPECTED FAILURE remains a failure'}),flush=True)
 if d['exitCode']!=1:raise RuntimeError('Original FLU witness changed status; inspect actual output')
 supplemented('archive');supplemented('approve');supplemented('cleanup');collect()
 for case in ['runtime','export']:
  print('BEGIN '+case,flush=True);op('case',case);ns['result'](case)
  if case=='export':ns['cleanup'](case)
 print('NATIVE SUPPLEMENT/STARTUP/EXPORT COMPLETE. Runtime retained for moved-package proof.',flush=True)
except Exception as e:collect();print('STOP '+str(e),flush=True);sys.exit(1)
