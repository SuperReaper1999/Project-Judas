#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

#include "../third_party/nlohmann/json.hpp"

class EngineHost;
class Project;
class RuntimeWorld;

// One standalone application owns preferences across scene and save transitions.
class GameSettings {
public:
    GameSettings(const Project&, int width, int height);
    std::optional<bool> BackendRequest() const;
    void BackendFailed(const std::string&);
    void Attach(EngineHost&);
    nlohmann::json Invoke(const std::string& operation, const std::string& argument);
    void Advance();
    void EnableBackendSwitch(bool allowed) { m_switchAllowed = allowed; }
    bool BackendSwitchPending() const { return m_switchRequest.has_value(); }
    bool AdvanceBackendSwitch(RuntimeWorld&, const char* title, bool visible,
                              float interpolation, std::string& fatalError);
    int FrameCap() const { return m_fpsCap; }

private:
    struct Display {
        bool fullscreen = false;
        int width = 1024, height = 768;
    };
    Display CurrentDisplay() const;
    bool ApplyDisplay(const Display&, std::string&);
    nlohmann::json Read() const;
    void Write(bool reset);
    std::filesystem::path m_path;
    EngineHost* m_host = nullptr;
    Display m_display;
    std::optional<Display> m_request, m_previous;
    std::chrono::steady_clock::time_point m_confirmationDeadline;
    bool m_revert = false;
    bool m_confirm = false;
    bool m_windowOverride = false;
    bool m_shadowsOverride = false;
    bool m_backendOverride = false;
    bool m_desktop = false;
    std::optional<bool> m_vsync, m_shadows;
    unsigned m_localShadowBudget = 8;
    int m_fpsCap = 0;
    bool CanSwitchBackend() const;
    bool m_switchAllowed = false;
    std::optional<bool> m_switchRequest;
    std::string m_switchState = "idle";
    std::string m_backend = "opengl";
    std::string m_loadError, m_displayError, m_startupError, m_backendError;
    std::string m_game = "{}";
};
