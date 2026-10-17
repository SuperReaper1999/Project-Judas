#!/usr/bin/env python3
"""M98 focused checks, separate processes and isolated saves. No full suite."""
from pathlib import Path
import argparse,os,subprocess,json,time,shutil
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--resume',action='store_true');a=p.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=a.resume);runs=json.loads((out/'RESULTS.json').read_text())if a.resume else []
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(out/'user-data'))
def run(name,argv,expected=0):
 if any(r['name']==name and r['exit']==expected for r in runs):print(name,'already passed',flush=True);return
 log=out/(name+'.log')
 if log.exists():log.rename(out/(name+'.previous-'+str(time.time_ns())+'.log'))
 start=time.monotonic()
 with log.open('w')as f:r=subprocess.run(list(map(str,argv)),cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
 runs.append(dict(name=name,argv=list(map(str,argv)),exit=r.returncode,expected=expected,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(runs,indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode!=expected:raise RuntimeError(name+' failed; retained evidence')
run('core',[R/'build/judas_save_evolution_tests',out/'core'])
run('storage',[R/'build/judas_save_storage_tests',out/'m61-storage-m98'])
lab=R/'projects/save_evolution_lab';application=R/'build/judas_save_evolution_application_tests'
for ver,mode in [('v1','write'),('v1','same-content'),('v3','exact'),('failure','failure'),('incompatible','incompatible'),('v3','mods'),('v3','native-failure'),('v2','read-v2'),('v3','read-v3'),('v3','direct-v3'),('v3','current-v3')]:run(mode,[application,lab/ver/'save_evolution.judasproj',mode,out/'application'])
root=next((out/'user-data/judas/games').iterdir())/'Saves/Slots'
run('inspect-original',[R/'build/judas_save','inspect',root,'old-progress'])
run('dry-convert',[R/'build/judas_save','check',lab/'v3/save_evolution.judasproj',root,'old-progress'])
run('offline-convert',[R/'build/judas_save','migrate',lab/'v3/save_evolution.judasproj',root,'old-progress','offline-upgraded'])
run('refuse-existing',[R/'build/judas_save','migrate',lab/'v3/save_evolution.judasproj',root,'old-progress','offline-upgraded'],1)
run('refuse-source',[R/'build/judas_save','migrate',lab/'v3/save_evolution.judasproj',root,'old-progress','old-progress'],1)
# Exact raw bytes of current/previous are preserved while recovery writes a new slot.
shutil.copy2(root/'old-progress.save',root/'recovery.previous');(root/'recovery.save').write_bytes(b'injected-corrupt-current')
previous=(root/'recovery.previous').read_bytes();corrupt=(root/'recovery.save').read_bytes()
run('recover-corrupt',[R/'build/judas_save','recover',root,'recovery','recovered-current'])
run('recover-previous',[R/'build/judas_save','previous',root,'recovery','recovered-previous'])
assert (root/'recovery.previous').read_bytes()==previous and (root/'recovery.save').read_bytes()==corrupt

for ver in ['v1','v2','v3','failure','incompatible']:run('script-'+ver,[R/'build/judas_script_check',lab/ver/'save_evolution.judasproj'])
