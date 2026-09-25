#include "AssetDatabase.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "MeshData.h"
#include "ModelLoader.h"
#include "TextureData.h"
#include "TextureLoader.h"

namespace fs = std::filesystem;

namespace {
std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string Quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}
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
std::string Generic(const fs::path& p) { return p.lexically_normal().generic_string(); }
}  // namespace

const char* AssetTypeName(AssetType type) {
    switch (type) {
        case AssetType::Mesh: return "mesh";
        case AssetType::Texture: return "texture";
        case AssetType::Font: return "font";
    }
    return "mesh";
}

bool AssetTypeForExtension(const std::string& extension, AssetType& outType) {
    const std::string e = Lower(extension);
    if (e == ".obj") { outType = AssetType::Mesh; return true; }
    if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp" || e == ".tga") { outType = AssetType::Texture; return true; }
    if (e == ".ttf") { outType = AssetType::Font; return true; }
    return false;
}

bool IsValidAssetId(const AssetId& id) {
    if (id.size() != 32) return false;
    for (char c : id) {
        if (!std::isxdigit(static_cast<unsigned char>(c)) || std::isupper(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

AssetId MintAssetId() {
    static std::mt19937_64 generator(static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()) ^ 0x9e3779b97f4a7c15ull);
    static const char* hex = "0123456789abcdef";
    AssetId id;
    for (int i = 0; i < 2; ++i) {
        unsigned long long v = generator();
        for (int n = 0; n < 16; ++n) {
            id += hex[v & 0xf];
            v >>= 4;
        }
    }
    return id;
}

bool AssetDatabase::ValidateAssetFile(const std::string& path, AssetType type, std::string& outError) {
    switch (type) {
        case AssetType::Mesh: {
            MeshData data;
            return LoadObjMesh(path, data, outError);
        }
        case AssetType::Texture: {
            TextureData data;
            return LoadTextureFromFile(path, data, outError);
        }
        case AssetType::Font: {
            std::ifstream file(path, std::ios::binary);
            if (!file) { outError = "could not open font file: " + path; return false; }
            char header[4] = {};
            file.read(header, 4);
            // TrueType 'true'/0x00010000, OpenType 'OTTO'.
            const bool ok = (header[0] == 0 && header[1] == 1 && header[2] == 0 && header[3] == 0) ||
                            std::string(header, 4) == "true" || std::string(header, 4) == "OTTO";
            if (!ok) outError = "not a TrueType/OpenType font: " + path;
            return ok;
        }
    }
    return false;
}

bool AssetDatabase::WriteMeta(const std::string& metaPath, const AssetId& id, AssetType type, const std::string& source,
                              std::string& outError) {
    std::ofstream file(metaPath, std::ios::binary | std::ios::trunc);
    if (!file) { outError = "could not write asset metadata: " + metaPath; return false; }
    file << "JudasAssetMeta " << kAssetMetaFormatVersion << "\n";
    file << "id " << Quote(id) << "\n";
    file << "type " << AssetTypeName(type) << "\n";
    file << "source " << Quote(source) << "\n";
    return static_cast<bool>(file);
}

bool AssetDatabase::ReadMeta(const std::string& metaPath, AssetId& outId, AssetType& outType, std::string& outSource,
                             std::string& outError) {
    std::ifstream file(metaPath, std::ios::binary);
    if (!file) { outError = "could not open asset metadata: " + metaPath; return false; }
    std::string line;
    std::size_t lineNumber = 0;
    bool header = false, idSeen = false, typeSeen = false, sourceSeen = false;
    while (std::getline(file, line)) {
        ++lineNumber;
        std::vector<std::pair<std::string, bool>> tokens;
        std::string error;
        if (!Tokenize(line, tokens, error)) { outError = metaPath + ":" + std::to_string(lineNumber) + ": " + error; return false; }
        if (tokens.empty()) continue;
        const auto fail = [&](const std::string& m) { outError = metaPath + ":" + std::to_string(lineNumber) + ": " + m; return false; };
        if (!header) {
            if (tokens.size() != 2 || tokens[0].first != "JudasAssetMeta") return fail("expected 'JudasAssetMeta <version>'");
            if (tokens[1].first != std::to_string(kAssetMetaFormatVersion)) return fail("unsupported metadata version " + tokens[1].first);
            header = true;
            continue;
        }
        if (tokens.size() != 2) return fail("expected 'key value'");
        const std::string& key = tokens[0].first;
        if (key == "id") {
            if (idSeen) return fail("duplicate id");
            if (!tokens[1].second || !IsValidAssetId(tokens[1].first)) return fail("id must be a quoted 32-hex-digit string");
            outId = tokens[1].first;
            idSeen = true;
        } else if (key == "type") {
            if (typeSeen) return fail("duplicate type");
            if (tokens[1].first == "mesh") outType = AssetType::Mesh;
            else if (tokens[1].first == "texture") outType = AssetType::Texture;
            else if (tokens[1].first == "font") outType = AssetType::Font;
            else return fail("type must be mesh, texture or font");
            typeSeen = true;
        } else if (key == "source") {
            if (sourceSeen) return fail("duplicate source");
            if (!tokens[1].second) return fail("source must be a quoted string");
            outSource = tokens[1].first;
            sourceSeen = true;
        } else {
            return fail("unknown key '" + key + "'");
        }
    }
    if (!header) { outError = metaPath + ": empty or missing header"; return false; }
    if (!idSeen || !typeSeen || !sourceSeen) { outError = metaPath + ": missing id, type or source"; return false; }
    return true;
}

void AssetDatabase::Scan(const std::string& projectRoot, const std::string& assetsDir) {
    m_projectRoot = Generic(fs::absolute(projectRoot));
    m_assetsDir = Generic(fs::absolute(assetsDir));
    m_records.clear();
    m_untracked.clear();
    m_problems.clear();
    std::error_code ec;
    if (!fs::is_directory(m_assetsDir, ec)) {
        m_problems.push_back({m_assetsDir, "assets directory does not exist"});
        return;
    }
    std::vector<fs::path> metas, files;
    for (const auto& entry : fs::recursive_directory_iterator(m_assetsDir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == kAssetMetaExtension) metas.push_back(entry.path());
        else files.push_back(entry.path());
    }
    std::sort(metas.begin(), metas.end());
    std::sort(files.begin(), files.end());
    std::map<std::string, bool> tracked;
    for (const fs::path& meta : metas) {
        AssetRecord record;
        std::string error;
        if (!ReadMeta(Generic(meta), record.id, record.type, record.source, error)) {
            m_problems.push_back({Generic(meta), error});
            continue;
        }
        const fs::path assetPath = meta.parent_path() / meta.stem();  // strip .judasmeta
        record.path = Generic(assetPath);
        record.relativePath = Generic(fs::path(record.path).lexically_relative(m_projectRoot));
        record.missing = !fs::is_regular_file(assetPath, ec);
        if (record.missing) m_problems.push_back({Generic(meta), "asset file is missing: " + record.path});
        const auto existing = m_records.find(record.id);
        if (existing != m_records.end()) {
            m_problems.push_back({Generic(meta), "duplicate asset id " + record.id + " (also " + existing->second.path + ")"});
            continue;
        }
        m_records[record.id] = record;
        tracked[record.path] = true;
    }
    for (const fs::path& file : files) {
        AssetType type;
        if (!AssetTypeForExtension(file.extension().string(), type)) continue;
        if (!tracked.count(Generic(file))) m_untracked.push_back(Generic(file));
    }
}

const AssetRecord* AssetDatabase::Find(const AssetId& id) const {
    const auto it = m_records.find(id);
    return it == m_records.end() ? nullptr : &it->second;
}

const AssetRecord* AssetDatabase::FindByRelativePath(const std::string& relativePath) const {
    for (const auto& [id, record] : m_records) {
        if (record.relativePath == relativePath) return &record;
    }
    return nullptr;
}

bool AssetDatabase::Track(const std::string& assetPath, AssetRecord& outRecord, std::string& outError, const AssetId& forcedId) {
    std::error_code ec;
    const fs::path absolute = fs::absolute(assetPath, ec).lexically_normal();
    if (!fs::is_regular_file(absolute, ec)) { outError = "asset file does not exist: " + assetPath; return false; }
    const std::string rel = Generic(absolute.lexically_relative(m_assetsDir));
    if (rel.empty() || rel.rfind("..", 0) == 0) { outError = "asset is not inside the assets directory: " + assetPath; return false; }
    AssetType type;
    if (!AssetTypeForExtension(absolute.extension().string(), type)) {
        outError = "unsupported asset type: " + absolute.extension().string();
        return false;
    }
    if (!ValidateAssetFile(Generic(absolute), type, outError)) return false;
    const AssetId id = forcedId.empty() ? MintAssetId() : forcedId;
    if (!IsValidAssetId(id)) { outError = "invalid asset id"; return false; }
    if (m_records.count(id)) { outError = "asset id already in use: " + id; return false; }
    const std::string metaPath = Generic(absolute) + kAssetMetaExtension;
    if (!WriteMeta(metaPath, id, type, Generic(absolute), outError)) return false;
    AssetRecord record;
    record.id = id;
    record.type = type;
    record.path = Generic(absolute);
    record.relativePath = Generic(absolute.lexically_relative(m_projectRoot));
    record.source = record.path;
    m_records[id] = record;
    m_untracked.erase(std::remove(m_untracked.begin(), m_untracked.end(), record.path), m_untracked.end());
    outRecord = record;
    return true;
}

bool AssetDatabase::Import(const std::string& sourcePath, const std::string& destinationRelative, AssetRecord& outRecord,
                           std::string& outError) {
    std::error_code ec;
    const fs::path source = fs::absolute(sourcePath, ec).lexically_normal();
    if (!fs::is_regular_file(source, ec)) { outError = "source file does not exist: " + sourcePath; return false; }
    AssetType type;
    if (!AssetTypeForExtension(source.extension().string(), type)) {
        outError = "unsupported asset type '" + source.extension().string() + "' (supported: .obj, .png/.jpg/.bmp/.tga, .ttf)";
        return false;
    }
    if (!ValidateAssetFile(Generic(source), type, outError)) return false;
    fs::path destination = fs::path(m_assetsDir) / (destinationRelative.empty() ? source.filename().string() : destinationRelative);
    if (fs::is_directory(destination, ec)) destination /= source.filename();
    destination = destination.lexically_normal();
    if (Generic(destination).rfind(m_assetsDir, 0) != 0) { outError = "destination must be inside the assets directory"; return false; }
    if (fs::exists(destination, ec)) { outError = "destination already exists: " + Generic(destination); return false; }
    fs::create_directories(destination.parent_path(), ec);
    fs::copy_file(source, destination, ec);
    if (ec) { outError = "could not copy asset into the project: " + ec.message(); return false; }
    AssetRecord record;
    const AssetId id = MintAssetId();
    const std::string metaPath = Generic(destination) + kAssetMetaExtension;
    if (!WriteMeta(metaPath, id, type, Generic(source), outError)) {
        fs::remove(destination, ec);
        return false;
    }
    record.id = id;
    record.type = type;
    record.path = Generic(destination);
    record.relativePath = Generic(destination.lexically_relative(m_projectRoot));
    record.source = Generic(source);
    m_records[id] = record;
    outRecord = record;
    return true;
}

bool AssetDatabase::Move(const AssetId& id, const std::string& newRelative, std::string& outError) {
    const auto it = m_records.find(id);
    if (it == m_records.end()) { outError = "unknown asset id " + id; return false; }
    AssetRecord& record = it->second;
    std::error_code ec;
    fs::path destination = (fs::path(m_assetsDir) / newRelative).lexically_normal();
    if (Generic(destination).rfind(m_assetsDir, 0) != 0) { outError = "destination must be inside the assets directory"; return false; }
    if (destination.extension() != fs::path(record.path).extension()) { outError = "moving an asset must keep its file extension"; return false; }
    if (fs::exists(destination, ec)) { outError = "destination already exists: " + Generic(destination); return false; }
    fs::create_directories(destination.parent_path(), ec);
    fs::rename(record.path, destination, ec);
    if (ec) { outError = "could not move asset: " + ec.message(); return false; }
    fs::rename(record.path + kAssetMetaExtension, Generic(destination) + kAssetMetaExtension, ec);
    if (ec) {
        // Keep the pair together: undo the file move.
        fs::rename(destination, record.path, ec);
        outError = "could not move asset metadata";
        return false;
    }
    record.path = Generic(destination);
    record.relativePath = Generic(destination.lexically_relative(m_projectRoot));
    return true;
}

bool AssetDatabase::Remove(const AssetId& id, std::string& outError) {
    const auto it = m_records.find(id);
    if (it == m_records.end()) { outError = "unknown asset id " + id; return false; }
    std::error_code ec;
    fs::remove(it->second.path, ec);
    fs::remove(it->second.path + kAssetMetaExtension, ec);
    m_records.erase(it);
    return true;
}
