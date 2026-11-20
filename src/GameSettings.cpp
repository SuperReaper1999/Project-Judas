#include "GameSettings.h"

#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldGraphicsState.h"
#include "WorldPresentation.h"
#include "CameraProjection.h"
#include <glm/gtc/matrix_transform.hpp>
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

constexpr unsigned ShadowResolutions[]{256,512,768,1024,1536};
constexpr float RenderScales[]{.5f,.67f,.75f,.85f,1.f};
constexpr unsigned AnisotropyLevels[]{1,2,4,8,16};
using WeatherQuality = GraphicsQuality::Weather;
constexpr std::pair<std::string_view,GraphicsQuality> QualityPresets[]{
    {"low",      { 256, 1, 1.f,   false, 2.f,  WeatherQuality::Low,      0, .5f,  2.f,  1,  1, .35f, .5f, 2.f, .4f, 0, false}},
    {"medium",   { 512, 2, 1.f,   false, 1.5f, WeatherQuality::Low,      2, .67f, 1.5f, 1,  2, .6f, .75f, 1.5f, .65f, 4, false}},
    {"high",     { 768, 3, .85f,  true,  1.f,  WeatherQuality::Authored, 4, .85f, 1.f,  0,  4, .8f, 1.f, 1.f, .85f, 7}},
    {"veryHigh", {1024, 3, 1.f,   true,  .75f, WeatherQuality::High,     6, 1.f,  .75f, 0,  8, 1.f, 1.f, .75f, 1.f, 10}},
    {"ultra",    {1536, 3, 1.25f, true,  .5f,  WeatherQuality::High,     8, 1.f,  .5f, -1, 16, 1.f, 1.5f, .5f, 1.f, 10}}
};

GraphicsQuality PresetForDevice(GraphicsQuality values, unsigned anisotropyLimit) {
    const auto cap=std::min(values.materialAnisotropy, anisotropyLimit);
    values.materialAnisotropy=*std::prev(std::upper_bound(std::begin(AnisotropyLevels),std::end(AnisotropyLevels),cap));
    return values;
}

Json QualityValues(const GraphicsQuality& quality) {
    const char* weather = quality.weather == WeatherQuality::Authored ? "authored" :
                          quality.weather == WeatherQuality::Low ? "low" : "high";
    return {{"shadowResolution", quality.shadowResolution},
            {"localShadowResolution",quality.shadowResolution},
            {"directionalShadowResolution",quality.SunShadowResolution()},
            {"directionalCascades", quality.directionalCascades},
            {"shadowDistanceScale", quality.shadowDistanceScale}, {"shadowFilter", quality.softShadows ? "soft" : "fast"},
            {"meshLodBias", quality.meshLodBias}, {"weather", weather}, {"localShadowBudget", quality.localShadowBudget},
            {"renderScale", double(std::lround(quality.renderScale*100))/100},
            {"heightfieldLodBias", quality.heightfieldLodBias}, {"waterSurfaceLodBias", quality.waterSurfaceLodBias},
            {"materialAnisotropy",quality.materialAnisotropy},
            {"decorativeDensity",quality.decorativeDensity},
            {"decorativeDistanceScale",quality.decorativeDistanceScale},
            {"planetLodBias",quality.planetLodBias},
            {"optionalParticleDensity",quality.optionalParticleDensity},
            {"waterReflectionSteps",quality.waterReflectionSteps},{"caveDetailNormals",quality.caveDetailNormals}};
}

float QualityNumber(const Json& value, float minimum, float maximum, const char* name) {
    Require(value.is_number() && value >= minimum && value <= maximum,
            std::string(name) + " is outside the supported range");
    return value.get<float>();
}

GraphicsQuality ParseQuality(const Json& input, GraphicsQuality quality, unsigned anisotropyLimit=16) {
    if (input.is_string()) {
        if (input == "default") return {};
        for (const auto& preset : QualityPresets)
            if (input == preset.first) return PresetForDevice(preset.second,anisotropyLimit);
        throw std::runtime_error("Unknown graphics quality preset");
    }
    Keys(input, {"shadowResolution", "directionalShadowResolution", "localShadowResolution",
                 "directionalCascades", "shadowDistanceScale", "shadowFilter",
                 "meshLodBias", "weather", "localShadowBudget", "renderScale", "heightfieldLodBias",
                 "waterSurfaceLodBias", "materialAnisotropy", "decorativeDensity",
                 "decorativeDistanceScale", "planetLodBias", "optionalParticleDensity",
                 "waterReflectionSteps", "caveDetailNormals"});
    const auto sizeFor=[&](const char* key) {
        const auto size=unsigned(Integer(input.at(key),256,1536,key));
        Require(std::find(std::begin(ShadowResolutions),std::end(ShadowResolutions),size)!=std::end(ShadowResolutions),
                std::string("Choose a supported size for ")+key);
        return size;
    };
    unsigned sunSize=quality.SunShadowResolution();
    if(input.contains("shadowResolution"))sunSize=quality.shadowResolution=sizeFor("shadowResolution");
    if(input.contains("localShadowResolution")){
        const auto localSize=sizeFor("localShadowResolution");
        Require(!input.contains("shadowResolution")||localSize==quality.shadowResolution,
                "Legacy shadowResolution and localShadowResolution disagree");
        quality.shadowResolution=localSize;
    }
    if(input.contains("directionalShadowResolution"))sunSize=sizeFor("directionalShadowResolution");
    quality.directionalShadowResolution=sunSize==quality.shadowResolution?0:sunSize;
    if (input.contains("directionalCascades"))
        quality.directionalCascades = Integer(input.at("directionalCascades"), 1, 3, "directionalCascades");
    if (input.contains("shadowDistanceScale"))
        quality.shadowDistanceScale = QualityNumber(input.at("shadowDistanceScale"), .5f, 1.5f, "shadowDistanceScale");
    if (input.contains("shadowFilter")) {
        const auto& filter = input.at("shadowFilter");
        Require(filter == "fast" || filter == "soft", "shadowFilter must be fast or soft");
        quality.softShadows = filter == "soft";
    }
    if (input.contains("meshLodBias"))
        quality.meshLodBias = QualityNumber(input.at("meshLodBias"), .5f, 2.f, "meshLodBias");
    if (input.contains("weather")) {
        const auto& weather = input.at("weather");
        Require(weather == "authored" || weather == "low" || weather == "high", "weather must be authored, low or high");
        quality.weather = weather == "authored" ? WeatherQuality::Authored :
                          weather == "low" ? WeatherQuality::Low : WeatherQuality::High;
    }
    if (input.contains("localShadowBudget"))
        quality.localShadowBudget = unsigned(Integer(input.at("localShadowBudget"), 0, 8, "localShadowBudget"));
    if (input.contains("renderScale")) {
        const auto scale = QualityNumber(input.at("renderScale"), .5f, 1.f, "renderScale");
        Require(std::find(std::begin(RenderScales),std::end(RenderScales),scale)!=std::end(RenderScales),
                "Choose a supported render scale");
        quality.renderScale = scale;
    }
    if (input.contains("heightfieldLodBias"))
        quality.heightfieldLodBias = QualityNumber(input.at("heightfieldLodBias"), .5f, 2.f, "heightfieldLodBias");
    if (input.contains("waterSurfaceLodBias"))
        quality.waterSurfaceLodBias = Integer(input.at("waterSurfaceLodBias"), -1, 1, "waterSurfaceLodBias");
    if (input.contains("materialAnisotropy")) {
        const auto amount=unsigned(Integer(input.at("materialAnisotropy"),1,16,"materialAnisotropy"));
        Require(amount<=anisotropyLimit &&
                std::find(std::begin(AnisotropyLevels),std::end(AnisotropyLevels),amount)!=std::end(AnisotropyLevels),
                "Choose a material anisotropy level supported by this device");
        quality.materialAnisotropy=amount;
    }
    if(input.contains("decorativeDensity"))
        quality.decorativeDensity=QualityNumber(input.at("decorativeDensity"),0,1,"decorativeDensity");
    if(input.contains("decorativeDistanceScale"))
        quality.decorativeDistanceScale=QualityNumber(input.at("decorativeDistanceScale"),.25f,2,"decorativeDistanceScale");
    if(input.contains("planetLodBias"))
        quality.planetLodBias=QualityNumber(input.at("planetLodBias"),.5f,2,"planetLodBias");
    if(input.contains("optionalParticleDensity"))
        quality.optionalParticleDensity=QualityNumber(input.at("optionalParticleDensity"),0,1,"optionalParticleDensity");
    if(input.contains("caveDetailNormals"))
        quality.caveDetailNormals=Boolean(input.at("caveDetailNormals"));
    if(input.contains("waterReflectionSteps"))
        quality.waterReflectionSteps=Integer(input.at("waterReflectionSteps"),0,10,"waterReflectionSteps");
    return quality;
}

Json QualityState(const GraphicsQuality& effective, const std::optional<GraphicsQuality>& pending, int limit,
                  glm::ivec2 mainView, unsigned anisotropyLimit, bool separateShadows,
                  std::size_t shadowStorageBytes) {
    std::string_view matching = "custom";
    auto presets = Json::object();
    for (const auto& [name, preset] : QualityPresets) {
        const auto values=PresetForDevice(preset,anisotropyLimit);
        if (values == effective) matching = name;
        if (int(values.shadowResolution) <= limit) presets[std::string(name)] = QualityValues(values);
    }
    auto resolutions = Json::array();
    for (const auto size : ShadowResolutions) if (int(size) <= limit) resolutions.push_back(size);
    auto anisotropy=Json::array();
    for (const auto amount:AnisotropyLevels) if(amount<=anisotropyLimit)anisotropy.push_back(amount);
    return {{"preset", matching}, {"effective", QualityValues(effective)},
            {"pending", pending ? QualityValues(*pending) : Json(nullptr)}, {"presets", presets},
            {"independentShadowResolutions",separateShadows},{"shadowStorageBytes",shadowStorageBytes},
            {"mainView", mainView.x > 0 ? Json{{"width",mainView.x},{"height",mainView.y}} : Json(nullptr)},
            {"choices", {{"shadowResolution", resolutions}, {"directionalCascades", {1,2,3}},
                         {"localShadowResolution",separateShadows?resolutions:Json::array()},
                         {"directionalShadowResolution",separateShadows?resolutions:Json::array()},
                         {"shadowDistanceScale", {{"min",.5},{"max",1.5}}},
                         {"shadowFilter", {"fast","soft"}}, {"meshLodBias", {{"min",.5},{"max",2}}},
                         {"weather", {"authored","low","high"}}, {"localShadowBudget", {{"min",0},{"max",8}}},
                         {"renderScale", {.5,.67,.75,.85,1}},
                         {"heightfieldLodBias", {{"min",.5},{"max",2}}},
                         {"waterSurfaceLodBias", {-1,0,1}}, {"materialAnisotropy",anisotropy},
                         {"decorativeDensity", {{"min",0},{"max",1}}},
                         {"decorativeDistanceScale", {{"min",.25},{"max",2}}},
                         {"planetLodBias", {{"min",.5},{"max",2}}},
                         {"optionalParticleDensity", {{"min",0},{"max",1}}},
                         {"waterReflectionSteps", {{"min",0},{"max",10}}},
                         {"caveDetailNormals", {false,true}}}}};
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
        Keys(video, {"width", "height", "fullscreen", "vsync", "shadows", "localShadowBudget", "fpsCap", "backend", "quality"});
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
        GraphicsQuality quality;
        quality.localShadowBudget = budget;
        if (video.contains("quality")) {
            Require(video.at("quality").is_object(), "Saved graphics quality must be an object");
            quality = ParseQuality(video.at("quality"), quality);
        }
        const int cap = video.contains("fpsCap") ? VideoFrameCap(video.at("fpsCap")) : 0;
        const auto backend = video.contains("backend") ? Backend(video.at("backend")) : "opengl";
        const auto& game = values.at("game");
        Require(game.is_object(), "Game preferences must be an object");
        m_game = game.dump();
        m_display = display;
        m_vsync = vsync;
        m_shadows = shadows;
        m_quality = quality;
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
    auto& renderer = host.GetRenderer();
    if (renderer.SupportsGraphicsQuality())
        renderer.SetGraphicsQuality(m_quality,m_qualityError,{host.GetWindow().Width(),host.GetWindow().Height()});
    else renderer.SetShadowBudget(m_quality.localShadowBudget);
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
            {"localShadowBudget", m_host->GetRenderer().ShadowBudget()}, {"activeBackend", active},
            {"quality", m_desktop && m_host->GetRenderer().SupportsGraphicsQuality()
                ? QualityState(m_host->GetRenderer().Quality(),m_qualityRequest,m_host->GraphicsCaps().limits.textureSize,
                               m_host->GetRenderer().MainViewSize(),m_host->GetRenderer().MaxMaterialAnisotropy(),
                               m_host->GetRenderer().SupportsIndependentShadowResolution(),
                               m_host->GetRenderer().ShadowStorageBytes())
                : Json(nullptr)}, {"qualityError", m_qualityError.empty() && m_host->GetRenderer().ShadowStorageLimited()
                    ? "Shadow storage could not grow on this device. Choose a smaller shadow resolution or budget."
                    : (m_qualityError.empty() ? m_host->GetRenderer().MainViewError() : m_qualityError)},
            {"requestedBackend", m_switchRequest ? (*m_switchRequest ? "vulkan" : "opengl") : m_backend}, {"backends", backends},
            {"restartRequired", !m_backendOverride && m_backend != active},
            {"liveBackendSwitch", CanSwitchBackend()}, {"backendTransition", m_switchState},
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
    Require(!m_switchRequest, "Wait for the backend transition before saving preferences");
    Require(!m_qualityRequest, "Wait for graphics quality to apply before saving preferences");
    Json video = Json::object();
    if (!reset) {
        video = {{"width", m_display.width}, {"height", m_display.height}, {"fullscreen", m_display.fullscreen},
                 {"fpsCap", m_fpsCap}, {"localShadowBudget", m_quality.localShadowBudget}, {"backend", m_backend}};
        auto quality = QualityValues(m_quality);
        quality.erase("localShadowBudget"); // Preserve the existing on-disk budget key.
        video["quality"] = std::move(quality);
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
    if (operation == "settingsSwitchBackend") {
        Require(CanSwitchBackend(), "Live backend switching is unavailable in this session");
        Require(!m_request && !m_previous, "Finish the display preview before changing backend");
        Require(!m_switchRequest, "A graphics backend change is already queued");
        Require(!m_qualityRequest, "Wait for graphics quality to apply before switching backend");
        const bool requested = Backend(Parse(argument)) == "vulkan";
        if (m_host->GetWindow().UsesVulkan() != requested) {
            m_switchRequest = requested;
            m_switchState = "pending";
            m_backendError.clear();
        }
    } else if (operation == "settingsApplyQuality") {
        Require(!m_switchRequest, "Wait for the backend transition before changing graphics quality");
        auto& renderer = m_host->GetRenderer();
        Require(renderer.SupportsGraphicsQuality(), "Graphics quality requires the shared renderer");
        const auto quality = ParseQuality(Parse(argument),m_qualityRequest.value_or(renderer.Quality()),
                                          renderer.MaxMaterialAnisotropy());
        Require(quality.SunShadowResolution()==quality.shadowResolution || renderer.SupportsIndependentShadowResolution(),
                "Independent shadow resolutions are unavailable on this device");
        Require(int(std::max(quality.shadowResolution,quality.SunShadowResolution()))<=m_host->GraphicsCaps().limits.textureSize,
                "Shadow resolution exceeds the current device texture limit");
        m_qualityError.clear();
        m_qualityRequest = quality;
    } else if (operation == "settingsSetGame") {
        const auto value = Parse(argument);
        Require(value.is_object(), "Game preferences must be an object");
        m_game = value.dump();
    } else if (operation == "settingsSave" || operation == "settingsResetSaved") {
        Write(operation == "settingsResetSaved");
    } else if (operation == "settingsDisplay") {
        Require(!m_switchRequest, "Wait for the backend transition before previewing display changes");
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
        if (value.contains("localShadowBudget"))
            Require(!m_qualityRequest, "Wait for graphics quality to apply before changing the local shadow budget");
        const auto budget = value.contains("localShadowBudget")
            ? unsigned(Integer(value.at("localShadowBudget"), 0, 8, "localShadowBudget")) : m_quality.localShadowBudget;
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
        if (value.contains("localShadowBudget")) {
            m_host->GetRenderer().SetShadowBudget(budget);
            m_quality.localShadowBudget = budget;
        }
        m_shadows = shadows;
        m_fpsCap = cap;
        m_backend = backend;
    } else {
        throw std::logic_error("Unknown settings operation");
    }
    return true;
}

void GameSettings::Advance() {
    if (m_qualityRequest) {
        const auto& window=m_host->GetWindow();
        if (m_host->GetRenderer().SetGraphicsQuality(*m_qualityRequest,m_qualityError,{window.Width(),window.Height()}))
            m_quality = *m_qualityRequest;
        m_qualityRequest.reset();
    }
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


bool GameSettings::CanSwitchBackend() const {
    return m_desktop && m_switchAllowed && !m_backendOverride && JudasVulkan::Compiled() &&
           HasDesktopWindow();
}

bool GameSettings::AdvanceBackendSwitch(RuntimeWorld& world, const char* title, bool visible,
                                        float interpolation, std::string& fatalError) {
    if (!m_switchRequest) return true;
    const auto requested = *m_switchRequest;
    m_switchRequest.reset();
    m_switchState = "active";
    fatalError.clear();
    auto session = world.SceneControl();
    if (!CanSwitchBackend() || (session && (session->OperationInProgress() || session->networkActive))) {
        m_backendError = "Finish scene, save or network operations before switching the graphics backend";
        m_switchState = "failed";
        return true;
    }
    auto& host = *m_host;
    auto& window = host.GetWindow();
    const bool originalBackend = window.UsesVulkan();
    const bool shadows = host.GetRenderer().ShadowsEnabled();
    const auto quality = host.GetRenderer().Quality();
    const auto vsync = window.VSync();
    auto display = CurrentDisplay();
    if (display.fullscreen) { display.width = m_display.width; display.height = m_display.height; }

    auto showTransition = [&] {
        auto& renderer = host.GetRenderer();
        const int width = std::max(window.Width(), 1), height = std::max(window.Height(), 1);
        renderer.BeginFrame(width, height);
        renderer.EndFrame();
        renderer.BeginUIFrame(width, height);
        renderer.DrawUIRect({0,0},{float(width),float(height)},{.025f,.03f,.05f,1});
        const std::string message = "Changing graphics backend...";
        const auto measured = renderer.MeasureUIText(message,.55f);
        renderer.DrawUIText(message,{(float(width)-measured.x)*.5f,(float(height)-measured.y)*.5f},
                            .55f,{1,1,1,1});
        renderer.EndUIFrame();
        window.SwapBuffers();
    };

    const auto started = std::chrono::steady_clock::now();
    WorldGraphicsState state;
    std::string error;
    if (!world.CaptureGraphicsState(state,error)) {
        m_backendError = error;
        m_switchState = "failed";
        return true;
    }
    showTransition();
    if (!world.ReleaseWorldGraphics(host.GetRenderer(), error) || !host.Resources().DetachGraphics(error)) {
        fatalError = "Could not safely retire world graphics: " + error;
        return false;
    }

    auto prepare = [&](bool backend, std::string& diagnostic) -> bool {
        try {
            if (!host.RebuildDesktopGraphics(title,display.width,display.height,visible,backend,diagnostic))
                return false;
            auto& renderer = host.GetRenderer();
            renderer.SetShadowsEnabled(shadows);
            if (!ApplyDisplay(display,diagnostic) || (vsync && !window.SetVSync(*vsync,diagnostic)) ||
                !renderer.SetGraphicsQuality(quality,diagnostic,{window.Width(),window.Height()}) ||
                !world.RestoreGraphicsState(state,host.GraphicsCaps(),diagnostic)) return false;
            showTransition();

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (std::chrono::steady_clock::now() < deadline) {
                host.PumpResources();
                bool assetsReady = false, uiReady = true;
                if (!host.Resources().GraphicsRestorationReady(assetsReady,diagnostic)) return false;
                const int width = std::max(window.Width(),1), height = std::max(window.Height(),1);
                if (world.UIIfLoaded() && !world.UI().PrepareStartup(width,height,uiReady,diagnostic)) return false;
                if (assetsReady && uiReady) {
                    glm::mat4 view(1);
                    if (world.view) {
                        const auto& pose = world.view->pose;
                        view = glm::lookAt(pose.position,pose.position + pose.rotation * glm::vec3(0,0,-1),
                                          pose.rotation * glm::vec3(0,1,0));
                    }
                    const auto frame = BuildCameraFrame(glm::vec3(0),glm::quat(1,0,0,0),60,.1f,1000,
                                                        CameraLens{},width,height);
                    RenderWorldFrame(renderer,width,height,world,view,frame.projection,
                                     glm::vec3(glm::inverse(view)[3]),interpolation,0,
                                     WorldRenderPurpose::GraphicsWarmup);
                    for (const auto& camera : world.PresentationCameras()) {
                        if (camera.settings.enabled && world.RuntimeDefinition(camera.id) && world.IsPublished(camera.id) &&
                            (!camera.target.IsValid() || !camera.error.empty())) {
                            diagnostic = "Camera " + std::to_string(camera.id) + " could not restore: " + camera.error;
                            return false;
                        }
                    }
                    renderer.BeginUIFrame(width,height);
                    if (world.UIIfLoaded()) world.UI().Draw(renderer,width,height,&world);
                    renderer.EndUIFrame();
                    // Keep the transition image visible while any demand found
                    // in this zero-clock frame is still awaiting publication.
                    if (!host.Resources().Stats().loading) {
                        window.SwapBuffers();
                        return true;
                    }
                }
                SDL_Delay(1);
            }
            diagnostic = "Timed out restoring graphics resources";
            return false;
        } catch (const std::exception& failure) {
            diagnostic = failure.what();
            return false;
        }
    };

    const auto logTransition = [&](const char* outcome) {
        const auto elapsed = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        std::fprintf(stderr,"[Graphics transition] %s -> %s: %s in %.2f ms\n",
                     originalBackend ? "vulkan":"opengl", requested ? "vulkan":"opengl", outcome, elapsed);
    };
    if (prepare(requested,error)) {
        m_backend = requested ? "vulkan" : "opengl";
        m_switchState = "completed";
        logTransition("completed");
        return true;
    }
    m_backendError = error;
    if (host.Resources().GetRenderer() &&
        (!world.ReleaseWorldGraphics(host.GetRenderer(),error) || !host.Resources().DetachGraphics(error))) {
        fatalError = m_backendError + "; could not retire the rejected backend: " + error;
        return false;
    }
    if (prepare(originalBackend,error)) {
        m_backend = originalBackend ? "vulkan" : "opengl";
        m_switchState = "recovered";
        logTransition("previous backend recovered");
        return true;
    }
    fatalError = m_backendError + "; previous backend recovery failed: " + error;
    return false;
}
