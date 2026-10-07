#!/usr/bin/env python3
"""Normal consumer/editor/cold-save and moved source-hidden package proof.
Requires a fresh owned directory. Games, historical scripts and source projects
are read-only; native observers only queue physical input and observe Application.
"""
import argparse, importlib.util, json, os, re, shutil, subprocess, sys, time
from pathlib import Path
sys.dont_write_bytecode=True
R=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--bails-only',action='store_true',help='Narrow real bail/recovery follow-up through the normal deterministic application harness');a=p.parse_args();out=a.output.resolve()
 if out.exists():p.error('Choose NEW output; prior evidence is preserved.')
 out.mkdir(parents=True);owned=out/'owned';owned.mkdir();records=[]
 env={k:v for k,v in os.environ.items() if not k.startswith(('JUDAS_','FTFT_'))};env.update(SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(out/'user-data'),XDG_CONFIG_HOME=str(out/'user-config'))
 def run(label,command,extra=None,cwd='/tmp'):
  t=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=dict(env,**(extra or {})),capture_output=True,text=True,timeout=360)
  (out/(label+'.log')).write_text(r.stdout+r.stderr);records.append({'label':label,'command':list(map(str,command)),'cwd':str(cwd),'exit':r.returncode,'seconds':time.monotonic()-t});(out/'results.json').write_text(json.dumps(records,indent=2)+'\n');print(label,r.returncode,flush=True)
  if r.returncode:raise RuntimeError(label+' failed; retained log')
  return r
 def copy(source,destination):shutil.copytree(source,destination,ignore=shutil.ignore_patterns('.cache','__pycache__','Saves','*.import-report.json'))
 # Reuse accepted focused scenarios by redirecting output only. Their documented
 # spawn/property/probe changes remain isolated, and their assertions stay intact.
 spec=importlib.util.spec_from_file_location('consumer',R/'scripts/post_m65_review.py');consumer=importlib.util.module_from_spec(spec);spec.loader.exec_module(consumer);consumer.E=out/'scenarios'
 if a.bails_only:
  game=owned/'bails';copy(R/'projects/post_m65_consumers',game);scene=game/'Scenes/skate/park.judas'
  consumer.probe(game,scene,"""import {console} from 'judas';
import {game} from '../skate/scripts/game.js';
import {RAGDOLL_KEYS} from '../skate/scripts/presentation.js';
export default class {constructor(){this.n=0;} uiUpdate(){
 if(++this.n%60)return;const r=game.skater?.view.ragdoll;
 console.log('M68_BAIL '+JSON.stringify({bails:game.bails,active:r?.active===true,
 bodies:r?.active?RAGDOLL_KEYS.filter(k=>r.body(k)?.valid).length:0}));}}
""")
  commands=out/'bails.txt';commands.write_text('FRAMES 500\nLOG_EVERY 0\nACTION skate_bail 1 20 21\nACTION skate_bail 1 240 241\n')
  result=run('skate-repeated-bails',[R/'build/judas',scene],{'JUDAS_TEST_SCRIPT':str(commands),'JUDAS_ENGINE_ROOT':str(R)})
  states=[json.loads(line.split('M68_BAIL ',1)[1]) for line in result.stdout.splitlines() if 'M68_BAIL ' in line]
  assert any(s['bails']==1 and s['active'] and s['bodies']==13 for s in states),states
  assert any(s['bails']==2 and s['active'] and s['bodies']==13 for s in states),states
  assert states[-1]['bails']==2 and not states[-1]['active'],states
  assert 'RAGDOLL enter failed' not in result.stdout,result.stdout
  (out/'BAILS.json').write_text(json.dumps({'pass':True,'states':states,'note':'Logical skate_bail uses authored X binding; existing harness explicitly blocks resources for deterministic readiness. Earlier T input was checkpoint, not bail.'},indent=2)+'\n')
  return
 old=sys.argv;oldEnv=dict(os.environ);os.environ.clear();os.environ.update(env)
 try:
  for mode in ['skate-stream-revisit','skate-save','skate-street','rooftop-checkpoint','void-input']:
   sys.argv=['post_m65_review.py',mode];consumer.main()
 finally:sys.argv=old;os.environ.clear();os.environ.update(oldEnv)
 games=owned/'games';copy(R/'projects/post_m65_consumers',games)
 commands='FRAMES 240\nLOG_EVERY 0\n'
 for frame,action,scene in [(5,'skate','skate/park'),(60,'rooftop','rooftop/rooftops'),(100,'void','void/system'),(150,'menu','launcher'),(170,'skate','skate/park'),(200,'rooftop','rooftop/rooftops'),(220,'menu','launcher')]:
  commands+=f'ACTION collection_{action} 1 {frame} {frame+1}\nWAIT_SERVICES scene 10000 {frame+1}\nEXPECT_SCENE Scenes/{scene}.judas {frame+2}\n'
 commands+='WAIT_SERVICES stream 15000 8\n'
 for frame,action in [(50,'skate'),(90,'rooftop'),(130,'void')]:commands+=f'ACTION {action}_pause 1 {frame} {frame+1}\nEXPECT_PAUSED 1 {frame+2}\n'
 commandFile=out/'switch.txt';commandFile.write_text(commands)
 run('game-switches',[R/'build/judas',games/'post_m65_consumers.judasproj'],{'JUDAS_TEST_SCRIPT':str(commandFile),'JUDAS_ENGINE_ROOT':str(R)})
 run('editor-three-games',[R/'build/judas_editor',games/'post_m65_consumers.judasproj'],{'JUDAS_EDITOR_AUTOTEST':str(out/'editor-games'),'JUDAS_ENGINE_ROOT':str(R)})
 workshop=owned/'workshop';copy(R/'projects/world_workshop',workshop)
 run('editor-workshop',[R/'build/judas_editor',workshop/'world_workshop.judasproj'],{'JUDAS_EDITOR_AUTOTEST':str(out/'editor-workshop'),'JUDAS_ENGINE_ROOT':str(R)})
 save=out/'workshop-save';run('workshop-fresh-write',[R/'build/judas_world_authoring_application_tests',workshop/'world_workshop.judasproj','write',save],{'JUDAS_ENGINE_ROOT':str(R)})
 # Exercise closure on real content with every registered identity as an explicit
 # runtime root. This proves package/save compatibility without guessing dynamic
 # JS demand; the smaller exclusion fixture separately proves actual pruning.
 reportProjects=[('three-games',games,'post_m65_consumers.judasproj'),('workshop',workshop,'world_workshop.judasproj')]
 moved=[]
 for label,source,project in reportProjects:
  projectFile=source/project;projectText=projectFile.read_text()
  roots=sorted({re.search(r'id "([0-9a-f]{32})"',meta.read_text())[1] for meta in (source/'Assets').rglob('*.judasmeta')})
  if projectText.lstrip().startswith('{'):
   document=json.loads(projectText);document['data']['exportAssetPolicy']='closure';document['data']['runtimeAssets']=roots;projectFile.write_text(json.dumps(document,indent=2)+'\n')
  else:projectFile.write_text(projectText+'\nexport-asset-policy "closure"\nruntime-assets '+json.dumps(' '.join(json.dumps(x) for x in roots))+'\n')
  package=owned/('package-'+label);run('export-'+label,[R/'build/judas_export',source/project,package,R/'build/judas'])
  destination=Path('/tmp')/('JudasM68-'+label+'-'+str(os.getpid()));shutil.copytree(package,destination);moved.append(str(destination));source.rename(source.with_name(source.name+'-SOURCE-HIDDEN'))
  # No JUDAS_ENGINE_ROOT and unrelated cwd; actual executable uses package marker.
  if label=='three-games':run('moved-'+label,[destination/'judas'],{'JUDAS_TEST_SCRIPT':str(commandFile)})
  if label=='workshop':
   observer=destination/'review-observer';shutil.copyfile(R/'build/judas_world_authoring_application_tests',observer);observer.chmod(0o755)
   run('moved-fresh-process-workshop-load',[observer,destination/'game.judasproj','read',save]);observer.unlink()
 (out/'MOVED_PACKAGES.json').write_text(json.dumps(moved,indent=2)+'\n')
 # Full original professional content remains local-only. Fresh source ownership
 # is hidden after export; the shipped archive alone must supply the animation.
 lab=owned/'import-lab';copy(R/'projects/import_lab',lab)
 package=owned/'package-import';run('export-original',[R/'build/judas_export',lab/'import_lab.judasproj',package,R/'build/judas'])
 destination=Path('/tmp')/('JudasM68-import-'+str(os.getpid()));shutil.copytree(package,destination);lab.rename(owned/'import-lab-SOURCE-HIDDEN')
 observer=destination/'review-observer';shutil.copyfile(R/'build/judas_model_application_tests',observer);observer.chmod(0o755);run('moved-original-startup',[observer,destination/'game.judasproj','probe',out/'moved-import']);observer.unlink();moved.append(str(destination))
 (out/'MOVED_PACKAGES.json').write_text(json.dumps(moved,indent=2)+'\n')
 print('M68 current consumers and moved cold-save/package proof passed',flush=True)
if __name__=='__main__':main()
