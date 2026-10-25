"""Apply Prompt3D's verified PBR finish to original, static Prompt3D GLBs.

This deliberately rejects rigs and unsupported layouts rather
than damaging them. The animated player model is retained unchanged.
"""

import json
import struct
import numpy as np


def finish_static(path, G, to_glb):
    raw = path.read_bytes()
    size, kind = struct.unpack_from("<II", raw, 12)
    if kind != 0x4E4F534A:
        raise ValueError("expected GLB JSON chunk")
    gltf = json.loads(raw[20 : 20 + size])
    binary = raw[28 + size :]
    if gltf.get("skins") or gltf.get("animations"):
        raise ValueError("static finishing must not rebuild an animated model")

    def accessor(index):
        a = gltf["accessors"][index]
        v = gltf["bufferViews"][a["bufferView"]]
        if "byteStride" in v or "sparse" in a:
            raise ValueError("unsupported accessor layout")
        dtype = {5126: "<f4", 5125: "<u4", 5123: "<u2"}[a["componentType"]]
        width = {"VEC3": 3, "SCALAR": 1}[a["type"]]
        offset = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        return (
            np.frombuffer(binary, dtype=dtype, count=a["count"] * width, offset=offset)
            .copy()
            .reshape((-1, width))
        )

    scene = G.Scene(path.stem)
    for index, m in enumerate(gltf["materials"]):
        pbr = m["pbrMetallicRoughness"]
        linear = pbr.get("baseColorFactor", [1, 1, 1, 1])
        rgb = [
            round(
                (12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055) * 255
            )
            for c in linear[:3]
        ]
        name = str(index) + "_" + m.get("name", "material")
        scene.material(
            name,
            "#" + "".join(f"{c:02x}" for c in rgb),
            roughness=pbr.get("roughnessFactor", 1),
            metallic=pbr.get("metallicFactor", 0),
            alpha=linear[3],
        )
    materials = list(scene.materials)

    def local_matrix(node):
        if "matrix" in node:
            return np.array(node["matrix"]).reshape((4, 4), order="F")
        x, y, z, w = node.get("rotation", [0, 0, 0, 1])
        rotation = np.array(
            [
                [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
            ]
        )
        matrix = np.eye(4)
        matrix[:3, :3] = rotation @ np.diag(node.get("scale", [1, 1, 1]))
        matrix[:3, 3] = node.get("translation", [0, 0, 0])
        return matrix

    visited = set()

    def visit(index, parent):
        if index in visited:
            raise ValueError("repeated/cyclic static node graph")
        visited.add(index)
        node = gltf["nodes"][index]
        matrix = parent @ local_matrix(node)
        if "mesh" in node:
            for p in gltf["meshes"][node["mesh"]]["primitives"]:
                attrs = p["attributes"]
                if p.get("mode", 4) != 4:
                    raise ValueError("only triangle primitives are supported")
                primitive = G.Prim(
                    accessor(attrs["POSITION"]),
                    accessor(attrs["NORMAL"]),
                    accessor(p["indices"]).reshape((-1, 3)),
                )
                scene.add(
                    "mesh_" + str(index),
                    materials[p["material"]],
                    primitive,
                    matrix[:3, :3],
                    matrix[:3, 3],
                )
        for child in node.get("children", []):
            visit(child, matrix)

    for index in gltf["scenes"][gltf.get("scene", 0)]["nodes"]:
        visit(index, np.eye(4))
    to_glb(scene, str(path), textures=True, contrast=1, flat=False)
    return scene
