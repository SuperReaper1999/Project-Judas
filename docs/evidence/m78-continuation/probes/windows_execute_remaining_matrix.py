#!/usr/bin/python3
"""Run the explicit final native affected matrix, one case at a time.

No source changes, CMake rebuild, VM resource change, or cleanup beyond the
separately verified completed-case artifacts. Original fixtures remain intact.
Performance/package cases are intentionally separate so the root can coordinate.
"""
import json,pathlib,subprocess,time,shutil,sys
root=pathlib.Path.cwd();ctl=root/'docs/evidence/m78-continuation/probes/windows_case_control.py';reports=root/'.cache/windows-vm-transfer/reports';evidence=root/'docs/evidence/m78-continuation';manifest=json.loads((root/'.cache/windows-vm-transfer/m78-continuation-direct-combined-v2.json').read_text());label=manifest['label']
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def op(mode,case,extra=None):
 args=['/usr/bin/python3',str(ctl),mode,case]
 if extra:args.append(extra)
 result=subprocess.run(args,capture_output=True,text=True)
 if result.returncode:raise RuntimeError(result.stderr+' '+result.stdout)
 data=json.loads(result.stdout)
 if mode=='approve':return data
 pid=data['return']['pid'];started=time.time()
 while True:
  time.sleep(3)
  status=subprocess.run(['/usr/bin/python3',str(ctl),'status',str(pid)],capture_output=True,text=True)
  if status.returncode:raise RuntimeError('Guest status unavailable; preserve artifacts and inspect: '+status.stderr)
  status=json.loads(status.stdout)['return']
  if status.get('exited'):
   if status.get('exitcode')!=0:raise RuntimeError('Guest verification tool failed '+mode+' '+case+' '+json.dumps(status))
   return status
  if time.time()-started>900:raise RuntimeError('Host wait bounded; preserve owned running guest PID '+str(pid))
def collect():
 for p in reports.glob('m78-'+label+'-*'):
  if p.suffix not in ['.zip','.exe','.lib']:shutil.copy2(p,evidence/p.name)
def result(case,run=None):
 name='m78-'+label+('-run-'+case+'-'+run if run else '-'+case);receipt=read(reports/(name+'.json'))
 collect();print(json.dumps({'case':case,'run':run,'exitCode':receipt['exitCode'],'seconds':receipt['seconds'],'freeBytes':receipt['freeBytes']}),flush=True)
 if receipt['exitCode']!=0:raise RuntimeError('Actual native case failed: '+name)
def cleanup(case):
 op('archive',case);op('approve',case);op('cleanup',case);collect()
try:
 # The manually completed unchanged controller already has its archive queued.
 if not (reports/('m78-'+label+'-controller-cleanup.json')).exists():
  if not (reports/('m78-'+label+'-controller-archive.json')).exists():raise RuntimeError('Wait for previously launched controller archival')
  op('approve','controller');op('cleanup','controller');collect()
 order=['foundation','physical','ragdoll','joints','rigid_contact','geometry','contact_lifecycle','contact_cache','impact_solver','impact_timing','gravity','uniform_gravity','world_save','kinematic','kinematic_lab','motor','lifecycle','character_lab','spacecraft_control','spacecraft_flight','prefab','streaming','scalability','nav_dependency']
 cleanup('balance_lab')
 for case in order:
  print('BEGIN '+case,flush=True)
  if case=='balance_lab':
   recovered=read(reports/('m78-'+label+'-run-balance_lab-step-trace-llvmpipe.json'))
   if recovered['exitCode']!=0:raise RuntimeError('Exact same-binary corrected VM driver run did not pass')
  else:op('case',case);result(case)
  for run in manifest.get('additionalRuns',{}).get(case,[]):op('run',case,run['id']);result(case,run['id'])
  cleanup(case)
 print('EXPLICIT CORRECTNESS MATRIX COMPLETE; concave timing, unchanged FLU witness, native runtime/export/moved-package remain separate.',flush=True)
except Exception as ex:
 collect();print('STOP '+str(ex),flush=True);sys.exit(1)
