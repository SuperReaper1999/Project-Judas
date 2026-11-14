#pragma once

namespace JudasPhysicsDetail {
// Shared physical resolution and bounded-search policies. A native interval
// may resolve up to 256 arrivals; successful responses do not freeze a body
// merely because it has reached a fourth distinct collision time.
inline constexpr float kMk2ImpactApproach = 1e-4f;
inline constexpr unsigned kMk2ArrivalsPerStep = 256;
inline constexpr double kMk2FastFraction = 0.25;
}
