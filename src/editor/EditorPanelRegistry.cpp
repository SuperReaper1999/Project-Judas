#include "EditorPanelRegistry.h"
#include "EditorPanels.h"
#include "imgui.h"
#include <algorithm>
#include <stdexcept>

void EditorPanelRegistry::Add(EditorPanelDescriptor descriptor, Draw draw) {
    if (drawing)
        throw std::logic_error("Cannot register an editor panel while drawing panels");
    if (descriptor.id.empty() || descriptor.title.empty() || descriptor.category.empty() || !draw ||
        descriptor.phases == 0 || (descriptor.phases & ~kEditorAllPanelPhases))
        throw std::invalid_argument(
            "Editor panels require identity, title, category, phase and draw callback");
    if (Contains(descriptor.id))
        throw std::logic_error("Duplicate editor panel: " + descriptor.id);
    entries.push_back({std::move(descriptor), std::move(draw)});
}

bool EditorPanelRegistry::Contains(const std::string &id) const {
    return std::any_of(entries.begin(), entries.end(),
                       [&](const auto &entry) { return entry.descriptor.id == id; });
}

void EditorPanelRegistry::DrawAll(EditorContext &context) {
    if (drawing)
        throw std::logic_error("Editor panels cannot recursively draw the registry");
    struct Scope {
        bool &active;
        ~Scope() { active = false; }
    } scope{drawing};
    drawing = true;
    for (auto &entry : entries) {
        const auto &panel = entry.descriptor;
        if (!(panel.phases & EditorPhaseMask(context.phase)))
            continue;
        if (panel.visibility && !(context.panels.*panel.visibility))
            continue;
        entry.draw(context);
    }
}

void EditorPanelRegistry::DrawWindowMenu(EditorContext &context) const {
    // Categories follow registration order, so each module's menu position is
    // stable and the registry needs no knowledge of individual panel types.
    std::vector<std::string> categories;
    for (const auto &entry : entries)
        if (std::find(categories.begin(), categories.end(), entry.descriptor.category) ==
            categories.end())
            categories.push_back(entry.descriptor.category);
    for (const auto &category : categories) {
        if (!ImGui::BeginMenu(category.c_str()))
            continue;
        for (const auto &entry : entries) {
            const auto &panel = entry.descriptor;
            if (panel.category != category || !panel.visibility)
                continue;
            ImGui::MenuItem(panel.title.c_str(), nullptr, &(context.panels.*panel.visibility));
        }
        ImGui::EndMenu();
    }
}

std::vector<EditorPanelDescriptor> EditorPanelRegistry::Descriptors() const {
    std::vector<EditorPanelDescriptor> result;
    for (const auto &entry : entries)
        result.push_back(entry.descriptor);
    return result;
}
