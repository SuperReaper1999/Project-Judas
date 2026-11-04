#pragma once

#include <cstdint>

class EditorDocument;
class Project;
class EngineHost;
class AssetDatabase;
class RuntimeWorld;
struct EditorPanelState;
struct EditorRequests;

// The world belongs to the session, never a panel. At a debugger stop only
// read-only observation and debugger commands are permitted: no world pumping.
enum class EditorPanelPhase : std::uint8_t { Edit = 1, Play = 2, Paused = 4, DebugStopped = 8 };
constexpr unsigned EditorPhaseMask(EditorPanelPhase phase) { return static_cast<unsigned>(phase); }
constexpr unsigned kEditorAllPanelPhases = 15;

// A frame-scoped borrow, not a second editor model. Panels retain their own UI
// drafts and only keep stable entity/asset IDs across frames, never this context.
struct EditorContext {
    EditorDocument &document;
    Project &project;
    EditorPanelState &panels;
    EditorRequests &requests;
    EngineHost &host;
    AssetDatabase &assets;
    RuntimeWorld *runtime = nullptr;
    EditorPanelPhase phase = EditorPanelPhase::Edit;
};
