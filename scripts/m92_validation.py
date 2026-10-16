#!/usr/bin/env python3
"""Focused serial Linux rendering checks. No production-suite dispatch or Git writes."""
import argparse,datetime,hashlib,json,os,pathlib,subprocess,time
R=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',action='store_true');p.add_argument('--output',default='.cache/m92/validation');p.add_argument('--typescript');a=p.parse_args();out=(R/a.output).resolve();out.mkdir(parents=True,exist_ok=True)
targets=['judas_rendering_architecture_tests','judas_rendering_lab_tests','judas_render_camera_tests','judas_local_multiplayer_render_tests','judas_planet_terrain_tests','judas_planet_world_tests','judas_planet_streaming_tests','judas_material_tests','judas_script_tests','judas_runtime_render_tests','judas_runtime_render_application_tests','judas_weather_render_tests','judasjs_examples_tests']
receipt={'started':datetime.datetime.now(datetime.timezone.utc).isoformat(),'platform':'native Linux','humanAcceptance':False,'windows':'not run','android':'not run','commands':[]}
def run(name,cmd,timeout=240):
 env={k:v for k,v in os.environ.items() if k not in ['JUDAS_TEST_SCRIPT','JUDAS_ENGINE_ROOT']};env['JUDAS_WORLD_STATE']='none';begin=time.monotonic();log=out/(name+'.log')
 with log.open('w') as f:
  try:r=subprocess.run([str(c) for c in cmd],cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout);code=r.returncode
  except subprocess.TimeoutExpired:code=124
 row={'name':name,'argv':list(map(str,cmd)),'exit':code,'seconds':time.monotonic()-begin,'log':str(log.relative_to(R))};receipt['commands'].append(row);(out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');print(name,code,round(row['seconds'],3),flush=True);return code==0
if a.build:
 if not run('configure',['cmake','-S','.', '-B','build','-DCMAKE_BUILD_TYPE=Release']):raise SystemExit(1)
 if not run('build',['cmake','--build','build','--target',*targets,'-j','2'],1800):raise SystemExit(1)
for target in targets:
 args=[]
 if target in ['judas_rendering_architecture_tests','judas_rendering_lab_tests','judas_render_camera_tests','judas_runtime_render_tests','judas_runtime_render_application_tests','judas_weather_render_tests','judas_planet_streaming_tests']:args=[out/target]
 if target=='judas_render_camera_tests':args=['--output',out/target]
 if target=='judasjs_examples_tests':args=[out/'cookbook','surface']
 run(target,[R/'build'/target,*args])
 if target=='judas_runtime_render_application_tests':run(target+'-read',[R/'build'/target,*args,'read'])
if a.typescript:run('api',['node',R/'scripts/check_judasjs_api.mjs','--typescript',a.typescript,'--runtime',out/'cookbook/surface.json'])
receipt['binaries']={t:hashlib.sha256((R/'build'/t).read_bytes()).hexdigest() for t in targets};receipt['pass']=all(c['exit']==0 for c in receipt['commands']);(out/'RESULTS.json').write_text(json.dumps(receipt,indent=2)+'\n');raise SystemExit(0 if receipt['pass'] else 1)
