#include "TerrainPlanetBinding.h"
#include "AssetDatabase.h"
#include "PlanetTiles.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "TerrainAsset.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace TerrainV2 {
bool CreatePlanetPatch(const PlanetSettings &settings, glm::dvec3 direction, unsigned cells,
                       Source &output, std::string &error) {
    try {
        if (!ValidatePlanetSettings(settings, error))
            return false;
        if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
            !std::isfinite(direction.z) || glm::length(direction) < 1e-10 ||
            !std::isfinite(glm::length(direction)))
            throw std::invalid_argument("Choose a finite direction away from the planet centre.");
        if (cells < 32 || cells > 512 || (cells & (cells - 1)))
            throw std::invalid_argument("Planet patches need32..512 power-of-two cells.");
        unsigned ratio = 0;
        for (unsigned n = 32; n < cells; n *= 2)
            ++ratio;
        const auto physical = PlanetCollisionLevel(settings);
        if (physical < ratio + 3)
            throw std::invalid_argument(
                "This patch is too wide for the planet. Choose fewer samples "
                "or finer planet collision spacing.");
        const PlanetSurface planet(settings);
        PlanetChart chart;
        Source source;
        if (!CreatePlanetChart(planet, PlanetSurface::Locate(direction, physical - ratio), cells,
                               source, chart, error))
            return false;
        source.version = 3;
        source.chart = chart;
        unsigned level;
        std::vector<PlanetTileKey> pins;
        if (!Validate(source, error) ||
            !PlanetTerrainBoundaryLevel(settings, source, level, error) ||
            !PreparePlanetCoverage(planet, {{chart.tile, level}}, pins, error))
            return false;
        output = std::move(source);
        error.clear();
        return true;
    } catch (const std::exception &exception) {
        error = exception.what();
        return false;
    }
}
SceneTransform PlanetTerrainPlacement(const PlanetSettings &planet, const PlanetChart &chart,
                                      glm::dvec3 origin) {
    SceneTransform placement;
    const auto rotation = glm::normalize(planet.rotation);
    placement.position = glm::vec3(planet.center + rotation * chart.ModelOrigin() - origin);
    placement.rotation = glm::quat(glm::normalize(rotation * chart.ModelRotation()));
    return placement;
}
bool PlanetTerrainBoundaryLevel(const PlanetSettings &planet, const Source &source,
                                unsigned &output, std::string &error) {
    try {
        if (!source.chart)
            throw std::invalid_argument("terrain has no geographic chart");
        const auto &chart = *source.chart;
        const auto physical = PlanetCollisionLevel(planet);
        const unsigned cells = source.samplesX - 1;
        if (chart.tile.level > physical || physical - chart.tile.level > 4 ||
            cells != (32u << (physical - chart.tile.level)) || cells < planet.segments)
            throw std::invalid_argument(
                "planet patch grid does not match its physical rim; regenerate the geographic "
                "patch at the planet's collision resolution");
        unsigned level = chart.tile.level;
        for (unsigned n = planet.segments; n < cells; n *= 2)
            ++level;
        output = level;
        error.clear();
        return true;
    } catch (const std::exception &exception) {
        error = exception.what();
        return false;
    }
}
bool ValidatePlanetBinding(const SceneObject &object, const Source &source,
                           const SceneObject *parent, glm::dvec3 origin, std::string &error,
                           bool requirePlacement) {
    auto fail = [&](const std::string &message) {
        error = message;
        return false;
    };
    if (!source.chart)
        return object.landscape && object.landscape->planet
                   ? fail("terrain.planet requires a projected native source")
                   : true;
    if (!object.landscape || !object.landscape->planet || !parent || !parent->planet ||
        parent->id != object.landscape->planet || parent->id == object.id)
        return fail("projected terrain requires a valid terrain.planet owner");
    if (object.prefabRoot || object.parent)
        return fail("planetary terrain is a scene-owned geographic patch, not a movable prefab");
    const auto &chart = *source.chart;
    if (chart.radius != parent->planet->radius ||
        chart.surfaceFingerprint != PlanetGeometryFingerprint(*parent->planet))
        return fail("planet terrain source changed; regenerate its geographic patch before Play");
    const auto placement = PlanetTerrainPlacement(*parent->planet, chart, origin);
    const auto delta = std::min(glm::length(placement.rotation - object.transform.rotation),
                                glm::length(placement.rotation + object.transform.rotation));
    if (requirePlacement &&
        (glm::distance(placement.position, object.transform.position) > .0001f || delta > 1e-6f ||
         object.transform.scale != glm::vec3(1)))
        return fail("planet patch pose is geographic; use its derived placement instead of "
                    "moving/scaling it");
    unsigned level;
    if (!PlanetTerrainBoundaryLevel(*parent->planet, source, level, error))
        return false;
    try {
        PlanetSurface planet(*parent->planet);
        size_t sample = 0;
        const unsigned cells = source.samplesX - 1;
        const double count = double(uint64_t(1) << chart.tile.level);
        auto check = [&](unsigned x, unsigned z) {
            const auto direction = PlanetSurface::Direction(
                chart.tile.face, 2 * (chart.tile.x + double(x) / cells) / count - 1,
                2 * (chart.tile.y + 1 - double(z) / cells) / count - 1);
            return sample < chart.rim.size() &&
                   chart.rim[sample++] == float(planet.HeightAt(direction));
        };
        for (unsigned x = 0; x <= cells; ++x)
            if (!check(x, 0))
                return fail("planet terrain rim differs from its source provider");
        for (unsigned z = 0; z <= cells; ++z)
            if (!check(cells, z))
                return fail("planet terrain rim differs from its source provider");
        for (unsigned x = 0; x <= cells; ++x)
            if (!check(x, cells))
                return fail("planet terrain rim differs from its source provider");
        for (unsigned z = 0; z <= cells; ++z)
            if (!check(0, z))
                return fail("planet terrain rim differs from its source provider");
        std::vector<PlanetTileKey> pins;
        return PreparePlanetCoverage(planet, {{chart.tile, level}}, pins, error);
    } catch (const std::exception &exception) {
        return fail(exception.what());
    }
}
bool ValidatePlanetTerrains(const Scene &scene, const AssetDatabase *assets, std::string &error) {
    std::map<SceneObjectId, std::vector<PlanetTerrainCutout>> cuts;
    for (const auto &object : scene.Objects())
        if (object.landscape && object.landscape->planet) {
            const auto *asset = assets ? assets->Find(object.landscape->asset) : nullptr;
            AssetPages pages;
            if (!asset || !pages.Open(asset->path, error))
                return false;
            const auto *parent = scene.Find(object.landscape->planet);
            const auto &source = pages.Index().metadata;
            if (!ValidatePlanetBinding(object, source, parent, scene.Settings().worldOrigin, error))
                return false;
            unsigned level;
            if (!PlanetTerrainBoundaryLevel(*parent->planet, source, level, error))
                return false;
            cuts[parent->id].push_back({source.chart->tile, level});
        }
    for (const auto &[id, coverage] : cuts) {
        std::vector<PlanetTileKey> pins;
        if (!PreparePlanetCoverage(PlanetSurface(*scene.Find(id)->planet), coverage, pins, error))
            return false;
    }
    error.clear();
    return true;
}
} // namespace TerrainV2

bool RuntimeWorld::ValidatePlanetTerrain(EntityId owner, const TerrainV2::Source &source,
                                         std::string &error) const {
    const auto definition = m_scriptDefinitions.find(owner);
    if (definition == m_scriptDefinitions.end()) {
        error = "terrain owner disappeared";
        return false;
    }
    const auto &object = definition->second;
    if (!source.chart) {
        if (object.landscape && object.landscape->planet) {
            error = "terrain.planet requires a projected source";
            return false;
        }
        return true;
    }
    if (!object.landscape || !object.landscape->planet) {
        error = "projected terrain requires terrain.planet";
        return false;
    }
    const auto planet = m_planets.find(object.landscape->planet);
    if (planet == m_planets.end()) {
        error = "terrain planet is unavailable; keep its region resident";
        return false;
    }
    SceneObject parent;
    parent.id = object.landscape->planet;
    parent.planet = planet->second->surface.settings;
    return TerrainV2::ValidatePlanetBinding(object, source, &parent, m_settings.worldOrigin, error);
}
bool RuntimeWorld::BindPlanetTerrain(EntityId owner, const TerrainV2::Source &source,
                                     std::string &error) {
    if (!source.chart) {
        error = "terrain has no projected source";
        return false;
    }
    if (!ValidatePlanetTerrain(owner, source, error))
        return false;
    const auto &object = m_scriptDefinitions.at(owner);
    const auto id = object.landscape->planet;
    if (IsPublished(owner) && !IsPublished(id)) {
        error = "publish the planet before its terrain replacement";
        return false;
    }
    auto &planet = *m_planets.at(id);
    unsigned level;
    if (!TerrainV2::PlanetTerrainBoundaryLevel(planet.surface.settings, source, level, error))
        return false;
    return planet.SetTerrainPatch(owner, PlanetTerrainCutout{source.chart->tile, level},
                                  IsPublished(owner), error);
}
void RuntimeWorld::UnbindPlanetTerrain(EntityId owner, EntityId planet) {
    if (auto found = m_planets.find(planet); found != m_planets.end()) {
        std::string error;
        found->second->SetTerrainPatch(owner, std::nullopt, false, error);
    }
}
