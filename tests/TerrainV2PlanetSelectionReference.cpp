#include "PlanetTerrain.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <stdexcept>
namespace {
using V = glm::dvec3;
bool finite(V p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
V unit(V p) {
    const double n = glm::length(p);
    if (!finite(p) || n < 1e-30)
        throw std::invalid_argument("undefined planetary direction");
    return p / n;
}
void require(bool b, const char *text) {
    if (!b)
        throw std::invalid_argument(text);
}
} // namespace
#include "fixtures/terrain_v2/planet_selection_reference.inc"
