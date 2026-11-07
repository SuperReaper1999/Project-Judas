#include "GameWater.h"
#include "SaveStorage.h"
#include "SceneFingerprint.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace JudasWater {
void Grid::Persist(SaveArchive &a) {
    std::uint32_t version = 2;
    a(version);
    a.Require(version == 1 || version == 2, "Unsupported game-water state version.");
    Settings settings = settings_;
    a(settings.cells, settings.cellSize, settings.origin, settings.rotation, settings.gravity,
      settings.density, settings.pressureIterations, settings.pressureTolerance,
      settings.minimumAirFraction, settings.minimumMobileFraction, settings.minimumReservoirCells);
    if (a.reading) {
        std::string error;
        a.Require(Initialize(settings, error), error);
        a.Require(std::abs(glm::dot(settings.rotation, settings.rotation) - 1.f) < 1e-4f,
                  "Saved water chart is not a unit rotation.");
        settings_.rotation = settings.rotation;
    }
    std::uint32_t count = std::uint32_t(cells_.size());
    a(count);
    a.Require(count == cells_.size(), "Water snapshot cell count differs from its declared grid.");
    const double volume = double(settings_.cellSize) * settings_.cellSize * settings_.cellSize;
    for (auto &c : cells_) {
        a(c.water, c.displaced, c.airMass, c.capacity, c.staticCapacity, c.pressure, c.air);
        a.Require(c.water >= 0 && c.displaced >= 0 && c.airMass >= 0 && c.capacity >= 0 &&
                      c.staticCapacity >= c.capacity && c.staticCapacity <= volume * (1 + 1e-6) &&
                      c.water <= c.capacity + 1e-10,
                  "Invalid saved water material or capacity.");
    }
    count = std::uint32_t(faces_.size());
    a(count);
    a.Require(count == faces_.size(), "Water snapshot face count mismatch.");
    for (std::size_t i = 0; i < faces_.size(); ++i) {
        a(faces_[i].velocity, faces_[i].open, staticFaces_[i]);
        a.Require(faces_[i].open >= 0 && faces_[i].open <= 1 && staticFaces_[i] >= 0 &&
                      staticFaces_[i] <= 1,
                  "Invalid saved water opening.");
    }
    for (auto &mask : staticSamples_)
        a(mask);
    for (auto &mask : samples_)
        a(mask);
    for (std::size_t i = 0; i < faceSamples_.size(); ++i)
        a(staticFaceSamples_[i], faceSamples_[i]);
    count = std::uint32_t(air_.size());
    a(count);
    a.Require(count <= cells_.size(), "Saved air-reservoir count exceeds grid.");
    if (a.reading)
        air_.resize(count);
    for (auto &p : air_) {
        a(p.volume, p.mass, p.pressure, p.atmosphere, p.unresolved);
        a.Require(p.volume > 0 && p.mass >= 0, "Invalid saved air reservoir.");
    }
    for (const auto &c : cells_)
        a.Require(c.air >= -1 && c.air < int(air_.size()),
                  "Saved air ownership is outside its reservoir table.");
    std::uint64_t sealed = sealedAmbiguousCells_;
    a(exteriorAirMass_, airInitialized_, sealed);
    a.Require(sealed <= cells_.size(), "Invalid saved static-cell diagnostics.");
    sealedAmbiguousCells_ = std::size_t(sealed);
    if (version >= 2)
        a(initialQuantity_, addedQuantity_, removedQuantity_);
    else if (a.reading) {
        initialQuantity_ = Quantity();
        addedQuantity_ = removedQuantity_ = 0;
    }
    a.Require(initialQuantity_ >= 0 && addedQuantity_ >= 0 && removedQuantity_ >= 0,
              "Invalid saved water source ledger.");
    if (a.reading) {
        a.Require(std::abs(Quantity() - (initialQuantity_ + addedQuantity_ - removedQuantity_)) <=
                      1e-6 * std::max(1., Quantity()),
                  "Saved water quantity disagrees with its source ledger.");
        UpdateVelocity();
        UpdateHeights();
    }
}
bool Grid::Save(const std::string &file, std::string &error) const {
    try {
        auto copy = *this;
        SaveArchive a;
        copy.Persist(a);
        GameSnapshot snapshot;
        snapshot.project = "judas-game-water-preview";
        snapshot.scene = "grid";
        snapshot.content = SceneFingerprintSha256("game-water-grid-v1");
        snapshot.participants["water"] = {1, std::move(a.bytes)};
        const std::filesystem::path p(file),
            directory = p.has_parent_path() ? p.parent_path() : std::filesystem::path(".");
        std::filesystem::create_directories(directory);
        SaveStorage storage(directory.string());
        return storage.Write(p.filename().string(), snapshot, error);
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool Grid::Load(const std::string &file, std::string &error) {
    try {
        const std::filesystem::path p(file),
            directory = p.has_parent_path() ? p.parent_path() : std::filesystem::path(".");
        SaveStorage storage(directory.string());
        GameSnapshot snapshot;
        bool recovered = false;
        if (!storage.Read(p.filename().string(), snapshot, recovered, error))
            return false;
        if (snapshot.project != "judas-game-water-preview" ||
            snapshot.participants.count("water") != 1 ||
            snapshot.participants.at("water").version != 1) {
            error = "Not a compatible game-water snapshot.";
            return false;
        }
        SaveArchive a(snapshot.participants.at("water").data);
        Grid candidate;
        candidate.Persist(a);
        a.Finish();
        *this = std::move(candidate);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
} // namespace JudasWater
