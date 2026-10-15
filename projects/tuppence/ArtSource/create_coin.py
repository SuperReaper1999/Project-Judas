"""Original stylised copper 2p token; no Royal Mint artwork or textures."""
import bpy,math
from pathlib import Path
P=Path(__file__).resolve().parents[1];A=P/'Assets/models'
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
mat=bpy.data.materials.new('Copper');mat.diffuse_color=(.64,.25,.10,1);mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=mat.diffuse_color;bs.inputs['Metallic'].default_value=.6;bs.inputs['Roughness'].default_value=.32
bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.21,depth=.04);body=bpy.context.object;body.name='Copper coin';body.data.materials.append(mat)
# Symmetric authored coordinates preserve the intended exact parallel edge families.
for vertex in body.data.vertices:
 vertex.co.x=round(vertex.co.x,7);vertex.co.y=round(vertex.co.y,7)
bpy.ops.export_scene.gltf(filepath=str(A/'coin_collider.glb'),export_format='GLB',use_selection=True)
bpy.ops.object.text_add(location=(-.145,-.07,.021));label=bpy.context.object;label.data.body='2p';label.data.size=.19;label.data.extrude=.0015;label.data.materials.append(mat);bpy.ops.object.convert(target='MESH');bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=body;bpy.ops.object.join()
bpy.ops.wm.save_as_mainfile(filepath=str(P/'ArtSource/coin.blend'))
bpy.ops.export_scene.gltf(filepath=str(A/'coin.glb'),export_format='GLB',use_selection=True)
