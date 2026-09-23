#include "HUD.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>

#include "Renderer.h"

namespace {
constexpr float kMargin = 14.0f;
constexpr float kTextScale = 0.4f;
constexpr float kLineSpacing = 4.0f;
constexpr float kPanelPaddingX = 12.0f;
constexpr float kPanelPaddingY = 10.0f;

const glm::vec4 kPanelColor(0.05f, 0.06f, 0.08f, 0.55f);
const glm::vec4 kTextColor(0.92f, 0.95f, 0.98f, 1.0f);

// Milestone 16: the interaction prompt's own layout constants — a small
// panel centered horizontally, anchored a fixed margin above the bottom
// of the window (the conventional "prompt near the crosshair" placement),
// deliberately separate from the top-left telemetry panel's own layout
// above so neither one's sizing logic has to account for the other.
constexpr float kPromptTextScale = 0.5f;
constexpr float kPromptBottomMargin = 60.0f;
constexpr float kPromptPaddingX = 16.0f;
constexpr float kPromptPaddingY = 10.0f;
const glm::vec4 kPromptPanelColor(0.05f, 0.06f, 0.08f, 0.7f);
const glm::vec4 kPromptTextColor(0.98f, 0.96f, 0.85f, 1.0f);

// Formats one line of the HUD from live data — a free function (not a
// method) so it's trivially unit-testable without a Renderer/GL context;
// see tests/UITests.cpp.
std::string FormatLine(int index, const HUDViewData& data) {
    char buffer[96];
    switch (index) {
        case 0:
            std::snprintf(buffer, sizeof(buffer), "Support: %s", data.grounded ? "Grounded" : "Airborne");
            break;
        case 1:
            std::snprintf(buffer, sizeof(buffer), "Gravity: %.2f m/s^2", data.gravityMagnitude);
            break;
        case 2:
            std::snprintf(buffer, sizeof(buffer), "Control: %s",
                           data.controllingSpacecraft ? "Spacecraft" : "Player");
            break;
        case 3:
            std::snprintf(buffer, sizeof(buffer), "Pilot attachment: %s",
                           data.pilotAttached ? "Secured" : "Unsecured");
            break;
        default:
            std::snprintf(buffer, sizeof(buffer), "Spacecraft speed: %.2f m/s", data.spacecraftLinearSpeed);
            break;
    }
    return std::string(buffer);
}

constexpr int kLineCount = 5;
}  // namespace

void HUD::Draw(Renderer& renderer, int windowWidth, int windowHeight, const HUDViewData& data) const {
    const float lineHeight = renderer.GetUITextLineHeight(kTextScale) + kLineSpacing;

    float maxWidth = 0.0f;
    for (int i = 0; i < kLineCount; ++i) {
        maxWidth = std::max(maxWidth, renderer.MeasureUIText(FormatLine(i, data), kTextScale).x);
    }

    const glm::vec2 panelPosition(kMargin, kMargin);
    const glm::vec2 panelSize(maxWidth + kPanelPaddingX * 2.0f,
                               kPanelPaddingY * 2.0f + static_cast<float>(kLineCount) * lineHeight);
    renderer.DrawUIRect(panelPosition, panelSize, kPanelColor);

    for (int i = 0; i < kLineCount; ++i) {
        const glm::vec2 textPosition(kMargin + kPanelPaddingX,
                                      kMargin + kPanelPaddingY + static_cast<float>(i) * lineHeight);
        renderer.DrawUIText(FormatLine(i, data), textPosition, kTextScale, kTextColor);
    }

    // Milestone 16: the interaction prompt — drawn only when something is
    // currently selectable (see src/InteractionSystem.h); this is the
    // entire mechanism by which a prompt "disappears" when a target goes
    // out of range/facing — Application.cpp simply passes an empty
    // string that frame, and HUD draws nothing extra.
    if (!data.interactPrompt.empty()) {
        const glm::vec2 promptTextSize = renderer.MeasureUIText(data.interactPrompt, kPromptTextScale);
        const glm::vec2 promptPanelSize(promptTextSize.x + kPromptPaddingX * 2.0f,
                                         promptTextSize.y + kPromptPaddingY * 2.0f);
        const glm::vec2 promptPanelPosition(static_cast<float>(windowWidth) * 0.5f - promptPanelSize.x * 0.5f,
                                             static_cast<float>(windowHeight) - kPromptBottomMargin -
                                                 promptPanelSize.y);
        renderer.DrawUIRect(promptPanelPosition, promptPanelSize, kPromptPanelColor);
        renderer.DrawUIText(data.interactPrompt,
                             promptPanelPosition + glm::vec2(kPromptPaddingX, kPromptPaddingY),
                             kPromptTextScale, kPromptTextColor);
    }
}
