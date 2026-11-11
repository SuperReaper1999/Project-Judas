#pragma once

namespace JudasPhysicsDetail {
// Shared numerical policies, retained from the accepted MK2 solver. Search
// scheduling changes do not increase arrival caps or relax approach coverage.
inline constexpr float kMk2ImpactApproach = 1e-4f;
inline constexpr unsigned kMk2ArrivalsPerBody = 4;
inline constexpr unsigned kMk2ArrivalsPerStep = 256;
inline constexpr double kMk2FastFraction = 0.25;
}
