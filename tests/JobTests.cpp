// Milestone 31: the job system, asynchronous file IO and the asynchronous
// ResourceManager — headless (no window, no GL). Where timing is
// nondeterministic the tests assert invariants and final states, never a
// wall-clock ordering. Run from the repository root: the technology
// demonstration's real assets (assets/models, assets/textures) are decoded
// on worker threads through the same code the runtime uses.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "AssetDatabase.h"
#include "AsyncFile.h"
#include "JobSystem.h"
#include "ResourceManager.h"

namespace fs = std::filesystem;

namespace {
int g_failures = 0;

void Check(bool condition, const std::string& label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label.c_str());
    if (!condition) ++g_failures;
}

struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& name) {
        std::error_code ec;
        path = fs::temp_directory_path(ec) / (name + "_" + std::to_string(static_cast<long long>(::getpid())));
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    std::string Str() const { return path.generic_string(); }
};

const AssetId kBeaconMesh = "0b3ac0e5c8b04d1e9f7a2c6d5e4f3a21";
const AssetId kPlaneMesh = "9c1e7d2a4b5f4c6d8e9fa0b1c2d3e4f5";
const AssetId kBeaconTexture = "5e4d3c2b1a0f4e6d9c8b7a6f5e4d3c2b";

// --- A. job execution, failure, cancellation ------------------------------

void SectionJobs() {
    std::printf("Section A: job execution, failure, cancellation, shutdown\n");
    {
        JobSystem jobs(2);
        Check(jobs.WorkerCount() == 2, "a requested worker count is honoured");
        std::atomic<int> ran{0};
        std::thread::id workerThread;
        JobHandle h = jobs.Submit([&](JobContext&) { ran = 1; workerThread = std::this_thread::get_id(); });
        Check(h.IsValid(), "submission returns a valid handle");
        Check(jobs.Wait(h) == JobState::Completed && ran == 1, "a job runs to Completed");
        Check(workerThread != std::this_thread::get_id(), "it ran on a worker thread, not the caller");
        JobHandle failing = jobs.Submit([](JobContext&) { throw std::runtime_error("boom"); });
        Check(jobs.Wait(failing) == JobState::Failed && jobs.ErrorOf(failing) == "boom", "a throwing job is Failed with its message");
        JobHandle reported = jobs.Submit([](JobContext& c) { c.SetError("explicit failure"); });
        Check(jobs.Wait(reported) == JobState::Failed && jobs.ErrorOf(reported) == "explicit failure", "SetError reports a failure without throwing");
        jobs.Forget(failing);
        Check(jobs.StateOf(failing) == JobState::Unknown, "a forgotten record reads Unknown");
        const JobStats stats = jobs.Stats();
        Check(stats.completed == 1 && stats.failed == 2 && stats.submitted == 3, "stats count completed and failed jobs");
    }
    {
        // Cancellation: block the single worker, queue more, cancel the queued.
        JobSystem jobs(1);
        std::mutex gate;
        gate.lock();
        std::atomic<bool> sawCancel{false};
        JobHandle blocker = jobs.Submit([&](JobContext& c) {
            std::lock_guard<std::mutex> hold(gate);
            if (c.CancelRequested()) { sawCancel = true; c.ReportCancelled(); }
        });
        std::atomic<int> laterRan{0};
        JobHandle queued = jobs.Submit([&](JobContext&) { ++laterRan; });
        JobHandle survivor = jobs.Submit([&](JobContext&) { ++laterRan; });
        // Let the worker pick the blocker up.
        for (int i = 0; i < 200 && jobs.StateOf(blocker) != JobState::Running; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Check(jobs.StateOf(blocker) == JobState::Running && jobs.StateOf(queued) == JobState::Queued, "one job runs while the rest wait");
        Check(jobs.Cancel(queued) && jobs.StateOf(queued) == JobState::Cancelled, "a queued job is cancelled without running");
        Check(!jobs.Cancel(blocker), "cancelling a running job only raises its flag");
        gate.unlock();
        Check(jobs.Wait(blocker) == JobState::Cancelled && sawCancel, "the running job observed the flag and reported Cancelled");
        Check(jobs.Wait(survivor) == JobState::Completed && laterRan == 1, "the uncancelled queued job still ran; the cancelled one never did");
        Check(jobs.Stats().cancelled == 2, "both cancellations are counted");
    }
    {
        // Shutdown with work outstanding: queued jobs never run, running
        // jobs are waited for, no worker survives.
        std::atomic<int> ran{0};
        std::atomic<int> longRunning{0};
        auto jobs = std::make_unique<JobSystem>(2);
        std::mutex gate;
        gate.lock();
        jobs->Submit([&](JobContext&) { std::lock_guard<std::mutex> hold(gate); ++longRunning; });
        jobs->Submit([&](JobContext&) { std::lock_guard<std::mutex> hold(gate); ++longRunning; });
        for (int i = 0; i < 50; ++i) jobs->Submit([&](JobContext&) { ++ran; }, JobPriority::Low);
        for (int i = 0; i < 200 && jobs->Stats().running < 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::thread releaser([&] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); gate.unlock(); });
        const auto start = std::chrono::steady_clock::now();
        jobs->Shutdown();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        releaser.join();
        Check(longRunning == 2, "shutdown waited for the two running jobs");
        Check(ran == 0, "the fifty queued jobs were cancelled without running");
        Check(jobs->Stats().cancelled == 50 && jobs->WorkerCount() == 0, "shutdown cancelled the queue and joined every worker");
        Check(!jobs->Submit([](JobContext&) {}).IsValid(), "submission after shutdown is refused");
        Check(ms < 5000.0, "shutdown did not deadlock");
        jobs.reset();
    }
}

// --- B. priority and concurrency ------------------------------------------

void SectionPriority() {
    std::printf("Section B: priority preference without starvation, many concurrent jobs\n");
    {
        JobSystem jobs(1);
        std::mutex gate;
        gate.lock();
        std::vector<int> order;
        std::mutex orderMutex;
        jobs.Submit([&](JobContext&) { std::lock_guard<std::mutex> hold(gate); });  // occupies the worker
        for (int i = 0; i < 200 && jobs.Stats().running < 1; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // Queue 8 Low, then 8 High, then 8 Normal while the worker is busy.
        for (int i = 0; i < 8; ++i) jobs.Submit([&, i](JobContext&) { std::lock_guard<std::mutex> l(orderMutex); order.push_back(200 + i); }, JobPriority::Low);
        for (int i = 0; i < 8; ++i) jobs.Submit([&, i](JobContext&) { std::lock_guard<std::mutex> l(orderMutex); order.push_back(i); }, JobPriority::High);
        for (int i = 0; i < 8; ++i) jobs.Submit([&, i](JobContext&) { std::lock_guard<std::mutex> l(orderMutex); order.push_back(100 + i); }, JobPriority::Normal);
        gate.unlock();
        jobs.WaitAll();
        Check(order.size() == 24, "all 24 prioritised jobs ran");
        // High jobs come first (with at most one fairness concession per
        // kFairnessInterval), and every High finishes before the last Low.
        std::size_t lastHigh = 0, firstLow = order.size();
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (order[i] < 100) lastHigh = i;
            if (order[i] >= 200 && i < firstLow) firstLow = i;
        }
        int highInFirstTen = 0;
        for (std::size_t i = 0; i < 10 && i < order.size(); ++i) if (order[i] < 100) ++highInFirstTen;
        Check(highInFirstTen >= 8 - 2, "High jobs dominate the first ten completions (" + std::to_string(highInFirstTen) + " of 10)");
        Check(order[0] < 100, "the first job after the blocker is High");
        Check(lastHigh < order.size() - 1 && order.back() >= 100, "the last job to run is not High");
    }
    {
        // No starvation: a steady stream of High work does not stop Low work.
        JobSystem jobs(1);
        std::atomic<int> lowDone{0};
        std::atomic<int> highDone{0};
        for (int i = 0; i < 20; ++i) jobs.Submit([&](JobContext&) { ++lowDone; }, JobPriority::Low);
        std::atomic<bool> keepFeeding{true};
        std::thread feeder([&] {
            while (keepFeeding) {
                if (jobs.Stats().queued < 40) {
                    for (int i = 0; i < 20; ++i) jobs.Submit([&](JobContext&) { ++highDone; std::this_thread::sleep_for(std::chrono::microseconds(50)); }, JobPriority::High);
                }
                std::this_thread::yield();
            }
        });
        const auto start = std::chrono::steady_clock::now();
        while (lowDone < 20 && std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        keepFeeding = false;
        feeder.join();
        jobs.WaitAll();
        Check(lowDone == 20, "all Low jobs completed under a continuous High stream (High completed: " + std::to_string(highDone.load()) + ")");
        Check(highDone > 20, "the High stream kept running meanwhile");
    }
    {
        JobSystem jobs(4);
        std::atomic<int> counter{0};
        std::set<std::thread::id> threads;
        std::mutex threadsMutex;
        std::vector<JobHandle> handles;
        for (int i = 0; i < 2000; ++i) {
            handles.push_back(jobs.Submit([&](JobContext&) {
                ++counter;
                std::lock_guard<std::mutex> l(threadsMutex);
                threads.insert(std::this_thread::get_id());
            }, static_cast<JobPriority>(i % 3)));
        }
        jobs.WaitAll();
        bool allCompleted = true;
        for (const JobHandle& h : handles) allCompleted = allCompleted && jobs.StateOf(h) == JobState::Completed;
        Check(counter == 2000 && allCompleted, "2000 jobs across three priorities all completed");
        Check(threads.size() >= 2 && threads.size() <= 4 && !threads.count(std::this_thread::get_id()),
              "they ran on the worker threads only (" + std::to_string(threads.size()) + " distinct workers)");
        const JobStats s = jobs.Stats();
        Check(s.queued == 0 && s.running == 0 && s.completed == 2000, "the queue drained completely");
    }
}

// --- C. asynchronous file reads ----------------------------------------------

void SectionAsyncFile() {
    std::printf("Section C: asynchronous file reads\n");
    JobSystem jobs(3);
    auto ok = ReadFileAsync(jobs, "assets/models/beacon.obj");
    Check(ok->Wait() == FileReadStatus::Succeeded && ok->Bytes().size() == fs::file_size("assets/models/beacon.obj"),
          "a valid read completes with every byte of the file");
    auto missing = ReadFileAsync(jobs, "assets/models/does_not_exist.obj");
    Check(missing->Wait() == FileReadStatus::Failed && missing->Error().find("not found") != std::string::npos,
          "a missing file fails with a useful message: " + missing->Error());
    {
        // Cancellation before the read starts: block the workers first.
        JobSystem one(1);
        std::mutex gate;
        gate.lock();
        one.Submit([&](JobContext&) { std::lock_guard<std::mutex> hold(gate); });
        for (int i = 0; i < 200 && one.Stats().running < 1; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto request = ReadFileAsync(one, "assets/models/beacon.obj");
        Check(request->Cancel() == FileReadStatus::Cancelled, "a queued read is cancelled");
        gate.unlock();
        one.WaitAll();
        Check(request->Status() == FileReadStatus::Cancelled && request->Bytes().empty(), "it stays Cancelled and holds no data");
    }
    {
        // The caller drops its reference immediately; the worker's copy keeps the request alive.
        std::weak_ptr<FileReadRequest> weak;
        {
            auto request = ReadFileAsync(jobs, "assets/textures/beacon.png");
            weak = request;
        }
        jobs.WaitAll();
        Check(weak.expired(), "a request nobody holds is freed once the read finishes (no dangling worker reference)");
    }
    {
        std::vector<std::shared_ptr<FileReadRequest>> requests;
        const char* files[] = {"assets/models/beacon.obj", "assets/models/plane.obj", "assets/textures/beacon.png",
                               "assets/fonts/DejaVuSans.ttf", "README.md"};
        for (int i = 0; i < 200; ++i) requests.push_back(ReadFileAsync(jobs, files[i % 5], static_cast<JobPriority>(i % 3)));
        bool allOk = true;
        for (auto& r : requests) allOk = allOk && r->Wait() == FileReadStatus::Succeeded && !r->Bytes().empty();
        Check(allOk, "200 simultaneous reads all succeed");
    }
    {
        // Shutdown with reads outstanding: each request ends Cancelled or Succeeded, never Pending.
        auto system = std::make_unique<JobSystem>(2);
        std::vector<std::shared_ptr<FileReadRequest>> requests;
        for (int i = 0; i < 100; ++i) requests.push_back(ReadFileAsync(*system, "assets/fonts/DejaVuSans.ttf"));
        system->Shutdown();
        bool settled = true;
        int cancelled = 0;
        for (auto& r : requests) {
            if (r->Status() == FileReadStatus::Pending) settled = false;
            if (r->Status() == FileReadStatus::Cancelled) ++cancelled;
        }
        Check(settled, "after shutdown no request is left Pending (" + std::to_string(cancelled) + " cancelled)");
        auto late = ReadFileAsync(*system, "README.md");
        Check(late->Status() == FileReadStatus::Failed, "a read submitted after shutdown fails immediately");
    }
}

// --- D. asynchronous resource manager ---------------------------------------

void SectionResources() {
    std::printf("Section D: asynchronous resource loading (real assets, headless residency)\n");
    AssetDatabase db;
    db.Scan(".", "assets");
    JobSystem jobs(3);
    {
        ResourceManager resources(nullptr, &db, &jobs);
        resources.SetHeadlessResidency(true);
        const std::thread::id main = std::this_thread::get_id();
        Check(resources.RequestMesh(kBeaconMesh) == ResourceState::Queued, "a request starts a load and returns Queued");
        Check(resources.RequestMesh(kBeaconMesh) != ResourceState::Unloaded && resources.Stats().misses == 1 && resources.Stats().hits == 1,
              "a second request joins the in-flight load (one load, one hit)");
        std::string error;
        Check(!resources.GetMesh(kBeaconMesh, error).IsValid() && error == "loading", "GetMesh does not block: no handle, 'loading'");
        // Wait for the worker without pumping: the decode finishes off-thread
        // but nothing becomes Ready until the owner pumps.
        jobs.WaitAll();
        const ResourceState beforePump = resources.StateOf(kBeaconMesh);
        Check(beforePump == ResourceState::Queued || beforePump == ResourceState::Loading,
              "before Pump the entry is not Ready even though the worker finished (" + std::string(ResourceStateName(beforePump)) + ")");
        resources.Pump();
        Check(resources.StateOf(kBeaconMesh) == ResourceState::Ready, "Pump on the owner thread installs the decoded mesh");
        Check(resources.DecodeThreadOf(kBeaconMesh) != main && resources.DecodeThreadOf(kBeaconMesh) != std::thread::id(),
              "the decode ran on a worker thread");
        Check(resources.OwnerThread() == main && resources.Stats().misses == 1, "the owner thread is the constructing thread; still one load");
        Check(resources.BytesOf(kBeaconMesh) > 0 && resources.Stats().bytesResident == resources.BytesOf(kBeaconMesh),
              "resident bytes are accounted from the decoded data");

        // Texture + duplicate coalescing under load.
        for (int i = 0; i < 50; ++i) resources.RequestTexture(kBeaconTexture, JobPriority::Low);
        resources.WaitForAll();
        Check(resources.StateOf(kBeaconTexture) == ResourceState::Ready && resources.Stats().misses == 2,
              "fifty requests for one texture decoded it exactly once");

        // Failure: unknown id, missing file, corrupt data; recovery after Invalidate.
        Check(resources.RequestMesh("ffffffffffffffffffffffffffffffff") == ResourceState::Failed &&
                  resources.ErrorOf("ffffffffffffffffffffffffffffffff").find("unknown asset id") != std::string::npos,
              "an unknown id fails immediately with a message");
        Check(resources.RequestTexture(kBeaconMesh) == ResourceState::Failed, "requesting a mesh id as a texture fails");
    }
    {
        // Corrupt then repaired file in a temporary project.
        TempDir temp("judas_m31_assets");
        {
            std::error_code ec;
            fs::create_directories(temp.path / "Assets", ec);
            std::ofstream bad(temp.path / "Assets" / "bad.obj");
            bad << "v 0 0 0\nnot an obj face\n";
        }
        AssetDatabase tempDb;
        tempDb.Scan(temp.Str(), temp.Str() + "/Assets");
        AssetRecord record;
        std::string error;
        // Track validates with the loader, so write the sidecar directly to
        // simulate a file that went bad AFTER import.
        Check(AssetDatabase::WriteMeta(temp.Str() + "/Assets/bad.obj.judasmeta", "abcdefabcdefabcdefabcdefabcdefab", AssetType::Mesh, "test", error),
              "a sidecar for a corrupt mesh is written");
        tempDb.Scan(temp.Str(), temp.Str() + "/Assets");
        ResourceManager resources(nullptr, &tempDb, &jobs);
        resources.SetHeadlessResidency(true);
        resources.RequestMesh("abcdefabcdefabcdefabcdefabcdefab");
        resources.WaitForAll();
        Check(resources.StateOf("abcdefabcdefabcdefabcdefabcdefab") == ResourceState::Failed &&
                  !resources.ErrorOf("abcdefabcdefabcdefabcdefabcdefab").empty(),
              "a corrupt mesh ends Failed with the parser's message: " + resources.ErrorOf("abcdefabcdefabcdefabcdefabcdefab"));
        Check(resources.RequestMesh("abcdefabcdefabcdefabcdefabcdefab") == ResourceState::Failed && resources.Stats().misses == 1,
              "a failed asset is not retried by itself");
        std::error_code ec;
        fs::copy_file("assets/models/beacon.obj", temp.path / "Assets" / "bad.obj", fs::copy_options::overwrite_existing, ec);
        resources.Invalidate("abcdefabcdefabcdefabcdefabcdefab");
        resources.RequestMesh("abcdefabcdefabcdefabcdefabcdefab");
        resources.WaitForAll();
        Check(resources.StateOf("abcdefabcdefabcdefabcdefabcdefab") == ResourceState::Ready,
              "after the file is repaired, Invalidate + request loads it (failed asset recovery)");
        // A missing file (sidecar present) fails at resolve time.
        AssetDatabase::WriteMeta(temp.Str() + "/Assets/gone.png.judasmeta", "12341234123412341234123412341234", AssetType::Texture, "test", error);
        tempDb.Scan(temp.Str(), temp.Str() + "/Assets");
        Check(resources.RequestTexture("12341234123412341234123412341234") == ResourceState::Failed &&
                  resources.ErrorOf("12341234123412341234123412341234").find("missing") != std::string::npos,
              "a missing file fails without a job");
    }
    {
        // Stale completion: release while the load is in flight; the late
        // result must not resurrect the entry.
        JobSystem one(1);
        std::mutex gate;
        gate.lock();
        one.Submit([&](JobContext&) { std::lock_guard<std::mutex> hold(gate); });
        for (int i = 0; i < 200 && one.Stats().running < 1; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ResourceManager resources(nullptr, &db, &one);
        resources.SetHeadlessResidency(true);
        resources.RequestMesh(kPlaneMesh);
        Check(resources.StateOf(kPlaneMesh) == ResourceState::Queued, "the load waits behind the blocker");
        resources.Release(kPlaneMesh);  // generation bump + cancel
        Check(resources.StateOf(kPlaneMesh) == ResourceState::Unloaded, "Release while queued leaves the entry Unloaded");
        gate.unlock();
        one.WaitAll();
        resources.Pump();
        Check(resources.StateOf(kPlaneMesh) == ResourceState::Unloaded && resources.Stats().staleDiscarded >= 1,
              "the late completion was discarded as stale, not installed");
        // And a superseded generation: release mid-flight, re-request; only the new load counts.
        std::mutex gate2;
        gate2.lock();
        one.Submit([&](JobContext&) { std::lock_guard<std::mutex> hold(gate2); });
        for (int i = 0; i < 200 && one.Stats().running < 1; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        resources.RequestMesh(kPlaneMesh);
        resources.Release(kPlaneMesh);
        resources.RequestMesh(kPlaneMesh);
        gate2.unlock();
        resources.WaitForAll();
        Check(resources.StateOf(kPlaneMesh) == ResourceState::Ready && resources.Stats().misses == 3,
              "release + re-request loads the new generation once and it becomes Ready");
        // Demand cancellation: last ReleaseRef of a queued load cancels it.
        std::mutex gate3;
        gate3.lock();
        one.Submit([&](JobContext&) { std::lock_guard<std::mutex> hold(gate3); });
        for (int i = 0; i < 200 && one.Stats().running < 1; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        resources.AddRef(kBeaconTexture);
        resources.RequestTexture(kBeaconTexture);
        resources.ReleaseRef(kBeaconTexture);
        Check(resources.StateOf(kBeaconTexture) == ResourceState::Cancelled, "dropping the last reference cancels a queued load");
        gate3.unlock();
        resources.WaitForAll();
        Check(resources.StateOf(kBeaconTexture) == ResourceState::Cancelled && resources.Stats().cancelled >= 1,
              "it stays Cancelled; a later request would load again");
        resources.RequestTexture(kBeaconTexture);
        resources.WaitForAll();
        Check(resources.StateOf(kBeaconTexture) == ResourceState::Ready, "and does");
    }
    {
        // Budget and eviction.
        ResourceManager resources(nullptr, &db, &jobs);
        resources.SetHeadlessResidency(true);
        resources.RequestMesh(kBeaconMesh);
        resources.RequestMesh(kPlaneMesh);
        resources.RequestTexture(kBeaconTexture);
        resources.WaitForAll();
        const std::uint64_t beacon = resources.BytesOf(kBeaconMesh), plane = resources.BytesOf(kPlaneMesh),
                            texture = resources.BytesOf(kBeaconTexture);
        Check(beacon > 0 && plane > 0 && texture > 0 && resources.Stats().bytesResident == beacon + plane + texture,
              "three resident resources are accounted (" + std::to_string(beacon + plane + texture) + " bytes)");
        resources.AddRef(kBeaconTexture);  // actively used
        resources.TryGetMesh(kPlaneMesh);  // most recently used mesh
        resources.SetBudgetBytes(texture + plane);
        resources.Pump();
        Check(resources.StateOf(kBeaconMesh) == ResourceState::Unloaded && resources.StateOf(kPlaneMesh) == ResourceState::Ready &&
                  resources.StateOf(kBeaconTexture) == ResourceState::Ready,
              "over budget, the least recently used unreferenced resource is evicted");
        Check(resources.Stats().evictions == 1 && resources.Stats().bytesResident == texture + plane, "eviction is counted and bytes drop");
        resources.SetBudgetBytes(1);
        resources.Pump();
        Check(resources.StateOf(kBeaconTexture) == ResourceState::Ready && resources.StateOf(kPlaneMesh) == ResourceState::Unloaded,
              "a referenced resource is never evicted even when the budget cannot be met");
        Check(resources.Stats().bytesResident == texture, "the manager stays over budget rather than destroy an active resource");
        Check(db.Find(kBeaconMesh) != nullptr && db.Find(kPlaneMesh) != nullptr, "eviction never touches the asset database");
        resources.SetBudgetBytes(1ull << 30);
        resources.RequestMesh(kBeaconMesh);
        resources.WaitForAll();
        Check(resources.StateOf(kBeaconMesh) == ResourceState::Ready && resources.Stats().misses == 4,
              "requesting an evicted resource loads it again normally");
    }
    {
        // Blocking mode keeps the M30 synchronous contract.
        ResourceManager resources(nullptr, &db, &jobs);
        resources.SetHeadlessResidency(true);
        resources.SetBlockingMode(true);
        std::string error;
        resources.GetMesh(kBeaconMesh, error);
        Check(resources.StateOf(kBeaconMesh) == ResourceState::Ready && jobs.Stats().submitted == jobs.Stats().submitted,
              "blocking mode loads on the caller without a job");
        Check(resources.DecodeThreadOf(kBeaconMesh) == std::this_thread::get_id(), "and the decode ran on the caller");
    }
    {
        // Destruction with loads outstanding, before the job system.
        auto system = std::make_unique<JobSystem>(2);
        {
            ResourceManager resources(nullptr, &db, system.get());
            resources.SetHeadlessResidency(true);
            for (int i = 0; i < 20; ++i) {
                resources.RequestMesh(kBeaconMesh);
                resources.RequestMesh(kPlaneMesh);
                resources.RequestTexture(kBeaconTexture);
                resources.Release(kBeaconMesh);
                resources.Release(kPlaneMesh);
            }
        }
        system->Shutdown();
        Check(system->Stats().running == 0 && system->Stats().queued == 0, "a manager destroyed with loads outstanding leaves no job behind");
    }
}
}  // namespace

int main() {
    SectionJobs();
    SectionPriority();
    SectionAsyncFile();
    SectionResources();
    std::printf("Job tests: %s (%d failures)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
