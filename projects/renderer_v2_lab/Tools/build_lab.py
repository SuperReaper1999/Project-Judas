#!/usr/bin/env python3
"""Rebuild this editable renderer workshop's original procedural source content.
Pillow is needed only to regenerate PNG source assets, never to run the project.
Existing edited Scenes/assets are overwritten only with explicit --replace.
"""
import argparse, copy, hashlib, json, math
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
CASES=[('ordinary','Ordinary gallery',256,16),('mixed','Mixed PBR materials',1024,32),
       ('sprites','Ordered sprite field',4096,0),('particles','Generic particle presentation',0,16),
       ('lights','Overlapping lights and shadows',1024,256),('repeated','Repeated-object stress',4096,16)]

def identity(path):return hashlib.sha256(('JudasRendererV2/'+path).encode()).hexdigest()[:32]
def meta(path,kind):
 (ROOT/(path+'.judasmeta')).write_text('JudasAssetMeta 1\nid "'+identity(path)+'"\ntype '+kind+'\nsource "Original procedural Renderer V2 Workshop asset"\n')
def obj(i,name,**fields):
 if 'render' in fields:
  fields={'render.half-extents':[.5,.5,.5],'render.radius':1,'render.color':[1,1,1],'render.alpha':1,
   'render.secondary-color':[.7,.7,.7],'render.secondary-alpha':1,'render.mesh-asset':'','render.texture-asset':'',**fields}
 return {'id':i,'name':name,'fields':{'position':[0,0,0],'rotation':[1,0,0,0],'scale':[1,1,1],**fields}}
def cube(i,name,p,extent,color):return obj(i,name,position=p,render={'symbol':'box'},**{'render.half-extents':extent,'render.color':color,'render.alpha':1})
def controller(case):
 return obj(1,'Renderer V2 workshop controller',scripts=1,**{'script.0.id':1,'script.0.asset':'976adfb1bd740285917be6704a2d3bda','script.0.enabled':True,'script.0.properties':json.dumps({'caseKey':case}),'ui.asset':'18a2a91d0a9c88a60313763e2a947adc','ui.name':'rendering_lab','ui.enabled':True})
def material(i):
 color=[.22+(i%4)*.16,.27+((i//4)%4)*.14,.58+((i//16)%3)*.1,1]
 lines=['JudasMaterial 1','model 1','alpha 0','base '+' '.join(map(str,color)),f'metallic {(i%4)*.22}',f'roughness {.15+(i%6)*.13}',
 'emissive 0 0 0','emission 1','normal 1','occlusion 1','cutoff .5','doubleSided 0','flipV 0','uv 2 2 0 0']
 for k in range(5):
  image=identity('Assets/textures/normal.png') if k==2 else identity('Assets/textures/checker.png') if k==0 else ''
  lines.append(f'map {k} "{image}" 10497 10497 9987 9729')
 path=f'Assets/materials/finish_{i:02d}.judasmat';(ROOT/path).write_text('\n'.join(lines)+'\n');meta(path,'material')
 return identity(path)
def emit(i,p,texture):
 return obj(30000+i,'Bounded generic emitter '+str(i+1),position=p,tags=4,**{'particle-emitter':[],
 'particle.enabled':True,'particle.loop':True,'particle.local-space':False,'particle.gravity':False,
 'particle.rate':300,'particle.lifetime':4,'particle.size':.25,'particle.end-size':.55,'particle.burst':1250,'particle.capacity':1250,
 'particle.spread':[6,1,6],'particle.velocity':[0,.65,0],'particle.variation':[.8,.2,.8],'particle.acceleration':[0,0,0],
 'particle.texture':texture,'particle.color':[.2+i*.08,.8,.95-i*.06],'particle.alpha':.65,
 'particle.end-color':[.9,.25,.6],'particle.end-alpha':0,'particle.seed':92000+i})
def scene(case,label,count,lamps,materials):
 objects=[controller(case)]
 if case!='sprites':
  objects += [cube(2,'Graphite foundation',[0,-.3,0],[48,.3,38],[.12,.16,.22]),
              cube(3,'West colonnade',[-43,3,0],[.5,3,36],[.25,.31,.4]),cube(4,'East colonnade',[43,3,0],[.5,3,36],[.25,.31,.4])]
  for i in range(count):
   columns=64 if count>=1024 else 32;rows=math.ceil(count/columns)
   x=(i%columns-(columns-1)/2)*(1.15 if count>=1024 else 2.3)
   z=(i//columns-(rows-1)/2)*(1.0 if count>=1024 else 3.8)
   o=cube(100+i,'Repeated machined module',[x,.65,z],[.38,.65,.38],[.2,.3,.42])
   if case=='mixed':o['fields']['render.materials']=f'1 "{materials[i%len(materials)]}" 0'
   objects.append(o)
  for i in range(lamps):
   overlap=case=='lights'; angle=i*2.39996323
   p=[12*math.cos(angle),7+math.sin(i*.7),12*math.sin(angle)] if overlap else [(i%16-7.5)*5,6,(i//16-1.5)*8]
   spot=i%8==0
   objects.append(obj(10000+i,'Independent luminaire '+str(i+1),position=p,rotation=[.7071067811865476,-.7071067811865476,0,0],tags=1,
       light={'symbol':'spot' if spot else 'point'},**{'light.color':[.12,.17,.22] if overlap else [2+.5*(i%3),1+.5*((i+1)%3),2.5],
       'light.intensity':3 if overlap else 35,
       'light.range':80 if overlap else 18,'light.cone':{'innerDegrees':30,'outerDegrees':50},'light.shadows':spot,'light.shadow-priority':i%4}))
   objects.append(cube(20000+i,'Luminaire housing',p,[.18,.1,.18],[.58,.5,.25]))
  if case=='particles':
   for i in range(8):objects.append(emit(i,[(i%4-1.5)*10,2,(i//4-.5)*15],identity('Assets/textures/disc.png')))
  # Real independent views, controlled separately from local split-screen.
  for i,(p,q) in enumerate([([-27,7,26],[.9173322375,-.0729548464,-.3901492023,-.0310283166]),([29,9,-24],[.4233636166,-.044490349,.8999113451,.094569699])]):
   objects.append(obj(40000+i,'Independent gallery camera '+str(i+1),position=p,rotation=q,tags=2,**{'render-camera':[],'camera.enabled':False,'camera.width':384,'camera.height':216,'camera.cadence':2,'camera.fov':65,'camera.near':.1,'camera.far':200}))
   monitor=cube(40010+i,'Live camera monitor',[(-1 if i==0 else 1)*5,5,-27],[4,2.25,.12],[1,1,1]);monitor['fields']['render.texture-camera']=40000+i;objects.append(monitor)
 else:
  side=64
  for i in range(count):
   settings={'version':1,'size':[.9,.9],'pivot':[.2 if i%9==0 else .5,.5],'rect':[0,0,1,1],'tint':[1,1,1,1],
    'flipX':i%5==0,'flipY':False,'nearest':i%11!=0,'pixelSnap':False,'alphaPick':True,'ySort':True,'sortingLayer':i%3,'order':i%7}
   objects.append(obj(100+i,'Ordered sprite '+str(i+1),position=[(i%side-31.5)*.7,(i//side-31.5)*.7,0],render={'symbol':'sprite'},
    **{'render.texture-asset':identity('Assets/textures/disc.png'),'render.color':[.35+(i%7)*.08,.8,.95],'render.alpha':.75,'render.sprite-v1':json.dumps(settings,separators=(',',':'))}))
 settings={'name':label,'world-origin':[0,0,0],'sun-direction':[-.3,.8,.4],'sun-color':[.65,.7,.8],'ambient':[.18,.21,.26],'linear-rendering':True,
  'next-id':50000,'fluid-scale':1,'fidelity-policy':{'mode':{'symbol':'none'}}}
 return {'kind':'scene','schema':1,'data':{'settings':settings,'objects':objects}}
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--replace',action='store_true');args=parser.parse_args()
 if (ROOT/'Scenes/ordinary.judas').exists() and not args.replace:raise SystemExit('Authored scenes exist. Use --replace only if you intend to regenerate them.')
 for d in ['Assets/textures','Assets/materials','Scenes']:(ROOT/d).mkdir(parents=True,exist_ok=True)
 for name in ['disc','checker','normal']:
  image=Image.new('RGBA',(32,32))
  for y in range(32):
   for x in range(32):
    alt=(x//4+y//4)%2
    if name=='normal':pixel=(136 if alt else 120,128,254,255)
    elif name=='checker':pixel=(210 if alt else 125,220 if alt else 155,235 if alt else 175,255)
    else:
     radius=math.hypot(x-15.5,y-15.5);alpha=max(0,min(255,int((15-radius)*140)))
     pixel=(255,255,255,alpha)
    image.putpixel((x,y),pixel)
  path='Assets/textures/'+name+'.png';image.save(ROOT/path);meta(path,'texture')
 materials=[material(i) for i in range(64)]
 for case,label,count,lamps in CASES:(ROOT/'Scenes'/f'{case}.judas').write_text(json.dumps(scene(case,label,count,lamps,materials),indent=2)+'\n')
if __name__=='__main__':main()
