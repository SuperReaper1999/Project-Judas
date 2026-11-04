#include "EditorContext.h"
#include "EditorDocument.h"
#include "EditorProjectWorkflow.h"
#include "EnginePaths.h"
#include "PlatformServices.h"
#include "Project.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

bool EditorProjectWorkflow::RunProject(EditorContext &context, std::string &outMessage) {
    if (!context.project.IsLoaded()) {
        outMessage = "no project open";
        return false;
    }
    Project saved;
    if (!saved.Load(context.project.ProjectFile(), outMessage))
        return false;
    if (saved.Settings().startupScene.empty()) {
        outMessage = "the saved project has no startup scene (Project settings)";
        return false;
    }
#ifdef _WIN32
    const auto runtime = fs::u8path(EngineExecutableDir()) / RuntimeExecutableName();
    if (!fs::is_regular_file(runtime)) {
        outMessage = "runtime not found beside editor: " + runtime.u8string();
        return false;
    }
    if (!WindowsLaunchProcess(runtime, {context.project.ProjectFile()}, outMessage))
        return false;
    outMessage = "Launched " + runtime.u8string();
    return true;
#elif defined(__unix__) || defined(__APPLE__)
    // The runtime binary sits beside the editor binary in every build tree.
    const std::string runtime = EngineExecutableDir() + "/judas";
    std::error_code ec;
    if (!fs::is_regular_file(runtime, ec)) {
        outMessage = "runtime not found beside the editor: " + runtime;
        return false;
    }
    // The child resolves engine data (the UI font) the way this editor
    // did; pass that root on explicitly so it never depends on the CWD.
    const std::string fontPath = ResolveEngineDataPath("assets/fonts/DejaVuSans.ttf");
    const std::string engineRoot =
        fs::path(fontPath).parent_path().parent_path().parent_path().generic_string();
    std::vector<std::string> envStrings;
    for (char **e = environ; e && *e; ++e) {
        if (std::strncmp(*e, "JUDAS_ENGINE_ROOT=", 18) == 0)
            continue;
        // The editor's own automation hook must not leak into the game.
        if (std::strncmp(*e, "JUDAS_EDITOR_AUTOTEST=", 22) == 0)
            continue;
        envStrings.push_back(*e);
    }
    envStrings.push_back("JUDAS_ENGINE_ROOT=" + engineRoot);
    std::vector<char *> envp;
    for (std::string &s : envStrings)
        envp.push_back(s.data());
    envp.push_back(nullptr);
    std::string argv0 = runtime, argv1 = context.project.ProjectFile();
    char *argv[] = {argv0.data(), argv1.data(), nullptr};
    pid_t pid = 0;
    const int result = posix_spawn(&pid, runtime.c_str(), nullptr, nullptr, argv, envp.data());
    if (result != 0) {
        outMessage = "could not launch the runtime: " + std::string(std::strerror(result));
        return false;
    }
    outMessage = "Launched " + runtime + " " + context.project.ProjectFile() + " (pid " +
                 std::to_string(pid) + ") on " + context.project.Settings().startupScene;
    return true;
#else
    outMessage =
        "Run project launches ./judas as a separate process; not implemented on this platform. Run: judas " +
        context.project.ProjectFile();
    return false;
#endif
}
