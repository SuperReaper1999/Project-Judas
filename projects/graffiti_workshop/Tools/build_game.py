#!/usr/bin/env python3
"""Regenerate original Graffiti Workshop content. Scripts remain hand-authored."""
import copy, hashlib, json, math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def asset(path): return hashlib.sha256(('JudasGraffitiWorkshop/'+path).encode()).hexdigest()[:32]
def meta(path,kind):
 (ROOT/(path+'.judasmeta')).write_text(f'JudasAssetMeta 1\nid "{asset(path)}"\ntype {kind}\nsource "Original Graffiti Workshop content"\n')
def obj(i,name,pos=(0,0,0),**fields):
 if 'render' in fields:
  fields={'render.half-extents':[.5,.5,.5],'render.radius':1,'render.color':[1,1,1],'render.alpha':1,'render.secondary-color':[.5,.5,.5],'render.secondary-alpha':1,'render.mesh-asset':'','render.texture-asset':'',**fields}
 if 'body' in fields:
  fields={'body.half-extents':[.5,.5,.5],'body.radius':1,'body.terrain':'','body.mass':1,'body.friction':.7,'body.restitution':.1,'body.initial-velocity':[0,0,0],'body.pickable':False,'body.managed':False,'body.compound-count':0,**fields}
 return {'id':i,'name':name,'fields':{'position':list(pos),'rotation':[1,0,0,0],'scale':[1,1,1],**fields}}
def box(i,name,pos,half,color,paint=False,motion='static'):
 return obj(i,name,pos,render={'symbol':'box'},**{'render.half-extents':half,'render.color':color,'body':[{'symbol':motion},{'symbol':'box'}],'body.half-extents':half,'tags':1 if paint else 0})
def write(path,data): (ROOT/path).write_text(json.dumps(data,indent=2)+'\n')
def main():
 # Third-party font/license, texture and math helper are bundled source assets.
 # Regeneration does not depend on another project or a service.
 for path in ['Assets/fonts/DejaVuSans.ttf','Assets/fonts/LICENSE.txt','Assets/textures/disc.png','Assets/scripts/math.js']:
  if not (ROOT/path).is_file():raise RuntimeError('Missing bundled source: '+path)
 for path,kind in [('Assets/fonts/DejaVuSans.ttf','font'),('Assets/textures/disc.png','texture'),('Assets/scripts/math.js','script'),('Assets/scripts/game.js','script')]:meta(path,kind)
 # A flat 24-triangle disc with +Z normal. Actual impact positions, not screen rays.
 lines=['# Original unit paint disc, radius 1, local XY plane','v 0 0 0','vn 0 0 1']
 for n in range(24):
  a=n*2*math.pi/24;lines.append(f'v {math.cos(a):.9f} {math.sin(a):.9f} 0')
 for n in range(24):lines.append(f'f 1//1 {n+2}//1 {(n+1)%24+2}//1')
 (ROOT/'Assets/models/paint.obj').write_text('\n'.join(lines)+'\n');meta('Assets/models/paint.obj','mesh')
 # Spray-can cylinder with rings and caps; original procedural source.
 lines=[]
 for y in [-.13,.13]:
  for n in range(24):
   a=n*2*math.pi/24;lines.append(f'v {.065*math.cos(a):.9f} {y} {.065*math.sin(a):.9f}')
 for n in range(24):
  a=n+1;b=(n+1)%24+1;lines.extend([f'f {a} {b} {b+24}',f'f {a} {b+24} {a+24}'])
 for n in range(1,23):lines.extend([f'f 1 {n+2} {n+1}',f'f 25 {n+25} {n+26}'])
 (ROOT/'Assets/models/can.obj').write_text('\n'.join(lines)+'\n');meta('Assets/models/can.obj','mesh')
 settings={'name':'Graffiti Workshop','world-origin':[0,0,0],'sun-direction':[-.3,.85,.4],'sun-color':[1,.95,.86],'ambient':[.35,.38,.42],'background-color':[.12,.18,.24],'linear-rendering':True,'fluid-scale':1,'next-id':1000,'fidelity-policy':{'mode':{'symbol':'none'}}}
 mark=obj(1,'Persistent game-owned paint disc',render={'symbol':'mesh'},**{'render.mesh-asset':asset('Assets/models/paint.obj'),'tags':2})
 write('Assets/prefabs/paint.judasprefab',{'kind':'prefab','schema':1,'data':{'settings':settings,'objects':[mark]}});meta('Assets/prefabs/paint.judasprefab','prefab')
 objects=[obj(1,'Graffiti game / controls and durable paint',scripts=1,**{'script.0.id':1,'script.0.asset':asset('Assets/scripts/game.js'),'script.0.enabled':True,'script.0.properties':json.dumps({'paintPrefab':asset('Assets/prefabs/paint.judasprefab')}),'ui.asset':asset('Assets/ui/game.judasui'),'ui.name':'graffiti','ui.enabled':True}),
 obj(2,'Player / native CharacterMotor',[0,.05,3],**{'motor.enabled':True,'motor.radius':.3,'motor.halfHeight':.55,'motor.offset':[0,.85,0],'motor.stepHeight':.3,'motor.supportDistance':.15,'motor.skin':.02,'motor.maxSlopeDegrees':50,'motor.gravityScale':1,'motor.reorientationDegreesPerSecond':180,'motor.interactionMass':80,'motor.maxPushImpulse':20,'motor.collisionLayer':1,'motor.collisionMask':18446744073709551615,'motor.requiredTags':0,'motor.excludedTags':0,'gravity-selection.mode':'uniform','gravity-selection.source':0,'gravity-selection.acceleration':[0,-9.81,0]}),
 box(100,'Paintable concrete floor',[0,-.2,0],[8,.2,8],[.35,.4,.43],True),
 box(101,'Paintable plaster wall',[0,2,-4],[6,2,.2],[.8,.78,.7],True),
 box(102,'Paintable red-brick wall',[-6,2,.5],[.2,2,4.5],[.48,.22,.16],True),
 box(103,'Paintable moving canvas',[3.4,1.8,-1.6],[1.2,1.1,.12],[.65,.72,.8],True,'kinematic'),
 box(110,'Non-paintable dark steel screen',[-3,1.4,1],[.6,1.4,.15],[.08,.13,.16]),
 box(111,'Courtyard right boundary',[8,1,0],[.25,1,8],[.2,.25,.3]),
 box(112,'Courtyard rear boundary',[0,1,8],[8,1,.25],[.2,.25,.3]),
 obj(30,'Your spray can',render={'symbol':'mesh'},**{'render.mesh-asset':asset('Assets/models/can.obj'),'render.color':[1,1,1]})]
 colors=[[.05,.85,.75],[1,.1,.42],[1,.75,.05],[.55,.25,1]]
 for n,c in enumerate(colors):
  objects.append(obj(10+n,'Reliable '+['teal','pink','gold','violet'][n]+' droplets',[0,-20,0],**{'particle-emitter':[],'particle.enabled':True,'particle.loop':False,'particle.local-space':False,'particle.gravity':True,'particle.rate':0,'particle.lifetime':1.1,'particle.size':.045,'particle.end-size':.035,'particle.burst':0,'particle.capacity':96,'particle.spread':[.025,.025,.005],'particle.velocity':[0,0,-14],'particle.variation':[.35,.35,.15],'particle.acceleration':[0,0,0],'particle.texture':asset('Assets/textures/disc.png'),'particle.color':c,'particle.alpha':1,'particle.end-color':c,'particle.end-alpha':1,'particle.seed':91100+n,'particle.backend':'cpu','particle.collision':True,'particle.collision-radius':.012,'particle.restitution':0,'particle.friction':0,'particle.response':'remove','particle.collision-layers':1,'particle.impacts':'reliable','particle.impact-cell-size':.04,'particle.impact-weight':1,'particle.max-impact-cells':8,'particle.max-contacts':8,'gravity-selection.mode':'uniform','gravity-selection.source':0,'gravity-selection.acceleration':[0,-2,0]}))
 write('Scenes/courtyard.judas',{'kind':'scene','schema':1,'data':{'settings':settings,'objects':objects}})
 project=json.loads((ROOT/'graffiti_workshop.judasproj').read_text());p=project['data']
 p.update(name='Graffiti Workshop',startupScene='Scenes/courtyard.judas',saveIdentity=hashlib.sha256(b'JudasGraffitiWorkshop-v1').hexdigest(),exportScenes=['Scenes/courtyard.judas'],runtimeAssets=[asset('Assets/scripts/game.js'),asset('Assets/scripts/math.js'),asset('Assets/ui/game.judasui'),asset('Assets/prefabs/paint.judasprefab')])
 p['classification']['collision']={'nextId':2,'entries':[{'id':0,'name':'World'},{'id':1,'name':'Player'}]}
 p['classification']['tags']={'nextId':2,'entries':[{'id':0,'name':'paintable'},{'id':1,'name':'paintmark'}]}
 p['input']=[i for i in p['input'] if i['name'] in ['move_x','move_z','look_x','look_y']]
 mapping={'spray':'mouse:Left','jump':'key:Space','sprint':'key:LeftShift','pause':'key:Escape','save':'key:F5','load':'key:F9','clear':'key:C','delivery':'key:B',**{'color_'+str(i):'key:'+str(i) for i in range(1,5)}}
 for name,control in mapping.items():p['input'].append({'name':name,'axis':False,'bindings':[{'control':control,'scale':1,'deadzone':0}]})
 write('graffiti_workshop.judasproj',project)
 template=json.loads((ROOT/'Assets/ui/game.judasui').read_text());elements=template['data']['elements'];byid={e['id']:e for e in elements}
 result=[byid['canvas']]
 def label(id,text,pos,size,color=[.85,.9,.92,1],font=18):
  e=copy.deepcopy(byid['title']);e.update(id=id,text=text,offset=pos,size=size,color=color,fontSize=font,anchorMin=[0,0],anchorMax=[0,0],align=[0,0],font=asset('Assets/fonts/DejaVuSans.ttf'));result.append(e)
 label('title','GRAFFITI WORKSHOP',[25,20],[900,34],[.1,.95,.8,1],26)
 label('help','WASD walk | mouse look | hold left mouse to spray | 1–4 colours\nSpace jump | Shift run | F5 save | F9 load | Esc pause',[25,58],[1200,54],font=17)
 label('status','Loading paint…',[25,120],[1200,120],font=17)
 label('crosshair','+',[0,0],[24,30],[1,1,1,.9],24);result[-1].update(anchorMin=[.5,.5],anchorMax=[.5,.5],align=[.5,.5])
 label('hint','PLASTER / BRICK / MOVING CANVAS: paintable   •   DARK STEEL: blocks droplets, rejects paint',[25,-65],[1210,30],font=15);result[-1].update(anchorMin=[0,1],anchorMax=[0,1])
 label('debug','B: pause receiver delivery to demonstrate reliable backpressure | C: clean courtyard',[25,-35],[1210,25],font=14);result[-1].update(anchorMin=[0,1],anchorMax=[0,1])
 template['data']['elements']=result;write('Assets/ui/game.judasui',template);meta('Assets/ui/game.judasui','ui')
 print(ROOT)
if __name__=='__main__':main()
