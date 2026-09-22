#include "Window.h"

#include <cstdio>

bool Window::Init(const char* title, int width, int height, bool visible) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    m_sdlInitialized = true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    windowFlags |= visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN;

    m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                                 height, windowFlags);
    if (!m_window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    if (visible) {
        // The demo controls a player with mouse look, so start with the
        // mouse captured for immediate look control. A hidden (test
        // harness) window has no real cursor to capture.
        SDL_SetRelativeMouseMode(SDL_TRUE);
        m_mouseCaptured = true;
    }

    m_width = width;
    m_height = height;
    return true;
}

Window::~Window() {
    Shutdown();
}

void Window::Shutdown() {
    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlInitialized) {
        SDL_Quit();
        m_sdlInitialized = false;
    }
}

void Window::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_shouldClose = true;
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                m_shouldClose = true;
            } else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                m_width = event.window.data1;
                m_height = event.window.data2;
            }
        } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                // Toggle mouse capture so the cursor can be released
                // without closing the application. This is the "sensible
                // way to release/restore mouse control" called for by the
                // brief, not a general input-remapping system.
                m_mouseCaptured = !m_mouseCaptured;
                SDL_SetRelativeMouseMode(m_mouseCaptured ? SDL_TRUE : SDL_FALSE);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_R) {
                m_resetRequested = true;
            } else if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                m_jumpRequested = true;
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F) {
                m_controlToggleRequested = true;
            }
        }
    }
}

void Window::SwapBuffers() {
    SDL_GL_SwapWindow(m_window);
}

bool Window::IsActionActive(Action action) const {
    if (m_testInputMode) {
        return m_testActionState[static_cast<int>(action)];
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    switch (action) {
        case Action::MoveForward:
            return keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
        case Action::MoveBackward:
            return keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
        case Action::StrafeLeft:
            return keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
        case Action::StrafeRight:
            return keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
        case Action::MoveUp:
            return keys[SDL_SCANCODE_E];
        case Action::MoveDown:
            return keys[SDL_SCANCODE_Q];
        // Milestone 11: spacecraft attitude control while piloting (see
        // src/FlyingPrimitiveControl.h) — a fresh keyboard cluster (IJKL +
        // U/O) chosen specifically so it shares no scancode with anything
        // above, including the arrow-key aliases.
        case Action::PitchUp:
            return keys[SDL_SCANCODE_I];
        case Action::PitchDown:
            return keys[SDL_SCANCODE_K];
        case Action::YawLeft:
            return keys[SDL_SCANCODE_J];
        case Action::YawRight:
            return keys[SDL_SCANCODE_L];
        case Action::RollLeft:
            return keys[SDL_SCANCODE_U];
        case Action::RollRight:
            return keys[SDL_SCANCODE_O];
    }
    return false;
}

void Window::GetMouseDelta(int& deltaX, int& deltaY) const {
    if (m_testInputMode) {
        deltaX = m_testMouseDeltaX;
        deltaY = m_testMouseDeltaY;
        m_testMouseDeltaX = 0;
        m_testMouseDeltaY = 0;
        return;
    }

    // Always drain SDL's accumulator, even while not captured, so motion
    // that happened while the mouse was released doesn't reappear as a
    // jump the moment it's recaptured.
    int rawDeltaX = 0;
    int rawDeltaY = 0;
    SDL_GetRelativeMouseState(&rawDeltaX, &rawDeltaY);

    if (m_mouseCaptured) {
        deltaX = rawDeltaX;
        deltaY = rawDeltaY;
    } else {
        deltaX = 0;
        deltaY = 0;
    }
}

bool Window::ConsumeResetRequest() {
    if (m_testInputMode) {
        const bool requested = m_testResetRequested;
        m_testResetRequested = false;
        return requested;
    }
    const bool requested = m_resetRequested;
    m_resetRequested = false;
    return requested;
}

bool Window::ConsumeJumpRequest() {
    if (m_testInputMode) {
        const bool requested = m_testJumpRequested;
        m_testJumpRequested = false;
        return requested;
    }
    const bool requested = m_jumpRequested;
    m_jumpRequested = false;
    return requested;
}

bool Window::ConsumeControlToggleRequest() {
    if (m_testInputMode) {
        const bool requested = m_testControlToggleRequested;
        m_testControlToggleRequested = false;
        return requested;
    }
    const bool requested = m_controlToggleRequested;
    m_controlToggleRequested = false;
    return requested;
}

void Window::SetTestInputMode(bool enabled) {
    m_testInputMode = enabled;
}

void Window::SetTestActionState(Action action, bool active) {
    m_testActionState[static_cast<int>(action)] = active;
}

void Window::QueueTestMouseDelta(int deltaX, int deltaY) {
    m_testMouseDeltaX += deltaX;
    m_testMouseDeltaY += deltaY;
}

void Window::RequestTestJump() {
    m_testJumpRequested = true;
}

void Window::RequestTestReset() {
    m_testResetRequested = true;
}

void Window::RequestTestControlToggle() {
    m_testControlToggleRequested = true;
}
