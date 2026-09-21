#pragma once

#include <SDL2/SDL.h>

// Input actions the engine exposes to game logic. This is deliberately an
// action enum rather than raw key codes, so a later input source (mouse,
// controller) can drive the same actions without callers changing.
enum class Action {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
};

// Owns the OS window, the GL context, and OS event pumping. Combines the
// "Window" and "Input" responsibilities from the design brief into one
// class since Milestone 1's input is just "keyboard state of this window".
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

    int Width() const { return m_width; }
    int Height() const { return m_height; }

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_sdlInitialized = false;
    bool m_shouldClose = false;
    int m_width = 0;
    int m_height = 0;
};
