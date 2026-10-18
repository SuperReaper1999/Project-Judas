// Retained positions must subtract the double-precision origin delta before
// narrowing back to float. Check actual native endpoints after two cold region
// restores, using the 512 m anchors required by RequestOrigin.
#include "AssetDatabase.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SceneSerialization.h"
#include "WorldStreaming.h"

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace {
namespace fs = std::filesystem;
unsigned checks = 0;
unsigned failures = 0;

void Check(bool condition, const std::string& description) {
    ++checks;
    if (!condition) ++failures;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", description.c_str());
    std::fflush(stdout);
}

void Require(bool condition, const std::string& description) {
    Check(condition, description);
    if (!condition) throw std::runtime_error(description);
}

// Only remove a directory this instance successfully created. Existing paths
// are never adopted, cleared or reused, including on an exceptional exit.
class OwnedTemporaryDirectory {
public:
    OwnedTemporaryDirectory() {
        const auto base = fs::temp_directory_path();
        std::random_device random;
        for (unsigned attempt = 0; attempt < 64; ++attempt) {
            const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto name = "judas-stream-origin-precision-" + std::to_string(ticks) + "-" +
                              std::to_string(random()) + "-" + std::to_string(attempt);
            auto candidate = base / name;
            std::error_code error;
            if (fs::create_directory(candidate, error)) {
                m_path = std::move(candidate);
                return;
            }
            if (error && error != std::errc::file_exists)
                throw fs::filesystem_error("create test directory", candidate, error);
        }
        throw std::runtime_error("cannot reserve a unique temporary test directory");
    }

    ~OwnedTemporaryDirectory() {
        if (!m_path.empty()) {
            std::error_code ignored;
            fs::remove_all(m_path, ignored);
        }
    }

    OwnedTemporaryDirectory(const OwnedTemporaryDirectory&) = delete;
    OwnedTemporaryDirectory& operator=(const OwnedTemporaryDirectory&) = delete;

    const fs::path& Path() const { return m_path; }

private:
    fs::path m_path;
};

uint32_t FloatBits(float value) {
    uint32_t result;
    static_assert(sizeof(result) == sizeof(value), "native position test requires binary32 size");
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

bool SameBits(glm::vec3 first, glm::vec3 second) {
    return FloatBits(first.x) == FloatBits(second.x) &&
           FloatBits(first.y) == FloatBits(second.y) &&
           FloatBits(first.z) == FloatBits(second.z);
}

void PrintPosition(const char* name, glm::vec3 position) {
    std::printf("%s decimal=%.9g,%.9g,%.9g bits=%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32 "\n",
                name, double(position.x), double(position.y), double(position.z),
                FloatBits(position.x), FloatBits(position.y), FloatBits(position.z));
}

bool Pump(WorldStreaming& stream, ResourceManager& resources,
          const std::function<bool()>& complete) {
    for (unsigned iteration = 0; iteration < 6000; ++iteration) {
        resources.Pump();
        stream.Advance(false);
        if (complete()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

struct RetainedArchive {
    unsigned version;
    std::string bytes;
};

struct StreamedWorld {
    RuntimeWorld world;
    std::unique_ptr<WorldStreaming> stream;

    StreamedWorld(ResourceManager& resources, const Project& project,
                  const WorldManifest& manifest, glm::dvec3 origin,
                  unsigned reservedIdentities = 0) {
        world.legacyGameplay = false;
        Scene root;
        root.Settings().worldOrigin = origin;
        std::string error;
        const bool built = world.Build(root, &resources, error);
        Require(built, "build fresh public native world: " + error);
        for (unsigned index = 0; index < reservedIdentities; ++index)
            (void)world.AllocateRuntimeEntityId();
        stream = std::make_unique<WorldStreaming>(world, resources, project, manifest);
    }

    uint64_t Activate(ResourceManager& resources) {
        std::string error;
        const auto token = stream->Request("box", false, error);
        const bool reached = token != 0 && Pump(*stream, resources, [&] {
            const auto status = stream->Status(token);
            return status && (status->state == "active" || status->state == "failed");
        });
        Require(reached, "asynchronous retained region reaches publication: " + error);
        const auto status = stream->Status(token);
        Require(status && status->state == "active",
                "actual rigid region is active" + (status ? ": " + status->error : ""));
        return token;
    }

    EntityId Actor() const { return stream->Resolve("box", 1); }

    void Suspend(ResourceManager& resources, uint64_t token) {
        Require(stream->Release(token), "release actual region demand");
        const bool unloaded = Pump(*stream, resources, [&] {
            return stream->Regions().front().state == "unloaded";
        });
        Require(unloaded, "region captures actual rigid state and unregisters");
    }

    RetainedArchive Capture() {
        const auto version = stream->ArchiveVersion();
        SaveArchive archive;
        stream->Persist(archive, version);
        return {version, std::move(archive.bytes)};
    }

    void Restore(const RetainedArchive& retained) {
        SaveArchive archive(retained.bytes);
        stream->Persist(archive, retained.version);
        archive.Finish();
    }
};

void CheckRoundTrips(ResourceManager& resources, const Project& project,
                    const WorldManifest& manifest, glm::dvec3 captureOrigin,
                    glm::dvec3 requestedDelta) {
    const std::string label = requestedDelta == glm::dvec3(0)
                                  ? "zero-origin-delta"
                                  : "one-anchor-quantum-delta";
    try {
        StreamedWorld writer(resources, project, manifest, captureOrigin);
        auto token = writer.Activate(resources);
        const auto writerEntity = writer.Actor();
        const auto native = writer.world.RuntimeBody(writerEntity);
        Require(native.IsValid(), label + ": actual native writer body exists");

        const glm::vec3 authored(.0001f, 1.2f, -.00017f);
        const auto saved = writer.world.Physics().GetTransform(native);
        const auto linear = writer.world.Physics().GetLinearVelocity(native);
        const auto angular = writer.world.Physics().GetAngularVelocity(native);
        Check(SameBits(saved.position, authored),
              label + ": initial large-origin authored local coordinates preserve exact float bits");
        Check(writer.world.Physics().AliveBodyCount() == 1,
              label + ": only the ordinary rigid box exists in native physics");

        writer.Suspend(resources, token);
        Check(!writer.world.Physics().IsBodyEnabled(native) &&
                  writer.world.Physics().AliveBodyCount() == 0,
              label + ": suspended writer retains no live physics body");
        const auto targetOrigin = captureOrigin + requestedDelta;
        Require(targetOrigin - captureOrigin == requestedDelta,
                label + ": chosen supported origin delta is exactly representable");
        std::string error;
        const bool committed = writer.world.RequestOrigin(targetOrigin, error) &&
                               writer.world.CommitOrigin();
        Require(committed,
                label + ": native origin transaction commits while region is suspended: " + error);
        const auto publishedOrigin = writer.world.Settings().worldOrigin;
        Require(publishedOrigin == targetOrigin,
                label + ": actual published origin equals requested supported anchor before serialization");
        const auto actualDelta = publishedOrigin - captureOrigin;

        // This harness directly owns the coordinator. SceneSession sends this
        // same notification using the committed delta in an attached game.
        writer.stream->ShiftOrigin(actualDelta);
        const auto retained = writer.Capture();
        Check(!retained.bytes.empty(),
              label + ": suspended state is serialized through real public archive");
        Require(actualDelta == glm::dvec3(0) || retained.version >= 7,
                label + ": changed-origin retained state uses a capture-origin-aware archive");
        std::printf("COMMITTED_ARCHIVE case=%s version=%u published=%.17g,%.17g,%.17g bytes=%zu\n",
                    label.c_str(), retained.version, publishedOrigin.x, publishedOrigin.y,
                    publishedOrigin.z, retained.bytes.size());

        StreamedWorld reader(resources, project, manifest, publishedOrigin, 7);
        reader.Restore(retained);
        Check(reader.world.Physics().AliveBodyCount() == 0,
              label + ": cold metadata decode grants no native bodies before activation");
        token = reader.Activate(resources);
        const auto readerEntity = reader.Actor();
        const auto handle = reader.world.RuntimeBody(readerEntity);
        Require(handle.IsValid() && reader.world.Physics().IsBodyEnabled(handle),
                label + ": fresh restored native body is enabled");
        Check(readerEntity != writerEntity,
              label + ": retained state remaps a fresh runtime entity identity");

        const auto expected = glm::vec3(glm::dvec3(saved.position) -
                                        (publishedOrigin - captureOrigin));
        const auto actual = reader.world.Physics().GetTransform(handle);
        std::printf("ORIGIN_PRECISION case=%s capture=%.17g,%.17g,%.17g target=%.17g,%.17g,%.17g\n",
                    label.c_str(), captureOrigin.x, captureOrigin.y, captureOrigin.z,
                    targetOrigin.x, targetOrigin.y, targetOrigin.z);
        PrintPosition("saved_actual_native", saved.position);
        PrintPosition("required_delta_result", expected);
        PrintPosition("restored_actual_native", actual.position);
        Check(SameBits(actual.position, expected),
              label + ": actual restored native position equals required delta operation bit for bit");
        if (actualDelta == glm::dvec3(0))
            Check(SameBits(actual.position, saved.position),
                  label + ": exact zero-delta round trip preserves original native position bits");

        EntityPhysicalState physical;
        Require(reader.world.GetEntityState(readerEntity, physical),
                label + ": cold reconstructed entity exposes actual physical state");
        Check(SameBits(physical.position, expected),
              label + ": cold entity physical state agrees with required native position");
        Check(SameBits(reader.world.Physics().GetLinearVelocity(handle), linear) &&
                  SameBits(reader.world.Physics().GetAngularVelocity(handle), angular),
              label + ": actual native velocities are preserved without any physical step");

        // Capture again and inspect another real endpoint. Metadata equality
        // alone cannot show that the second native reconstruction is correct.
        reader.Suspend(resources, token);
        const auto secondArchive = reader.Capture();
        StreamedWorld second(resources, project, manifest, publishedOrigin, 19);
        second.Restore(secondArchive);
        const auto secondToken = second.Activate(resources);
        (void)secondToken;
        const auto secondHandle = second.world.RuntimeBody(second.Actor());
        Require(secondHandle.IsValid(), label + ": second cold retained body actually exists");
        const auto secondPose = second.world.Physics().GetTransform(secondHandle);
        PrintPosition("second_restored_actual_native", secondPose.position);
        Check(SameBits(secondPose.position, expected),
              label + ": second cold retained round trip preserves the same required position bits");
    } catch (const std::exception& error) {
        Check(false, label + ": case did not complete actual endpoint verification");
        std::fprintf(stderr, "PRECISION_CASE_EXCEPTION %s: %s\n", label.c_str(), error.what());
    }
}
}  // namespace

int main() {
    try {
        OwnedTemporaryDirectory temporary;
        Project project;
        std::string error;
        const bool created = Project::CreateNew((temporary.Path() / "project").string(),
                                                "ordinary-origin-precision", project, error);
        Require(created, "create isolated ordinary precision project: " + error);
        project.Settings().legacyGameplay = false;
        const bool projectSaved = project.Save(error);
        Require(projectSaved, "save script-owned execution policy: " + error);

        Scene region;
        auto& object = region.CreateObject("ordinary precision box");
        object.transform.position = {.0001f, 1.2f, -.00017f};
        object.body = SceneBodyComponent{};
        object.body->motion = SceneBodyMotion::Dynamic;
        object.body->halfExtents = {.05f, .08f, .09f};
        object.body->initialLinearVelocity = {.125f, -.25f, .375f};
        object.body->restitution = 0;
        const bool sceneSaved = SaveSceneToFile(region, project.Resolve("Scenes/box.judas"), error);
        Require(sceneSaved, "write ordinary nonbinary-position regional source: " + error);

        AssetDatabase assets;
        assets.Scan(project.RootDir(), project.AssetsDir());
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        const glm::dvec3 captureOrigin(1e12, -1e12, 1e12);
        WorldManifest manifest;
        manifest.installMilliseconds = 10;
        manifest.unitsPerFrame = 32;
        WorldRegion description;
        description.id = "box";
        description.scene = "Scenes/box.judas";
        description.origin = captureOrigin;
        manifest.regions.emplace(description.id, description);
        const bool valid = ValidateWorldManifest(manifest, project, error);
        Require(valid, "validate ordinary public region source: " + error);

        CheckRoundTrips(resources, project, manifest, captureOrigin, glm::dvec3(0));
        CheckRoundTrips(resources, project, manifest, captureOrigin, glm::dvec3(-512, 512, -512));
        std::printf("SUMMARY %u checks %u failures\n", checks, failures);
        return failures ? 1 : 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ORDINARY_ORIGIN_TEST_EXCEPTION %s\n", error.what());
        return 1;
    }
}
