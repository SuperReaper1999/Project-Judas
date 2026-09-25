#pragma once

#include <string>
#include <vector>

#include "EntityLifecycle.h"

#include "FlyingPrimitiveControl.h"
#include "ObjectManipulation.h"
#include "PilotAttachment.h"
#include "PlayerController.h"
#include "PlayerView.h"

class Interactable;
class RuntimeWorld;
class Window;

// Milestone 28: the gameplay layer over a RuntimeWorld — the one player,
// its view mode and torch, control of the world's vehicle (M8/M11 pilot
// attachment, M21 SAS), M18 object carrying, and M16 interaction
// targeting. Everything here is per-run state that exists only while a
// scene is being played; the authored Scene knows none of it beyond the
// player-start object it reads its spawn from.
//
// This is deliberately not "the game": the fixed-step ordering that
// advances it lives in src/Simulation.h and the loop that drives that in
// Application/EditorApplication. GameSession only owns the gameplay
// participants and the small input decisions that belong to them.
class GameSession {
public:
    GameSession() = default;
    ~GameSession();
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    // Spawns the player at the world's player-start (or the origin when a
    // scene has none), wires pick-up targets and interactables, and applies
    // the vehicle's initial pilot attachment if authored.
    bool Begin(RuntimeWorld& world, std::string& outError);
    void End();
    bool IsActive() const { return m_world != nullptr; }

    // The full R-key reset: authored world state, player at spawn, control
    // released, SAS off, held object dropped, initial attachment reapplied.
    void ResetToAuthoredState();

    // Render-frame gameplay input that the Milestone 13 input boundary lets
    // through (never called while a menu owns input): view toggle, mouse
    // look/jump latching, pilot toggle, SAS, torch, interact/drop/throw.
    // `igniterHeld` etc. are read from the window here so no gameplay
    // system ever learns that a menu exists.
    void HandleFrameInput(Window& window, bool torchToggleRequested, bool interactRequested,
                          bool viewToggleRequested, bool throwRequested, bool sasToggleRequested);

    // Recomputes the current interaction target from the player's pose.
    // Safe to call while paused (read-only).
    void UpdateInteractionTarget();

    RuntimeWorld& World() { return *m_world; }
    const RuntimeWorld& World() const { return *m_world; }
    PlayerController& Player() { return *m_player; }
    const PlayerController& Player() const { return *m_player; }
    FlyingPrimitiveControl& VehicleControl() { return m_vehicleControl; }
    const FlyingPrimitiveControl& VehicleControl() const { return m_vehicleControl; }
    PilotAttachment& Attachment() { return m_pilotAttachment; }
    const PilotAttachment& Attachment() const { return m_pilotAttachment; }
    ObjectManipulation& Manipulation() { return *m_manipulation; }
    const ObjectManipulation& Manipulation() const { return *m_manipulation; }
    bool HasVehicle() const { return m_vehicleControl.handle.IsValid(); }
    bool IsPiloting() const { return HasVehicle() && m_vehicleControl.controlled; }
    PlayerViewMode ViewMode() const { return m_viewMode; }
    bool TorchOn() const { return m_torchOn; }
    bool IgniterPowered() const { return m_igniterPowered; }
    void SetIgniterPowered(bool powered) { m_igniterPowered = powered; }
    const Interactable* InteractionTarget() const { return m_interactTarget; }
    // True once after each ResetToAuthoredState, so the loop driving this
    // session can clear its own per-run bookkeeping (accumulator, timers).
    bool ConsumeResetOccurred();

    // Milestone 29: entities gameplay needs at Full fidelity this step (the
    // held object, whatever supports the player), handed to the policy.
    std::vector<EntityId> PinnedEntities() const;
    // Re-derives handle-keyed gameplay lists (pick-up eligibility,
    // interaction targets) when the world's entity set changed.
    void RefreshEntityBindings();
    // Z: creates a persistent pickable crate ahead of the player. Returns
    // the new entity id or the invalid id.
    EntityId SpawnPersistentEntity();
    // Y: permanently destroys the targeted pickable entity (or the held
    // one). Returns false when nothing eligible is targeted.
    bool DestroyTargetedEntity();
    const std::string& LastLifecycleMessage() const { return m_lastLifecycleMessage; }
    void SetLastLifecycleMessage(const std::string& message) { m_lastLifecycleMessage = message; }

private:
    RuntimeWorld* m_world = nullptr;
    std::unique_ptr<PlayerController> m_player;
    std::unique_ptr<ObjectManipulation> m_manipulation;
    std::vector<PickupInteractable> m_pickupTargets;
    std::vector<Interactable*> m_interactables;
    Interactable* m_interactTarget = nullptr;
    FlyingPrimitiveControl m_vehicleControl;
    PilotAttachment m_pilotAttachment;
    PlayerViewMode m_viewMode = PlayerViewMode::ThirdPerson;
    bool m_torchOn = false;
    bool m_igniterPowered = false;
    bool m_initialPilotAttached = false;
    bool m_resetOccurred = false;
    unsigned int m_boundEntityVersion = 0;
    std::string m_lastLifecycleMessage;
};
