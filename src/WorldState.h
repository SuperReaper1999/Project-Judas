#pragma once

#include <string>
#include <vector>

#include "EntityLifecycle.h"

class RuntimeWorld;

// Milestone 29: persistent world state as DELTAS over an unchanged
// baseline scene.
//
//   baseline scene (assets/..., authored, never rewritten by gameplay)
//     + world-state deltas (this file)
//     = the current world
//
// The delta vocabulary is deliberately small and explicit:
//
//   moved      — a persistent entity's physical state (pose + velocities)
//                differs from its baseline definition
//   destroyed  — an entity was permanently destroyed
//   created    — a persistent entity was created at runtime: its full
//                definition (a scene object block) plus physical state
//   door       — a door's open/closed state
//   switch     — a light switch's lamp state
//
// Nothing transient is stored: no physics internals, contacts, solver
// state, handles, GPU resources, presentation history, fidelity or editor
// pointers. Fidelity is re-decided by policy on load; a delta describes
// what the world IS, not how it is currently being simulated.
//
// The file format (`.judasstate`, version 1) follows the scene format's
// conventions (line-oriented, quoted strings, shortest exact floats,
// deterministic order by entity id) and its strictness: any malformed
// line, unknown id, colliding created id or invalid created definition
// fails validation BEFORE anything is applied, so a bad file never leaves
// the live world half-modified.
struct WorldStateEntityChange {
    EntityId id = kInvalidSceneObjectId;
    bool destroyed = false;
    bool created = false;
    SceneObject definition;  // created only
    EntityPhysicalState state;  // moved / created
};

struct WorldStateInteractableChange {
    SceneObjectId id = kInvalidSceneObjectId;
    bool isDoor = true;  // else light switch
    bool on = false;     // open / lamp on
};

struct WorldState {
    std::string baselineName;
    EntityId nextRuntimeId = kRuntimeEntityIdBase;
    std::vector<WorldStateEntityChange> entities;
    std::vector<WorldStateInteractableChange> interactables;
    bool Empty() const { return entities.empty() && interactables.empty(); }
};

constexpr int kWorldStateFormatVersion = 1;

// Captures every difference between the running world and its baseline.
WorldState CaptureWorldState(const RuntimeWorld& world);

// Validates the whole delta against `world` (ids exist, created ids are
// runtime-range and unused, definitions instantiable), then applies it.
// On a validation failure nothing is changed and `outError` says why.
bool ApplyWorldState(RuntimeWorld& world, const WorldState& state, std::string& outError);

bool SaveWorldStateToString(const WorldState& state, std::string& outText);
bool SaveWorldStateToFile(const WorldState& state, const std::string& path, std::string& outError);
bool LoadWorldStateFromString(const std::string& text, WorldState& outState, std::string& outError);
bool LoadWorldStateFromFile(const std::string& path, WorldState& outState, std::string& outError);

// The conventional delta location for a scene file:
// saves/<scene file stem>.judasstate, or "" for an unsaved scene.
std::string DefaultWorldStatePath(const std::string& scenePath);

// Convenience for launch: if `path` exists, load and apply it. Returns
// false only on a real error; a missing file is "no saved state".
bool ApplyWorldStateFileIfPresent(RuntimeWorld& world, const std::string& path, bool& outApplied,
                                  std::string& outError);
