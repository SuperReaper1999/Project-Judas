#include "UIWidgets.h"

#include <algorithm>

#include "Renderer.h"
#include "UIStack.h"

namespace {
// Demo/authoring-level layout constants — deliberately not configurable
// per-screen (both the pause screen and the options screen use the exact
// same button sizing/spacing/colors): M13 needs exactly one visual style
// of menu, not a themeable UI system. See docs/ARCHITECTURE.md, "Milestone
// 13," for why a generalized layout/theme system was judged out of scope.
constexpr float kButtonWidth = 260.0f;
constexpr float kButtonHeight = 44.0f;
constexpr float kButtonSpacing = 14.0f;
constexpr float kTitleSpacing = 46.0f;
constexpr float kTextScale = 0.5f;
constexpr float kTitleTextScale = 0.75f;

const glm::vec4 kButtonColor(0.16f, 0.17f, 0.21f, 0.92f);
const glm::vec4 kButtonFocusedColor(0.30f, 0.42f, 0.62f, 0.95f);
const glm::vec4 kTextColor(0.93f, 0.94f, 0.96f, 1.0f);
const glm::vec4 kTitleColor(0.98f, 0.98f, 1.0f, 1.0f);
}  // namespace

UIMenuScreen::UIMenuScreen(std::string title, std::vector<UIButton> buttons)
    : m_title(std::move(title)), m_buttons(std::move(buttons)) {}

void UIMenuScreen::Layout(int windowWidth, int windowHeight) {
    const float totalHeight =
        kTitleSpacing +
        static_cast<float>(m_buttons.size()) * kButtonHeight +
        static_cast<float>(std::max(static_cast<int>(m_buttons.size()) - 1, 0)) * kButtonSpacing;
    const float startX = static_cast<float>(windowWidth) * 0.5f - kButtonWidth * 0.5f;
    float y = static_cast<float>(windowHeight) * 0.5f - totalHeight * 0.5f + kTitleSpacing;

    for (UIButton& button : m_buttons) {
        button.rectPosition = glm::vec2(startX, y);
        button.rectSize = glm::vec2(kButtonWidth, kButtonHeight);
        y += kButtonHeight + kButtonSpacing;
    }
}

void UIMenuScreen::Draw(Renderer& renderer) const {
    if (!m_buttons.empty()) {
        const glm::vec2 titlePosition(m_buttons.front().rectPosition.x,
                                       m_buttons.front().rectPosition.y - kTitleSpacing + 6.0f);
        renderer.DrawUIText(m_title, titlePosition, kTitleTextScale, kTitleColor);
    }

    for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
        const UIButton& button = m_buttons[static_cast<size_t>(i)];
        const bool focused = (i == m_focusIndex);
        renderer.DrawUIRect(button.rectPosition, button.rectSize,
                             focused ? kButtonFocusedColor : kButtonColor);

        const glm::vec2 textSize = renderer.MeasureUIText(button.label, kTextScale);
        const glm::vec2 textPosition(
            button.rectPosition.x + button.rectSize.x * 0.5f - textSize.x * 0.5f,
            button.rectPosition.y + button.rectSize.y * 0.5f - textSize.y * 0.5f);
        renderer.DrawUIText(button.label, textPosition, kTextScale, kTextColor);
    }
}

void UIMenuScreen::MoveFocus(int direction) {
    if (m_buttons.empty()) return;
    const int count = static_cast<int>(m_buttons.size());
    m_focusIndex = ((m_focusIndex + direction) % count + count) % count;
}

void UIMenuScreen::Activate(UIStack& stack) {
    if (m_buttons.empty()) return;
    if (m_buttons[static_cast<size_t>(m_focusIndex)].onActivate) {
        m_buttons[static_cast<size_t>(m_focusIndex)].onActivate(stack);
    }
}

void UIMenuScreen::SetButtonLabel(int index, std::string label) {
    if (index < 0 || index >= static_cast<int>(m_buttons.size())) return;
    m_buttons[static_cast<size_t>(index)].label = std::move(label);
}

int UIMenuScreen::HitTest(const glm::vec2& point) const {
    for (int i = 0; i < static_cast<int>(m_buttons.size()); ++i) {
        const UIButton& button = m_buttons[static_cast<size_t>(i)];
        if (point.x >= button.rectPosition.x && point.x <= button.rectPosition.x + button.rectSize.x &&
            point.y >= button.rectPosition.y && point.y <= button.rectPosition.y + button.rectSize.y) {
            return i;
        }
    }
    return -1;
}

void UIMenuScreen::HandleMouseMove(const glm::vec2& point) {
    const int hit = HitTest(point);
    if (hit >= 0) m_focusIndex = hit;
}

bool UIMenuScreen::HandleMouseClick(const glm::vec2& point, UIStack& stack) {
    const int hit = HitTest(point);
    if (hit < 0) return false;
    m_focusIndex = hit;
    Activate(stack);
    return true;
}
