#include "PlatformServices.h"
#include <atomic>
#include <cstdlib>
#include <cwchar>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace fs = std::filesystem;
const char* RuntimeExecutableName() {
#ifdef _WIN32
    return "judas.exe";
#else
    return "judas";
#endif
}
const char* RuntimePlatformName() {
#ifdef _WIN32
    return "Windows";
#else
    return "Linux";
#endif
}
std::string PlatformUserDataDirectory(std::string& error) {
#ifdef _WIN32
    PWSTR path = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &path);
    if (FAILED(result)) { error = "Cannot locate writable Windows LocalAppData"; return {}; }
    const std::string root = fs::path(path).generic_u8string();
    CoTaskMemFree(path);
    return root;
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    fs::path root;
    if (xdg && *xdg) root = xdg;
    else if (home && *home) root = fs::path(home) / ".local/share";
    if (root.empty() || !root.is_absolute()) { error = "Runtime requires an absolute XDG_DATA_HOME or HOME for writable saves"; return {}; }
    return root.generic_string();
#endif
}
#ifdef _WIN32
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
    Handle(const Handle&) = delete;
};
std::wstring Wide(const std::string& text) {
    if (text.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!count) throw std::runtime_error("Invalid UTF-8 process argument");
    std::wstring result(size_t(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), result.data(), count);
    return result;
}
// Windows CRT argv quoting: backslashes are doubled before quotes and the closing quote.
std::wstring Quote(const std::wstring& text) {
    std::wstring result = L"\""; size_t slashes = 0;
    for (wchar_t ch : text) {
        if (ch == L'\\') { ++slashes; continue; }
        result.append(ch == L'\"' ? slashes * 2 + 1 : slashes, L'\\'); slashes = 0;
        result += ch;
    }
    result.append(slashes * 2, L'\\'); result += L'\"'; return result;
}
std::wstring Command(const fs::path& executable, const std::vector<std::string>& args) {
    std::wstring result = Quote(executable.wstring());
    for (const auto& arg : args) result += L" " + Quote(Wide(arg));
    return result;
}
}
bool WindowsLaunchProcess(const fs::path& executable, const std::vector<std::string>& args, std::string& error) {
    try {
        auto command = Command(executable, args); STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        // Preserve the editor's existing rule: its automation hook is not inherited
        // by a separately launched game. Build a Unicode child environment.
        std::vector<wchar_t> environment;
        LPWCH inherited = GetEnvironmentStringsW();
        if (!inherited) throw std::runtime_error("Cannot read child environment");
        for (const wchar_t* entry = inherited; *entry; entry += wcslen(entry) + 1) {
            if (wcsncmp(entry, L"JUDAS_EDITOR_AUTOTEST=", 22) == 0) continue;
            environment.insert(environment.end(), entry, entry + wcslen(entry) + 1);
        }
        FreeEnvironmentStringsW(inherited);
        environment.push_back(L'\0');
        if (environment.size() == 1) environment.push_back(L'\0');
        if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT, environment.data(), nullptr, &startup, &process)) {
            error = "CreateProcess failed (Windows error " + std::to_string(GetLastError()) + ")"; return false;
        }
        CloseHandle(process.hThread); CloseHandle(process.hProcess); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool WindowsCaptureBuildInfo(const fs::path& executable, std::string& output) {
    output.clear(); SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE}; HANDLE readPipe, writePipe;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) return false;
    Handle reader(readPipe), writer(writePipe);
    if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) return false;
    Handle nullInput(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr));
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe; startup.hStdError = writePipe; startup.hStdInput = nullInput.value;
    auto command = Command(executable, {"--build-info"}); PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return false;
    Handle child(process.hProcess), thread(process.hThread); CloseHandle(writer.value); writer.value = INVALID_HANDLE_VALUE;
    bool success = true; ULONGLONG deadline = GetTickCount64() + 5000;
    for (;;) {
        DWORD available = 0;
        if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available) {
            char data[256]; DWORD received = 0;
            if (!ReadFile(readPipe, data, sizeof(data), &received, nullptr) || output.size() + received > 1024) { success = false; break; }
            output.append(data, received); continue;
        }
        if (WaitForSingleObject(process.hProcess, 10) == WAIT_OBJECT_0) {
            // The child can publish stdout between the first peek and its exit.
            // Drain those final bytes before accepting the build identity.
            DWORD remaining = 0;
            if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &remaining, nullptr) && remaining) continue;
            break;
        }
        if (GetTickCount64() >= deadline) { success = false; break; }
    }
    if (!success) { TerminateProcess(process.hProcess, 1); WaitForSingleObject(process.hProcess, 5000); return false; }
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
    // CRT text stdout uses CRLF on Windows.
    for (size_t at = 0; (at = output.find('\r', at)) != std::string::npos;) output.erase(at, 1);
    return code == 0;
}
#endif
std::string ImportFileStamp(const fs::path& path) {
#ifdef _WIN32
    Handle file(CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    BY_HANDLE_FILE_INFORMATION info{}; FILE_BASIC_INFO basic{};
    if (file.value == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(file.value, &info) ||
        !GetFileInformationByHandleEx(file.value, FileBasicInfo, &basic, sizeof(basic))) throw std::runtime_error("Cannot stat import input: " + path.u8string());
    return std::to_string(info.dwVolumeSerialNumber) + ":" + std::to_string(info.nFileIndexHigh) + ":" +
        std::to_string(info.nFileIndexLow) + ":" + std::to_string(info.nFileSizeHigh) + ":" + std::to_string(info.nFileSizeLow) + ":" +
        std::to_string(basic.LastWriteTime.QuadPart) + ":" + std::to_string(basic.ChangeTime.QuadPart);
#else
    struct stat value{};
    if (::stat(path.c_str(), &value)) throw std::runtime_error("cannot stat import input: " + path.string());
    return std::to_string(value.st_dev)+":"+std::to_string(value.st_ino)+":"+std::to_string(value.st_size)+":"+
        std::to_string(value.st_mtim.tv_sec)+":"+std::to_string(value.st_mtim.tv_nsec)+":"+
        std::to_string(value.st_ctim.tv_sec)+":"+std::to_string(value.st_ctim.tv_nsec);
#endif
}
std::string CreateImportStagingFile(const fs::path& output) {
    fs::create_directories(output.parent_path());
#ifdef _WIN32
    static std::atomic<unsigned long long> next{0};
    for (int attempt = 0; attempt < 128; ++attempt) {
        auto path = fs::u8path(output.u8string() + ".import-staging-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(next.fetch_add(1)));
        Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (file.value != INVALID_HANDLE_VALUE) return path.u8string();
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    throw std::runtime_error("Cannot create owned import staging file");
#else
    std::string path = output.string() + ".import-staging-XXXXXX";
    int fd = mkstemp(path.data()); if (fd < 0) throw std::runtime_error("cannot create owned import staging file");
    close(fd); return path;
#endif
}
void ReplaceStagedFile(const fs::path& source, const fs::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace staged file (Windows error " + std::to_string(GetLastError()) + ")");
#else
    fs::rename(source, destination);
#endif
}

std::string CreateExportStagingDirectory(const fs::path& parent) {
#ifdef _WIN32
    static std::atomic<unsigned long long> next{0};
    for (int attempt = 0; attempt < 128; ++attempt) {
        auto path = parent / (".judas-export-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(next.fetch_add(1)));
        if (CreateDirectoryW(path.c_str(), nullptr)) return path.u8string();
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    throw std::runtime_error("Cannot create export staging directory");
#else
    std::string pattern = (parent / ".judas-export-XXXXXX").string();
    if (!mkdtemp(pattern.data())) throw std::runtime_error("Cannot create export staging directory");
    return pattern;
#endif
}
