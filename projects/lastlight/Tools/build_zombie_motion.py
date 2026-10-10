"""Author original Lastlight zombie clips using the preserved starter rig.

Run with Blender's Python. Writes only a NEW game GLB and editable .blend; never
overwrites character_zombie.glb or the original LowPolyAssets collection.
The imported GLB palette is retained. Locomotion clips are in-place pose sources,
not root locomotion, collision resolution or the timing/meaning of an attack.
"""
from pathlib import Path
import json
import math
import os
import struct
import sys

import bpy
from mathutils import Quaternion, Vector

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT.parents[1] / "LowPolyAssets/low_poly_starter/tools"))
from build_pack import Library, character

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene
scene.render.fps = 30
lib = Library()
_, collection, _ = character(lib)
scene.collection.children.link(collection)
rig = next(obj for obj in collection.objects if obj.type == "ARMATURE")
# All clips in the NEW asset use one rotation representation. Keeping the
# helper's Euler actions alongside quaternion actions causes ambiguous Blender
# export sampling; the preserved original asset still retains its own bank.
rig.animation_data.action = None
for old_action in list(bpy.data.actions):
    bpy.data.actions.remove(old_action)

# Reuse the current zombie's authored colours on the same original mesh/rig.
source = PROJECT / "Assets/models/character_zombie.glb"
raw = source.read_bytes()
size, _ = struct.unpack_from("<II", raw, 12)
palette = json.loads(raw[20:20 + size])
for item in palette.get("materials", []):
    material = bpy.data.materials.get(item.get("name", ""))
    color = item.get("pbrMetallicRoughness", {}).get("baseColorFactor")
    if material and color:
        material.diffuse_color = color
        material.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = color


def rotate_world(name, axis, radians):
    bone = rig.pose.bones[name]
    basis = bone.bone.matrix_local.to_quaternion()
    bone.rotation_mode = "QUATERNION"
    bone.rotation_quaternion = (basis.inverted() @ Quaternion(Vector(axis), radians) @ basis).normalized()


def reach_arm(side, shoulder, elbow, inward=.34):
    """Pose one arm in the model's forward direction (Blender -Y).

    The rest arms angle outwards; a small inward shoulder rotation brings the
    fists ahead of the chest rather than leaving a broad, raised T pose. This
    composes rotations at the joint, preserving the original bind/rest axes.
    """
    name = "upper_arm." + side
    bone = rig.pose.bones[name]
    basis = bone.bone.matrix_local.to_quaternion()
    turn = Quaternion(Vector((1, 0, 0)), -shoulder) @ Quaternion(
        Vector((0, 1, 0)), inward if side == "L" else -inward)
    bone.rotation_mode = "QUATERNION"
    bone.rotation_quaternion = (basis.inverted() @ turn @ basis).normalized()
    rotate_world("forearm." + side, (1, 0, 0), -elbow)


def reset_pose():
    for bone in rig.pose.bones:
        bone.rotation_mode = "QUATERNION"
        bone.rotation_quaternion = (1, 0, 0, 0)
        bone.location = (0, 0, 0)
        bone.scale = (1, 1, 1)


def author_clip(name, frames, sample):
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data.action = action
    for frame in range(1, frames + 2):
        reset_pose()
        sample((frame - 1) / frames)
        for bone in rig.pose.bones:
            bone.keyframe_insert("rotation_quaternion", frame=frame, group=bone.name)
            bone.keyframe_insert("location", frame=frame, group=bone.name)
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:
                        key.interpolation = "LINEAR"


def arms(left, right, elbow=.25):
    rotate_world("upper_arm.L", (1, 0, 0), -left)
    rotate_world("upper_arm.R", (1, 0, 0), -right)
    rotate_world("forearm.L", (1, 0, 0), -elbow)
    rotate_world("forearm.R", (1, 0, 0), -elbow * .85)


def idle(t):
    phase = math.tau * t
    reach_arm("L", 1.38 + .035 * math.sin(phase), .18)
    reach_arm("R", 1.46 + .025 * math.sin(phase), .12)
    rotate_world("spine", (1, 0, 0), .07)
    rotate_world("chest", (0, 0, 1), .055 * math.sin(phase))
    rotate_world("head", (0, 1, 0), .10 + .04 * math.sin(phase))
    rig.pose.bones["pelvis"].location.y = .007 * math.sin(phase)


def gait(t, running=False):
    phase = math.tau * t
    strength = .72 if running else .35
    for side in (1, -1):
        suffix = "L" if side > 0 else "R"
        swing = math.sin(phase) * side
        rotate_world("thigh." + suffix, (1, 0, 0), strength * swing)
        rotate_world("shin." + suffix, (1, 0, 0), .55 * max(0, -swing))
        rotate_world("foot." + suffix, (1, 0, 0), -.14 * swing)
    # Keep the reaching silhouette during pursuit, with asymmetric bobbing for
    # the shamble/run variants. These are poses, never root locomotion.
    reach_arm("L", (1.54 if running else 1.44) + .07 * math.sin(phase), .14)
    reach_arm("R", (1.60 if running else 1.50) - .06 * math.sin(phase), .09)
    rotate_world("spine", (1, 0, 0), .12 if running else .08)
    rotate_world("chest", (0, 0, 1), .06 * math.sin(phase))
    rotate_world("head", (0, 1, 0), .04 * math.sin(phase * 2))
    rig.pose.bones["pelvis"].location.y = (.022 if running else .012) * (1 - math.cos(phase * 2))


def key_pose(t, keys):
    # A simple time-addressable authored curve. No gameplay callback or physical
    # movement is encoded into the asset; JS uses its own visible windup window.
    for index in range(len(keys) - 1):
        a, b = keys[index], keys[index + 1]
        if t <= b[0]:
            amount = max(0, min(1, (t - a[0]) / (b[0] - a[0])))
            return [a[n] + (b[n] - a[n]) * amount for n in range(1, len(a))]
    return keys[-1][1:]


def attack(t):
    # A right-handed punch: visible preparation, quick extension, short contact
    # pose, then return to the arms-out reach. JS owns attack/impact timing.
    arm, elbow, lean, twist = key_pose(t, [
        (0, 1.50, .10, .08, 0), (.27, .80, .86, -.08, -.14),
        (.49, 1.78, -.06, .22, .13), (.64, 1.72, -.04, .18, .09),
        (1, 1.50, .10, .08, 0)])
    reach_arm("L", 1.45, .20, .40)
    reach_arm("R", arm, elbow, .38)
    rotate_world("chest", (1, 0, 0), lean)
    rotate_world("spine", (0, 0, 1), twist)
    rotate_world("head", (1, 0, 0), -.05 * math.sin(math.pi * t))


def stagger(t):
    lean, spread = key_pose(t, [(0, .05, 0), (.22, -.36, .20),
                              (.48, -.16, .12), (1, .07, 0)])
    arms(.38, .47, .25)
    rotate_world("chest", (1, 0, 0), lean)
    rotate_world("upper_arm.L", (0, 1, 0), spread)
    rotate_world("upper_arm.R", (0, 1, 0), -spread)
    rotate_world("head", (0, 1, 0), .12 * math.sin(math.pi * t))


def jump(t):
    amount = math.sin(math.pi * t)
    for suffix in ("L", "R"):
        rotate_world("thigh." + suffix, (1, 0, 0), -.36 * amount)
        rotate_world("shin." + suffix, (1, 0, 0), .62 * amount)
    arms(.55 * amount, .55 * amount, .15)
    rig.pose.bones["pelvis"].location.y = .02 * amount


clips = [("Idle", 75, idle), ("Walk", 60, lambda t: gait(t)),
         ("Run", 26, lambda t: gait(t, True)), ("Jump", 30, jump),
         ("ZombieIdle", 75, idle), ("Shamble", 60, lambda t: gait(t)),
         ("ZombieRun", 26, lambda t: gait(t, True)),
         ("Attack", 27, attack), ("Stagger", 21, stagger)]
for name, frames, sample in clips:
    author_clip(name, frames, sample)

rig.animation_data.action = bpy.data.actions["ZombieIdle"]
scene.frame_set(1)
scene.name = "Lastlight zombie motion — editable in-place clips"
for action in bpy.data.actions:
    action.use_fake_user = True
bpy.ops.object.select_all(action="DESELECT")
for obj in collection.all_objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active = rig
source_blend = PROJECT / "ArtSource/zombie_motion.blend"
source_blend.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(source_blend))
output = PROJECT / "Assets/models/zombie_animation.glb"
bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", use_selection=True,
    use_active_scene=True, export_yup=True, export_materials="EXPORT",
    export_animations=True, export_animation_mode="ACTIONS", export_force_sampling=True,
    export_frame_range=False, export_anim_slide_to_zero=True, export_cameras=False,
    export_lights=False, export_apply=False, export_skins=True)
notes = {"source": str(source.relative_to(PROJECT)),
         "editableSource": str(source_blend.relative_to(PROJECT)),
         "output": str(output.relative_to(PROJECT)),
         "clips": {name: frames / 30 for name, frames, _ in clips},
         "skeleton": "Original starter-pack hierarchy and joint-local bind axes, 17 deform bones",
         "presentation": {"ZombieIdle": "Arms extended forward with slight asymmetric breathing",
                          "Shamble": "Forward reaching arms, slow asymmetric gait",
                          "ZombieRun": "Forward reaching arms, faster asymmetric pursuit gait",
                          "Attack": "Right-fist windup to 0.243 s; extension peaks at 0.441 s; contact pose through 0.576 s; recovery to 0.9 s"},
         "limits": ["Placeholder authored clips, not motion capture", "No foot-plant IK",
                    "No dedicated bite, vault, death, or getting-up clips", "No root-motion travel",
                    "Same visual rig for the three gameplay variants; behaviour/speed differ"]}
(PROJECT / "Tools/zombie-motion.json").write_text(json.dumps(notes, indent=2) + "\n")
print("ZOMBIE_MOTION", json.dumps(notes), flush=True)
sys.stdout.flush()
sys.stderr.flush()
os._exit(0)
