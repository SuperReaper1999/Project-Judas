#include "Window.h"

#include <cstdio>

bool Window::Init(const char* title, int width, int height) {
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

    m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                                 height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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

    // Milestone 2 is a free-flight 3D camera demo, so start with the mouse
    // captured for immediate look control.
    SDL_SetRelativeMouseMode(SDL_TRUE);
    m_mouseCaptured = true;

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
            }
        }
    }
}

void Window::SwapBuffers() {
    SDL_GL_SwapWindow(m_window);
}

bool Window::IsActionActive(Action action) const {
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
        case Action::Ascend:
            return keys[SDL_SCANCODE_SPACE];
        case Action::Descend:
            return keys[SDL_SCANCODE_LCTRL];
    }
    return false;
}

void Window::GetMouseDelta(int& deltaX, int& deltaY) const {
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
    const bool requested = m_resetRequested;
    m_resetRequested = false;
    return requested;
}
