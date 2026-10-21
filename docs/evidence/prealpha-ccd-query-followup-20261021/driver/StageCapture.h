#pragma once
// Private diagnostic reader of COMPLETED public profiler snapshots. Optional;
// outside fixed-step spans, with bounded frames/records. Does not change physics.
#include "PerformanceProfiler.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace stage_capture {
class Session {
  public:
    Session(const std::filesystem::path& output, bool enabled)
        : directory_(output), enabled_(enabled) {
        if (enabled_) {
            std::filesystem::create_directories(output);
            rows_.open(output / "stage-frames.jsonl");
            if (!rows_)
                enabled_ = false;
        }
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() { Stop(); }

    // Application has opened this frame but has not ended it. Its predecessor
    // was published to profiler history at the end of the previous outer loop.
    void AtFrameStart(unsigned benchmarkFrame) {
        if (!enabled_ || benchmarkFrame == 0)
            return;
        const auto current = PerformanceProfiler::Get().FrameId();
        if (current > 0)
            Capture(current - 1, benchmarkFrame - 1, current, "next-beforeFrame");
    }
    // beforeShutdown is called after the final loop's ProfileFrame destructor.
    void AtShutdown(unsigned completedBenchmarkFrames) {
        if (!enabled_ || completedBenchmarkFrames == 0)
            return;
        const auto latest = PerformanceProfiler::Get().FrameId();
        Capture(latest, completedBenchmarkFrames - 1, latest, "beforeShutdown-after-EndFrame");
        Stop();
    }
    void Stop() {
        if (stopped_)
            return;
        stopped_ = true;
        if (!enabled_)
            return;
        rows_.flush();
        nlohmann::json metadata = {
            {"schema", "judas.private.completed-profiler-frames.v1"},
            {"captured_frames", captured_}, {"missing_completed_snapshots", missing_},
            {"capacity_dropped_frames", dropped_}, {"max_frames", kMaxFrames},
            {"scope", "Raw existing profiler records; elapsed spans on named threads overlap; inclusive rows are not additive."},
            {"publication", "Current afterFrame is not published; next-beforeFrame reads previous FrameId minus one, shutdown reads final ended frame."},
            {"timing_qualification", "Snapshot copying/JSON output occurs outside fixed-step timer but perturbs frame work and live cadence; never substitute for uninstrumented performance."},
            {"worker_qualification", "Only work already instrumented/registered by existing profiler appears; absence is not zero worker CPU."},
            {"fixed_ids", "Profiler step IDs and benchmark steps are separate identities; absolute monotonic boundaries and benchmark frame link are preserved."}};
        std::ofstream(directory_ / "stage-metadata.json") << metadata.dump(2) << '\n';
    }

  private:
    static constexpr std::uint64_t kMaxFrames = 4096;
    void Capture(std::uint64_t id, unsigned benchmarkFrame, std::uint64_t readerFrame,
                 const char* publication) {
        if (!id || id == lastId_)
            return;
        if (captured_ >= kMaxFrames) {
            ++dropped_;
            return;
        }
        const auto frame = PerformanceProfiler::Get().Snapshot(id);
        if (frame.id != id || frame.end < frame.start || !frame.end) {
            ++missing_;
            return;
        }
        lastId_ = id;
        nlohmann::json record = {
            {"benchmark_frame", benchmarkFrame}, {"profile_frame", frame.id},
            {"read_while_profile_frame", readerFrame}, {"publication", publication},
            {"world_boundary", frame.world}, {"main_thread", frame.mainThread},
            {"start_ns", frame.start}, {"end_ns", frame.end}, {"interval_ns", frame.interval},
            {"collection_ns", frame.collectionNanoseconds}, {"covered_ns", frame.coverage},
            {"waiting_ns", frame.waiting}, {"unattributed_ns", frame.unattributed},
            {"mode", frame.mode.c_str()}, {"startup", frame.startup},
            {"paused", frame.paused}, {"incomplete", frame.incomplete},
            {"cap_reached", frame.capReached}, {"discarded_seconds", frame.discardedSeconds},
            {"accumulator_seconds", frame.accumulatorSeconds},
            {"resident_bytes", frame.processResidentBytes}, {"virtual_bytes", frame.processVirtualBytes},
            {"profiler_reserved_bytes", frame.profilerReservedBytes},
            {"gpu_available", frame.gpuAvailable}, {"gpu_status", frame.gpuStatus.c_str()},
            {"threads", nlohmann::json::array()}, {"fixed_steps", nlohmann::json::array()},
            {"scopes", nlohmann::json::array()}, {"counters", nlohmann::json::array()},
            {"gpu", nlohmann::json::array()}, {"boundaries", nlohmann::json::array()}};
        for (const auto& thread : frame.threads)
            record["threads"].push_back({{"id", thread.id}, {"name", thread.name.c_str()}});
        for (const auto& step : frame.fixed)
            record["fixed_steps"].push_back({{"id", step.id}, {"start_ns", step.start},
                                              {"end_ns", step.end}, {"simulation_seconds", step.simulationSeconds}});
        for (const auto& scope : frame.scopes)
            record["scopes"].push_back({{"node", scope.node}, {"parent", scope.parent},
                {"depth", scope.depth}, {"thread", scope.thread}, {"origin_frame", scope.originFrame},
                {"name", scope.name.c_str()}, {"start_ns", scope.start}, {"end_ns", scope.end},
                {"inclusive_ns", scope.end - scope.start}, {"exclusive_ns", scope.exclusive}, {"wait", scope.wait}});
        for (const auto& counter : frame.counters)
            record["counters"].push_back({{"name", counter.name.c_str()}, {"value", counter.value},
                {"observed_ns", counter.observed}, {"mode", unsigned(counter.mode)}});
        for (const auto& gpu : frame.gpu)
            record["gpu"].push_back({{"token", gpu.token}, {"frame", gpu.frame},
                {"camera", gpu.camera}, {"pass", gpu.pass.c_str()}, {"pending", gpu.pending}, {"milliseconds", gpu.milliseconds}});
        for (const auto& boundary : frame.boundaries)
            record["boundaries"].push_back(boundary.c_str());
        const auto& diagnostics = frame.diagnostics;
        record["diagnostics"] = {{"dropped_events", diagnostics.droppedEvents},
            {"exhausted_labels", diagnostics.exhaustedLabels}, {"exhausted_nodes", diagnostics.exhaustedNodes},
            {"exhausted_threads", diagnostics.exhaustedThreads}, {"incomplete_scopes", diagnostics.incompleteScopes},
            {"truncated_frames", diagnostics.truncatedFrames}, {"gpu_dropped", diagnostics.gpuDropped},
            {"nested_frames", diagnostics.nestedFrames}, {"open_scopes", diagnostics.openScopes}};
        rows_ << record.dump() << '\n';
        rows_.flush();
        ++captured_;
    }
    std::filesystem::path directory_;
    std::ofstream rows_;
    bool enabled_ = false, stopped_ = false;
    std::uint64_t captured_ = 0, dropped_ = 0, missing_ = 0, lastId_ = 0;
};
} // namespace stage_capture
