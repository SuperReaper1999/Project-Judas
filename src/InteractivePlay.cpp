#include "InteractivePlay.h"

#include <algorithm>
#include <chrono>

#include "GameplayHud.h"
#include "Renderer.h"
#include "RuntimeWorld.h"
#include "SimulationTiming.h"
#include "Window.h"
#include "WorldPresentation.h"

namespace {
using Clock = std::chrono::steady_clock;
double MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
}  // namespace

bool InteractivePlay::Begin(RuntimeWorld& world, const WorldCoordinates& worldCoordinates, std::string& outError) {
    End();
    m_worldCoordinates = worldCoordinates;
    if (!m_session.Begin(world, outError)) return false;
    m_pauseMenu = PauseMenu();
    m_physicsAccumulator = 0.0f;
    m_fixedStepsSinceReset = 0;
    m_resetOccurred = false;
    m_wasPauseMenuOpen = false;
    m_lastAerodynamicDrag = {};
    return true;
}

void InteractivePlay::End() {
    m_session.End();
}

void InteractivePlay::SetFixedStepMeasurementFlags(bool atmosphere, bool fire, bool fluid) {
    m_measurements.measureAtmosphere = atmosphere;
    m_measurements.measureFire = fire;
    m_measurements.measureFluid = fluid;
}

bool InteractivePlay::ConsumeResetOccurred() {
    const bool occurred = m_resetOccurred;
    m_resetOccurred = false;
    return occurred;
}

float InteractivePlay::Frame(Window& window, Renderer& renderer, float frameDeltaTime, bool drawHud) {
    RuntimeWorld& world = m_session.World();

    // --- Milestone 13: the single input-routing boundary ---
    // Everything UI-related is handled here, before any gameplay system
    // sees this frame's input. Gameplay code is never told a menu exists;
    // it simply isn't called while one owns input.
    if (window.ConsumeUIBackRequest()) m_pauseMenu.HandleBackRequest();
    const bool uiUp = window.ConsumeUINavigateUpRequest();
    const bool uiDown = window.ConsumeUINavigateDownRequest();
    const bool uiActivate = window.ConsumeUIActivateRequest();
    int uiClickX = 0, uiClickY = 0;
    const bool uiClicked = window.ConsumeUIClickRequest(uiClickX, uiClickY);
    if (m_pauseMenu.IsOpen()) {
        m_pauseMenu.Layout(window.Width(), window.Height());
        if (uiUp) m_pauseMenu.NavigateUp();
        if (uiDown) m_pauseMenu.NavigateDown();
        if (uiActivate) m_pauseMenu.Activate();
        int mouseX = 0, mouseY = 0;
        window.GetMousePosition(mouseX, mouseY);
        m_pauseMenu.HandleMouseMove(glm::vec2(static_cast<float>(mouseX), static_cast<float>(mouseY)));
        if (uiClicked) {
            m_pauseMenu.HandleMouseClick(glm::vec2(static_cast<float>(uiClickX), static_cast<float>(uiClickY)));
        }
    }
    if (m_pauseMenu.IsOpen() != m_wasPauseMenuOpen) {
        window.SetMouseCaptured(!m_pauseMenu.IsOpen());
        m_wasPauseMenuOpen = m_pauseMenu.IsOpen();
    }
    // Edge requests are drained every frame regardless of pause state so a
    // press while the menu owns input can never fire on resume.
    const bool torchToggleRequested = window.ConsumeTorchToggleRequest();
    const bool interactRequested = window.ConsumeInteractRequest();
    const bool viewToggleRequested = window.ConsumeViewToggleRequest();
    const bool throwRequested = window.ConsumeThrowRequest();
    const bool sasToggleRequested = window.ConsumeSasToggleRequest();
    m_session.UpdateInteractionTarget();

    frameDeltaTime = std::min(frameDeltaTime, SimulationTiming::kMaxFrameDeltaTime);

    // Milestone 13 pause policy: while the menu is open nothing advances —
    // no input, no accumulator, no steps.
    if (!m_pauseMenu.IsOpen()) {
        m_session.HandleFrameInput(window, torchToggleRequested, interactRequested, viewToggleRequested,
                                   throwRequested, sasToggleRequested);
        if (m_session.ConsumeResetOccurred()) {
            m_lastAerodynamicDrag = {};
            m_physicsAccumulator = 0.0f;
            m_fixedStepsSinceReset = 0;
            m_resetOccurred = true;
        }
        m_physicsAccumulator += frameDeltaTime;
        int stepsThisFrame = 0;
        const bool measuring = m_measurements.measureAtmosphere || m_measurements.measureFire ||
                               m_measurements.measureFluid;
        while (m_physicsAccumulator >= SimulationTiming::kFixedTimestep &&
               stepsThisFrame < SimulationTiming::kMaxPhysicsStepsPerFrame) {
            const auto stepStart = measuring ? Clock::now() : Clock::time_point{};
            m_measurements.atmosphereMeasured = m_measurements.fireMeasured = m_measurements.fluidMeasured = false;
            StepPlayedWorld(m_session, window, SimulationTiming::kFixedTimestep, &m_measurements);
            m_lastAerodynamicDrag = m_measurements.lastAerodynamicDrag;
            if (m_observer) m_observer(m_measurements, measuring ? MillisecondsSince(stepStart) : 0.0);
            m_physicsAccumulator -= SimulationTiming::kFixedTimestep;
            ++stepsThisFrame;
            ++m_fixedStepsSinceReset;
        }
        // Hit the catch-up cap: drop the backlog instead of compounding it.
        if (stepsThisFrame == SimulationTiming::kMaxPhysicsStepsPerFrame) m_physicsAccumulator = 0.0f;
    }

    // How far real time has progressed into an as-yet-unsimulated fixed
    // step — the presentation interpolation factor (M6).
    const float presentationAlpha = m_physicsAccumulator / SimulationTiming::kFixedTimestep;
    const auto surfaceStart = Clock::now();
    UpdateFluidSurface(renderer, world, presentationAlpha);
    m_lastSurfaceMilliseconds = MillisecondsSince(surfaceStart);

    const int height = std::max(window.Height(), 1);
    const float aspectRatio = static_cast<float>(window.Width()) / static_cast<float>(height);
    const PlayerController& player = m_session.Player();
    glm::mat4 view;
    if (m_session.IsPiloting()) {
        // Milestone 8: anchor the same camera to the vehicle's presented
        // pose while piloting.
        const DynamicBody& ship = world.DynamicBodies()[world.GetVehicle()->dynamicIndex];
        view = player.GetViewMatrix(ship.GetPresentedPosition(presentationAlpha),
                                    ship.GetPresentedOrientation(presentationAlpha));
    } else {
        view = player.GetViewMatrix(presentationAlpha, m_session.ViewMode());
    }
    const auto sceneStart = Clock::now();
    RenderWorldFrame(renderer, window.Width(), window.Height(), world, &m_session, view,
                     player.GetProjectionMatrix(aspectRatio), player.GetPresentedPosition(presentationAlpha),
                     presentationAlpha);
    m_lastSceneMilliseconds = MillisecondsSince(sceneStart);

    // Milestone 13: HUD + pause menu overlay, drawn last, on top.
    renderer.BeginUIFrame(window.Width(), window.Height());
    if (drawHud && m_pauseMenu.IsHudVisible()) {
        m_hud.Draw(renderer, window.Width(), window.Height(),
                   BuildHudView(m_session, m_worldCoordinates, m_lastAerodynamicDrag));
    }
    m_pauseMenu.Draw(renderer, window.Width(), window.Height());
    renderer.EndUIFrame();
    return presentationAlpha;
}
