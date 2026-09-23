#pragma once

#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class Renderer;
class UIStack;

// Milestone 13: the minimum reusable UI capability this milestone's pause
// menu actually needs — a titled screen holding a vertical list of
// selectable buttons, keyboard- and mouse-navigable, resize-safe. NOT a
// general retained-mode UI framework: no arbitrary widget tree, no
// scrolling, no text input, no generic layout/anchoring beyond "a column
// of buttons under a title, centered on screen" — see
// docs/ARCHITECTURE.md, "Milestone 13," for why this shape was judged
// sufficient for "pause menu + one nested options screen."
//
// A UIMenuScreen renders itself via Renderer's UI overlay calls
// (DrawUIRect/DrawUIText — see src/Renderer.h) and reads no gameplay state
// directly; the label a button shows and what it does when activated are
// both supplied by whoever constructs it (src/PauseMenu.h), keeping this
// class itself completely agnostic to what a "pause menu" or "options
// screen" even is.
struct UIButton {
    std::string label;
    // Given `UIStack&` so a button can push/pop/close the stack it lives
    // in (e.g. "Back", "Resume") without UIMenuScreen itself knowing about
    // navigation semantics — see UIStack below.
    std::function<void(UIStack&)> onActivate;

    // Computed by UIMenuScreen::Layout each call (see "resize-safe
    // rendering" below) — screen-space pixel rect, top-left origin.
    glm::vec2 rectPosition{0.0f, 0.0f};
    glm::vec2 rectSize{0.0f, 0.0f};
};

class UIMenuScreen {
public:
    UIMenuScreen(std::string title, std::vector<UIButton> buttons);

    // Recomputes every button's screen-space rect for the CURRENT window
    // size — called once per frame before Draw (see src/PauseMenu.cpp),
    // the same "recompute from current size every frame rather than react
    // to a resize event" approach Renderer::SetCamera's aspect ratio
    // already uses (see docs/ARCHITECTURE.md). Cheap (a handful of
    // buttons), so recomputing unconditionally costs nothing observable.
    void Layout(int windowWidth, int windowHeight);

    void Draw(Renderer& renderer) const;

    // Keyboard navigation: `direction` is -1 (up) or +1 (down), wraps
    // around the button list. A no-op on a screen with no buttons.
    void MoveFocus(int direction);

    // Activates the currently focused button, if any.
    void Activate(UIStack& stack);

    // Mouse support: `point` in screen-space pixels. HandleMouseMove only
    // updates which button is focused/hovered (so keyboard nav afterward
    // continues from wherever the mouse last was — the two input methods
    // share one focus concept rather than tracking hover and focus
    // separately); HandleMouseClick additionally activates the button
    // under `point`, if any, and reports whether it hit one.
    void HandleMouseMove(const glm::vec2& point);
    bool HandleMouseClick(const glm::vec2& point, UIStack& stack);

    const std::string& Title() const { return m_title; }

    // Rewrites one button's visible label in place — the one mutation a
    // caller needs for a toggle-style button (e.g. "Show HUD: On/Off",
    // see src/PauseMenu.cpp) whose text reflects live state rather than
    // being fixed at construction. Out-of-range `index` is a no-op.
    void SetButtonLabel(int index, std::string label);

private:
    int HitTest(const glm::vec2& point) const;

    std::string m_title;
    std::vector<UIButton> m_buttons;
    int m_focusIndex = 0;
};
