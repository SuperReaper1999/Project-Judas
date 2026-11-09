#pragma once

#include <functional>
#include <optional>
#include <string>

#include "AerodynamicDrag.h"
#include "Simulation.h"
#include "WorldCoordinates.h"

class Renderer;
class RuntimeWorld;
class Window;
struct RenderTargetHandle;

// Shared runtime/editor Play lifecycle. Borrows the caller-owned world, routes
// authored UI before gameplay input, advances the bounded fixed clock and
// presents authored views. Gameplay, menus and controls belong to scripts.
class InteractivePlay {
public:
    using FixedStepObserver = std::function<void(const FixedStepMeasurements&, double fixedStepMilliseconds)>;
    // Milestone 30: drawn inside the 3D frame after the world and before the
    // HUD — the editor's debug view. Receives the presentation alpha.
    using WorldOverlay = std::function<void(Renderer&, float presentationAlpha)>;

    bool Begin(RuntimeWorld& world, const WorldCoordinates& worldCoordinates, std::string& outError);
    void End();
    bool IsActive() const { return m_world != nullptr; }
    RuntimeWorld& World() { return *m_world; }
    const RuntimeWorld& World() const { return *m_world; }

    void SetFixedStepMeasurementFlags(bool fire, bool fluid);
    // Presentation policy persists through scene/save replacement. No callback recovery.
    void SetScriptFaultNoticeEnabled(bool enabled) {
        m_scriptFaultNoticeOverride = enabled;
        m_scriptFaultNotice = enabled;
    }
    // Diagnostic/editor cursor routing only; does not change project intent or pause simulation.
    void SetPointerCaptureAllowed(bool allowed) { m_pointerCaptureAllowed = allowed; }
    void SetFixedStepObserver(FixedStepObserver observer) { m_observer = std::move(observer); }
    void SetWorldOverlay(WorldOverlay overlay) { m_overlay = std::move(overlay); }
    // Milestone 30 profiler inputs: how many fixed steps the last Frame ran,
    // the wall time of the last StepPlayedWorld call (always measured), and
    // the last step's opt-in subsystem measurements.
    int LastFixedStepsThisFrame() const { return m_lastStepsThisFrame; }
    double LastFixedStepMilliseconds() const { return m_lastStepMilliseconds; }
    const FixedStepMeasurements& LastMeasurements() const { return m_measurements; }

    // Route input, run fixed steps unless authored UI pauses/owns input, then
    // present the world and project UI. A hidden display can retain authored
    // camera updates; auxiliary=false delegates those to the sole Scene view.
    float Frame(Window& window, Renderer& renderer, float frameDeltaTime, bool render = true,
                const RenderTargetHandle* destination = nullptr, bool auxiliary = false);

    bool QuitRequested() const;
    bool IsPaused() const;
    // Fixed steps completed by this Play instance.
    std::size_t FixedStepsSinceReset() const { return m_fixedStepsSinceReset; }
    // Presentation-side timings of the last Frame (fluid surface rebuild,
    // scene submission), for the runtime's diagnostics.
    double LastSurfaceMilliseconds() const { return m_lastSurfaceMilliseconds; }
    double LastSceneMilliseconds() const { return m_lastSceneMilliseconds; }

private:
    bool m_scriptFaultNotice = false;
    std::optional<bool> m_scriptFaultNoticeOverride;
    RuntimeWorld* m_world = nullptr;
    WorldCoordinates m_worldCoordinates;
    FixedStepMeasurements m_measurements;
    FixedStepObserver m_observer;
    WorldOverlay m_overlay;
    int m_lastStepsThisFrame = 0;
    double m_lastStepMilliseconds = 0.0;
    float m_physicsAccumulator = 0.0f;
    std::size_t m_fixedStepsSinceReset = 0;
    bool m_wasPauseMenuOpen = false;
    bool m_captureInitialized = false;
    bool m_pointerCaptureAllowed = true;
    double m_lastSurfaceMilliseconds = 0.0;
    double m_lastSceneMilliseconds = 0.0;
};
