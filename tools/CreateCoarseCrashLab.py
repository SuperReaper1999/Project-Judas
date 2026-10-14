#!/usr/bin/env python3
"""M83 current content: real coarse material partitions, wheels and transmission.
The rejected node laboratory remains a separate preserved research project.
"""
from pathlib import Path
import argparse,json,math,shutil,subprocess
from types import SimpleNamespace
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'projects/coarse_crash_lab'
IDS={'controller': '83838383838383838383c83d00000001', 'math': '83838383838383838383c83d00000002', 'ui': '83838383838383838383c83d00000003', 'font': '83838383838383838383c83d00000004', 'nominal': '83838383838383838383c83d00000005', 'strong': '83838383838383838383c83d00000006', 'weak': '83838383838383838383c83d00000007', 'specimen': '83838383838383838383c83d00000008', 'car': '83838383838383838383c83d00000009', 'specimen_prefab': '83838383838383838383c83d0000000a', 'cabin': '83838383838383838383c83d0000000b', 'wheel': '83838383838383838383c83d0000000c', 'paint': '83838383838383838383c83d0000000d', 'metal': '83838383838383838383c83d0000000e', 'world': '83838383838383838383c83d0000000f'}

def vec(value):
    return ' '.join(f'{number:.10g}' for number in value)

def rotation(axis, angle):
    return (math.cos(angle/2), *(number*math.sin(angle/2) for number in axis))

def rotate(q, v):
    w, x, y, z = q
    cross = (y*v[2]-z*v[1], z*v[0]-x*v[2], x*v[1]-y*v[0])
    cross2 = (y*cross[2]-z*cross[1], z*cross[0]-x*cross[2], x*cross[1]-y*cross[0])
    return tuple(v[i]+2*(w*cross[i]+cross2[i]) for i in range(3))

def settings(name, next_id=10000):
    return ('JudasScene 3\nsettings\n  name '+json.dumps(name)+'\n'
            '  world-origin 0 0 0\n  sun-color 1 .97 .91\n'
            '  sun-direction -.3 .8 .5\n  ambient .34 .37 .40\n'
            f'  fluid-scale 1\n  fidelity-policy none\n  next-id {next_id}\nend\n')

def object_(identity, name, position, fields='', q=(1, 0, 0, 0), parent=0):
    # Version-3 component readers require the complete base component record,
    # even where a box does not use a sphere radius or a mesh asset. Keep these
    # emitted defaults aligned with SceneSerialization's registered grammar.
    authored = {line.strip().split()[0] for line in fields.splitlines() if line.strip()}
    defaults = {}
    if 'render' in authored:
        defaults.update({'render.half-extents': '.5 .5 .5', 'render.radius': '.5',
                         'render.color': '.8 .8 .8', 'render.alpha': '1',
                         'render.secondary-color': '.8 .8 .8', 'render.secondary-alpha': '1',
                         'render.mesh-asset': '""', 'render.texture-asset': '""'})
    if 'body' in authored:
        defaults.update({'body.half-extents': '.5 .5 .5', 'body.radius': '.5',
                         'body.terrain': '""', 'body.mass': '1', 'body.friction': '.7',
                         'body.restitution': '0', 'body.initial-velocity': '0 0 0',
                         'body.pickable': 'false', 'body.managed': 'false',
                         'body.compound-count': '0'})
    if 'player-start' in authored:
        defaults['player-start.yaw'] = '0'
    if 'wheel.enabled' in authored:
        defaults.update({'wheel.requiredTags': '0', 'wheel.excludedTags': '0'})
    fields += ''.join(f'  {key} {value}\n' for key, value in defaults.items() if key not in authored)
    return f'object {identity} {json.dumps(name)}\n'+(f'  parent {parent}\n' if parent else '')+\
        f'  position {vec(position)}\n  rotation {vec(q)}\n  scale 1 1 1\n'+fields+'end\n'

def render(half, color):
    return f'  render box\n  render.half-extents {vec(half)}\n  render.color {vec(color)}\n'

def body(half, mass=0, layer=0, mask=7):
    return f'  body {"dynamic" if mass else "static"} box\n  body.half-extents {vec(half)}\n'+\
        f'  body.mass {mass or 1}\n  body.friction .7\n  body.restitution 0\n'+\
        f'  body.collision-layer {layer}\n  body.collision-mask {mask}\n'

def compound(parts, mass, layer=1):
    return ('  body dynamic compound\n'+f'  body.mass {mass}\n  body.friction .7\n  body.restitution 0\n'
            f'  body.collision-layer {layer}\n  body.collision-mask 7\n'
            f'  body.compound-count {len(parts)}\n'+''.join(
                '  body.compound-box '+vec(center)+' '+vec(half)+'\n' for _, center, half in parts))

def metadata(relative, name, kind):
    (OUT/(relative+'.judasmeta')).write_text(
        f'JudasAssetMeta 1\nid "{IDS[name]}"\ntype {kind}\nsource ""\n')

def wheel(chassis, mount):
    values = {'enabled': 'true', 'chassis': str(chassis), 'mount': vec(mount),
              'orientation': '1 0 0 0', 'radius': '.32', 'minLength': '.10',
              'restLength': '.35', 'maxLength': '.53', 'springStiffness': '26000',
              'damping': '1900', 'spinInertia': '.8', 'friction': '1',
              'rollingResistance': '.012', 'collisionLayer': '1', 'collisionMask': '1'}
    return ''.join(f'  wheel.{name} {value}\n' for name, value in values.items())

def script(case):
    return ('  scripts 1\n  script.0.id 1\n'+f'  script.0.asset "{IDS["controller"]}"\n'
            '  script.0.enabled true\n  script.0.properties '+
            json.dumps(json.dumps({'case': case}))+'\n'+f'  ui.asset "{IDS["ui"]}"\n  ui.name "lab"\n  ui.enabled true\n')

f=SimpleNamespace(ROOT=ROOT,OUT=OUT,IDS=IDS,body=body,metadata=metadata,rotation=rotation,render=render,script=script,rotate=rotate,object_=object_,compound=compound,wheel=wheel,vec=vec,settings=settings)

def material(variant,root):
 values={'asset':f.IDS[variant],'enabled':1,'selfContact':1,'substeps':2,'iterations':12,
 'collisionLayer':1,'collisionMask':7,'material.density':240/(1.6*.56*2.4),'material.stretchCompliance':1e-7,
 'material.shearCompliance':1e-7,'material.bendCompliance':1e-5,'material.volumeCompliance':1e-8,
 'material.damping':0,'material.thickness':.025,'material.friction':.6,'material.airDrag':0,
 'material.yieldStrain':0,'material.plasticRate':0,'material.maximumPlasticStrain':0,'airVelocity':'0 0 0'}
 fields=''.join(f'  deformable.{k} {json.dumps(str(v))}\n' for k,v in values.items())
 fields+='  deformable.attachments '+json.dumps(f'2 "part-2" 1 {root} "" 0 0 -.6 1 "part-3" 1 {root} "" 0 0 -.6 1')+'\n'
 return fields

def train(root):
 graph={'version':1,'enabled':True,'shafts':[{'name':'motor','kind':'internal','inertia':.4,'initialAngularVelocity':0,'viscousDrag':.01,'target':'0','bearing':str(root),'axis':[1,0,0]}],
 'sources':[{'name':'motor','shaft':0,'kind':'electric','torqueCurve':[],'maxTorque':220,'maxPower':75000,'starterTorque':0}],'links':[],'brakes':[]}
 for i in range(4):
  graph['shafts'].append({'name':f'wheel{i+1}','kind':'wheel','inertia':.2,'initialAngularVelocity':0,'viscousDrag':0,'target':str(root+i+1),'bearing':'0','axis':[1,0,0]})
  link={'name':f'drive{i+1}','kind':'clutch','input':0,'output':i+1,'outputB':0,'ratio':.25,'maxTorque':350}
  if i<2:link['connection']={'owner':str(root+20),'attachment':f'part-{i}','rootAttachment':f'part-{i+2}','groups':True}
  graph['links'].append(link);graph['brakes'].append({'name':f'wheel{i+1}','shaft':i+1,'maxTorque':500})
 return '  powertrain.graph '+json.dumps(json.dumps(graph,separators=(',',':')))+'\n'

def car(root,pos,variant='nominal',q=(1,0,0,0)):
 # Supported hollow roof and pillars: these collision boxes have their own
 # explicit50kg; they do not duplicate the526kg six-section lower body.
 shell=[('rear floor',(0,0,1.2),(.8,.28,.6)),('roof',(0,.77,.52),(.75,.035,.98)),('rear rail',(0,.20,1.72),(.75,.20,.06)),
 ('left A',(-.74,.55,-.34),(.035,.25,.035)),('right A',(.74,.55,-.34),(.035,.25,.035)),

 ('left C',(-.74,.55,1.42),(.035,.25,.035)),('right C',(.74,.55,1.42),(.035,.25,.035))]
 color={'nominal':(.95,.35,.12),'strong':(.3,.75,.46),'weak':(.75,.28,.52)}[variant]
 result=f.object_(root,'Supported hollow cabin',pos,f.compound(shell,336)+train(root),q)
 for i,(name,p,h) in enumerate(shell):result+=f.object_(root+100+i,name,p,f.render(h,color),parent=root)
 for i,(p,h) in enumerate([((0,.48,-.35),(.68,.24,.012)),((0,.48,1.44),(.68,.24,.012)),((-.76,.48,.55),(.01,.24,.83)),((.76,.48,.55),(.01,.24,.83))]):
  result+=f.object_(root+120+i,'Cabin glass',p,f.render(h,(.05,.23,.30)),parent=root)
 result+=f.object_(root+20,'Four physical crush sections',(0,0,-.6),material(variant,root)+f.render((.8,.28,1.8),color),parent=root)
 for i,(side,z) in enumerate([(-1,-1.20),(1,-1.20),(-1,1.20),(1,1.20)]):
  mount=(side*.43,.12,0) if i<2 else (side*.83,.12,z)
  chassis=root+20 if i<2 else root
  wheel=f.wheel(chassis,mount)
  if i<2:wheel+=f'  wheel.part "part-{i}"\n'
  result+=f.object_(root+1+i,'Tyre', (side*.83,-.20,z),f'  render mesh\n  render.mesh-asset "{f.IDS["wheel"]}"\n  render.color .055 .06 .075\n'+wheel,parent=root)
 return result

def script(case):return f.script(case)
def scene(case):
 q=f.rotation((1/math.sqrt(14),2/math.sqrt(14),3/math.sqrt(14)),.7) if case=='rotated' else (1,0,0,0)
 result=f.settings('Physical section crash / '+case)+f.object_(1,'Gravity frame',(0,0,0),'  gravity uniform 9.81\n  gravity.region sphere 200\n',q)
 result+=f.object_(3,'Ground',(0,-.15,0),f.render((40,.15,150),(.11,.16,.20))+f.body((40,.15,150)),parent=1)
 if case!='normal':
  half=(.4,1.3,.3) if case=='offset' else (20,1.3,.3)
  result+=f.object_(5000,'Impact wall',(.57 if case=='offset' else 0,1.3,-.3),f.render(half,(.40,.47,.51))+f.body(half),parent=1)
 else:result+=f.object_(5000,'Uneven road',(0,.04,-8),f.render((3,.04,.25),(.35,.42,.47))+f.body((3,.04,.25)),parent=1)
 result+=f.object_(10,'Inputs and public JS',(0,0,0),script(case))
 count=4 if case.startswith('group') else 3 if case=='comparison' else 1
 for i in range(count):
  pos=f.rotate(q,((i-(count-1)/2)*4.5,.67,90))
  result+=car(900+300*i,pos,('weak','nominal','strong')[i] if case=='comparison' else 'nominal',q)
 result+=f.object_(60,'Observer start',f.rotate(q,(7,3,96)),'  player-start first-person\n',q)
 return result

def specimen_source():
 rows=['JudasFractureSource 1','settings 1 1000000 1000000 0 1e-8 256','neighbour-filter 1']
 centers=[(0,0,-.6),(0,0,0),(0,0,.6)];half=(.15,.15,.3)
 for part,c in enumerate(centers):
  for n in range(8):rows.append('node '+f.vec(tuple(c[a]+(1 if n&(1<<a) else -1)*half[a] for a in range(3))))
  for tet in [(0,1,3,7),(0,3,2,7),(0,2,6,7),(0,6,4,7),(0,4,5,7),(0,5,1,7)]:rows.append('tet '+' '.join(str(part*8+n) for n in tet))
  rows.append(f'part "part-{part}" '+f.vec(c)+' '+f.vec(half)+' 8 '+' '.join(str(part*8+n) for n in range(8))+' 6 '+' '.join(str(part*6+n) for n in range(6)))
  rows.append(f'group "part-{part}" 8 '+' '.join(str(part*8+n) for n in range(8)))
 for part in range(2):
  rows.append(f'bond "interface-{part}" {part} {part+1} 0 0 1 0 0 {-.3+.6*part:.10g} .09 4 '+' '.join(f'{part*8+n+4} {(part+1)*8+n}' for n in range(4)))
  rows.append(f'material "interface-{part}" 20000 5000 200 40 800 80 1000 100 .3 .6')
 rows.append('end');return '\n'.join(rows)+'\n'

def specimen_scene():
 rows=f.settings('Generic elastic/plastic material cantilever')+f.object_(1,'Zero gravity specimen frame',(0,0,0),'  gravity uniform 0\n  gravity.region sphere 200\n')+f.object_(10,'Public force controls',(0,0,0),script('specimen'))
 shape=f.compound([('clamp',(0,0,.6),(.3,.3,.25))],1).replace('body dynamic compound','body static compound')
 rows+=f.object_(500,'Physical clamp',(0,1.5,0),shape)+f.object_(501,'Clamp shell',(0,0,.6),f.render((.3,.3,.25),(.24,.30,.36)),parent=500)
 fields=material('specimen',500).replace(str(240/(1.6*.56*2.4)),'1000')
 start=fields.index('  deformable.attachments ');fields=fields[:start]+'  deformable.attachments '+json.dumps('1 "part-2" 1 500 "" 0 0 0 1')+'\n'
 rows+=f.object_(520,'Generic three-section beam',(0,0,0),fields+f.render((.15,.15,.9),(.18,.60,.80)),parent=500)
 rows+=f.object_(550,'Ordinary contact prop',(.25,.45,-.7),f.render((.2,.2,.2),(.65,.55,.22))+f.body((.2,.2,.2),12))
 rows+=f.object_(3,'Specimen contact floor',(0,-.2,0),f.render((5,.2,5),(.10,.14,.18))+f.body((5,.2,5)))+f.object_(60,'Observer',(3,3,4),'  player-start first-person\n')
 return rows

def main():
 p=argparse.ArgumentParser();p.add_argument('--baker',type=Path,default=ROOT/'build/judas_deformable_bake');p.add_argument('--source-only',action='store_true');a=p.parse_args()
 for d in ['Assets/deformables','Assets/models','Assets/scripts','Assets/fonts','Assets/ui','Assets/prefabs','Assets/worlds','Scenes','Saves']:(OUT/d).mkdir(parents=True,exist_ok=True)
 for name,scale in [('nominal',1),('strong',1.4),('weak',.7)]:
  rel=f'Assets/deformables/{name}.judasdeform'
  if not a.source_only:subprocess.run([str(a.baker),'material-sections',str(OUT/rel),'2','1','2','1.6','.56','2.4',str(scale),'.20'],check=True)
  f.metadata(rel,name,'deformable')
 for rel in ['Assets/models/wheel.obj','Assets/fonts/DejaVuSans.ttf','Assets/fonts/LICENSE.txt','Assets/scripts/math.js','Assets/ui/lab.judasui']:
  source=ROOT/'projects/damage_lab'/rel
  if rel=='Assets/models/wheel.obj':source=ROOT/'projects/damage_lab/Assets/models/vehicle_wheel.obj'
  if rel.endswith('.txt') or rel.endswith('.ttf') or rel.endswith('.obj'):shutil.copyfile(source,OUT/rel)
  else:(OUT/rel).write_text(source.read_text().replace('83838383838383838383838300000005',f.IDS['font']).replace('COUPLED PHYSICAL DAMAGE','PHYSICAL CRASH LAB').replace('DAMAGE LAB','PHYSICAL CRASH LAB'))
 for rel,name,kind in [('Assets/models/wheel.obj','wheel','mesh'),('Assets/fonts/DejaVuSans.ttf','font','font'),('Assets/scripts/math.js','math','script'),('Assets/scripts/controller.js','controller','script'),('Assets/ui/lab.judasui','ui','ui'),('Assets/prefabs/car.judasprefab','car','prefab'),('Assets/worlds/lab.judasworld','world','world')]:f.metadata(rel,name,kind)
 (OUT/'Assets/deformables/specimen.source').write_text(specimen_source())
 if not a.source_only:subprocess.run([str(a.baker),'partition',str(OUT/'Assets/deformables/specimen.judasdeform'),str(OUT/'Assets/deformables/specimen.source')],check=True)
 f.metadata('Assets/deformables/specimen.judasdeform','specimen','deformable')
 (OUT/'Scenes/specimen.judas').write_text(specimen_scene())
 for case in ['normal','low','medium','high','offset','comparison','rotated','group','group_high']:(OUT/f'Scenes/{case}.judas').write_text(scene(case))
 (OUT/'Assets/prefabs/car.judasprefab').write_text(f.settings('Physical car prefab')+car(900,(0,.67,0)))
 annex=f.settings('Independent streamed physical car')+f.object_(301,'Annex ground',(0,-.15,60),f.render((10,.15,20),(.14,.20,.18))+f.body((10,.15,20)))+car(900,(0,.67,60))
 (OUT/'Scenes/annex.judas').write_text(annex)
 (OUT/'Assets/worlds/lab.judasworld').write_text('JudasWorld 1\nbudget 2 8 2 8388608 8388608 8388608 2097152\nregion "annex" "Scenes/annex.judas" 16 0 0 0 0 0 1 4 5 165 1 "snapshot" 1048576 ""\n')
 original='JudasProject 1\nname "Physical Crash Lab"\nstartup-scene "Scenes/medium.judas"\nassets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\nlegacy-gameplay "false"\nclassification "JudasClassification 1 0 0 3 3 0 \\"Environment\\" 1 \\"Assembly\\" 2 \\"Material\\" 1 1 0 \\"Default\\""\ninput-map "1 30 \\"brake\\" 0 1 \\"key:Space\\" 1 0 \\"select\\" 0 1 \\"key:Tab\\" 1 0 \\"watch\\" 0 1 \\"key:V\\" 1 0 \\"spawn\\" 0 1 \\"key:P\\" 1 0 \\"tour\\" 0 1 \\"key:T\\" 1 0 \\"region\\" 0 1 \\"key:F3\\" 1 0 \\"normal\\" 0 1 \\"key:1\\" 1 0 \\"low\\" 0 1 \\"key:2\\" 1 0 \\"medium\\" 0 1 \\"key:3\\" 1 0 \\"high\\" 0 1 \\"key:4\\" 1 0 \\"offset\\" 0 1 \\"key:5\\" 1 0 \\"comparison\\" 0 1 \\"key:6\\" 1 0 \\"specimen\\" 0 1 \\"key:7\\" 1 0 \\"rotated\\" 0 1 \\"key:8\\" 1 0 \\"structure\\" 0 1 \\"key:G\\" 1 0 \\"contacts\\" 0 1 \\"key:J\\" 1 0 \\"load\\" 0 1 \\"key:B\\" 1 0 \\"overload\\" 0 1 \\"key:H\\" 1 0 \\"save_game\\" 0 1 \\"key:F5\\" 1 0 \\"load_game\\" 0 1 \\"key:F8\\" 1 0 \\"restart\\" 0 1 \\"key:R\\" 1 0 \\"pause\\" 0 1 \\"key:Escape\\" 1 0 \\"ui_click\\" 0 1 \\"mouse:Left\\" 1 0 \\"ui_activate\\" 0 1 \\"key:Return\\" 1 0 \\"ui_up\\" 0 1 \\"key:Up\\" 1 0 \\"ui_down\\" 0 1 \\"key:Down\\" 1 0 \\"throttle\\" 1 2 \\"key:S\\" -1 0 \\"key:W\\" 1 0 \\"steer\\" 1 2 \\"key:D\\" -1 0 \\"key:A\\" 1 0 \\"look_x\\" 1 1 \\"mouse:dx\\" 1 0 \\"look_y\\" 1 1 \\"mouse:dy\\" 1 0 "\nworld-manifest "83838383838383838383c83d0000000f"\n'
 (OUT/'coarse_crash_lab.judasproj').write_text(original.replace('Structural Crash Lab','Physical Crash Lab').replace('c83c','c83d'))
 print(OUT)
if __name__=='__main__':main()
