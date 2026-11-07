#include "Project.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainCook.h"
#include "terrain/TerrainGeneration.h"
#include "terrain/TerrainMesher.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
} // namespace
int main() {
    try {
        std::string error;
        TerrainV2::LandscapeRecipe recipe;
        TerrainV2::Source source, again;
        Check(TerrainV2::Generate(recipe, source, error), "generate editable highlands: " + error);
        Check(TerrainV2::Generate(recipe, again, error) &&
                  TerrainV2::Serialize(source) == TerrainV2::Serialize(again),
              "same seed and recipe reproduce source exactly on this build");
        recipe.seed++;
        Check(TerrainV2::Generate(recipe, again, error) && source.heights != again.heights,
              "seed controls actual landscape shape");
        recipe.preset = TerrainV2::LandscapeRecipe::Preset::Flat;
        Check(TerrainV2::Generate(recipe, again, error), "generate flat terrain");
        Check(std::all_of(again.heights.begin(), again.heights.end(),
                          [&](float h) { return h == recipe.base; }),
              "flat preset has exact selected base height");
        Check(again.weights.front() == glm::vec4(1, 0, 0, 0) && again.top >= 4096 &&
                  again.bottom <= -4096,
              "flat starts with one clear material and practical sculpting headroom");
        recipe.preset = TerrainV2::LandscapeRecipe::Preset::Island;
        Check(TerrainV2::Generate(recipe, again, error) && again.heights.front() < 0 &&
                  TerrainV2::Height(again, 0, 0) > 2,
              "island has low coastline and a raised interior");
        std::vector<uint8_t> native;
        Check(
            TerrainV2::EncodeAsset(again, native, error) &&
                TerrainV2::DecodeAsset(
                    {reinterpret_cast<const char *>(native.data()), native.size()}, source, error),
            "generated terrain uses ordinary native asset encoding");
        const auto before = TerrainV2::Serialize(source);
        recipe.samples = 19;
        Check(!TerrainV2::Generate(recipe, source, error) && TerrainV2::Serialize(source) == before,
              "bad lattice preserves previous source");
        recipe.samples = 129;
        recipe.height = std::numeric_limits<float>::quiet_NaN();
        Check(!TerrainV2::Generate(recipe, source, error) && TerrainV2::Serialize(source) == before,
              "nonfinite recipe preserves previous source");
        // Original synthetic grayscale16 PNG; no texture conversion or external asset.
        const unsigned char image[] = {
            137, 80,  78,  71,  13,  10, 26, 10, 0,  0,  0,  13,  73,  72,  68, 82, 0,  0,  0,
            2,   0,   0,   0,   2,   16, 0,  0,  0,  0,  7,  77,  142, 187, 0,  0,  0,  16, 73,
            68,  65,  84,  120, 156, 99, 96, 96, 96, 96, 4,  194, 255, 255, 1,  3,  17, 2,  1,
            218, 231, 163, 127, 0,   0,  0,  0,  73, 69, 78, 68,  174, 66,  96, 130};
        recipe.samples = 17;
        recipe.base = 0;
        recipe.height = 65535;
        const std::string_view bytes(reinterpret_cast<const char *>(image), sizeof(image));
        Check(TerrainV2::ImportHeightmap(bytes, recipe, source, error),
              "import grayscale16 without RGBA8 conversion: " + error);
        Check(source.heights[0] == 0 && source.heights[16] == 1 && source.heights[16 * 17] == 256 &&
                  source.heights.back() == 65535,
              "16-bit one-unit differences and image row orientation survive");
        Check(source.heights[8 * 17 + 8] == 16448,
              "explicit bilinear resampling matches independent corner average");
        Check(source.samplesX == 17 && source.spacing == recipe.spacing,
              "imported image uses explicitly chosen physical lattice");
        const auto imported = TerrainV2::Serialize(source);
        Check(!TerrainV2::ImportHeightmap("not an image", recipe, source, error) &&
                  TerrainV2::Serialize(source) == imported,
              "malformed heightmap preserves source");
        auto flat = TerrainV2::Create(17, 17, 1);
        std::vector<TerrainV2::Chunk> chunks;
        Check(TerrainV2::BuildChunks(flat, {{0, 0}}, chunks, error, {}, 512),
              "exact batch triangle budget permits valid geometry");
        Check(!TerrainV2::BuildChunks(flat, {{0, 0}}, chunks, error, {}, 511) &&
                  chunks.size() == 1 && chunks[0].mesh.indices.size() == 1536,
              "failed bounded batch preserves previous complete product");
        namespace fs = std::filesystem;
        const auto root =
            fs::temp_directory_path() /
            ("judas-terrain-write-policy-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Project project;
        Check(Project::CreateNew(root.string(), "Terrain policy", project, error),
              "create isolated policy fixture");
        fs::create_directories(root / "Kit");
        fs::create_directories(root / "Authoring");
        {
            std::ofstream marker(root / "mod-authoring.json");
            marker << "{}";
        }
        Check(!ValidateTerrainWritePath((root / "Kit/locked.judasterrain2").string(), error),
              "V2 and V1 share published-kit write protection");
        Check(
            !ValidateTerrainWritePath((root / "Dependencies/locked.judasterrain2").string(), error),
            "dependency source cannot be overwritten");
        Check(ValidateTerrainWritePath((root / "Authoring/own.judasterrain2").string(), error),
              "owned new source remains writable");
        std::error_code linkError;
        fs::create_directory_symlink(root / "Kit", root / "Authoring/link", linkError);
        if (!linkError)
            Check(!ValidateTerrainWritePath((root / "Authoring/link/locked.judasterrain2").string(),
                                            error),
                  "symlink cannot bypass published source protection");
        else
            std::cout << "NOT EXERCISED symlink write policy: " << linkError.message() << '\n';
        std::vector<std::string> args = {
            "terrain", "v2-new", (root / "Kit/locked.judasterrain2").string(), "flat", "17", "1",
            "7",       "4"};
        std::vector<char *> argv;
        for (auto &arg : args)
            argv.push_back(arg.data());
        Check(TerrainV2AuthorCommand(int(argv.size()), argv.data()) != 0 &&
                  !fs::exists(root / "Kit/locked.judasterrain2"),
              "real writer command rejects protected publication without creating files");
        fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
