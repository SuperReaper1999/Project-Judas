#pragma once
#include "WorldDebugView.h"
#include "imgui.h"

// The menu bar and Scene overlay edit the same existing visualization flags.
inline void DrawEditorOverlayMenu(DebugViewOptions &d) {
    ImGui::MenuItem("Collision shapes", nullptr, &d.collisionShapes);
    ImGui::MenuItem("Player capsule + support", nullptr, &d.playerCapsule);
    ImGui::MenuItem("Contacts", nullptr, &d.contacts);
    ImGui::MenuItem("Gravity vectors + regions", nullptr, &d.gravity);
    ImGui::MenuItem("Frame axes", nullptr, &d.frameAxes);
    ImGui::MenuItem("Lights", nullptr, &d.lights);
    ImGui::MenuItem("Interaction ranges", nullptr, &d.interactionRanges);
    ImGui::MenuItem("Lifecycle / fidelity", nullptr, &d.lifecycle);
    ImGui::MenuItem("Terrain normals (sampled)", nullptr, &d.terrainNormals);
    ImGui::MenuItem("Fluid particles", nullptr, &d.fluidParticles);
    ImGui::MenuItem("Atmosphere radii", nullptr, &d.atmosphere);
    ImGui::MenuItem("Broadphase bounds", nullptr, &d.broadphase);
    ImGui::MenuItem("Navigation mesh / routes", nullptr, &d.navigation);
    ImGui::Separator();
    if (ImGui::MenuItem("All off"))
        d = {};
}
