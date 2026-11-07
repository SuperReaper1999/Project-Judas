#include "TerrainCook.h"
#include "CollisionAsset.h"
#include "ModelArchive.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "TerrainPreview.h"
#include "TerrainSource.h"
#include "TerrainTopology.h"
#include "authoring/AuthoringDocument.h"
#include "authoring/TerrainAuthoring.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
std::string Read(const std::string &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f || f.tellg() < 0 || f.tellg() > 64 * 1024 * 1024)
        throw std::runtime_error("missing/bounded terrain source: " + path);
    std::string bytes(size_t(f.tellg()), '\0');
    f.seekg(0);
    f.read(bytes.data(), bytes.size());
    if (!f)
        throw std::runtime_error("terrain source read failed");
    return bytes;
}
} // namespace
int TerrainV2AuthorCommand(int argc, char **argv) {
    try {
        if (argc < 3)
            throw std::runtime_error("v2-validate SOURCE | v2-cook SOURCE OUTPUT.judasmodel | "
                                     "v2-import V1_SOURCE NEW_V2_SOURCE");
        std::string error;
        const std::string command = argv[1];
        TerrainV2::Source source;
        if (command == "v2-import") {
            if (argc != 4 || std::filesystem::exists(argv[3]))
                throw std::runtime_error(
                    "v2-import requires a new destination; originals are preserved");
            TerrainSource old;
            if (!ParseTerrain(Read(argv[2]), old, error))
                throw std::runtime_error(error);
            const float spacing = old.width / (old.nx - 1);
            if (std::abs(old.depth / (old.nz - 1) - spacing) > spacing * 1e-5f ||
                (old.nx - 1) % 16 || (old.nz - 1) % 16)
                throw std::runtime_error("V1 import currently requires a square sample lattice "
                                         "aligned to 16 cells; resampling must be explicit");
            source = TerrainV2::Create(old.nx, old.nz, spacing);
            source.heights = old.heights;
            source.weights.clear();
            source.bottom = *std::min_element(old.heights.begin(), old.heights.end()) - 8 * spacing;
            source.top = *std::max_element(old.heights.begin(), old.heights.end()) + 8 * spacing;
            for (auto w : old.weights)
                source.weights.emplace_back(w[0], w[1], w[2], w[3]);
            for (unsigned i = 0; i < old.layers.size(); ++i) {
                if (std::abs(old.layers[i].tileMetres - 4) > 1e-6f)
                    throw std::runtime_error("V1 import currently requires 4-metre material "
                                             "tiling; originals are unchanged");
                source.layerTextures[i] = old.layers[i].texture;
                source.layerColours[i] = glm::vec3(1);
            }
            const auto destination = std::filesystem::absolute(argv[3]);
            if (!PublishAuthoringOutputs(
                    destination.parent_path().string(),
                    {{destination.string(), TerrainV2::Serialize(source), {}, false}}, error))
                throw std::runtime_error(error);
            return 0;
        }
        const auto capturedSourceBytes = Read(argv[2]);
        if (!TerrainV2::Parse(capturedSourceBytes, source, error))
            throw std::runtime_error(error);
        if (command == "v2-validate") {
            std::cout << "valid terrain V2 " << source.ChunksX() << " x " << source.ChunksZ()
                      << " chunks\n";
            return 0;
        }
        if (command != "v2-cook" || argc != 4)
            throw std::runtime_error("v2-cook SOURCE OUTPUT.judasmodel");
        auto modelPath = std::filesystem::absolute(argv[3]);
        auto collisionPath = modelPath;
        collisionPath.replace_extension(".judascollision");
        auto module = modelPath.parent_path() / "terrain-source.js";
        auto previewPath = modelPath;
        previewPath.replace_extension(".preview.tga");
        auto receipt = modelPath;
        receipt.replace_extension(".terrain-v2-receipt.json");
        const auto &sourceBytes = capturedSourceBytes;
        std::vector<AuthoringOutput> outputs;
        for (const auto &path : std::vector<std::filesystem::path>{modelPath, collisionPath, module,
                                                                   receipt, previewPath}) {
            const bool exists = std::filesystem::exists(path);
            outputs.push_back(
                {path.string(), {}, exists ? Read(path.string()) : std::string{}, exists});
        }
        MeshData mesh;
        for (unsigned z = 0; z < source.ChunksZ(); ++z)
            for (unsigned x = 0; x < source.ChunksX(); ++x) {
                TerrainV2::Chunk c;
                if (!TerrainV2::BuildChunk(source, {x, z}, c, error))
                    throw std::runtime_error(error);
                const auto offset = unsigned(mesh.vertices.size());
                for (auto v : c.mesh.vertices) {
                    v.position += c.anchor;
                    v.uv = v.uv1; // Ordinary preview atlas; native terrain uses tiled layers.
                    mesh.vertices.push_back(v);
                }
                for (auto i : c.mesh.indices)
                    mesh.indices.push_back(offset + i);
            }
        if (!source.edits.empty())
            TerrainV2::ConformChunkTopology(mesh, source.spacing);
        mesh.primitives.emplace_back(0, unsigned(mesh.indices.size()), -1, "terrain");
        std::vector<uint8_t> bytes;
        if (!EncodeModelArchive(mesh, bytes, error))
            throw std::runtime_error(error);
        outputs[0].bytes.assign(bytes.begin(), bytes.end());
        CollisionAsset collision;
        if (!CookCollision(mesh, {}, collision, error))
            throw std::runtime_error(error);
        struct Temporary {
            std::filesystem::path path =
                std::filesystem::temp_directory_path() /
                ("judas-terrain-cook-" + MintAssetId() + ".judascollision");
            ~Temporary() {
                std::error_code ec;
                std::filesystem::remove(path, ec);
            }
        } temporary;
        if (!SaveCollisionAsset(temporary.path.string(), collision, error))
            throw std::runtime_error(error);
        outputs[1].bytes = Read(temporary.path.string());
        outputs[2].bytes = "export default " + TerrainV2::Serialize(source) + ";\n";
        outputs[3].bytes = "{\"format\":\"JudasTerrainV2Cook\",\"version\":1,\"source\":\"" +
                           SceneFingerprintSha256(sourceBytes) + "\",\"model\":\"" +
                           SceneFingerprintSha256(outputs[0].bytes) + "\",\"collision\":\"" +
                           SceneFingerprintSha256(outputs[1].bytes) + "\"}\n";
        AssetDatabase assets;
        Project project;
        const auto projectFile = Project::FindProjectFileFor(argv[2]);
        if (!projectFile.empty()) {
            if (!project.Load(projectFile, error))
                throw std::runtime_error(error);
            assets.Scan(project.RootDir(), project.AssetsDir());
        }
        TextureData preview;
        if (!TerrainV2::BuildPreviewTexture(source, projectFile.empty() ? nullptr : &assets,
                                            preview, error))
            throw std::runtime_error(error);
        outputs[4].bytes = TerrainV2::EncodePreviewTga(preview);
        // Guard source and every prior product before publishing the set. The
        // standard authoring journal rolls back failures; no half-new preview
        // or gameplay module can silently replace the last good products.
        outputs.push_back(
            {std::filesystem::absolute(argv[2]).string(), sourceBytes, sourceBytes, true});
        if (!PublishAuthoringOutputs(modelPath.parent_path().string(), outputs, error))
            throw std::runtime_error(error);
        std::cout << "terrain V2 cooked " << mesh.vertices.size() << " vertices "
                  << mesh.indices.size() / 3 << " triangles\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
