#pragma once

#include <SDL2/SDL.h>

// Input actions the engine exposes to game logic. This is deliberately an
// action enum rather than raw key codes, so a later input source (mouse,
// controller) can drive the same actions without callers changing.
//
// As of Milestone 4 these drive the player's grounded horizontal
// locomotion (see PlayerController): "forward"/"strafe" are relative to
// wherever the player is currently looking, not to any fixed world
// direction. Vertical movement was not one of these through Milestone
// 7-Final — jumping was a discrete event (see ConsumeJumpRequest below),
// not a held direction.
//
// Milestone 8 adds MoveUp/MoveDown (Q/E), continuously polled the same way
// as the rest — needed for the flying primitive's vertical control (see
// src/FlyingPrimitiveControl.h), which has no "jump" concept. The player
// itself still never consults these two.
enum class Action {
    MoveForward,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    MoveUp,
    MoveDown,
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

    // `visible = false` creates the window hidden (still a real, usable GL
    // context — just not shown on screen) for the headless test harness
    // (see src/TestHarness.h). The normal game loop always uses the default.
    bool Init(const char* title, int width, int height, bool visible = true);
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

    // Milestone 8: returns true exactly once per `F` key press
    // (edge-triggered, same shape as ConsumeResetRequest/ConsumeJumpRequest).
    // Application interprets this as "toggle control of the flying
    // primitive" — this class knows nothing about what F does, only that a
    // press happened.
    bool ConsumeControlToggleRequest();

    int Width() const { return m_width; }
    int Height() const { return m_height; }

    // --- Test/automation input override ---
    //
    // Developer tooling only (see src/TestHarness.h) — not used by the
    // normal interactive game loop. When enabled, IsActionActive,
    // GetMouseDelta, ConsumeJumpRequest, and ConsumeResetRequest all return
    // scripted values below instead of querying real SDL keyboard/mouse
    // state, so an automated test can drive the engine deterministically
    // without a human at the keyboard.
    void SetTestInputMode(bool enabled);
    void SetTestActionState(Action action, bool active);
    void QueueTestMouseDelta(int deltaX, int deltaY);
    void RequestTestJump();
    void RequestTestReset();

    // Milestone 8: scripted equivalent of an `F` press.
    void RequestTestControlToggle();

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_sdlInitialized = false;
    bool m_shouldClose = false;
    bool m_mouseCaptured = false;
    bool m_resetRequested = false;
    bool m_jumpRequested = false;
    bool m_controlToggleRequested = false;
    int m_width = 0;
    int m_height = 0;

    bool m_testInputMode = false;
    bool m_testActionState[6] = {false, false, false, false, false, false};
    // mutable: GetMouseDelta is const (it only ever mutates external SDL
    // state in the non-test path), but test mode needs "read once, then
    // drain to zero" semantics on its own queued delta, matching real
    // mouse behavior.
    mutable int m_testMouseDeltaX = 0;
    mutable int m_testMouseDeltaY = 0;
    bool m_testJumpRequested = false;
    bool m_testResetRequested = false;
    bool m_testControlToggleRequested = false;
};
