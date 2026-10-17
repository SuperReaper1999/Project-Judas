#!/usr/bin/env python3
"""Focused M94 final-source checks. No full production-suite sweep."""
from pathlib import Path
import argparse,datetime,json,os,subprocess,time
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',default='.cache/m94/final');p.add_argument('--build',action='store_true');p.add_argument('--start-at');a=p.parse_args();out=(R/a.output).resolve();out.mkdir(parents=True,exist_ok=False)
tests=['judas_script_developer_tools_tests','judas_script_tools_editor_tests','judas_script_developer_consumer_tests','judas_script_tests','judas_script_application_tests','judas_signal_tests','judas_signal_application_tests','judas_script_presentation_tests','judas_runtime_ui_tests','judas_runtime_ui_application_tests','judas_scene_transition_tests','judas_save_application_tests','judas_prefab_tests','judas_mod_application_tests','judas_character_motor_tests','judasjs_examples_tests']
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(out/'user-data'))
runs=[];record={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),'platform':'native Linux; Windows/Android unvalidated','tests':runs}
def run(name,cmd,timeout=240):
 start=time.monotonic()
 with (out/(name+'.log')).open('w') as f:result=subprocess.run(list(map(str,cmd)),cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
 runs.append({'name':name,'argv':list(map(str,cmd)),'seconds':time.monotonic()-start,'exit':result.returncode});(out/'RESULTS.json').write_text(json.dumps(record,indent=2)+'\n');print(name,result.returncode,flush=True)
 if result.returncode:raise RuntimeError(name+' failed; raw evidence retained')
if a.build:run('build',['cmake','--build','build','--target',*tests,'judas_script_check','judas_editor','judas','judas_export','-j','3'],2400)
if a.start_at:tests=tests[tests.index(a.start_at):]
for target in tests:
 args=[]
 if target=='judas_save_application_tests':
  for mode in ['write','read']:run('save-'+mode,[R/'build'/target,R/'projects/save_lab/save_lab.judasproj',mode,out/('save-'+mode)])
  continue
 if target=='judas_mod_application_tests':args=[R/'projects/modding_lab/modding_lab.judasproj','zero',out/'mods-off']
 if target=='judasjs_examples_tests':args=[out/'cookbook']
 run(target,[R/'build'/target,*args])
for game in ['developer_lab','shooter_game','boundary_demo','asset_lab','core_scramble','lastlight','planetfall','streamed_range']:
 project=R/'projects'/game/(game+'.judasproj')
 run('source-'+game,[R/'build/judas_script_check',project])
run('api',['node',R/'scripts/check_judasjs_api.mjs','--typescript',R/'.cache/m90/types/node_modules/typescript','--runtime',out/'cookbook/surface.json'])
print(out,flush=True)
