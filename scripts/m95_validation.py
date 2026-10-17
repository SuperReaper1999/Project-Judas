#!/usr/bin/env python3
"""Focused Linux M95 validation; intentionally no production-suite sweep."""
from pathlib import Path
import os,subprocess,json,time,datetime,argparse
R=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser();parser.add_argument('--output',default='.cache/m95/final');parser.add_argument('--start-at',default='');a=parser.parse_args();out=R/a.output;out.mkdir(parents=True,exist_ok=False)
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(out/'data'))
runs=[]
def run(name,args,timeout=240):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f:r=subprocess.run(list(map(str,args)),cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
 runs.append(dict(name=name,argv=list(map(str,args)),exit=r.returncode,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(dict(head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),platform='native Linux; Windows/Android M95 unvalidated',runs=runs),indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name+' failed; evidence retained')
tests=['judas_animation_workflow_tests','judas_animation_workflow_application_tests','judas_character_motor_tests','judas_motor_runtime_tags_tests','judas_skeletal_animation_tests','judas_pose_composition_tests','judas_physical_animation_tests','judas_animation_retarget_cook_tests','judas_model_reimport_tests','judas_save_storage_tests','judas_retarget_editor_tests','judas_animation_application_tests','judas_script_developer_tools_tests']
if a.start_at:tests=tests[tests.index(a.start_at):]
for name in tests:
 args=[out/name]if name in ['judas_motor_runtime_tags_tests','judas_animation_retarget_cook_tests','judas_model_reimport_tests','judas_save_storage_tests','judas_retarget_editor_tests']else []
 if name=='judas_save_storage_tests':args=[out/'m61-storage-m95']
 run(name,[R/'build'/name,*args])
for mode in ['write','read']:run('animation-cold-'+mode,[R/'build/judas_animation_cold_save_tests',mode,out/'cold-save'])
for mode in ['write','read']:run('save-'+mode,[R/'build/judas_save_application_tests',R/'projects/save_lab/save_lab.judasproj',mode,out/('save-'+mode)])
for mode in ['write','read','read-b']:run('save-streaming-'+mode,[R/'build/judas_save_streaming_tests',R/'projects/streamed_range/streamed_range.judasproj',mode,out/('save-streaming-'+mode)])
for case in ['animation-motion','animation','surface']:run('cookbook-'+case,[R/'build/judasjs_examples_tests',out/'cookbook',case])
run('api',[ 'node',R/'scripts/check_judasjs_api.mjs','--typescript',R/'.cache/m90/types/node_modules/typescript','--runtime',out/'cookbook/surface.json'])
run('source-check',[R/'build/judas_script_check',R/'projects/animation_studio/animation_studio.judasproj'])
