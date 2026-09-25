#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "JobSystem.h"

// Milestone 31: asynchronous file reads on the job system.
//
// ReadFileAsync submits one job that reads the whole file into a byte
// vector in chunks, checking for cancellation between chunks. The request
// object is shared: the worker holds its own reference, so a caller may
// drop the request at any time without the worker ever touching freed
// memory, and a caller that keeps it observes exactly one of:
//
//   Pending -> Succeeded (Bytes() holds the file)
//           -> Failed    (Error() says why: missing, unreadable, ...)
//           -> Cancelled (Cancel() before or during the read; a read that
//                         was already running stops at the next chunk)
//
// Bytes() and Error() are stable once the request is finished; reading
// them earlier is a data race by contract, so callers poll Status() (or
// Wait() in tests/tools) first. There is no virtual filesystem, no
// archive format and no memory mapping here — a request is a path.
enum class FileReadStatus { Pending, Succeeded, Failed, Cancelled };

class FileReadRequest {
public:
    // Pending resolves to Cancelled once the job system reports the read's
    // job cancelled (a shutdown with the read still queued).
    FileReadStatus Status() const;
    bool IsFinished() const { return Status() != FileReadStatus::Pending; }
    const std::vector<std::uint8_t>& Bytes() const { return m_bytes; }
    const std::string& Error() const { return m_error; }
    const std::string& Path() const { return m_path; }
    JobHandle Job() const { return m_job; }

    // Cancels if the read has not finished; a queued read never starts, a
    // running one stops at its next chunk. Returns the status afterwards
    // (Succeeded/Failed if it had already finished).
    FileReadStatus Cancel();
    // Blocking; tests and tools only.
    FileReadStatus Wait();

private:
    friend std::shared_ptr<FileReadRequest> ReadFileAsync(JobSystem&, const std::string&, JobPriority);
    JobSystem* m_system = nullptr;
    std::string m_path;
    JobHandle m_job;
    std::atomic<FileReadStatus> m_status{FileReadStatus::Pending};
    std::vector<std::uint8_t> m_bytes;
    std::string m_error;
};

std::shared_ptr<FileReadRequest> ReadFileAsync(JobSystem& system, const std::string& path,
                                               JobPriority priority = JobPriority::Normal);

// The synchronous counterpart used by the same job (and by blocking
// mode): reads the whole file, checking `cancel` between chunks when
// given. Returns false with a message on failure; sets `outCancelled`
// when it stopped for cancellation.
bool ReadWholeFile(const std::string& path, std::vector<std::uint8_t>& outBytes, std::string& outError,
                   const JobContext* cancel = nullptr, bool* outCancelled = nullptr);
