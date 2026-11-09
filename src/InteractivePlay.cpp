#include "PerformanceProfiler.h"
#include "ScriptSystem.h"
#include "ScriptFaultNotice.h"
#include "ResourceManager.h"
#include "InteractivePlay.h"

#include <algorithm>
#include <chrono>

#include <cstdio>

#include "Renderer.h"
#include "CameraProjection.h"
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
    m_scriptFaultNotice=m_scriptFaultNoticeOverride.value_or(DefaultScriptFaultNoticeEnabled());
    PerformanceProfiler::Get().Boundary("Play begin");
    End();
    m_worldCoordinates = worldCoordinates;
    m_world = &world;
    outError.clear();
    m_physicsAccumulator = 0.0f;
    m_fixedStepsSinceReset = 0;
    m_wasPauseMenuOpen = false;
    m_captureInitialized = false;
    world.BeginAudio();
    return true;
}

void InteractivePlay::End() {
 PerformanceProfiler::Get().Boundary("Play end");
    if(m_world){m_world->EndScripts();m_world->EndAudio();}
    m_world = nullptr;
}

void InteractivePlay::SetFixedStepMeasurementFlags(bool fire, bool fluid) {
    m_measurements.measureFire = fire;
    m_measurements.measureFluid = fluid;
}



float InteractivePlay::Frame(Window& window, Renderer& renderer, float frameDeltaTime, bool render, const RenderTargetHandle* destination, bool auxiliary) {
    JUDAS_PROFILE_SCOPE("Play frame");
    RuntimeWorld& world = *m_world;
    world.PumpNetwork(std::chrono::duration<double>(Clock::now().time_since_epoch()).count());

    // Paired controller observations belong to this play session. A menu's
    // resume frame must not replay stick movement delivered while it owned input.
    if (!m_captureInitialized || IsPaused()) window.Input().DiscardStickHistory();

    // --- Milestone 13: the single input-routing boundary ---
    // Everything UI-related is handled here, before any gameplay system
    // sees this frame's input. Gameplay code is never told a menu exists;
    // it simply isn't called while one owns input.
    world.viewportWidth=window.InputWidth();world.viewportHeight=std::max(1,window.InputHeight());
    if(auto* players=world.PlayersIfLoaded())if(auto devices=window.Input().localDevices.lock()){players->Route(*devices);if(world.UIIfLoaded())for(auto& p:players->All())world.UI().PlayerViewport(p.first,p.second.viewport);}
    window.Input().SuspendRumble(IsPaused());
    world.UpdateUIScripts(&window.Input(),frameDeltaTime);
    const bool authoredUI=world.UIIfLoaded()&&!world.UIIfLoaded()->Empty();
    bool uiOwned=false;
    if(authoredUI){auto& ui=world.UI();uiOwned=ui.OwnsInput();int x,y;window.GetMousePosition(x,y);ui.Input(window.Input(),{x,y},!window.IsMouseCaptured(),window.InputWidth(),window.InputHeight());if(!ui.OwnsInput())if(auto* players=world.PlayersIfLoaded())for(auto& p:players->All()){const auto pointer=p.second.input.Pointer();ui.Input(p.second.input,{pointer.x*window.InputWidth(),pointer.y*window.InputHeight()},!window.IsMouseCaptured(),window.InputWidth(),window.InputHeight(),p.first);}
    world.DispatchUIEvents(&window.Input(),frameDeltaTime);uiOwned|=ui.OwnsInput();}
    auto textTarget=world.UIIfLoaded()?world.UI().TextTarget():std::optional<UITextTarget>{};if(textTarget)window.SetTextInput(true,textTarget->player,int(textTarget->rect.position.x),int(textTarget->rect.position.y),int(textTarget->rect.size.x),int(textTarget->rect.size.y),textTarget->token);else window.SetTextInput(false);
    if(world.Scripts())world.Scripts()->DrainSignals(&window.Input(),frameDeltaTime,true);
    uiOwned|=world.UIIfLoaded()&&world.UIIfLoaded()->OwnsInput();
    window.Input().SuspendRumble(IsPaused());
    // A replacement scene may begin after an outgoing modal menu released
    // the cursor. Reconcile ownership after the new scene's UI starts.
    const bool capture = m_pointerCaptureAllowed && !IsPaused() && world.pointerCapture;
    if (!m_captureInitialized || window.IsMouseCaptured() != capture) {
        window.SetMouseCaptured(capture);
        m_wasPauseMenuOpen = IsPaused();
        m_captureInitialized = true;
    }
    frameDeltaTime = std::min(frameDeltaTime, SimulationTiming::kMaxFrameDeltaTime);

    // Milestone 13 pause policy: while the menu is open nothing advances —
    // no input, no accumulator, no steps.
    bool profileCap=false; double profileDiscarded=0;
    if (!IsPaused()&&!uiOwned) {
        world.UpdateScripts(&window.Input(),frameDeltaTime);
        m_physicsAccumulator += frameDeltaTime;
        int stepsThisFrame = 0;
        const bool measuring = m_measurements.measureFire ||
                               m_measurements.measureFluid;
        while (m_physicsAccumulator >= SimulationTiming::kFixedTimestep &&
               stepsThisFrame < SimulationTiming::kMaxPhysicsStepsPerFrame) {
            window.Input().BeginFixedStep();if(auto* players=world.PlayersIfLoaded())players->BeginFixedStep();
            const auto stepStart = Clock::now();
            m_measurements.fireMeasured = m_measurements.fluidMeasured = false;
            StepPlayedWorld(world, window, SimulationTiming::kFixedTimestep, &m_measurements);
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
        window.ClearPendingRequests();if(auto* players=world.PlayersIfLoaded())players->DiscardPending();
        m_lastStepsThisFrame = 0;
    }

    // How far real time has progressed into an as-yet-unsimulated fixed
    // step — the presentation interpolation factor (M6).
    PerformanceProfiler::Get().FixedState(m_physicsAccumulator,profileCap,profileDiscarded,IsPaused()||uiOwned);
    const float presentationAlpha = m_physicsAccumulator / SimulationTiming::kFixedTimestep;
    window.Input().SuspendRumble(IsPaused());
    world.PresentationScripts(&window.Input(),frameDeltaTime,presentationAlpha);
    const int height = std::max(window.InputHeight(), 1);
    glm::mat4 view(1);
    if(world.view){const auto& v=*world.view;view=glm::lookAt(v.pose.position,v.pose.position+v.pose.rotation*glm::vec3(0,0,-1),v.pose.rotation*glm::vec3(0,1,0));}
    world.viewportWidth=window.InputWidth();world.viewportHeight=height;
    if(world.Scripts())world.Scripts()->SetView(view);
    world.UpdateAudio(view,presentationAlpha,frameDeltaTime);
    // Audio publication is a runtime service, independent of drawing/capture.
    if (!render && (!auxiliary || world.PresentationCameras().empty())) {
        m_lastSceneMilliseconds = m_lastSurfaceMilliseconds = 0;
        return presentationAlpha;
    }
    const auto surfaceStart = Clock::now();
    m_lastSurfaceMilliseconds = MillisecondsSince(surfaceStart);
    const auto sceneStart = Clock::now();
    RenderWorldFrame(renderer, window.InputWidth(), window.InputHeight(), world, view,
                     BuildCameraFrame(glm::vec3(0),glm::quat(1,0,0,0),60,.1f,1000,CameraLens{},window.InputWidth(),height).projection, glm::vec3(glm::inverse(view)[3]),
                     presentationAlpha,frameDeltaTime,
                     render ? WorldRenderPurpose::Runtime : WorldRenderPurpose::Auxiliary,
                     render ? destination : nullptr);
    m_lastSceneMilliseconds = MillisecondsSince(sceneStart);
    if (!render) return presentationAlpha;
    // Auxiliary cameras have finished. Resume the display image without clearing
    // or starting another HDR pass; overlays remain identical to desktop play.
    if(destination&&!renderer.BeginRenderTarget(*destination, /*preserveContents=*/true))return presentationAlpha;
    if (m_overlay) m_overlay(renderer, presentationAlpha);

    // Milestone 13: HUD + pause menu overlay, drawn last, on top.
    renderer.BeginUIFrame(window.InputWidth(), window.InputHeight());
    if(authoredUI)world.UI().Draw(renderer,window.InputWidth(),window.InputHeight(),&world);
    if(m_scriptFaultNotice)DrawScriptFaultNotice(renderer,window.InputWidth(),window.InputHeight(),world.Scripts(),world.Resources()?world.Resources()->Assets():nullptr);
    renderer.EndUIFrame();
    if(destination)renderer.EndRenderTarget();
    return presentationAlpha;
}

bool InteractivePlay::IsPaused()const{return m_world && m_world->UIIfLoaded() && m_world->UIIfLoaded()->Paused();}
bool InteractivePlay::QuitRequested()const{return m_world && m_world->UIIfLoaded() && m_world->UIIfLoaded()->QuitRequested();}
