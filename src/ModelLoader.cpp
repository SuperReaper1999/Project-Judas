#include "ModelLoader.h"

#include <sstream>

// tiny_obj_loader.h defines a couple of internal helper functions this
// project's own call path never reaches (parseTriple and friends — used
// only by API surface Judas doesn't call), which -Wunused-function then
// flags. A lint characteristic of a third-party header we don't control,
// not of this project's code — suppressed only for this one include,
// matching the existing stb_image_write.h precedent in TestHarness.cpp.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define TINYOBJLOADER_IMPLEMENTATION
#include "../third_party/tiny_obj_loader.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace {
bool ConvertShapes(const tinyobj::attrib_t& attrib, const std::vector<tinyobj::shape_t>& shapes, const std::string& name,
                   MeshData& outMesh, std::string& outError);
}

bool ParseObjMesh(const char* data, std::size_t size, const std::string& nameForErrors, MeshData& outMesh,
                  std::string& outError) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    std::istringstream stream(std::string(data, size));
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &stream, nullptr)) {
        outError = "Failed to parse OBJ model '" + nameForErrors + "': " + (err.empty() ? "unknown parse error" : err);
        return false;
    }
    return ConvertShapes(attrib, shapes, nameForErrors, outMesh, outError);
}

bool LoadObjMesh(const std::string& path, MeshData& outMesh, std::string& outError) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;  // ignored — see header comment
    std::string warn;
    std::string err;

    // Triangulation defaults to on (tinyobjloader's own default), which is
    // exactly what a "triangle topology, no scene hierarchy" importer needs
    // — an OBJ with quad/n-gon faces still comes back as triangles with no
    // extra code here.
    const bool parsed =
        tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

    if (!parsed) {
        outError = "Failed to load OBJ model '" + path + "': " +
                   (err.empty() ? "unknown parse error" : err);
        return false;
    }
    return ConvertShapes(attrib, shapes, path, outMesh, outError);
}

namespace {
bool ConvertShapes(const tinyobj::attrib_t& attrib, const std::vector<tinyobj::shape_t>& shapes, const std::string& path,
                   MeshData& outMesh, std::string& outError) {
    outMesh.vertices.clear();
    outMesh.indices.clear();

    for (const tinyobj::shape_t& shape : shapes) {
        for (const tinyobj::index_t& index : shape.mesh.indices) {
            MeshVertex vertex;

            if (index.vertex_index >= 0) {
                vertex.position = glm::vec3(attrib.vertices[3 * index.vertex_index + 0],
                                             attrib.vertices[3 * index.vertex_index + 1],
                                             attrib.vertices[3 * index.vertex_index + 2]);
            }

            // Defensive fallback for a malformed/incomplete source file only
            // — this project's own beacon.obj always supplies real per-face
            // normals and UVs (see ModelLoader.h); a missing normal here
            // would otherwise leave lighting reading an uninitialized
            // direction, which is a worse failure than a visibly-flat +Y
            // placeholder for a file that shouldn't be shipped this way.
            if (index.normal_index >= 0) {
                vertex.normal = glm::vec3(attrib.normals[3 * index.normal_index + 0],
                                           attrib.normals[3 * index.normal_index + 1],
                                           attrib.normals[3 * index.normal_index + 2]);
            } else {
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (index.texcoord_index >= 0) {
                vertex.uv = glm::vec2(attrib.texcoords[2 * index.texcoord_index + 0],
                                       attrib.texcoords[2 * index.texcoord_index + 1]);
            }

            // Sequential, undeduplicated indices — this milestone's asset is
            // small enough (see beacon.obj) that vertex-sharing compaction
            // isn't worth the extra bookkeeping; what matters is that an
            // imported model always goes through the indexed (glDrawElements)
            // path, per MeshData's own convention, not that the index buffer
            // is maximally compact.
            outMesh.vertices.push_back(vertex);
            outMesh.indices.push_back(static_cast<std::uint32_t>(outMesh.indices.size()));
        }
    }

    if (outMesh.vertices.empty()) {
        outError = "OBJ model '" + path + "' parsed but contains no triangle data.";
        return false;
    }

    return true;
}
}  // namespace
