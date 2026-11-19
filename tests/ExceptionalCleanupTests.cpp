#include "AudioSystem.h"
#include "GltfLoader.h"
#include "JobSystem.h"
#include "TextureLoader.h"
#include "miniaudio.h"

#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <new>
#include <stdexcept>
#include <vector>

namespace {
struct AllocationProbe {
    std::size_t cppSize = 0, cSize = 0;
    void* owned = nullptr;
    bool injected = false, released = false;
};
thread_local AllocationProbe probe;
std::atomic<unsigned> decoderCloses{0};

void Check(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
    std::printf("PASS %s\n", description);
}

std::vector<unsigned char> Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Check(bool(file), path.string().c_str());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

}

#ifdef JUDAS_TRACK_NATIVE_CLEANUP
extern "C" void* __real_malloc(std::size_t);
extern "C" void __real_free(void*);
extern "C" ma_result __real_ma_decoder_uninit(ma_decoder*);

extern "C" void* __wrap_malloc(std::size_t size) {
    void* allocation = __real_malloc(size);
    if (probe.cSize && size == probe.cSize && !probe.injected) {
        probe.owned = allocation;
        probe.released = false;
    }
    return allocation;
}
extern "C" void __wrap_free(void* allocation) {
    if (allocation && allocation == probe.owned) {
        probe.owned = nullptr;
        if (probe.injected) probe.released = true;
    }
    __real_free(allocation);
}
extern "C" ma_result __wrap_ma_decoder_uninit(ma_decoder* decoder) {
    ++decoderCloses;
    return __real_ma_decoder_uninit(decoder);
}
#endif

void* operator new(std::size_t size) {
    if (size == probe.cppSize && size && (!probe.cSize || probe.owned)) {
        probe.cppSize = 0;
        probe.injected = true;
        throw std::bad_alloc();
    }
    if (auto* data = std::malloc(size ? size : 1)) return data;
    throw std::bad_alloc();
}
void operator delete(void* data) noexcept { std::free(data); }
void operator delete(void* data, std::size_t) noexcept { std::free(data); }

namespace {
void CheckDecoderCloses(unsigned expected, const char* description) {
#ifdef JUDAS_TRACK_NATIVE_CLEANUP
    Check(decoderCloses.load() == expected, description);
#else
    (void)expected;
    (void)description;
#endif
}

void CheckReleased(const char* description) {
    const auto result = probe;
    probe = {};
    Check(result.injected, "targeted C++ allocation failed");
#ifdef JUDAS_TRACK_NATIVE_CLEANUP
    Check(result.released && !result.owned, description);
#else
    (void)description;
#endif
}

void CheckDecodeFailures(const std::filesystem::path& fixtures, const std::filesystem::path& wavPath) {
    const auto png = Read(fixtures / "engine-assets/textures/beacon.png");
    const auto wav = Read(wavPath);
    const auto gltf = Read(fixtures / "m66/material-uv.gltf");
    TextureData texture;
    AudioData audio;
    MeshData mesh;
    std::string error;
    bool threw = false;

    constexpr std::size_t rgbaSize = 256 * 256 * 4;
#ifdef JUDAS_TRACK_NATIVE_CLEANUP
    probe = {rgbaSize, rgbaSize};
#else
    probe = {rgbaSize};
#endif
    try { DecodeTextureFromMemory(png.data(), png.size(), "fixture", texture, error); }
    catch (const std::bad_alloc&) { threw = true; }
    CheckReleased("stb released its decoded RGBA buffer during unwinding");
    Check(threw && DecodeTextureFromMemory(png.data(), png.size(), "fixture", texture, error) &&
          texture.pixels.size() == rgbaSize, "texture allocation failure propagates and a later decode succeeds");

    const auto closes = decoderCloses.load();
    probe = {96000 * sizeof(float)};
    threw = false;
    try { DecodeAudioFromMemory(wav.data(), wav.size(), audio, error); }
    catch (const std::bad_alloc&) { threw = true; }
    const auto failedPcm = probe.injected;
    probe = {};
    Check(threw && failedPcm && audio.samples.empty(), "PCM allocation failure propagates without publishing output");
    CheckDecoderCloses(closes + 1, "initialized memory decoder uninitializes exactly once");
    Check(DecodeAudioFromMemory(wav.data(), wav.size(), audio, error) && audio.Frames() == 96000,
          "WAV decoding recovers after allocation failure");

    // The first data URI in material-uv.gltf decodes to 72 encoded bytes.
#ifdef JUDAS_TRACK_NATIVE_CLEANUP
    probe = {72, 72};
#else
    probe = {72};
#endif
    const bool success = ParseGltfMesh(gltf.data(), gltf.size(), mesh, error);
    CheckReleased("cgltf released the owned data URI allocation during unwinding");
    Check(!success, "glTF maintains its decode-error contract on allocation failure");
    Check(ParseGltfMesh(gltf.data(), gltf.size(), mesh, error) && !mesh.materials.empty() &&
          mesh.materials.front().maps[0].encodedImage.size() == 72,
          "glTF data URI decoding recovers after allocation failure");
}

enum class RetirementCase { Worker, Rejected, Cancelled };

void CheckRetirement(RetirementCase scenario, const std::string& wavPath) {
    JobSystem jobs(1);
    AudioSystem audio;
    audio.SetJobSystem(&jobs);
    std::string error;
    Check(audio.Init(error, true), "device-free mixer initializes");
    AudioSettings settings;
    settings.loading = AudioLoading::Streamed;
    const auto voice = audio.CreateStreamVoice(wavPath, settings, error);
    jobs.WaitAll();
    AudioVoiceSnapshot snapshot;
    Check(voice.IsValid() && audio.Snapshot(voice, snapshot) && snapshot.ready,
          "decode worker finished and published real PCM");
    const auto closes = decoderCloses.load();
    audio.DestroyVoice(voice);

    if (scenario == RetirementCase::Rejected) {
        jobs.Shutdown();  // Subsequent Submit genuinely rejects the job.
        audio.Update();
    } else if (scenario == RetirementCase::Cancelled) {
        std::promise<void> release;
        auto gate = release.get_future();
        const auto blocker = jobs.Submit([&gate](JobContext&) { gate.wait(); });
        struct Unblock {
            std::promise<void>& release;
            JobSystem& jobs;
            JobHandle blocker;
            ~Unblock() { release.set_value(); jobs.Wait(blocker); }
        } unblock{release, jobs, blocker};
        audio.Update();  // The single worker cannot begin the queued cleanup.
        const JobHandle cleanup{blocker.id + 1};
        Check(jobs.NameOf(cleanup) == "audio stream retirement" && jobs.Cancel(cleanup),
              "retirement was cancelled while queued");
        CheckDecoderCloses(closes, "cancelled worker has not run the decoder destructor");
        audio.Update();  // Must finish while the unrelated worker is still held.
    } else {
        audio.Update();
        jobs.WaitAll();
        audio.Update();
    }

    CheckDecoderCloses(closes + 1, "retiring the stream uninitializes its decoder exactly once");
    Check(audio.Diagnostics().pendingRetirements == 0 && audio.Diagnostics().streamBytes == 0,
          "retirement returns the stream slot and PCM pages");
    audio.Shutdown();
    CheckDecoderCloses(closes + 1, "audio shutdown does not double-close the decoder");
}
}

int main(int argc, char** argv) {
    try {
        const std::filesystem::path fixtures = argc > 1 ? argv[1] : "tests/fixtures";
        const auto wavPath = fixtures / "engine-assets/audio/loop.wav";
        CheckDecodeFailures(fixtures, wavPath);
        CheckRetirement(RetirementCase::Worker, wavPath.string());
        CheckRetirement(RetirementCase::Rejected, wavPath.string());
        CheckRetirement(RetirementCase::Cancelled, wavPath.string());
        return 0;
    } catch (const std::exception& error) {
        probe = {};
        std::fprintf(stderr, "FAIL %s\n", error.what());
        return 1;
    }
}
