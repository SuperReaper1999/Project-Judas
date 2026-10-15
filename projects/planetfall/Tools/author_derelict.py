"""An authored enterable wreck with real deck, bulkheads and cargo collision."""
from pathlib import Path
import re,hashlib,shutil
P=Path(__file__).resolve().parents[1];ROOT=P.parents[1];A=P/'Assets';path=P/'Scenes/derelict.judas';text=path.read_text();header=text[:text.index('object ')];blocks=re.findall(r'object \d+ [\s\S]*?\nend\n',text)
kept=[]
for b in blocks:
 id=int(b.split()[1])
 if 10<=id<=25 or 3000<=id<=3059 or id==50 or id==26003:kept.append(b)
# Native primitive shapes are the collision representation of modular ship panels.
# Furniture and equipment use the actual low-poly pack models.
def render(half,color):return f'  render box\n  render.half-extents {" ".join(map(str,half))}\n  render.radius .5\n  render.color {color}\n  render.alpha 1\n  render.secondary-color .8 .8 .8\n  render.secondary-alpha 1\n  render.mesh-asset ""\n  render.texture-asset ""\n'
def body(half):return f'  body static box\n  body.half-extents {" ".join(map(str,half))}\n  body.radius .5\n  body.terrain ""\n  body.mass 100\n  body.friction .7\n  body.restitution 0\n  body.initial-velocity 0 0 0\n  body.pickable false\n  body.managed false\n  body.compound-count 0\n  body.fluid-cavity-count 0\n'
def obj(id,name,p,fields):return f'object {id} "{name}"\n  position {" ".join(map(str,p))}\n  rotation 1 0 0 0\n  scale 1 1 1\n'+fields+'end\n\n'
kept.append(obj(2,'Derelict artificial gravity',(0,0,0),'  gravity uniform 9.81\n  gravity.region sphere 300\n'))
for i,(pos,h,col) in enumerate([((0,-.2,0),(24,.2,80),'.18 .22 .25'),((0,7,0),(24,.15,80),'.23 .28 .32'),((-24,3.5,0),(.2,3.5,80),'.24 .31 .35'),((24,3.5,0),(.2,3.5,80),'.24 .31 .35'),((0,3.5,-80),(24,3.5,.2),'.2 .3 .34'),((0,3.5,80),(24,3.5,.2),'.2 .3 .34')]):kept.append(obj(40000+i,'Hull deck and bulkhead',pos,render(h,col)+body(h)))
for i,z in enumerate([-55,-30,-5,20]):
 for k,x in enumerate([-16,16]):kept.append(obj(40010+i*2+k,'Room divider with broad central doorway',(x,2.5,z),render((8,2.5,.15),'.3 .38 .42')+body((8,2.5,.15))))
for i,name in enumerate(['bed','computer_terminal','bookshelf','chair','table']):
 src=ROOT/'LowPolyAssets/low_poly_starter/Exports'/f'{name}.glb';dest=A/'models'/f'{name}.glb';shutil.copy2(src,dest);aid=hashlib.md5(('planetfall/'+name).encode()).hexdigest();dest.with_name(dest.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{aid}"\ntype mesh\nsource "original low-poly starter pack"\n');fields=render((.5,.5,.5),'1 1 1').replace('render box','render mesh').replace('render.mesh-asset ""',f'render.mesh-asset "{aid}"');kept.append(obj(40100+i,'Crew habitat '+name,(-12+i*5,.02,-65),fields))
for i in range(12):
 id=23000+i;b=next((b for b in blocks if b.startswith(f'object {id} ')),None)
 if b:kept.append(re.sub(r'  position [^\n]*',f'  position {-14+i%4*8} .55 {40-(i//4)*20}',b))
result=header+''.join(kept);result=re.sub(r'(object 26003 [\s\S]*?  position )[^\n]*',r'\g<1>0 1 -72',result);path.write_text(result)
print('Derelict: real interior, five crew rooms, physical cargo and a recoverable flight recorder')
