#!/usr/bin/env python3
"""Ordinary M79 project. The lab applies loads; it never breaks bond connections."""
from pathlib import Path
import ast,json,shutil,subprocess,sys,math
root=Path(__file__).resolve().parents[1];out=root/'projects/structural_lab'
baker=Path(sys.argv[1] if len(sys.argv)>1 else root/'build/judas_deformable_bake')
for d in ('Assets/deformables','Assets/scripts','Assets/materials','Assets/ui','Assets/fonts','Assets/prefabs','Assets/worlds','Scenes'):(out/d).mkdir(parents=True,exist_ok=True)
ids={name:f'7979797979797979797979797979{n:04x}' for n,name in enumerate(('bridge','strong','controller','lab','ui','prefab','world','outer','inner'),1)}
tree=ast.parse((root/'tools/CreateDeformableLab.py').read_text())
exec(compile(ast.Module([n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('v','obj','render','body','script','deform','settings')],type_ignores=[]),'<existing scene format>','exec'),globals())
def meta(path,key,kind):(out/(path+'.judasmeta')).write_text(f'JudasAssetMeta 1\nid "{ids[key]}"\ntype {kind}\nsource ""\n')
for name,strength in [('bridge',120000),('strong',1000000)]:
 path=f'Assets/deformables/{name}.judasdeform'
 subprocess.run([str(baker),'structure',str(out/path),'8','1','1','8','.6','2',str(strength),'150000','1e-8'],check=True);meta(path,name,'deformable')
for name in ('outer','inner'):
 shutil.copyfile(root/f'projects/fracture_lab/Assets/materials/{name}.judasmat',out/f'Assets/materials/{name}.judasmat');meta(f'Assets/materials/{name}.judasmat',name,'material')
for name in ('DejaVuSans.ttf','DejaVuSans.ttf.judasmeta','LICENSE.txt'):shutil.copyfile(root/'projects/fracture_lab/Assets/fonts'/name,out/'Assets/fonts'/name)
for name in ('controller','math'):
 shutil.copyfile(root/f'projects/fracture_lab/Assets/scripts/{name}.js',out/f'Assets/scripts/{name}.js')
p=out/'Assets/scripts/controller.js';p.write_text(p.read_text().replace('pitch:-.2','pitch:.1'))
meta('Assets/scripts/controller.js','controller','script');(out/'Assets/scripts/math.js.judasmeta').write_text('JudasAssetMeta 1\nid "79797979797979797979797979799999"\ntype script\nsource ""\n')
meta('Assets/scripts/lab.js','lab','script');meta('Assets/ui/lab.judasui','ui','ui');meta('Assets/prefabs/bridge.judasprefab','prefab','prefab');meta('Assets/worlds/lab.judasworld','world','world')
ui=(root/'projects/fracture_lab/Assets/ui/lab.judasui').read_text().replace('FRACTURE LAB','STRUCTURAL LAB').replace('fracture-current','structure-current')
lines=ui.splitlines()
for i,line in enumerate(lines):
 if line.startswith('"controls"'):lines[i]=line[:line.index('"WASD')]+json.dumps('WASD/mouse | Click impact | B load | R right support | H middle | P prefab | F3 annex | 1/2 scenes | F5/F8 save/load | F9 reset | Esc')+' "" "be36a149b9918581ce406f3632b79cab"'
(out/'Assets/ui/lab.judasui').write_text('\n'.join(lines)+'\n')
project=(root/'projects/fracture_lab/fracture_lab.judasproj').read_text().replace('Fracture Lab','Structural Lab').replace('fracture-lab','structural-lab').replace('63636363636363636363636363630008',ids['world'])
(out/'structural_lab.judasproj').write_text(project)
def bridge(i,name,p,asset='bridge',middle=False,q=(1,0,0,0)):
 pins=[('left',0,0,'',(0,0,0)),('right',0,0,'',(0,0,0))]
 if middle:pins.append(('middle',0,0,'',(0,0,0)))
 fields=render((4,.3,1),(.22,.64,.75) if asset=='bridge' else (.78,.57,.2))+deform(asset,density=80,attachments=pins)
 result=obj(i,name,p,fields,q)
 return result.replace(json.dumps('1 "62626262626262626262626262629997" 0'),json.dumps('2 "'+ids['outer']+'" 0 "'+ids['inner']+'" 0'))
for radial in (False,True):
 rows=[obj(1,'Actual gravity field',(0,-100,0) if radial else (0,0,0),'  gravity '+('radial' if radial else 'uniform')+' 9.81\n  gravity.region sphere 400\n')]
 rows += [obj(3,'Laboratory floor',(8,-.3,0),render((27,.3,27),(.13,.19,.23))+body((27,.3,27))),obj(10,'Script-controlled visitor',(12,1.1,22),'  motor.enabled true\n  motor.radius .3\n  motor.halfHeight .6\n'+script('controller')+f'  ui.asset "{ids["ui"]}"\n  ui.name "lab"\n  ui.enabled true\n'),obj(11,'Load controls',(0,0,0),script('lab')),obj(12,'View seed',(12,1.1,22),'  player-start first-person\n')]
 for i,(x,asset,middle,name) in enumerate(((0,'bridge',False,'A: two supports / 120 kPa'),(12,'strong',False,'B: same bridge / 1 MPa'),(24,'bridge',True,'C: same strength / middle pier'))):
  rows.append(bridge(100+i*100,name,(x,4,0),asset,middle))
  for side in (-1,1):rows.append(obj(500+i*10+(side+1)//2,'Authored support pedestal',(x+side*3.5,1.7,0),render((.5,1.7,1.1),(.32,.36,.4))+body((.5,1.7,1.1))))
  if middle:rows.append(obj(523,'Middle support pedestal',(x+.5,1.7,0),render((.5,1.7,1.1),(.3,.48,.36))+body((.5,1.7,1.1))))
  rows.append(obj(550+i,'Physical proof load',(x,4.9,0),render((.4,.4,.4),(.85,.35,.12))+body((.4,.4,.4),True,20)+'  tags 1\n'))
 # Oblique independent setup uses an ordinary higher-precedence field, not a
 # structural-world-up assumption. Transform and gravity rotate together.
 q=(math.cos(.4),0,0,math.sin(.4))
 rows.insert(0,obj(5,'Oblique uniform region',(-12,5,-6),'  gravity uniform 9.81\n  gravity.region box 6 8 4\n',q))
 rows.append(bridge(400,'D: rotated bridge',(-12,5,-6),q=q))
 (out/'Scenes'/('radial.judas' if radial else 'flat.judas')).write_text(settings('Structural lab / '+('RadicalGravity' if radial else 'FaithfulGravity'))+'\n'.join(rows))
(out/'Assets/prefabs/bridge.judasprefab').write_text(settings('Independent structure prefab',2)+bridge(1,'Eight-cell bridge',(0,0,0)))
(out/'Scenes/annex.judas').write_text(settings('Streaming structure',3)+bridge(1,'Region structure',(45,4,0))+obj(2,'Region floor',(45,-.3,0),render((6,.3,4),(.25,.3,.34))+body((6,.3,4))))
(out/'Assets/worlds/lab.judasworld').write_text('JudasWorld 1\nbudget 2 8 2 8388608 8388608 8388608 2097152\nregion "annex" "Scenes/annex.judas" 0 0 0 0 0 0 1 50 8 8 1 "snapshot" 262144 ""\n')
print(out)
