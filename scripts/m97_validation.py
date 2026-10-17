#!/usr/bin/env python3
"""Focused M97 Linux suite. Does not rebuild or run the full production suite."""
from pathlib import Path
import argparse,os,subprocess,time,json,datetime,shutil
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--resume',action='store_true',help='Resume this same frozen source after a tooling failure; use a fresh output after engine/project changes.');args=p.parse_args();out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=args.resume)
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',JUDAS_PROFILE='1',XDG_DATA_HOME=str(out/'user-data'));runs=json.loads((out/'RESULTS.json').read_text())['runs'] if args.resume else []
def run(name,cmd,timeout=240):
 if any(r['name']==name and r['exit']==0 for r in runs):print(name,'already passed',flush=True);return
 if (out/(name+'.log')).exists():shutil.copy2(out/(name+'.log'),out/(name+'.previous-failure.log'))
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f:r=subprocess.run(list(map(str,cmd)),cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
 runs.append(dict(name=name,argv=list(map(str,cmd)),exit=r.returncode,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(dict(baseline=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),runs=runs),indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name+' failed; evidence retained')
for exe,extra in [('judas_living_world_tests',[out/'core']),('judas_living_world_streaming_tests',[out/'stream']),('judas_streaming_tests',[]),('judas_character_motor_tests',[]),('judas_volume_terrain_tests',[out/'m84']),('judas_navigation_tests',[]),('judas_origin_stream_tests',[]),('judas_post_consumer_metadata_tests',[]),('judas_prefab_tests',[]),('judas_classification_tests',[]),('judas_lifecycle_tests',[]),('judas_physics_continuation_tests',[]),('judas_save_exclusion_tests',[out/'save-exclusion'])]:run(exe,[R/'build'/exe,*extra])
project=R/'projects/living_world_lab/living_world_lab.judasproj'
run('script-check',[R/'build/judas_script_check',project])
for mode in ['write','read','rotated','performance']:run('living-'+mode,[*(['/usr/bin/time','-v']if mode=='performance' else []),R/'build/judas_living_world_application_tests',project,mode,out/'application'])
for mode in ['write','read']:run('existing-save-stream-'+mode,[R/'build/judas_save_streaming_tests',R/'projects/streamed_range/streamed_range.judasproj',mode,out/'existing-save-stream'])
for mode in ['write','read']:run('animation-cold-'+mode,[R/'build/judas_animation_cold_save_tests',mode,out/'animation-cold'])
run('cookbook-surface',[R/'build/judasjs_examples_tests',out/'cookbook','surface'])
run('api',[ 'node',R/'scripts/check_judasjs_api.mjs','--typescript',R/'.cache/m90/types/node_modules/typescript','--runtime',out/'cookbook/surface.json'])
