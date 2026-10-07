#pragma once

#include <SDL2/SDL.h>

#include <functional>
#include <set>
#include "InputSystem.h"

enum class Action {
    MoveForward,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    MoveUp,
    MoveDown,
    PitchUp,
    PitchDown,
    YawLeft,
    YawRight,
    RollLeft,
    RollRight,
    PlanetProgradeThrust,
    PlanetRetrogradeThrust,
    PlanetRadialThrust,
    AddTerrainWater,
    UseIgniter,
    Count,
};

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Init(const char* title, int width, int height, bool visible = true);
    void Shutdown();

    void PollEvents();
    InputSystem& Input() { return m_input; }
    const InputSystem& Input() const { return m_input; }
    float InputAxis(const std::string& name) const;
    bool ConsumeNamedAction(const std::string& name);
    bool ShouldClose() const { return m_shouldClose; }
    void SwapBuffers();

    bool IsActionActive(Action action) const;

    void GetMouseDelta(int& deltaX, int& deltaY) const;
    void GetLookDelta(float& deltaX,float& deltaY) const;

    bool ConsumeResetRequest();

    bool ConsumeJumpRequest();

    bool ConsumeControlToggleRequest();

    bool ConsumeTorchToggleRequest();

    bool ConsumeInteractRequest();

    bool ConsumeViewToggleRequest();

    bool ConsumeThrowRequest();

    bool ConsumeSasToggleRequest();

    bool ConsumeSpawnEntityRequest();
    bool ConsumeDestroyEntityRequest();
    bool ConsumeSaveWorldStateRequest();
    bool ConsumeDeleteWorldStateRequest();

    bool ConsumeUIBackRequest();
    bool ConsumeUINavigateUpRequest();
    bool ConsumeUINavigateDownRequest();
    bool ConsumeUIActivateRequest();

    bool ConsumeUIClickRequest(int& outX, int& outY);

    void GetMousePosition(int& outX, int& outY) const;

    void SetMouseCaptured(bool captured);

    int Width() const { return m_width; }
    int Height() const { return m_height; }

    void SetEventHook(std::function<void(const SDL_Event&)> hook);
    void SetInputClaimed(bool keyboard, bool mouse);
    void ClearPendingRequests();
    bool IsMouseCaptured() const { return m_mouseCaptured; }
    SDL_Window* NativeWindow() const { return m_window; }
    SDL_GLContext NativeGLContext() const { return m_glContext; }

    void SetTestInputMode(bool enabled);
    void BeginTestFrame();
    void QueueTestPhysical(std::string control,float value);
    void SetTestPointer(int x,int y) { m_uiClickX=x;m_uiClickY=y; }
    void SetTestActionState(Action action, bool active);
    void QueueTestMouseDelta(int deltaX, int deltaY);
    void RequestTestJump();
    void RequestTestReset();

    void RequestTestControlToggle();

    void RequestTestTorchToggle();

    void RequestTestInteract();

    void RequestTestSasToggle();

private:
    InputSystem m_input;
    std::set<std::string> m_consumed;
    SDL_GameController* m_controller=nullptr;
    SDL_JoystickID m_controllerId=-1;
    bool m_controllerInputReady=false;
    bool m_inputFocused=true;
    void RefreshController(bool poll=true);
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_sdlInitialized = false;
    bool m_shouldClose = false;
    std::function<void(const SDL_Event&)> m_eventHook;
    bool m_keyboardClaimed = false;
    bool m_mouseClaimed = false;
    bool m_mouseCaptured = false;
    bool m_resetRequested = false;
    bool m_jumpRequested = false;
    bool m_controlToggleRequested = false;
    bool m_torchToggleRequested = false;
    bool m_interactRequested = false;
    bool m_viewToggleRequested = false;
    bool m_throwRequested = false;
    bool m_sasToggleRequested = false;
    bool m_spawnEntityRequested = false;
    bool m_destroyEntityRequested = false;
    bool m_saveWorldStateRequested = false;
    bool m_deleteWorldStateRequested = false;
    bool m_uiBackRequested = false;
    bool m_uiUpRequested = false;
    bool m_uiDownRequested = false;
    bool m_uiActivateRequested = false;
    bool m_uiClickRequested = false;
    int m_uiClickX = 0;
    int m_uiClickY = 0;
    int m_width = 0;
    int m_height = 0;

    std::vector<std::pair<std::string,float>> m_testPhysical;
    bool m_testInputMode = false;
    bool m_testActionState[static_cast<int>(Action::Count)] = {};
    mutable int m_testMouseDeltaX = 0;
    mutable int m_testMouseDeltaY = 0;
    bool m_testJumpRequested = false;
    bool m_testResetRequested = false;
    bool m_testControlToggleRequested = false;
    bool m_testTorchToggleRequested = false;
    bool m_testInteractRequested = false;
    bool m_testSasToggleRequested = false;
};
