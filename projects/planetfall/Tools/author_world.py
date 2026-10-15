#!/usr/bin/env python3
"""Rebuild Planetfall content. Does not change the source consumer games."""
from pathlib import Path
import json,hashlib,re,shutil,subprocess,math
ROOT=Path(__file__).resolve().parents[3];P=Path(__file__).resolve().parents[1];A=P/'Assets'
def asset(path,kind):
 id=hashlib.md5(('planetfall/'+str(path.relative_to(P))).encode()).hexdigest();path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{id}"\ntype {kind}\nsource "Planetfall authored content"\n');return id
ids=json.loads((A/'ids.json').read_text())
for f in (A/'scripts').glob('*.js'):
 meta=f.with_name(f.name+'.judasmeta');ids[f.stem]=re.search(r'id "(.*?)"',meta.read_text()).group(1) if meta.exists() else asset(f,'script')
for f in (P/'ArtSource/generated').glob('*.glb'):
 name='frontier_house' if 'cottage' in f.name else 'research_tower' if 'research' in f.name else 'rover_body' if 'pickup' in f.name else 'astronaut' if 'astronaut' in f.name else 'pirate' if 'pirate' in f.name else 'windmill' if 'windmill' in f.name else 'relay_rocket' if 'rocket' in f.name else 'generated_pine' if 'pine' in f.name else 'generated_oak' if 'oak' in f.name else None
 # Unknown experimental exports must never silently replace a runtime asset.
 if name is None:continue
 dest=A/'models'/f'{name}.glb';shutil.copy2(f,dest);ids[name]=asset(dest,'mesh')
 if name=='frontier_house':
  from prompt3d.library import load_scene
  from prompt3d.export import to_glb
  opened=load_scene(dest)
  for mats in opened.groups.values():mats.pop('lib_house_door',None)
  to_glb(opened,dest,textures=False,flat=True)

for f in (A/'models').glob('*_surface.glb'):ids[f.stem]=asset(f,'mesh')
ids['aether']=asset(A/'models/aether.glb','mesh')
for name in ['earth','moon','mars','derelict']:
 src=A/'models'/f'{name}_surface.glb';dest=A/'models'/f'{name}_surface.judascollision';subprocess.run([str(ROOT/'build/judas_model_import_cli'),'--collision',str(src),'0',str(dest),'--weld=0.00001'],check=True,stdout=subprocess.DEVNULL);ids[name+'_collision']=asset(dest,'collision')
# Preserve the exact accepted cave products and all freshness dependencies.
shutil.copytree(ROOT/'projects/volume_terrain_lab/Assets/Volumes',A/'Volumes',dirs_exist_ok=True)
shutil.copy2(ROOT/'projects/volume_terrain_lab/Imports/cave.judasvolume',P/'Imports/cave.judasvolume')
# Prompt3D visual houses contain intersecting separately authored wall objects.
# The collision cooker correctly rejects a merged non-manifold render source.
# Retain the generated per-piece primitives as ordinary physical boxes instead.
from prompt3d.nlu import parse
from prompt3d.generators.house import house
from prompt3d.generators.building import building
from prompt3d.pipeline import build_styled
from prompt3d.export import to_glb
import numpy as np
street_sources={}
for name,category,params,seed in [('supply_shop','shop',{'type':'supermarket','features':{},'size':1,'count':1},710),('stone_road','road',{'type':'straight','length':30,'material':'stone','features':{'streetlights':True},'size':1,'count':1},711),('wood_railing','railing',{'length':8,'material':'wood','features':{},'size':1,'count':1},712),('stone_stairs','stairs',{'type':'straight','material':'stone','steps':10,'features':{},'size':1,'count':1},713)]:
 scene=build_styled(category,params,seed,'lowpoly')
 # An abandoned shop has an open storefront, authored in this consumer copy.
 if category=='shop':
  for mats in scene.groups.values():mats.pop('market_glass',None)
 dest=A/'models'/f'{name}.glb';to_glb(scene,dest,textures=False,flat=True);ids[name]=asset(dest,'mesh');street_sources[name]=scene
house_sources={}
for name,prompt,seed,fn in [('frontier_house','a low poly two storey cottage with 2 bedrooms a porch and a chimney',83,house),('research_tower','a low poly 9 m tall stone research building with an antenna',181,building)]:
 scene=fn(parse(prompt)['params'],np.random.default_rng(seed));parts=[]
 for group,mats in scene.groups.items():
  for material,prims in mats.items():
   if not any(k in material for k in ['wall','slab','stair','floor','roof','tower_frame','tower_glass','tower_core']):continue
   for prim in prims:
    lo=prim.V.min(axis=0);hi=prim.V.max(axis=0);half=(hi-lo)/2
    if min(half)<.01:continue
    parts.append(((lo+hi)/2,half))
 house_sources[name]=parts
def obj(id,name,p,fields='',parent=None):return f'object {id} "{name}"\n'+(f'  parent {parent}\n' if parent else '')+f'  position {p[0]} {p[1]} {p[2]}\n  rotation 1 0 0 0\n  scale 1 1 1\n'+fields+'end\n\n'
def mesh(name):return f'  render mesh\n  render.half-extents .5 .5 .5\n  render.radius .5\n  render.alpha 1\n  render.secondary-color .8 .8 .8\n  render.secondary-alpha 1\n  render.texture-asset \"\"\n  render.mesh-asset "{ids[name]}"\n  render.color 1 1 1\n'
def box(half=(.5,.5,.5),dynamic=False,mass=20):return f'  body {"dynamic" if dynamic else "static"} box\n  body.half-extents {" ".join(map(str,half))}\n  body.radius .5\n  body.terrain \"\"\n  body.mass {mass}\n  body.initial-velocity 0 0 0\n  body.pickable false\n  body.managed false\n  body.compound-count 0\n  body.fluid-cavity-count 0\n  body.friction .7\n  body.restitution .05\n'
def tri(id):return f'  body static triangle-mesh\n  body.collision-asset "{id}"\n  body.half-extents .5 .5 .5\n  body.radius .5\n  body.terrain \"\"\n  body.mass 1\n  body.friction .8\n  body.restitution .05\n  body.initial-velocity 0 0 0\n  body.pickable false\n  body.managed false\n  body.compound-count 0\n  body.fluid-cavity-count 0\n'
# Ordinary base building prefab, with open door and physical roof/walls.
buildid=hashlib.md5(b'planetfall/base-build').hexdigest();pref=A/'prefabs/base_build.judasprefab'
header=(ROOT/'projects/lastlight/Assets/prefabs/zombie.judasprefab').read_text().split('object ')[0]
parts=[((0,-.2,0),(2,.1,2)),((-1.9,1.4,0),(.1,1.3,2)),((1.9,1.4,0),(.1,1.3,2)),((0,1.4,-1.9),(2,1.3,.1)),((-1.4,1.4,1.9),(.6,1.3,.1)),((1.4,1.4,1.9),(.6,1.3,.1)),((0,2.8,0),(2,.1,2))]
pref.write_text(header+obj(1,'Built refuge',(0,0,0),box((2,1.5,2)).replace('body static box','body static compound').replace('body.compound-count 0','body.compound-count 7')+''.join('  body.compound-box '+' '.join(map(str,p+h))+'\n' for p,h in parts))+''.join(obj(i+2,'Fortification panel',p,mesh('wood_crate').replace('render mesh','render box').replace('render.half-extents .5 .5 .5','render.half-extents '+' '.join(map(str,h))).replace('render.color 1 1 1','render.color .32 .42 .39'),1) for i,(p,h) in enumerate(parts)));asset(pref,'prefab');ids['building']=re.search(r'id "(.*?)"',pref.with_name(pref.name+'.judasmeta').read_text()).group(1)
base=(ROOT/'projects/lastlight/Scenes/town.judas').read_text();head=base[:base.index('object ')];blocks=re.findall(r'object (\d+) [\s\S]*?\nend\n',base)
parsed={int(b.split()[1]):b+'\n' for b in re.findall(r'object \d+ [\s\S]*?\nend\n',base)}
# Retain proven combat, cabin, UI and neighbourhood. Remove only original tiny
# island planets and their contents in this copied project.
keep={i:b for i,b in parsed.items() if i not in [1,2,50] and not (3060<=i<=3158) and i not in [4000,5000,6000]}
# Add authored build prefab to existing player properties (same reviewed controls).
b=keep[10];m=re.search(r'script.0.properties (".*")',b);props=json.loads(json.loads(m.group(1)));props['building']=ids['building'];keep[10]=b[:m.start(1)]+json.dumps(json.dumps(props,separators=(',',':')))+b[m.end(1):]
# Reuse exact physical M81/M82 mechanical assembly, remapping EVERY native ID.
rover=(ROOT/'projects/powertrain_lab/Scenes/garage.judas').read_text();rblocks=re.findall(r'object \d+ [\s\S]*?\nend\n',rover);mapping={int(b.split()[1]):25000+int(b.split()[1])-1 for b in rblocks}
for b in rblocks:
 for prefix in ['object','parent','wheel.chassis']:
  b=re.sub(r'(?m)^(\s*'+re.escape(prefix)+r' )([0-9]+)',lambda m:m.group(1)+str(mapping.get(int(m.group(2)),int(m.group(2)))),b)
 b=re.sub(r'(\\"(?:target|bearing)\\":\\")([0-9]+)',lambda m:m.group(1)+str(mapping.get(int(m.group(2)),int(m.group(2)))),b)
 if b.startswith('object 25000 '):b=b.replace('position 35 0.94 4','position -8 0.94 24')
 # Keep inherited wheel geometry assets and their metadata unchanged.
 for aid in re.findall(r'render.mesh-asset "(.*?)"',b):
  for meta in (ROOT/'projects/powertrain_lab/Assets').rglob('*.judasmeta'):
   if f'id "{aid}"' in meta.read_text():
    f=meta.with_name(meta.name[:-10]);shutil.copy2(f,A/'models'/f.name);shutil.copy2(meta,A/'models'/meta.name)
 keep[int(b.split()[1])]=b+'\n'
# Cover the compact lab chassis with an actual Prompt3D pickup exterior.
keep[25000]=re.sub(r'  render box\n[\s\S]*?  body dynamic',mesh('rover_body')+'  body dynamic',keep[25000],count=1)
seen=set();clean=[]
for line in keep[25000].splitlines():
 key=line.strip().split(' ')[0]
 if key.startswith('render.'):
  if key in seen:continue
  seen.add(key)
 clean.append(line)
keep[25000]='\n'.join(clean)+'\n\n'
# Three memorable companions are actual low-poly characters in the world.
for i,(name,p) in enumerate([('Mara Venn / outpost keeper',(-10,.05,8)),('Ivo Rook / caravan captain',(-14,.05,8)),('Dr Sen Vale / survey scientist',(-6,.05,8))]):keep[22100+i]=obj(22100+i,name,p,mesh('character_adventurer'))
for i,(x,z) in enumerate([(-55,-35),(70,-15),(-100,45),(105,55),(-110,-100),(125,-100)]):keep[22200+i]=obj(22200+i,'Frontier cottage',(x,0,z),mesh('frontier_house'))
for owner,name in [(22200+i,'frontier_house') for i in range(6)]+[(22220,'research_tower')]:
 for j,(center,half) in enumerate(house_sources[name]):
  id=30000+(owner-22200)*200+j
  keep[id]=obj(id,'Generated physical wall/floor',center,box(tuple(half)),owner)
# Large chunks retain open authored doors and stairs rather than phantom colliders.
keep[22220]=obj(22220,'Research tower',(110,0,-80),mesh('research_tower'))
for i in range(65):
 a=i*2.399963;r=75+(i%8)*23;x=math.sin(a)*r;z=math.cos(a)*r
 if abs(x)<18 or abs(z)<18:continue
 keep[22400+i]=obj(22400+i,'Wildwood tree',(x,0,z),mesh('generated_pine' if i%3==0 else 'generated_oak')+box((.25,1.2,.25)))
stairs=A/'models/stairs_5step.glb';shutil.copy2(ROOT/'LowPolyAssets/low_poly_starter/Exports/stairs_5step.glb',stairs);ids['stairs']=asset(stairs,'mesh')
for k in range(10):
 fields=mesh('stairs')+box().replace('body static box','body static compound').replace('body.compound-count 0','body.compound-count 5')
 fields+=''.join(f'  body.compound-box 0 {.1*(j+1)} {-.3-.6*j} .75 {.1*(j+1)} .3\n' for j in range(5))
 keep[22600+k]=obj(22600+k,'Research scaffold steps',(125.5,k,-50-k*3),fields.replace('  scale 1 1 1','  scale 1 1 2'))
 # Visual scaling belongs to the separate render-only child, not the physical body.
 keep[22600+k]=re.sub(r'(?m)^  render(?:\.[^ ]+)? .*\n','',keep[22600+k])
 keep[22800+k]=obj(22800+k,'Research stair visual',(0,0,0),mesh('stairs'),22600+k).replace('  scale 1 1 1','  scale 1 1 2')

# Real service ramp, an independently collidable authored access route.
angle=math.atan2(10.6,36);length_ramp=math.hypot(10.6,36)
ramp=mesh('metal_crate').replace('render mesh','render box').replace('render.half-extents .5 .5 .5',f'render.half-extents .9 .12 {length_ramp/2}')+box((.9,.12,length_ramp/2))
keep[22850]=obj(22850,'Research service ramp',(128.5,5.3,-62),ramp).replace('rotation 1 0 0 0',f'rotation {math.cos(angle/2)} {math.sin(angle/2)} 0 0')
keep[22851]=obj(22851,'Roof access landing',(126.5,10.6,-80),mesh('metal_crate').replace('render mesh','render box').replace('render.half-extents .5 .5 .5','render.half-extents 2.9 .1 1')+box((2.9,.1,1)))

keep[22103]=obj(22103,'Ada Quill / Wayfarer engineer',(5,.05,43),mesh('astronaut'))
keep[22700]=obj(22700,'Lantern Supply / abandoned supermarket',(-145,0,-30),mesh('supply_shop'))
shop_parts=[]
for mats in street_sources['supply_shop'].groups.values():
 for material,prims in mats.items():
  if not any(k in material for k in ['wall_metal','roof_concrete','floor_tile']):continue
  for prim in prims:
   lo=prim.V.min(axis=0);hi=prim.V.max(axis=0);shop_parts.append(((lo+hi)/2,(hi-lo)/2))
for i,(center,half) in enumerate(shop_parts):keep[37000+i]=obj(37000+i,'Supply shop physical wall/floor',center,box(tuple(half)),22700)
for i,z in enumerate([-45,-15,15]):keep[22720+i]=obj(22720+i,'Generated settlement road',(0,0,z),mesh('stone_road'))
keep[22750]=obj(22750,'Salvage yard concrete stop',(145,1,73),mesh('metal_crate').replace('render mesh','render box').replace('render.half-extents .5 .5 .5','render.half-extents 2.5 1 .35').replace('render.color 1 1 1','render.color .38 .39 .35')+box((2.5,1,.35)))

for i in range(12):keep[23000+i]=obj(23000+i,'Finite salvage supply',(-30+(i%6)*12,.55,-15+(i//6)*45),mesh('metal_crate')+box((.5,.5,.5),True))
for i,(item,p) in enumerate([('iron',(-80,1,-85)),('aether',(85,1,90)),('alloy',(-65,1,-65))]):keep[24000+i]=obj(24000+i,item+' deposit',p,mesh('aether')+box((1,1,1)))
keep[22080]=obj(22080,'Volumetric blue seam cave',(85,5,105),mesh('wood_crate').replace(ids['wood_crate'],'457b2a103bb193731760653e57080873')+tri('2bac78442c6fad3e6313ae32895f5678'))
# Destructible physical obstruction ahead of the survivor. Its script spawns
# actual body debris; mission checks its lifetime, never a dialogue toggle.
block=parsed[700];block=re.sub(r'^object 700 .*', 'object 22090 "Collapsed rescue passage"',block);block=re.sub(r'  position .*','  position 85 1 90',block);keep[22090]=block
for i,(name,p) in enumerate([('Survey recorder',(119,11.1,-80)),('Relay core',(35,1,-35)),('Red archive',(119,11.1,-80)),('Flight recorder',(110,1,-80)),('Trapped survivor',(85,1,93))]):
 keep[26000+i]=obj(26000+i,name,p,mesh('metal_crate' if i<4 else 'astronaut')+box((.3,.3,.3)))
for region,R,origin in [('earth',796375,(0,796375,0)),('moon',217175,(48050000,217175,0)),('mars',423687.5,(28000000000,423687.5,0)),('derelict',796375,(0,806375,0))]:
 h=head.replace('name "Lastlight"',f'name "Planetfall / {region}"').replace('world-origin 0 0 0','world-origin '+' '.join(map(str,origin))).replace('next-id 7000','next-id 40000')
 extra=obj(1,'Curved planetary surface',(0,0,0),mesh(region+'_surface')+tri(ids[region+'_collision']))+obj(2,'Ordinary RadicalGravity',(0,-R,0),f'  gravity radial {1.62 if region=="moon" else 3.71 if region=="mars" else 9.81}\n  gravity.region sphere {R*2}\n')
 if region=='moon':extra+=obj(22900,'Abandoned mining launch',(50,0,-35),mesh('relay_rocket'))+obj(22901,'Mining launch physical envelope',(50,25,-35),box((6.5,25,6.5)))
 nav=parsed[50].replace('nav.surface.halfExtents "48 12 54"','nav.surface.halfExtents "160 30 160"').replace('nav.surface.cellSize "0.25"','nav.surface.cellSize "0.5"');aid=hashlib.md5(('planetfall/nav/'+region).encode()).hexdigest();nav=re.sub(r'nav.surface.asset ".*?"',f'nav.surface.asset "{aid}"',nav);nf=A/'navigation'/f'{region}.judasnav';nf.with_name(nf.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{aid}"\ntype navigation\nsource ""\n')
 (P/'Scenes'/f'{region}.judas').write_text(h+extra+''.join(keep.values())+nav)
# Project-only controls use the existing logical input machinery.
project=(ROOT/'projects/lastlight/lastlight.judasproj').read_text().replace('name \"Lastlight\"','name \"Planetfall\"').replace('Scenes/town.judas','Scenes/earth.judas');match=re.search(r'input-map (".*")',project);im=json.loads(match.group(1));im=im.replace('1 30 ','1 39 ',1)
for action,key in [('journal','J'),('quest_use','H'),('mine','M'),('save_game','F5'),('load_game','F9'),('drive_use','K'),('atlas','O'),('eat','Y'),('side_quest','P')]:im+=f' "{action}" 0 1 "key:{key}" 1 0'
project=project[:match.start(1)]+json.dumps(im)+project[match.end(1):];(P/'planetfall.judasproj').write_text(project)
(A/'ids.json').write_text(json.dumps(ids,indent=2))
print('Planetfall authored: four regions, curved collision terrain, campaign and physical rover')
