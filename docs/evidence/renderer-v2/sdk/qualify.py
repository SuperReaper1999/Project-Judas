from pathlib import Path
import copy,hashlib,json,os,shutil,subprocess,tempfile,time
root=Path('/home/conner/Documents/GitHub/Project-Judas/.cache/renderer-v2/sdk-qualification')
sdk=root.parent/'sdk';work=root/'fresh-project';work.mkdir(exist_ok=True)
rows=[]
env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')}
env.update(SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',PYTHONDONTWRITEBYTECODE='1')
def run(name,args,expected=0,extra=None,cwd=Path('/tmp')):
 e=dict(env);e.update(extra or {});start=time.monotonic()
 with (root/(name+'.log')).open('w') as f:r=subprocess.run([str(x) for x in args],cwd=cwd,env=e,stdout=f,stderr=subprocess.STDOUT,timeout=120)
 rows.append({'name':name,'argv':[str(x) for x in args],'cwd':str(cwd),'exit':r.returncode,'expected':expected,'seconds':time.monotonic()-start})
 (root/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n')
 if r.returncode!=expected:raise RuntimeError(name+' failed: '+str(root/(name+'.log')))
 return (root/(name+'.log')).read_text()
for d in ['Scenes','Assets','Saves']:(work/d).mkdir(exist_ok=True)
project=work/'SDK_Renderer_Qualification.judasproj';scene=work/'Scenes/main.judas'
run('create-project',[sdk/'judas_scene_author','--create','project',project])
run('create-scene',[sdk/'judas_scene_author','--create','scene',scene])
p=json.loads(project.read_text());p['data']['name']='SDK Renderer Qualification';p['data']['saveIdentity']=hashlib.sha256(str(work).encode()).hexdigest();p['data']['startupScene']='Scenes/main.judas';p['data']['input']=[{'name':n,'axis':False,'bindings':[{'control':'key:'+k,'scale':1,'deadzone':0}]}for n,k in [('save','F6'),('load','F7')]]
project.write_text(json.dumps(p,indent=2)+'\n')
s=json.loads(scene.read_text());s['data']['settings']['name']='Packaged SDK fresh scene'
s['data']['settings']['next-id']=11
identity='35b2a3e4f53048fc9e759a99f2a5be6c'
controller={'id':1,'name':'SDK authored save controller','fields':{'position':[0,0,0],'rotation':[1,0,0,0],'scale':[1,1,1],'scripts':1,'script.0.id':1,'script.0.asset':identity,'script.0.enabled':True,'script.0.properties':'{}'}}
reference=json.loads((root/'menu-reference.json').read_text())
floor=copy.deepcopy(next(o for o in reference['data']['objects'] if o['id']==8));floor['id']=8
box=copy.deepcopy(floor);box['id']=9;box['name']='Cold-save MK2 body';box['fields'].update({'position':[0,1,0],'render.half-extents':[.5,.5,.5],'render.color':[.8,.3,.12],'body.half-extents':[.5,.5,.5],'body':{'motion':{'symbol':'dynamic'},'shape':{'symbol':'box'}},'body.mass':2,'body.managed':True})
s['data']['objects']=[controller,floor,box];scene.write_text(json.dumps(s,indent=2)+'\n')
script=work/'Assets/game.js';script.write_text('''import {world,input,saves,console} from 'judas';
export default class {
 constructor(){this.state={gold:37};this.request=0;this.reported=false;}
 start(){console.log('SDK_NEW_GAME gold='+this.state.gold);}
 restore(){if(this.state.gold!==37)throw new Error('cold save lost progress');console.log('SDK_COLD_RESTORE_OK gold='+this.state.gold);this.request=0;}
 update(){
  if(input.pressed('save'))this.request=saves.save('qualification',{name:'Renderer SDK qualification'});
  if(input.pressed('load'))this.request=saves.load('qualification');
  if(this.request){const status=saves.status(this.request);if(status?.state==='failed')throw new Error(status.error);if(status?.state==='completed'&&!this.reported){console.log('SDK_SAVE_REQUEST_COMPLETED gold='+this.state.gold);this.reported=true;}}
 }
 presentationUpdate(){world.setView({position:{x:0,y:2,z:6},rotation:{w:1,x:0,y:0,z:0}},60,{near:.1,far:100});}
}
''')
(script.with_suffix('.js.judasmeta')).write_text('JudasAssetMeta 1\nid "'+identity+'"\ntype script\nsource "Fresh packaged-SDK qualification"\n')
run('validate-project',[sdk/'judas_scene_author','--validate','project',project]);run('validate-scene',[sdk/'judas_scene_author','--validate','scene',scene]);run('script-positive',[sdk/'judas_script_check',project])
original=script.read_text();script.write_text('export default class { update( { invalid');negative=run('script-negative',[sdk/'judas_script_check',project],expected=1);assert 'game.js' in negative;script.write_text(original)
run('script-restored',[sdk/'judas_script_check',project]);run('save-identity',[sdk/'judas_save','identity',project])
export=root/'cli-export';run('export',[sdk/'judas_export',project,export])
relocated=Path(tempfile.mkdtemp(prefix='judas-renderer-v2-sdk-relocated-',dir='/tmp'));moved=relocated/'game';shutil.move(export,moved);assert not export.exists();(relocated/'empty-cwd').mkdir();(root/'RELOCATION.json').write_text(json.dumps({'original':str(export),'relocated':str(moved),'originalRemoved':not export.exists()},indent=2)+'\n')
assert hashlib.sha256((moved/'judas').read_bytes()).digest()==hashlib.sha256((sdk/'judas').read_bytes()).digest()
base={'XDG_DATA_HOME':str(relocated/'data'),'XDG_CONFIG_HOME':str(relocated/'config'),'JUDAS_WORLD_STATE':'none'}
for name,action in [('write','save'),('cold-read','load')]:
 harness=relocated/(name+'.test');harness.write_text('FRAMES 30\nLOG_EVERY 10\nENTITY 9\nACTION '+action+' 1 5 6\nWAIT_SERVICES save 10000 10\nSCREENSHOT 20 '+str(relocated/(name+'.png'))+'\n')
 log=run(name,[moved/'judas',moved/'game.judasproj'],extra={**base,'JUDAS_TEST_SCRIPT':str(harness)},cwd=relocated/'empty-cwd')
 if name=='write':assert 'SDK_SAVE_REQUEST_COMPLETED' in log,log
 else:assert 'SDK_COLD_RESTORE_OK gold=37' in log,log
 assert (relocated/(name+'.png')).is_file()
(root/'GPU_DONE').write_text(str(relocated)+'\n');print('GPU_DONE',relocated,flush=True)
slot=list((relocated/'data').rglob('qualification.judassave'));print('SLOT_FILES',[str(p)for p in (relocated/'data').rglob('*') if p.is_file()],flush=True)
run('sdk-verify-after',['python3',sdk/'tools/package_sdk.py','verify',sdk]);(root/'COMPLETE.json').write_text(json.dumps({'passed':True,'operations':len(rows),'sdk':str(sdk),'relocated':str(moved),'hostBaseline':'local glibc2.43; not Ubuntu24 certification','coldProcessRestoredGold':37,'mk2Body':9,'sourceTreeHidden':False,'releasePackage':False},indent=2)+'\n')
