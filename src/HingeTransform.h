#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Milestone 16: pure, stateless hinge-rotation math shared by every
// interactable that swings about a pivot (the door, and the light
// switch's own small lever — see src/Door.h/src/LightSwitch.h). Factored
// out specifically so it's directly unit-testable with hand-crafted
// (including rotated) authored poses — see tests/InteractableTests.cpp —
// the same "small testable free function" pattern src/LightTransforms.h
// (M14) and src/ShadowTransforms.h (M15) already established.
//
// Rotates a box's CLOSED-pose center around a PIVOT POINT (not the box's
// own center — a door swings about its hinge edge, not its middle) by
// `angleRadians` about `hingeAxisWorld`. `hingeAxisWorld` must already be
// a WORLD-space axis — callers derive it from the object's own authored
// orientation (`baseOrientation * localHingeAxis`, normalized) exactly
// once, at construction, never from a fixed world direction — see
// docs/ARCHITECTURE.md, "Milestone 16," for why this is what "the door
// must work relative to its own authored transform; do not assume world
// Y is the hinge/up axis" means in practice.
//
// The math: rotating a point `p` by world rotation `R` about pivot `c` is
// `c + R * (p - c)`; the object's own orientation rotates by the same `R`,
// composed on the LEFT (a world-space rotation applied on top of whatever
// the object's current orientation already is).
void ComputeHingeTransform(const glm::vec3& baseCenter, const glm::quat& baseOrientation,
                            const glm::vec3& pivotWorld, const glm::vec3& hingeAxisWorld,
                            float angleRadians, glm::vec3& outPosition, glm::quat& outOrientation);
