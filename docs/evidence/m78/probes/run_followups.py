#!/usr/bin/env python3
"""Fresh processes, unchanged suites; each case retains independent receipts."""
import pathlib,json,subprocess
r=pathlib.Path(__file__).resolve().parents[4];out=r/'.cache/m78/followups';out.mkdir(exist_ok=True)
cases=[]
def add(id,target,*args):cases.append({'id':id,'command':[str(r/'build'/target),*[str(x) for x in args]]})
for t in ['script','runtime_ui']:
 add(t+'-export-prerequisite','judas_'+t+'_application_tests','--output',out/t)
add('storage-path-guard','judas_save_storage_tests',out/'m61-storage-confirm')
add('reimport-local-prerequisite','judas_model_reimport_tests',out/'reimport')
add('fluid-rotated-repeat','judas_production_fluid_tests','--case','rotated_half','--output',out/'fluid-repeat')
for mode in ['supports','recovery','radial','denied','blocked-recovery','low-grip','lifecycle','regions','fault','variants','step-trace','cookbook']:
 add('balance-'+mode,'judas_balance_lab_tests',out/('balance-'+mode),mode)
for key in ['step','stand','recovery']:
 for mode in ['write','read']:add('balance-cold-'+key+'-'+mode,'judas_balance_lab_tests',out/('balance-cold-'+key),'cold-'+key+'-'+mode)
for target,project in [('save_application','save_lab'),('save_streaming','streamed_range')]:
 for mode in ['write','read']:add(target+'-cold-'+mode,'judas_'+target+'_tests',r/'.cache/m78/frozen/projects'/project/(project+'.judasproj'),mode,out/target)
add('scalability-unload-repeat','judas_scalability_tests',out/'scalability-repeat')
(r/'docs/evidence/m78/FOLLOWUP_PLAN.json').write_text(json.dumps(cases,indent=2)+'\n')
for case in cases:
 if (r/'docs/evidence/m78/cases'/(case['id']+'.json')).exists():continue
 print('RUN',case['id'],flush=True)
 p=subprocess.run(['python3',str(r/'docs/evidence/m78/probes/run_case.py'),case['id'],'--',*case['command']],cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 print('RESULT',case['id'],p.returncode,p.stdout[-600:],flush=True)
