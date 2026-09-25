#include "GameSession.h"

#include "InteractionSystem.h"
#include "PilotControl.h"
#include "RuntimeWorld.h"
#include "Window.h"

GameSession::~GameSession() {
    End();
}

bool GameSession::Begin(RuntimeWorld& world, std::string& outError) {
    End();
    m_world = &world;

    glm::vec3 spawn(0.0f);
    float yaw = 0.0f;
    m_viewMode = PlayerViewMode::ThirdPerson;
    if (world.GetPlayerStart()) {
        spawn = world.GetPlayerStart()->position;
        yaw = world.GetPlayerStart()->yawDegrees;
        m_viewMode = world.GetPlayerStart()->view == ScenePlayerView::FirstPerson
                         ? PlayerViewMode::FirstPerson
                         : PlayerViewMode::ThirdPerson;
    }
    m_player = std::make_unique<PlayerController>(spawn, yaw);
    if (!m_player->Spawn(world.Physics())) {
        outError = "player spawn failed";
        m_player.reset();
        m_world = nullptr;
        return false;
    }

    // M18: the authored pickable whitelist; PickupInteractable references
    // its DynamicBody, so the targets are built after every body exists.
    m_manipulation = std::make_unique<ObjectManipulation>(world.PickableBodies());
    m_pickupTargets.reserve(world.PickableBodies().size());
    std::vector<DynamicBody>& bodies = world.DynamicBodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (world.DynamicVisuals()[i].pickable) {
            m_pickupTargets.emplace_back(bodies[i], *m_manipulation, world.Physics());
        }
    }
    for (Door& door : world.Doors()) m_interactables.push_back(&door);
    for (LightSwitch& lightSwitch : world.LightSwitches()) m_interactables.push_back(&lightSwitch);
    for (PickupInteractable& target : m_pickupTargets) m_interactables.push_back(&target);

    m_vehicleControl = FlyingPrimitiveControl{};
    m_pilotAttachment = PilotAttachment{};
    m_initialPilotAttached = false;
    if (world.GetVehicle()) {
        m_vehicleControl.handle = world.GetVehicle()->handle;
        m_initialPilotAttached = world.GetVehicle()->component.initialPilotAttached;
        if (m_initialPilotAttached) {
            BeginPilotAttachment(m_pilotAttachment, world.Physics().GetTransform(m_vehicleControl.handle),
                                 m_player->GetPosition(), m_player->GetOrientation());
            m_vehicleControl.controlled = true;
        }
    }
    m_torchOn = false;
    m_igniterPowered = false;
    m_interactTarget = nullptr;
    return true;
}

void GameSession::End() {
    if (!m_world) return;
    if (m_player && m_world->IsBuilt()) m_player->Destroy(m_world->Physics());
    m_player.reset();
    m_interactables.clear();
    m_pickupTargets.clear();
    m_manipulation.reset();
    m_interactTarget = nullptr;
    m_vehicleControl = FlyingPrimitiveControl{};
    m_pilotAttachment = PilotAttachment{};
    m_world = nullptr;
}

void GameSession::ResetToAuthoredState() {
    if (!m_world) return;
    m_manipulation->Drop();
    m_player->Reset();
    m_world->RestoreAuthoredState();
    m_vehicleControl.controlled = false;
    m_pilotAttachment.attached = false;
    if (HasVehicle()) {
        SetSpacecraftSasEnabled(m_vehicleControl, false, m_world->Physics());
        if (m_initialPilotAttached) {
            BeginPilotAttachment(m_pilotAttachment,
                                 m_world->Physics().GetTransform(m_vehicleControl.handle),
                                 m_player->GetPosition(), m_player->GetOrientation());
            m_vehicleControl.controlled = true;
        }
    }
    m_igniterPowered = false;
    m_resetOccurred = true;
}

bool GameSession::ConsumeResetOccurred() {
    const bool occurred = m_resetOccurred;
    m_resetOccurred = false;
    return occurred;
}

void GameSession::UpdateInteractionTarget() {
    if (!m_world) return;
    m_interactTarget = SelectInteractable(m_player->GetPosition(), m_player->GetLookDirection(),
                                          m_interactables);
}

void GameSession::HandleFrameInput(Window& window, bool torchToggleRequested, bool interactRequested,
                                   bool viewToggleRequested, bool throwRequested,
                                   bool sasToggleRequested) {
    if (!m_world) return;
    ApplyPlayerViewToggle(m_viewMode, viewToggleRequested, /*gameplayOwnsInput=*/true);
    // Mouse look and jump-key latching happen every render frame,
    // independent of how many fixed physics steps run this frame.
    m_player->UpdateFrameInput(window);

    if (window.ConsumeResetRequest()) ResetToAuthoredState();

    // Milestone 8/11: F toggles input authority (and the secured-pilot
    // attachment) between the player and the vehicle. Taking control is
    // gated on the player's own current support state.
    if (window.ConsumeControlToggleRequest() && HasVehicle()) {
        const bool wasControlled = m_vehicleControl.controlled;
        HandlePilotToggleRequest(m_vehicleControl, m_pilotAttachment, *m_player, m_world->Physics(),
                                 m_world->Gravity());
        if (!wasControlled && m_vehicleControl.controlled) m_manipulation->Drop();
    }
    // M21: SAS only while the player owns spacecraft controls.
    if (sasToggleRequested && IsPiloting()) {
        SetSpacecraftSasEnabled(m_vehicleControl, !m_vehicleControl.sasEnabled, m_world->Physics());
    }
    // M27: the held heater only while gameplay owns input and the player
    // is on foot; there is no edge request that can survive a pause menu.
    m_igniterPowered = !m_world->Combustibles().empty() && !IsPiloting() &&
                       window.IsActionActive(Action::UseIgniter);

    if (torchToggleRequested) m_torchOn = !m_torchOn;

    // Milestone 16: G triggers the selected interaction; M18 reuses that
    // path for pickup, and drops when nothing else is targeted.
    if (interactRequested && m_interactTarget && m_interactTarget->CanInteract()) {
        m_interactTarget->Interact();
    } else if (interactRequested && m_manipulation->IsHolding() && !IsPiloting()) {
        m_manipulation->Drop();
    }
    if (throwRequested && !IsPiloting()) {
        m_manipulation->Throw(m_world->Physics(), m_player->GetLookDirection(), 8.0f);
    }
}
