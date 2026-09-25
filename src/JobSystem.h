#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Milestone 31: Judas's general-purpose background worker system.
//
// A bounded pool of worker threads (never one thread per job, never a
// detached thread) executes submitted jobs by priority. A job is a plain
// callable that receives a JobContext (a cooperative cancellation flag and
// an error slot); it produces DATA — a result object it owns or shares —
// and never mutates engine state: results are installed by the thread
// that owns the state, at that thread's own safe point (for resources,
// ResourceManager::Pump on the GL thread; see docs/ARCHITECTURE.md,
// "Milestone 31, Thread ownership law").
//
// Lifecycle of a job:
//
//   Queued ---> Running ---> Completed
//     |            |    \--> Failed      (the callable threw, or SetError)
//     \--> Cancelled (Cancel before it started)
//                  \--> Cancelled (the callable observed CancelRequested
//                                 and returned early, reporting so)
//
// Priority: High, Normal, Low. Workers take the highest non-empty queue,
// except that after kFairnessInterval consecutive higher-priority takes
// while a lower queue waits, one job is taken from the lower queue, so
// low-priority work is delayed by high-priority work but never starved.
//
// Records of finished jobs stay queryable until Forget(handle) is called
// or until kMaxFinishedRecords finished records exist, when the oldest
// are dropped (their state is then reported as Unknown). Ordinary runtime
// consumers (ResourceManager) forget a job as soon as they have consumed
// its result; nothing here grows without bound.
//
// Shutdown (destructor or Shutdown()): no new submissions are accepted,
// every still-queued job becomes Cancelled without running, running jobs
// are asked to cancel and are waited for, workers are joined. This is
// deterministic and never returns with a worker alive.
//
// Waiting: Wait/WaitAll block the caller on a condition variable — for
// tests, tools and shutdown. The runtime never has to: it polls state or
// consumes results at its own frame boundary.
enum class JobPriority { High = 0, Normal = 1, Low = 2 };
enum class JobState { Unknown, Queued, Running, Completed, Failed, Cancelled };

using JobId = std::uint64_t;

struct JobHandle {
    JobId id = 0;
    bool IsValid() const { return id != 0; }
};

class JobContext {
public:
    explicit JobContext(const std::atomic<bool>* cancel) : m_cancel(cancel) {}
    // True once Cancel was requested for a running job (or the system is
    // shutting down). A cooperative job checks this between its steps and
    // returns early; ReportCancelled() then records the job as Cancelled
    // rather than Completed.
    bool CancelRequested() const { return m_cancel->load(std::memory_order_acquire); }
    void ReportCancelled() { m_cancelled = true; }
    void SetError(const std::string& message) { m_error = message; m_failed = true; }
    bool WasCancelled() const { return m_cancelled; }
    bool Failed() const { return m_failed; }
    const std::string& Error() const { return m_error; }

private:
    const std::atomic<bool>* m_cancel;
    bool m_cancelled = false;
    bool m_failed = false;
    std::string m_error;
};

using JobFunction = std::function<void(JobContext&)>;

struct JobStats {
    unsigned int workers = 0;
    std::size_t queued = 0;
    std::size_t running = 0;
    unsigned long long submitted = 0;
    unsigned long long completed = 0;
    unsigned long long failed = 0;
    unsigned long long cancelled = 0;
    // Wall time workers spent inside job callables since construction,
    // summed over workers (utilization = busy / (workers * elapsed)).
    double workerBusySeconds = 0.0;
    double elapsedSeconds = 0.0;
};

class JobSystem {
public:
    // `workerCount` 0 derives a count from the hardware: concurrency - 1
    // (one core is left for the simulation/render thread), at least 1,
    // at most kMaxWorkers.
    explicit JobSystem(unsigned int workerCount = 0);
    ~JobSystem();
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    static constexpr unsigned int kMaxWorkers = 16;
    static constexpr unsigned int kFairnessInterval = 4;
    static constexpr std::size_t kMaxFinishedRecords = 4096;

    // Returns an invalid handle after Shutdown.
    JobHandle Submit(JobFunction work, JobPriority priority = JobPriority::Normal, std::string name = std::string());

    // Cancels a job. Queued: removed, becomes Cancelled, returns true.
    // Running: the cancel flag is raised for the callable to observe;
    // returns false (the work is under way; whether it stops early is the
    // callable's decision). Finished/unknown: returns false.
    bool Cancel(JobHandle handle);

    JobState StateOf(JobHandle handle) const;
    std::string ErrorOf(JobHandle handle) const;
    std::string NameOf(JobHandle handle) const;
    bool IsFinished(JobHandle handle) const;
    // Drops the finished record. No-op while queued/running.
    void Forget(JobHandle handle);

    // Blocking waits (tests, tools, shutdown). Wait returns the final state.
    JobState Wait(JobHandle handle);
    void WaitAll();

    void Shutdown();
    bool IsShutDown() const { return m_shutdown.load(); }
    unsigned int WorkerCount() const { return static_cast<unsigned int>(m_workers.size()); }
    JobStats Stats() const;

private:
    struct Job {
        JobId id = 0;
        std::string name;
        JobFunction work;
        JobPriority priority = JobPriority::Normal;
        JobState state = JobState::Queued;
        std::string error;
        std::atomic<bool> cancel{false};
    };
    using JobPtr = std::shared_ptr<Job>;

    void WorkerLoop(unsigned int workerIndex);
    JobPtr TakeNextJobLocked();
    void PruneFinishedLocked();

    mutable std::mutex m_mutex;
    std::condition_variable m_workAvailable;
    std::condition_variable m_jobFinished;
    std::vector<std::thread> m_workers;
    std::deque<JobPtr> m_queues[3];
    std::map<JobId, JobPtr> m_records;  // every job not yet forgotten
    std::deque<JobId> m_finishedOrder;
    JobId m_nextId = 1;
    std::atomic<bool> m_shutdown{false};
    bool m_stopWorkers = false;
    unsigned int m_consecutiveHigherTakes = 0;
    std::size_t m_running = 0;
    unsigned long long m_submitted = 0, m_completed = 0, m_failed = 0, m_cancelled = 0;
    double m_busySeconds = 0.0;
    std::chrono::steady_clock::time_point m_start;
};
