#include "ToolProcess.h"
#include "ProcessArguments.h"
#include <algorithm>
#include <cerrno>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#elif !defined(__ANDROID__)
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

std::filesystem::path SystemHttpsProbeExecutable() {
#ifdef _WIN32
    std::wstring directory(32768, L'\0');
    const auto count = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
    if (count == 0 || count >= directory.size()) return {};
    directory.resize(count);
    return std::filesystem::path(directory) / L"curl.exe";
#elif defined(__ANDROID__)
    return {};
#else
    return "curl";
#endif
}

bool CaptureToolOutput(const std::filesystem::path& executable,
                       const std::vector<std::string>& arguments,
                       std::string& output, std::string& error,
                       std::chrono::milliseconds timeout, std::size_t maximumBytes) {
    output.clear();
    error.clear();
    if (timeout.count() <= 0 || maximumBytes == 0) {
        error = "Tool probe requires a positive time/output bound";
        return false;
    }
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    // Native games do not launch desktop exporters or HTTP helper processes.
    // In particular, posix_spawn is unavailable at the supported API-26 floor.
    (void)executable;
    (void)arguments;
#ifdef __EMSCRIPTEN__
    error = "External desktop tool probing is unavailable in the browser runtime";
#else
    error = "External desktop tool probing is unavailable in the Android runtime";
#endif
    return false;
#else
    const auto deadline = std::chrono::steady_clock::now() + timeout;
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE reader = INVALID_HANDLE_VALUE, writer = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&reader, &writer, &security, 0)) {
        error = "Cannot create tool output pipe";
        return false;
    }
    SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);
    HANDLE nullFile = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writer;
    startup.hStdError = nullFile;
    startup.hStdInput = nullFile;
    std::wstring command = QuoteWindowsProcessArgument(executable.wstring());
    for (const auto& argument : arguments)
        command += L" " + QuoteWindowsProcessArgument(std::filesystem::u8path(argument).wstring());
    PROCESS_INFORMATION process{};
    const bool launched = CreateProcessW(nullptr, command.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE;
    CloseHandle(writer);
    if (nullFile != INVALID_HANDLE_VALUE) CloseHandle(nullFile);
    if (!launched) {
        CloseHandle(reader);
        error = "Cannot start tool (Windows error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    bool completed = false;
    for (;;) {
        DWORD available = 0;
        if (PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr) && available) {
            char buffer[4096];
            DWORD count = 0;
            if (!ReadFile(reader, buffer, (std::min)(available, DWORD(sizeof(buffer))), &count, nullptr)) {
                error = "Cannot read tool output";
                break;
            }
            if (output.size() + count > maximumBytes) { error = "Tool output exceeded its bound"; break; }
            output.append(buffer, count);
        } else if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) {
            // The child can write and exit between the first pipe peek and
            // this wait. Drain its final bytes before declaring EOF.
            DWORD remaining = 0;
            if (PeekNamedPipe(reader, nullptr, 0, nullptr, &remaining, nullptr) && remaining)
                continue;
            completed = true;
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) { error = "Tool probe timed out"; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (!completed) TerminateProcess(process.hProcess, 1);
    WaitForSingleObject(process.hProcess, 5000);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(reader);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (completed && code != 0) error = "Tool exited with status " + std::to_string(code);
    for (std::size_t at = 0; (at = output.find('\r', at)) != std::string::npos;) output.erase(at, 1);
    return completed && code == 0;
#else
    int pipes[2];
    if (pipe(pipes) != 0) { error = "Cannot create tool output pipe"; return false; }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipes[0]);
    posix_spawn_file_actions_addclose(&actions, pipes[1]);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    std::vector<std::string> storage{executable.string()};
    storage.insert(storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& argument : storage) argv.push_back(argument.data());
    argv.push_back(nullptr);
    pid_t child = 0;
    const int result = posix_spawnp(&child, storage.front().c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipes[1]);
    if (result != 0) {
        close(pipes[0]);
        error = "Cannot start tool (error " + std::to_string(result) + ")";
        return false;
    }
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    int status = 0;
    bool completed = false;
    for (;;) {
        char buffer[4096];
        const auto count = read(pipes[0], buffer, sizeof(buffer));
        if (count > 0) {
            if (output.size() + std::size_t(count) > maximumBytes) { error = "Tool output exceeded its bound"; break; }
            output.append(buffer, std::size_t(count));
        } else if (!completed) {
            const auto waited = waitpid(child, &status, WNOHANG);
            if (waited == child) { completed = true; continue; }
            if (waited < 0 && errno != EINTR) { error = "Cannot wait for tool"; break; }
        } else break; // Drain bytes published immediately before child exit.
        if (std::chrono::steady_clock::now() >= deadline) { error = "Tool probe timed out"; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (!completed) {
        kill(child, SIGKILL);
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    }
    close(pipes[0]);
    if (!error.empty()) return false;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error = "Tool failed or exited unsuccessfully";
        return false;
    }
    return true;
#endif
#endif
}
