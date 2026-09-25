#pragma once

#include "DebugDraw.h"

class GameSession;
class RuntimeWorld;
class Scene;

// Milestone 30: the debug visualisation layer. Turns engine truth (a
// RuntimeWorld and, optionally, the GameSession playing it; or an authored
// Scene in edit mode) into a DebugLineList that Renderer::DrawDebugLines
// draws. Every category is off by default, rebuilt from scratch each frame
// it is on, and reads state only — nothing here writes simulation or
// authored data, and nothing here issues a GL call.
//
// What is shown, honestly:
//   collisionShapes  the physics shapes actually created (static bodies,
//                    live dynamic bodies; compound children; terrain as
//                    its bounding sphere — the surface itself is drawn by
//                    the ordinary mesh)
//   playerCapsule    the sweep capsule PlayerController is moved as, and
//                    its support state (green ring grounded, red airborne)
//   contacts         last fixed step's resolved contact points and normals
//                    (PhysicsWorld::LastStepContacts), plus the player's
//                    support body
//   gravity          the gravity vector sampled at each live body and the
//                    player, and the authored gravity regions' volumes
//   frameAxes        local axes at every body and the world origin
//   lights           this frame's dynamic lights: point ranges as circles,
//                    spot cones as lines
//   interactionRanges  each interactable's interaction sphere, the current
//                    target highlighted, the held object tethered
//   lifecycle        a marker at every persistent entity coloured by
//                    fidelity (Full green, Coarse yellow, Dormant grey,
//                    Destroyed red)
//   terrainNormals   surface normals sampled on a ring around the player
//                    (or the terrain's top) — a sample, not every vertex
//   fluidParticles   a small marker per presented particle, capped at
//                    kMaxFluidParticleMarkers so a lake stays interactive
//   atmosphere       the reference and top radii of each atmosphere
struct DebugViewOptions {
    bool collisionShapes = false;
    bool playerCapsule = false;
    bool contacts = false;
    bool gravity = false;
    bool frameAxes = false;
    bool lights = false;
    bool interactionRanges = false;
    bool lifecycle = false;
    bool terrainNormals = false;
    bool fluidParticles = false;
    bool atmosphere = false;

    bool AnyEnabled() const {
        return collisionShapes || playerCapsule || contacts || gravity || frameAxes || lights ||
               interactionRanges || lifecycle || terrainNormals || fluidParticles || atmosphere;
    }
};

constexpr std::size_t kMaxFluidParticleMarkers = 6000;

// Play mode: `session` may be null (no player/interaction categories then).
void BuildWorldDebugLines(const RuntimeWorld& world, const GameSession* session, float presentationAlpha,
                          const DebugViewOptions& options, DebugLineList& out);

// Edit mode: the authored counterparts (collision shapes, gravity regions,
// lights, player start capsule, frame axes) straight from the Scene.
void BuildAuthoredDebugLines(const Scene& scene, const DebugViewOptions& options, DebugLineList& out);
