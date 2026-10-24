#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Editor-only filesystem picker. Choosing a path never creates or overwrites it.
class EditorFileBrowser {
public:
    enum class Mode { OpenFile, SaveFile, Directory };
    enum class Target { Document, NewProject, Import, Export };
    struct Entry { std::string name; bool directory=false; };
    struct Selection { Target target; std::string path; };
    void Open(Target, Mode, const std::string& path, std::string extension={});
    std::optional<Selection> Draw();
    bool Active() const { return m_active; }
    static std::vector<Entry> List(const std::filesystem::path&, const std::string& extension,
                                   std::string& error);
    static bool Select(const std::filesystem::path&, const std::string& name, Mode,
                       const std::string& extension, std::string& path, std::string& error);
private:
    Target m_target=Target::Document;
    Mode m_mode=Mode::OpenFile;
    std::filesystem::path m_directory;
    std::string m_name, m_extension, m_error, m_location, m_listingError;
    std::vector<Entry> m_entries;
    bool m_refresh=true;
    bool m_open=false, m_active=false;
};
