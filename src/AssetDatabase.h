#pragma once

#include <map>
#include <string>
#include <vector>

// Milestone 30: stable asset identity for a project.
//
// Authored content (scenes) refers to assets by AssetId, never by path. An
// AssetId is a 32-hex-digit string minted when the asset is imported and
// written into a sidecar file next to the asset:
//
//   Assets/models/beacon.obj
//   Assets/models/beacon.obj.judasmeta
//       JudasAssetMeta 1
//       id "3f1c…"                (32 hex digits)
//       type mesh                 (mesh | texture | font)
//       source "/where/it/came/from.obj"   (provenance; informational)
//
// The sidecar travels with the file: rename or move both and the id — and
// therefore every scene reference — is unchanged. The database is a scan
// of the project's assets directory for sidecars; it holds id -> current
// path and reports, rather than guessing at, every inconsistency: a
// duplicate id, a sidecar whose asset is gone, a corrupt sidecar. Assets
// without a sidecar are listed as untracked (importable) and are not
// referenceable until imported.
//
// This is the smallest identity layer Judas needs today, kept deliberately
// short of an asset pipeline: no cooking, no dependency graph, no
// streaming. Formats are exactly the ones the engine can consume
// (ModelLoader: .obj; TextureLoader: .png/.jpg/.bmp/.tga; fonts: .ttf).
enum class AssetType { Mesh, Texture, Font };

using AssetId = std::string;

struct AssetRecord {
    AssetId id;
    AssetType type = AssetType::Mesh;
    std::string path;          // absolute (root-joined) path of the asset file
    std::string relativePath;  // project-relative, generic slashes
    std::string source;        // provenance recorded at import
    bool missing = false;      // sidecar present, asset file absent
};

struct AssetProblem {
    std::string path;
    std::string message;
};

constexpr int kAssetMetaFormatVersion = 1;
constexpr const char* kAssetMetaExtension = ".judasmeta";

const char* AssetTypeName(AssetType type);
// Type from a file extension the engine can load, or false.
bool AssetTypeForExtension(const std::string& extension, AssetType& outType);
bool IsValidAssetId(const AssetId& id);
AssetId MintAssetId();

class AssetDatabase {
public:
    // Rescans `assetsDir` recursively. Problems (duplicate ids, corrupt
    // sidecars) are collected, not fatal: the database stays usable for
    // every consistent asset and the editor shows the rest.
    void Scan(const std::string& projectRoot, const std::string& assetsDir);

    const std::map<AssetId, AssetRecord>& Records() const { return m_records; }
    const AssetRecord* Find(const AssetId& id) const;
    const AssetRecord* FindByRelativePath(const std::string& relativePath) const;
    const std::vector<std::string>& Untracked() const { return m_untracked; }
    const std::vector<AssetProblem>& Problems() const { return m_problems; }
    const std::string& AssetsDir() const { return m_assetsDir; }

    // Validates `sourcePath` with the engine's own loader for its type,
    // copies it to <assetsDir>/<destinationRelative> (a file path relative
    // to the assets directory; "" keeps the source file name at the root),
    // writes the sidecar, and registers it. Fails cleanly on unsupported
    // or invalid input without leaving a half-imported file behind.
    bool Import(const std::string& sourcePath, const std::string& destinationRelative, AssetRecord& outRecord,
                std::string& outError);
    // Adopts an untracked file already inside the assets directory (writes
    // its sidecar in place). Used for the shipped demo tree and for files
    // dropped in by hand.
    bool Track(const std::string& assetPath, AssetRecord& outRecord, std::string& outError,
               const AssetId& forcedId = AssetId());
    // Moves/renames an asset AND its sidecar within the assets directory;
    // the id is untouched. `newRelative` is relative to the assets dir.
    bool Move(const AssetId& id, const std::string& newRelative, std::string& outError);
    bool Remove(const AssetId& id, std::string& outError);

    static bool ReadMeta(const std::string& metaPath, AssetId& outId, AssetType& outType, std::string& outSource,
                         std::string& outError);
    static bool WriteMeta(const std::string& metaPath, const AssetId& id, AssetType type, const std::string& source,
                          std::string& outError);
    // Runs the engine loader for the type; false with a message if the
    // file cannot be consumed.
    static bool ValidateAssetFile(const std::string& path, AssetType type, std::string& outError);

private:
    std::string m_projectRoot;
    std::string m_assetsDir;
    std::map<AssetId, AssetRecord> m_records;
    std::vector<std::string> m_untracked;
    std::vector<AssetProblem> m_problems;
};
