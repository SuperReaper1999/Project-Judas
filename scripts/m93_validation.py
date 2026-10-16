#!/usr/bin/env python3
"""Focused serial asset integration checks; no full-suite or Git dispatch."""
import argparse,datetime,hashlib,json,os,pathlib,subprocess,time
R=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',action='store_true');p.add_argument('--typescript',default=os.environ.get('JUDAS_TYPESCRIPT',str(R/'.cache/m90/types/node_modules/typescript') if (R/'.cache/m90/types/node_modules/typescript').exists() else 'typescript'));p.add_argument('--model-fixture',default=str(R/'.cache/m66/originals/Skateboarder.fbx'));p.add_argument('--output',default='.cache/m93/validation');a=p.parse_args();out=(R/a.output).resolve();out.mkdir(parents=True,exist_ok=True)
targets=['judas_asset_pipeline_tests','judas_asset_residency_tests','judas_asset_lab_tests','judas_rendering_architecture_tests','judas_render_camera_tests','judas_local_multiplayer_render_tests','judas_material_tests','judas_material_application_tests','judas_model_import_tests','judas_model_reimport_tests','judas_resource_handoff_tests','judas_async_application_tests','judas_runtime_render_tests','judas_runtime_render_authoring_tests','judas_save_storage_tests','judas_streaming_tests','judas_export_tests','judasjs_examples_tests']
receipt={'started':datetime.datetime.now(datetime.timezone.utc).isoformat(),'platform':'native Linux','humanAcceptance':False,'windows':'not run','android':'not run','commands':[]}
def run(name,cmd,timeout=240):
 env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(out/'user-data'));begin=time.monotonic();log=out/(name+'.log')
 with log.open('w') as f:
  try:r=subprocess.run(list(map(str,cmd)),cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout);code=r.returncode
  except subprocess.TimeoutExpired:code=124
 row={'name':name,'argv':list(map(str,cmd)),'exit':code,'seconds':time.monotonic()-begin};receipt['commands'].append(row);(out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');print(name,code,round(row['seconds'],3),flush=True);return code==0
tools=['judas','judas_editor','judas_export','judas_cook','judas_asset_pipeline_benchmark']
if a.build:
 if not run('build',['cmake','--build','build','--target',*targets,*tools,'-j','2'],2400):raise SystemExit(1)
for t in targets:
 args=[]
 if t in ['judas_asset_pipeline_tests','judas_asset_residency_tests','judas_asset_lab_tests','judas_rendering_architecture_tests','judas_model_reimport_tests','judas_runtime_render_tests','judas_runtime_render_authoring_tests','judas_save_storage_tests','judas_streaming_tests']:args=[out/t]
 if t=='judas_save_storage_tests':args=[out/'m61-storage-m93']
 if t=='judas_model_import_tests':args=[a.model_fixture,out/'model-import']
 if t=='judas_render_camera_tests':args=['--output',out/t]
 if t=='judas_material_application_tests':args=[out/t,'quick']
 if t=='judas_resource_handoff_tests':args=[out/(t+'-owned')]
 if t=='judas_async_application_tests':args=['--output',out/t]
 if t=='judasjs_examples_tests':args=[out/'cookbook','surface']
 run(t,[R/'build'/t,*args])
run('api',['node',R/'scripts/check_judasjs_api.mjs','--typescript',a.typescript,'--runtime',out/'cookbook/surface.json'])
receipt['binaries']={t:hashlib.sha256((R/'build'/t).read_bytes()).hexdigest() for t in targets+tools};receipt['pass']=all(c['exit']==0 for c in receipt['commands']);(out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');raise SystemExit(0 if receipt['pass'] else 1)
