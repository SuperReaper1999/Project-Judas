#include "TerrainCook.h"
#include "CollisionAsset.h"
#include "ModelArchive.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "TerrainAsset.h"
#include "TerrainGeneration.h"
#include "TerrainMesher.h"
#include "TerrainPlanetBinding.h"
#include "TerrainPreview.h"
#include "TerrainSource.h"
#include "TerrainTopology.h"
#include "authoring/AuthoringDocument.h"
#include "authoring/TerrainAuthoring.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
std::string Read(const std::string &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f || f.tellg() < 0 || f.tellg() > 96 * 1024 * 1024)
        throw std::runtime_error("missing/bounded terrain source: " + path);
    std::string bytes(size_t(f.tellg()), '\0');
    f.seekg(0);
    f.read(bytes.data(), bytes.size());
    if (!f)
        throw std::runtime_error("terrain source read failed");
    return bytes;
}
template <class T> T Number(const char *text) {
    T value{};
    const auto end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
        throw std::runtime_error("invalid terrain recipe number");
    return value;
}
} // namespace
bool ValidateTerrainWritePath(const std::string &path, std::string &error) {
    try {
        const auto file = Project::FindProjectFileFor(path);
        if (file.empty()) { error.clear(); return true; }
        Project project;
        if (!project.Load(file,error)) return false;
        const auto root = std::filesystem::weakly_canonical(project.RootDir());
        const auto relative = std::filesystem::weakly_canonical(std::filesystem::absolute(path)).lexically_relative(root);
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
            throw std::runtime_error("terrain destination leaves the project through a link; choose an owned source path");
        if (std::filesystem::exists(root / "mod-authoring.json") &&
            (*relative.begin() == "Kit" || *relative.begin() == "Dependencies"))
            throw std::runtime_error("published kit/dependency source is read-only; fork into this project's Authoring or Imports first");
        error.clear(); return true;
    } catch (const std::exception &e) { error=e.what(); return false; }
}
int TerrainV2AuthorCommand(int argc, char **argv) {
    try {
        if (argc < 3)
            throw std::runtime_error(
                "v2-validate SOURCE | v2-native SOURCE OUTPUT.judasmodel | "
                "v2-pack SOURCE OUTPUT.judasterrain | v2-cook SOURCE OUTPUT.judasmodel | "
                "v2-import V1_SOURCE NEW_V2_SOURCE");
        std::string error;
        const std::string command = argv[1];
        TerrainV2::Source source;
        if (command == "v2-planet") {
            if (argc != 8)
                throw std::runtime_error("v2-planet PLANET.json OUTPUT DIRECTION_X DIRECTION_Y "
                                         "DIRECTION_Z CELLS");
            PlanetSettings planet;
            if (!DecodePlanetSettings(Read(argv[2]), planet, error) ||
                !TerrainV2::CreatePlanetPatch(planet,
                    {Number<double>(argv[4]), Number<double>(argv[5]), Number<double>(argv[6])},
                    Number<unsigned>(argv[7]), source, error))
                throw std::runtime_error(error);
            const auto destination = std::filesystem::absolute(argv[3]);
            if (!ValidateTerrainWritePath(destination.string(), error))
                throw std::runtime_error(error);
            if (std::filesystem::exists(destination))
                throw std::runtime_error("Choose a new source path; existing data is preserved.");
            if (!PublishAuthoringOutputs(destination.parent_path().string(),
                    {{destination.string(), TerrainV2::Serialize(source), {}, false}}, error))
                throw std::runtime_error(error);
            std::cout << "created geographic terrain source " << destination
                      << " span_m=" << source.chart->span << "; use terrain.planet and the "
                         "derived geographic placement, not a flat mesh overlay\n";
            return 0;
        }
        if (command == "v2-new" || command == "v2-heightmap") {
            if (argc != 8)
                throw std::runtime_error(
                    "v2-new OUTPUT flat|highlands|island SAMPLES SPACING SEED HEIGHT | "
                    "v2-heightmap IMAGE OUTPUT SAMPLES SPACING MIN_HEIGHT MAX_HEIGHT");
            TerrainV2::LandscapeRecipe recipe;
            recipe.samples = Number<unsigned>(argv[4]);
            recipe.spacing = Number<float>(argv[5]);
            std::filesystem::path destination;
            if (command == "v2-new") {
                const std::string preset = argv[3];
                if (preset == "flat")
                    recipe.preset = TerrainV2::LandscapeRecipe::Preset::Flat;
                else if (preset == "highlands")
                    recipe.preset = TerrainV2::LandscapeRecipe::Preset::Highlands;
                else if (preset == "island")
                    recipe.preset = TerrainV2::LandscapeRecipe::Preset::Island;
                else
                    throw std::runtime_error("terrain preset must be flat, highlands or island");
                recipe.seed = Number<unsigned>(argv[6]);
                recipe.height = Number<float>(argv[7]);
                if (!TerrainV2::Generate(recipe, source, error))
                    throw std::runtime_error(error);
                destination = argv[2];
            } else {
                recipe.base = Number<float>(argv[6]);
                recipe.height = Number<float>(argv[7]) - recipe.base;
                if (!TerrainV2::ImportHeightmap(Read(argv[2]), recipe, source, error))
                    throw std::runtime_error(error);
                destination = argv[3];
            }
            destination = std::filesystem::absolute(destination);
            if (!ValidateTerrainWritePath(destination.string(),error)) throw std::runtime_error(error);
            if (std::filesystem::exists(destination))
                throw std::runtime_error(
                    "choose a new terrain source path; existing data is never overwritten");
            const auto text = TerrainV2::Serialize(source);
            if (text.size() > 64 * 1024 * 1024)
                throw std::runtime_error(
                    "generated editable JSON exceeds the 64 MiB source budget; "
                    "choose fewer samples or split the landscape into regions");
            if (!PublishAuthoringOutputs(destination.parent_path().string(),
                                         {{destination.string(), text, {}, false}}, error))
                throw std::runtime_error(error);
            std::cout << "created terrain source " << destination << '\n';
            return 0;
        }
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
            if (!ValidateTerrainWritePath(destination.string(),error)) throw std::runtime_error(error);
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
        if (!ValidateTerrainWritePath(argv[2],error)) throw std::runtime_error(error);
        if (argc > 3 && !ValidateTerrainWritePath(argv[3],error)) throw std::runtime_error(error);
        if (command == "v2-pack") {
            if (argc != 4)
                throw std::runtime_error("v2-pack SOURCE OUTPUT.judasterrain");
            std::vector<uint8_t> native;
            if (!TerrainV2::EncodeAsset(source, native, error))
                throw std::runtime_error(error);
            auto destination = std::filesystem::absolute(argv[3]);
            const bool existed = std::filesystem::exists(destination);
            if (!PublishAuthoringOutputs(
                    destination.parent_path().string(),
                    {{destination.string(),
                      std::string(reinterpret_cast<const char *>(native.data()), native.size()),
                      existed ? Read(destination.string()) : std::string{}, existed},
                     {std::filesystem::absolute(argv[2]).string(), capturedSourceBytes,
                      capturedSourceBytes, true}},
                    error))
                throw std::runtime_error(error);
            std::cout << "native terrain packed " << native.size() << " bytes\n";
            return 0;
        }
        const bool nativeMode = command == "v2-native";
        if ((command != "v2-cook" && !nativeMode) || argc != 4)
            throw std::runtime_error(
                "v2-native SOURCE OUTPUT.judasmodel | v2-cook SOURCE OUTPUT.judasmodel");
        if (source.chart && !nativeMode)
            throw std::runtime_error("Projected terrain requires v2-native; the older full-mesh "
                                     "script bootstrap cannot own planetary replacement.");
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
        if (nativeMode) {
            if (!TerrainV2::BuildPreviewMesh(source, mesh, error))
                throw std::runtime_error(error);
        } else {
            std::vector<TerrainV2::ChunkKey> keys;
            for (unsigned z = 0; z < source.ChunksZ(); ++z)
                for (unsigned x = 0; x < source.ChunksX(); ++x)
                    keys.push_back({x, z});
            std::vector<TerrainV2::Chunk> prepared;
            if (!TerrainV2::BuildChunks(source, keys, prepared, error))
                throw std::runtime_error(error);
            for (auto &c : prepared) {
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
        }
        std::vector<uint8_t> bytes;
        if (!EncodeModelArchive(mesh, bytes, error))
            throw std::runtime_error(error);
        outputs[0].bytes.assign(bytes.begin(), bytes.end());
        if (!nativeMode) {
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
        }
        outputs[2].bytes =
            nativeMode
                ? "// Native terrain is initialized by terrain.asset.\nexport default null;\n"
                : "export default " + TerrainV2::Serialize(source) + ";\n";
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
        auto nativePath = modelPath;
        nativePath.replace_extension(".judasterrain");
        std::vector<uint8_t> nativeData;
        if (!TerrainV2::EncodeAsset(source, nativeData, error))
            throw std::runtime_error(error);
        const bool hadNative = std::filesystem::exists(nativePath);
        outputs.push_back(
            {nativePath.string(),
             std::string(reinterpret_cast<const char *>(nativeData.data()), nativeData.size()),
             hadNative ? Read(nativePath.string()) : std::string{}, hadNative});
        outputs[3].bytes =
            "{\"format\":\"JudasTerrainV2Cook\",\"version\":2,\"representation\":\"" +
            std::string(nativeMode ? "native" : "full-mesh") + "\",\"source\":\"" +
            SceneFingerprintSha256(sourceBytes) + "\",\"model\":\"" +
            SceneFingerprintSha256(outputs[0].bytes) + "\",\"preview\":\"" +
            SceneFingerprintSha256(outputs[4].bytes) + "\",\"native\":\"" +
            SceneFingerprintSha256(outputs.back().bytes) + "\",\"collision\":" +
            (nativeMode ? "null" : "\"" + SceneFingerprintSha256(outputs[1].bytes) + "\"") + "}\n";
        // Guard source and every prior product before publishing the set. The
        // standard authoring journal rolls back failures; no half-new preview
        // or gameplay module can silently replace the last good products.
        outputs.push_back(
            {std::filesystem::absolute(argv[2]).string(), sourceBytes, sourceBytes, true});
        if (nativeMode)
            outputs.erase(outputs.begin() + 1);
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
