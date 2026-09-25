#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "EditorDocument.h"

// Milestone 30: the small set of Dear ImGui widget wrappers every editor
// panel and component editor uses. Each wrapper records exactly one undo
// step through EditorDocument::BeginEdit/CommitEdit: drag/input widgets
// snapshot on activation and commit when they deactivate having changed
// something; instant widgets (checkbox, combo, selectable) commit at once.
void TrackEdit(EditorDocument& doc);
bool DragVec3(EditorDocument& doc, const char* label, glm::vec3& value, float speed = 0.05f);
bool DragScalar(EditorDocument& doc, const char* label, float& value, float speed = 0.01f, float min = 0.0f,
                float max = 0.0f);
bool DragInt(EditorDocument& doc, const char* label, int& value, int min = 0, int max = 100000);
bool ColorEdit(EditorDocument& doc, const char* label, glm::vec3& value);
bool Checkbox(EditorDocument& doc, const char* label, bool& value);
bool TextField(EditorDocument& doc, const char* label, std::string& value);
// A combo over string values; `allowNone` offers an empty selection.
bool StringCombo(EditorDocument& doc, const char* label, std::string& value, const std::vector<std::string>& options,
                 bool allowNone);
// A combo whose displayed labels differ from the stored values (asset
// name shown, asset id stored). `labels` and `values` are parallel.
bool LabelledCombo(EditorDocument& doc, const char* label, std::string& value, const std::vector<std::string>& labels,
                   const std::vector<std::string>& values, bool allowNone, const char* noneLabel = "(none)");

// Raw ImGui combo over an int; returns true when the user picked a value.
bool EnumCombo(const char* label, int& value, const char* const* names, int count);

template <typename Enum>
bool Combo(EditorDocument& doc, const char* label, Enum& value, const char* const* names, int count) {
    int index = static_cast<int>(value);
    if (!EnumCombo(label, index, names, count)) return false;
    doc.BeginEdit();
    value = static_cast<Enum>(index);
    doc.CommitEdit();
    return true;
}
