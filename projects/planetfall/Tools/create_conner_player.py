"""Blender-only, applied to a COPY of Conner's supplied rig. Original untouched.
Reusable game clips are authored poses, not a claim that the recorded take is locomotion.
"""
import bpy,math,json,sys
from pathlib import Path
from mathutils import Vector,Quaternion
P=Path(__file__).resolve().parents[1]
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
meshes=[o for o in bpy.data.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==rig for m in o.modifiers)]
rig.animation_data_create();original=rig.animation_data.action
for c in rig.pose.bones:
 for constraint in list(c.constraints):c.constraints.remove(constraint)
for track in list(rig.animation_data.nla_tracks):rig.animation_data.nla_tracks.remove(track)
rig.animation_data.action=None
scene=bpy.context.scene;scene.render.fps=30
names={'Idle':60,'Run':30,'Jump':30,'Punch':14,'HeavyPunch':24,'Shove':18,'Seat':40}
for name,count in names.items():
 action=bpy.data.actions.new(name);rig.animation_data.action=action
 for frame in range(1,count+1):
  t=(frame-1)/(count-1);phase=t*math.tau
  for pb in rig.pose.bones:
   pb.rotation_mode='QUATERNION';pb.rotation_quaternion=Quaternion((1,0,0,0));pb.location=Vector((0,0,0));pb.scale=Vector((1,1,1))
  def swing(b,angle,axis=(1,0,0)):
   pb=rig.pose.bones.get(b)
   if pb:pb.rotation_quaternion=Quaternion(pb.bone.matrix_local.to_quaternion().inverted()@Vector(axis),angle)
  if name=='Idle':swing('Chest',.018*math.sin(phase));swing('LeftArm',-.035);swing('RightArm',.035)
  elif name=='Run':
   swing('LeftUpLeg',.5*math.sin(phase));swing('RightUpLeg',-.5*math.sin(phase));swing('LeftLeg',max(0,-math.sin(phase))*.65);swing('RightLeg',max(0,math.sin(phase))*.65);swing('LeftArm',-.35*math.sin(phase));swing('RightArm',.35*math.sin(phase));swing('Chest',.1)
  elif name=='Jump':
   bend=math.sin(math.pi*t);swing('LeftUpLeg',-.35*bend);swing('RightUpLeg',-.35*bend);swing('LeftLeg',.7*bend);swing('RightLeg',.7*bend);swing('LeftArm',-.3*bend);swing('RightArm',-.3*bend)
  elif name=='Seat':
   rig.pose.bones['Hips'].location.z=-.5;swing('LeftUpLeg',-1.5);swing('RightUpLeg',-1.5);swing('LeftLeg',1.5);swing('RightLeg',1.5);swing('LeftArm',-.6);swing('RightArm',-.6)
  else:
   hit=math.sin(math.pi*t);swing('Chest',.12*hit);swing('RightArm',-1.2*hit);swing('RightForeArm',-.45*(1-hit));
   if name in ['Shove','HeavyPunch']:swing('LeftArm',-1.2*hit);swing('LeftForeArm',-.35*(1-hit))
  for pb in rig.pose.bones:
   pb.keyframe_insert('rotation_quaternion',frame=frame,group=pb.name);pb.keyframe_insert('location',frame=frame,group=pb.name);pb.keyframe_insert('scale',frame=frame,group=pb.name)
 track=rig.animation_data.nla_tracks.new();track.name=name;track.strips.new(name,1,action);track.mute=True
rig.animation_data.action=None
# Select only the character, excluding source preview lights, camera and floor.
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
for o in meshes:o.select_set(True)
bpy.context.view_layer.objects.active=rig
for t in rig.animation_data.nla_tracks:t.mute=False
scene.frame_start=1;scene.frame_end=60;scene.frame_set(1)
out=P/'Assets/models/conner_player.glb';out.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.export_scene.gltf(filepath=str(out),export_format='GLB',use_selection=True,export_animations=True,export_animation_mode='NLA_TRACKS',export_nla_strips=True,export_force_sampling=True,export_skins=True,export_materials='EXPORT')
(P/'ArtSource').mkdir(exist_ok=True);bpy.ops.wm.save_as_mainfile(filepath=str(P/'ArtSource/conner_planetfall.blend'))
(P/'ArtSource/conner-animation.json').write_text(json.dumps({'source':'conner-stylised.blend','rig':rig.name,'joints':len(rig.data.bones),'meshPieces':len(meshes),'gameClips':list(names),'limitations':'Segmented single-bone weights; procedural locomotion and melee clips, not polished mocap'},indent=2))

import os,sys
sys.stdout.flush();sys.stderr.flush();os._exit(0)
