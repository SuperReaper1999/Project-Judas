#pragma once

#include <string>

class Renderer;

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
    float spacecraftLinearSpeed = 0.0f;  // world-frame speed (m/s)
    bool celestialReferenceAvailable = false;
    float celestialBodyWorldSpeed = 0.0f;
    float spacecraftRelativeCelestialSpeed = 0.0f;
    float pilotWorldSpeed = 0.0f;
    float pilotRelativeSpacecraftSpeed = 0.0f;

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
