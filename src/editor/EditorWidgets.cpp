#include "EditorWidgets.h"

#include <cstring>

#include "imgui.h"

void TrackEdit(EditorDocument& doc) {
    if (ImGui::IsItemActivated()) doc.BeginEdit();
    if (ImGui::IsItemDeactivatedAfterEdit()) doc.CommitEdit();
    else if (ImGui::IsItemDeactivated()) doc.CancelEdit();
}

bool DragVec3(EditorDocument& doc, const char* label, glm::vec3& value, float speed) {
    const bool changed = ImGui::DragFloat3(label, &value.x, speed, 0.0f, 0.0f, "%.4g");
    TrackEdit(doc);
    return changed;
}

bool DragScalar(EditorDocument& doc, const char* label, float& value, float speed, float min, float max) {
    const bool changed = ImGui::DragFloat(label, &value, speed, min, max, "%.4g");
    TrackEdit(doc);
    return changed;
}

bool DragInt(EditorDocument& doc, const char* label, int& value, int min, int max) {
    const bool changed = ImGui::DragInt(label, &value, 1.0f, min, max);
    TrackEdit(doc);
    return changed;
}

bool ColorEdit(EditorDocument& doc, const char* label, glm::vec3& value) {
    const bool changed = ImGui::ColorEdit3(label, &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    TrackEdit(doc);
    return changed;
}

bool Checkbox(EditorDocument& doc, const char* label, bool& value) {
    bool v = value;
    if (ImGui::Checkbox(label, &v)) {
        doc.BeginEdit();
        value = v;
        doc.CommitEdit();
        return true;
    }
    return false;
}

bool TextField(EditorDocument& doc, const char* label, std::string& value) {
    char buffer[512];
    std::strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    const bool changed = ImGui::InputText(label, buffer, sizeof(buffer));
    if (ImGui::IsItemActivated()) doc.BeginEdit();
    if (changed) value = buffer;
    if (ImGui::IsItemDeactivatedAfterEdit()) doc.CommitEdit();
    else if (ImGui::IsItemDeactivated()) doc.CancelEdit();
    return changed;
}

bool StringCombo(EditorDocument& doc, const char* label, std::string& value, const std::vector<std::string>& options,
                 bool allowNone) {
    return LabelledCombo(doc, label, value, options, options, allowNone);
}

bool LabelledCombo(EditorDocument& doc, const char* label, std::string& value, const std::vector<std::string>& labels,
                   const std::vector<std::string>& values, bool allowNone, const char* noneLabel) {
    bool changed = false;
    std::string preview = value.empty() ? noneLabel : value;
    for (std::size_t i = 0; i < values.size() && i < labels.size(); ++i) {
        if (values[i] == value) preview = labels[i];
    }
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (allowNone && ImGui::Selectable(noneLabel, value.empty())) {
            doc.BeginEdit(); value.clear(); doc.CommitEdit(); changed = true;
        }
        for (std::size_t i = 0; i < values.size() && i < labels.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(labels[i].c_str(), values[i] == value)) {
                doc.BeginEdit(); value = values[i]; doc.CommitEdit(); changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool EnumCombo(const char* label, int& value, const char* const* names, int count) {
    return ImGui::Combo(label, &value, names, count);
}
