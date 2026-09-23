#pragma once

#include <glm/glm.hpp>

#include "UIStack.h"
#include "UIWidgets.h"

class Renderer;

// Milestone 13: the concrete pause menu — a root screen (Resume/Options/
// Quit) and one nested options screen (a single "Show HUD" toggle/Back),
// composed from the generic src/UIWidgets.h/src/UIStack.h primitives. This
// is the ONE place in the engine that knows what a "pause menu" is; those
// primitives themselves stay completely agnostic to it (see their own
// header comments).
//
// Owns exactly the state M13's required flow needs:
// - the navigation stack itself (open/closed/nested, see UIStack);
// - whether the HUD should currently draw (the options screen's one real,
//   non-invented toggle — see docs/ARCHITECTURE.md, "Milestone 13," for
//   why this was chosen over inventing a settings system with nothing
//   genuine to configure);
// - a quit-requested flag, set by the Quit button and read by
//   Application::Run exactly like Window::ShouldClose() already is,
//   rather than this class reaching into SDL/Window itself.
//
// Deliberately does NOT know about gameplay, physics, or input devices —
// Application is the one place that decides what a key/mouse event means
// and calls the methods below; see docs/ARCHITECTURE.md's own "Input
// ownership" section for the single boundary this keeps the rest of the
// engine from needing to scatter `if (menuOpen)` checks around.
class PauseMenu {
public:
    PauseMenu();

    bool IsOpen() const { return m_stack.IsOpen(); }
    bool IsHudVisible() const { return m_hudVisible; }
    bool QuitRequested() const { return m_quitRequested; }

    // The single contextual "back" action (bound to one key — see
    // src/Window.h's UI-back request): closed -> open (push the pause
    // root); on a nested screen -> pop back to its parent; on the pause
    // root itself -> close entirely (resume). Matches this milestone's
    // required flow (gameplay -> pause -> nested -> back -> resume ->
    // gameplay) with one action, not a separate open/close/back trio the
    // caller has to pick between.
    void HandleBackRequest();

    void NavigateUp();
    void NavigateDown();
    void Activate();
    void HandleMouseMove(const glm::vec2& point);
    // Returns true if the click hit a button (Application uses this only
    // for symmetry with other Consume*-style input calls; the menu already
    // acted on the click either way).
    bool HandleMouseClick(const glm::vec2& point);

    // Re-lays-out whichever screen(s) exist for the CURRENT window size —
    // cheap enough to call unconditionally every frame the menu is open,
    // exactly like UIMenuScreen::Layout's own doc comment explains (see
    // src/UIWidgets.h) — this is what makes the menu resize-safe without
    // any resize-event plumbing.
    void Layout(int windowWidth, int windowHeight);

    // Draws a full-screen dim overlay (so gameplay behind the menu reads
    // as visibly inactive) plus whichever screen is currently on top of
    // the stack. A no-op if the menu is closed.
    void Draw(Renderer& renderer, int windowWidth, int windowHeight) const;

private:
    UIMenuScreen m_pauseScreen;
    UIMenuScreen m_optionsScreen;
    UIStack m_stack;
    bool m_hudVisible = true;
    bool m_quitRequested = false;
};
