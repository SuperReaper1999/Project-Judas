#pragma once

#include <SDL2/SDL.h>

#include <functional>

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
//
// Milestone 11 adds Pitch/Yaw/Roll (six more, one per rotation direction),
// used only by the spacecraft's attitude control (see
// src/FlyingPrimitiveControl.h) while piloting — keyboard-only and
// deliberately separate from mouse look, so mouse deltas keep meaning
// "free camera look" the entire time and never also drive the spacecraft's
// own orientation (see docs/ARCHITECTURE.md, "Milestone 11," for why
// double-applying the same input to both would be a real bug, not a
// style choice). The player itself never consults these six either.
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
    // M25 authored water source: held B adds real particles on fixed steps
    // only while gameplay owns input. No fluid system sees a keyboard key.
    AddTerrainWater,
    // M27: C powers a radiant heater at the player's current local-frame
    // look pose. Application alone maps the held action to thermal energy.
    UseIgniter,
    Count,
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

    // Milestone 14: returns true exactly once per `T` key press
    // (edge-triggered, same shape as ConsumeControlToggleRequest). `T` was
    // otherwise unused by every prior milestone's key bindings — see
    // docs/ARCHITECTURE.md, "Milestone 14, Player torch," for the full
    // list this was checked against. Application interprets this as
    // "toggle the player's torch" — this class knows nothing about what a
    // torch is, only that a press happened, same as every other Consume*
    // request here.
    bool ConsumeTorchToggleRequest();

    // Milestone 16: returns true exactly once per `G` key press
    // (edge-triggered, same shape as ConsumeTorchToggleRequest). NOT the
    // more conventional `E` — `E` is already `Action::MoveUp` (the
    // spacecraft's own "ascend" control, see IsActionActive below), a
    // real conflict caught only after first trying `E` here — see
    // docs/ARCHITECTURE.md, "Milestone 16, Interaction," for the full
    // story. Like every Consume* request here, this class knows nothing
    // about doors, switches, or what "interact" even means — Application
    // interprets the press.
    bool ConsumeInteractRequest();

    // Milestone 17: edge-triggered request from V. Window reports the key;
    // Application applies it only while gameplay owns input.
    bool ConsumeViewToggleRequest();

    // M18: H throws a currently held object; drained every render frame so
    // a menu-owned key press cannot fire after resume.
    bool ConsumeThrowRequest();

    // M21: X toggles active spacecraft attitude stabilization. Drained
    // every render frame so a menu-owned press cannot fire on resume.
    bool ConsumeSasToggleRequest();

    // --- Milestone 13: UI input ---
    //
    // Five more edge-triggered one-shot requests, same shape as
    // ConsumeResetRequest/ConsumeJumpRequest/ConsumeControlToggleRequest
    // above — this class still knows nothing about menus, screens, or
    // navigation, only that a key was pressed. Escape now drives
    // ConsumeUIBackRequest instead of the old "toggle mouse capture"
    // behavior it had through Milestone 12 — see docs/ARCHITECTURE.md,
    // "Milestone 13, Input ownership," for why that standalone debug
    // toggle is superseded now that PauseMenu (src/PauseMenu.h) drives
    // capture explicitly via SetMouseCaptured whenever it opens/closes.
    bool ConsumeUIBackRequest();
    bool ConsumeUINavigateUpRequest();
    bool ConsumeUINavigateDownRequest();
    bool ConsumeUIActivateRequest();

    // Returns true exactly once per left-mouse-button press, and writes
    // the cursor position (window-client pixels, top-left origin) at the
    // moment of that press into `outX`/`outY`. Always drains the pending
    // click, mirroring ConsumeJumpRequest's own "read once, then clear"
    // shape.
    bool ConsumeUIClickRequest(int& outX, int& outY);

    // The cursor's current position (window-client pixels, top-left
    // origin), regardless of capture state — used for continuous
    // hover/focus tracking while a menu is open (see PauseMenu::
    // HandleMouseMove), unlike GetMouseDelta which reads zero while
    // uncaptured.
    void GetMousePosition(int& outX, int& outY) const;

    // Explicitly captures (relative mouse mode, for gameplay look) or
    // releases (absolute cursor, for menu interaction) the mouse — called
    // by Application whenever PauseMenu opens/closes (see
    // docs/ARCHITECTURE.md, "Milestone 13, Input ownership"), not by any
    // key binding of this class's own.
    void SetMouseCaptured(bool captured);

    int Width() const { return m_width; }
    int Height() const { return m_height; }

    // Milestone 28: lets a host application observe every OS event this
    // window pumps (the editor forwards them to its UI layer). Window
    // still owns the pump and its own interpretation of keys; the hook
    // is a plain callback with no knowledge of who listens. `wantsKeyboard`
    // / `wantsMouse` let that listener claim input for a frame: while
    // claimed, IsActionActive/the Consume* requests report nothing so a
    // UI text field never also walks the player.
    void SetEventHook(std::function<void(const SDL_Event&)> hook);
    void SetInputClaimed(bool keyboard, bool mouse);
    // Discards every pending edge-triggered request (reset, jump, control
    // toggle, torch, interact, view, throw, SAS, UI) so presses made while
    // a host was not routing gameplay input cannot fire later.
    void ClearPendingRequests();
    // Whether the OS mouse is currently in relative (captured) mode.
    bool IsMouseCaptured() const { return m_mouseCaptured; }
    // Native handles for a host that must initialise a UI layer against
    // this window's own GL context (the editor). Never used by gameplay.
    SDL_Window* NativeWindow() const { return m_window; }
    SDL_GLContext NativeGLContext() const { return m_glContext; }

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

    // Milestone 14: scripted equivalent of a `T` press.
    void RequestTestTorchToggle();

    // Milestone 16: scripted equivalent of an `E` press.
    void RequestTestInteract();

    // Milestone 21: scripted equivalent of an `X` press.
    void RequestTestSasToggle();

private:
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
    // Milestone 13: UI input edge flags — see PollEvents.
    bool m_uiBackRequested = false;
    bool m_uiUpRequested = false;
    bool m_uiDownRequested = false;
    bool m_uiActivateRequested = false;
    bool m_uiClickRequested = false;
    int m_uiClickX = 0;
    int m_uiClickY = 0;
    int m_width = 0;
    int m_height = 0;

    bool m_testInputMode = false;
    bool m_testActionState[static_cast<int>(Action::Count)] = {};
    // mutable: GetMouseDelta is const (it only ever mutates external SDL
    // state in the non-test path), but test mode needs "read once, then
    // drain to zero" semantics on its own queued delta, matching real
    // mouse behavior.
    mutable int m_testMouseDeltaX = 0;
    mutable int m_testMouseDeltaY = 0;
    bool m_testJumpRequested = false;
    bool m_testResetRequested = false;
    bool m_testControlToggleRequested = false;
    bool m_testTorchToggleRequested = false;
    bool m_testInteractRequested = false;
    bool m_testSasToggleRequested = false;
};
