#pragma once

#include <SDL2/SDL.h>

// Input actions the engine exposes to game logic. This is deliberately an
// action enum rather than raw key codes, so a later input source (mouse,
// controller) can drive the same actions without callers changing.
//
// As of Milestone 4 these drive the player's grounded horizontal
// locomotion (see PlayerController): "forward"/"strafe" are relative to
// wherever the player is currently looking, not to any fixed world
// direction. Vertical movement is not one of these — jumping is a discrete
// event (see ConsumeJumpRequest below), not a held direction.
enum class Action {
    MoveForward,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
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

    // Returns true exactly once per `R` key press (edge-triggered, not
    // polled state), then clears itself — a minimal debug control, not a
    // general input-remapping system. Used by Application to reset the
    // demo's dynamic cube and player.
    bool ConsumeResetRequest();

    // Returns true exactly once per `Space` key press (edge-triggered).
    // Deliberately separate from the continuously-polled Action enum:
    // jumping is a one-shot event, not a held direction. PlayerController
    // latches this into its own longer-lived flag so a press during a
    // render frame with zero fixed physics steps isn't lost — see
    // docs/ARCHITECTURE.md, "Simulation timing."
    bool ConsumeJumpRequest();

    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_sdlInitialized = false;
    bool m_shouldClose = false;
    bool m_mouseCaptured = false;
    bool m_resetRequested = false;
    bool m_jumpRequested = false;
    int m_width = 0;
    int m_height = 0;
};
