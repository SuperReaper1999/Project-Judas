"""Original seated pilot pose, derived from Lastlight's original fighter asset.
Blender background authoring only; output is a normal shared glTF asset.
"""
from pathlib import Path
import bpy,math
from mathutils import Quaternion,Vector
P=Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(P/'Assets/models/character_fighter.glb'))
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
rig.animation_data_create();action=bpy.data.actions.new('PilotSeat');rig.animation_data.action=action
for f in [1,31]:
 for b in rig.pose.bones:
  b.rotation_mode='QUATERNION';b.rotation_quaternion=Quaternion();b.location=(0,0,0);b.scale=(1,1,1)
 for side in ['L','R']:
  for name,angle in [('thigh.'+side,-math.pi/2),('shin.'+side,math.pi/2),('upper_arm.'+side,-.6),('forearm.'+side,-.7)]:
   b=rig.pose.bones[name];basis=b.bone.matrix_local.to_quaternion();b.rotation_quaternion=basis.inverted()@Quaternion(Vector((1,0,0)),angle)@basis
 # Lower pelvis onto the chair; the pose remains purely visual.
 b=rig.pose.bones['pelvis'];b.location=b.bone.matrix_local.to_quaternion().inverted()@Vector((0,0,-.35))
 for b in rig.pose.bones:
  b.keyframe_insert('rotation_quaternion',frame=f);b.keyframe_insert('location',frame=f)
bpy.context.scene.render.fps=30;bpy.context.scene.frame_set(1)
bpy.ops.export_scene.gltf(filepath=str(P/'Assets/models/pilot_seated.glb'),export_format='GLB',export_animations=True,
 export_animation_mode='ACTIVE_ACTIONS',export_nla_strips_merged_animation_name='PilotSeat',export_force_sampling=True,export_frame_range=False,export_anim_slide_to_zero=True,export_cameras=False,export_lights=False)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(P/'ArtSource/pilot_seated.blend'))
