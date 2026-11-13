#pragma once

#include "EditorTheme.h"
#include "EditorDesign.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Small editor-only drawing primitives. Native ImGui widgets retain keyboard,
// focus and accessibility labels. Tool icons are vector geometry; the brand
// uses the shared app artwork. Neither introduces a second UI/layout system.
namespace EditorVisuals {
enum class Icon {
    Cube,
    Person,
    Folder,
    File,
    Script,
    Image,
    Audio,
    Scene,
    Search,
    Settings,
    Play,
    Stop,
    Move,
    Rotate,
    Scale,
    Camera,
    Animation,
    UI,
    Import,
    Plus,
    Globe,
    Light
};
inline ImVec4 AccentColor() {
    return ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
}
inline ImVec4 SurfaceColor(float lift = .025f) {
    auto c = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    const float direction = (c.x + c.y + c.z > 1.5f) ? -1.f : 1.f;
    c.x = std::clamp(c.x + direction * lift, 0.f, 1.f);
    c.y = std::clamp(c.y + direction * lift, 0.f, 1.f);
    c.z = std::clamp(c.z + direction * lift, 0.f, 1.f);
    return c;
}
inline ImFont *HeadingFont() {
    const auto *fonts = ImGui::GetIO().Fonts;
    return fonts->Fonts.Size > 1 ? fonts->Fonts[1] : ImGui::GetFont();
}
inline void DrawIcon(ImDrawList *draw, Icon icon, ImVec2 origin, float size, ImU32 color) {
    const float width = std::max(1.f, size * .07f);
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * size, origin.y + y * size); };
    const auto line = [&](float x, float y, float a, float b) {
        draw->AddLine(p(x, y), p(a, b), color, width);
    };
    const auto rect = [&](float x, float y, float a, float b) {
        draw->AddRect(p(x, y), p(a, b), color, size * .07f, 0, width);
    };
    switch (icon) {
    case Icon::Cube:
    case Icon::Scene:
        line(.5, .08, .9, .29);
        line(.9, .29, .9, .73);
        line(.9, .73, .5, .94);
        line(.5, .94, .1, .73);
        line(.1, .73, .1, .29);
        line(.1, .29, .5, .08);
        line(.1, .29, .5, .51);
        line(.5, .51, .9, .29);
        line(.5, .51, .5, .94);
        break;
    case Icon::Person:
        draw->AddCircle(p(.5, .26), size * .16f, color, 16, width);
        draw->PathArcTo(p(.5, .91), size * .34f, 3.14159f, 6.28318f, 16);
        draw->PathStroke(color, 0, width);
        line(.16, .91, .84, .91);
        break;
    case Icon::Folder:
        line(.08, .27, .42, .27);
        line(.42, .27, .52, .39);
        line(.52, .39, .93, .39);
        line(.93, .39, .88, .84);
        line(.88, .84, .08, .84);
        line(.08, .84, .08, .27);
        line(.08, .48, .93, .48);
        break;
    case Icon::Script:
        line(.32, .28, .12, .5);
        line(.12, .5, .32, .72);
        line(.68, .28, .88, .5);
        line(.88, .5, .68, .72);
        line(.58, .15, .42, .85);
        break;
    case Icon::Search:
        draw->AddCircle(p(.42, .4), size * .28f, color, 18, width);
        line(.63, .62, .9, .91);
        break;
    case Icon::Play:
        draw->AddTriangleFilled(p(.27, .15), p(.85, .5), p(.27, .85), color);
        break;
    case Icon::Stop:
        draw->AddRectFilled(p(.22, .22), p(.78, .78), color, size * .05f);
        break;
    case Icon::Plus:
        line(.5, .15, .5, .85);
        line(.15, .5, .85, .5);
        break;
    case Icon::Move:
        line(.5, .08, .5, .92);
        line(.08, .5, .92, .5);
        line(.5, .08, .35, .25);
        line(.5, .08, .65, .25);
        line(.08, .5, .25, .35);
        line(.08, .5, .25, .65);
        line(.5, .92, .35, .75);
        line(.5, .92, .65, .75);
        line(.92, .5, .75, .35);
        line(.92, .5, .75, .65);
        break;
    case Icon::Rotate:
        draw->PathArcTo(p(.5, .5), size * .34f, -1.3f, 3.8f, 24);
        draw->PathStroke(color, 0, width);
        line(.18, .28, .18, .08);
        line(.18, .28, .38, .26);
        break;
    case Icon::Scale:
        rect(.13, .52, .48, .87);
        line(.38, .62, .87, .13);
        line(.87, .13, .57, .13);
        line(.87, .13, .87, .43);
        break;
    case Icon::Audio:
        line(.12, .38, .32, .38);
        line(.32, .38, .57, .16);
        line(.57, .16, .57, .84);
        line(.57, .84, .32, .62);
        line(.32, .62, .12, .62);
        line(.12, .62, .12, .38);
        draw->PathArcTo(p(.52, .5), size * .35f, -.8f, .8f, 12);
        draw->PathStroke(color, 0, width);
        break;
    case Icon::Camera:
        rect(.08, .24, .92, .81);
        draw->AddCircle(p(.51, .52), size * .19f, color, 16, width);
        line(.27, .24, .33, .12);
        line(.33, .12, .66, .12);
        line(.66, .12, .73, .24);
        break;
    case Icon::Image:
        rect(.1, .12, .9, .88);
        draw->AddCircle(p(.68, .33), size * .08f, color, 12, width);
        line(.15, .77, .4, .47);
        line(.4, .47, .63, .72);
        line(.63, .72, .74, .59);
        line(.74, .59, .9, .77);
        break;
    case Icon::Animation:
        rect(.12, .16, .88, .84);
        line(.12, .32, .88, .32);
        line(.31, .16, .23, .32);
        line(.58, .16, .5, .32);
        line(.83, .16, .75, .32);
        line(.42, .43, .67, .58);
        line(.67, .58, .42, .73);
        break;
    case Icon::UI:
        rect(.08, .15, .92, .85);
        line(.08, .35, .92, .35);
        line(.35, .35, .35, .85);
        break;
    case Icon::Import:
        line(.5, .08, .5, .65);
        line(.5, .65, .3, .45);
        line(.5, .65, .7, .45);
        line(.14, .62, .14, .89);
        line(.14, .89, .86, .89);
        line(.86, .89, .86, .62);
        break;
    case Icon::Light:
        draw->AddCircle(p(.5, .42), size * .2f, color, 16, width);
        line(.4, .65, .6, .65);
        line(.4, .77, .6, .77);
        for (int i = 0; i < 6; ++i) {
            float a = float(i) * 1.0471976f;
            line(.5f + std::cos(a) * .31f, .42f + std::sin(a) * .31f, .5f + std::cos(a) * .42f,
                 .42f + std::sin(a) * .42f);
        }
        break;
    case Icon::Settings:
    case Icon::Globe:
        draw->AddCircle(p(.5, .5), size * .36f, color, 20, width);
        if (icon == Icon::Globe) {
            line(.15, .5, .85, .5);
            draw->AddEllipse(p(.5, .5), {size * .17f, size * .36f}, color, 0, 20, width);
        } else {
            draw->AddCircle(p(.5, .5), size * .12f, color, 12, width);
            for (int i = 0; i < 8; ++i) {
                float a = float(i) * .785398f;
                line(.5f + std::cos(a) * .35f, .5f + std::sin(a) * .35f, .5f + std::cos(a) * .48f,
                     .5f + std::sin(a) * .48f);
            }
        }
        break;
    case Icon::File:
        line(.2, .08, .62, .08);
        line(.62, .08, .84, .3);
        line(.84, .3, .84, .92);
        line(.84, .92, .2, .92);
        line(.2, .92, .2, .08);
        line(.62, .08, .62, .3);
        line(.62, .3, .84, .3);
        line(.33, .5, .7, .5);
        line(.33, .68, .7, .68);
        break;
    }
}
inline void IconText(Icon icon, const char *text, float size = 16.f) {
    size *= ImGui::GetIO().FontGlobalScale;
    const auto at = ImGui::GetCursorScreenPos();
    DrawIcon(ImGui::GetWindowDrawList(), icon, at, size, ImGui::GetColorU32(ImGuiCol_Text));
    ImGui::Dummy({size, size});
    ImGui::SameLine();
    ImGui::TextUnformatted(text);
}
// Backward-compatible convenience for the existing icon catalogue. Painting
// and interactions belong to the shared Judas control, not native Button style.
inline bool ActionButton(const char *label, Icon icon, bool primary = false, ImVec2 size = {}) {
    const float glyph = ImGui::GetFontSize();
    const float leading = glyph + ImGui::GetStyle().ItemInnerSpacing.x;
    const bool clicked = EditorDesign::ActionButton(label,
        primary ? EditorDesign::Tone::Primary : EditorDesign::Tone::Secondary, size, leading);
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    if (ImGui::IsItemVisible())
        DrawIcon(ImGui::GetWindowDrawList(), icon,
            {low.x + 12.f * ImGui::GetIO().FontGlobalScale, low.y + (high.y - low.y - glyph) * .5f},
            glyph, ImGui::GetColorU32(primary ? AccentColor() : ImGui::GetStyleColorVec4(ImGuiCol_Text)));
    return clicked;
}
inline void BrandMark(ImDrawList *draw, ImVec2 at, float size, std::uintptr_t image) {
    if (image)
        draw->AddImage(ImTextureID(image), at, {at.x + size, at.y + size}, {0, 1}, {1, 0});
    else
        draw->AddText(at, ImGui::GetColorU32(ImGuiCol_Text), "J");
}

} // namespace EditorVisuals
