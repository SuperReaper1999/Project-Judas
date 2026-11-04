#pragma once

#include "imgui.h"
#include <cstddef>

struct ImGuiWindow;

// Judas-owned editor presentation over ImGui's input, focus and layout engine.
// Controls retain native label IDs and disabled/navigation semantics. None of
// these helpers own project data, transactions, rendering resources or a VM.
namespace EditorDesign {
enum class Tone { Primary, Secondary, Quiet, Danger };

bool ActionButton(const char *label, Tone tone = Tone::Secondary, ImVec2 size = {},
                  float leadingIconSpace = 0);
bool Toggle(const char *label, bool *value);
bool Row(const char *label, bool selected, ImVec2 size = {}, bool allowDoubleClick = false,
         float leadingIconSpace = 0, bool spanColumns = false);
// A collapsing header, deliberately not a pushed tree/ID scope. explicitId lets
// a friendly display title retain its original component's canonical identity.
bool Disclosure(const char *label, bool defaultOpen = false, ImGuiID explicitId = 0,
                float leadingIconSpace = 0);
bool TreeRow(const char *label, bool selected, bool hasChildren, bool defaultOpen = false,
             float leadingIconSpace = 0);
bool Search(const char *label, const char *hint, char *buffer, std::size_t capacity, float width = -1);

// Custom inset paint around a real Input/Drag/Combo. Does not add an item or ID
// scope; TrackEdit sees the original native item before and after destruction.
class Field {
  public:
    Field();
    ~Field();
    Field(const Field &) = delete;
    Field &operator=(const Field &) = delete;

  private:
    ImDrawList *draw = nullptr;
    ImDrawListSplitter splitter;
    ImGuiWindow *window = nullptr;
};
using FieldFrame = Field;

// A padded visual group, not a child window or ID scope. Independent splitters
// allow fields/cards to nest without taking over their parent's draw channels.
class Surface {
  public:
    explicit Surface(Tone tone = Tone::Secondary, ImVec2 padding = {12, 10}, float minimumHeight = 0);
    ~Surface();
    Surface(const Surface &) = delete;
    Surface &operator=(const Surface &) = delete;

  private:
    ImDrawList *draw;
    ImDrawListSplitter splitter;
    ImVec2 origin, padding;
    ImGuiWindow *window;
    float width, minimumHeight, workRight, contentRight;
    Tone tone;
};
} // namespace EditorDesign
