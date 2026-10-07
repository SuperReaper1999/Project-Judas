#!/usr/bin/env python3
"""One frozen M67 Release/production/actual async gate, with isolated outputs.
Uses the existing production discovery and assertions. Only argument/output
adapters change; protected historical runners and evidence remain untouched.
"""
import argparse,importlib.util,json,re,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'docs/evidence/m67/final'
def main():
 global OUT
 parser=argparse.ArgumentParser();parser.add_argument("--resume",action="store_true");parser.add_argument("--output",type=Path);args=parser.parse_args()
 if args.output:OUT=args.output.resolve()
 assert not OUT.exists(),'Preserve the completed gate; use affected checks for late fixes.'
 if not args.resume:
  backup=ROOT/'.cache/m67-before-clean-release';assert not backup.exists();backup.parent.mkdir(exist_ok=True)
  if (ROOT/'build').exists():(ROOT/'build').rename(backup)
 else:assert (ROOT/'build/CMakeCache.txt').exists(),'Resume only the interrupted clean build.'
 OUT.mkdir(parents=True)
 subprocess.run([sys.executable,str(ROOT/'tools/CreateAudioFixtures.py'),str(ROOT/'.cache/m67-audio')],cwd=ROOT,check=True)
 spec=importlib.util.spec_from_file_location('gate',ROOT/'scripts/ftft9_validation.py');gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
 gate.REVIEWED_SHARED.add('src/WorldState.cpp') # Existing unchanged M59 composed-save guard.
 source=(ROOT/'scripts/m56_validation.py').read_text();gate.OUTPUT_TESTS.update(re.findall(r"'(judas_\w+_tests)'",source.split('original=subprocess.run')[0]))
 original=subprocess.run
 def run(command,*args,**kwargs):
  if command==['cmake','-S','.','-B','build']:command=command+['-DCMAKE_BUILD_TYPE=Release']
  if isinstance(command,list) and command[:3]==['cmake','--build','build']:kwargs['timeout']=3600
  if isinstance(command,list) and command:
   name=Path(command[0]).name
   application={'judas_collision_application_tests':['projects/collision_lab/collision_lab.judasproj','probe','collision'],'judas_fracture_application_tests':['projects/fracture_lab/fracture_lab.judasproj','probe','fracture'],'judas_deformable_application_tests':['projects/deformable_lab/deformable_lab.judasproj','probe','deformable'],'judas_shooter_application_tests':['projects/shooter_game/shooter_game.judasproj',None,'shooter'],'judas_streaming_application_tests':['projects/streamed_range/streamed_range.judasproj',None,'streaming'],'judas_audio_application_tests':['projects/audio_lab/audio_lab.judasproj',None,'audio'],'judas_save_application_tests':['projects/save_lab/save_lab.judasproj','write','save'],'judas_save_streaming_tests':['projects/streamed_range/streamed_range.judasproj','write','save-streaming'],'judas_save_performance_tests':['projects/save_lab/save_lab.judasproj',None,'save-performance'],'judas_developer_application_tests':['projects/m65_integration/m65_integration.judasproj','probe','developer-application'],'judas_model_application_tests':['projects/import_lab/import_lab.judasproj','probe','import-application'],'judas_world_authoring_application_tests':['projects/world_workshop/world_workshop.judasproj','probe','authoring-application']}
   if name in application and len(command)==1:
    project,mode,label=application[name];command=command+[str(ROOT/project)]+([mode] if mode else [])+[str(OUT/label)]
   if name=='judas_world_building_tests':command=command+[str(OUT/'authoring-services')]
   if name=='judas_world_authoring_runtime_tests':command=command+[str(OUT/'authoring-runtime')]
   if name.startswith('judas_world_authoring_'):kwargs['env']=dict(kwargs.get('env') or {},SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(ROOT/'.cache/m67-gate-data'))
   if name=='judas_model_import_tests':command=command+[str(ROOT/'.cache/m66/originals/Skateboarder.fbx'),str(OUT/'import-numerical')]
   if name=='judas_model_reimport_tests':command=command+[str(ROOT/'.cache/m67-final-reimport')]
   if name=='judas_model_runtime_tests':command=command+[str(OUT/'import-GL')]
   if name.startswith('judas_model_') or name=='judasjs_examples_tests':kwargs['env']=dict(kwargs.get('env') or {},SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='0')
   if name=='judasjs_examples_tests':command=command+[str(OUT/'cookbook')]
   if name=='judas_consumer_authoring_tests' and len(command)==1:command=command+[str(ROOT/'projects/post_m65_consumers'),str(OUT/'consumer-authoring')]
   if name in ('judas_developer_integration_tests','judas_developer_authoring_tests') and len(command)==1:command=command+[str(OUT/name)]
   if name=='judas_material_application_tests' and len(command)==1:command=command+[str(OUT/'material-application'),'quick']
   if name=='judas_material_tests' and len(command)==1:command=command+[str(OUT/'material-gl'),str(ROOT/'projects/material_lab/Assets/environment/studio.judasenv')]
   if name in ('judas_text_localization_tests','judas_text_render_tests','judas_localization_application_tests') and len(command)==1:command=command+[str(OUT/name)]
   if name=='judas_streaming_performance_tests' and len(command)==1:command=command+[str(OUT/'streaming-performance')]
   if name=='judas_audio_acoustics_tests' and len(command)==1:command=command+[str(ROOT/'.cache/m67-audio')]
   if name=='judas_save_storage_tests' and len(command)==1:command=command+[str(ROOT/'.cache/m61-storage-m67')]
   if name.startswith('judas_save_'):kwargs['env']=dict(kwargs.get('env') or {},XDG_DATA_HOME=str(ROOT/'.cache/m67-gate-data'),SDL_AUDIODRIVER='dummy')
  return original(command,*args,**kwargs)
 subprocess.run=run
 try:result=gate.production(OUT,4)
 finally:subprocess.run=original
 (OUT/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert result['overall_pass']
if __name__=='__main__':main()
