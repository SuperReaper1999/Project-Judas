#include "editor/EditorApplication.h"
#include "editor/EditorPanels.h"
#include "editor/EditorPreferences.h"
#include "imgui.h"
#include <SDL2/SDL.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
unsigned checks = 0, failures = 0;

void Check(bool condition, const char *message) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}

class Environment {
  public:
    Environment(const char *name, const std::string &value) : m_name(name) {
        if (const char *previous = std::getenv(name)) {
            m_existed = true;
            m_original = previous;
        }
        if (SDL_setenv(name, value.c_str(), 1) != 0)
            throw std::runtime_error(std::string("Cannot isolate environment: ") + name);
    }
    ~Environment() {
        if (m_existed)
            SDL_setenv(m_name.c_str(), m_original.c_str(), 1);
        else {
#if defined(_WIN32)
            _putenv_s(m_name.c_str(), "");
#else
            unsetenv(m_name.c_str());
#endif
        }
    }
    Environment(const Environment &) = delete;
    Environment &operator=(const Environment &) = delete;

  private:
    std::string m_name, m_original;
    bool m_existed = false;
};

struct Fixture {
    fs::path root = fs::temp_directory_path() /
                    ("judas-editor-lifecycle-" + std::to_string(
                         std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() {
        if (!fs::create_directory(root))
            throw std::runtime_error("Cannot create unique lifecycle fixture");
    }
    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
};

bool Inside(const fs::path &path, const fs::path &root) {
    const auto normalizedPath = path.lexically_normal();
    const auto normalizedRoot = root.lexically_normal();
    auto actual = normalizedPath.begin();
    for (const auto &part : normalizedRoot) {
        if (actual == normalizedPath.end() || *actual != part)
            return false;
        ++actual;
    }
    return true;
}

void Write(const fs::path &path, const std::string &bytes) {
    std::ofstream file(path, std::ios::binary);
    if (!file || !file.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot write lifecycle fixture: " + path.string());
}

using Files = std::map<std::string, std::string>;
Files ReadFiles(const fs::path &directory) {
    Files result;
    for (const auto &entry : fs::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file())
            continue;
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot read lifecycle fixture: " + entry.path().string());
        result.emplace(fs::relative(entry.path(), directory).generic_string(),
                       std::string(std::istreambuf_iterator<char>(file), {}));
    }
    return result;
}

int Run(EditorApplication &editor, const std::string &argument) {
    std::array<std::string, 2> arguments{"judas_editor_lifecycle_tests", argument};
    std::array<char *, 2> argv{arguments[0].data(), arguments[1].data()};
    return editor.Run(static_cast<int>(argv.size()), argv.data());
}

void ProjectFailure(const fs::path &project, const fs::path &preferences,
                    const Files &originalPreferences) {
    Uint32 windowId = 0;
    {
        // One application per lifetime. Run is deliberately a one-shot API.
        EditorApplication editor;
        Check(Run(editor, project.string()) == 1, "invalid project returns failure");
        Check(ImGui::GetCurrentContext() != nullptr,
              "project failure occurs after actual editor UI initialization");
        if (ImGui::GetCurrentContext()) {
            const auto &io = ImGui::GetIO();
            Check(io.BackendPlatformUserData && io.BackendRendererUserData,
                  "project failure exercises both initialized native UI backends");
        }
        SDL_Window *window = SDL_GL_GetCurrentWindow();
        windowId = window ? SDL_GetWindowID(window) : 0;
        Check(windowId != 0 && SDL_GL_GetCurrentContext() != nullptr,
              "project failure exercises a real native window and GL context");
    }
    Check(ImGui::GetCurrentContext() == nullptr, "application destruction retires ImGui context");
    Check(SDL_GL_GetCurrentContext() == nullptr && SDL_GL_GetCurrentWindow() == nullptr,
          "application destruction retires current GL context and window");
    Check(windowId != 0 && SDL_GetWindowFromID(windowId) == nullptr,
          "application destruction removes its native SDL window");
    Check(ReadFiles(preferences) == originalPreferences,
          "failed startup preserves preference and layout bytes without new files or backups");
}
} // namespace

int main() {
    try {
        Fixture fixture;
        Environment data("XDG_DATA_HOME", (fixture.root / "data").string());
        Environment config("XDG_CONFIG_HOME", (fixture.root / "config").string());
        Environment cache("XDG_CACHE_HOME", (fixture.root / "cache").string());
        Environment automation("JUDAS_EDITOR_AUTOTEST", "1");

        // SDL owns the platform-specific choice. Refuse to run if it did not
        // honor isolation, rather than ever loading or writing real preferences.
        char *directory = SDL_GetPrefPath("Judas", "Editor");
        if (!directory)
            throw std::runtime_error(std::string("Cannot resolve isolated preferences: ") + SDL_GetError());
        const fs::path preferences(directory);
        SDL_free(directory);
        const bool isolated = Inside(preferences, fixture.root);
        Check(isolated, "SDL preferences resolve inside the isolated fixture");
        if (!isolated)
            throw std::runtime_error("Platform did not honor isolated XDG preference paths; editor not started");

        EditorPreferences seed;
        EditorPanelState panelState;
        seed.automaticUpdates = false;
        // Extra newlines detect even a semantically equivalent unsolicited save.
        // Serialization captures ImGui layout state, so fixture creation needs
        // its own context. It must be retired before testing application startup.
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        const auto preferenceBytes = seed.Serialize(panelState) + "\n\n";
        ImGui::DestroyContext();
        Write(preferences / "workspace.json", preferenceBytes);
        Write(preferences / "windows.ini", "; lifecycle fixture: failed startup must not save a layout\n");
        const auto originalPreferences = ReadFiles(preferences);
        Check(ImGui::GetCurrentContext() == nullptr && SDL_GL_GetCurrentContext() == nullptr,
              "fixture starts without an editor or GL context");

        {
            EditorApplication editor;
            Check(Run(editor, "-ExpBrowserSupport=invalid") == 1,
                  "malformed launch option rejects before host initialization");
            Check(ImGui::GetCurrentContext() == nullptr && SDL_GL_GetCurrentContext() == nullptr &&
                      SDL_GL_GetCurrentWindow() == nullptr,
                  "pre-host rejection creates no UI, GL context or native window");
        }
        Check(ReadFiles(preferences) == originalPreferences,
              "pre-host rejection leaves isolated preferences untouched");

        const auto missing = fixture.root / "missing.judasproj";
        Check(!fs::exists(missing), "missing-project negative control is actually absent");
        ProjectFailure(missing, preferences, originalPreferences);

        const auto malformed = fixture.root / "malformed.judasproj";
        Write(malformed, "{ not a valid project\n");
        // A second distinct application exposes stale backend global state left
        // by the first failed lifetime, and reaches a different project failure.
        ProjectFailure(malformed, preferences, originalPreferences);
    } catch (const std::exception &error) {
        Check(false, error.what());
    }
    std::printf("Editor application lifecycle: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
