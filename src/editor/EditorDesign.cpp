#include "EditorDesign.h"
#include "EditorVisuals.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace EditorDesign {
namespace {
ImVec4 Blend(ImVec4 a, ImVec4 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}
ImVec4 Accent(Tone tone) {
    return tone == Tone::Danger ? ImVec4{.94f, .30f, .39f, 1} : EditorVisuals::AccentColor();
}
void Gradient(ImDrawList *draw, ImVec2 lo, ImVec2 hi, ImVec4 top, ImVec4 bottom, float rounding) {
    const int first = draw->VtxBuffer.Size;
    draw->AddRectFilled(lo, hi, IM_COL32_WHITE, rounding);
    const float span = std::max(1.f, hi.y - lo.y);
    for (int i = first; i < draw->VtxBuffer.Size; ++i) {
        auto &vertex = draw->VtxBuffer[i];
        auto color = Blend(top, bottom, std::clamp((vertex.pos.y - lo.y) / span, 0.f, 1.f));
        // Rounded fills carry transparent fringe vertices. Preserve coverage
        // while tinting; replacing their alpha would make corners jagged.
        color.w *= float((vertex.col >> IM_COL32_A_SHIFT) & 255) / 255.f;
        vertex.col = ImGui::GetColorU32(color);
    }
}
void FocusRing(ImDrawList *draw, ImVec2 lo, ImVec2 hi, float rounding) {
    if (ImGui::GetCurrentContext()->NavCursorVisible && ImGui::IsItemFocused()) {
        const float gap = 2 * ImGui::GetIO().FontGlobalScale;
        draw->AddRect({lo.x - gap, lo.y - gap}, {hi.x + gap, hi.y + gap},
                      ImGui::GetColorU32(ImGuiCol_NavCursor), rounding + gap, 0, 1.5f);
    }
}
void Label(ImDrawList *draw, const char *label, ImVec2 lo, ImVec2 hi, float leading = 0,
           bool bold = false) {
    auto *font = bold ? EditorVisuals::HeadingFont() : ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    const float padding = ImGui::GetStyle().FramePadding.x;
    const ImVec4 clip{lo.x + padding + leading, lo.y, hi.x - padding, hi.y};
    draw->AddText(font, size, {clip.x, lo.y + (hi.y - lo.y - size) * .5f},
                  ImGui::GetColorU32(ImGuiCol_Text), label, ImGui::FindRenderedTextEnd(label), 0,
                  &clip);
}
ImVec2 ButtonSize(const char *label, ImVec2 size, float leading, float height, bool bold = false) {
    const auto available = ImGui::GetContentRegionAvail();
    if (size.x == 0) {
        auto *font = bold ? EditorVisuals::HeadingFont() : ImGui::GetFont();
        const float labelWidth = font->CalcTextSizeA(ImGui::GetFontSize(), INFINITY, 0, label,
                                                     ImGui::FindRenderedTextEnd(label))
                                     .x;
        size.x = std::ceil(labelWidth) + leading + 2 * ImGui::GetStyle().FramePadding.x;
    } else if (size.x < 0)
        size.x += available.x;
    if (size.y == 0)
        size.y = std::max(ImGui::GetFrameHeight(), height * ImGui::GetIO().FontGlobalScale);
    else if (size.y < 0)
        size.y += available.y;
    return {std::max(1.f, size.x), std::max(1.f, size.y)};
}
class RowColumns {
  public:
    RowColumns(bool span, ImVec2 &size)
        : captionRight(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x),
          owner(ImGui::GetCurrentWindow()), previousMax(owner->DC.CursorMaxPos.x) {
        if (auto *table = ImGui::GetCurrentTable(); span && table) {
            spanning = true;
            size.x = std::max(1.f, table->WorkRect.Max.x - ImGui::GetCursorScreenPos().x);
            // Frozen headers constrain this column's vertical clip. Span only
            // horizontally, otherwise scrolled rows could paint over a header.
            ImGui::PushClipRect({table->InnerClipRect.Min.x, owner->ClipRect.Min.y},
                                {table->InnerClipRect.Max.x, owner->ClipRect.Max.y}, false);
        }
    }
    ~RowColumns() {
        if (spanning) {
            // The hit surface spans the row; it must not make the Name column
            // report the entire table as its requested content width.
            owner->DC.CursorMaxPos.x = std::max(previousMax, captionRight);
            ImGui::PopClipRect();
        }
    }
    float captionRight;

  private:
    ImGuiWindow *owner;
    float previousMax;
    bool spanning = false;
};

ImGuiLastItemData LastItemFor(ImGuiWindow *owner) {
    auto &g = *ImGui::GetCurrentContext();
    if (g.CurrentWindow == owner)
        return g.LastItemData;
    for (int i = 1; i < g.CurrentWindowStack.Size; ++i)
        if (g.CurrentWindowStack[i - 1].Window == owner)
            return g.CurrentWindowStack[i].ParentLastItemDataBackup;
    return {};
}
} // namespace

bool ActionButton(const char *label, Tone tone, ImVec2 size, float leadingIconSpace) {
    if (ImGui::GetCurrentWindow()->SkipItems)
        return false;
    size = ButtonSize(label, size, leadingIconSpace, 32, tone == Tone::Primary);
    const bool pressed = ImGui::InvisibleButton(label, size, ImGuiButtonFlags_EnableNav);
    if (!ImGui::IsItemVisible())
        return pressed;
    const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    auto *draw = ImGui::GetWindowDrawList();
    const bool hover = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    const float rounding = 6 * ImGui::GetIO().FontGlobalScale;
    auto top = EditorVisuals::SurfaceColor(tone == Tone::Quiet ? .025f : .12f);
    auto bottom = EditorVisuals::SurfaceColor(tone == Tone::Quiet ? .008f : .075f);
    if (tone == Tone::Primary || tone == Tone::Danger) {
        top = Blend(top, Accent(tone), active ? .50f : hover ? .46f : .32f);
        bottom = Blend(bottom, Accent(tone), active ? .36f : hover ? .31f : .20f);
    } else if (hover || active) {
        top = Blend(top, Accent(tone), active ? .22f : .10f);
        bottom = Blend(bottom, Accent(tone), active ? .15f : .05f);
    }
    if (tone != Tone::Quiet) {
        draw->AddRectFilled({lo.x, lo.y + 2}, {hi.x, hi.y + 2}, ImGui::GetColorU32({0, 0, 0, .18f}),
                            rounding);
    }
    Gradient(draw, lo, hi, top, bottom, rounding);
    auto edge = tone == Tone::Primary || tone == Tone::Danger
                    ? Accent(tone)
                    : ImGui::GetStyleColorVec4(ImGuiCol_Border);
    edge.w *= hover ? .85f : .55f;
    if (tone != Tone::Quiet || hover)
        draw->AddRect(lo, hi, ImGui::GetColorU32(edge), rounding);
    Label(draw, label, lo, hi, leadingIconSpace, tone == Tone::Primary);
    FocusRing(draw, lo, hi, rounding);
    return pressed;
}

bool Toggle(const char *label, bool *value) {
    if (ImGui::GetCurrentWindow()->SkipItems)
        return false;
    const float scale = ImGui::GetIO().FontGlobalScale;
    const float trackWidth = 34 * scale, trackHeight = 18 * scale;
    const auto size = ButtonSize(label, {}, trackWidth + 12 * scale, 30);
    const bool pressed = ImGui::InvisibleButton(label, size, ImGuiButtonFlags_EnableNav);
    if (pressed) {
        *value = !*value;
        ImGui::MarkItemEdited(ImGui::GetItemID());
    }
    if (!ImGui::IsItemVisible())
        return pressed;
    const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    auto *draw = ImGui::GetWindowDrawList();
    const float x = hi.x - ImGui::GetStyle().FramePadding.x - trackWidth;
    const float y = lo.y + (hi.y - lo.y - trackHeight) * .5f;
    auto track = *value ? Accent(Tone::Primary) : EditorVisuals::SurfaceColor(.16f);
    track.w = *value ? .85f : 1.f;
    draw->AddRectFilled({x, y}, {x + trackWidth, y + trackHeight}, ImGui::GetColorU32(track),
                        trackHeight * .5f);
    draw->AddCircleFilled(
        {x + (*value ? trackWidth - trackHeight * .5f : trackHeight * .5f), y + trackHeight * .5f},
        trackHeight * .5f - 3 * scale, ImGui::GetColorU32(ImGuiCol_Text), 16);
    // Label applies its own right padding. Reserve the same gap used by the
    // size calculation rather than subtracting that padding a second time.
    Label(draw, label, lo, {x - 12 * scale + ImGui::GetStyle().FramePadding.x, hi.y});
    FocusRing(draw, lo, hi, 5 * scale);
    return pressed;
}

bool Row(const char *label, bool selected, ImVec2 size, bool allowDoubleClick, float leadingIconSpace,
         bool spanColumns) {
    if (ImGui::GetCurrentWindow()->SkipItems)
        return false;
    if (size.x == 0)
        size.x = ImGui::GetContentRegionAvail().x;
    size = ButtonSize(label, size, leadingIconSpace, 26);
    RowColumns columns(spanColumns, size);
    const auto flags =
        ImGuiButtonFlags_EnableNav | (allowDoubleClick ? ImGuiButtonFlags_PressedOnClickRelease |
                                                             ImGuiButtonFlags_PressedOnDoubleClick
                                                       : 0);
    const bool pressed = ImGui::InvisibleButton(label, size, flags);
    if (pressed && (ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_Popup))
        ImGui::CloseCurrentPopup();
    if (!ImGui::IsItemVisible())
        return pressed;
    const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    auto *draw = ImGui::GetWindowDrawList();
    const float rounding = 5 * ImGui::GetIO().FontGlobalScale;
    if (selected || ImGui::IsItemHovered()) {
        auto background =
            Blend(EditorVisuals::SurfaceColor(.04f), Accent(Tone::Primary), selected ? .22f : .08f);
        draw->AddRectFilled(lo, hi, ImGui::GetColorU32(background), rounding);
        if (selected) {
            const auto accent = ImGui::GetColorU32(Accent(Tone::Primary));
            draw->AddRectFilled({lo.x, lo.y + 4}, {lo.x + 3, hi.y - 4}, accent, 2);
        }
    }
    Label(draw, label, lo, {std::min(hi.x, columns.captionRight), hi.y}, leadingIconSpace);
    FocusRing(draw, lo, hi, rounding);
    return pressed;
}

bool Disclosure(const char *label, bool defaultOpen, ImGuiID explicitId, float leadingIconSpace) {
    if (ImGui::GetCurrentWindow()->SkipItems)
        return false;
    const auto id = explicitId ? explicitId : ImGui::GetID(label);
    ImGui::PushOverrideID(id);
    ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Header, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0, 0, 0, 0});
    const auto flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                       ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
                       (defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    const bool open = ImGui::TreeNodeEx("", flags);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    if (!ImGui::IsItemVisible())
        return open;
    const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    auto *draw = ImGui::GetWindowDrawList();
    const float scale = ImGui::GetIO().FontGlobalScale, glyph = ImGui::GetFontSize();
    auto top = EditorVisuals::SurfaceColor(ImGui::IsItemHovered() ? .15f : .105f);
    Gradient(draw, lo, hi, top, EditorVisuals::SurfaceColor(.065f), 5 * scale);
    const ImVec2 center{lo.x + 12 * scale, (lo.y + hi.y) * .5f};
    const float radius = 3.5f * scale;
    if (open)
        draw->AddTriangleFilled(
            {center.x - radius, center.y - radius * .5f}, {center.x + radius, center.y - radius * .5f},
            {center.x, center.y + radius}, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    else
        draw->AddTriangleFilled(
            {center.x - radius * .5f, center.y - radius}, {center.x - radius * .5f, center.y + radius},
            {center.x + radius, center.y}, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    Label(draw, label, lo, hi, glyph + leadingIconSpace, true);
    FocusRing(draw, lo, hi, 5 * scale);
    return open;
}

bool TreeRow(const char *label, bool selected, bool hasChildren, bool defaultOpen,
             float leadingIconSpace) {
    if (ImGui::GetCurrentWindow()->SkipItems)
        return false;
    ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Header, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0, 0, 0, 0});
    const auto flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                       ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_FramePadding |
                       ImGuiTreeNodeFlags_NoTreePushOnOpen |
                       (hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf) |
                       (defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0) |
                       (selected ? ImGuiTreeNodeFlags_Selected : 0);
    const bool open = ImGui::TreeNodeEx(label, flags);
    ImGui::PopStyleColor(4);
    if (!ImGui::IsItemVisible())
        return open;
    const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    auto *draw = ImGui::GetWindowDrawList();
    const float scale = ImGui::GetIO().FontGlobalScale;
    if (selected || ImGui::IsItemHovered()) {
        const auto fill =
            Blend(EditorVisuals::SurfaceColor(.035f), Accent(Tone::Primary), selected ? .22f : .08f);
        draw->AddRectFilled(lo, hi, ImGui::GetColorU32(fill), 5 * scale);
        if (selected)
            draw->AddRectFilled({lo.x, lo.y + 3}, {lo.x + 3 * scale, hi.y - 3},
                                ImGui::GetColorU32(Accent(Tone::Primary)), 2 * scale);
    }
    if (hasChildren) {
        const ImVec2 center{lo.x + 10 * scale, (lo.y + hi.y) * .5f};
        const float radius = 3 * scale;
        const auto color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        if (open)
            draw->AddTriangleFilled({center.x - radius, center.y - radius * .5f},
                                    {center.x + radius, center.y - radius * .5f},
                                    {center.x, center.y + radius}, color);
        else
            draw->AddTriangleFilled({center.x - radius * .5f, center.y - radius},
                                    {center.x - radius * .5f, center.y + radius},
                                    {center.x + radius, center.y}, color);
    }
    Label(draw, label, lo, hi, ImGui::GetFontSize() + leadingIconSpace);
    FocusRing(draw, lo, hi, 5 * scale);
    return open;
}

Field::Field() {
    auto *owner = ImGui::GetCurrentWindow();
    if (owner->SkipItems)
        return;
    window = owner;
    draw = owner->DrawList;
    splitter.Split(draw, 2);
    splitter.SetCurrentChannel(draw, 1);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0, 0, 0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
}
Field::~Field() {
    if (!draw)
        return;
    const auto item = LastItemFor(window);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    splitter.SetCurrentChannel(draw, 0);
    if (item.NavRect.Overlaps(window->ClipRect)) {
        const bool active = ImGui::GetActiveID() == item.ID;
        const bool hovered = (item.StatusFlags & ImGuiItemStatusFlags_HoveredRect) != 0;
        const float rounding = 5 * ImGui::GetIO().FontGlobalScale;
        Gradient(draw, item.NavRect.Min, item.NavRect.Max, EditorVisuals::SurfaceColor(-.014f),
                 EditorVisuals::SurfaceColor(.012f), rounding);
        auto edge = active ? Accent(Tone::Primary) : ImGui::GetStyleColorVec4(ImGuiCol_Border);
        edge.w *= active ? .9f : hovered ? .8f : .55f;
        draw->AddRect(item.NavRect.Min, item.NavRect.Max, ImGui::GetColorU32(edge), rounding);
    }
    splitter.Merge(draw);
}

bool Search(const char *label, const char *hint, char *buffer, std::size_t capacity, float width) {
    if (ImGui::GetCurrentWindow()->SkipItems)
        return false;
    const auto id = ImGui::GetID(label);
    const float scale = ImGui::GetIO().FontGlobalScale, glyph = ImGui::GetFontSize();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {glyph + 9 * scale, 6 * scale});
    ImGui::SetNextItemWidth(width);
    ImGui::PushOverrideID(id);
    bool changed;
    {
        Field frame;
        ImGui::SetNextItemAllowOverlap();
        changed = ImGui::InputTextWithHint("", hint, buffer, capacity);
    }
    ImGui::PopID();
    ImGui::PopStyleVar();
    auto &g = *ImGui::GetCurrentContext();
    const auto field = g.LastItemData;
    if (!ImGui::IsItemVisible())
        return changed;
    const auto after = ImGui::GetCursorScreenPos();
    const auto lo = field.Rect.Min, hi = field.Rect.Max;
    EditorVisuals::DrawIcon(ImGui::GetWindowDrawList(), EditorVisuals::Icon::Search,
                            {lo.x + 8 * scale, lo.y + (hi.y - lo.y - glyph) * .5f}, glyph,
                            ImGui::GetColorU32(ImGuiCol_TextDisabled));
    if (buffer[0]) {
        ImGui::SetCursorScreenPos({hi.x - glyph - 9 * scale, lo.y + (hi.y - lo.y - glyph) * .5f});
        ImGui::PushOverrideID(id);
        const bool clear = ImGui::InvisibleButton("clear", {glyph, glyph}, ImGuiButtonFlags_EnableNav);
        ImGui::PopID();
        auto *draw = ImGui::GetWindowDrawList();
        const auto at = ImGui::GetItemRectMin();
        const auto color =
            ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        draw->AddLine({at.x + 4 * scale, at.y + 4 * scale},
                      {at.x + glyph - 4 * scale, at.y + glyph - 4 * scale}, color);
        draw->AddLine({at.x + glyph - 4 * scale, at.y + 4 * scale},
                      {at.x + 4 * scale, at.y + glyph - 4 * scale}, color);
        if (clear) {
            buffer[0] = 0;
            changed = true;
            if (auto *state = ImGui::GetInputTextState(id))
                state->ReloadUserBufAndMoveToEnd();
            ImGui::ActivateItemByID(id);
        }
        ImGui::SetCursorScreenPos(after);
        ImGui::Dummy({0, 0});
        g.LastItemData = field;
    }
    return changed;
}

Surface::Surface(Tone tone, ImVec2 padding, float minimumHeight)
    : draw(ImGui::GetWindowDrawList()), origin(ImGui::GetCursorScreenPos()),
      padding{padding.x * ImGui::GetIO().FontGlobalScale, padding.y * ImGui::GetIO().FontGlobalScale},
      window(ImGui::GetCurrentWindow()), width(ImGui::GetContentRegionAvail().x),
      minimumHeight(minimumHeight * ImGui::GetIO().FontGlobalScale),
      workRight(ImGui::GetCurrentWindow()->WorkRect.Max.x),
      contentRight(ImGui::GetCurrentWindow()->ContentRegionRect.Max.x), tone(tone) {
    if (ImGui::GetCurrentWindow()->SkipItems) {
        draw = nullptr;
        return;
    }
    splitter.Split(draw, 2);
    splitter.SetCurrentChannel(draw, 1);
    ImGui::BeginGroup();
    ImGui::Indent(this->padding.x);
    auto *owner = window;
    owner->WorkRect.Max.x -= this->padding.x;
    owner->ContentRegionRect.Max.x -= this->padding.x;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + this->padding.y);
    ImGui::PushItemWidth(std::max(1.f, width - 2 * this->padding.x));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::max(1.f, width - 2 * this->padding.x));
}
Surface::~Surface() {
    if (!draw)
        return;
    const float end = std::max(ImGui::GetCursorScreenPos().y + padding.y, origin.y + minimumHeight);
    ImGui::PopTextWrapPos();
    ImGui::PopItemWidth();
    ImGui::Unindent(padding.x);
    auto *owner = window;
    owner->WorkRect.Max.x = workRight;
    owner->ContentRegionRect.Max.x = contentRight;
    ImGui::SetCursorScreenPos({origin.x, end});
    ImGui::Dummy({width, 0});
    ImGui::EndGroup();
    const auto hi = ImGui::GetItemRectMax();
    splitter.SetCurrentChannel(draw, 0);
    if (ImRect(origin, {origin.x + width, hi.y}).Overlaps(owner->ClipRect)) {
        const float rounding = 8 * ImGui::GetIO().FontGlobalScale;
        draw->AddRectFilled({origin.x, origin.y + 3}, {origin.x + width, hi.y + 3},
                            ImGui::GetColorU32({0, 0, 0, .14f}), rounding);
        auto top = EditorVisuals::SurfaceColor(tone == Tone::Quiet ? .025f : .075f);
        auto bottom = EditorVisuals::SurfaceColor(tone == Tone::Quiet ? .015f : .045f);
        if (tone == Tone::Primary || tone == Tone::Danger)
            top = Blend(top, Accent(tone), .10f);
        Gradient(draw, origin, {origin.x + width, hi.y}, top, bottom, rounding);
        auto edge = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        edge.w *= .40f;
        draw->AddRect(origin, {origin.x + width, hi.y}, ImGui::GetColorU32(edge), rounding);
    }
    splitter.Merge(draw);
    ImGui::Dummy({0, 6 * ImGui::GetIO().FontGlobalScale});
}
} // namespace EditorDesign
