#include "GameWaterCoupling.h"
#include "GameWaterRuntimeInternal.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace JudasWater {
namespace {
bool Overlaps(const Grid &a, const Grid &b) {
    const auto &aa = a.Configuration(), &bb = b.Configuration();
    const auto ah = glm::vec3(aa.cells) * aa.cellSize * .5f,
               bh = glm::vec3(bb.cells) * bb.cellSize * .5f;
    const glm::mat3 ar = glm::mat3_cast(aa.rotation), br = glm::mat3_cast(bb.rotation);
    const auto delta = b.ToWorld(bh) - a.ToWorld(ah);
    auto separated = [&](glm::vec3 axis) {
        if (glm::length(axis) < 1e-6f)
            return false;
        axis = glm::normalize(axis);
        float ra = 0, rb = 0;
        for (int i = 0; i < 3; ++i) {
            ra += ah[i] * std::abs(glm::dot(axis, ar[i]));
            rb += bh[i] * std::abs(glm::dot(axis, br[i]));
        }
        return std::abs(glm::dot(axis, delta)) >= ra + rb - 1e-5f;
    };
    for (int i = 0; i < 3; ++i) {
        if (separated(ar[i]) || separated(br[i]))
            return false;
        for (int j = 0; j < 3; ++j)
            if (separated(glm::cross(ar[i], br[j])))
                return false;
    }
    return true;
}
} // namespace
WorldSystem::WorldSystem(RuntimeWorld &world) : world_(world) {}
WorldSystem::~WorldSystem() {
    if (auto *resources = world_.Resources())
        if (auto *renderer = resources->GetRenderer())
            for (auto &[_, region] : regions_)
                for(auto mesh:region->meshes)if(mesh.IsValid())renderer->DestroyMesh(mesh);
}
bool WorldSystem::Build(const Scene &scene, std::string &error) {
    std::map<std::uint64_t, std::unique_ptr<Region>> staged;
    std::size_t total = 0;
    for (const auto &object : scene.Objects()) {
        if (!object.fluidVolume || object.fluidVolume->backend != SceneFluidBackend::Grid)
            continue;
        if (staged.size() >= 8) {
            error = "at most eight active grid-water regions are supported";
            return false;
        }
        const auto &component = *object.fluidVolume;
        if (component.emitter || !std::isfinite(component.fillHeight) || component.fillHeight < 0 ||
            component.fillHeight > component.spacing * component.countY) {
            error = "grid-water region needs a fill height inside its domain and no "
                    "particle emitter; use scripted volume sources";
            return false;
        }
        if ((object.body && object.body->motion != SceneBodyMotion::Static) ||
            object.characterMotor || object.ragdoll) {
            error = "grid-water domain must be stationary; put moving colliders "
                    "inside it, not on "
                    "its owner";
            return false;
        }
        const auto scale = object.transform.scale;
        if (scale.x <= 0 || std::abs(scale.x - scale.y) > 1e-5f ||
            std::abs(scale.x - scale.z) > 1e-5f) {
            error = "grid-water domain requires positive uniform scale; use cell "
                    "dimensions for "
                    "its shape";
            return false;
        }
        Settings settings;
        settings.cells = {component.countX, component.countY, component.countZ};
        settings.cellSize = component.spacing * scale.x;
        total += std::size_t(component.countX) * component.countY * component.countZ;
        if (total > 262144) {
            error = "active grid-water cell budget exceeds 262144";
            return false;
        }
        settings.rotation = object.transform.rotation;
        settings.origin =
            object.transform.position -
            settings.rotation * glm::vec3(.5f * settings.cells.x * settings.cellSize, 0,
                                          .5f * settings.cells.z * settings.cellSize);
        settings.density = component.density;
        const auto centre = settings.origin + settings.rotation * (glm::vec3(settings.cells) *
                                                                   settings.cellSize * .5f);
        settings.gravity = world_.SampleEntityGravity(object.id, centre);
        auto region = std::make_unique<Region>();
        region->owner = object.id;
        region->authored = component;
        if (!region->grid.Initialize(settings, error) ||
            !region->RefreshGravity(world_, region->grid, error))
            return false;
        for (const auto &[other, existing] : staged)
            if (Overlaps(region->grid, existing->grid)) {
                error = "grid-water domains overlap (" + std::to_string(other) + ", " +
                        std::to_string(object.id) +
                        "); use one connected domain; independent grids do not "
                        "exchange water";
                return false;
            }
        std::vector<SolidBox> solids;
        std::vector<BodyHandle> moving;
        if (!region->PrepareGeometry(world_, region->grid, solids, moving, error))
            return false;
        region->statics.swap(region->pendingStatics);
        region->grid.Fill([&](glm::vec3 p) {
            return region->grid.ToLocal(p).y < component.fillHeight * scale.x;
        });
        staged.emplace(object.id, std::move(region));
    }
    if (auto *resources = world_.Resources())
        if (auto *renderer = resources->GetRenderer())
            for (auto &[_, region] : regions_)
                for(auto mesh:region->meshes)if(mesh.IsValid())renderer->DestroyMesh(mesh);
    regions_ = std::move(staged);
    error_.clear();
    error.clear();
    return true;
}
bool WorldSystem::Step(float seconds, std::string &error) {
    for (auto &[id, owner] : regions_) {
        auto &region = *owner;
        if (region.paused || !world_.IsPublished(id) || !world_.RuntimeDefinition(id))
            continue;
        // Reused staging arrays make failed updates transactional. Both buffer
        // owners retain capacity; this is not a fresh graph allocation per tick.
        const auto begin = std::chrono::steady_clock::now();
        auto elapsed = [](auto since) {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                             since)
                .count();
        };
        region.staging = region.grid;
        std::vector<SolidBox> solids;
        std::vector<BodyHandle> moving;
        if (!region.RefreshGravity(world_, region.staging, error) ||
            !region.PrepareGeometry(world_, region.staging, solids, moving, error)) {
            region.error = error;
            region.paused = true;
            error_ = "water region " + std::to_string(id) + ": " + error;
            return false;
        }
        region.preparationMs = elapsed(begin);
        if (!region.staging.Step(seconds, error)) {
            region.error = error;
            region.paused = true;
            error_ = "water region " + std::to_string(id) + ": " + error;
            return false;
        }
        std::swap(region.grid, region.staging);
        region.statics.swap(region.pendingStatics);
        ++region.revision;
        const auto coupling = std::chrono::steady_clock::now();
        for (auto body : moving)
            ApplyBodyLoad(region.grid, world_.Physics(), body, seconds);
        region.couplingMs = elapsed(coupling);
        region.runtimeMs = elapsed(begin);
    }
    error.clear();
    return true;
}
void WorldSystem::ShiftOrigin(glm::dvec3 delta) {
    for (auto &[_, region] : regions_) {
        region->grid.ShiftOrigin(delta);
        region->staging = Grid{};
        region->gravitySignature.clear();
        region->statics.clear();
        region->pendingStatics.clear();
        ++region->revision;
    }
}
const Grid *WorldSystem::Find(std::uint64_t id) const {
    auto it = regions_.find(id);
    return it == regions_.end() ? nullptr : &it->second->grid;
}
std::vector<std::uint64_t> WorldSystem::Owners() const {
    std::vector<std::uint64_t> result;
    for (const auto &[id, _] : regions_)
        result.push_back(id);
    return result;
}
bool WorldSystem::Paused(std::uint64_t id) const {
    auto it = regions_.find(id);
    return it != regions_.end() && it->second->paused;
}
bool WorldSystem::SetPaused(std::uint64_t id, bool paused) {
    auto it = regions_.find(id);
    if (it == regions_.end())
        return false;
    it->second->paused = paused;
    if (!paused) {
        it->second->error.clear();
        it->second->gravitySignature.clear();
    }
    error_.clear();
    for (const auto &[owner, region] : regions_)
        if (!region->error.empty()) {
            error_ = "water region " + std::to_string(owner) + ": " + region->error;
            break;
        }
    return true;
}
bool WorldSystem::AddImpulse(std::uint64_t id, glm::vec3 point, glm::vec3 impulse, float radius) {
    auto it = regions_.find(id);
    if (it == regions_.end() || !std::isfinite(glm::dot(point, point)) ||
        !std::isfinite(glm::dot(impulse, impulse)) || !std::isfinite(radius) || radius < 0)
        return false;
    it->second->grid.AddImpulse(point, impulse, radius);
    return true;
}
WorldSystem::RuntimeStats WorldSystem::Stats(std::uint64_t id) const {
    const auto it = regions_.find(id);
    if (it == regions_.end())
        return {};
    const auto &r = *it->second;
    return {r.runtimeMs,     r.preparationMs,
            r.couplingMs,    r.grid.Stats().bytes + r.staging.Stats().bytes,
            r.geometryBakes, r.error};
}
double WorldSystem::ChangeWater(std::uint64_t id, glm::vec3 point, float radius,
                                double cubicMetres) {
    auto it = regions_.find(id);
    if (it == regions_.end())
        return 0;
    auto &region = *it->second;
    const double result = region.grid.ChangeWater(point, radius, cubicMetres);
    if (result != 0)
        ++region.revision;
    return result;
}
} // namespace JudasWater

bool RuntimeWorld::BuildGridWater(const Scene &scene, std::string &error) {
    m_gridWater.reset();
    if (!m_advancedFluids)
        return true;
    if (!std::any_of(scene.Objects().begin(), scene.Objects().end(), [](const auto &object) {
            return object.fluidVolume && object.fluidVolume->backend == SceneFluidBackend::Grid;
        }))
        return true;
    auto candidate = std::make_unique<JudasWater::WorldSystem>(*this);
    if (!candidate->Build(scene, error))
        return false;
    m_gridWater = std::move(candidate);
    return true;
}
void RuntimeWorld::UpdateGridWater(float dt) {
    if (!m_gridWater)
        return;
    std::string error;
    if (!m_gridWater->Step(dt, error))
        std::fprintf(stderr, "Grid water paused: %s\n", error.c_str());
}
