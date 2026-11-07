#include "SaveArchive.h"
#include "fluid/GameWater.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL " << what << '\n';
    }
}
std::string State(JudasWater::Grid grid) {
    SaveArchive a;
    grid.Persist(a);
    return a.bytes;
}
} // namespace
int main() {
    using namespace JudasWater;
    Grid grid;
    Settings settings;
    std::string error;
    float unusedHeight = 123;
    Check(!grid.Surface(0, 0, unusedHeight) && grid.Velocity({}) == glm::vec3(0) &&
              grid.NearbyMass({}, 1) == 0,
          "an uninitialized region has no queryable water");
    auto invalid = settings;
    invalid.cells = {2147483647, 2147483647, 2147483647};
    Check(!grid.Initialize(invalid, error),
          "oversized dimensions reject before integer multiplication");
    invalid = settings;
    invalid.gravity = {0, 0, -9.81f};
    Check(!grid.Initialize(invalid, error),
          "unsupported gravity/chart combinations give an explicit diagnostic");
    settings.cells = {8, 8, 8};
    settings.cellSize = .25f;
    Check(grid.Initialize(settings, error), "bounded grid initializes");
    Check(!grid.Surface(-1, 0, unusedHeight) && !grid.Surface(settings.cells.x, 0, unusedHeight),
          "out-of-region surface queries do not access a cell");
    grid.BakeStatic({});
    const double initial = grid.FillBox({0, 0, 0}, {2, .75f, 2});
    bool completed = true;
    for (int i = 0; i < 120; ++i)
        if (!grid.Step(1.f / 60, error)) {
            completed = false;
            break;
        }
    Check(completed, "filled pool advances two seconds");
    Check(std::abs(grid.Stats().water - initial) < 1e-6,
          "paired transport conserves the global water inventory");
    Check(grid.Stats().rmsSpeed < .02,
          "quiet pool stays quiet without damping or a velocity clamp");
    const auto bytes = State(grid);
    Grid restored;
    SaveArchive read(bytes);
    restored.Persist(read);
    read.Finish();
    Check(State(restored) == bytes, "all fluid and reservoir state roundtrips exactly");
    bool same = true;
    for (int i = 0; i < 20; ++i) {
        if (!grid.Step(1.f / 60, error) || !restored.Step(1.f / 60, error)) {
            same = false;
            break;
        }
        same &= State(grid) == State(restored);
    }
    Check(same, "cold reconstructed fluid state continues deterministically");
    auto rotatedSettings = settings;
    rotatedSettings.rotation = glm::angleAxis(.83f, glm::normalize(glm::vec3(1, -2, 3)));
    rotatedSettings.gravity = rotatedSettings.rotation * rotatedSettings.gravity;
    Grid rotated;
    rotated.Initialize(rotatedSettings, error);
    rotated.BakeStatic({});
    rotated.Fill([&](glm::vec3 p) { return rotated.ToLocal(p).y < .75f; });
    rotated.Step(1.f / 60, error);
    auto rotatedBytes = State(rotated);
    Grid rotatedRead;
    SaveArchive rotatedArchive(rotatedBytes);
    rotatedRead.Persist(rotatedArchive);
    rotatedArchive.Finish();
    Check(State(rotatedRead) == rotatedBytes,
          "saved chart rotations retain their exact accepted bits");
    const auto dir = std::filesystem::temp_directory_path() /
                     ("judas-game-water-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    Check(grid.Save((dir / "grid").string(), error),
          "normal engine SaveStorage writes water state");
    Grid cold;
    Check(cold.Load((dir / "grid").string(), error), "normal engine SaveStorage reads water state");
    Check(State(cold) == State(grid),
          "file load preserves exact water arrays and reservoir identities");
    const auto before = State(cold);
    Check(!cold.Load((dir / "missing").string(), error) && State(cold) == before,
          "failed load preserves the published grid");
    std::filesystem::remove_all(dir);
    Grid wall;
    wall.Initialize(settings, error);
    wall.BakeStatic([](glm::vec3 p) { return std::abs(p.x - .87f) < .04f; });
    const auto left = wall.FillBox({0, 0, 0}, {.75f, .75f, 2});
    completed = true;
    for (int i = 0; i < 120; ++i)
        if (!wall.Step(1.f / 60, error)) {
            completed = false;
            break;
        }
    double right = 0;
    for (int i = 0; i < int(wall.Cells().size()); ++i)
        if (wall.Center(i).x > 1)
            right += wall.Cells()[i].water;
    Check(completed && right < 1e-6,
          "an unresolved internal wall cannot become an invented connection");
    Check(wall.Stats().sealedAmbiguousCells > 0,
          "ambiguous static cuts are reported and conservatively sealed");
    Check(std::abs(wall.Stats().water - left) < 1e-6,
          "conservative static voxelization preserves authored water");
    Grid tank;
    tank.Initialize(settings, error);
    tank.BakeStatic([](glm::vec3 p) { return std::abs(p.x - 1.f) <= .04f; });
    tank.FillBox({1.04f, 0, 0}, {2, 1, 2});
    tank.Step(1.f / 60, error);
    glm::vec3 center(0);
    SolidBox dry;
    dry.center = {.85f, .4f, 1};
    dry.half = glm::vec3(.1f);
    Check(tank.ImmersedBox(dry, center) == 0, "water cannot buoy a dry body through a static wall");
    dry.center = {-1, .4f, 1};
    Check(grid.ImmersedBox(dry, center) == 0, "water cannot buoy a body outside its finite region");
    Grid full;
    full.Initialize(settings, error);
    full.BakeStatic({});
    const auto fullQuantity = full.FillBox({0, 0, 0}, {2, 2, 2});
    SolidBox insert;
    insert.id = 7;
    insert.center = {1, 1, 1};
    insert.half = glm::vec3(.4f);
    full.SetSolids({insert});
    Check(full.Step(1.f / 60, error), "a body can displace a completely filled region");
    Check(full.Stats().storedDisplacement > .1 &&
              std::abs(full.Stats().water - fullQuantity) < 1e-6,
          "unfittable water remains in explicit storage instead of being deleted");
    full.SetSolids({});
    Check(full.Step(1.f / 60, error) && full.Stats().storedDisplacement < 1e-8 &&
              std::abs(full.Stats().water - fullQuantity) < 1e-6,
          "stored water returns when a moving solid releases capacity");
    const auto beforeInvalidStep = State(full);
    Check(!full.Step(0, error) && State(full) == beforeInvalidStep,
          "invalid intervals reject before mutating physical state");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
