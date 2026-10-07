#!/usr/bin/env python3
"""Assemble ordinary demo content with the PUBLIC import CLI. No DCC conversion.
Original third-party assets remain local and excluded from version control.
"""
from pathlib import Path
import json,hashlib,shutil,subprocess,sys,re
ROOT=Path(__file__).resolve().parents[1];P=ROOT/'projects/import_lab';A=P/'Assets'
CLI=Path(sys.argv[1] if len(sys.argv)>1 else ROOT/'build/judas_model_import_cli')
ORIGINAL=Path(sys.argv[2] if len(sys.argv)>2 else '/home/conner/Documents/GitHub/ClaudeJudasSkateGame/Assets')
def key(path):return hashlib.md5(('M66 import lab:'+path).encode()).hexdigest()
def asset(path,kind,text=None):
 p=A/path;p.parent.mkdir(parents=True,exist_ok=True)
 if text is not None:p.write_text(text)
 Path(str(p)+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{key(path)}"\ntype {kind}\nsource ""\n');return key(path)
def run(*args):
 r=subprocess.run([str(CLI),*map(str,args)],capture_output=True,text=True)
 if r.returncode not in (0,2):raise RuntimeError(r.stdout+r.stderr)
 return json.loads(r.stdout)
def cook(name,source,motions=None,clips=None,owned=False):
 folder=P/'Sources'/('original' if owned else name);folder.mkdir(parents=True,exist_ok=True);
 if source.resolve()!=(folder/source.name).resolve():shutil.copy2(source,folder/source.name)
 if name=='external':
  for f in ['geometry.bin','café color.png']:shutil.copy2(source.parent/f,folder/f)
 output='Assets/models/'+name+'.judasmodel';recipe={'format':'JudasImport','version':1,'assetId':key('models/'+name+'.judasmodel'),'source':str((folder/source.name).relative_to(P)),'output':output,'settings':{'unitMeters':0,'sampleRate':60,'allowBaseMesh':False,'dependencyRemaps':{}},'motions':motions or []}
 if clips is not None:recipe['clips']=clips
 rp=P/'Imports'/(name+'.judasimport');rp.parent.mkdir(parents=True,exist_ok=True);rp.write_text(json.dumps(recipe,indent=2)+'\n');run('--recipe',rp);return key('models/'+name+'.judasmodel')
P.mkdir(parents=True,exist_ok=True);(P/'Scenes').mkdir(exist_ok=True)
(P/'.gitignore').write_text('Sources/original/\nSources/artist-halfpipe/\nAssets/models/skate_original.judasmodel*\nAssets/models/halfpipe.judasmodel*\n.cache/\nSaves/\n')
owned=P/'Sources/original';owned.mkdir(parents=True,exist_ok=True)
motions=[]
for filename,name in [('SkateboardingPush.fbx','PushSource'),('SkateboardingCruise.fbx','CruiseSource'),('Walking.fbx','WalkingSource')]:
 shutil.copy2(ORIGINAL/filename,owned/filename);motions.append({'source':'Sources/original/'+filename,'take':'mixamo.com','name':name,'jointRemaps':{}})
clips=[{'sourceClip':name+'Source','name':name,'loop':True,'rootMotion':{'policy':'preserve'}} for name in ['Push','Cruise','Walking']]
clips +=[{'sourceClip':'WalkingSource','name':'WalkingExtract','loop':True,'rootMotion':{'policy':'extract','node':'__root/mixamorig:Hips','translation':[True,False,True]}},{'sourceClip':'PushSource','name':'PushInPlace','loop':True,'rootMotion':{'policy':'inPlace','node':'__root/mixamorig:Hips','translation':[True,False,True]}}]
model=cook('skate_original',ORIGINAL/'Skateboarder.fbx',motions,clips,True)
ramp=cook('clean-ramp',P/'Sources/clean-ramp/ramp.obj') if (P/'Sources/clean-ramp/ramp.obj').exists() else None
halfpipe=None
artist=ROOT/'.cache/m66/halfpipe'
if (artist/'Skate Ramp Texture.fbx').exists():
 folder=P/'Sources/artist-halfpipe';folder.mkdir(parents=True,exist_ok=True)
 for source in artist.iterdir():
  if source.is_file():shutil.copy2(source,folder/source.name)
 recipe={'format':'JudasImport','version':1,'assetId':key('models/halfpipe.judasmodel'),'source':'Sources/artist-halfpipe/Skate Ramp Texture.fbx','output':'Assets/models/halfpipe.judasmodel','settings':{'unitMeters':0,'sampleRate':60,'allowBaseMesh':False,'dependencyRemaps':{}},'motions':[]}
 rp=P/'Imports/halfpipe.judasimport';rp.write_text(json.dumps(recipe,indent=2)+'\n');run('--recipe',rp);halfpipe=recipe['assetId']
capacity=cook('capacity256',ROOT/'tests/fixtures/m66/capacity256.gltf');multipart=cook('multipart',ROOT/'tests/fixtures/m66/multipart.gltf');external=cook('external',ROOT/'tests/fixtures/m66/external.gltf')
font=asset('fonts/DejaVuSans.ttf','font');shutil.copy2(ROOT/'projects/m65_integration/Assets/fonts/DejaVuSans.ttf',A/'fonts/DejaVuSans.ttf');shutil.copy2(ROOT/'projects/m65_integration/Assets/fonts/LICENSE.txt',A/'fonts/LICENSE.txt')
uiText=(ROOT/'projects/pose_demo/Assets/ui/game.judasui').read_text() if (ROOT/'projects/pose_demo/Assets/ui/game.judasui').exists() else next((ROOT/'projects/pose_demo/Assets/ui').glob('*.judasui')).read_text()
uiText=uiText.replace('41414141414141414141414141414103',font).replace('"37373737373737373737373737373737"','""');hud=asset('ui/lab.judasui','ui',uiText)
script=asset('scripts/lab.js','script');camera=asset('scripts/camera.js','script')
# Reuse the existing current project's logical bindings, never poll raw keys.
project=(ROOT/'projects/animation_demo/animation_demo.judasproj').read_text();project=project.replace('name "Skeletal animation"','name "M66 Import Lab"')+'legacy-gameplay "false"\n'
line=next(x for x in project.splitlines() if x.startswith('input-map '));decoded=json.loads(line[len('input-map '):]);decoded=decoded.replace('1 52 ','1 58 ',1)+' "ragdoll_primary" 0 1 "key:F" 1 0 "ik" 0 1 "key:I" 1 0 "root_intent" 0 1 "key:B" 1 0 "part" 0 1 "key:T" 1 0 "stream" 0 1 "key:U" 1 0 "reload_scene" 0 1 "key:F5" 1 0 '
project=project.replace(line,'input-map '+json.dumps(decoded));old_registry=next(x for x in (ROOT/'projects/post_m65_consumers/post_m65_consumers.judasproj').read_text().splitlines() if x.startswith('classification '));project=re.sub(r'^classification .*$',lambda _:old_registry,project,flags=re.M);(P/'import_lab.judasproj').write_text(project)
header='''JudasScene 3
settings
 name "M66 original rig — G clips, J mask, K additive, I IK, F ragdoll, B root intent"
 world-origin 0 0 0
 sun-direction .5 1 .4
 sun-color 1 .97 .91
 ambient .4 .4 .45
 fluid-scale 1
 fluid-update-rate-hz 30
 fluid-hydrostatic-drag-rate 2
 fidelity-policy none
 linear-rendering true
 exposure 1
 next-id 1000
end
'''
def obj(i,name,pos,fields,rot=(1,0,0,0)):
 if ' render ' in fields:
  for k,v in {'render.radius':'1','render.secondary-color':'1 1 1','render.secondary-alpha':'1','render.mesh-asset':'""','render.texture-asset':'""','render.half-extents':'1 1 1'}.items():
   if k not in fields:fields+='\n '+k+' '+v
 if 'motor.enabled' in fields:
  for k,v in {'reorientationDegreesPerSecond':'360','interactionMass':'80','maxPushImpulse':'15','collisionLayer':'0','collisionMask':'4294967295','requiredTags':'0','excludedTags':'0'}.items():fields+='\n motor.'+k+' '+v
 return f'object {i} "{name}"\n position '+ ' '.join(map(str,pos))+'\n rotation '+' '.join(map(str,rot))+'\n scale 1 1 1\n'+fields+'\nend\n'
def render_mesh(asset):return f' render mesh\n render.half-extents 1 1 1\n render.radius 1\n render.color 1 1 1\n render.alpha 1\n render.mesh-asset "{asset}"\n render.texture-asset ""'
def box(half,color,body=True):
 fields=' render box\n render.half-extents '+' '.join(map(str,half))+'\n render.color '+' '.join(map(str,color))+'\n render.alpha 1'
 if body:fields+='\n body static box\n body.half-extents '+' '.join(map(str,half))+'\n body.radius .5\n body.terrain ""\n body.mass 1\n body.friction .6\n body.restitution .1\n body.initial-velocity 0 0 0\n body.pickable false\n body.managed false\n body.compound-count 0\n body.fluid-cavity-count 0'
 return fields
def animation(clip):return f'\n animation.enabled true\n animation.play-on-start true\n animation.loop true\n animation.clip "{clip}"\n animation.speed 1\n animation.time 0'
# The accepted physical subset is content, retained intact; new full rig retains
# all other joints rather than creating one body per bone.
old=(ROOT/'projects/post_m65_consumers/Scenes/skate/park.judas').read_text();rig=re.search(r'object 12 .*?\nend',old,re.S).group();ragdoll='\n'+'\n'.join(x for x in rig.splitlines() if x.strip().startswith('ragdoll.'))
scene=header+obj(1,'Floor',(0,-.25,0),box((15,.25,15),(.2,.25,.28)))+obj(2,'Generic gravity',(0,0,0),' gravity uniform 9.81\n gravity.region box 100 100 100')
scene+=obj(10,'Original full rig',(-2,0,0),render_mesh(model)+animation('Push')+ragdoll)
scene+=obj(11,'Independent oblique instance',(2,.4,0),render_mesh(model)+animation('Cruise')+ragdoll, (.965925826,0,0,.258819045))
scene+=obj(12,'Extracted track drives separate motor',(0,0,-5),render_mesh(model)+animation('WalkingExtract')+'\n motor.enabled true\n motor.radius .3\n motor.halfHeight .6\n motor.offset 0 .9 0\n motor.gravityScale 1\n motor.stepHeight .3\n motor.supportDistance .12\n motor.skin .005\n motor.maxSlopeDegrees 50')
scene+=obj(13,'Collision wins over root motion',(0,1,-2),box((1,1,.12),(.65,.3,.15)))
scene+=obj(20,'256 animated joints',(-5,.2,-5),render_mesh(capacity)+animation('Translate'))
scene+=obj(21,'Multiple skins and affine attachment',(5,0,-5),render_mesh(multipart)+animation('Translate'))
scene+=obj(22,'External UV1 material',(5,0,-8),render_mesh(external)+animation('Translate'))
scene+=obj(30,'Resolved hand socket',(-2,0,0),' render box\n render.half-extents .08 .08 .08\n render.color 1 .65 .1\n render.alpha 1\n socket.target 10\n socket.joint "mixamorig:RightHand"\n socket.enabled true\n socket.position 0 0 0\n socket.rotation 1 0 0 0\n socket.scale 1 1 1')
scene+=obj(40,'Secondary camera',(0,3,7),' render-camera\n camera.enabled true\n camera.cadence 1\n camera.width 320\n camera.height 240\n camera.near .05\n camera.far 100\n camera.fov 60')
scene+=obj(41,'Camera display',(5,2,2),' render box\n render.half-extents 1.3 .9 .04\n render.color 1 1 1\n render.alpha 1\n render.texture-camera 40')
if ramp:
 collision=A/'collision/clean-ramp.judascollision';collision.parent.mkdir(parents=True,exist_ok=True);run('--collision',A/'models/clean-ramp.judasmodel',0,collision);collisionId=asset('collision/clean-ramp.judascollision','collision')
 scene+=obj(50,'Separate clean concave physical reference',(-8,0,-8),render_mesh(ramp)+'\n body static triangle-mesh\n body.half-extents 1 1 1\n body.radius .5\n body.terrain ""\n body.collision-asset "'+collisionId+'"\n body.mass 1\n body.friction .6\n body.restitution .1\n body.initial-velocity 0 0 0\n body.pickable false\n body.managed false\n body.compound-count 0\n body.fluid-cavity-count 0')
if halfpipe:scene+=obj(51,'Original artist halfpipe — visual import, optional collision diagnostic',(-8,0,5),render_mesh(halfpipe))
scene+=obj(90,'Lab controls' ,(0,0,0),f' ui.asset "{hud}"\n ui.name "game_ui"\n ui.enabled true\n scripts 1\n script.0.id 1\n script.0.asset "{script}"\n script.0.enabled true\n script.0.properties "{{}}"')
scene+=obj(91,'Script camera',(0,2,8),f' scripts 1\n script.0.id 1\n script.0.asset "{camera}"\n script.0.enabled true\n script.0.properties "{{}}"')
(P/'Scenes/main.judas').write_text(scene)
region=header+obj(1,'Streamed original rig',(0,0,0),render_mesh(model)+animation('Cruise')+ragdoll)
(P/'Scenes/region.judas').write_text(region)
manifest=asset('world/lab.judasworld','world','JudasWorld 1\nbudget 2 8 2 536870912 536870912 16777216 536870912\nregion "rig" "Scenes/region.judas" 0 0 -10 0 0 0 1 8 8 8 1 "snapshot" 262144 ""\n')
project+='world-manifest "'+manifest+'"\n';(P/'import_lab.judasproj').write_text(project)
# Ordinary runtime prefab shares the asset and authored physical subset.
prefab=asset('prefabs/full_rig.judasprefab','prefab',header+obj(1,'Original rig instance',(0,0,0),render_mesh(model)+animation('Walking')+ragdoll))
(A/'scripts/lab.js').write_text(f'''import {{input,ui,world,scenes,saves}} from 'judas';
const PREFAB='{prefab}';
export default class {{
 constructor(){{this.state={{spawns:0,clip:0,ik:false,root:false,part:true,mask:false,add:false}};}}
 start(){{this.hud=ui.get('game_ui');this.hud.modal=false;ui.debugOverlayVisible=false;for(const id of ['main','options','pause','image','pause_options'])this.hud.get(id).visible=false;this.hud.get('hud').visible=true;this.hud.get('hud_title').text='M66 ORIGINAL RIG / NORMAL RUNTIME';this.hud.get('hud_help').text='G clips / J mask / K additive / I IK / F ragdoll\\nT part / P spawn / B root motor / F5 reload / U stream / F6 save / F7 load\\nMouse drag/look, WASD camera / Esc pause';this.a=world.entity('10');this.b=world.entity('11');this.motor=world.entity('12');}}
 restore(){{this.start();}}
 update(){{if(!this.a.animation.info.ready)return;
  if(input.pressed('interact')){{const names=['Push','Cruise','Walking','PushInPlace'];this.a.animation.crossFade(names[++this.state.clip%names.length],.65);}}
  if(input.pressed('pulse')){{this.state.mask=!this.state.mask;this.a.animation.layer('arm',{{clip:'Walking',mask:['mixamorig:RightArm','mixamorig:RightForeArm','mixamorig:RightHand'],weight:.7,enabled:this.state.mask}});}}
  if(input.pressed('collect_key')){{this.state.add=!this.state.add;this.a.animation.layer('offset',{{clip:'Push',referenceClip:'Push',referenceTime:0,additive:true,mask:['mixamorig:Spine'],weight:.25,enabled:this.state.add}});}}
  if(input.pressed('ik')){{this.state.ik=!this.state.ik;this.a.animation.limb('hand',{{root:'mixamorig:RightArm',middle:'mixamorig:RightForeArm',end:'mixamorig:RightHand',target:{{x:.45,y:1.2,z:.45}},pole:{{x:1,y:1,z:0}},weight:.65,enabled:this.state.ik}});}}
  if(input.pressed('ragdoll_primary')){{if(this.a.ragdoll.active)this.a.ragdoll.leave(.7);else this.a.ragdoll.enter();}}
  if(input.pressed('part')){{this.state.part=!this.state.part;const part=this.a.modelParts[0];this.a.setPartVisible(part.identity,this.state.part);}}
  if(input.pressed('spawn_prefab')){{world.spawnPrefab(PREFAB,{{position:{{x:-4+(this.state.spawns++%5)*2,y:2,z:-3}}}});}}
  if(input.pressed('root_intent'))this.state.root=!this.state.root;
  if(input.pressed('reload_scene'))scenes.reload();
  if(input.pressed('save_state'))saves.save('import-lab');
  if(input.pressed('delete_state'))saves.load('import-lab');
  if(input.pressed('stream')){{if(this.region){{scenes.releaseRegion(this.region);scenes.unloadRegion('rig');this.region=null;}}else this.region=scenes.requestRegion('rig');}}
  this.hud.get('counter').text=`65 source bones / 6 parts / independent instances\n${{this.a.animation.info.clip}} ${{this.a.animation.time.toFixed(2)}}s / ragdoll ${{this.a.ragdoll.active}}\nIK ${{this.state.ik}} / root motor ${{this.state.root}} / spawned ${{this.state.spawns}}`;
 }}
 fixedUpdate(dt){{if(!this.state.root||!this.motor.animation.info.ready)return;const t=this.motor.animation.time,m=this.motor.animation.rootMotion('WalkingExtract',t,t+dt,true);const v={{x:m.translation.x/dt,y:this.motor.character.velocity.y,z:m.translation.z/dt}};this.motor.character.velocity=v;}}
 uiUpdate(){{this.pauseEdge=input.pressed('pause');if(this.pauseEdge){{this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;}}}}
 onUI(e){{if(e.type==='back'&&!this.pauseEdge||e.type==='click'&&e.element==='resume'){{this.hud.modal=false;this.hud.get('pause').visible=false;}}if(e.type==='click'&&e.element==='pause_quit')ui.quit();}}
}}
''')
(A/'scripts/camera.js').write_text('''import {world,input} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={yaw:0,pitch:0};}
 start(){input.pointerCapture=true;}
 restore(){input.pointerCapture=true;}
 update(dt){this.state.yaw-=input.axis('look_x')*.002;this.state.pitch=Math.max(-1.2,Math.min(1.2,this.state.pitch-input.axis('look_y')*.002));const q={w:Math.cos(this.state.yaw/2)*Math.cos(this.state.pitch/2),x:Math.cos(this.state.yaw/2)*Math.sin(this.state.pitch/2),y:Math.sin(this.state.yaw/2)*Math.cos(this.state.pitch/2),z:-Math.sin(this.state.yaw/2)*Math.sin(this.state.pitch/2)};const p=this.entity.transform.position,x=input.axis('move_x')*dt*4,z=-input.axis('move_y')*dt*4;this.entity.transform={position:{x:p.x+Math.cos(this.state.yaw)*x+Math.sin(this.state.yaw)*z,y:p.y,z:p.z-Math.sin(this.state.yaw)*x+Math.cos(this.state.yaw)*z}};world.setView({position:this.entity.presentedTransform.position,rotation:q},60);}
 destroy(){input.pointerCapture=false;world.clearView();}
}
''')
print('Created ordinary import lab:',P)
