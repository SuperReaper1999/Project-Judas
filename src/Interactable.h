#pragma once

#include <string>

#include <glm/glm.hpp>

// Milestone 16: the entire Judas-owned "environmental interaction" concept
// — the minimum needed for player discovers X -> HUD shows a prompt ->
// interaction input -> X performs its own action. `PlayerController` and
// `Application`'s interaction-selection logic (see src/InteractionSystem.h)
// understand only THIS interface — never `Door`, never `LightSwitch`, never
// any other concrete type — see docs/ARCHITECTURE.md, "Milestone 16," for
// why this is what keeps the door from being hard-coded into player
// movement code, and what makes the second interactable a genuine proof
// the abstraction isn't secretly `DoorManager`.
//
// Deliberately minimal: no event bus, no reflection, no scripting, no
// entity-component system. A concrete `Interactable` is just an ordinary
// C++ object (owned by `Application.cpp`, the composition root) that
// happens to implement these five methods.
class Interactable {
public:
    virtual ~Interactable() = default;

    // World-space point interaction PROXIMITY is measured against — for a
    // moving interactable (the door, mid-swing), this is its CURRENT
    // authoritative position, not a presented/interpolated one (selection
    // is a gameplay decision, not a rendering one — see
    // docs/ARCHITECTURE.md, "Milestone 16, Architecture").
    virtual glm::vec3 GetInteractionPoint() const = 0;

    // World units — how close the player's own position must be to this
    // point for this object to become selectable at all (before the
    // facing check — see src/InteractionSystem.h).
    virtual float GetInteractionRadius() const = 0;

    // Short prompt text for the HUD (e.g. "Press G to open door") — the
    // HUD draws this string verbatim; it never interprets or inspects it.
    virtual std::string GetPromptText() const = 0;

    // Whether this object can currently be acted on at all, independent
    // of range/facing — checked in ADDITION to selection, so a valid-but-
    // busy interactable (this milestone doesn't actually have one, but
    // the hook exists for a future interactable that might) is never
    // selected/prompted while unable to respond.
    virtual bool CanInteract() const = 0;

    // Performs this object's own defined action. Called only when this
    // object was the SELECTED interactable at the moment of a real
    // interaction request (see src/InteractionSystem.h's
    // SelectInteractable and Application::Run's own input-ownership-gated
    // call site).
    virtual void Interact() = 0;
};
