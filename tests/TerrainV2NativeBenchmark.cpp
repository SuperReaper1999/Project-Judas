#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Native source IO, complete scene construction and real dirty collision work.
// This is a measurement driver, not a timing pass/fail gate. File reads are
// warm; the caller may measure peak process RSS independently.
namespace {
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
void Require(bool ok, const std::string &error) {
    if (!ok)
        throw std::runtime_error(error);
}
} // namespace
int main(int argc, char **argv) {
    try {
        namespace fs = std::filesystem;
        if (argc != 2 || fs::exists(argv[1]))
            throw std::runtime_error("provide a fresh benchmark output directory");
        const fs::path root(argv[1]);
        fs::create_directories(root / "Assets");
        auto source = TerrainV2::Create(513, 513, 2);
        source.bottom = -256;
        source.top = 256;
        for (unsigned z = 0; z < source.samplesZ; ++z)
            for (unsigned x = 0; x < source.samplesX; ++x)
                source.heights[size_t(z) * source.samplesX + x] =
                    30 + 12 * std::sin(float(x) * .035f) * std::cos(float(z) * .027f);
        std::string error;
        std::vector<uint8_t> bytes;
        auto start = Clock::now();
        Require(TerrainV2::EncodeAsset(source, bytes, error), error);
        const auto encodeMs = Ms(start);
        const auto path = root / "Assets/landscape.judasterrain";
        {
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            Require(bool(file), "write native benchmark fixture");
        }
        AssetDatabase assets;
        assets.Scan(root.string(), (root / "Assets").string());
        AssetRecord asset;
        Require(assets.Track(path.string(), asset, error), error);
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        Scene scene;
        auto &owner = scene.CreateObject("One kilometre native landscape");
        owner.body = SceneBodyComponent{};
        owner.landscape = SceneLandscapeSettings{asset.id};
        std::cout << "{\"case\":\"fixture\",\"samples\":263169,\"extent_m\":1024,"
                  << "\"native_bytes\":" << bytes.size() << ",\"encode_ms\":" << encodeMs << "}\n";
        for (unsigned run = 0; run < 5; ++run) {
            TerrainV2::AssetPages reader;
            start = Clock::now();
            Require(reader.Open(path.string(), error), error);
            const auto openMs = Ms(start);
            TerrainV2::Source decoded;
            start = Clock::now();
            Require(reader.ReadAll(decoded, error), error);
            const auto readMs = Ms(start);
            Require(decoded.heights == source.heights && decoded.weights == source.weights,
                    "native sample readback differs");
            RuntimeWorld world;
            world.legacyGameplay = false;
            start = Clock::now();
            Require(world.Build(scene, &resources, error), error);
            const auto buildMs = Ms(start);
            const auto initial = *world.LiveTerrain().Status(1);
            Require(initial.ready && initial.chunks == 1024, "initial native collision incomplete");
            const auto hit = world.Physics().Raycast({0, 100, 0}, {0, -1, 0}, 200);
            Require(hit.hit, "native terrain has no real collision");
            TerrainV2::SurfaceBrush surface;
            surface.radius = 6;
            surface.strength = .25f;
            start = Clock::now();
            Require(world.LiveTerrain().Surface(1, hit.point, surface, error), error);
            Require(world.LiveTerrain().Wait(1, error), error);
            const auto sculptMs = Ms(start);
            auto raised = world.Physics().Raycast({0, 100, 0}, {0, -1, 0}, 200);
            Require(raised.hit && std::abs(raised.point.y - hit.point.y - .25f) < .001f,
                    "runtime sculpt did not change actual support");
            surface.kind = TerrainV2::SurfaceBrush::Kind::Paint;
            surface.layer = 1;
            surface.strength = 1;
            start = Clock::now();
            Require(world.LiveTerrain().Surface(1, hit.point, surface, error), error);
            Require(world.LiveTerrain().Wait(1, error), error);
            const auto paintMs = Ms(start);
            start = Clock::now();
            Require(world.LiveTerrain().Brush(1, hit.point, {VolumeBrushKind::Remove, 6, 1}, error),
                    error);
            Require(world.LiveTerrain().Wait(1, error), error);
            const auto editMs = Ms(start);
            const auto edited = *world.LiveTerrain().Status(1);
            const auto excavated = world.Physics().Raycast({0, 100, 0}, {0, -1, 0}, 200);
            Require(excavated.hit && excavated.point.y < hit.point.y - 2,
                    "native benchmark edit did not alter real collision");
            std::string snapshot;
            start = Clock::now();
            Require(world.CaptureRuntimeTerrain(1, snapshot, error), error);
            const auto saveMs = Ms(start);
            std::cout << "{\"case\":\"native-scene\",\"run\":" << run
                      << ",\"index_open_ms\":" << openMs << ",\"sample_read_ms\":" << readMs
                      << ",\"scene_build_ms\":" << buildMs
                      << ",\"initial_prepare_ms\":" << initial.prepareMs
                      << ",\"initial_publish_ms\":" << initial.publishMs
                      << ",\"edit_complete_ms\":" << editMs
                      << ",\"sculpt_complete_ms\":" << sculptMs
                      << ",\"paint_complete_ms\":" << paintMs
                      << ",\"edit_prepare_ms\":" << edited.prepareMs
                      << ",\"edit_publish_ms\":" << edited.publishMs << ",\"capture_ms\":" << saveMs
                      << ",\"snapshot_bytes\":" << snapshot.size()
                      << ",\"resident_chunks\":" << edited.chunks
                      << ",\"resident_volume_chunks\":" << edited.volumeChunks
                      << ",\"estimated_geometry_sample_bytes\":" << edited.bytes << "}\n";
        }
        resources.Shutdown();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
