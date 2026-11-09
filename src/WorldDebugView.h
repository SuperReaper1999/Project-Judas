#pragma once

#include "DebugDraw.h"

class RuntimeWorld;
class Scene;

// Read-only overlays for current runtime/authored geometry. No gameplay state.
struct DebugViewOptions {
    bool collisionShapes = false;
    bool playerCapsule = false;
    bool contacts = false;
    bool gravity = false;
    bool frameAxes = false;
    bool lights = false;
    bool lifecycle = false;
    bool terrainNormals = false;
    bool waterBounds = false;
    bool atmosphere = false;
    bool broadphase = false;
    bool navigation = false;

    bool AnyEnabled() const {
        return collisionShapes || playerCapsule || contacts || gravity || frameAxes || lights ||
               lifecycle || terrainNormals || waterBounds || atmosphere || broadphase || navigation;
    }
};


// Runtime overlays.
void BuildWorldDebugLines(const RuntimeWorld& world, float presentationAlpha,
                          const DebugViewOptions& options, DebugLineList& out);

// Edit mode: the authored counterparts (collision shapes, gravity regions,
// lights, character capsules, frame axes) straight from the Scene.
void BuildAuthoredDebugLines(const Scene& scene, const DebugViewOptions& options, DebugLineList& out);
