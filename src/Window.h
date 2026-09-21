#pragma once

#include <SDL2/SDL.h>

// Input actions the engine exposes to game logic. This is deliberately an
// action enum rather than raw key codes, so a later input source (mouse,
// controller) can drive the same actions without callers changing.
//
// These are free-flight camera actions (Milestone 2). "Forward"/"strafe"
// are relative to wherever the camera is currently looking, not to any
// fixed world direction.
enum class Action {
    MoveForward,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    Ascend,
    Descend,
};

// Owns the OS window, the GL context, and OS event pumping. Combines the
// "Window" and "Input" responsibilities from the design brief into one
// class since input is just "keyboard/mouse state of this window".
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Init(const char* title, int width, int height);
    void Shutdown();

    void PollEvents();
    bool ShouldClose() const { return m_shouldClose; }
    void SwapBuffers();

    bool IsActionActive(Action action) const;

    // Mouse motion accumulated since the last call, in pixels. Returns
    // (0, 0) while the mouse is not captured (see PollEvents' Escape
    // handling), so releasing the mouse also stops camera look. Always
    // drains SDL's internal relative-motion accumulator regardless of
    // capture state, so re-capturing doesn't produce a stale jump.
    void GetMouseDelta(int& deltaX, int& deltaY) const;

    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_sdlInitialized = false;
    bool m_shouldClose = false;
    bool m_mouseCaptured = false;
    int m_width = 0;
    int m_height = 0;
};
