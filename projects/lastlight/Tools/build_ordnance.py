"""Create original low-poly Lastlight launcher, rocket and grenade assets.

Standard glTF +Z forward / +Y up. Launcher pivot is the rear hand grip.
The .blend retains editable mesh/material geometry. Runtime motion/damage is JS.
Run with Blender: blender -b -noaudio --python Tools/build_ordnance.py
"""
import bpy, math, json, hashlib, os, sys
from pathlib import Path
from mathutils import Vector
P=Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system='METRIC'
def material(name,color,metal=.0):
 m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
 n=m.node_tree.nodes['Principled BSDF'];n.inputs['Base Color'].default_value=(*color,1);n.inputs['Metallic'].default_value=metal;n.inputs['Roughness'].default_value=.65;return m
olive=material('Ordnance olive',(.20,.29,.16));dark=material('Ordnance dark steel',(.055,.073,.082),.45);amber=material('Ordnance warning amber',(.80,.48,.13));steel=material('Ordnance steel',(.29,.34,.37),.65)
assets={}
def collection(name):
 c=bpy.data.collections.new(name);bpy.context.scene.collection.children.link(c);return c
def link(o,c,m):
 for old in list(o.users_collection):old.objects.unlink(o)
 c.objects.link(o);o.data.materials.append(m);return o
def box(c,name,location,dimensions,m,bevel=.01):
 bpy.ops.mesh.primitive_cube_add(size=1,location=location);o=bpy.context.object;o.name=name;o.dimensions=dimensions
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 if bevel:
  mod=o.modifiers.new('Faceted bevel','BEVEL');mod.width=bevel;mod.segments=1;bpy.ops.object.modifier_apply(modifier=mod.name)
 return link(o,c,m)
def cylinder(c,name,location,radius,depth,m,vertices=12,rotation=(math.pi/2,0,0),r2=None):
 if r2 is None:bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=location,rotation=rotation)
 else:bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=radius,radius2=r2,depth=depth,location=location,rotation=rotation)
 o=bpy.context.object;o.name=name;return link(o,c,m)
launcher=collection('rocket_launcher')
cylinder(launcher,'Launch tube',(0,.04,.21),.12,1.12,olive)
for y in [-.50,.51]:cylinder(launcher,'Protective rim',(0,y,.21),.134,.065,dark)
cylinder(launcher,'Muzzle interior',(0,-.538,.21),.096,.015,dark)
for y in [-.20,.31]:cylinder(launcher,'Tube reinforcement',(0,y,.21),.126,.055,steel)
box(launcher,'Pistol grip',(0,.10,-.015),(.085,.11,.28),dark)
box(launcher,'Trigger guard',(0,.005,.035),(.045,.17,.08),steel)
box(launcher,'Front hand rest',(0,-.22,.055),(.13,.23,.08),dark)
box(launcher,'Warning marking',(.119,-.14,.22),(.014,.23,.08),amber,.003)
box(launcher,'Rear sight',(0,.24,.38),(.045,.06,.09),dark)
box(launcher,'Front sight',(0,-.35,.38),(.025,.025,.08),steel)
assets['rocket_launcher']=launcher
rocket=collection('rocket')
cylinder(rocket,'Rocket body',(0,0,0),.065,.44,olive)
cylinder(rocket,'Warhead nose',(0,-.275,0),.074,.15,dark,r2=0)
cylinder(rocket,'Tail motor',(0,.235,0),.046,.055,steel)
for side in [-1,1]:
 box(rocket,'Stabilizer side',(side*.075,.18,0),(.075,.16,.016),steel,.002)
 box(rocket,'Stabilizer upright',(0,.18,side*.075),(.016,.16,.075),steel,.002)
assets['rocket']=rocket
grenade=collection('grenade')
cylinder(grenade,'Grenade body',(0,0,.06),.075,.15,olive,rotation=(0,0,0),r2=.053)
cylinder(grenade,'Grenade lower band',(0,0,.015),.08,.022,dark,rotation=(0,0,0))
cylinder(grenade,'Fuse neck',(0,0,.154),.032,.058,steel,rotation=(0,0,0))
box(grenade,'Safety lever',(.04,0,.166),(.11,.035,.02),dark,.004)
box(grenade,'Lever side',(.083,0,.107),(.02,.035,.12),dark,.004)
bpy.ops.mesh.primitive_torus_add(major_segments=12,minor_segments=4,location=(-.04,0,.174),rotation=(math.pi/2,0,0),major_radius=.032,minor_radius=.004)
link(bpy.context.object,grenade,steel).name='Pull ring'
assets['grenade']=grenade
records=[]
for name,c in assets.items():
 bpy.ops.object.select_all(action='DESELECT')
 for o in c.objects:o.select_set(True)
 path=P/'Assets/models'/f'{name}.glb'
 bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,export_yup=True,export_animations=False,export_materials='EXPORT',export_cameras=False,export_lights=False)
 id=hashlib.md5(('lastlight/'+str(path.relative_to(P))).encode()).hexdigest()
 path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{id}"\ntype mesh\nsource ""\n')
 records.append({'name':name,'id':id,'path':str(path.relative_to(P))})
# Arrange editable originals after export; exported pivot positions stay useful.
for i,(name,c) in enumerate(assets.items()):
 meshes=list(c.objects);root=bpy.data.objects.new(name+'_export_root',None);c.objects.link(root)
 for o in meshes:o.parent=root
 root.location.x=i*.6
(P/'ArtSource').mkdir(exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(P/'ArtSource/ordnance.blend'))
(P/'Tools/ordnance-models.json').write_text(json.dumps({'assets':records,'basis':'glTF +Y up, +Z forward; launcher grip pivot','source':'Original Lastlight low-poly assets'},indent=2)+'\n')
print('ORDNANCE_ASSETS',json.dumps(records),flush=True)
sys.stdout.flush();sys.stderr.flush();os._exit(0)
