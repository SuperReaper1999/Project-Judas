#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <set>
#include <sstream>

using namespace EditorPanelWidgets;
#include "AuthoringDocument.h"
#include "NamedAuthoring.h"
#include "RuntimeUI.h"

// Per-editor authoring draft: no function-static document shared between sessions.
struct EditorUIAssetState {
    AuthoringDocument document;
    UIDocument source;
    std::string error;
    int selected = 0;
    bool loaded = false, pending = false;
    std::vector<char> text = std::vector<char>(4 * 1024 * 1024 + 1);
    uint64_t textGeneration = ~uint64_t(0);
    bool sourceDirty = false;
    std::set<std::string> selection;
    UIElement clipboard;
    bool hasClipboard = false;
};

void DrawUIAssetDocument(const AssetRecord &record, EditorPanelState &state) {
    if (!state.uiAuthoring)
        state.uiAuthoring = std::make_shared<EditorUIAssetState>();
    auto &ui = *state.uiAuthoring;
    auto &document = ui.document;
    auto &source = ui.source;
    auto &error = ui.error;
    auto &selected = ui.selected;
    auto &loaded = ui.loaded;
    auto &pending = ui.pending;
    auto &text = ui.text;
    auto &textGeneration = ui.textGeneration;
    auto &sourceDirty = ui.sourceDirty;
    auto &selection = ui.selection;
    auto &clipboard = ui.clipboard;
    auto &hasClipboard = ui.hasClipboard;
    auto reloadTyped = [&] {
        sourceDirty = false;
        loaded = ParseUIDocument(document.Source(), source, error);
        selected = 0;
        pending = false;
        selection.clear();
        state.uiPreviewDocument = source;
        ++state.uiPreviewRevision;
        textGeneration = ~uint64_t(0);
    };
    if (document.Path() != record.path) {
        if (loaded && (document.Dirty() || pending || sourceDirty)) {
            ImGui::TextWrapped("Unsaved UI %s. Save or discard before opening another document.",
                               document.Path().c_str());
            if (ImGui::Button("Discard previous UI changes")) {
                loaded = false;
                pending = false;
            }
            return;
        }
        loaded = document.Load("ui", record.path, error);
        if (loaded)
            reloadTyped();
    }
    if (!loaded) {
        ImGui::TextWrapped("UI: %s", error.c_str());
        return;
    }
    auto flush = [&] {
        if (!pending)
            return true;
        std::string named;
        if (!LegacyToNamed(SerializeUIDocument(source), "ui", named, error) ||
            !document.Replace(named, error))
            return false;
        pending = false;
        return true;
    };
    if (!ImGui::BeginTable("UI authoring columns", 2, ImGuiTableFlags_Resizable))
        return;
    ImGui::TableSetupColumn("Properties / source", ImGuiTableColumnFlags_WidthFixed, 440);
    ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextColumn();
    ImGui::BeginChild("UI properties and source", {0, 0}, true);
    ImGui::Text("%s%s", record.relativePath.c_str(), document.Dirty() || pending ? " *" : "");
    if (document.ExternalChanged())
        ImGui::TextColored({1, .6f, .2f, 1},
                           "External edit detected; saving is blocked until reviewed reload.");
    if (ImGui::Button("Save UI source")) {
        if (sourceDirty)
            state.status = "Apply or discard the named source draft before saving";
        else if (flush() && document.Save(error))
            state.status = "Saved named UI source; restart Play to reload";
        else
            state.status = error;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload (discard dirty UI)")) {
        if (document.Load("ui", record.path, error))
            reloadTyped();
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo UI")) {
        if (sourceDirty)
            state.status = "Apply or explicitly discard the named source draft before visual undo";
        else if (flush() && document.Undo())
            reloadTyped();
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo UI")) {
        if (sourceDirty)
            state.status = "Apply or explicitly discard the named source draft before visual redo";
        else if (document.Redo())
            reloadTyped();
    }
    if (selected < 0 || selected >= int(source.elements.size()))
        selected = 0;
    if (!state.uiCanvasSelected.empty()) {
        for (size_t i = 0; i < source.elements.size(); ++i)
            if (source.elements[i].id == state.uiCanvasSelected)
                selected = int(i);
        state.uiCanvasSelected.clear();
    }
    auto before = SerializeUIDocument(source);
    ImGui::Checkbox("Document visible", &source.visible);
    ImGui::SameLine();
    ImGui::Checkbox("Document enabled", &source.enabled);
    ImGui::Checkbox("Modal (pause gameplay)", &source.modal);
    ImGui::BeginChild("UI hierarchy", {0, 130}, true);
    for (size_t i = 0; i < source.elements.size(); ++i) {
        ImGui::PushID(int(i));
        auto &e = source.elements[i];
        if (ImGui::Selectable((e.parent.empty() ? e.id : e.parent + " / " + e.id).c_str(),
                              selected == int(i) || selection.count(e.id))) {
            selected = int(i);
            if (!ImGui::GetIO().KeyCtrl)
                selection.clear();
            if (selection.count(e.id))
                selection.erase(e.id);
            else
                selection.insert(e.id);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (ImGui::Button("Add child panel")) {
        UIElement e;
        e.id = "element_" + std::to_string(source.elements.size() + 1);
        while (std::any_of(source.elements.begin(), source.elements.end(),
                           [&](const auto &x) { return x.id == e.id; }))
            e.id += "_";
        e.parent = source.elements[selected].id;
        source.elements.push_back(e);
        selected = int(source.elements.size()) - 1;
    }
    ImGui::SameLine();
    if (selected && ImGui::Button("Duplicate subtree")) {
        auto root = source.elements[selected].id;
        std::set<std::string> members{root};
        std::map<std::string, std::string> ids;
        bool more = true;
        while (more) {
            more = false;
            for (auto &e : source.elements)
                if (members.count(e.parent))
                    more |= members.insert(e.id).second;
        }
        for (auto &key : members) {
            auto copy = key + "_copy";
            while (std::any_of(source.elements.begin(), source.elements.end(),
                               [&](const auto &e) { return e.id == copy; }))
                copy += "_";
            ids[key] = copy;
        }
        std::vector<UIElement> copies;
        for (auto e : source.elements)
            if (members.count(e.id)) {
                auto id = e.id;
                e.id = ids.at(id);
                if (ids.count(e.parent))
                    e.parent = ids.at(e.parent);
                if (id == root)
                    e.offset += glm::vec2(12);
                copies.push_back(e);
            }
        source.elements.insert(source.elements.end(), copies.begin(), copies.end());
    }
    auto &e = source.elements[selected];
    auto str = [](const char *label, std::string &s) {
        char b[16385];
        std::snprintf(b, sizeof(b), "%s", s.c_str());
        if (ImGui::InputText(label, b, sizeof(b)))
            s = b;
    };
    ImGui::TextDisabled("Stable ID: %s", e.id.c_str());
    if (selected) {
        if (ImGui::BeginCombo("Parent", e.parent.c_str())) {
            for (auto &parent : source.elements)
                if (parent.id != e.id && ImGui::Selectable(parent.id.c_str(), e.parent == parent.id))
                    e.parent = parent.id;
            ImGui::EndCombo();
        }
    }
    int kind = int(e.kind);
    if (selected && ImGui::Combo("Element type", &kind,
                                 "Canvas\0Panel\0Text\0Image\0Button\0Slider\0Toggle\0Text "
                                 "input\0Scroll container\0Virtual list\0"))
        e.kind = UIKind(kind);
    int flow = int(e.flow);
    if (ImGui::Combo("Child layout", &flow, "Free\0Horizontal\0Vertical\0"))
        e.flow = UIFlow(flow);
    for (auto &parent : source.elements)
        if (parent.id == e.parent && parent.flow != UIFlow::Free)
            ImGui::TextWrapped("%s owns this element's %s placement; offsets and margins still contribute.",
                               parent.id.c_str(),
                               parent.flow == UIFlow::Horizontal ? "horizontal" : "vertical");
    if (auto it = state.uiPreviewLayout.find(e.id); it != state.uiPreviewLayout.end())
        ImGui::TextDisabled("Effective rectangle %.1f %.1f / %.1f %.1f px", it->second.rect.position.x,
                            it->second.rect.position.y, it->second.rect.size.x, it->second.rect.size.y);
    ImGui::Checkbox("Visible", &e.visible);
    ImGui::SameLine();
    ImGui::Checkbox("Enabled", &e.enabled);
    ImGui::Checkbox("Clip children", &e.clip);
    ImGui::InputFloat2("Anchor min", &e.anchorMin.x);
    ImGui::InputFloat2("Anchor max", &e.anchorMax.x);
    ImGui::InputFloat2("Offset (reference px)", &e.offset.x);
    ImGui::InputFloat2("Fixed size", &e.size.x);
    ImGui::InputFloat2("Relative size", &e.relativeSize.x);
    ImGui::InputFloat2("Pivot alignment", &e.align.x);
    ImGui::InputFloat4("Margins L T R B", &e.margin.x);
    ImGui::InputFloat4("Padding L T R B", &e.padding.x);
    ImGui::InputFloat("Spacing", &e.spacing);
    ImGui::ColorEdit4("Background", &e.background.x);
    ImGui::ColorEdit4("Tint", &e.color.x);
    str("Text", e.text);
    str("Localization key", e.textKey);
    int direction = int(e.direction);
    if (ImGui::Combo("Text direction", &direction, "Auto\0LTR\0RTL\0"))
        e.direction = TextDirection(direction);
    int alignment = e.textLogicalAlign + 1;
    if (ImGui::Combo("Text alignment", &alignment, "Legacy\0Left\0Right\0Center\0Start\0End\0"))
        e.textLogicalAlign = alignment - 1;
    ImGui::Checkbox("Mirror horizontal children for RTL", &e.mirrorRow);
    auto assetPick = [&](const char *label, AssetType type, std::string &id) {
        if (!state.assets)
            return;
        if (ImGui::BeginCombo(label, id.empty() ? "(default / none)" : id.c_str())) {
            if (ImGui::Selectable("(default / none)", id.empty()))
                id.clear();
            for (auto &[key, asset] : state.assets->Records())
                if (asset.type == type && !asset.missing &&
                    ImGui::Selectable(asset.relativePath.c_str(), id == key))
                    id = key;
            ImGui::EndCombo();
        }
    };
    assetPick("Font", AssetType::Font, e.font);
    assetPick("Image", AssetType::Texture, e.texture);
    ImGui::InputFloat("Font size", &e.fontSize);
    ImGui::InputFloat2("Text alignment legacy", &e.textAlign.x);
    ImGui::Checkbox("Wrap", &e.wrap);
    ImGui::Checkbox("Fit image", &e.fit);
    if (ImGui::Button("Copy style")) {
        clipboard = e;
        hasClipboard = true;
    }
    ImGui::SameLine();
    if (hasClipboard && ImGui::Button("Paste style to selected")) {
        if (selection.empty())
            selection.insert(e.id);
        for (auto &target : source.elements)
            if (selection.count(target.id)) {
                target.background = clipboard.background;
                target.color = clipboard.color;
                target.font = clipboard.font;
                target.fontSize = clipboard.fontSize;
                target.textLogicalAlign = clipboard.textLogicalAlign;
            }
    }
    str("Accessible label", e.accessibleLabel);
    str("Accessible description", e.description);
    ImGui::ColorEdit4("Focus indicator", &e.focusColor.x);
    if (ImGui::CollapsingHeader("Drag and drop")) {
        str("Drag category", e.dragType);
        str("Accept category", e.dropType);
        ImGui::TextWrapped("Matching nonempty categories permit transfer requests. JavaScript owns item "
                           "validation and mutation. Pointer drag or focus + Activate; Back cancels.");
    }
    if (e.kind == UIKind::Image)
        ImGui::TextWrapped("3D preview: bind an authored render-camera entity with "
                           "element.setPreviewCamera(camera). Disable its camera when the menu closes.");
    if (e.kind == UIKind::TextInput) {
        str("Placeholder", e.placeholder);
        ImGui::Checkbox("Read only", &e.readOnly);
        int maxBytes = int(e.maxBytes);
        if (ImGui::InputInt("Maximum UTF-8 bytes", &maxBytes))
            e.maxBytes = unsigned(std::clamp(maxBytes, 1, 16384));
        ImGui::TextWrapped("Single-line UTF-8; runtime selection uses extended grapheme indices. IME "
                           "composition is transient.");
    }
    if (e.kind == UIKind::List || e.kind == UIKind::Scroll) {
        ImGui::InputFloat("Row / scroll step", &e.rowHeight);
        ImGui::TextWrapped("Children of a Scroll container clip automatically. Virtual List rows come "
                           "from JS setItems; only visible rows are shaped/drawn.");
    }
    ImGui::InputFloat("Value", &e.value);
    ImGui::InputFloat("Minimum", &e.minimum);
    ImGui::InputFloat("Maximum", &e.maximum);
    if (selected && ImGui::Button("Delete element subtree")) {
        std::set<std::string> remove{e.id};
        bool more = true;
        while (more) {
            more = false;
            for (const auto &x : source.elements)
                if (remove.count(x.parent))
                    more |= remove.insert(x.id).second;
        }
        source.elements.erase(std::remove_if(source.elements.begin(), source.elements.end(),
                                             [&](const auto &x) { return remove.count(x.id); }),
                              source.elements.end());
        selected = 0;
    }
    auto after = SerializeUIDocument(source);
    if (after != before)
        pending = true;
    if (!ImGui::IsAnyItemActive())
        flush();
    if (!source.Validate(error)) {
        ImGui::TextWrapped("Last-good source retained: %s", error.c_str());
        if (ImGui::Button("Discard invalid visual draft"))
            reloadTyped();
    } else if (state.uiPreviewRevision == 0 || after != before) {
        state.uiPreviewDocument = source;
        ++state.uiPreviewRevision;
    }
    if (ImGui::CollapsingHeader("Named UI source")) {
        if (textGeneration != document.Generation() && !sourceDirty) {
            CopyToBuffer(document.Source(), text.data(), text.size());
            textGeneration = document.Generation();
        }
        if (ImGui::InputTextMultiline("##ui-source", text.data(), text.size(), {0, 250}))
            sourceDirty = true;
        if (ImGui::Button("Discard named draft")) {
            sourceDirty = false;
            textGeneration = ~uint64_t(0);
        }
        if (ImGui::Button("Validate / apply UI source")) {
            if (flush() && document.Replace(text.data(), error))
                reloadTyped();
            else
                state.status = error;
        }
    }
    ImGui::EndChild();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Canvas - normal runtime layout / shaping");
    ImGui::Checkbox("Runtime canvas preview", &state.uiCanvasPreview);
    ImGui::InputInt2("Preview resolution", &state.uiPreviewResolution.x);
    state.uiPreviewResolution =
        glm::clamp(state.uiPreviewResolution, glm::ivec2(160, 90), glm::ivec2(2560, 1440));
    if (state.project) {
        auto &c = state.project->Settings().localization;
        if (state.previewLocale.empty())
            state.previewLocale = c.defaultLocale;
        if (ImGui::BeginCombo("Preview locale", state.previewLocale.c_str())) {
            for (auto &[tag, entry] : c.locales) {
                (void)entry;
                if (ImGui::Selectable(tag.c_str(), tag == state.previewLocale))
                    state.previewLocale = tag;
            }
            ImGui::EndCombo();
        }
    }
    if (state.uiCanvasPreview && state.uiPreviewToken) {
        float width = std::max(160.f, ImGui::GetContentRegionAvail().x),
              height = width * state.uiPreviewResolution.y / state.uiPreviewResolution.x;
        auto origin = ImGui::GetCursorScreenPos();
        ImGui::Image(ImTextureID(state.uiPreviewToken), {width, height}, {0, 1}, {1, 0});
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
            auto mouse = ImGui::GetMousePos();
            state.uiCanvasPick = {(mouse.x - origin.x) / width * state.uiPreviewResolution.x,
                                  (mouse.y - origin.y) / height * state.uiPreviewResolution.y};
        }
    }
    ImGui::EndTable();
}
