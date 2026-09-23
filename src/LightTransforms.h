#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Milestone 14: pure, stateless geometry shared by every dynamic light
// this milestone attaches to a moving/rotating owner (the player torch,
// the spacecraft's headlight/nav lights — see docs/ARCHITECTURE.md,
// "Milestone 14, Dynamic means dynamic"). Factored out of both
// PlayerController.cpp and Application.cpp specifically so it's directly
// unit-testable with hand-crafted poses (including rotated ones) without
// needing a live PhysicsWorld/Window to drive a real player or spacecraft
// through — see tests/LightingTests.cpp. No gameplay state, no GL, no
// knowledge that a "light" or a "player" or a "spacecraft" exists —
// exactly the kind of small, engine-agnostic math free function this
// project already keeps separate from the classes that use it (compare
// DynamicBody.h's PrepareDynamicBodiesForStep, PilotAttachment.h's
// ComputePilotReleaseVelocity).

// worldPosition = ownerPosition + ownerOrientation * localOffset
// The exact formula the brief itself specifies for attaching a light to a
// moving/rotating owner — see docs/ARCHITECTURE.md, "Milestone 14,
// Spacecraft lights."
glm::vec3 TransformLocalLightPosition(const glm::vec3& ownerPosition, const glm::quat& ownerOrientation,
                                       const glm::vec3& localOffset);

// worldDirection = normalize(ownerOrientation * localDirection)
glm::vec3 TransformLocalLightDirection(const glm::quat& ownerOrientation,
                                        const glm::vec3& localDirection);

// The player torch's world-space eye position/look direction, given a
// base position/orientation (the player's own PRESENTED transform — see
// PlayerController::GetTorchTransform) and the SAME free-look yaw/pitch
// composition BuildViewMatrix/GetLookDirection already use (mouse look is
// applied on top of the base orientation, never baked into it). Never
// assumes a world axis: `baseOrientation` may be any quaternion (see
// tests/LightingTests.cpp's own rotate-the-universe check).
void ComputeTorchTransform(const glm::vec3& basePosition, const glm::quat& baseOrientation,
                            float yawDegrees, float pitchDegrees, float eyeHeight,
                            glm::vec3& outPosition, glm::vec3& outDirection);
