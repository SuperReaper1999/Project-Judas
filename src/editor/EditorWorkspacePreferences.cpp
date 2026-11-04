#include "EditorPanels.h"
#include "EditorPreferences.h"
#include "imgui.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void EditorPreferences::LoadUserSettings(EditorPanelState &panels) {
    // Store editor preferences in the platform's user-data directory, never
    // beside a scene or in the repository. Malformed preferences cannot block
    // opening a project; layout clamps also protect smaller window sizes.
    char *directory = SDL_GetPrefPath("Judas", "Editor");
    if (!directory)
        return;
    m_storagePath = (fs::path(directory) / "workspace.json").string();
    m_iniPath = (fs::path(directory) / "windows.ini").string();
    SDL_free(directory);
    ImGui::GetIO().IniFilename = m_iniPath.c_str();
    std::ifstream file(m_storagePath);
    if (!file)
        return;
    // Bound user configuration before parsing. Invalid settings never prevent startup.
    std::string text(4 * 1024 * 1024 + 1, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<size_t>(file.gcount()));
    std::string error;
    if (!Load(text, panels, error))
        status = "Preferences could not be loaded: " + error +
                 ". Defaults are in use; the next save retains a .bak copy.";
}

void EditorPreferences::SaveUserSettings(const EditorPanelState &panels) {
    if (m_storagePath.empty())
        return;
    std::string error;
    if (!SaveFile(m_storagePath, panels, error)) {
        status = "Preferences were not saved: " + error;
        std::fprintf(stderr, "[editor] %s\n", status.c_str());
    } else
        status = "Editor preferences saved.";
}
