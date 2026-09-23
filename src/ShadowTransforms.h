#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Milestone 15: pure, stateless light-space transform math — the shadow-
// mapping sibling of src/LightTransforms.h (M14). No GL, no gameplay
// state, directly unit-testable (including under an arbitrary rigid
// rotation of the whole scenario — see tests/ShadowTests.cpp) without a
// real GL context. See docs/ARCHITECTURE.md, "Milestone 15," for the full
// derivation.

// Picks a reference "up" vector for building a `glm::lookAt`-style camera
// basis along `forward` — a plain graphics utility (the same problem
// `glm::lookAt`'s own `up` parameter always has: pick something not
// parallel to `forward`), NOT a gravity/world-up assumption. This project's
// "no world +Y" laws are about GRAVITY/local-up semantics (see
// docs/ARCHITECTURE.md's architectural laws); constructing a shadow
// camera's own view-matrix basis is pure geometry with no relationship to
// gravity at all — the same category of "+Y used only to build a
// perpendicular reference, not to encode a real direction" already
// established by Application.cpp's own RotationAligningUpTo (used to
// orient static M10 step/ramp geometry). Falls back to world +X only in
// the near-degenerate case where `forward` is itself nearly parallel to
// +Y, so the resulting basis is never near-singular.
glm::vec3 ChooseShadowUpHint(const glm::vec3& forward);

// A single bounded orthographic shadow frustum for the existing directional
// "sun" light, recentered on `focusPosition` every frame (typically the
// player's own presented position — see docs/ARCHITECTURE.md, "Milestone
// 15, Directional-light shadows," for why a single recentered frustum was
// chosen over cascaded shadow maps or a whole-world-covering one for this
// milestone's small, bounded demo). `lightDirectionToLight` matches this
// engine's existing directional-light convention (points FROM a lit
// surface TOWARD the light — see Renderer::SetLighting). `halfExtent` sets
// the frustum's half-width/half-height (world units); `shadowDistance`
// both places the shadow camera that far from `focusPosition` along
// `lightDirectionToLight` AND sets the far plane at `2 * shadowDistance`
// (comfortably covering the round trip from the light back through
// `focusPosition`).
glm::mat4 ComputeDirectionalShadowMatrix(const glm::vec3& focusPosition,
                                          const glm::vec3& lightDirectionToLight, float halfExtent,
                                          float shadowDistance);

// A perspective shadow frustum for one spotlight (the player torch or the
// spacecraft headlight — see docs/ARCHITECTURE.md, "Milestone 15,
// Spotlight shadows"), placed at `lightPosition` looking along
// `lightDirection` (the direction the light FACES — same convention as
// DynamicLight::direction, see src/Light.h). The vertical field of view is
// derived from `outerConeDegrees` (the spotlight's own outer cone half-
// angle — see src/Light.h) with a small fixed margin, so the shadow
// frustum is never tighter than the light's own illuminated cone (a
// tighter frustum would incorrectly treat the cone's own edge as
// unshadowed, since sampling outside the shadow map's bounds is treated as
// "no occluder" — see Renderer.cpp's fragment shader). `range` sets the far
// plane; the near plane is a small fixed constant.
glm::mat4 ComputeSpotShadowMatrix(const glm::vec3& lightPosition, const glm::vec3& lightDirection,
                                   float outerConeDegrees, float range);
