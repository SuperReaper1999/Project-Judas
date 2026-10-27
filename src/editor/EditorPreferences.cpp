#include "EditorPreferences.h"
#include "EditorPanels.h"
#include "EditorTheme.h"
#include "PlatformServices.h"
#include "imgui.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
constexpr size_t maxLayouts = 32, maxWindows = 1024 * 1024, maxFile = 4 * 1024 * 1024;
EditorAppearance ThemeDefaults(bool light) {
    EditorAppearance result; result.light = light;
    if (light) {
        result.background = {.94f, .95f, .97f};
        result.text = {.10f, .13f, .17f};
        result.accent = {.16f, .38f, .64f};
    }
    return result;
}
struct Panel { const char* name; const char* label; bool EditorPanelState::*flag; };
constexpr Panel panels[] = {
    {"assets", "Asset browser", &EditorPanelState::showAssetBrowser}, {"scripts", "JudasJS tools", &EditorPanelState::showScriptTools},
    {"runtimeInspector", "Runtime Inspector", &EditorPanelState::showRuntimeInspector},
    {"profiler", "Profiler", &EditorPanelState::showProfiler}, {"scene", "Scene settings", &EditorPanelState::showSceneSettings},
    {"project", "Project settings", &EditorPanelState::showProjectSettings}, {"terrain", "Terrain tools", &EditorPanelState::showTerrain},
    {"volume", "Volumetric terrain", &EditorPanelState::showVolumeTerrain}, {"animation", "Animation timeline", &EditorPanelState::showAnimationTimeline},
    {"retarget", "Animation retargeting", &EditorPanelState::showRetarget}, {"world", "World building", &EditorPanelState::showWorldBuilding},
    {"mods", "Local mods", &EditorPanelState::showMods}, {"help", "Navigation help", &EditorPanelState::showNavigationHelp}
};
float Number(const Json& j, const char* key, float fallback, float low, float high) {
    if (!j.contains(key)) return fallback;
    if (!j[key].is_number()) throw std::runtime_error(std::string("Invalid preference: ") + key);
    const auto v = j[key].get<double>();
    if (!std::isfinite(v) || v < low || v > high) throw std::runtime_error(std::string("Out-of-range preference: ") + key);
    return static_cast<float>(v);
}
std::array<float, 3> Color(const Json& j, const char* key, std::array<float, 3> fallback) {
    if (!j.contains(key)) return fallback;
    const auto& a = j[key];
    if (!a.is_array() || a.size() != 3) throw std::runtime_error("Invalid theme colour");
    for (size_t i = 0; i < 3; ++i) {
        if (!a[i].is_number()) throw std::runtime_error("Invalid theme colour");
        const auto v = a[i].get<double>();
        if (!std::isfinite(v) || v < 0 || v > 1) throw std::runtime_error("Theme colour must be between zero and one");
        fallback[i] = static_cast<float>(v);
    }
    return fallback;
}
EditorLayoutPreset ReadLayout(const Json& j) {
    if (!j.is_object()) throw std::runtime_error("Invalid layout");
    EditorLayoutPreset l;
    l.name = j.value("name", std::string{});
    if (l.name.empty() || l.name.find_first_not_of(" \t") == std::string::npos || l.name.size() > 128 || l.name.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error("Layout names need 1–128 characters, without line breaks");
    l.hierarchy = Number(j, "hierarchyWidth", 250, 150, 800);
    l.inspector = Number(j, "inspectorWidth", 340, 220, 900);
    l.assets = Number(j, "assetBrowserHeight", 200, 80, 700);
    l.windows = j.value("windows", std::string{});
    if (l.windows.size() > maxWindows || l.windows.find('\0') != std::string::npos) throw std::runtime_error("Layout window state is too large or invalid");
    l.panels = j.value("panels", std::vector<std::string>{});
    if (l.panels.size() > std::size(panels)) throw std::runtime_error("Invalid layout panel list");
    for (const auto& name : l.panels)
        if (std::none_of(std::begin(panels), std::end(panels), [&](auto p) { return name == p.name; }))
            throw std::runtime_error("Unknown layout panel: " + name);
    return l;
}
Json LayoutJson(const EditorLayoutPreset& l) {
    return {{"name", l.name}, {"hierarchyWidth", l.hierarchy}, {"inspectorWidth", l.inspector},
            {"assetBrowserHeight", l.assets}, {"panels", l.panels}, {"windows", l.windows}};
}
void ApplyLayout(const EditorLayoutPreset& l, EditorPanelState& state) {
    state.hierarchyWidth = l.hierarchy; state.inspectorWidth = l.inspector; state.assetBrowserHeight = l.assets;
    for (auto p : panels) state.*(p.flag) = std::find(l.panels.begin(), l.panels.end(), p.name) != l.panels.end();
}
void ApplyAppearance(const EditorAppearance& a) {
    ImGui::GetStyle() = ImGuiStyle{};
    ApplyEditorTheme();
    if (a.light) ImGui::StyleColorsLight();
    auto& s = ImGui::GetStyle();
    auto& c = s.Colors;
    const auto tint = [&](float amount, float alpha = 1.f) {
        return ImVec4(a.background[0] * (1 - amount) + a.accent[0] * amount,
                      a.background[1] * (1 - amount) + a.accent[1] * amount,
                      a.background[2] * (1 - amount) + a.accent[2] * amount, alpha);
    };
    c[ImGuiCol_Text] = {a.text[0], a.text[1], a.text[2], 1};
    c[ImGuiCol_TextDisabled] = {a.text[0], a.text[1], a.text[2], .58f};
    c[ImGuiCol_WindowBg] = c[ImGuiCol_ChildBg] = c[ImGuiCol_PopupBg] = tint(0);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_MenuBarBg] = tint(.06f);
    c[ImGuiCol_TitleBgActive] = tint(.17f);
    c[ImGuiCol_FrameBg] = tint(.10f); c[ImGuiCol_FrameBgHovered] = tint(.22f); c[ImGuiCol_FrameBgActive] = tint(.30f);
    c[ImGuiCol_Button] = c[ImGuiCol_Header] = c[ImGuiCol_Tab] = tint(.18f);
    c[ImGuiCol_ButtonHovered] = c[ImGuiCol_HeaderHovered] = c[ImGuiCol_TabHovered] = tint(.36f);
    c[ImGuiCol_ButtonActive] = c[ImGuiCol_HeaderActive] = c[ImGuiCol_TabSelected] = tint(.48f);
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrabActive] = c[ImGuiCol_NavCursor] = tint(1);
    c[ImGuiCol_SliderGrab] = tint(.8f);
    c[ImGuiCol_SeparatorHovered] = c[ImGuiCol_SeparatorActive] = tint(.85f);
    c[ImGuiCol_TextSelectedBg] = tint(.7f, .45f);
    s.ScaleAllSizes(a.scale);
    ImGui::GetIO().FontGlobalScale = a.scale;
}
} // namespace

EditorLayoutPreset EditorPreferences::Capture(const std::string& name, const EditorPanelState& state) const {
    EditorLayoutPreset l; l.name = name; l.hierarchy = state.hierarchyWidth;
    l.inspector = state.inspectorWidth; l.assets = state.assetBrowserHeight;
    for (auto p : panels) if (state.*(p.flag)) l.panels.emplace_back(p.name);
    size_t bytes = 0; const char* ini = ImGui::SaveIniSettingsToMemory(&bytes);
    if (bytes > maxWindows) throw std::runtime_error("Too many tool-window settings to save this layout");
    l.windows.assign(ini, bytes);
    return l;
}

bool EditorPreferences::Load(const std::string& text, EditorPanelState& state, std::string& error) {
    error.clear();
    try {
        if (text.size() > maxFile) throw std::runtime_error("Editor preferences exceed 4 MiB");
        auto j = Json::parse(text);
        if (!j.is_object()) throw std::runtime_error("Invalid editor preferences");
        const auto version = j.value("version", 1);
        if (version != 1 && version != 2) throw std::runtime_error("Unsupported editor preferences version");
        EditorPreferences next;
        EditorLayoutPreset current;
        if (version == 1) {
            j["name"] = "Current";
            current = ReadLayout(j);
            if (j.value("showAssets", true)) current.panels.emplace_back("assets");
        } else {
            current = ReadLayout(j.at("current"));
            const auto& a = j.at("appearance");
            const auto mode = a.value("mode", std::string("dark"));
            if (mode != "dark" && mode != "light") throw std::runtime_error("Unknown editor theme");
            next.appearance = ThemeDefaults(mode == "light");
            next.appearance.accent = Color(a, "accent", next.appearance.accent);
            next.appearance.background = Color(a, "background", next.appearance.background);
            next.appearance.text = Color(a, "text", next.appearance.text);
            next.appearance.scale = Number(a, "scale", 1, .85f, 1.5f);
            const auto& list = j.at("layouts");
            if (!list.is_array() || list.size() > maxLayouts) throw std::runtime_error("Too many saved layouts");
            for (const auto& entry : list) {
                auto l = ReadLayout(entry);
                if (std::any_of(next.layouts.begin(), next.layouts.end(), [&](const auto& existing) { return l.name == existing.name; }))
                    throw std::runtime_error("Duplicate saved layout name");
                next.layouts.push_back(std::move(l));
            }
        }
        if (j.contains("updates")) {
            const auto& updates = j.at("updates");
            next.automaticUpdates = updates.at("automaticAtStartup").get<bool>();
            next.updateChannel = updates.at("channel").get<std::string>();
            if (!next.updateChannel.empty() && next.updateChannel != "Alpha" && next.updateChannel != "Beta" &&
                next.updateChannel != "Stable" && next.updateChannel != "Experimental")
                throw std::runtime_error("Unknown update channel");
        }
        ApplyLayout(current, state);
        // Current windows.ini remains the normal window store. Named layouts
        // carry their own snapshots and are restored only when explicitly chosen.
        *this = std::move(next);
        if (version == 2) { m_pendingLayout = std::move(current); m_layoutPending = true; }
        return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}

std::string EditorPreferences::Serialize(const EditorPanelState& state) const {
    Json saved = Json::array();
    for (const auto& l : layouts) saved.push_back(LayoutJson(l));
    return Json{{"version", 2}, {"current", LayoutJson(Capture("Current", state))},
                {"appearance", {{"mode", appearance.light ? "light" : "dark"}, {"accent", appearance.accent},
                                {"background", appearance.background}, {"text", appearance.text}, {"scale", appearance.scale}}},
                {"updates", {{"automaticAtStartup", automaticUpdates}, {"channel", updateChannel}}},
                {"layouts", saved}}.dump(2) + "\n";
}

bool EditorPreferences::SaveFile(const std::string& path, const EditorPanelState& state, std::string& error) const {
    std::string staged;
    try {
        const auto bytes = Serialize(state);
        if (bytes.size() > maxFile) throw std::runtime_error("Saved layouts exceed the 4 MiB preference budget; delete unused layouts");
        EditorPreferences validated; EditorPanelState scratch;
        if (!validated.Load(bytes, scratch, error)) throw std::runtime_error("Preferences could not be saved: " + error);
        staged = CreateImportStagingFile(std::filesystem::u8path(path));
        std::ofstream file(staged, std::ios::binary); file << bytes; file.close();
        if (!file) throw std::runtime_error("Cannot write editor preferences");
        if (std::filesystem::exists(path))
            std::filesystem::copy_file(path, path + ".bak", std::filesystem::copy_options::overwrite_existing);
        ReplaceStagedFile(staged, path); error.clear(); return true;
    } catch (const std::exception& e) {
        if (!staged.empty()) { std::error_code ignored; std::filesystem::remove(staged, ignored); }
        error = e.what(); return false;
    }
}

bool EditorPreferences::SaveLayout(const std::string& name, const EditorPanelState& state, bool replace, std::string& error) {
    try {
        auto l = ReadLayout(LayoutJson(Capture(name, state)));
        auto found = std::find_if(layouts.begin(), layouts.end(), [&](const auto& x) { return x.name == name; });
        if (found != layouts.end()) {
            if (!replace) throw std::runtime_error("Name already exists. Select it and use Update selected, or choose another name");
            *found = std::move(l);
        } else {
            if (layouts.size() >= maxLayouts) throw std::runtime_error("Maximum 32 saved layouts");
            layouts.push_back(std::move(l));
        }
        m_selected = name; m_saveRequested = true; error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool EditorPreferences::DeleteLayout(const std::string& name) {
    const auto old = layouts.size();
    layouts.erase(std::remove_if(layouts.begin(), layouts.end(), [&](const auto& l) { return l.name == name; }), layouts.end());
    if (old != layouts.size()) { m_saveRequested = true; m_selected.clear(); return true; }
    return false;
}
bool EditorPreferences::QueueLayout(const std::string& name) {
    for (const auto& l : layouts) if (l.name == name) { m_pendingLayout = l; m_layoutPending = true; m_saveAfterLayout = true; return true; }
    return false;
}
void EditorPreferences::ResetLayout() {
    m_pendingLayout = EditorLayoutPreset{}; m_pendingLayout.panels = {"assets"}; m_layoutPending = true; m_saveAfterLayout = true;
}
void EditorPreferences::ApplyPending(EditorPanelState& state) {
    if (m_appearancePending) { ApplyAppearance(appearance); m_appearancePending = false; }
    if (m_layoutPending) {
        ApplyLayout(m_pendingLayout, state);
        ImGui::LoadIniSettingsFromMemory(m_pendingLayout.windows.data(), m_pendingLayout.windows.size());
        m_layoutPending = false;
        if (m_saveAfterLayout) m_saveRequested = true;
        m_saveAfterLayout = false;
    }
}
bool EditorPreferences::ConsumeSaveRequest() { const bool requested = m_saveRequested; m_saveRequested = false; return requested; }

void EditorPreferences::Draw(EditorPanelState& state) {
    if (!state.showEditorPreferences) return;
    ImGui::SetNextWindowSize({560, 610}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Editor preferences", &state.showEditorPreferences)) {
        ImGui::TextWrapped("Editor-only settings, shared across your projects. Game UI and project settings are unaffected.");
        if (ImGui::BeginTabBar("preferences")) {
            if (ImGui::BeginTabItem("Appearance")) {
                int mode = appearance.light ? 1 : 0;
                if (ImGui::Combo("Theme", &mode, "Dark\0Light\0")) {
                    const auto scale = appearance.scale;
                    appearance = ThemeDefaults(mode == 1);
                    appearance.scale = scale;
                    m_appearancePending = true;
                }
                m_appearancePending |= ImGui::ColorEdit3("Accent", appearance.accent.data());
                m_appearancePending |= ImGui::ColorEdit3("Background", appearance.background.data());
                m_appearancePending |= ImGui::ColorEdit3("Text", appearance.text.data());
                m_appearancePending |= ImGui::SliderFloat("UI scale", &appearance.scale, .85f, 1.5f, "%.2fx");
                int presetIndex = 0;
                for (auto preset : {std::array<float,3>{.30f,.70f,.69f}, {.56f,.35f,.88f}, {.18f,.52f,.92f}, {.83f,.42f,.16f}}) {
                    ImGui::PushID(presetIndex++);
                    if (ImGui::ColorButton("Accent preset", {preset[0],preset[1],preset[2],1})) { appearance.accent = preset; m_appearancePending = true; }
                    ImGui::PopID(); ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::TextWrapped("Colours preview immediately. Save preferences, or close the editor normally, to keep them.");
                if (ImGui::Button("Reset appearance")) { appearance = EditorAppearance{}; m_appearancePending = true; }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Layouts")) {
                ImGui::TextWrapped("Drag the workspace splitters or resize tool windows, then save a named layout. This editor uses fixed hierarchy/inspector rails, not free-form docking.");
                ImGui::SliderFloat("Hierarchy width", &state.hierarchyWidth, 150, 800, "%.0f px");
                ImGui::SliderFloat("Inspector width", &state.inspectorWidth, 220, 900, "%.0f px");
                ImGui::SliderFloat("Assets height", &state.assetBrowserHeight, 80, 700, "%.0f px");
                ImGui::TextUnformatted("Visible panels");
                if (ImGui::BeginTable("visibleTools", 2)) {
                    for (auto p : panels) { ImGui::TableNextColumn(); ImGui::Checkbox(p.label, &(state.*(p.flag))); }
                    ImGui::EndTable();
                }
                if (ImGui::BeginCombo("Saved layout", m_selected.empty() ? "Choose a layout" : m_selected.c_str())) {
                    for (const auto& l : layouts) if (ImGui::Selectable(l.name.c_str(), m_selected == l.name)) m_selected = l.name;
                    ImGui::EndCombo();
                }
                ImGui::BeginDisabled(m_selected.empty());
                if (ImGui::Button("Apply selected")) QueueLayout(m_selected);
                ImGui::SameLine();
                if (ImGui::Button("Update selected")) SaveLayout(m_selected, state, true, status);
                ImGui::SameLine();
                if (ImGui::Button("Delete selected")) ImGui::OpenPopup("Delete layout?");
                ImGui::EndDisabled();
                if (ImGui::BeginPopupModal("Delete layout?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("Delete '%s'?", m_selected.c_str());
                    if (ImGui::Button("Delete")) { DeleteLayout(m_selected); ImGui::CloseCurrentPopup(); }
                    ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                char name[129]; std::snprintf(name, sizeof(name), "%s", m_newName.c_str());
                if (ImGui::InputText("New layout name", name, sizeof(name))) m_newName = name;
                if (ImGui::Button("Save as new layout")) SaveLayout(m_newName, state, false, status);
                if (ImGui::Button("Reset panel layout")) ResetLayout();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Updates")) {
                ImGui::Checkbox("Check for updates at startup", &automaticUpdates);
                const char* names[] = {"Installed build channel", "Alpha", "Beta", "Stable (GA only)", "Experimental (opt in)"};
                const char* values[] = {"", "Alpha", "Beta", "Stable", "Experimental"};
                int selected = 0;
                for (int i = 1; i < 5; ++i) if (updateChannel == values[i]) selected = i;
                if (ImGui::Combo("Update channel", &selected, names, 5)) updateChannel = values[selected];
                ImGui::TextWrapped("Notification only. No automatic installation or project changes. Manual checks are in Help. Stable is reserved for GA; Alpha is not stable.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Separator();
        if (ImGui::Button("Save preferences")) m_saveRequested = true;
        if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
    }
    ImGui::End();
}
