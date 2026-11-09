#pragma once

#include <string>
#include <functional>
#include <string_view>

class Scene;

// Persisted compatibility scheme. Bump this whenever canonical field coverage,
// ordering or meaning changes. The save format records this independently.
constexpr int kSceneFingerprintVersion = 6;

// SHA-256 over canonical authored data, never runtime state or file paths.
// Schema 6 retires built-in gameplay, PBF and reservoir fields. Older saves
// require their historical engine; no retired-format migration is provided.
// Includes scene/object names, object order (gravity priority), stable
// IDs/NextId, every serialized component and active settings. Inactive gravity
// region alternatives and None-policy distance radii are not serialized and
// therefore do not enter the fingerprint. Signed zero is canonicalized to +0.
// Finite IEEE-754 numbers use fixed-width, big-endian bits; strings use an
// unsigned 64-bit byte length followed by bytes. No locale or std::hash is used.
// On failure outFingerprint is unchanged and outError describes the bad data.
bool ComputeSceneFingerprint(const Scene& scene, std::string& outFingerprint,
                             std::string& outError);

// The portable digest primitive is exposed for independent standard-vector
// tests. Returns 64 lowercase hex digits. This is integrity/identity hashing,
// not authentication of an untrusted save.
std::string SceneFingerprintSha256(std::string_view bytes);

// Same digest, fixed 64 KiB IO buffer; output is unchanged on failure/cancellation.
bool SceneFingerprintSha256File(const std::string& path, std::string& result,
                               std::string& error, const std::function<bool()>& cancelled = {});
