#include "AsyncFile.h"

#include <cstdio>
#include <filesystem>

namespace {
constexpr std::size_t kChunkBytes = 1u << 20;  // 1 MiB between cancellation checks
}

bool ReadWholeFile(const std::string& path, std::vector<std::uint8_t>& outBytes, std::string& outError,
                   const JobContext* cancel, bool* outCancelled) {
    if (outCancelled) *outCancelled = false;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        outError = "file not found: " + path;
        return false;
    }
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        outError = "could not open file: " + path;
        return false;
    }
    outBytes.clear();
    std::vector<std::uint8_t> chunk(kChunkBytes);
    for (;;) {
        if (cancel && cancel->CancelRequested()) {
            std::fclose(file);
            outBytes.clear();
            if (outCancelled) *outCancelled = true;
            outError = "cancelled";
            return false;
        }
        const std::size_t got = std::fread(chunk.data(), 1, chunk.size(), file);
        if (got > 0) outBytes.insert(outBytes.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(got));
        if (got < chunk.size()) {
            const bool failed = std::ferror(file) != 0;
            std::fclose(file);
            if (failed) {
                outError = "read error: " + path;
                outBytes.clear();
                return false;
            }
            return true;
        }
    }
}

std::shared_ptr<FileReadRequest> ReadFileAsync(JobSystem& system, const std::string& path, JobPriority priority) {
    auto request = std::make_shared<FileReadRequest>();
    request->m_system = &system;
    request->m_path = path;
    // The job captures its own shared reference: the caller's copy may go
    // away before the read finishes.
    request->m_job = system.Submit(
        [request](JobContext& context) {
            std::string error;
            bool cancelled = false;
            if (ReadWholeFile(request->m_path, request->m_bytes, error, &context, &cancelled)) {
                request->m_status.store(FileReadStatus::Succeeded, std::memory_order_release);
            } else if (cancelled) {
                request->m_status.store(FileReadStatus::Cancelled, std::memory_order_release);
                context.ReportCancelled();
            } else {
                request->m_error = error;
                request->m_status.store(FileReadStatus::Failed, std::memory_order_release);
                context.SetError(error);
            }
        },
        priority, "read " + path);
    if (!request->m_job.IsValid()) {
        request->m_error = "job system is shut down";
        request->m_status.store(FileReadStatus::Failed, std::memory_order_release);
    }
    return request;
}

FileReadStatus FileReadRequest::Status() const {
    const FileReadStatus status = m_status.load(std::memory_order_acquire);
    if (status != FileReadStatus::Pending || !m_system || !m_job.IsValid()) return status;
    const JobState job = m_system->StateOf(m_job);
    // A queued job that was cancelled (or dropped) never runs, so nothing
    // else will ever write the status.
    if (job == JobState::Cancelled || job == JobState::Unknown) return FileReadStatus::Cancelled;
    return status;
}

FileReadStatus FileReadRequest::Cancel() {
    if (IsFinished()) return Status();
    if (m_system && m_system->Cancel(m_job)) {
        // Removed from the queue before it started: nothing will ever
        // write the buffers, so the status is ours to set.
        m_status.store(FileReadStatus::Cancelled, std::memory_order_release);
    }
    // Otherwise the running read observes the flag and sets Cancelled itself.
    return Status();
}

FileReadStatus FileReadRequest::Wait() {
    if (m_system && m_job.IsValid()) m_system->Wait(m_job);
    return Status();
}
