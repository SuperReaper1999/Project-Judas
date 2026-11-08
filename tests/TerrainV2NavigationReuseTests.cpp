#include "navigation/NavigationTileReuse.h"
#include "terrain/TerrainBrush.h"
#include "terrain/TerrainNavigation.h"
#include "terrain/TerrainPreparation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
std::shared_ptr<const NavigationData> Bake(const TerrainV2::NavigationInput &input) {
    std::string error;
    std::shared_ptr<const NavigationData> result;
    const auto ok = TerrainV2::PrepareNavigation(input, result, error);
    Check(ok, "prepare navigation: " + error);
    return result;
}
void Compare(const TerrainV2::NavigationInput &input, const NavigationData &actual) {
    auto full = input;
    full.previous.reset();
    full.previousChunks.clear();
    const auto expected = Bake(full);
    Check(actual.fingerprint == expected->fingerprint && actual.layers == expected->layers,
          "incremental compressed layers equal an independent full-coverage bake byte for byte");
}
void Geometry(const TerrainV2::Source &source, const std::vector<TerrainV2::ChunkKey> &keys,
              TerrainV2::NavigationInput &input) {
    TerrainV2::Preparation prepared;
    prepared.source = std::make_shared<const TerrainV2::Source>(source);
    Check(TerrainV2::PrepareGeometry(prepared, keys), "prepare changed physical geometry");
    for (const auto &part : prepared.chunks)
        input.chunks[part.chunk.key] = {part.chunk.anchor, part.collision};
}
void Kilometre() {
    // Same physical source, profile, sculpt and excavation as the measured
    // native benchmark. Full recomputation stays outside its timing driver.
    auto source = TerrainV2::Create(513, 513, 2);
    source.bottom = -256;
    source.top = 256;
    for (unsigned z = 0; z < source.samplesZ; ++z)
        for (unsigned x = 0; x < source.samplesX; ++x)
            source.heights[size_t(z) * source.samplesX + x] =
                30 + 12 * std::sin(float(x) * .035f) * std::cos(float(z) * .027f);
    TerrainV2::NavigationInput input;
    input.settings.halfExtents = {512, 64, 512};
    input.settings.cellSize = 1;
    input.settings.minRegion = 0;
    std::vector<TerrainV2::ChunkKey> all;
    for (unsigned z = 0; z < source.ChunksZ(); ++z)
        for (unsigned x = 0; x < source.ChunksX(); ++x)
            all.push_back({x, z});
    Geometry(source, all, input);
    auto result = Bake(input);
    const glm::vec3 point(0, TerrainV2::Height(source, 0, 0), 0);
    input.previous = result;
    input.previousChunks = input.chunks;
    TerrainV2::SurfaceBrush sculpt;
    sculpt.radius = 6;
    sculpt.strength = .25f;
    std::vector<TerrainV2::ChunkKey> dirty;
    std::string error;
    Check(TerrainV2::ApplySurfaceBrush(source, point, sculpt, dirty, error),
          "large benchmark sculpt changes real sampled support");
    Geometry(source, dirty, input);
    result = Bake(input);
    Compare(input, *result);
    input.previous = result;
    input.previousChunks = input.chunks;
    TerrainV2::Edit cut{TerrainV2::Edit::Kind::Remove, point, 6};
    source.edits.push_back(cut);
    Geometry(source, TerrainV2::Affected(source, cut), input);
    result = Bake(input);
    Compare(input, *result);
    Check(result->rebuiltTiles == 16 && result->reusedTiles == 1008,
          "the measured kilometre excavation replaces16dependent tiles and reuses1008");
    std::cout << "METRIC kilometre_full_bake_layers=" << result->layers.size()
              << " rebuilt_tiles=" << result->rebuiltTiles
              << " reused_tiles=" << result->reusedTiles << '\n';
}
} // namespace
int main() {
    try {
        auto source = TerrainV2::Create(129, 129, 1);
        source.bottom = -8;
        source.top = 16;
        std::fill(source.heights.begin(), source.heights.end(), 2);
        TerrainV2::NavigationInput current;
        current.settings.halfExtents = {64, 12, 64};
        current.settings.cellSize = .5f;
        current.settings.tileSize = 16;
        current.settings.minRegion = 0;
        std::vector<TerrainV2::ChunkKey> all;
        for (unsigned z = 0; z < source.ChunksZ(); ++z)
            for (unsigned x = 0; x < source.ChunksX(); ++x)
                all.push_back({x, z});
        Geometry(source, all, current);
        auto result = Bake(current);
        Check(result->rebuiltTiles == 256 && !result->reusedTiles && !result->layers.empty(),
              "initial full-coverage operator builds all tiles");
        // Several cuts cross physical chunks and navigation tile/filter borders.
        // Both cavities and source-height changes exercise erosion/layer changes.
        for (const auto point :
             {glm::vec3(0, 2, 0), glm::vec3(15.5f, 2, 8), glm::vec3(-31, 2, -32)}) {
            current.previous = result;
            current.previousChunks = current.chunks;
            TerrainV2::Edit cut{TerrainV2::Edit::Kind::Remove, point, 3};
            source.edits.push_back(cut);
            Geometry(source, TerrainV2::Affected(source, cut), current);
            result = Bake(current);
            Compare(current, *result);
            Check(result->layers != current.previous->layers,
                  "the edited fixture genuinely changes its physical navigation layers");
            Check(result->rebuiltTiles > 0 && result->rebuiltTiles < 100 &&
                      result->reusedTiles + result->rebuiltTiles == 256,
                  "local physical changes preserve most independent tiles");
            std::cout << "METRIC rebuilt_tiles=" << result->rebuiltTiles
                      << " reused_tiles=" << result->reusedTiles << '\n';
        }
        current.previous = result;
        current.previousChunks = current.chunks;
        TerrainV2::SurfaceBrush raise;
        raise.radius = 5;
        raise.strength = 2;
        std::vector<TerrainV2::ChunkKey> changed;
        std::string error;
        Check(TerrainV2::ApplySurfaceBrush(source, {40, 2, 40}, raise, changed, error),
              "raise real sampled support outside the earlier caves");
        Geometry(source, changed, current);
        result = Bake(current);
        Compare(current, *result);
        Check(result->layers != current.previous->layers && result->reusedTiles > 0,
              "sculpted heights invalidate their full physical footprint");
        current.previous = result;
        current.previousChunks = current.chunks;
        result = Bake(current);
        Compare(current, *result);
        Check(result->reusedTiles == 256 && result->rebuiltTiles == 0,
              "identical immutable cooks reuse the whole result, including empty tiles");

        // Removed support must invalidate its OLD extent; an empty replacement
        // has no new mesh bounds from which to infer that dependency.
        const auto removed = current.chunks.at({3, 3});
        current.previous = result;
        current.previousChunks = current.chunks;
        current.chunks[{3, 3}].collision.reset();
        result = Bake(current);
        Compare(current, *result);
        Check(result->rebuiltTiles > 0 && result->reusedTiles > 0,
              "empty replacement rebuilds old support and neighbouring erosion borders");
        current.previous = result;
        current.previousChunks = current.chunks;
        current.chunks[{3, 3}] = removed;
        current.chunks[{3, 3}].anchor.x += 1.25f;
        result = Bake(current);
        Compare(current, *result);
        Check(result->rebuiltTiles > 0 && result->reusedTiles > 0,
              "new/moved geometry invalidates its replacement extent");

        current.previous = result;
        current.previousChunks = current.chunks;
        current.settings.cellSize = .6f;
        result = Bake(current);
        Compare(current, *result);
        Check(!result->reusedTiles && result->rebuiltTiles > 0,
              "resolution changes fall back to a complete bake");
        current.previous = result;
        current.profile.radius = .65f;
        result = Bake(current);
        Compare(current, *result);
        Check(!result->reusedTiles, "profile/erosion changes cannot reuse stale layers");

        auto published = result;
        Check(!TerrainV2::PrepareNavigation(current, published, error, [] { return true; }) &&
                  published == result && error == "cancelled",
              "cancellation preserves the previous complete output");
        current.previous = result;
        auto corrupted = std::make_shared<NavigationData>(*result);
        corrupted->layers.front().resize(1);
        current.previous = corrupted;
        Check(!TerrainV2::PrepareNavigation(current, published, error) && published == result &&
                  error.find("truncated") != std::string::npos,
              "a corrupt previous result rejects before changing published output");

        NavigationBakeUpdate invalid;
        invalid.previous = result.get();
        invalid.changed.push_back({{NAN, 0, 0}, {1, 1, 1}});
        NavigationTileReuse reuse;
        Check(!reuse.Prepare(&invalid, current.settings, current.profile, 14, 14, 5, error),
              "nonfinite changed bounds cannot certify reuse");

        // Independent membership control: this point is outside tile2's core
        // but INSIDE its two-metre filter border. Omitting the border must fail.
        NavigationData empty;
        empty.settings.halfExtents = {64, 12, 64};
        empty.settings.cellSize = .5f;
        empty.settings.tileSize = 16;
        NavigationBakeUpdate edge;
        edge.previous = &empty;
        std::vector<std::vector<unsigned char>> output;
        for (float x : {-48.5f, -50.f, -38.f}) {
            edge.changed = {{{x, 0, -48.5f}, {x, 0, -48.5f}}};
            Check(reuse.Prepare(&edge, empty.settings, empty.profile, 16, 16, 4, error) &&
                      !reuse.Append(2, 2, output) && reuse.Append(15, 15, output),
                  "filter-border dependencies include both exact tangent edges");
        }
        for (float x : {std::nextafter(-50.f, -INFINITY), std::nextafter(-38.f, INFINITY)}) {
            edge.changed = {{{x, 0, -48.5f}, {x, 0, -48.5f}}};
            Check(reuse.Prepare(&edge, empty.settings, empty.profile, 16, 16, 4, error) &&
                      reuse.Append(2, 2, output),
                  "one-ULP exterior bounds preserve a genuinely independent empty tile");
        }
        Kilometre();
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << " after " << checks << " checks\n";
        return 1;
    }
}
