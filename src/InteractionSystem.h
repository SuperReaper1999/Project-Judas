#pragma once

#include <vector>

#include <glm/glm.hpp>

class Interactable;

// Milestone 16: pure selection logic — given the player's own current
// position/look direction and a list of candidate `Interactable`s, decides
// which ONE (if any) is currently the target: close enough
// (`GetInteractionRadius`) AND roughly faced (within a fixed facing cone —
// "physically/spatially meaningful" detection without needing a real
// raycast/line-of-sight query, per this milestone's own brief). The
// CLOSEST qualifying candidate wins when more than one does. Deliberately
// a free function, not a class — no state to own, directly testable with
// hand-crafted candidates (see tests/InteractableTests.cpp), the same
// "small testable free function" shape src/LightTransforms.h/
// src/ShadowTransforms.h already established.
//
// Returns nullptr when nothing qualifies — this IS "stop offering
// interaction when it is no longer valid": the caller (Application::Run)
// simply stops showing a prompt / stops accepting an interact press the
// very next time this is called and nothing qualifies anymore, there is
// no separate "currently targeting" state to explicitly clear.
Interactable* SelectInteractable(const glm::vec3& playerPosition, const glm::vec3& lookDirection,
                                  const std::vector<Interactable*>& candidates);
