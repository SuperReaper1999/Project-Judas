#include "AssetBrowserModel.h"
#include <algorithm>
#include <filesystem>
#include <set>

namespace {
std::string SafeRelative(std::filesystem::path value) {
    value = value.lexically_normal();
    if (value.is_absolute())
        return {};
    for (const auto &part : value)
        if (part == "..")
            return {};
    const auto text = value.generic_string();
    return text == "." ? std::string{} : text;
}
} // namespace
std::string EditorAssetBrowserPath(const AssetRecord &record, const std::string &assetsDirectory) {
    namespace fs = std::filesystem;
    if (!record.owner.empty()) {
        auto owner = record.owner;
        std::replace(owner.begin(), owner.end(), '/', '_');
        std::replace(owner.begin(), owner.end(), '\\', '_');
        auto relative = SafeRelative(fs::path(record.relativePath));
        if (relative.empty())
            relative = fs::path(record.path).filename().generic_string();
        return "Mods/" + owner + "/" + relative;
    }
    auto path = SafeRelative(fs::path(record.path).lexically_relative(fs::path(assetsDirectory)));
    if (!path.empty())
        return path;
    // A malformed or out-of-tree record can still be inspected without becoming
    // a navigation escape or masquerading as a writable assets-relative path.
    return "External/" + record.id + "/" + fs::path(record.path).filename().generic_string();
}
std::vector<std::string> EditorAssetBrowserDirectories(const AssetDatabase &assets) {
    std::set<std::string> directories{std::string{}};
    for (const auto &[id, record] : assets.Records()) {
        (void)id;
        auto parent = std::filesystem::path(EditorAssetBrowserPath(record, assets.AssetsDir())).parent_path();
        while (!parent.empty()) {
            directories.insert(parent.generic_string());
            parent = parent.parent_path();
        }
    }
    return {directories.begin(), directories.end()};
}
bool EditorAssetBrowserContains(const std::string &path, const std::string &directory, bool recursive) {
    const auto parent = std::filesystem::path(path).parent_path().generic_string();
    if (parent == directory)
        return true;
    if (!recursive)
        return false;
    return directory.empty() ||
           (parent.size() > directory.size() && parent.compare(0, directory.size(), directory) == 0 &&
            parent[directory.size()] == '/');
}
std::vector<const AssetRecord *> EditorAssetBrowserRecords(const AssetDatabase &assets,
                                                           const std::string &directory, bool recursive,
                                                           int type, bool missingOnly) {
    std::vector<const AssetRecord *> records;
    for (const auto &[id, record] : assets.Records()) {
        (void)id;
        if (type >= 0 && int(record.type) != type)
            continue;
        if (missingOnly && !record.missing)
            continue;
        if (EditorAssetBrowserContains(EditorAssetBrowserPath(record, assets.AssetsDir()), directory,
                                       recursive))
            records.push_back(&record);
    }
    std::sort(records.begin(), records.end(), [](const auto *a, const auto *b) {
        const auto left = std::filesystem::path(a->relativePath).filename().generic_string();
        const auto right = std::filesystem::path(b->relativePath).filename().generic_string();
        return left != right                        ? left < right
               : a->relativePath != b->relativePath ? a->relativePath < b->relativePath
                                                    : a->id < b->id;
    });
    return records;
}
