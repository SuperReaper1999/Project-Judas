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
    (void)windowHeight;
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
    (void)windowWidth;
}
