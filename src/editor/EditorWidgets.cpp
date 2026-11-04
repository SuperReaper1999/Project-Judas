#include "EditorWidgets.h"
#include "EditorDesign.h"

#include <cstring>

#include "imgui.h"
#include "imgui_internal.h"

namespace {
// Give the value the inspector's whole row. The hidden widget keeps the
// original label's ID, including component IDs inside vector/color widgets.
class StackedField {
  public:
    explicit StackedField(const char *label) {
        const auto id = ImGui::GetID(label);
        const char *end = ImGui::FindRenderedTextEnd(label);
        if (end != label) {
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(label, end);
            ImGui::PopTextWrapPos();
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::PushOverrideID(id);
    }
    ~StackedField() {
        ImGui::PopID();
    }
};
thread_local InspectorReadOnlyScope *inspectorReadOnly = nullptr;
} // namespace

InspectorReadOnlyScope::InspectorReadOnlyScope(bool readOnly) : enabled(readOnly) {
    if (enabled) {
        previous = inspectorReadOnly;
        inspectorReadOnly = this;
        ImGui::BeginDisabled();
    }
}
InspectorReadOnlyScope::~InspectorReadOnlyScope() {
    if (enabled) {
        IM_ASSERT(inspectorReadOnly == this);
        inspectorReadOnly = previous;
        ImGui::EndDisabled();
    }
}
bool InspectorDisclosure(const char *label, bool defaultOpen, ImGuiID explicitId, float leadingIconSpace) {
    auto *owner = inspectorReadOnly;
    if (!owner)
        return EditorDesign::Disclosure(label, defaultOpen, explicitId, leadingIconSpace);
    // Lift this Inspector-owned value lock only while submitting the navigation
    // item. Any separately owned outer disabled scope remains effective.
    ImGui::EndDisabled();
    inspectorReadOnly = owner->previous;
    const bool open = EditorDesign::Disclosure(label, defaultOpen, explicitId, leadingIconSpace);
    ImGui::BeginDisabled();
    inspectorReadOnly = owner;
    return open;
}

void TrackEdit(EditorDocument &doc) {
    if (ImGui::IsItemActivated())
        doc.BeginEdit();
    if (ImGui::IsItemDeactivatedAfterEdit())
        doc.CommitEdit();
    else if (ImGui::IsItemDeactivated())
        doc.CancelEdit();
}

bool DragVec2(EditorDocument &doc, const char *label, glm::vec2 &value, float speed, float min, float max) {
    const StackedField field(label);
    auto candidate = value;
    EditorDesign::FieldFrame frame;
    const bool changed =
        ImGui::DragFloat2("", &candidate.x, speed, min, max, "%.4g", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemActivated())
        doc.BeginEdit();
    if (changed)
        value = candidate;
    TrackEdit(doc);
    return changed;
}

bool DragVec3(EditorDocument &doc, const char *label, glm::vec3 &value, float speed) {
    const StackedField field(label);
    auto candidate = value;
    EditorDesign::FieldFrame frame;
    const bool changed = ImGui::DragFloat3("", &candidate.x, speed, 0.0f, 0.0f, "%.4g");
    if (ImGui::IsItemActivated())
        doc.BeginEdit();
    if (changed)
        value = candidate;
    TrackEdit(doc);
    return changed;
}

bool DragRotation(EditorDocument &doc, const char *label, glm::quat &value) {
    const StackedField field(label);
    auto candidate = glm::degrees(glm::eulerAngles(glm::normalize(value)));
    EditorDesign::FieldFrame frame;
    const bool changed = ImGui::DragFloat3("", &candidate.x, 0.5f, 0.0f, 0.0f, "%.3g");
    // Keep the authored quaternion, including its sign, in the undo snapshot
    // before converting the first changed Euler value back to a quaternion.
    if (ImGui::IsItemActivated())
        doc.BeginEdit();
    if (changed)
        value = glm::normalize(glm::quat(glm::radians(candidate)));
    TrackEdit(doc);
    return changed;
}

bool DragScalar(EditorDocument &doc, const char *label, float &value, float speed, float min, float max) {
    const StackedField field(label);
    auto candidate = value;
    EditorDesign::FieldFrame frame;
    const bool changed = ImGui::DragFloat("", &candidate, speed, min, max, "%.4g");
    if (ImGui::IsItemActivated())
        doc.BeginEdit();
    if (changed)
        value = candidate;
    TrackEdit(doc);
    return changed;
}

bool DragInt(EditorDocument &doc, const char *label, int &value, int min, int max) {
    const StackedField field(label);
    auto candidate = value;
    EditorDesign::FieldFrame frame;
    const bool changed = ImGui::DragInt("", &candidate, 1.0f, min, max);
    if (ImGui::IsItemActivated())
        doc.BeginEdit();
    if (changed)
        value = candidate;
    TrackEdit(doc);
    return changed;
}

bool ColorEdit(EditorDocument &doc, const char *label, glm::vec3 &value) {
    const StackedField field(label);
    auto candidate = value;
    EditorDesign::FieldFrame frame;
    const bool changed =
        ImGui::ColorEdit3("", &candidate.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    if (ImGui::IsItemActivated())
        doc.BeginEdit();
    if (changed)
        value = candidate;
    TrackEdit(doc);
    return changed;
}

bool Checkbox(EditorDocument &doc, const char *label, bool &value) {
    bool v = value;
    if (EditorDesign::Toggle(label, &v)) {
        doc.BeginEdit();
        value = v;
        doc.CommitEdit();
        return true;
    }
    return false;
}

bool TextField(EditorDocument &doc, const char *label, std::string &value) {
    const StackedField field(label);
    char buffer[512];
    std::strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    EditorDesign::FieldFrame frame;
    const bool changed = ImGui::InputText("", buffer, sizeof(buffer));
    if (ImGui::IsItemActivated())
        doc.BeginEdit();
    if (changed)
        value = buffer;
    if (ImGui::IsItemDeactivatedAfterEdit())
        doc.CommitEdit();
    else if (ImGui::IsItemDeactivated())
        doc.CancelEdit();
    return changed;
}

bool StringCombo(EditorDocument &doc, const char *label, std::string &value,
                 const std::vector<std::string> &options, bool allowNone) {
    return LabelledCombo(doc, label, value, options, options, allowNone);
}

bool LabelledCombo(EditorDocument &doc, const char *label, std::string &value,
                   const std::vector<std::string> &labels, const std::vector<std::string> &values,
                   bool allowNone, const char *noneLabel) {
    const StackedField field(label);
    bool changed = false;
    std::string preview = value.empty() ? noneLabel : value;
    for (std::size_t i = 0; i < values.size() && i < labels.size(); ++i) {
        if (values[i] == value)
            preview = labels[i];
    }
    bool open;
    {
        EditorDesign::FieldFrame frame;
        open = ImGui::BeginCombo("", preview.c_str());
    }
    if (open) {
        if (allowNone) {
            const bool selected = value.empty();
            if (EditorDesign::Row(noneLabel, selected)) {
                doc.BeginEdit();
                value.clear();
                doc.CommitEdit();
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        for (std::size_t i = 0; i < values.size() && i < labels.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool selected = values[i] == value;
            if (EditorDesign::Row(labels[i].c_str(), selected)) {
                doc.BeginEdit();
                value = values[i];
                doc.CommitEdit();
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
            if (labels[i] != values[i])
                ImGui::SetItemTooltip("%s\nStored value: %s", labels[i].c_str(), values[i].c_str());
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (!value.empty() && *label && preview != value)
        ImGui::SetItemTooltip("%s\nStored value: %s", preview.c_str(), value.c_str());
    return changed;
}

bool EnumCombo(const char *label, int &value, const char *const *names, int count) {
    const StackedField field(label);
    const char *preview = value >= 0 && value < count ? names[value] : nullptr;
    bool open;
    {
        EditorDesign::FieldFrame frame;
        open = ImGui::BeginCombo("", preview);
    }
    bool changed = false;
    if (open) {
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            const bool selected = value == i;
            if (EditorDesign::Row(names[i], selected)) {
                value = i;
                changed = true;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}
