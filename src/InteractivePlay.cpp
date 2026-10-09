#include "PerformanceProfiler.h"
#include "ScriptSystem.h"
#include "InteractivePlay.h"

#include <algorithm>
#include <chrono>

#include <cstdio>

#include "GameplayHud.h"
#include "Renderer.h"
#include "WorldState.h"
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
    PerformanceProfiler::Get().Boundary("Play begin");
    End();
    m_worldCoordinates = worldCoordinates;
    if (!m_session.Begin(world, outError)) return false;
    m_pauseMenu.Reset();
    m_physicsAccumulator = 0.0f;
    m_fixedStepsSinceReset = 0;
    m_resetOccurred = false;
    m_wasPauseMenuOpen = false;
    m_captureInitialized = false;
    m_lastAerodynamicDrag = {};
    world.BeginAudio();
    return true;
}

void InteractivePlay::End() {
 PerformanceProfiler::Get().Boundary("Play end");
    if(m_session.IsActive()){m_session.World().EndScripts();m_session.World().EndAudio();}
    m_session.End();
}

void InteractivePlay::SetWorldStatePath(const std::string& path, bool loadedFromFile) {
    m_worldStatePath = path;
    m_worldStateStatus = path.empty() ? std::string() : path + (loadedFromFile ? " (loaded)" : " (none saved)");
}

bool InteractivePlay::SaveWorldStateNow(std::string& outMessage) {
    if (m_worldStatePath.empty()) {
        outMessage = "No world-state path for this scene (unsaved scene?)";
        return false;
    }
    std::string error;
    if(!CanCaptureLegacyWorldState(m_session.World(),error)){outMessage="Save failed: "+error;return false;}
    const WorldState state = CaptureWorldState(m_session.World());
    if (!SaveWorldStateToFile(state, m_worldStatePath, error)) {
        outMessage = "Save failed: " + error;
        return false;
    }
    outMessage = "Saved world state (" + std::to_string(state.entities.size()) + " entity, " +
                 std::to_string(state.interactables.size()) + " interactable changes) to " + m_worldStatePath;
    m_worldStateStatus = m_worldStatePath + " (saved)";
    return true;
}

bool InteractivePlay::DeleteWorldStateNow(std::string& outMessage) {
    if (m_worldStatePath.empty()) {
        outMessage = "No world-state path for this scene";
        return false;
    }
    if (std::remove(m_worldStatePath.c_str()) != 0) {
        outMessage = "No saved world state to delete at " + m_worldStatePath;
        m_worldStateStatus = m_worldStatePath + " (none saved)";
        return false;
    }
    outMessage = "Deleted " + m_worldStatePath + "; the next launch is the pristine baseline";
    m_worldStateStatus = m_worldStatePath + " (deleted)";
    return true;
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

float InteractivePlay::Frame(Window& window, Renderer& renderer, float frameDeltaTime, bool drawHud, bool render) {
    JUDAS_PROFILE_SCOPE("Play frame");
    RuntimeWorld& world = m_session.World();

    // Paired controller observations belong to this play session. A menu's
    // resume frame must not replay stick movement delivered while it owned input.
    if (!m_captureInitialized || IsPaused()) window.Input().DiscardStickHistory();

    // --- Milestone 13: the single input-routing boundary ---
    // Everything UI-related is handled here, before any gameplay system
    // sees this frame's input. Gameplay code is never told a menu exists;
    // it simply isn't called while one owns input.
    world.UpdateUIScripts(&window.Input(),frameDeltaTime);
    const bool authoredUI=world.UIIfLoaded()&&!world.UIIfLoaded()->Empty();
    bool uiOwned=false;
    if(authoredUI){auto& ui=world.UI();uiOwned=ui.OwnsInput();int x,y;window.GetMousePosition(x,y);ui.Input(window.Input(),{x,y},!window.IsMouseCaptured(),window.Width(),window.Height());world.DispatchUIEvents(&window.Input(),frameDeltaTime);uiOwned|=ui.OwnsInput();}
    if (window.ConsumeUIBackRequest()&&!authoredUI&&world.legacyGameplay) m_pauseMenu.HandleBackRequest();
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
    // A replacement scene may begin after an outgoing modal menu released
    // the cursor. Reconcile ownership after the new scene's UI starts.
    const bool capture = m_pointerCaptureAllowed && !IsPaused() && (world.legacyGameplay || world.pointerCapture);
    if (!m_captureInitialized || window.IsMouseCaptured() != capture) {
        window.SetMouseCaptured(capture);
        m_wasPauseMenuOpen = IsPaused();
        m_captureInitialized = true;
    }
    // Edge requests are drained every frame regardless of pause state so a
    // press while the menu owns input can never fire on resume.
    const bool torchToggleRequested = window.ConsumeTorchToggleRequest();
    const bool interactRequested = window.ConsumeInteractRequest();
    const bool viewToggleRequested = window.ConsumeViewToggleRequest();
    const bool throwRequested = window.ConsumeThrowRequest();
    const bool sasToggleRequested = window.ConsumeSasToggleRequest();
    // Milestone 29: persistence keys work whether or not the menu is open —
    // saving a frozen world is exactly when you want it.
    if (world.legacyGameplay && window.ConsumeSaveWorldStateRequest()) {
        std::string message;
        SaveWorldStateNow(message);
        m_session.SetLastLifecycleMessage(message);
    }
    if (world.legacyGameplay && window.ConsumeDeleteWorldStateRequest()) {
        std::string message;
        DeleteWorldStateNow(message);
        m_session.SetLastLifecycleMessage(message);
    }
    m_session.UpdateInteractionTarget();

    frameDeltaTime = std::min(frameDeltaTime, SimulationTiming::kMaxFrameDeltaTime);

    // Milestone 13 pause policy: while the menu is open nothing advances —
    // no input, no accumulator, no steps.
    bool profileCap=false; double profileDiscarded=0;
    if (!IsPaused()&&!uiOwned) {
        m_session.HandleFrameInput(window, torchToggleRequested, interactRequested, viewToggleRequested,
                                   throwRequested, sasToggleRequested, frameDeltaTime);
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
            window.Input().BeginFixedStep();
            const auto stepStart = Clock::now();
            m_measurements.atmosphereMeasured = m_measurements.fireMeasured = m_measurements.fluidMeasured = false;
            StepPlayedWorld(m_session, window, SimulationTiming::kFixedTimestep, &m_measurements);
            m_lastAerodynamicDrag = m_measurements.lastAerodynamicDrag;
            m_lastStepMilliseconds = MillisecondsSince(stepStart);
            if (m_observer) m_observer(m_measurements, measuring ? m_lastStepMilliseconds : 0.0);
            m_physicsAccumulator -= SimulationTiming::kFixedTimestep;
            ++stepsThisFrame;
            ++m_fixedStepsSinceReset;
        }
        // Hit the catch-up cap: drop the backlog instead of compounding it.
        if (stepsThisFrame == SimulationTiming::kMaxPhysicsStepsPerFrame) { profileCap=true; profileDiscarded=m_physicsAccumulator; m_physicsAccumulator = 0.0f; }
        m_lastStepsThisFrame = stepsThisFrame;
    } else {
        window.ClearPendingRequests();
        m_lastStepsThisFrame = 0;
    }

    // How far real time has progressed into an as-yet-unsimulated fixed
    // step — the presentation interpolation factor (M6).
    PerformanceProfiler::Get().FixedState(m_physicsAccumulator,profileCap,profileDiscarded,IsPaused()||uiOwned);
    const float presentationAlpha = m_physicsAccumulator / SimulationTiming::kFixedTimestep;
    world.PresentationScripts(&window.Input(),frameDeltaTime,presentationAlpha);
    const int height = std::max(window.Height(), 1);
    const float aspectRatio = static_cast<float>(window.Width()) / static_cast<float>(height);
    const PlayerController& player = m_session.Player();
    glm::mat4 view(1.0f);
    if (world.legacyGameplay && m_session.IsPiloting()) {
        // Milestone 8: anchor the same camera to the vehicle's presented
        // pose while piloting.
        const DynamicBody& ship = world.DynamicBodies()[world.GetVehicle()->dynamicIndex];
        view = player.GetViewMatrix(ship.GetPresentedPosition(presentationAlpha),
                                    ship.GetPresentedOrientation(presentationAlpha));
    } else if (world.legacyGameplay) {
        view = player.GetViewMatrix(presentationAlpha, m_session.ViewMode());
    }
    if(world.view){const auto& v=*world.view;view=glm::lookAt(v.pose.position,v.pose.position+v.pose.rotation*glm::vec3(0,0,-1),v.pose.rotation*glm::vec3(0,1,0));}
    world.viewportWidth=window.Width();world.viewportHeight=height;
    if(world.Scripts())world.Scripts()->SetView(view);
    world.UpdateAudio(view,presentationAlpha,frameDeltaTime);
    // Audio publication is a runtime service, independent of drawing/capture.
    if (!render) return presentationAlpha;
    const auto surfaceStart = Clock::now();
    UpdateFluidSurface(renderer, world, presentationAlpha);
    m_lastSurfaceMilliseconds = MillisecondsSince(surfaceStart);
    const auto sceneStart = Clock::now();
    RenderWorldFrame(renderer, window.Width(), window.Height(), world, &m_session, view,
                     player.GetProjectionMatrix(aspectRatio), player.GetPresentedPosition(presentationAlpha),
                     presentationAlpha);
    m_lastSceneMilliseconds = MillisecondsSince(sceneStart);
    if (m_overlay) m_overlay(renderer, presentationAlpha);

    // Milestone 13: HUD + pause menu overlay, drawn last, on top.
    renderer.BeginUIFrame(window.Width(), window.Height());
    if (drawHud && m_pauseMenu.IsHudVisible() && (!authoredUI||world.UI().debugOverlayVisible)) {
        HUDViewData hud = BuildHudView(m_session, m_worldCoordinates, m_lastAerodynamicDrag);
        hud.worldStateInfo = m_worldStateStatus;
        hud.lifecycleMessage = m_session.LastLifecycleMessage();
        if (!hud.worldStateInfo.empty() || !hud.lifecycleMessage.empty()) hud.lifecycleAvailable = true;
        m_hud.Draw(renderer, window.Width(), window.Height(), hud);
    }
    if(world.legacyGameplay&&!authoredUI)m_pauseMenu.Draw(renderer, window.Width(), window.Height());
    if(authoredUI)world.UI().Draw(renderer,window.Width(),window.Height());
    renderer.EndUIFrame();
    return presentationAlpha;
}

bool InteractivePlay::IsPaused()const{return m_pauseMenu.IsOpen()||(m_session.IsActive()&&m_session.World().UIIfLoaded()&&m_session.World().UIIfLoaded()->Paused());}
bool InteractivePlay::QuitRequested()const{return m_pauseMenu.QuitRequested()||(m_session.IsActive()&&m_session.World().UIIfLoaded()&&m_session.World().UIIfLoaded()->QuitRequested());}
