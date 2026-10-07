#!/usr/bin/env python3
"""M67 normal CLI/editor/VM/consumer/cold-save/moved-export follow-up.
Run after scripts/m67_validation.py. Fresh outputs and save namespace required.
Does not build, rerun production, alter shipped content or touch other workspaces.
"""
from pathlib import Path
import argparse,os,json,subprocess,shutil,hashlib,time,re
ROOT=Path(__file__).resolve().parents[3]
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--export-only',action='store_true');p.add_argument('--edited-project',type=Path);args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
 env={k:v for k,v in os.environ.items() if not k.startswith(('JUDAS_','FTFT_'))};env.update(SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',JUDAS_PROFILE='1',XDG_DATA_HOME=str(out/'user-data'),XDG_CONFIG_HOME=str(out/'config'))
 records=[]
 def run(label,command,extra=None,cwd=ROOT,expect=0,timeout=180):
  command=[str(x) for x in command];start=time.monotonic();r=subprocess.run(command,cwd=cwd,env=dict(env,**(extra or {})),capture_output=True,text=True,timeout=timeout);text=r.stdout+r.stderr;(out/(label+'.log')).write_text(text)
  records.append({'name':label,'command':command,'cwd':str(cwd),'exit':r.returncode,'seconds':time.monotonic()-start,'expected':expect});(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print(label,r.returncode,flush=True)
  assert (r.returncode==0 if expect==0 else r.returncode!=0),(label,text[-3000:])
  if expect==0:assert not re.search(r'^\s*(FAIL|FAULT)\b',text,re.M),(label,text[-3000:])
  return text
 cli=ROOT/'build/judas_scene_author';project=ROOT/'projects/world_workshop/world_workshop.judasproj'
 if not args.export_only:
  run('performance',[ROOT/'build/judas_world_authoring_performance',out/'performance'])
  node='/home/conner/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node';ts='/tmp/judas-m66-typescript/package/lib/typescript.js'
  run('vm-enumeration',[ROOT/'build/judasjs_examples_tests',out/'enumeration','surface'])
  run('api-types-live',[node,ROOT/'scripts/check_judasjs_api.mjs','--typescript',ts,'--runtime',out/'enumeration/surface.json'])
  # Editing tests use only a disposable copy. The shipped project is untouched.
  edited=out/'edited-project';shutil.copytree(project.parent,edited);scene=edited/'Scenes/workshop.judas';hud=edited/'Assets/ui/workshop.judasui';config=edited/project.name
  run('inspect',[cli,'--inspect','scene',scene]);run('dependencies',[cli,'--dependencies','scene',scene]);run('references',[cli,'--references','scene',scene])
  before=json.loads(scene.read_text());recipe=edited/'Authoring/gallery-0.judasrecipe';doc=json.loads(recipe.read_text());old_count=doc['data']['count'];doc['data']['count']=old_count+1;recipe.write_text(json.dumps(doc))
  run('generation-preview',[cli,'--generate',recipe,scene,config,scene,'--dry-run'])
  run('generation-edit',[cli,'--generate',recipe,scene,config,scene,'--overwrite'])
  after=json.loads(scene.read_text());a={x['id'] for x in before['data']['objects']};b={x['id'] for x in after['data']['objects']};assert a.issubset(b);assert len(after['data']['objects'])==len(before['data']['objects'])+1
  run('semantic-diff',[cli,'--diff','scene',project.parent/'Scenes/workshop.judas',scene])
  ramp=edited/'Authoring/quarter.judasrecipe';doc=json.loads(ramp.read_text());doc['data']['profile'][0][0]= -3.25;ramp.write_text(json.dumps(doc))
  run('physical-profile-edit',[cli,'--generate',ramp,scene,config,scene,'--overwrite'])
  ui=json.loads(hud.read_text());i=next(i for i,e in enumerate(ui['data']['elements']) if e['id']=='marker')
  run('ui-source-edit',[cli,'--edit','ui',hud,hud,f'/data/elements/{i}/offset','[12,8]','--overwrite'])
  # Logical binding-scale edit preserves the rest of the project and all binding IDs.
  run('input-source-edit',[cli,'--edit','project',config,config,'/data/input/0/bindings/0/scale','0.9','--overwrite'])
  invalid=out/'invalid.judas';invalid.write_text('{"kind":"scene","schema":1,"data":')
  run('syntax-diagnostic',[cli,'--validate','scene',invalid],expect=1)
  invalid.write_text(json.dumps({'kind':'scene','schema':1,'data':{'settings':{},'objects':[{'id':1,'name':'bad','fields':{'parent':2}}]}}))
  run('reference-diagnostic',[cli,'--validate','scene',invalid],expect=1)
  invalid_recipe=out/'invalid.judasrecipe';doc=json.loads(ramp.read_text());doc['data']['profile']=[[0,0],[1,0],[.2,.2],[1,1],[0,1]];invalid_recipe.write_text(json.dumps(doc));old_bytes=scene.read_bytes()
  run('geometry-diagnostic',[cli,'--generate',invalid_recipe,scene,config,scene,'--overwrite'],expect=1);assert scene.read_bytes()==old_bytes
  run('editor-source-roundtrip',[ROOT/'build/judas_editor',config],{'JUDAS_EDITOR_AUTOTEST':str(out/'editor'),'JUDAS_EDITOR_AUTOTEST_AUTHORING':'1','JUDAS_EDITOR_AUTOTEST_AUTHORING_LOCALE':'ar'})
  log=(out/'editor-source-roundtrip.log').read_text();assert 'shared authoring batch + undo PASS' in log and 'authored scene after play/stop is IDENTICAL' in log and 'save ok' in log
  # Save and load are separate Application/QuickJS/GL processes, not an in-memory loop.
  cold=out/'cold';run('cold-write',[ROOT/'build/judas_world_authoring_application_tests',project,'write',cold]);run('cold-read',[ROOT/'build/judas_world_authoring_application_tests',project,'read',cold])
  # Existing Three Games remains a normal consumer; no tuning/assets are rewritten.
  consumer=ROOT/'projects/post_m65_consumers/post_m65_consumers.judasproj';events=out/'consumer-events.txt';events.write_text('10 key:F1 1\n11 key:F1 0\n40 key:W 1\n120 key:W 0\n150 key:Escape 1\n151 key:Escape 0\n220 key:F2 1\n221 key:F2 0\n280 key:W 1\n360 key:W 0\n400 key:Escape 1\n401 key:Escape 0\n480 key:F3 1\n481 key:F3 0\n540 key:W 1\n600 key:W 0\n640 key:Escape 1\n641 key:Escape 0\n720 key:F4 1\n721 key:F4 0\n')
  run('three-games',[ROOT/'build/judas_consumer_review',consumer,events,out/'three-games','820'],{'JUDAS_REVIEW_CAPTURE_EVERY':'200'},cwd='/tmp')
 else:
  assert args.edited_project,'Export follow-up requires the previously edited project.'
  config=args.edited_project.resolve();assert config.is_file()
 # Export both the changed working copy and the final shipped fixture normally.
 run('edited-export',[cli,'--build-project',config,out/'edited-package',ROOT/'build/judas',ROOT])
 packages=ROOT/'.cache/m67/packages';packages.mkdir(parents=True,exist_ok=True)
 destination=packages/out.name/'WorldWorkshop';destination.parent.mkdir(parents=True,exist_ok=True);assert not destination.exists()
 text=run('final-export',[cli,'--build-project',project,destination,ROOT/'build/judas',ROOT])
 moved=Path('/tmp')/('judas-m67-offline-'+out.name);moved.mkdir(exist_ok=False);package=moved/'WorldWorkshop';shutil.copytree(destination,package)
 hidden=moved/'hidden-output';hidden.mkdir();empty=moved/'empty-source';empty.mkdir();steps=moved/'events.txt';steps.write_text('FRAMES 420\nLOG_EVERY 0\nACTION view_toggle 1 20 21\nACTION spawn 1 40 41\nACTION spawn 1 60 61\nACTION ragdoll 1 90 91\nACTION locale 1 120 121\nACTION pause 1 150 151\nEXPECT_PAUSED 1 160\nSCREENSHOT 180 '+str(hidden/'paused.png')+'\nACTION pause 1 220 221\nEXPECT_PAUSED 0 230\nSCREENSHOT 350 '+str(hidden/'playing.png')+'\n')
 # Private namespaces mask source paths only in this child; other processes/files
 # (including Claude's) remain unaffected. No internet/import tool is available.
 launch="import os,subprocess,sys;subprocess.run(['mount','--bind',sys.argv[1],sys.argv[2]],check=True);os.chdir('/tmp');os.execv(sys.argv[3],[sys.argv[3]])"
 run('source-hidden-moved-runtime',['unshare','--user','--map-root-user','--mount','--net','python3','-c',launch,empty,ROOT.parent,package/'judas'],{'JUDAS_TEST_SCRIPT':str(steps),'JUDAS_PROFILE_OUTPUT':str(hidden/'profile.json'),'XDG_DATA_HOME':str(moved/'user-data'),'XDG_CONFIG_HOME':str(moved/'config')},cwd='/tmp')
 shutil.copytree(hidden,out/'moved-captures')
 # The edited export starts independently with its changed scene/UI/geometry.
 steps2=out/'edited-steps.txt';steps2.write_text('FRAMES 160\nLOG_EVERY 0\nSCREENSHOT 120 '+str(out/'edited-package.png')+'\n')
 run('edited-package-startup',[out/'edited-package/judas'],{'JUDAS_TEST_SCRIPT':str(steps2)},cwd='/tmp')
 summary={'pass':True,'human_gui_clicks':'NOT RUN; shared commands and actual editor drawing/Play-Stop tested','source_project':str(project),'edited_project':str(config),'package':str(destination),'moved_package':str(package),'package_bytes':sum(p.stat().st_size for p in package.rglob('*') if p.is_file()),'asset_count':len(list(package.rglob('*.judasmeta'))),'runtime_sha256':hashlib.sha256((package/'judas').read_bytes()).hexdigest(),'commands':records}
 (out/'RESULTS.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps({k:v for k,v in summary.items() if k!='commands'},indent=2))
if __name__=='__main__':main()
