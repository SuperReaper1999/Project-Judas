#pragma once

#include <functional>
#include <string>

#include "AerodynamicDrag.h"
#include "GameSession.h"
#include "HUD.h"
#include "PauseMenu.h"
#include "Simulation.h"
#include "WorldCoordinates.h"

class Renderer;
class RuntimeWorld;
class Window;

// Milestone 28: one interactive frame of a played scene — the loop body
// the `judas` runtime has run since Milestone 13, factored out so the
// editor's Play mode runs the identical thing: the single input-routing
// boundary (pause menu first, gameplay only when no menu owns input), the
// fixed-step accumulator with its catch-up cap, presentation
// interpolation, the M15 shadow/colour frame from the player's camera,
// and the HUD/menu overlay. Owns the PauseMenu, HUD and accumulator; the
// GameSession it drives is begun over a RuntimeWorld the caller built.
//
// The runtime's opt-in measurement printers hook in through
// `SetFixedStepObserver`; nothing in this class prints.
class InteractivePlay {
public:
    using FixedStepObserver = std::function<void(const FixedStepMeasurements&, double fixedStepMilliseconds)>;
    // Milestone 30: drawn inside the 3D frame after the world and before the
    // HUD — the editor's debug view. Receives the presentation alpha.
    using WorldOverlay = std::function<void(Renderer&, float presentationAlpha)>;

    bool Begin(RuntimeWorld& world, const WorldCoordinates& worldCoordinates, std::string& outError);
    // Milestone 29: where F6 writes / F7 deletes the world-state delta for
    // this run, and whether one was applied at load. Empty disables both.
    void SetWorldStatePath(const std::string& path, bool loadedFromFile);
    const std::string& WorldStatePath() const { return m_worldStatePath; }
    bool SaveWorldStateNow(std::string& outMessage);
    bool DeleteWorldStateNow(std::string& outMessage);
    void End();
    bool IsActive() const { return m_session.IsActive(); }

    void SetFixedStepMeasurementFlags(bool atmosphere, bool fire, bool fluid);
    void SetFixedStepObserver(FixedStepObserver observer) { m_observer = std::move(observer); }
    void SetWorldOverlay(WorldOverlay overlay) { m_overlay = std::move(overlay); }
    // Milestone 30 profiler inputs: how many fixed steps the last Frame ran,
    // the wall time of the last StepPlayedWorld call (always measured), and
    // the last step's opt-in subsystem measurements.
    int LastFixedStepsThisFrame() const { return m_lastStepsThisFrame; }
    double LastFixedStepMilliseconds() const { return m_lastStepMilliseconds; }
    const FixedStepMeasurements& LastMeasurements() const { return m_measurements; }

    // Routes this frame's already-pumped input, advances the simulation
    // by however many fixed steps `frameDeltaTime` earns (none while the
    // pause menu is open), and renders the presented world plus the HUD
    // and menu into the window. `drawHud` false skips the overlay (the
    // editor draws its own). Returns the presentation alpha used.
    float Frame(Window& window, Renderer& renderer, float frameDeltaTime, bool drawHud = true);

    GameSession& Session() { return m_session; }
    const GameSession& Session() const { return m_session; }
    PauseMenu& Menu() { return m_pauseMenu; }
    bool QuitRequested() const { return m_pauseMenu.QuitRequested(); }
    bool IsPaused() const { return m_pauseMenu.IsOpen(); }
    // Fixed steps since Begin or the last reset (M25's screenshot timer).
    std::size_t FixedStepsSinceReset() const { return m_fixedStepsSinceReset; }
    bool ConsumeResetOccurred();
    const AerodynamicDragResult& LastAerodynamicDrag() const { return m_lastAerodynamicDrag; }
    // Presentation-side timings of the last Frame (fluid surface rebuild,
    // scene submission), for the runtime's diagnostics.
    double LastSurfaceMilliseconds() const { return m_lastSurfaceMilliseconds; }
    double LastSceneMilliseconds() const { return m_lastSceneMilliseconds; }

private:
    GameSession m_session;
    PauseMenu m_pauseMenu;
    HUD m_hud;
    WorldCoordinates m_worldCoordinates;
    FixedStepMeasurements m_measurements;
    FixedStepObserver m_observer;
    WorldOverlay m_overlay;
    int m_lastStepsThisFrame = 0;
    double m_lastStepMilliseconds = 0.0;
    AerodynamicDragResult m_lastAerodynamicDrag;
    float m_physicsAccumulator = 0.0f;
    std::size_t m_fixedStepsSinceReset = 0;
    bool m_resetOccurred = false;
    bool m_wasPauseMenuOpen = false;
    double m_lastSurfaceMilliseconds = 0.0;
    double m_lastSceneMilliseconds = 0.0;
    std::string m_worldStatePath;
    std::string m_worldStateStatus;
};
