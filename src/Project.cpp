#include "Project.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::string Quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

// Tokenizer shared in spirit with the scene format: whitespace-separated,
// quoted strings with \" and \\ escapes, # comments.
bool Tokenize(const std::string& line, std::vector<std::pair<std::string, bool>>& out, std::string& error) {
    out.clear();
    std::size_t i = 0;
    while (i < line.size()) {
        const char c = line[i];
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }
        if (c == '#') break;
        if (c == '"') {
            std::string text;
            ++i;
            bool closed = false;
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) { text += line[i + 1]; i += 2; continue; }
                if (line[i] == '"') { closed = true; ++i; break; }
                text += line[i++];
            }
            if (!closed) { error = "unterminated string"; return false; }
            out.emplace_back(text, true);
            continue;
        }
        std::string text;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '#') text += line[i++];
        out.emplace_back(text, false);
    }
    return true;
}

std::string Generic(const fs::path& p) { return p.generic_string(); }
}  // namespace

std::string Project::SerializeToString(const ProjectSettings& s) {
    std::string out = "JudasProject " + std::to_string(kProjectFormatVersion) + "\n";
    out += "name " + Quote(s.name) + "\n";
    out += "startup-scene " + Quote(s.startupScene) + "\n";
    out += "assets-dir " + Quote(s.assetsDir) + "\n";
    out += "scenes-dir " + Quote(s.scenesDir) + "\n";
    out += "saves-dir " + Quote(s.savesDir) + "\n";
    return out;
}

bool Project::ParseFromString(const std::string& text, ProjectSettings& outSettings, std::string& outError) {
    std::istringstream stream(text);
    std::string line;
    std::size_t lineNumber = 0;
    bool headerSeen = false;
    std::map<std::string, std::string> values;
    while (std::getline(stream, line)) {
        ++lineNumber;
        std::vector<std::pair<std::string, bool>> tokens;
        std::string error;
        if (!Tokenize(line, tokens, error)) {
            outError = "project line " + std::to_string(lineNumber) + ": " + error;
            return false;
        }
        if (tokens.empty()) continue;
        if (!headerSeen) {
            if (tokens.size() != 2 || tokens[0].first != "JudasProject" || tokens[0].second) {
                outError = "project line " + std::to_string(lineNumber) + ": expected 'JudasProject <version>'";
                return false;
            }
            if (tokens[1].first != std::to_string(kProjectFormatVersion)) {
                outError = "project line " + std::to_string(lineNumber) + ": unsupported project version " +
                           tokens[1].first + " (this build reads version " + std::to_string(kProjectFormatVersion) + ")";
                return false;
            }
            headerSeen = true;
            continue;
        }
        if (tokens.size() != 2 || tokens[0].second || !tokens[1].second) {
            outError = "project line " + std::to_string(lineNumber) + ": expected 'key \"value\"'";
            return false;
        }
        if (values.count(tokens[0].first)) {
            outError = "project line " + std::to_string(lineNumber) + ": duplicate key '" + tokens[0].first + "'";
            return false;
        }
        values[tokens[0].first] = tokens[1].first;
    }
    if (!headerSeen) {
        outError = "project file is empty or has no 'JudasProject <version>' header";
        return false;
    }
    ProjectSettings s;
    const char* required[] = {"name", "startup-scene", "assets-dir", "scenes-dir", "saves-dir"};
    for (const char* key : required) {
        if (!values.count(key)) {
            outError = "project file is missing required key '" + std::string(key) + "'";
            return false;
        }
    }
    for (const auto& [key, value] : values) {
        if (key == "name") s.name = value;
        else if (key == "startup-scene") s.startupScene = value;
        else if (key == "assets-dir") s.assetsDir = value;
        else if (key == "scenes-dir") s.scenesDir = value;
        else if (key == "saves-dir") s.savesDir = value;
        else {
            outError = "project file has unknown key '" + key + "'";
            return false;
        }
    }
    for (const std::string* dir : {&s.assetsDir, &s.scenesDir, &s.savesDir}) {
        if (dir->empty() || fs::path(*dir).is_absolute() || dir->find("..") != std::string::npos) {
            outError = "project directories must be non-empty relative paths inside the project";
            return false;
        }
    }
    if (!s.startupScene.empty() && (fs::path(s.startupScene).is_absolute() || s.startupScene.find("..") != std::string::npos)) {
        outError = "startup-scene must be a project-relative path";
        return false;
    }
    outSettings = s;
    return true;
}

bool Project::Load(const std::string& projectFilePath, std::string& outError) {
    std::ifstream file(projectFilePath, std::ios::binary);
    if (!file) {
        outError = "could not open project file: " + projectFilePath;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    ProjectSettings settings;
    if (!ParseFromString(buffer.str(), settings, outError)) {
        outError = projectFilePath + ": " + outError;
        return false;
    }
    m_settings = settings;
    m_projectFile = Generic(fs::absolute(projectFilePath).lexically_normal());
    m_rootDir = Generic(fs::path(m_projectFile).parent_path());
    return true;
}

bool Project::Save(std::string& outError) const {
    if (m_projectFile.empty()) {
        outError = "project has no file path";
        return false;
    }
    std::ofstream file(m_projectFile, std::ios::binary | std::ios::trunc);
    if (!file) {
        outError = "could not write project file: " + m_projectFile;
        return false;
    }
    file << SerializeToString(m_settings);
    return static_cast<bool>(file);
}

bool Project::SaveAs(const std::string& projectFilePath, std::string& outError) {
    m_projectFile = Generic(fs::absolute(projectFilePath).lexically_normal());
    m_rootDir = Generic(fs::path(m_projectFile).parent_path());
    return Save(outError);
}

bool Project::CreateNew(const std::string& rootDir, const std::string& name, Project& outProject, std::string& outError) {
    if (name.empty()) {
        outError = "a project needs a name";
        return false;
    }
    std::error_code ec;
    fs::create_directories(rootDir, ec);
    if (ec) {
        outError = "could not create project directory: " + rootDir;
        return false;
    }
    for (const auto& entry : fs::directory_iterator(rootDir, ec)) {
        if (entry.path().extension() == kProjectFileExtension) {
            outError = "directory already contains a project: " + Generic(entry.path());
            return false;
        }
    }
    Project project;
    project.m_settings = ProjectSettings{};
    project.m_settings.name = name;
    std::string fileStem;
    for (char c : name) fileStem += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
    const fs::path file = fs::path(rootDir) / (fileStem + kProjectFileExtension);
    fs::create_directories(fs::path(rootDir) / project.m_settings.assetsDir, ec);
    fs::create_directories(fs::path(rootDir) / project.m_settings.scenesDir, ec);
    fs::create_directories(fs::path(rootDir) / project.m_settings.savesDir, ec);
    if (!project.SaveAs(Generic(file), outError)) return false;
    outProject = project;
    return true;
}

std::string Project::FindProjectFileFor(const std::string& startPath) {
    std::error_code ec;
    fs::path dir = fs::absolute(startPath, ec);
    if (!fs::is_directory(dir, ec)) dir = dir.parent_path();
    while (!dir.empty()) {
        std::vector<fs::path> found;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == kProjectFileExtension) found.push_back(entry.path());
        }
        if (found.size() == 1) return Generic(found[0].lexically_normal());
        if (found.size() > 1) return std::string();
        const fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return std::string();
}

std::string Project::Resolve(const std::string& projectRelative) const {
    if (projectRelative.empty()) return std::string();
    if (fs::path(projectRelative).is_absolute()) return Generic(fs::path(projectRelative).lexically_normal());
    return Generic((fs::path(m_rootDir) / projectRelative).lexically_normal());
}

std::string Project::AssetsDir() const { return Resolve(m_settings.assetsDir); }
std::string Project::ScenesDir() const { return Resolve(m_settings.scenesDir); }
std::string Project::SavesDir() const { return Resolve(m_settings.savesDir); }
std::string Project::StartupScenePath() const {
    return m_settings.startupScene.empty() ? std::string() : Resolve(m_settings.startupScene);
}

std::string Project::MakeRelative(const std::string& path) const {
    std::error_code ec;
    const fs::path absolute = fs::absolute(path, ec).lexically_normal();
    const fs::path root = fs::path(m_rootDir).lexically_normal();
    const fs::path relative = absolute.lexically_relative(root);
    if (relative.empty() || relative.string().rfind("..", 0) == 0) return Generic(absolute);
    return Generic(relative);
}

std::string Project::WorldStatePathForScene(const std::string& scenePath) const {
    const std::string stem = fs::path(scenePath).stem().string();
    if (stem.empty()) return std::string();
    return Generic(fs::path(SavesDir()) / (stem + ".judasstate"));
}
