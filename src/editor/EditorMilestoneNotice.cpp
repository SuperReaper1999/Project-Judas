#include "EditorMilestoneNotice.h"
#include "JudasEditorMilestone.h"
#include "../../third_party/nlohmann/json.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <regex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace {
constexpr const char* endpoint = "repos/SuperReaper1999/Project-Judas/commits?sha=main&per_page=100";
constexpr const char* selection = "[.[]|{sha:.sha,title:(.commit.message|split(\"\\n\")[0])}]";

// Run only a read-only API request. Bound time/output and terminate only our
// own child process on shutdown. Missing CLI/authentication stays non-blocking.
bool ReadMilestones(const std::atomic<bool>& cancelled, std::string& output) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE reader = nullptr, writer = nullptr;
    if (!CreatePipe(&reader, &writer, &sa, 0)) return false;
    SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);
    HANDLE nullFile = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW start{}; start.cb = sizeof(start);
    start.dwFlags = STARTF_USESTDHANDLES;
    start.hStdOutput = writer; start.hStdError = nullFile; start.hStdInput = nullFile;
    PROCESS_INFORMATION process{};
    // Quote using the Windows CRT rules (including backslashes before quotes).
    const auto quote = [](const char* argument) {
        std::wstring value = L"\"";
        unsigned slashes = 0;
        for (const char* c = argument; *c; ++c) {
            if (*c == '\\') { ++slashes; continue; }
            if (*c == '"') { value.append(slashes * 2 + 1, L'\\'); value += L'"'; }
            else { value.append(slashes, L'\\'); value += static_cast<wchar_t>(*c); }
            slashes = 0;
        }
        value.append(slashes * 2, L'\\'); value += L'"';
        return value;
    };
    std::wstring command = L"gh.exe api " + quote(endpoint) + L" --jq " + quote(selection);
    const bool launched = CreateProcessW(nullptr, command.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &start, &process) != FALSE;
    CloseHandle(writer);
    if (nullFile != INVALID_HANDLE_VALUE) CloseHandle(nullFile);
    if (!launched) { CloseHandle(reader); return false; }
    bool completed = false;
    while (!cancelled && std::chrono::steady_clock::now() < deadline && output.size() <= 65536) {
        DWORD bytes = 0;
        if (PeekNamedPipe(reader, nullptr, 0, nullptr, &bytes, nullptr) && bytes) {
            char buffer[4096]; DWORD count = 0;
            if (ReadFile(reader, buffer, (std::min)(bytes, DWORD(sizeof(buffer))), &count, nullptr))
                output.append(buffer, count);
        } else if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) {
            completed = true; break;
        } else std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!completed) TerminateProcess(process.hProcess, 1);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(reader); CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return completed && code == 0;
#else
    int pipes[2];
    if (pipe(pipes) != 0) return false;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipes[0]);
    posix_spawn_file_actions_addclose(&actions, pipes[1]);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);
    const char* args[] = {"gh", "api", endpoint, "--jq", selection, nullptr};
    pid_t child = 0;
    const int error = posix_spawnp(&child, "gh", &actions, &attributes,
        const_cast<char**>(args), environ);
    posix_spawn_file_actions_destroy(&actions); posix_spawnattr_destroy(&attributes);
    close(pipes[1]);
    if (error != 0) { close(pipes[0]); return false; }
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    bool completed = false; int status = 0;
    while (!cancelled && std::chrono::steady_clock::now() < deadline && output.size() <= 65536) {
        char buffer[4096]; const auto count = read(pipes[0], buffer, sizeof(buffer));
        if (count > 0) { output.append(buffer, static_cast<std::size_t>(count)); continue; }
        if (waitpid(child, &status, WNOHANG) == child) {
            // Drain bytes written just before exit.
            while (true) {
                const auto remaining = read(pipes[0], buffer, sizeof(buffer));
                if (remaining <= 0 || output.size() > 65536) break;
                output.append(buffer, static_cast<std::size_t>(remaining));
            }
            completed = true; break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!completed) { kill(-child, SIGKILL); waitpid(child, &status, 0); }
    close(pipes[0]);
    return completed && output.size() <= 65536 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}
}

EditorMilestoneNotice::~EditorMilestoneNotice() {
    m_cancelled = true;
    if (m_pending.valid()) m_pending.wait();
}

EditorMilestoneNotice::Result EditorMilestoneNotice::Query(const std::atomic<bool>& cancelled) {
    Result result;
    std::string response;
    if (!ReadMilestones(cancelled, response)) return result;
    const auto records = nlohmann::json::parse(response, nullptr, false);
    if (!records.is_array()) return result;
    const std::regex milestoneTitle(R"(^(Add|Implement|Complete|Checkpoint) M[0-9]+\b|^M[0-9]+\b|^Milestone [0-9]+\b)");
    const std::regex number(R"(\bM([0-9]+)\b|\bMilestone ([0-9]+)\b)");
    const std::regex shaPattern("^[0-9a-f]{40}$");
    for (const auto& record : records) {
        if (!record.is_object() || !record.contains("title") || !record["title"].is_string()
            || !record.contains("sha") || !record["sha"].is_string()) continue;
        const auto title = record["title"].get<std::string>();
        const auto sha = record["sha"].get<std::string>();
        if (!std::regex_search(title, milestoneTitle) || !std::regex_match(sha, shaPattern)) continue;
        for (auto i = std::sregex_iterator(title.begin(), title.end(), number); i != std::sregex_iterator(); ++i) {
            const auto digits = (*i)[1].matched ? (*i)[1].str() : (*i)[2].str();
            if (digits.size() > 4) continue;
            const int milestone = std::stoi(digits);
            if (milestone > result.milestone) { result.milestone = milestone; result.sha = sha; }
        }
    }
    result.available = result.milestone > 0;
    return result;
}

void EditorMilestoneNotice::Check(bool manual) {
    if (m_pending.valid()) { m_manual = m_manual || manual; return; }
    m_manual = manual;
    m_pending = std::async(std::launch::async, [this] {
        try { return Query(m_cancelled); } catch (...) { return Result{}; }
    });
}

void EditorMilestoneNotice::Draw(std::string& status) {
    if (m_pending.valid() && m_pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_result = m_pending.get();
        m_visible = m_result.available && m_result.milestone > JUDAS_EDITOR_MILESTONE;
        if (m_manual) status = m_visible ? "New Judas milestone available."
            : m_result.available ? "No newer milestone found on main."
            : "Milestone check unavailable (offline, or GitHub CLI not installed/signed in).";
    }
    if (!m_visible) return;
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 20,
        viewport->WorkPos.y + 75), ImGuiCond_Appearing, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(310, 0), ImGuiCond_Always);
    if (ImGui::Begin("New milestone available", &m_visible,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Judas M%d is available", m_result.milestone);
        ImGui::TextWrapped("This editor was built through M%d.", JUDAS_EDITOR_MILESTONE);
        if (ImGui::Button("View milestone")) {
            const auto url = "https://github.com/SuperReaper1999/Project-Judas/commit/" + m_result.sha;
            SDL_OpenURL(url.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Dismiss")) m_visible = false;
    }
    ImGui::End();
}
