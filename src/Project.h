#pragma once

#include <string>

// Milestone 30: a Judas PROJECT — the unit a game is made in. It sits above
// individual scenes: a root directory, where its assets and scenes live,
// which scene the game starts with, and the handful of settings the runtime
// needs to launch it. The editor opens a project and works within it; the
// runtime launches a project (its startup scene) rather than a scene chosen
// by environment variables.
//
// A project is a directory containing one `<name>.judasproj` file:
//
//   JudasProject 1
//   name "My Game"
//   startup-scene "Scenes/main.judas"     (relative to the project root)
//   assets-dir "Assets"                   (relative to the project root)
//   scenes-dir "Scenes"
//   saves-dir "Saves"
//
// Every path in project and scene data is project-relative; the project
// root is the directory the .judasproj file is in, never the process's
// working directory. The Judas technology demonstration is an ordinary
// project (judas_tech_demo.judasproj at the repository root, pointing at
// the existing assets/ tree); a new game gets Assets/ Scenes/ Saves/ from
// the editor's New Project. Neither layout is assumed by engine code.
struct ProjectSettings {
    std::string name;
    std::string startupScene;  // project-relative scene file
    std::string assetsDir = "Assets";
    std::string scenesDir = "Scenes";
    std::string savesDir = "Saves";
};

constexpr int kProjectFormatVersion = 1;
constexpr const char* kProjectFileExtension = ".judasproj";

class Project {
public:
    // Loads `<root>/<file>.judasproj`. Strict: unknown version/key, missing
    // required key, duplicate key or malformed value fails and leaves this
    // object untouched.
    bool Load(const std::string& projectFilePath, std::string& outError);
    bool Save(std::string& outError) const;
    bool SaveAs(const std::string& projectFilePath, std::string& outError);

    // Creates a new project directory tree with a project file and empty
    // asset/scene/save folders. Fails if `rootDir` already contains a
    // project file.
    static bool CreateNew(const std::string& rootDir, const std::string& name, Project& outProject,
                          std::string& outError);

    // Walks up from `startPath` (a file or directory) to find the nearest
    // directory containing exactly one .judasproj; returns its path or "".
    static std::string FindProjectFileFor(const std::string& startPath);

    bool IsLoaded() const { return !m_projectFile.empty(); }
    const std::string& ProjectFile() const { return m_projectFile; }
    const std::string& RootDir() const { return m_rootDir; }
    ProjectSettings& Settings() { return m_settings; }
    const ProjectSettings& Settings() const { return m_settings; }

    // Absolute-ish paths (root-joined) for the project's folders and files.
    std::string AssetsDir() const;
    std::string ScenesDir() const;
    std::string SavesDir() const;
    std::string StartupScenePath() const;  // "" when no startup scene is set
    std::string Resolve(const std::string& projectRelative) const;
    // The project-relative form of a path under the root (generic slashes),
    // or the input unchanged when it is outside the project.
    std::string MakeRelative(const std::string& path) const;
    // Where a scene's M29 world-state delta lives: <saves-dir>/<stem>.judasstate.
    std::string WorldStatePathForScene(const std::string& scenePath) const;

    static bool ParseFromString(const std::string& text, ProjectSettings& outSettings, std::string& outError);
    static std::string SerializeToString(const ProjectSettings& settings);

private:
    std::string m_projectFile;
    std::string m_rootDir;
    ProjectSettings m_settings;
};
