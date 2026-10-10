"""Author Lastlight's original combat presentation assets with Blender.

Uses the read-only starter-pack authoring helpers, writes only new game assets.
Run: blender -b -noaudio --python Tools/build_combat_models.py
The short clips are visual pose producers, not physics or gameplay controllers.
"""
from pathlib import Path
import sys, math, hashlib, json, os
import bpy
from mathutils import Vector, Quaternion

PROJECT=Path(__file__).resolve().parents[1]
HELPERS=PROJECT.parents[1]/'LowPolyAssets/low_poly_starter/tools'
sys.path.insert(0,str(HELPERS))
from build_pack import Library, character, ico, point_at

bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.render.fps=30
lib=Library();_,collection,_=character(lib)
scene.collection.children.link(collection)
rig=next(o for o in collection.objects if o.type=='ARMATURE')
# The reference bones retain their original names and hierarchy. Only the
# additional visual clips differ from the starter-pack character.
def world_rotation(name, axis, angle):
    bone=rig.pose.bones[name]
    basis=bone.bone.matrix_local.to_quaternion()
    bone.rotation_mode='XYZ'
    bone.rotation_euler=(basis.inverted()@Quaternion(Vector(axis),angle)@basis).to_euler('XYZ')

def author_clip(name,keys):
    action=bpy.data.actions.new(name);action.use_fake_user=True
    rig.animation_data.action=action
    for frame,values in keys:
        for bone in rig.pose.bones:
            bone.rotation_mode='XYZ';bone.rotation_euler=(0,0,0)
            bone.location=(0,0,0);bone.scale=(1,1,1)
        for bone,axis,angle in values:world_rotation(bone,axis,angle)
        for bone in rig.pose.bones:
            bone.keyframe_insert('rotation_euler',frame=frame,group=bone.name)
            bone.keyframe_insert('location',frame=frame,group=bone.name)
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:key.interpolation='LINEAR'

def punch(amount,twist=0,other=.45):
    return [('upper_arm.R',(1,0,0),-amount),('forearm.R',(1,0,0),-.5*(1-amount/1.6)),
            ('upper_arm.L',(1,0,0),-other),('forearm.L',(1,0,0),-.3),
            ('chest',(0,0,1),twist),('spine',(0,0,1),twist*.25)]
author_clip('Punch',[(1,[]),(4,punch(.9,-.08)),(8,punch(1.55,.20)),(11,punch(.7,.08)),(14,[])])
author_clip('HeavyPunch',[(1,[]),(6,punch(.8,-.40)),(12,punch(.6,-.50)),(18,punch(1.7,.48)),(25,[])])
def shove(amount,elbow):
    return [('upper_arm.'+s,(1,0,0),-amount) for s in ['L','R']]+[('forearm.'+s,(1,0,0),-elbow) for s in ['L','R']]+[('chest',(1,0,0),-.10 if amount>1.4 else .05)]
author_clip('Shove',[(1,[]),(5,shove(.7,.65)),(9,shove(1.6,0)),(12,shove(1.5,.05)),(19,[])])
rig.animation_data.action=bpy.data.actions['Idle'];scene.frame_set(1)
bpy.ops.object.select_all(action='DESELECT')
for obj in collection.all_objects:obj.select_set(True)
model=PROJECT/'Assets/models/character_fighter.glb'
bpy.ops.export_scene.gltf(filepath=str(model),export_format='GLB',use_selection=True,use_active_scene=True,
    export_yup=True,export_materials='EXPORT',export_animations=True,export_animation_mode='ACTIONS',
    export_force_sampling=True,export_frame_range=False,export_anim_slide_to_zero=True,
    export_cameras=False,export_lights=False,export_apply=False,export_skins=True)

# Camera-space wrist origin. Blender +Y becomes glTF -Z (the engine view's
# forward direction). The sleeve ends behind the wrist, toward the camera.
bpy.ops.object.select_all(action='DESELECT')
fists=lib.collection('first_person_fist');scene.collection.children.link(fists)
teal=lib.material('LP_Teal',(.12,.34,.39,1));dark=lib.material('LP_Slate_Dark',(.035,.055,.085,1));skin=lib.material('LP_Skin',(.60,.35,.22,1))
lib.box('Sleeve',(0,-.20,0),(.145,.39,.15),teal,.018,fists)
lib.box('Cuff',(0,-.022,0),(.153,.07,.155),dark,.01,fists)
lib.box('Clenched fist',(0,.09,0),(.16,.19,.16),skin,.025,fists)
lib.box('Thumb',(.095,.058,-.025),(.065,.11,.075),skin,.015,fists)
# Shallow finger ridges provide a readable fist silhouette without high detail.
for x in [-.055,-.02,.02,.055]:
    lib.box('Knuckle', (x,.186,.025),(.033,.032,.085),skin,.006,fists)
for obj in fists.objects:obj.select_set(True)
fist=PROJECT/'Assets/models/fist_forearm.glb'
bpy.ops.export_scene.gltf(filepath=str(fist),export_format='GLB',use_selection=True,use_active_scene=True,
    export_yup=True,export_materials='EXPORT',export_animations=False,export_cameras=False,export_lights=False)

# Original models are never edited; IDs follow the game's existing asset rule.
records=[]
for path in [model,fist]:
    id=hashlib.md5(('lastlight/'+str(path.relative_to(PROJECT))).encode()).hexdigest()
    path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{id}"\ntype mesh\nsource ""\n')
    records.append({'path':str(path.relative_to(PROJECT)),'id':id})
notes={'assets':records,'clips':{'Idle':2.0,'Walk':1.0,'Run':.8,'Jump':1.0,'Punch':13/30,'HeavyPunch':24/30,'Shove':18/30},
       'fistBasis':'glTF +Y up, -Z forward; wrist origin, sleeve toward +Z',
       'source':'Original starter-pack character/helper, additional original Lastlight geometry and clips',
       'gameplay':'Clips are presentation only. Project scripts decide hit timing and results.'}
(PROJECT/'Tools/combat-models.json').write_text(json.dumps(notes,indent=2)+'\n')
print('COMBAT_MODELS',json.dumps(notes),flush=True)
sys.stdout.flush();sys.stderr.flush();os._exit(0)
