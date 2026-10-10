"""Original CC0 M74 format fixture. Blender CREATES source art; Judas retargets it.

Run Blender 5.2.1 --background --factory-startup --python this_file.py
Output is written beside this script. No source animation is retargeted here.
"""
from pathlib import Path
import math
import bpy
from mathutils import Vector

OUT = Path(__file__).resolve().parent
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.render.fps = 30
scene.frame_start = 1
scene.frame_end = 31
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 1

data = bpy.data.armatures.new("SourceHierarchy")
rig = bpy.data.objects.new("BorrowSource", data)
scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
definitions = [
    ("Base", None, (0, 0, 0), (0, 0, .45), .31),
    ("UnmappedPivot", "Base", (.2, 0, .45), (.2, 0, .75), -.37),
    ("Reach", "UnmappedPivot", (.2, 0, .75), (.7, 0, 1), .49),
    ("Tip", "Reach", (.7, 0, 1), (1, 0, 1.2), -.24),
    ("Opposite", "Base", (0, 0, .4), (-.6, 0, .6), .18),
]
for name, parent, head, tail, roll in definitions:
    bone = data.edit_bones.new(name)
    bone.head, bone.tail, bone.roll = head, tail, roll
    if parent:
        bone.parent = data.edit_bones[parent]
bpy.ops.object.mode_set(mode="OBJECT")

vertices, faces, assignments = [], [], []
for name, _, head, tail, _ in definitions:
    center = (Vector(head) + Vector(tail)) * .5
    start = len(vertices)
    # Independent weighted little boxes make asymmetric articulated motion
    # visible without commercially sourced art or auto-weighting ambiguity.
    for z in (-.10, .10):
        for y in (-.08, .08):
            for x in (-.10, .10):
                vertices.append(tuple(center + Vector((x, y, z))))
                assignments.append(name)
    for corners in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1),
                    (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        faces.append(tuple(start + i for i in corners))
mesh_data = bpy.data.meshes.new("RigidWeightedBoxes")
mesh_data.from_pydata(vertices, [], faces)
mesh_data.update()
mesh = bpy.data.objects.new("SourceBoxes", mesh_data)
scene.collection.objects.link(mesh)
for name, *_ in definitions:
    group = mesh.vertex_groups.new(name=name)
    group.add([i for i, owner in enumerate(assignments) if owner == name], 1, "REPLACE")
modifier = mesh.modifiers.new("Skin", "ARMATURE")
modifier.object = rig
mesh.parent = rig

for pose in rig.pose.bones:
    pose.rotation_mode = "XYZ"
for frame, phase in ((1, 0), (16, .5), (31, 1)):
    scene.frame_set(frame)
    # Root locomotion is separate from helper bob and asymmetrical reach.
    rig.pose.bones["Base"].location = (phase * .4, 0, 0)
    rig.pose.bones["Base"].rotation_euler = (0, 0, phase * .25)
    rig.pose.bones["UnmappedPivot"].location = (0, .05 * math.sin(phase * math.pi), 0)
    rig.pose.bones["UnmappedPivot"].rotation_euler = (0, 0, phase * .30)
    rig.pose.bones["Reach"].rotation_euler = (phase * .40, 0, -.50 * math.sin(phase * math.pi))
    rig.pose.bones["Tip"].rotation_euler = (0, phase * -.20, 0)
    for pose in rig.pose.bones:
        pose.keyframe_insert("location", frame=frame)
        pose.keyframe_insert("rotation_euler", frame=frame)
rig.animation_data.action.name = "BorrowReach"
scene.frame_set(1)
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
mesh.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(
    filepath=str(OUT / "borrow_source.fbx"), use_selection=True,
    object_types={"ARMATURE", "MESH"}, axis_forward="-Z", axis_up="Y",
    add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
    bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0,
    path_mode="AUTO", use_mesh_modifiers=True,
)
bpy.ops.export_scene.gltf(filepath=str(OUT / "borrow_source.glb"),
                          export_format="GLB", use_selection=True,
                          export_animations=True, export_frame_range=True)
print("M74_ORIGINAL_FIXTURE", OUT / "borrow_source.fbx")
