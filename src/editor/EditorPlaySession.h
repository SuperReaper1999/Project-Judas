#pragma once
#include "DebugDraw.h"
#include <memory>
#include <string>
struct EditorContext;
struct EditorSceneViewport;
struct ScriptTools;
struct RenderTargetHandle;
class RuntimeWorld;
class InteractivePlay;

// Exactly one authoritative played world. Panel visibility never owns its lifetime.
struct EditorPlaySession {
    EditorPlaySession();
    ~EditorPlaySession();
    bool StartPlay(EditorContext &, EditorSceneViewport &, std::unique_ptr<ScriptTools> &, std::string &);
    void StopPlay(EditorContext &, EditorSceneViewport &);
    bool Frame(EditorContext &, std::unique_ptr<ScriptTools> &, float deltaSeconds, bool render = true,
               bool allowCapture = true, const RenderTargetHandle *destination = nullptr);
    std::unique_ptr<RuntimeWorld> world;
    std::unique_ptr<InteractivePlay> play;
    DebugLineList debugLines;
    float lastAlpha = 0;
};
