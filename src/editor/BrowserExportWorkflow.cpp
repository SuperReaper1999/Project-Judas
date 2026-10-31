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
        }
        return;
    }
    projectFile = project.ProjectFile();
    initialized = true;
    script = fs::absolute(ResolveEngineDataPath("tools/build_browser.py")).lexically_normal().string();
    const auto root = fs::path(script).parent_path().parent_path();
    sdk = (root / ".cache/browser-sdk/emsdk").string();
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
        "--output", destination, "--emsdk", sdk, "--build", cache,
        "--exporter", exporter, "--desktop-runtime", runtime, "--jobs", std::to_string(jobs)};
    if (!icu.empty()) { words.push_back("--icu-host"); words.push_back(icu); }
    if (check) words.push_back("--check");
    return words;
}

void BrowserExportWorkflow::Start(const Project& project, bool check) {
    operation = check ? "Prerequisite check" : "Browser build";
    checks.clear(); parsedCheckOutput.clear();
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
            for (const auto& check : report["checks"])
                checks.emplace_back(check.value("ok", false), check.value("name", "") + ": " + check.value("detail", ""));
        }
        } catch (const nlohmann::json::exception&) { checks.clear(); }
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
}

void PollBrowserExportWorkflow(EditorPanelState& state) {
    if (state.browserExport) state.browserExport->Poll();
}

void DrawBrowserExportWorkflow(EditorPanelState& state, EditorRequests& requests) {
    if (!state.experimentalBrowserSupport || !state.project) return;
    if (!state.browserExport) state.browserExport = std::make_shared<BrowserExportWorkflow>();
    auto& flow = *state.browserExport;
    flow.Initialize(*state.project);
    ImGui::SeparatorText("Browser game (Experimental)");
#if !defined(__linux__) || defined(__ANDROID__)
    ImGui::TextWrapped("Browser compilation currently requires a Linux source checkout. Native export remains available above.");
    return;
#endif
    ImGui::TextWrapped("Export a saved, script-owned game for WebGL2. Requires the Linux source checkout, Emscripten 6.0.13 and matching Release tools. Nothing is installed or published automatically. Checks and builds use saved project files.");
    const bool busy = flow.job.Running() || flow.preview.Running();
    ImGui::BeginDisabled(busy);
    Field("New browser package folder (outside the project)", flow.destination);
    if (ImGui::Button("Choose parent folder...")) requests.browseBrowserExport = true;
    ImGui::TextDisabled("Choose a parent, then append a new folder name. Existing packages are preserved.");
    if (ImGui::CollapsingHeader("Toolchain and build options")) {
        Field("Emscripten SDK root", flow.sdk);
        Field("Browser build cache", flow.cache);
        Field("Native Release exporter", flow.exporter);
        Field("Native Release runtime", flow.runtime);
        Field("Native ICU build (optional; otherwise prepared automatically)", flow.icu);
        Field("Browser compiler script", flow.script);
        Field("Python executable", flow.python);
        ImGui::SliderInt("Compiler jobs", &flow.jobs, 1, 8);
    }
    if (ImGui::Button("Check prerequisites") && state.mode == EditorMode::Edit) flow.Start(*state.project, true);
    ImGui::SameLine();
    if (ImGui::Button("Build browser game") && state.mode == EditorMode::Edit) requests.exportBrowser = true;
    ImGui::EndDisabled();
    if (flow.job.Running()) {
        ImGui::SameLine(); if (ImGui::Button("Cancel build/check")) flow.job.Stop();
        ImGui::TextWrapped("The editor remains responsive. Packaging uses saved files; do not modify saved assets during export. The first engine build can take several minutes.");
    }
    if (!flow.operation.empty()) {
        ImGui::TextWrapped("%s: %s", flow.operation.c_str(), flow.job.Status().c_str());
        if (!flow.job.Log().empty()) ImGui::TextWrapped("Log: %s", flow.job.Log().string().c_str());
        for (const auto& check : flow.checks) {
            ImGui::PushStyleColor(ImGuiCol_Text, check.first ? ImVec4(.3f,.8f,.65f,1) : ImVec4(1,.55f,.4f,1));
            ImGui::TextWrapped("%s %s", check.first ? "Ready:" : "Fix:", check.second.c_str());
            ImGui::PopStyleColor();
        }
        if (ImGui::CollapsingHeader("Build/check output", flow.job.Running() ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            ImGui::BeginChild("Browser tool output", {0, 160}, true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(flow.job.Output().c_str()); ImGui::EndChild();
        }
    }
    ImGui::BeginDisabled(flow.job.Running());
    if (!flow.preview.Running()) {
        ImGui::SetNextItemWidth(140);
        if (ImGui::InputInt("Local preview port", &flow.previewPort)) flow.previewPort = std::clamp(flow.previewPort, 0, 65535);
    }
    if (!flow.preview.Running()) {
        if (ImGui::Button("Preview package locally")) {
            flow.preview.Start(flow.python, {(fs::path(flow.script).parent_path() / "serve_browser.py").string(),
                flow.destination, "--port", std::to_string(flow.previewPort)}, NewLog(), std::chrono::seconds(0));
        }
    } else {
        if (!flow.previewURL.empty()) {
            if (ImGui::Button("Open game in browser")) SDL_OpenURL(flow.previewURL.c_str());
            ImGui::SameLine(); ImGui::TextUnformatted(flow.previewURL.c_str());
        }
        if (ImGui::Button("Stop local preview")) flow.preview.Stop();
    }
    ImGui::EndDisabled();
    if (!flow.preview.Status().empty()) {
        ImGui::TextWrapped("Local preview: %s", flow.preview.Status().c_str());
        if (!flow.preview.Running()) ImGui::TextWrapped("%s", flow.preview.Output().c_str());
    }
    ImGui::TextWrapped("Preview checks package integrity and serves only on this computer with the required isolation headers. Closing the editor stops its preview. Browser saves belong to the URL/port/profile; keep this port fixed to retain the same save origin. Port 0 chooses a fresh available port.");
    if (ImGui::Button("Browser export guide")) requests.openExternalPath = ResolveEngineDataPath("docs/BROWSER_SUPPORT.md");
}
