#include "../authoring/AuthoringDocument.h"
#include "AnimationTimeline.h"
#include "AuthoringJSON.h"
#include "ComponentEditors.h"
#include "EditorApplication.h"
#include "EditorAutomation.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "ModAuthoring.h"
#include "ModelCook.h"
#include "PerformanceProfiler.h"
#include "Prefab.h"
#include "RetargetPanel.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "ScriptTools.h"
#include "SpriteSheetEditor.h"
#include "TerrainV2Editor.h"
#include "TilemapEditor.h"
#include "UtilityAIEditor.h"
#include "WorldStreaming.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
void EditorAutomation::RunTerrainAndAsyncChecks(EditorRequests &) {
    if (autotestFrame == 30 && std::getenv("JUDAS_EDITOR_AUTOTEST_RUNTIME_INSPECTOR") &&
        app.m_playSession.world) {
        app.m_panels.showRuntimeInspector = true;
        const auto keys = app.m_playSession.world->ScriptKeys();
        if (!keys.empty())
            app.m_panels.runtimeInspectEntity = keys.front().first;
    }
    bool waiting = importRecipe &&
                   (!app.m_panels.modelPreviewToken ||
                    (animationTimelineAutotest && app.m_panels.importTask != app.m_panels.importAccepted));
    if (waiting &&
        ((app.m_panels.importTask->done && !app.m_panels.importTask->success) || ++importWaitFrames > 4000)) {
        std::fprintf(stderr, "[editor autotest] import FAIL: %s\n", app.m_panels.importTask->error.c_str());
        app.m_quit = true;
    }
    if (retargetAutotest) {
        std::string error;
        int status = RetargetAutotestStatus(app.m_panels, error);
        if (status < 0 || (status == 0 && ++retargetWaitFrames > 4000)) {
            std::fprintf(stderr, "[M74 editor retarget] FAIL: %s\n", error.c_str());
            app.m_quit = true;
        }
        waiting = waiting || status == 0;
    }

}
