#include "HUD.h"

#include <algorithm>
#include <vector>
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
    char buffer[128];
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
        case 4:
            std::snprintf(buffer, sizeof(buffer), "Ship world speed: %.2f m/s",
                          data.spacecraftLinearSpeed);
            break;
        case 5:
            if (data.celestialReferenceAvailable) {
                std::snprintf(buffer, sizeof(buffer), "%s world speed: %.2f m/s",
                              data.celestialReferenceLabel.c_str(), data.celestialBodyWorldSpeed);
            } else {
                std::snprintf(buffer, sizeof(buffer), "Body A world speed: N/A");
            }
            break;
        case 6:
            if (data.celestialReferenceAvailable) {
                std::snprintf(buffer, sizeof(buffer), "Ship rel. speed to %s: %.2f m/s",
                              data.celestialReferenceLabel.c_str(), data.spacecraftRelativeCelestialSpeed);
            } else {
                std::snprintf(buffer, sizeof(buffer), "Ship rel. speed to A: N/A");
            }
            break;
        case 7:
            std::snprintf(buffer, sizeof(buffer), "Pilot world speed: %.2f m/s",
                          data.pilotWorldSpeed);
            break;
        case 8:
            std::snprintf(buffer, sizeof(buffer), "Pilot rel. speed to ship: %.2f m/s",
                          data.pilotRelativeSpacecraftSpeed);
            break;
        case 9:
            std::snprintf(buffer, sizeof(buffer), "Spacecraft SAS: %s",
                          data.spacecraftSasEnabled ? "ON" : "OFF");
            break;
        case 10:
            std::snprintf(buffer, sizeof(buffer), "World origin: (%.2e, %.2e, %.2e) m",
                          data.worldOrigin.x, data.worldOrigin.y, data.worldOrigin.z);
            break;
        case 11:
            std::snprintf(buffer, sizeof(buffer), "Player absolute: (%.1f, %.1f, %.1f) m",
                          data.absolutePlayerPosition.x, data.absolutePlayerPosition.y,
                          data.absolutePlayerPosition.z);
            break;
        case 12:
            if (!data.atmosphereAvailable) return std::string();
            std::snprintf(buffer, sizeof(buffer), "Gas: %.5f kg/m^3 | %.3f Pa",
                          data.atmosphereDensity, data.atmospherePressure);
            break;
        case 13:
            if (!data.atmosphereAvailable) return std::string();
            std::snprintf(buffer, sizeof(buffer), "Airspeed: %.2f m/s | q %.3f Pa",
                          data.spacecraftRelativeAirspeed, data.spacecraftDynamicPressure);
            break;
        case 14:
            if (!data.atmosphereAvailable) return std::string();
            std::snprintf(buffer, sizeof(buffer), "Aerodynamic drag: %.2f N",
                          data.spacecraftAerodynamicForce);
            break;
        default:
            if (data.thermalAvailable && index == 15) {
                std::snprintf(buffer, sizeof(buffer), "Heater C: %s | Player O2: %.5f kg/m^3",
                              data.igniterPowered ? "ON" : "OFF", data.oxidizerMassDensity);
            } else if (data.thermalAvailable && index >= 16 &&
                       static_cast<std::size_t>(index - 16) < data.thermalBodies.size()) {
                const HUDThermalBody& body = data.thermalBodies[index - 16];
                std::snprintf(buffer, sizeof(buffer), "%s: %.0f K | fuel %.3f kg | %.4f kg/s",
                              body.label.c_str(), body.temperatureKelvin,
                              body.remainingFuelKg, body.burnRateKgPerSecond);
            } else {
                // Milestone 29: lifecycle lines follow whatever thermal lines
                // exist (see Draw's lineCount).
                const int lifecycleStart = 15 + (data.thermalAvailable ? 1 + static_cast<int>(data.thermalBodies.size()) : 0);
                if (data.lifecycleAvailable && index == lifecycleStart) {
                    std::snprintf(buffer, sizeof(buffer),
                                  "Entities: %zu full | %zu coarse | %zu dormant | %zu destroyed | %zu bodies",
                                  data.entitiesFull, data.entitiesCoarse, data.entitiesDormant,
                                  data.entitiesDestroyed, data.physicsBodies);
                } else if (data.lifecycleAvailable && index == lifecycleStart + 1) {
                    std::snprintf(buffer, sizeof(buffer), "World state: %s",
                                  data.worldStateInfo.empty() ? "(none)" : data.worldStateInfo.c_str());
                } else if (data.lifecycleAvailable && index == lifecycleStart + 2) {
                    std::snprintf(buffer, sizeof(buffer), "%s",
                                  data.lifecycleMessage.empty() ? "Z spawn | Y destroy | F6 save | F7 delete state"
                                                                : data.lifecycleMessage.c_str());
                } else {
                    return std::string();
                }
            }
            break;
    }
    return std::string(buffer);
}

}  // namespace

void HUD::Draw(Renderer& renderer, int windowWidth, int windowHeight, const HUDViewData& data) const {
    // Line indices are fixed per section (see FormatLine): the lifecycle
    // section always sits after the full 15-line atmosphere layout, so a
    // scene without an atmosphere but with lifecycle data draws the
    // atmosphere lines' slots as empty strings and skips them below.
    const int thermalLines = data.thermalAvailable ? 1 + static_cast<int>(data.thermalBodies.size()) : 0;
    const int lineCount = data.lifecycleAvailable ? 15 + thermalLines + 3
                                                  : (data.atmosphereAvailable ? 15 : 12) + thermalLines;
    const float lineHeight = renderer.GetUITextLineHeight(kTextScale) + kLineSpacing;

    // Empty slots (sections this scene lacks) are dropped, not drawn blank.
    std::vector<std::string> lines;
    float maxWidth = 0.0f;
    for (int i = 0; i < lineCount; ++i) {
        std::string line = FormatLine(i, data);
        if (line.empty()) continue;
        maxWidth = std::max(maxWidth, renderer.MeasureUIText(line, kTextScale).x);
        lines.push_back(std::move(line));
    }

    const glm::vec2 panelPosition(kMargin, kMargin);
    const glm::vec2 panelSize(maxWidth + kPanelPaddingX * 2.0f,
                               kPanelPaddingY * 2.0f + static_cast<float>(lines.size()) * lineHeight);
    renderer.DrawUIRect(panelPosition, panelSize, kPanelColor);

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const glm::vec2 textPosition(kMargin + kPanelPaddingX,
                                      kMargin + kPanelPaddingY + static_cast<float>(i) * lineHeight);
        renderer.DrawUIText(lines[i], textPosition, kTextScale, kTextColor);
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
