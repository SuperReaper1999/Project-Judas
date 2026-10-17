#!/usr/bin/env python3
"""Focused native UI validation. Requires built targets; retains failed runs."""
from pathlib import Path
import os,subprocess,time,json,datetime,argparse
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);args=p.parse_args();out=(R/args.output).resolve();out.mkdir(parents=True,exist_ok=False)
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(out/'user-data'));runs=[]
def run(name,cmd,timeout=240):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f:result=subprocess.run(list(map(str,cmd)),cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
 runs.append(dict(name=name,argv=list(map(str,cmd)),exit=result.returncode,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(dict(base=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),runs=runs),indent=2)+'\n');print(name,result.returncode,flush=True)
 if result.returncode:raise RuntimeError(name+' failed; evidence preserved')
for name,argv in [
 ('judas_ui_production_tests',[out/'production']),('judas_ui_showcase_application_tests',[out/'application']),
 ('judas_runtime_ui_tests',['--output',out/'legacy-ui']),('judas_runtime_ui_application_tests',['--output',out/'legacy-application']),
 ('judas_text_localization_tests',[]),('judas_input_tests',[]),('judas_touch_input_tests',[]),
 ('judas_local_multiplayer_tests',[]),('judas_local_multiplayer_script_tests',[]),('judas_local_device_adapter_tests',[]),('judas_animation_workflow_tests',[])]:run(name,[R/'build'/name,*argv])
for mode in ['write','read']:run('save-'+mode,[R/'build/judas_save_application_tests',R/'projects/save_lab/save_lab.judasproj',mode,out/('save-'+mode)])
for case in ['ui-controls','ui','surface']:run('cookbook-'+case,[R/'build/judasjs_examples_tests',out/'cookbook',case])
run('api',['node',R/'scripts/check_judasjs_api.mjs','--typescript',R/'.cache/m90/types/node_modules/typescript','--runtime',out/'cookbook/surface.json'])
run('source-check',[R/'build/judas_script_check',R/'projects/ui_showcase/ui_showcase.judasproj'])
