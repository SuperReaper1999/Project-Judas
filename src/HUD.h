#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Renderer;

struct HUDThermalBody {
    std::string label;
    float temperatureKelvin = 0.0f;
    double remainingFuelKg = 0.0;
    float burnRateKgPerSecond = 0.0f;
};

// Milestone 13: the persistent gameplay HUD. Plain view data only — see
// docs/ARCHITECTURE.md, "Milestone 13" — HUD code (this file) never
// includes PlayerController.h, GravityContextMap.h, PilotAttachment.h, or
// any other concrete gameplay type; Application.cpp (the composition root)
// reads those each frame and fills in a HUDViewData, exactly the same
// "gameplay hands Judas's rendering layer plain data, never a live
// reference to itself" shape DrawBox/DrawMesh's position/rotation
// parameters already use for the 3D world.
struct HUDViewData {
    bool grounded = false;
    float gravityMagnitude = 0.0f;
    bool controllingSpacecraft = false;
    bool pilotAttached = false;
    bool spacecraftSasEnabled = false;
    glm::dvec3 worldOrigin{0.0};  // M23 absolute placement; local physics stays unchanged
    glm::dvec3 absolutePlayerPosition{0.0};
    float spacecraftLinearSpeed = 0.0f;  // world-frame speed (m/s)
    bool celestialReferenceAvailable = false;
    std::string celestialReferenceLabel = "Body A";
    float celestialBodyWorldSpeed = 0.0f;
    float spacecraftRelativeCelestialSpeed = 0.0f;
    float pilotWorldSpeed = 0.0f;
    float pilotRelativeSpacecraftSpeed = 0.0f;
    bool atmosphereAvailable = false;
    float atmosphereDensity = 0.0f;
    float atmospherePressure = 0.0f;
    float spacecraftRelativeAirspeed = 0.0f;
    float spacecraftDynamicPressure = 0.0f;
    float spacecraftAerodynamicForce = 0.0f;
    bool thermalAvailable = false;
    bool igniterPowered = false;
    float oxidizerMassDensity = 0.0f;
    std::vector<HUDThermalBody> thermalBodies;

    // Milestone 29: lifecycle/persistence readout. `lifecycleAvailable` is
    // set only when the scene runs a fidelity policy or has persistent
    // changes/state to show, so a simple scene's HUD stays as it was.
    bool lifecycleAvailable = false;
    std::size_t entitiesFull = 0, entitiesCoarse = 0, entitiesDormant = 0, entitiesDestroyed = 0;
    std::size_t physicsBodies = 0;
    std::string worldStateInfo;   // e.g. "saves/x.judasstate (loaded)"
    std::string lifecycleMessage; // last spawn/destroy/save/delete result

    // Milestone 16: the currently-selected interactable's own prompt text
    // (see src/Interactable.h's GetPromptText), or empty when nothing is
    // currently selectable — HUD draws this verbatim, never interpreting
    // it or knowing what a "door" or "switch" is (see
    // docs/ARCHITECTURE.md, "Milestone 16, Architecture").
    std::string interactPrompt;
};

class HUD {
public:
    // Draws a small telemetry panel anchored to the top-left corner of the
    // window (see docs/ARCHITECTURE.md for why a single fixed anchor was
    // enough for M13's one HUD panel, rather than a general anchor-enum
    // layout system). No-op-safe to call every frame regardless of window
    // size — like UIMenuScreen::Layout, position is recomputed from
    // `windowWidth`/`windowHeight` each call.
    void Draw(Renderer& renderer, int windowWidth, int windowHeight, const HUDViewData& data) const;
};
