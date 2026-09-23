#pragma once

// Milestone 14: a deliberately MIRRORED copy of the attenuation/spotlight-
// cone formulas actually running in Renderer.cpp's fragment shader
// (kFragmentShaderSource) — not shared code (GLSL and C++ can't share a
// function body), kept as a separate copy on purpose so a future change to
// one formula that isn't mirrored to the other makes tests/LightingTests.cpp
// visibly fail rather than silently drift out of sync. Same pattern this
// project already uses for tests/SpacecraftControlTests.cpp's own mirrored
// production constants (see that file's own header comment) — see
// docs/ARCHITECTURE.md, "Milestone 14, Point-light attenuation," for the
// full derivation/citation and why a real GL-context screenshot (not a
// headless unit test) is what actually verifies the GLSL side; this file
// verifies the FORMULA's own mathematical properties (monotonic falloff,
// exact zero at the range cutoff, finite at zero distance, a smooth —
// not binary — cone edge) in isolation.

// Smooth-windowed inverse-square attenuation: genuinely inverse-square
// close to the light, smoothly reaches exactly 0 at `range` (never a hard
// cliff), and stays finite as `distance` approaches 0 (the "+1" term). See
// Karis, "Real Shading in Unreal Engine 4" (2013), "Punctual Lights," for
// the general shape this is based on — M14 does not claim the exact same
// coefficients or photometric correctness, just the same qualitative
// "windowed inverse square" idea, chosen over a hard cutoff or raw
// unbounded inverse-square (see docs/ARCHITECTURE.md, "Milestone 14,
// Point-light attenuation," for why).
float ComputeDistanceAttenuation(float distance, float range);

// Spotlight cone factor: 1.0 inside `innerCos` (i.e. `cosAngle >=
// innerCos`), 0.0 outside `outerCos` (`cosAngle <= outerCos`), and a smooth
// Hermite interpolation between the two — never a binary on/off edge. Cosines
// in, not degrees, matching the shader's own convention (see Renderer.cpp,
// SetDynamicLights, for the degrees->cosine conversion done once on the CPU
// per light per frame rather than once per fragment on the GPU).
float ComputeSpotConeFactor(float cosAngle, float innerCos, float outerCos);
