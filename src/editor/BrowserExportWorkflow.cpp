#include "BrowserExportWorkflow.h"
#include "EditorPanels.h"
#include "Project.h"
#include "EnginePaths.h"
#include "imgui.h"
#include "../../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <thread>
#if defined(__linux__) && !defined(__ANDROID__)
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace fs = std::filesystem;
namespace {
constexpr std::uintmax_t MaxLogBytes = 16 * 1024 * 1024;
constexpr std::size_t DisplayBytes = 64 * 1024;
fs::path NewLog() {
    std::error_code error;
    const auto root = fs::temp_directory_path(error);
    if (error) return {};
    return root / ("judas-browser-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".log");
}
void Field(const char* label, std::string& value) {
    char buffer[4096];
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::TextUnformatted(label);
    ImGui::PushID(label);
    if (ImGui::InputText("##path", buffer, sizeof(buffer))) value = buffer;
    ImGui::PopID();
}
}

BrowserToolJob::~BrowserToolJob() { Stop(); }

bool BrowserToolJob::Start(const std::string& executable,
                           const std::vector<std::string>& arguments, const fs::path& log, std::chrono::seconds limit) {
    if (Running()) return false;
    m_success = false;
    m_limit = limit;
    m_output.clear();
    m_log = log;
#if defined(__linux__) && !defined(__ANDROID__)
    const int file = open(log.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (file < 0) { m_status = "Cannot create a fresh build log"; return false; }
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    posix_spawn_file_actions_init(&actions);
    posix_spawnattr_init(&attributes);
    posix_spawn_file_actions_adddup2(&actions, file, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, file, STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, file);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    // All descendants inherit this private group. Cancel must also stop emcc,
    // wasm-opt and make, rather than leaving them compiling behind the editor.
    int result = posix_spawnattr_setpgroup(&attributes, 0);
    if (!result) result = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    std::vector<std::string> storage{executable};
    storage.insert(storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& word : storage) argv.push_back(word.data());
    argv.push_back(nullptr);
    pid_t child = -1;
    if (!result) result = posix_spawnp(&child, executable.c_str(), &actions, &attributes, argv.data(), environ);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    close(file);
    if (result) { m_status = "Cannot start tool (error " + std::to_string(result) + ")"; return false; }
    m_pid = child;
    m_started = std::chrono::steady_clock::now();
    m_readAt = {};
    m_status = "Running";
    return true;
#else
    (void)executable; (void)arguments;
    m_status = "Experimental browser compilation currently requires Linux";
    return false;
#endif
}

void BrowserToolJob::ReadLog() {
    std::ifstream stream(m_log, std::ios::binary | std::ios::ate);
    if (!stream) return;
    const auto size = stream.tellg();
    if (size < 0) return;
    const auto count = std::min<std::streamoff>(size, DisplayBytes);
    stream.seekg(size - count);
    m_output.resize(static_cast<std::size_t>(count));
    stream.read(m_output.data(), count);
}

void BrowserToolJob::Poll() {
#if defined(__linux__) && !defined(__ANDROID__)
    if (!Running()) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - m_readAt >= std::chrono::milliseconds(200)) {
        m_readAt = now;
        ReadLog();
        std::error_code error;
        const auto size = fs::file_size(m_log, error);
        if ((!error && size > MaxLogBytes) || (m_limit.count() > 0 && now - m_started > m_limit)) {
            Stop(); m_status = "Stopped: tool exceeded its time/log bound; inspect the retained log"; return;
        }
    }
    int status = 0;
    const auto result = waitpid(m_pid, &status, WNOHANG);
    if (result == 0) return;
    if (result < 0 && errno == EINTR) return;
    if (result < 0) { m_pid = -1; m_status = "Cannot reap owned tool process"; return; }
    m_pid = -1;
    m_success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    m_status = m_success ? "Completed" : "Failed (" + std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1) + ")";
    ReadLog();
#endif
}

void BrowserToolJob::Stop() {
#if defined(__linux__) && !defined(__ANDROID__)
    if (!Running()) return;
    const int child = m_pid;
    kill(-child, SIGTERM);
    // Observe exit without reaping: the leader reserves its PID/group until
    // descendants receive the final signal. Give Python time to remove its lock.
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < end) {
        siginfo_t info{};
        if (waitid(P_PID, child, &info, WEXITED | WNOHANG | WNOWAIT) == 0 && info.si_pid == child) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    kill(-child, SIGKILL);
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    m_pid = -1;
    m_success = false;
    m_status = "Stopped; logs and incomplete staging files retained";
    ReadLog();
#endif
}

void BrowserExportWorkflow::Initialize(const Project& project) {
    if (initialized) {
        if (projectFile != project.ProjectFile() && !job.Running() && !preview.Running()) {
            destination = (fs::path(project.RootDir()).parent_path() /
                (fs::path(project.RootDir()).filename().string() + "-web")).string();
            projectFile = project.ProjectFile();
            automaticCheck=false;ready=false;operation.clear();builtPackage.clear();
        }
        return;
    }
    projectFile = project.ProjectFile();
    initialized = true;
    script = fs::absolute(ResolveEngineDataPath("tools/build_browser.py")).lexically_normal().string();
    const auto root = fs::path(script).parent_path().parent_path();
    sdk = (root / ".cache/browser-sdk/emsdk").string();
    if (const char* installed = std::getenv("EMSDK"))
        if (fs::is_regular_file(fs::path(installed)/"upstream/emscripten/emcmake")) sdk = installed;
    cache = (root / ".cache/browser-build").string();
    exporter = (fs::path(EngineExecutableDir()) / "judas_export").string();
    runtime = (fs::path(EngineExecutableDir()) / "judas").string();
    const auto host = fs::path(EngineExecutableDir())/"unicode/build/src/judas_icu_build-build";
    if (fs::is_regular_file(host/"config/icucross.mk")) icu = host.string();
    destination = (fs::path(project.RootDir()).parent_path() /
        (fs::path(project.RootDir()).filename().string() + "-web")).string();
}

std::vector<std::string> BrowserExportWorkflow::Arguments(const Project& project, bool check) const {
    std::vector<std::string> words{script, "-ExpBrowserSupport=1", "--project", project.ProjectFile(),
        "--output", check ? NextDestination() : activeDestination, "--emsdk", sdk, "--build", cache,
        "--exporter", exporter, "--desktop-runtime", runtime, "--jobs", std::to_string(jobs)};
    if (!icu.empty()) { words.push_back("--icu-host"); words.push_back(icu); }
    if (prepareNative) words.push_back("--prepare-native");
    if (check) words.push_back("--check");
    return words;
}

void BrowserExportWorkflow::Start(const Project& project, bool check) {
    operation = check ? "Prerequisite check" : "Browser build";
    if (!check) {
        refreshExistingTab=preview.Running() && !previewURL.empty();
        preview.Stop();
        activeDestination=NextDestination();buildHandled=false;
    }
    checks.clear(); parsedCheckOutput.clear();if(check)ready=false;
    job.Start(python, Arguments(project, check), NewLog(), std::chrono::seconds(check ? 30 : 7200));
}

void BrowserExportWorkflow::Poll() {
    job.Poll(); preview.Poll();
    if (operation == "Prerequisite check" && !job.Running() && parsedCheckOutput != job.Output()) {
        parsedCheckOutput = job.Output();
        try {
        auto report = nlohmann::json::parse(parsedCheckOutput, nullptr, false);
        if (report.is_object() && report.value("kind", "") == "judas-browser-preflight" && report.contains("checks")) {
            checks.clear();
            ready=job.Succeeded() && report.value("ready",false);
            for (const auto& check : report["checks"])
                checks.emplace_back(check.value("ok", false), check.value("name", "") + ": " + check.value("detail", ""));
        }
        } catch (const nlohmann::json::exception&) { checks.clear(); }
    }
    if (operation=="Browser build" && !job.Running() && !buildHandled) {
        buildHandled=true;
        if (job.Succeeded()) {
            ready=true;builtPackage=activeDestination;
            if (playAfterBuild) StartPreview(!refreshExistingTab);
        }
    }
    previewURL.clear();
    if (preview.Running()) {
        const auto start = preview.Output().find("http://127.0.0.1:");
        if (start != std::string::npos) {
            const auto end = preview.Output().find_first_of("\r\n ", start);
            const auto candidate = preview.Output().substr(start, end - start);
            if (candidate.size() > 17 && candidate.find_first_not_of("0123456789", 17) == std::string::npos)
                previewURL = candidate;
        }
    }
    if (openPreviewWhenReady && !previewURL.empty()) {
        openPreviewWhenReady=false;
        SDL_OpenURL(previewURL.c_str());
    }
}

std::string BrowserExportWorkflow::NextDestination() const {
    fs::path base=fs::path(destination).lexically_normal();
    if(base.filename().empty())base=base.parent_path();
    std::error_code error;
    if (!fs::exists(base,error)) return base.string();
    for (unsigned n=2;n<10000;++n) {
        const auto candidate=base.parent_path()/(base.filename().string()+"-"+std::to_string(n));
        if (!fs::exists(candidate,error)) return candidate.string();
    }
    return destination; // The compiler rejects an occupied destination.
}

void BrowserExportWorkflow::StartPreview(bool openWhenReady) {
    if (preview.Running()) {
        if (openWhenReady && !previewURL.empty()) SDL_OpenURL(previewURL.c_str());
        return;
    }
    const auto package=builtPackage.empty()?destination:builtPackage;
    openPreviewWhenReady=openWhenReady;
    preview.Start(python,{(fs::path(script).parent_path()/"serve_browser.py").string(),
        package,"--port",std::to_string(previewPort)},NewLog(),std::chrono::seconds(0));
}

void PollBrowserExportWorkflow(EditorPanelState& state) {
    if (state.browserExport) state.browserExport->Poll();
}

void DrawBrowserExportWorkflow(EditorPanelState& state, EditorRequests& requests) {
    if (!state.experimentalBrowserSupport || !state.project) return;
    if (!state.project->IsLoaded()) {
        ImGui::TextWrapped("Open a game project before exporting it for the web.");
        if(ImGui::Button("Open project..."))requests.openProject=true;
        return;
    }
    if (!state.browserExport) state.browserExport = std::make_shared<BrowserExportWorkflow>();
    auto& flow = *state.browserExport;
    flow.Initialize(*state.project);
    ImGui::SeparatorText("Browser game (Experimental)");
#if !defined(__linux__) || defined(__ANDROID__)
    ImGui::TextWrapped("Browser compilation currently requires a Linux source checkout. Native export remains available above.");
    return;
#endif
    if(state.mode!=EditorMode::Edit)ImGui::TextWrapped("Stop Play in the editor before exporting your game.");
    ImGui::TextWrapped("Make web files for your game and try it in your browser. Nothing is uploaded.");
    if (!flow.automaticCheck && state.mode==EditorMode::Edit) {
        flow.automaticCheck=true;
        flow.Start(*state.project,true);
    }
    const bool busy=flow.job.Running();
    ImGui::Spacing();
    ImGui::TextUnformatted("1. Choose where to save it");
    ImGui::TextWrapped("%s",fs::path(flow.destination).parent_path().string().c_str());
    ImGui::BeginDisabled(busy || state.mode!=EditorMode::Edit);
    if (ImGui::Button("Change folder...")) requests.browseBrowserExport=true;
    ImGui::EndDisabled();
    ImGui::TextDisabled("A new game folder is created each time. Previous builds are kept.");
    ImGui::Spacing();
    ImGui::TextUnformatted("2. Build your game");
    const bool checking=flow.operation=="Prerequisite check" && flow.job.Running();
    const bool building=flow.operation=="Browser build" && flow.job.Running();
    if (checking) ImGui::TextUnformatted("Checking this computer...");
    else if (building) ImGui::TextWrapped("Building your web game. The first build can take several minutes; you can keep using the editor.");
    else if (flow.operation=="Browser build" && !flow.job.Succeeded())
        ImGui::TextWrapped("The build did not finish. Your project and previous builds are safe. Open Setup & troubleshooting below for the reason.");
    else if (!flow.ready)
        ImGui::TextWrapped("This computer needs some setup before it can build browser games. Open Setup & troubleshooting below.");
    else ImGui::TextUnformatted(flow.builtPackage.empty()?"Ready to build.":"Your web game is ready.");
    ImGui::BeginDisabled(busy || state.mode!=EditorMode::Edit || !flow.ready);
    if (ImGui::Button("Build & play",{160,34}) && state.mode==EditorMode::Edit && flow.ready && !busy) {flow.playAfterBuild=true;requests.exportBrowser=true;}
    ImGui::SameLine();
    if (ImGui::Button("Export files only") && state.mode==EditorMode::Edit && flow.ready && !busy) {flow.playAfterBuild=false;requests.exportBrowser=true;}
    ImGui::EndDisabled();
    if (flow.job.Running() && ImGui::Button("Cancel")) {flow.job.Stop();}
    ImGui::TextWrapped("Save any changes when asked. When the browser page opens, press Play to start.");
    if (!flow.builtPackage.empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted("3. Play or share the files");
        ImGui::TextWrapped("Saved to: %s",flow.builtPackage.c_str());
        if (ImGui::Button("Play exported game")) flow.StartPreview(true);
        ImGui::SameLine();
        if (ImGui::Button("Show exported files")) {
            std::string uri="file://";
            constexpr char hex[]="0123456789ABCDEF";
            for(unsigned char c:flow.builtPackage) {
                if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.') uri+=char(c);
                else {uri+='%';uri+=hex[c>>4];uri+=hex[c&15];}
            }
            SDL_OpenURL(uri.c_str());
        }
        ImGui::TextWrapped("Keep the whole exported folder together. The included README explains how to host it online.");
    }
    if (flow.preview.Running()) {
        ImGui::TextUnformatted(flow.previewURL.empty()?"Opening your game...":flow.refreshExistingTab?"Updated game ready. Refresh your existing browser tab to play it.":"Your game is available in the browser.");
        if (ImGui::Button("Stop local preview")) flow.preview.Stop();
        ImGui::TextDisabled("Close the game tab when finished. Closing the editor stops local hosting.");
    } else if (!flow.preview.Status().empty() && !flow.preview.Succeeded() && flow.preview.Status().rfind("Stopped",0)!=0) {
        ImGui::TextWrapped("The browser preview could not start. Try another local address, or open troubleshooting.");
        if (ImGui::Button("Try another local address")) {flow.previewPort=flow.previewPort<65535?flow.previewPort+1:8081;flow.StartPreview(true);}
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Setup & troubleshooting")) {
        ImGui::TextWrapped("Build tools are detected automatically. Only change these if setup fails or you use a custom installation.");
        ImGui::BeginDisabled(busy);
        Field("Game folder name",flow.destination);
        Field("Emscripten installation",flow.sdk);
        Field("Build cache",flow.cache);
        Field("Native exporter",flow.exporter);
        Field("Native runtime",flow.runtime);
        Field("ICU build (optional)",flow.icu);
        Field("Browser build script",flow.script);
        Field("Python executable",flow.python);
        ImGui::Checkbox("Refresh existing build tools automatically",&flow.prepareNative);
        ImGui::SliderInt("Build workers",&flow.jobs,1,8);
        if(ImGui::InputInt("Local preview port",&flow.previewPort))flow.previewPort=std::clamp(flow.previewPort,0,65535);
        if(ImGui::Button("Check setup again")) flow.Start(*state.project,true);
        ImGui::EndDisabled();
        for(const auto& check:flow.checks)ImGui::TextWrapped("%s %s",check.first?"OK:":"Needs attention:",check.second.c_str());
        if(!flow.job.Log().empty())ImGui::TextWrapped("Build log: %s",flow.job.Log().string().c_str());
        ImGui::BeginChild("Browser details",{0,150},true,ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(flow.job.Output().c_str());ImGui::EndChild();
        if(!flow.preview.Output().empty())ImGui::TextWrapped("%s",flow.preview.Output().c_str());
        ImGui::TextWrapped("Browser saves use this local address and browser profile. Changing the preview port uses a different save location. Nothing is installed automatically.");
        if(ImGui::Button("Detailed browser guide"))requests.openExternalPath=ResolveEngineDataPath("docs/BROWSER_SUPPORT.md");
    }
}

void DrawBrowserExportWindow(EditorPanelState& state,EditorRequests& requests) {
    if(!state.experimentalBrowserSupport || !state.showBrowserExport || !state.project)return;
    ImGui::SetNextWindowSize({700,550},ImGuiCond_FirstUseEver);
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_FirstUseEver,{.5f,.5f});
    if(ImGui::Begin("Export your game for the web",&state.showBrowserExport))
        DrawBrowserExportWorkflow(state,requests);
    ImGui::End();
}
