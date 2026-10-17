#!/usr/bin/env python3
"""Affected M98 native regressions; no full suite."""
from pathlib import Path
import subprocess,os,time,json
import argparse
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False)
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(out/'user-data'));runs=[]
def run(name,args):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f:r=subprocess.run([str(x)for x in args],cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
 runs.append(dict(name=name,argv=[str(x)for x in args],exit=r.returncode,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(runs,indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name)
for exe,extra in [('judas_physics_continuation_tests',[]),('judas_save_exclusion_tests',[out/'exclusion']),('judas_character_motor_tests',[]),('judas_streaming_tests',[]),('judas_living_world_tests',[out/'terrain']),('judas_living_world_streaming_tests',[out/'regions'])]:run(exe,[R/'build'/exe,*extra])
for mode in ['write','read']:run('save-stream-'+mode,[R/'build/judas_save_streaming_tests',R/'projects/streamed_range/streamed_range.judasproj',mode,out/'save-stream'])
for mode in ['write','read']:run('animation-cold-'+mode,[R/'build/judas_animation_cold_save_tests',mode,out/'animation-cold'])
run('cookbook-surface',[R/'build/judasjs_examples_tests',out/'cookbook','surface'])
run('api',['node',R/'scripts/check_judasjs_api.mjs','--typescript',R/'.cache/m90/types/node_modules/typescript','--runtime',out/'cookbook/surface.json'])

run('cookbook-evolution',[R/'build/judasjs_examples_tests',out/'cookbook-evolution','save-evolution'])
