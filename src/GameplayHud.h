#pragma once

#include "HUD.h"

class GameSession;
class WorldCoordinates;
struct AerodynamicDragResult;

// Milestone 28: fills the M13 HUD's plain view data from a played scene.
// The only place that reads GameSession/RuntimeWorld state for display;
// HUD itself still knows no gameplay type. Labels come from scene object
// names, never from demo-specific strings.
HUDViewData BuildHudView(const GameSession& session, const WorldCoordinates& worldCoordinates,
                         const AerodynamicDragResult& lastAerodynamicDrag);
