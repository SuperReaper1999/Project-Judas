#pragma once
#include "NavigationTypes.h"
#include "Scene.h"
#include <memory>
#include <functional>
// Immutable CPU resource. Runtime worlds build independent mutable Detour caches.
class AssetDatabase;
struct NavigationData {
    NavigationSurfaceSettings settings;
    NavigationProfile profile;
    std::string fingerprint;
    std::vector<std::vector<unsigned char>> layers;
    // Preparation diagnostics, not serialized or used by Detour/gameplay.
    size_t rebuiltTiles = 0, reusedTiles = 0;
};
struct NavigationGeometry {
    std::vector<float> vertices;
    std::vector<int> triangles;
    struct Volume {
        std::vector<float> polygon;
        float bottom = 0, top = 0;
        unsigned area = 0;
        bool blocked = false;
    };
    std::vector<Volume> modifiers;
    std::string fingerprint;
};
// Optional incremental bake. The owner certifies that geometry and modifiers
// outside these local bounds are unchanged. Bounds must cover both removed and
// replacement geometry. Profile/frame changes automatically require a full bake.
struct NavigationBakeUpdate {
    const NavigationData *previous = nullptr;
    struct Bounds {
        glm::vec3 minimum, maximum;
    };
    std::vector<Bounds> changed;
};
bool CollectNavigationGeometry(const Scene &, const SceneObject &, const ProjectNavigation &,
                               NavigationGeometry &, std::string &,
                               const AssetDatabase *assets = nullptr);
bool DecodeNavigation(const std::vector<unsigned char> &, NavigationData &, std::string &);
bool LoadNavigation(const std::string &, NavigationData &, std::string &);
bool SaveNavigation(const std::string &, const NavigationData &, std::string &);
// Shared bounded geometry baker, used by tools and opt-in live terrain.
bool ValidateNavigationBakeSettings(const NavigationSurfaceSettings &, const NavigationProfile &,
                                    std::string &);
bool BakeNavigationGeometry(const NavigationGeometry &, const NavigationSurfaceSettings &,
                            const NavigationProfile &, NavigationData &, std::string &,
                            bool allowEmpty = false, const std::function<bool()> &cancelled = {},
                            const NavigationBakeUpdate *update = nullptr);
// Scene collection and authoring wrapper.
bool BakeNavigation(const Scene &, const SceneObject &, const ProjectNavigation &, NavigationData &,
                    std::string &, const AssetDatabase *assets = nullptr);
