#pragma once

// Fixed-timestep simulation constants shared between the real interactive
// loop (Application.cpp) and the test harness's real-time diagnostic mode
// (TestHarness.cpp) — kept in one place so the harness's real-time mode is
// an accurate mirror of the actual loop's timing behavior by construction,
// not by two files' literals happening to agree. See docs/ARCHITECTURE.md,
// "Simulation timing."
namespace SimulationTiming {
constexpr float kFixedTimestep = 1.0f / 60.0f;
constexpr float kMaxFrameDeltaTime = 0.25f;  // clamp stalls before they reach the accumulator
constexpr int kMaxPhysicsStepsPerFrame = 8;  // catch-up cap
}  // namespace SimulationTiming
