#pragma once

#include <string>

#include "MeshData.h"

// Loads a single static mesh from an OBJ file on disk into a Judas-owned
// MeshData — see docs/ARCHITECTURE.md, "Milestone 9," for why OBJ +
// tinyobjloader was chosen over a glTF/FBX/custom-format importer. Pure
// CPU-side: no OpenGL context is required (or touched) to call this, so it
// can run — and is tested — headlessly (see tests/AssetTests.cpp).
//
// Deliberately supports only what M9 needs: one shape's worth of
// triangulated position/normal/uv geometry. An OBJ with multiple objects/
// groups has all of them flattened into one MeshData (Judas has no scene-
// hierarchy concept to import into — see "Deliberately Not Implemented").
// Materials, if the file references an MTL, are ignored entirely; texture
// binding is Judas's own explicit, separate step (see TextureLoader.h),
// not something an imported file's material assignment drives.
//
// Returns false and fills `outError` with a human-readable message
// (including the offending path) on any failure — missing file, unparsable
// OBJ, or an OBJ with no triangle data — never partially fills `outMesh` or
// invents placeholder geometry.
bool LoadObjMesh(const std::string& path, MeshData& outMesh, std::string& outError);

// Milestone 31: the same parse over bytes already in memory (an
// asynchronous file read's result), so a worker thread never needs the
// file system itself here. `nameForErrors` labels messages. Materials are
// ignored exactly as in LoadObjMesh.
bool ParseObjMesh(const char* data, std::size_t size, const std::string& nameForErrors, MeshData& outMesh,
                  std::string& outError);
