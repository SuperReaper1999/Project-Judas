#!/usr/bin/env python3
"""M78 isolated current-suite observer. Never edit baseline tests or product code.

Run after the recorded all-target Release build succeeds. Existing test defaults
are inspected first; unknown argument surfaces are deferred rather than guessed.
Records every exit, timeout, output and binary identity; zero exit is not a claim
that assertions prove all capabilities. Resumes only completed immutable receipts.
"""
import pathlib,re,os,json,subprocess,time,hashlib,argparse
ROOT=pathlib.Path(__file__).resolve().parents[4]
EV=ROOT/'docs/evidence/m78'; OUT=ROOT/'.cache/m78/linux-baseline'; SUBJECT=ROOT/'.cache/m78/frozen'

def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()

def commands():
 targets=json.loads((EV/'TEST_TARGETS.json').read_text()); tasks=[];deferred=[]
 positional=set('gravity_selection ragdoll fluid_demo_integration character_motor_application boundary_migration shooter_game script_presentation navigation_game liquid liquid_surface profiler_script text_localization text_render localization_application save_storage world_persistence developer_integration developer_authoring model_reimport model_runtime world_building world_authoring_runtime scalability resource_handoff paired_input_script physics_batch_script terrain_authoring terrain_pipeline editor_picking runtime_render runtime_render_authoring runtime_render_retained_region animation_retarget animation_retarget_cook retarget_editor'.split())
 safeDefault=set('impact_timing project_application export windows_readiness paired_input audio liquid_surface_application liquid_surface_refill kinematic_lab'.split())
 implicitOutput=set('scene_transition physics_cast_application joint_application skeletal_animation animation_application pose_composition_application ragdoll_application'.split())
 projectProbe={'collision_application':'collision_lab','fracture_application':'fracture_lab','deformable_application':'deformable_lab','developer_application':'m65_integration','model_application':'import_lab','world_authoring_application':'world_workshop','terrain_application':'terrain_lab','retarget_application':'retarget_lab','mod_application':'modding_lab'}
 projectOutput={'streaming_application':'streamed_range','streaming_revisit':'streamed_range','audio_application':'audio_lab','shooter_application':'shooter_game'}
 for t in targets:
  name=t['target'];key=name.removeprefix('judas_').removesuffix('_tests');s=(ROOT/t['source']).read_text();m=re.search(r'int main\s*\(([^)]*)\)',s);folder=OUT/name
  argv=[];why='explicit no-argument main'
  if m and ('argc' in m[1] or 'argv' in m[1]):
   if name=='judasjs_examples_tests':argv=[str(folder)];why='all existing public cookbook examples'
   elif key in implicitOutput:argv=['--output',str(folder)];why='existing argc==3 output path; test assertions unchanged'
   elif key in projectProbe:
    project=projectProbe[key];mode='zero' if key=='mod_application' else 'probe';argv=[str(SUBJECT/'projects'/project/(project+'.judasproj')),mode,str(folder)];why='ordinary project and inspected application probe mode'
   elif key in projectOutput:
    project=projectOutput[key];argv=[str(SUBJECT/'projects'/project/(project+'.judasproj')),str(folder)];why='ordinary project and isolated result root'
   elif key=='material':argv=[str(folder),str(SUBJECT/'projects/material_lab/Assets/environment/studio.judasenv')];why='normal material pixels plus existing environment asset'
   elif key in {'material_application','runtime_render_application','streaming_performance'}:argv=[str(folder)];why='inspected owned output path; full existing modes'
   elif key=='liquid_surface_loop':argv=[str(SUBJECT/'projects/liquid_surface_demo/Scenes/lab.judas'),str(folder/'view.png')];why='ordinary surface scene and owned capture'
   elif key in {'save_application','save_streaming','save_performance'}:
    project='streamed_range' if key=='save_streaming' else 'save_lab';argv=[str(SUBJECT/'projects'/project/(project+'.judasproj')),str(folder)] if key=='save_performance' else [str(SUBJECT/'projects'/project/(project+'.judasproj')),'write',str(folder)];why='existing save application writes in isolated user data; fresh-process reads scheduled separately'
   elif key=='signal_application':argv=[str(SUBJECT/'projects/signals_lab/signals_lab.judasproj')];why='ordinary signal project with isolated saves and callback fault cases'
   elif key=='audio_acoustics':argv=[str(ROOT/'.cache/m78/audio-fixtures')];why='fresh generated known codec fixtures required before execution'
   elif key in positional:argv=[str(folder)];why='inspected output positional argument'
   elif key in safeDefault:why='inspected defaults: read-only source / owned temporary output'
   elif key=='contact_geometry':argv=[str(ROOT/'docs/evidence/ftft4/geometry/fixtures.txt')];why='read-only accepted geometry fixture'
   elif '"--output"' in s:argv=['--output',str(folder)];why='existing explicit output option'
   elif key=='character_lab':argv=[str(folder)];why='fresh project output'
   elif key=='balance_lab':argv=[str(folder),'standing'];why='standing mode only; other modes separate'
   elif key=='model_import':
    source=ROOT/'.cache/m66/originals/Skateboarder.fbx'
    if not source.is_file():deferred.append(dict(t,reason='original local FBX unavailable'));continue
    argv=[str(source),str(folder)];why='existing original FBX, output isolated'
   elif key=='consumer_authoring':argv=[str(ROOT/'projects/post_m65_consumers'),str(folder)];why='read-only consumer source and owned output'
   elif key=='scalability_pipeline':argv=[str(folder),str(ROOT/'build/judas')];why='fresh output and normal runtime'
   elif key=='retarget_lab':argv=[str(ROOT/'projects/retarget_lab'),str(folder/'result.json')];why='read-only existing lab and owned result'
   else:deferred.append(dict(t,reason='argument/mutation/output contract requires further trace'));continue
  tasks.append({'id':name,'source':t['source'],'command':[str(ROOT/'build'/name),*argv],'adapter':why,'timeoutSeconds':300})
 return tasks,deferred

def main():
 p=argparse.ArgumentParser();p.add_argument('--plan',action='store_true');a=p.parse_args();tasks,deferred=commands();OUT.mkdir(parents=True,exist_ok=True)
 (EV/'BASELINE_PLAN.json').write_text(json.dumps({'tasks':tasks,'deferred':deferred,'scope':'Existing suites only; each suite requires assertion review before capability evidence classification.'},indent=2)+'\n')
 print('scheduled',len(tasks),'deferred',len(deferred),flush=True)
 if a.plan:return
 assert (SUBJECT/'CMakeLists.txt').is_file(), 'Complete isolated baseline archive before execution'
 baseline=json.loads((EV/'BASELINE.json').read_text());assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()==baseline['checkpoint']
 env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')};env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(OUT/'userdata'),XDG_CONFIG_HOME=str(OUT/'config'))
 (EV/'linux').mkdir(exist_ok=True)
 for task in tasks:
  receipt=EV/'linux'/(task['id']+'.json')
  if receipt.exists():continue
  binary=pathlib.Path(task['command'][0]);row=dict(task,checkpoint=baseline['checkpoint'],environment={k:env[k] for k in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','LIBGL_ALWAYS_SOFTWARE','XDG_DATA_HOME','XDG_CONFIG_HOME')},cwd=str(SUBJECT));start=time.monotonic();log=EV/'linux'/(task['id']+'.log')
  print('RUN',task['id'],flush=True)
  if not binary.is_file():row['status']='blocked-missing-binary'
  else:
   row['executableSHA256']=sha(binary)
   with log.open('wb') as out:
    try:r=subprocess.run(task['command'],cwd=SUBJECT,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=task['timeoutSeconds']);row['exitCode']=r.returncode;row['status']='exited-zero' if r.returncode==0 else 'failed'
    except subprocess.TimeoutExpired:row['status']='timeout'
   data=log.read_text(errors='replace');row['passingLines']=len(re.findall(r'^\s*(?:PASS|OK)\b',data,re.M));row['failingLines']=len(re.findall(r'^\s*FAIL\b',data,re.M));row['log']=str(log.relative_to(ROOT));row['logSHA256']=sha(log)
  row['seconds']=time.monotonic()-start;receipt.write_text(json.dumps(row,indent=2)+'\n');print(row['status'],task['id'],round(row['seconds'],3),flush=True)
 if subprocess.check_output(['git','diff','--name-only'],cwd=ROOT,text=True).strip():raise RuntimeError('Unexpected tracked changes: retain receipts and investigate')
if __name__=='__main__':main()
