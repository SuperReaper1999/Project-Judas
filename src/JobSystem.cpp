#include "JobSystem.h"

#include <algorithm>
#include <chrono>
#include <exception>

namespace {
unsigned int DeriveWorkerCount(unsigned int requested) {
    if (requested > 0) return std::min(requested, JobSystem::kMaxWorkers);
    const unsigned int hardware = std::thread::hardware_concurrency();
    if (hardware <= 1) return 1;
    return std::min(hardware - 1, JobSystem::kMaxWorkers);
}
}  // namespace

JobSystem::JobSystem(unsigned int workerCount) : m_start(std::chrono::steady_clock::now()) {
    const unsigned int count = DeriveWorkerCount(workerCount);
    m_workers.reserve(count);
    for (unsigned int i = 0; i < count; ++i) m_workers.emplace_back([this, i] { WorkerLoop(i); });
}

JobSystem::~JobSystem() {
    Shutdown();
}

JobHandle JobSystem::Submit(JobFunction work, JobPriority priority, std::string name) {
    if (m_shutdown.load()) return JobHandle{};
    JobPtr job = std::make_shared<Job>();
    job->work = std::move(work);
    job->priority = priority;
    job->name = std::move(name);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopWorkers) return JobHandle{};
        job->id = m_nextId++;
        m_records[job->id] = job;
        m_queues[static_cast<int>(priority)].push_back(job);
        ++m_submitted;
    }
    m_workAvailable.notify_one();
    return JobHandle{job->id};
}

bool JobSystem::Cancel(JobHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_records.find(handle.id);
    if (it == m_records.end()) return false;
    JobPtr job = it->second;
    if (job->state == JobState::Queued) {
        std::deque<JobPtr>& queue = m_queues[static_cast<int>(job->priority)];
        queue.erase(std::remove(queue.begin(), queue.end(), job), queue.end());
        job->state = JobState::Cancelled;
        job->cancel.store(true);
        job->work = nullptr;  // release whatever the callable captured
        ++m_cancelled;
        m_finishedOrder.push_back(job->id);
        PruneFinishedLocked();
        m_jobFinished.notify_all();
        return true;
    }
    if (job->state == JobState::Running) job->cancel.store(true, std::memory_order_release);
    return false;
}

JobState JobSystem::StateOf(JobHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_records.find(handle.id);
    return it == m_records.end() ? JobState::Unknown : it->second->state;
}

std::string JobSystem::ErrorOf(JobHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_records.find(handle.id);
    return it == m_records.end() ? std::string() : it->second->error;
}

std::string JobSystem::NameOf(JobHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_records.find(handle.id);
    return it == m_records.end() ? std::string() : it->second->name;
}

bool JobSystem::IsFinished(JobHandle handle) const {
    const JobState state = StateOf(handle);
    return state == JobState::Completed || state == JobState::Failed || state == JobState::Cancelled ||
           state == JobState::Unknown;
}

void JobSystem::Forget(JobHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_records.find(handle.id);
    if (it == m_records.end()) return;
    const JobState state = it->second->state;
    if (state == JobState::Queued || state == JobState::Running) return;
    m_records.erase(it);
}

JobState JobSystem::Wait(JobHandle handle) {
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
        const auto it = m_records.find(handle.id);
        if (it == m_records.end()) return JobState::Unknown;
        const JobState state = it->second->state;
        if (state != JobState::Queued && state != JobState::Running) return state;
        m_jobFinished.wait(lock);
    }
}

void JobSystem::WaitAll() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_jobFinished.wait(lock, [this] {
        return m_running == 0 && m_queues[0].empty() && m_queues[1].empty() && m_queues[2].empty();
    });
}

void JobSystem::Shutdown() {
    if (m_shutdown.exchange(true)) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopWorkers = true;
        // Everything still queued is cancelled without running.
        for (std::deque<JobPtr>& queue : m_queues) {
            for (JobPtr& job : queue) {
                job->state = JobState::Cancelled;
                job->cancel.store(true);
                job->work = nullptr;
                ++m_cancelled;
                m_finishedOrder.push_back(job->id);
            }
            queue.clear();
        }
        // Running jobs are asked to stop early; they are waited for below.
        for (auto& [id, job] : m_records) {
            if (job->state == JobState::Running) job->cancel.store(true, std::memory_order_release);
        }
    }
    m_workAvailable.notify_all();
    m_jobFinished.notify_all();
    for (std::thread& worker : m_workers) {
        if (worker.joinable()) worker.join();
    }
    m_workers.clear();
}

JobStats JobSystem::Stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    JobStats stats;
    stats.workers = static_cast<unsigned int>(m_workers.size());
    stats.queued = m_queues[0].size() + m_queues[1].size() + m_queues[2].size();
    stats.running = m_running;
    stats.submitted = m_submitted;
    stats.completed = m_completed;
    stats.failed = m_failed;
    stats.cancelled = m_cancelled;
    stats.workerBusySeconds = m_busySeconds;
    stats.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
    return stats;
}

JobSystem::JobPtr JobSystem::TakeNextJobLocked() {
    // Highest non-empty queue, with a fairness concession: after
    // kFairnessInterval consecutive takes from a higher queue while a lower
    // one is waiting, take from the highest waiting LOWER queue once.
    int chosen = -1;
    for (int p = 0; p < 3; ++p) {
        if (!m_queues[p].empty()) { chosen = p; break; }
    }
    if (chosen < 0) return nullptr;
    bool lowerWaiting = false;
    for (int p = chosen + 1; p < 3; ++p) lowerWaiting = lowerWaiting || !m_queues[p].empty();
    if (lowerWaiting && m_consecutiveHigherTakes >= kFairnessInterval) {
        for (int p = chosen + 1; p < 3; ++p) {
            if (!m_queues[p].empty()) { chosen = p; break; }
        }
        m_consecutiveHigherTakes = 0;
    } else if (lowerWaiting) {
        ++m_consecutiveHigherTakes;
    } else {
        m_consecutiveHigherTakes = 0;
    }
    JobPtr job = m_queues[chosen].front();
    m_queues[chosen].pop_front();
    return job;
}

void JobSystem::WorkerLoop(unsigned int workerIndex) {
    (void)workerIndex;
    for (;;) {
        JobPtr job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_workAvailable.wait(lock, [this] {
                return m_stopWorkers || !m_queues[0].empty() || !m_queues[1].empty() || !m_queues[2].empty();
            });
            if (m_stopWorkers) return;  // Shutdown already cancelled every queued job
            job = TakeNextJobLocked();
            if (!job) continue;
            job->state = JobState::Running;
            ++m_running;
        }
        const auto start = std::chrono::steady_clock::now();
        JobContext context(&job->cancel);
        JobState finalState = JobState::Completed;
        std::string error;
        try {
            job->work(context);
            if (context.WasCancelled()) finalState = JobState::Cancelled;
            else if (context.Failed()) { finalState = JobState::Failed; error = context.Error(); }
        } catch (const std::exception& e) {
            finalState = JobState::Failed;
            error = e.what();
        } catch (...) {
            finalState = JobState::Failed;
            error = "job threw a non-standard exception";
        }
        const double busy = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        job->work = nullptr;  // a finished job releases its captures at once
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            job->state = finalState;
            job->error = error;
            --m_running;
            m_busySeconds += busy;
            if (finalState == JobState::Completed) ++m_completed;
            else if (finalState == JobState::Failed) ++m_failed;
            else ++m_cancelled;
            m_finishedOrder.push_back(job->id);
            PruneFinishedLocked();
        }
        m_jobFinished.notify_all();
    }
}

void JobSystem::PruneFinishedLocked() {
    while (m_finishedOrder.size() > kMaxFinishedRecords) {
        const JobId oldest = m_finishedOrder.front();
        m_finishedOrder.pop_front();
        const auto it = m_records.find(oldest);
        if (it != m_records.end() && it->second->state != JobState::Queued && it->second->state != JobState::Running) {
            m_records.erase(it);
        }
    }
}
