#!/usr/bin/env python3
"""Original ordinary project content; no historical demo or consumer edits."""
from pathlib import Path
import hashlib,json,shutil,shlex
ROOT=Path(__file__).resolve().parents[1];P=ROOT/'projects/signals_lab';A=P/'Assets'
def asset(path,kind,data=None):
 p=A/path;p.parent.mkdir(parents=True,exist_ok=True)
 if data is not None:p.write_text(data)
 identity=hashlib.md5(('Judas M73 original:'+path).encode()).hexdigest()
 Path(str(p)+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{identity}"\ntype {kind}\nsource ""\n');return identity
counter=asset('scripts/counter.js','script');lab=asset('scripts/lab.js','script')
fontpath=A/'fonts/DejaVuSans.ttf';fontpath.parent.mkdir(parents=True,exist_ok=True)
shutil.copy2(ROOT/'projects/render_control_lab/Assets/fonts/DejaVuSans.ttf',fontpath)
shutil.copy2(ROOT/'projects/render_control_lab/Assets/fonts/LICENSE.txt',fontpath.parent/'LICENSE.txt')
font=asset('fonts/DejaVuSans.ttf','font')
def uirow(id,parent,kind,x,y,w,h,text='',visible=True,flow=0):
 return [id,parent,kind,flow,int(visible),1,0,0,1,0,0,0,0,x,y,w,h,0,0,0,0,0,0,*([0]*8),.04,.08,.13,(.96 if kind in (1,4) else 0),.94,.97,1,1,12,22,0,0,1,text,'',font]
rows=[uirow('canvas','',0,0,0,0,0),uirow('title','canvas',2,24,18,1160,42,'M73 / SIGNALS LAB'),uirow('counts','canvas',2,24,64,1220,55),uirow('stats','canvas',2,24,125,1220,70),uirow('controls','canvas',2,24,620,1240,36,'B broadcast | T left pair | U toggle left | D remove right | P prefab'),uirow('controls2','canvas',2,24,664,1240,36,'L annex | Esc menu | F6/F7 save/load | R reload'),uirow('menu','canvas',1,400,240,480,330,visible=False),uirow('menu_title','menu',2,24,18,430,40,'PAUSED / UI SIGNALS'),uirow('preview','menu',4,24,80,430,60,'UI preview + queue fixed pulse'),uirow('pulse','menu',4,24,150,430,60,'Queue fixed broadcast'),uirow('resume','menu',4,24,225,430,60,'Resume and deliver fixed messages')]
u='JudasUI 1\n1280 720 1 1 0 '+str(len(rows))+'\n'+'\n'.join(' '.join(json.dumps(v) if isinstance(v,str) else str(v) for v in row) for row in rows)+'\n'
uiid=asset('ui/lab.judasui','ui',u)
def obj(id,name,pos,extra=''):
 if ' render ' in extra:extra+=' render.radius .75\n render.alpha 1\n render.secondary-color 1 1 1\n render.secondary-alpha 1\n render.mesh-asset ""\n render.texture-asset ""\n'
 return f'object {id} "{name}"\n position {pos}\n rotation 1 0 0 0\n scale 1 1 1\n'+extra+'end\n\n'
def slots(labels):
 out=f' scripts {len(labels)}\n'
 for i,label in enumerate(labels):out+=f' script.{i}.id {i+1}\n script.{i}.asset "{counter}"\n script.{i}.enabled true\n script.{i}.properties '+json.dumps(json.dumps({'label':label}))+'\n'
 return out
render=' render box\n render.half-extents .7 1 .7\n render.color .18 .4 .7\n'
header='JudasScene 3\nsettings\n name "Signals Lab"\n world-origin 0 0 0\n sun-direction .4 .8 1\n sun-color 1.1 1.1 1.1\n ambient .2 .2 .2\n fluid-scale 1\n fidelity-policy none\n next-id 100\nend\n\n'
prefab=asset('prefabs/receiver.judasprefab','prefab',header+obj(1,'Independent receiver','0 1 0',render+slots(['Prefab'])))
scene=header+obj(1,'Ground','0 -.4 0',' render box\n render.half-extents 9 .4 7\n render.color .12 .15 .21\n')+obj(20,'Left indicators','-2 1 0',render+slots(['Left A','Left B']))+obj(30,'Right indicator','2 1 0',render+slots(['Right']))
scene+=obj(90,'Publisher and UI','0 0 0',f' ui.asset "{uiid}"\n ui.name "signals_ui"\n ui.enabled true\n scripts 1\n script.0.id 1\n script.0.asset "{lab}"\n script.0.enabled true\n script.0.properties '+json.dumps(json.dumps({'prefab':prefab}))+'\n')
(P/'Scenes').mkdir(exist_ok=True);(P/'Scenes/lab.judas').write_text(scene);(P/'Scenes/annex.judas').write_text(header+obj(10,'Annex receiver','-4 1 -3',render+slots(['Annex adopted']))+obj(11,'Annex revisited receiver','-6 1 -3',render+slots(['Annex revisited'])))
manifest=asset('world/lab.judasworld','world','JudasWorld 1\nbudget 2 8 2 8388608 8388608 8388608 2097152\nregion "annex" "Scenes/annex.judas" 0 0 0 0 0 0 1 40 5 5 1 "snapshot" 262144 ""\n')
actions={'broadcast':'B','target':'T','toggle':'U','delete_receiver':'D','spawn_receiver':'P','region':'L','pause':'Escape','reset':'R','save':'F6','load':'F7','ui_activate':'Return','ui_up':'Up','ui_down':'Down','ui_click':None}
inputmap='1 '+str(len(actions))+' '+' '.join(json.dumps(k)+' 0 1 '+json.dumps('mouse:Left' if v is None else 'key:'+v)+' 1 0' for k,v in actions.items())
project='JudasProject 1\nname "Signals Lab"\nstartup-scene "Scenes/lab.judas"\nassets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\nlegacy-gameplay "false"\ninput-map '+json.dumps(inputmap)+'\nworld-manifest '+json.dumps(manifest)+'\nsave-identity "c8c01f780430553f0a099f259cb556e7998aad815f1731efce6bf7c170105c17"\nruntime-assets '+json.dumps(' '.join(json.dumps(i) for i in [prefab,counter,lab,uiid,font,manifest]))+'\n'
(P/'signals_lab.judasproj').write_text(project)
print(P)
