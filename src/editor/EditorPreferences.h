#pragma once
#include <array>
#include <string>
#include <vector>

struct EditorPanelState;

struct EditorAppearance {
    bool light = false;
    std::array<float, 3> accent{.30f, .70f, .69f};
    std::array<float, 3> background{.075f, .091f, .115f};
    std::array<float, 3> text{.86f, .91f, .94f};
    float scale = 1;
};

struct EditorLayoutPreset {
    std::string name, windows;
    float hierarchy = 250, inspector = 340, assets = 200;
    // Stable preference names, independent of scene/component serialization.
    std::vector<std::string> panels;
};

// User-owned editor configuration. Never stored in a game project/export.
class EditorPreferences {
public:
    EditorAppearance appearance;
    std::vector<EditorLayoutPreset> layouts;
    bool Load(const std::string& json, EditorPanelState&, std::string& error);
    std::string Serialize(const EditorPanelState&) const;
    bool SaveFile(const std::string& path, const EditorPanelState&, std::string& error) const;
    bool SaveLayout(const std::string& name, const EditorPanelState&, bool replace, std::string& error);
    bool DeleteLayout(const std::string& name);
    bool QueueLayout(const std::string& name);
    void ResetLayout();
    void ApplyPending(EditorPanelState&); // between frames, before NewFrame
    void Draw(EditorPanelState&);
    void RefreshAppearance() { m_appearancePending = true; }
    bool ConsumeSaveRequest();
    std::string status;
private:
    EditorLayoutPreset Capture(const std::string&, const EditorPanelState&) const;
    EditorLayoutPreset m_pendingLayout;
    bool m_layoutPending = false, m_appearancePending = true, m_saveRequested = false, m_saveAfterLayout = false;
    std::string m_selected, m_newName = "My layout";
};
