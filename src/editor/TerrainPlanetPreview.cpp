#include "TerrainPlanetPreview.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "terrain/TerrainPlanetBinding.h"
#include <cmath>
#include <set>

const TerrainPlanetPreview::Header &TerrainPlanetPreview::ReadHeader(const std::string &id,
                                                                     ResourceManager &resources) {
    auto &header = sources[id];
    const auto *asset = resources.Assets() ? resources.Assets()->Find(id) : nullptr;
    std::error_code error;
    const auto path = asset ? asset->path : std::string{};
    const auto modified = std::filesystem::last_write_time(path, error);
    const auto bytes = error ? 0 : std::filesystem::file_size(path, error);
    if (!error && header.path == path && header.modified == modified && header.bytes == bytes)
        return header;
    header = {};
    header.path = path;
    header.modified = modified;
    header.bytes = bytes;
    TerrainV2::AssetPages pages;
    if (error || !asset || asset->type != AssetType::Terrain || asset->missing)
        header.error = "Missing geographic terrain asset: " + id;
    else if (pages.Open(path, header.error))
        header.source = pages.Index().metadata;
    return header;
}
void TerrainPlanetPreview::Draw(Renderer &renderer, const Scene &scene, ResourceManager &resources,
                                std::string &error) {
    std::set<uint64_t> keep;
    std::set<std::string> used;
    for (const auto &object : scene.Objects()) {
        if (!object.planet)
            continue;
        keep.insert(object.id);
        auto &preview = planets[object.id];
        const auto descriptor = EncodePlanetSettings(*object.planet);
        if (preview.descriptor != descriptor) {
            preview.tiles.reset();
            preview.descriptor = descriptor;
            preview.tiles = std::make_unique<PlanetTiles>(*object.planet);
        }
        std::vector<PlanetTerrainCutout> cuts;
        std::string why;
        for (const auto &patch : scene.Objects()) {
            if (!patch.landscape || patch.landscape->planet != object.id)
                continue;
            used.insert(patch.landscape->asset);
            const auto &header = ReadHeader(patch.landscape->asset, resources);
            if (!header.error.empty()) {
                why = header.error;
                break;
            }
            if (!TerrainV2::ValidatePlanetBinding(patch, header.source, &object,
                                                  scene.Settings().worldOrigin, why))
                break;
            unsigned level;
            if (!TerrainV2::PlanetTerrainBoundaryLevel(*object.planet, header.source, level, why))
                break;
            cuts.push_back({header.source.chart->tile, level});
        }
        if (why.empty())
            preview.tiles->SetTerrainCutouts(cuts, why);
        if (!why.empty())
            error = "Planet preview: " + why;
        const auto eye =
            glm::dvec3(glm::inverse(renderer.ViewMatrix())[3]) + scene.Settings().worldOrigin;
        preview.tiles->Prepare(renderer, eye, scene.Settings().worldOrigin,
                               2 * std::atan(1. / double(renderer.ProjectionMatrix()[1][1])), 900,
                               1. / 60);
        preview.tiles->Draw(renderer, scene.Settings().worldOrigin);
    }
    for (auto it = planets.begin(); it != planets.end();)
        if (!keep.count(it->first))
            it = planets.erase(it);
        else
            ++it;
    for (auto it = sources.begin(); it != sources.end();)
        if (!used.count(it->first))
            it = sources.erase(it);
        else
            ++it;
}
void TerrainPlanetPreview::Clear() {
    planets.clear();
    sources.clear();
}
