"""Optional Blender preview of the exported art; does not modify source models."""
from pathlib import Path
import bpy
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def model(name, position, scale=1):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(root / "Assets/models" / (name + ".glb")))
    added = set(bpy.data.objects) - before
    for obj in added:
        if obj.parent is None:
            obj.location += Vector(position)
            obj.scale *= scale


model("home", (0, 2, 0))
model("tree_pine", (-7, 3, 0))
model("tree_birch", (7, 4, 0))
model("rock_cluster", (-6, -4, 0))
model("fence", (6, -3, 0.75))
model("wood_crate", (4, -4, 0))
model("tool_rack", (1, -4, 0))
for x, name in [(-3, "axe"), (-1.5, "shovel"), (0, "pickaxe"), (2, "gravity_gun")]:
    model(name, (x, -7, 0.5), 1.5)
bpy.ops.mesh.primitive_plane_add(size=200)
plane = bpy.context.object
plane.name = "Preview ground only"
mat = bpy.data.materials.new("Preview ground")
mat.diffuse_color = (0.14, 0.22, 0.19, 1)
plane.data.materials.append(mat)
bpy.ops.object.camera_add(location=(18, -25, 17))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, 1.8)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 25
scene = bpy.context.scene
scene.camera = camera
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.studio_light = "paint.sl"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "BOTH"
scene.display.shading.curvature_ridge_factor = 1.4
scene.display.shading.curvature_valley_factor = 1.1
scene.display.shading.show_object_outline = True
scene.display.shading.background_type = "WORLD"
scene.world.color = (0.12, 0.17, 0.19)
scene.render.resolution_x = 1280
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(root / "Assets/art_sources/art_preview.png")
bpy.ops.render.render(write_still=True)
