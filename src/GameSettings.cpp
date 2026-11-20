#include "GameSettings.h"

#include "EngineHost.h"
#include "PlatformServices.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "vulkan/VulkanBackend.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
constexpr size_t MaxSettingsBytes = 64 * 1024;
constexpr auto ConfirmationTime = std::chrono::seconds(15);

void Require(bool ok, const std::string& error) {
    if (!ok) throw std::runtime_error(error);
}

Json Parse(const std::string& bytes) {
    Require(bytes.size() <= MaxSettingsBytes, "Settings exceed 64 KiB");
    return Json::parse(bytes, [](int depth, Json::parse_event_t, Json&) {
        Require(depth <= 32, "Settings are nested too deeply");
        return true;
    });
}

void Keys(const Json& value, std::initializer_list<std::string_view> names) {
    Require(value.is_object(), "Settings must be an object");
    for (auto it = value.begin(); it != value.end(); ++it)
        Require(std::find(names.begin(), names.end(), it.key()) != names.end(),
                "Unknown setting: " + it.key());
}

int Integer(const Json& value, int min, int max, const char* name) {
    Require(value.is_number_integer() && value >= min && value <= max,
            std::string(name) + " must be an integer in " + std::to_string(min) + ".." + std::to_string(max));
    return value.get<int>();
}

int VideoFrameCap(const Json& value) {
    const int cap = Integer(value, 0, 1000, "fpsCap");
    // A very low cap can force the 60 Hz simulation to exceed its per-frame
    // catch-up limit even when the machine can run the workload in real time.
    Require(cap == 0 || cap >= 15, "fpsCap must be zero (uncapped) or at least 15");
    return cap;
}

bool HasDesktopWindow() {
    const auto* driver = SDL_GetCurrentVideoDriver();
    return driver && (std::string_view(driver) == "x11" || std::string_view(driver) == "wayland");
}

bool Boolean(const Json& value) {
    Require(value.is_boolean(), "Expected a boolean setting");
    return value.get<bool>();
}

std::string Backend(const Json& value) {
    Require(value == "opengl" || value == "vulkan", "Backend must be opengl or vulkan");
    return value.get<std::string>();
}
}

GameSettings::GameSettings(const Project& project, int width, int height) {
#if defined(__linux__) && !defined(__ANDROID__)
    m_desktop = true;
#endif
    m_windowOverride = std::getenv("JUDAS_WINDOW_SIZE") != nullptr;
    if (!m_windowOverride) m_display = {false, width, height};
    m_shadowsOverride = std::getenv("JUDAS_SHADOWS") != nullptr;
    m_backendOverride = std::getenv("JUDAS_GRAPHICS") != nullptr;
    if (!m_desktop || !project.IsLoaded()) return;

    const auto root = PlatformUserDataDirectory(m_loadError);
    if (root.empty()) return;
    const auto& projectSettings = project.Settings();
    const auto id = projectSettings.saveIdentity.empty()
        ? SceneFingerprintSha256(projectSettings.name + "\n" + std::filesystem::path(project.ProjectFile()).filename().string())
        : projectSettings.saveIdentity;
    m_path = std::filesystem::path(root) / "Judas" / "settings" / id / "preferences.json";

    try {
        if (!std::filesystem::exists(m_path)) return;
        std::ifstream file(m_path, std::ios::binary);
        Require(bool(file), "Cannot read preferences");
        std::string bytes(MaxSettingsBytes + 1, '\0');
        file.read(bytes.data(), bytes.size());
        Require(!file.bad(), "Cannot read preferences");
        bytes.resize(file.gcount());
        const auto values = Parse(bytes);
        Keys(values, {"version", "video", "game"});
        Require(values.at("version").is_number_integer() && values.at("version") == 1,
                "Unsupported preferences version");
        const auto& video = values.at("video");
        Keys(video, {"width", "height", "fullscreen", "vsync", "shadows", "localShadowBudget", "fpsCap", "backend"});
        Display display = m_display;
        if (video.contains("width")) {
            display = {Boolean(video.at("fullscreen")), Integer(video.at("width"), 1, 8192, "width"),
                       Integer(video.at("height"), 1, 8192, "height")};
        }
        const auto unpack = [&](const char* key) -> std::optional<bool> {
            if (!video.contains(key)) return {};
            return Boolean(video.at(key));
        };
        const auto vsync = unpack("vsync");
        const auto shadows = unpack("shadows");
        const int budget = video.contains("localShadowBudget")
            ? Integer(video.at("localShadowBudget"), 0, 8, "localShadowBudget") : 8;
        const int cap = video.contains("fpsCap") ? VideoFrameCap(video.at("fpsCap")) : 0;
        const auto backend = video.contains("backend") ? Backend(video.at("backend")) : "opengl";
        const auto& game = values.at("game");
        Require(game.is_object(), "Game preferences must be an object");
        m_game = game.dump();
        m_display = display;
        m_vsync = vsync;
        m_shadows = shadows;
        m_localShadowBudget = budget;
        m_fpsCap = cap;
        m_backend = backend;
    } catch (const std::exception& e) {
        m_loadError = e.what();
    }
}

std::optional<bool> GameSettings::BackendRequest() const {
    if (m_backendOverride || !m_desktop) return {};
    return m_backend == "vulkan";
}

void GameSettings::BackendFailed(const std::string& error) {
    m_backendError = error;
}

GameSettings::Display GameSettings::CurrentDisplay() const {
    const auto& window = m_host->GetWindow();
    return {bool(SDL_GetWindowFlags(window.NativeWindow()) & SDL_WINDOW_FULLSCREEN),
            window.Width(), window.Height()};
}

bool GameSettings::ApplyDisplay(const Display& display, std::string& error) {
    auto& window = m_host->GetWindow();
    if (CurrentDisplay().fullscreen != display.fullscreen &&
        !window.SetBorderlessFullscreen(display.fullscreen, error)) return false;
    return display.fullscreen ||
        (window.Width() == display.width && window.Height() == display.height) ||
        window.Resize(display.width, display.height, error);
}

void GameSettings::Attach(EngineHost& host) {
    m_host = &host;
    if (!m_desktop) return;
    const auto original = CurrentDisplay();
    if (!m_windowOverride && !ApplyDisplay(m_display, m_startupError)) {
        std::string recovery;
        if (!ApplyDisplay(original, recovery)) m_startupError += "; recovery: " + recovery;
        m_display = CurrentDisplay();
    }
    if (m_vsync) {
        std::string error;
        if (!host.GetWindow().SetVSync(*m_vsync, error)) {
            if (!m_startupError.empty()) m_startupError += "; ";
            m_startupError += error;
            m_vsync.reset();
        }
    }
    if (!m_shadowsOverride && m_shadows) host.GetRenderer().SetShadowsEnabled(*m_shadows);
    host.GetRenderer().SetShadowBudget(m_localShadowBudget);
}

Json GameSettings::Read() const {
    const auto& window = m_host->GetWindow();
    const auto display = CurrentDisplay();
    const auto active = window.UsesVulkan() ? "vulkan" : "opengl";
    auto backends = Json::array();
    backends.push_back({{"name", "opengl"}, {"status", window.UsesVulkan() ? "candidate" : "active"}});
    if (JudasVulkan::Compiled())
        backends.push_back({{"name", "vulkan"}, {"status", window.UsesVulkan() ? "active" :
                                         (m_backendError.empty() ? "candidate" : "failed")}});
    auto resolutions = Json::array();
    const bool desktopWindow = m_desktop && HasDesktopWindow();
    if (desktopWindow) {
        std::set<std::pair<int,int>> distinct;
        const int monitor = SDL_GetWindowDisplayIndex(window.NativeWindow());
        const int count = SDL_GetNumDisplayModes(monitor);
        for (int i = 0; i < count; ++i) {
            SDL_DisplayMode mode;
            if (SDL_GetDisplayMode(monitor, i, &mode) == 0 && distinct.emplace(mode.w, mode.h).second)
                resolutions.push_back({{"width", mode.w}, {"height", mode.h}});
        }
    }
    const auto vsync = window.VSync();
    const bool preview = m_previous.has_value();
    const auto remaining = preview ?
        std::chrono::duration<double>(m_confirmationDeadline - std::chrono::steady_clock::now()).count() : 0.0;
    return {{"width", display.width}, {"height", display.height}, {"fullscreen", display.fullscreen},
            {"resolutions", resolutions},
            {"displayModes", desktopWindow ? Json::array({"windowed", "borderless"}) : Json::array()},
            {"vsync", vsync ? Json(*vsync) : Json(nullptr)},
            {"vsyncRequested", m_vsync ? Json(*m_vsync) : Json(nullptr)},
            {"canDisableVSync", window.UsesVulkan() ? Json(JudasVulkan::SupportsSwapInterval(0)) : Json(nullptr)},
            {"fpsCap", m_fpsCap}, {"shadows", m_host->GetRenderer().ShadowsEnabled()},
            {"localShadowBudget", m_localShadowBudget}, {"activeBackend", active},
            {"requestedBackend", m_backend}, {"backends", backends},
            {"restartRequired", !m_backendOverride && m_backend != active},
            {"displayLocked", m_windowOverride}, {"shadowsLocked", m_shadowsOverride},
            {"backendLocked", m_backendOverride}, {"desktop", m_desktop}, {"persistent", !m_path.empty()},
            {"displayPending", m_request.has_value() || m_revert || m_confirm},
            {"displayPreviewSeconds", std::max(0.0, remaining)}, {"displayPreview", preview},
            {"displayError", m_displayError}, {"loadError", m_loadError},
            {"startupError", m_startupError}, {"backendError", m_backendError}};
}

void GameSettings::Write(bool reset) {
    Require(!m_path.empty(), "This game has no writable preferences path");
    Require(reset || m_loadError.empty(), "Preferences could not be loaded; explicitly reset before replacing them");
    Require(!m_request && !m_previous, "Confirm or revert the display preview before saving preferences");
    Json video = Json::object();
    if (!reset) {
        video = {{"width", m_display.width}, {"height", m_display.height}, {"fullscreen", m_display.fullscreen},
                 {"fpsCap", m_fpsCap}, {"localShadowBudget", m_localShadowBudget}, {"backend", m_backend}};
        if (m_vsync) video["vsync"] = *m_vsync;
        if (m_shadows) video["shadows"] = *m_shadows;
    }
    const auto data = Json{{"version", 1}, {"video", video}, {"game", reset ? Json::object() : Parse(m_game)}}.dump(2);
    Parse(data);
    std::filesystem::create_directories(m_path.parent_path());
    const auto staged = CreateImportStagingFile(m_path);
    try {
        std::ofstream out(staged, std::ios::binary);
        out.write(data.data(), data.size());
        out.close();
        Require(bool(out), "Cannot write preferences");
        ReplaceStagedFile(staged, m_path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(staged, ignored);
        throw;
    }
    if (reset) m_loadError.clear();
}

Json GameSettings::Invoke(const std::string& operation, const std::string& argument) {
    if (operation == "settingsRead") return Read();
    if (operation == "settingsGame") return Parse(m_game);
    Require(m_desktop, "Game settings changes are supported only on Linux desktop");
    if (operation == "settingsSetGame") {
        const auto value = Parse(argument);
        Require(value.is_object(), "Game preferences must be an object");
        m_game = value.dump();
    } else if (operation == "settingsSave" || operation == "settingsResetSaved") {
        Write(operation == "settingsResetSaved");
    } else if (operation == "settingsDisplay") {
        Require(!m_windowOverride, "Display is locked by JUDAS_WINDOW_SIZE");
        Require(HasDesktopWindow(), "Display changes require an X11 or Wayland window");
        Require(!m_request && !m_previous, "A display preview is already in progress");
        const auto value = Parse(argument);
        Keys(value, {"fullscreen", "width", "height"});
        auto request = m_display;
        request.fullscreen = Boolean(value.at("fullscreen"));
        if (!request.fullscreen) {
            request.width = Integer(value.at("width"), 1, 8192, "width");
            request.height = Integer(value.at("height"), 1, 8192, "height");
        } else {
            Require(!value.contains("width") && !value.contains("height"), "Borderless fullscreen uses the current desktop resolution");
        }
        m_displayError.clear();
        m_request = request;
    } else if (operation == "settingsConfirmDisplay" || operation == "settingsRevertDisplay") {
        Require(m_previous && !m_confirm && !m_revert, "No display preview awaits confirmation");
        m_confirm = operation == "settingsConfirmDisplay";
        m_revert = !m_confirm;
    } else if (operation == "settingsApplyVideo") {
        const auto value = Parse(argument);
        Keys(value, {"vsync", "fpsCap", "shadows", "localShadowBudget", "backend"});
        const auto cap = value.contains("fpsCap") ? VideoFrameCap(value.at("fpsCap")) : m_fpsCap;
        const auto budget = value.contains("localShadowBudget")
            ? unsigned(Integer(value.at("localShadowBudget"), 0, 8, "localShadowBudget")) : m_localShadowBudget;
        const auto backend = value.contains("backend") ? Backend(value.at("backend")) : m_backend;
        auto shadows = m_shadows;
        if (value.contains("shadows")) {
            Require(!m_shadowsOverride, "Shadows are locked by JUDAS_SHADOWS");
            shadows = Boolean(value.at("shadows"));
        }
        if (value.contains("backend")) {
            Require(!m_backendOverride, "Graphics backend is locked by JUDAS_GRAPHICS");
            Require(backend != "vulkan" || JudasVulkan::Compiled(), "Vulkan is not compiled into this runtime");
        }
        if (value.contains("vsync")) {
            const bool sync = Boolean(value.at("vsync"));
            std::string error;
            const bool applied = m_host->GetWindow().SetVSync(sync, error);
            Require(applied, error);
            m_vsync = sync;
        }
        if (shadows && !m_shadowsOverride) m_host->GetRenderer().SetShadowsEnabled(*shadows);
        m_host->GetRenderer().SetShadowBudget(budget);
        m_localShadowBudget = budget;
        m_shadows = shadows;
        m_fpsCap = cap;
        m_backend = backend;
    } else {
        throw std::logic_error("Unknown settings operation");
    }
    return true;
}

void GameSettings::Advance() {
    if (m_request) {
        const auto prior = CurrentDisplay();
        const auto request = *m_request;
        m_request.reset();
        if (ApplyDisplay(request, m_displayError)) {
            m_previous = prior;
            m_confirmationDeadline = std::chrono::steady_clock::now() + ConfirmationTime;
        } else {
            std::string recovery;
            if (!ApplyDisplay(prior, recovery)) m_displayError += "; recovery: " + recovery;
        }
        return;
    }
    if (!m_previous) return;
    if (m_revert || std::chrono::steady_clock::now() >= m_confirmationDeadline) {
        if (ApplyDisplay(*m_previous, m_displayError) && !m_revert)
            m_displayError = "Display preview expired and was reverted";
        m_previous.reset();
    } else if (m_confirm) {
        const auto current = CurrentDisplay();
        if (!current.fullscreen) m_display = current;
        else {
            m_display.fullscreen = true;
            if (!m_previous->fullscreen) {
                m_display.width = m_previous->width;
                m_display.height = m_previous->height;
            }
        }
        m_previous.reset();
    }
    m_revert = m_confirm = false;
}
