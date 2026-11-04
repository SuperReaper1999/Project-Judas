#pragma once
#include "EditorAutomationCoordinates.h"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>
class EditorApplication;
class EngineHost;
class Window;
class Renderer;
struct EditorRequests;

// Opt-in qualification only. All actions enter normal editor requests;
// this fixture never advances simulation or publishes assets itself.
class EditorAutomation {
  public:
    EditorAutomation(EditorApplication &, EngineHost &);
    bool Initialize();
    void BeforeRequests(EditorRequests &);
    void AfterRequests();
    void AfterRender(EditorRequests &deferredRequests);
    bool Active() const {
        return autotest != nullptr;
    }
    bool Failed() const {
        return terrainFailures || navigationFailures;
    }

  private:
    void RunTerrainAndAsyncChecks(EditorRequests &);
    void RunAssetsChecks();
    void RunWorkspaceChecks();
    void RunSimulationChecks(EditorRequests &);
    EditorAutomationNativePoint NativePoint(glm::vec2, const char *panelName = "Scene###Judas.Scene") const;
    SDL_Window *SceneWindow() const;
    void WarpPoint(glm::vec2, const char *panelName = "Scene###Judas.Scene");
    void navigationCheck(bool, const char *);
    void navigationButton(bool);
    void navigationMotion(int, int);
    void navigationFocus(bool);
    void volumeCheck(bool, const char *);
    void terrainCheck(bool, const char *);
    void screenshot(const std::string &);
    EditorApplication &app;
    EngineHost &host;
    Window &window;
    Renderer &renderer;
    int initialWindowWidth, initialWindowHeight;
    const char *autotest;
    int autotestFrame = 0;
    bool navigationAutotest, projectUnsavedAutotest, polishAutotest, pickingAutotest, workspaceAutotest;
    glm::vec3 dollyStartPosition{0}, dollyAnchor{0}, navigationStartPosition{0}, navigationStartFocus{0},
        navigationStartForward{0}, navigationExpectedPosition{0};
    float dollyStartDistance = 0;
    int navigationFailures = 0;
    std::uint64_t profilerFrozenFrame = 0;
    std::size_t profilerFrozenSteps = 0;
    std::string autotestBaseline;
    bool animationTimelineRecook = false, animationTimelineAutotest;
    const char *importRecipe, *terrainAutotest, *volumeAutotest, *retargetAutotest;
    int volumeUiFrame = 0, volumeWaitFrames = 0, volumeFailures = 0;
    bool volumeUiReady = false;
    std::vector<float> volumeBefore, volumeEdited, terrainInitial, terrainEdited;
    std::vector<std::array<float, 4>> terrainPainted;
    int terrainFailures = 0, terrainUiFrame = 0;
    bool terrainUiReady = false, automatedExport = false;
    const char *exportDestination = nullptr;
    int exportRequestFrame = 14;
    unsigned retargetWaitFrames = 0, importWaitFrames = 0;
};
