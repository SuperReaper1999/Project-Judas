#include "render/SkinnedBoundsCache.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

int main() {
    unsigned failures = 0;
    auto check = [&](bool value, const char* label) {
        std::printf("%s %s\n", value ? "PASS" : "FAIL", label); failures += !value;
    };
    JudasRender::SkinnedBoundsCache cache;
    VisualBounds source; source.min = {-1,-1,-1}; source.max = {1,1,1};
    std::vector<glm::mat4> pose{glm::translate(glm::mat4(1),{10,0,0}),
                               glm::scale(glm::mat4(1),{2,2,2})};
    auto bounds = cache.Resolve(source, pose);
    check(bounds.min == glm::vec3(-2) && bounds.max == glm::vec3(11,2,2),
          "all joint transforms contribute to the conservative local bound");
    for (unsigned pass = 0; pass < 8; ++pass) {
        auto repeated = cache.Resolve(source, pose);
        check(repeated.min == bounds.min && repeated.max == bounds.max,
              "repeated camera/shadow input retains identical bounds");
    }
    pose[0][3].x = -20;
    bounds = cache.Resolve(source, pose);
    check(bounds.min == glm::vec3(-21,-2,-2) && bounds.max == glm::vec3(2),
          "in-place animation change invalidates an existing palette address");
    source.min = {-3,-3,-3}; source.max = {3,3,3};
    bounds = cache.Resolve(source, pose);
    check(bounds.min == glm::vec3(-23,-6,-6) && bounds.max == glm::vec3(6),
          "geometry-bound change invalidates the cached product");
    pose.resize(1); pose[0] = glm::translate(glm::mat4(1),{100,0,0});
    check(!Frustum{}.IsVisible(cache.Resolve(source, pose)), "offscreen pose is culled");
    pose[0] = glm::mat4(1);
    check(Frustum{}.IsVisible(cache.Resolve(source, pose)),
          "returning animation becomes visible with the same palette address");
    pose.clear();
    bounds = cache.Resolve(source, pose);
    check(bounds.min == source.min && bounds.max == source.max, "empty palette uses original mesh bounds");
    std::vector<std::vector<glm::mat4>> owners(300, std::vector<glm::mat4>(16,glm::mat4(1)));
    for (auto& owner : owners) cache.Resolve(source, owner);
    check(cache.Entries() <= cache.MaxEntries && cache.PoseBytes() <= cache.MaxPoseBytes,
          "unbounded unique draw owners cannot grow the cache without limit");
    owners.assign(8,std::vector<glm::mat4>(16384,glm::mat4(1)));
    for (auto& owner : owners) cache.Resolve(source, owner);
    check(cache.Entries() <= cache.MaxEntries && cache.PoseBytes() <= cache.MaxPoseBytes,
          "large legal palettes obey the byte limit independently of entry count");
    cache.Clear();
    check(cache.Entries() == 0 && cache.PoseBytes() == 0, "frame/context retirement releases pose snapshots");
    return failures ? 1 : 0;
}
