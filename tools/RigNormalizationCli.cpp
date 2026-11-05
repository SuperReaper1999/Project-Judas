#include "RigNormalizationCli.h"
#include "RigNormalization.h"
#include "ModelImport.h"
#include "ModelArchive.h"
#include "SceneFingerprint.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int NormalizeRigCommand(const char *input, const char *output) {
    namespace fs = std::filesystem;
    using Json = nlohmann::json;
    fs::path scratch;
    try {
        const fs::path target = fs::u8path(output);
        if (target.extension() != ".judasmodel")
            throw std::runtime_error("output must be a NEW .judasmodel file");
        if (fs::exists(target)) throw std::runtime_error("output exists; original assets are never overwritten");
        MeshData source, converted;
        ModelImportSettings settings;
        ModelImportReport imported;
        RigNormalizationReport normalized;
        std::string error, sourceHash;
        if (!SceneFingerprintSha256File(input, sourceHash, error) ||
            !ImportModelSource(input, settings, source, imported, error) ||
            !NormalizeModelRig(source, converted, normalized, error))
            throw std::runtime_error(error);
        std::vector<uint8_t> bytes;
        if (!EncodeModelArchive(converted, bytes, error)) throw std::runtime_error(error);
        const auto parent = target.has_parent_path() ? target.parent_path() : fs::path(".");
        fs::create_directories(parent);
        scratch = parent / (".rig-normalization-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!fs::create_directory(scratch)) throw std::runtime_error("cannot reserve temporary output directory");
        const auto temporary = scratch / "model";
        {
            std::ofstream file(temporary, std::ios::binary);
            file.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
            file.close();
            if (!file) throw std::runtime_error("normalised model write failed");
        }
        std::string outputHash;
        if (!SceneFingerprintSha256File(temporary.string(), outputHash, error)) throw std::runtime_error(error);
        // Same-filesystem hard link publishes completely written bytes, atomically
        // and without ever replacing an existing destination (including races).
        fs::create_hard_link(temporary, target);
        std::error_code cleanupError;
        fs::remove_all(scratch, cleanupError);
        Json diagnostics = Json::array();
        for (const auto &item : imported.diagnostics)
            diagnostics.push_back({{"severity",item.severity},{"code",item.code},{"message",item.message}});
        std::cout << Json{{"success",true},{"operation","normalize-rig"},
            {"input",input},{"output",output},{"inputSha256",sourceHash},{"outputSha256",outputHash},
            {"meshCoordinateScale",normalized.meshScale},{"posesChecked",normalized.posesChecked},
            {"maximumMatrixError",normalized.maximumMatrixError},
            {"sourceDiagnostics",diagnostics},
            {"warning","Derived rig has new joint coordinate units. Review socket offsets, IK/ragdoll profiles and external animation bindings before adoption; existing projects were not changed."}}.dump(2) << '\n';
        return 0;
    } catch (const std::exception &exception) {
        if (!scratch.empty()) {std::error_code ignored; fs::remove_all(scratch, ignored);}
        std::cout << Json{{"success",false},{"operation","normalize-rig"},{"error",exception.what()}}.dump(2) << '\n';
        return 1;
    }
}
