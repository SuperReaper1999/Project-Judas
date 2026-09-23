#include "PauseMenu.h"

#include "Renderer.h"

namespace {
const glm::vec4 kDimOverlayColor(0.0f, 0.0f, 0.0f, 0.55f);

std::string HudToggleLabel(bool hudVisible) {
    return hudVisible ? "Show HUD: On" : "Show HUD: Off";
}
}  // namespace

PauseMenu::PauseMenu()
    : m_pauseScreen("Paused",
                    {
                        UIButton{"Resume", [](UIStack& stack) { stack.Clear(); }},
                        UIButton{"Options", [this](UIStack& stack) { stack.Push(m_optionsScreen); }},
                        UIButton{"Quit", [this](UIStack&) { m_quitRequested = true; }},
                    }),
      m_optionsScreen(
          "Options",
          {
              // The toggle button's activation callback both flips
              // m_hudVisible AND rewrites its own label in place (see
              // UIMenuScreen::SetButtonLabel) — the one place this menu's
              // buttons mutate their own presentation rather than just
              // navigating the stack.
              UIButton{HudToggleLabel(true),
                       [this](UIStack&) {
                           m_hudVisible = !m_hudVisible;
                           m_optionsScreen.SetButtonLabel(0, HudToggleLabel(m_hudVisible));
                       }},
              UIButton{"Back", [](UIStack& stack) { stack.Pop(); }},
          }) {}

void PauseMenu::HandleBackRequest() {
    if (!m_stack.IsOpen()) {
        m_stack.Push(m_pauseScreen);
        return;
    }
    m_stack.Pop();
}

void PauseMenu::NavigateUp() {
    if (UIMenuScreen* top = m_stack.Top()) top->MoveFocus(-1);
}

void PauseMenu::NavigateDown() {
    if (UIMenuScreen* top = m_stack.Top()) top->MoveFocus(1);
}

void PauseMenu::Activate() {
    UIMenuScreen* top = m_stack.Top();
    if (!top) return;
    top->Activate(m_stack);
}

void PauseMenu::HandleMouseMove(const glm::vec2& point) {
    if (UIMenuScreen* top = m_stack.Top()) top->HandleMouseMove(point);
}

bool PauseMenu::HandleMouseClick(const glm::vec2& point) {
    UIMenuScreen* top = m_stack.Top();
    if (!top) return false;
    return top->HandleMouseClick(point, m_stack);
}

void PauseMenu::Layout(int windowWidth, int windowHeight) {
    if (!m_stack.IsOpen()) return;
    m_pauseScreen.Layout(windowWidth, windowHeight);
    m_optionsScreen.Layout(windowWidth, windowHeight);
}

void PauseMenu::Draw(Renderer& renderer, int windowWidth, int windowHeight) const {
    if (!m_stack.IsOpen()) return;
    renderer.DrawUIRect(glm::vec2(0.0f, 0.0f),
                         glm::vec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight)),
                         kDimOverlayColor);
    if (const UIMenuScreen* top = m_stack.Top()) top->Draw(renderer);
}
