#pragma once

// Opt-in diagnostic utility, not an engine subsystem. Construct, mark, Stop and
// destroy on the process's main thread. No copying, moving, concurrent Sessions
// or external users of the selected realtime signal are supported. `directory`
// names an output directory. Call Stop before unloading native modules.
//
// Samples are the main thread's instruction pointer AT SIGNAL DELIVERY after a
// CPU-time timer expiration. They are not hardware cycles, wall-time samples,
// call stacks, or exact instruction pointers at the expiration instant.

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <new>
#include <ostream>
#include <string>
#include <string_view>

#if defined(__linux__)
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#endif

namespace sampler {

class Session {
  public:
    static constexpr std::uint32_t kCapacity = 131072;

    explicit Session(const std::filesystem::path& directory, bool enabled,
                     std::uint64_t period_us = 2000) noexcept
        : periodUs_(period_us), requested_(enabled) {
        if (!enabled)
            return;
        try {
            directory_ = directory;
        } catch (...) {
            Fail("cannot retain sampler output path", 0);
            return;
        }
#if defined(__linux__) && defined(__x86_64__) && defined(REG_RIP) && defined(SIGEV_THREAD_ID)
        static_assert(std::atomic<Session*>::is_always_lock_free);
        static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
        static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
        static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
        static_assert(std::atomic<int>::is_always_lock_free);
        pid_ = ::getpid();
        tid_ = static_cast<pid_t>(::syscall(SYS_gettid));
        if (pid_ != tid_) {
            Fail("CPU-PC sampler must be constructed on the main thread", 0);
            return;
        }
        if (!periodUs_ || periodUs_ / 1000000 >
                              static_cast<std::uint64_t>(std::numeric_limits<time_t>::max())) {
            Fail("invalid CPU sampling period", 0);
            return;
        }
        signal_ = SIGRTMIN + 5;
        if (signal_ > SIGRTMAX) {
            Fail("no available dedicated realtime signal", 0);
            return;
        }
        storage_.reset(new (std::nothrow) Slot[kCapacity]);
        if (!storage_) {
            Fail("cannot allocate bounded CPU sample storage", ENOMEM);
            return;
        }
        slots_ = storage_.get();
        SignalBlock block(signal_);
        if (!block.Good()) {
            Fail("cannot block sampler signal during setup", block.Error());
            return;
        }
        if (::sigismember(&block.Previous(), signal_) == 1) {
            Fail("sampler realtime signal was already blocked", 0);
            return;
        }
        sigset_t pending{};
        if (::sigpending(&pending) != 0) {
            Fail("cannot inspect pending sampler signal", errno);
            return;
        }
        if (::sigismember(&pending, signal_) == 1) {
            Fail("sampler realtime signal was already pending", 0);
            return;
        }
        if (::sigaction(signal_, nullptr, &previousAction_) != 0) {
            Fail("cannot inspect sampler signal disposition", errno);
            return;
        }
        if (previousAction_.sa_handler != SIG_DFL) {
            Fail("sampler realtime signal already has a disposition", 0);
            return;
        }
        Session* vacant = nullptr;
        if (!active_.compare_exchange_strong(vacant, this, std::memory_order_acq_rel)) {
            Fail("another CPU-PC sampler Session is active", 0);
            return;
        }
        ownsSlot_ = true;
        struct sigaction action{};
        action.sa_sigaction = &Session::Handle;
        action.sa_flags = SA_SIGINFO | SA_RESTART;
        ::sigemptyset(&action.sa_mask);
        ::sigaddset(&action.sa_mask, signal_);
        if (::sigaction(signal_, &action, nullptr) != 0) {
            Fail("cannot install sampler signal handler", errno);
            CleanupBlocked();
            return;
        }
        handlerInstalled_ = true;
        struct sigevent event{};
        event.sigev_notify = SIGEV_THREAD_ID;
        event.sigev_signo = signal_;
        event.sigev_value.sival_ptr = this;
#if defined(sigev_notify_thread_id)
        event.sigev_notify_thread_id = tid_;
#else
        // glibc's Linux SIGEV_THREAD_ID representation.
        event._sigev_un._tid = tid_;
#endif
        if (::timer_create(CLOCK_THREAD_CPUTIME_ID, &event, &timer_) != 0) {
            Fail("cannot create main-thread CPU timer", errno);
            CleanupBlocked();
            return;
        }
        timerCreated_ = true;
        struct itimerspec schedule{};
        schedule.it_interval.tv_sec = static_cast<time_t>(periodUs_ / 1000000);
        schedule.it_interval.tv_nsec = static_cast<long>((periodUs_ % 1000000) * 1000);
        schedule.it_value = schedule.it_interval;
        ReadClocks(cpuStartNs_, wallStartNs_);
        if (::timer_settime(timer_, 0, &schedule, nullptr) != 0) {
            Fail("cannot arm main-thread CPU timer", errno);
            CleanupBlocked();
            return;
        }
        started_ = true;
        if (block.Restore() != 0) {
            Fail("cannot restore signal mask after sampler setup", block.Error());
            CleanupBlocked();
            started_ = false;
        }
#else
        Fail("CPU-PC sampling requires Linux x86_64 REG_RIP and SIGEV_THREAD_ID", 0);
#endif
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;
    ~Session() { Stop(); }

    void MarkCompletedStep(std::uint64_t step) noexcept {
        completedStep_.store(step, std::memory_order_relaxed);
    }
    bool Active() const noexcept { return started_ && !stopped_; }
    const char* Error() const noexcept { return error_ ? error_ : "none"; }

    // Main thread only; idempotent. Sample publication stops before allocation,
    // filesystem, module enumeration and stream operations. If an OS cleanup
    // operation fails, an inert handler can remain installed rather than expose
    // a pending private RT signal to the original default (fatal) disposition.
    void Stop() noexcept {
        if (stopped_)
            return;
        stopped_ = true;
        if (!requested_)
            return;
#if defined(__linux__) && defined(__x86_64__) && defined(REG_RIP) && defined(SIGEV_THREAD_ID)
        if (ownsSlot_ || handlerInstalled_ || timerCreated_) {
            SignalBlock block(signal_);
            if (block.Good()) {
                CleanupBlocked();
                const int restore = block.Restore();
                maskRestored_ = restore == 0;
                if (restore)
                    Fail("cannot restore signal mask after sampler stop", restore);
            } else {
                // Best effort on an unavailable masking operation: disarm and
                // remove publication, but keep our non-dereferencing handler
                // installed rather than expose a pending RT signal to SIG_DFL.
                Fail("cannot block sampler signal during stop", block.Error());
                if (timerCreated_) {
                    const struct itimerspec zero{};
                    ::timer_settime(timer_, 0, &zero, nullptr);
                    timerDeleted_ = ::timer_delete(timer_) == 0;
                    timerCreated_ = false;
                }
                ReleaseSlot();
            }
        }
        ReadClocks(cpuEndNs_, wallEndNs_);
#endif
        WriteOutputs();
    }

  private:
    struct Slot {
        std::atomic<std::uintptr_t> pc{0};
        std::atomic<std::uint64_t> completedStep{0};
        std::atomic<int> overruns{0};
    };
    void Fail(const char* message, int code) noexcept {
        if (!error_) {
            error_ = message;
            errorCode_ = code;
        }
    }

#if defined(__linux__) && defined(__x86_64__) && defined(REG_RIP) && defined(SIGEV_THREAD_ID)
    class SignalBlock {
      public:
        explicit SignalBlock(int signal) noexcept {
            ::sigemptyset(&set_);
            ::sigaddset(&set_, signal);
            error_ = ::pthread_sigmask(SIG_BLOCK, &set_, &previous_);
            armed_ = error_ == 0;
        }
        ~SignalBlock() { Restore(); }
        bool Good() const noexcept { return armed_; }
        int Error() const noexcept { return error_; }
        const sigset_t& Previous() const noexcept { return previous_; }
        int Restore() noexcept {
            if (!armed_)
                return error_;
            error_ = ::pthread_sigmask(SIG_SETMASK, &previous_, nullptr);
            if (!error_)
                armed_ = false;
            return error_;
        }

      private:
        sigset_t set_{}, previous_{};
        int error_ = 0;
        bool armed_ = false;
    };

    static void Handle(int, siginfo_t* info, void* context) noexcept {
        const int savedErrno = errno;
        Session* session = active_.load(std::memory_order_acquire);
        if (session && info && context && info->si_code == SI_TIMER &&
            info->si_value.sival_ptr == session) {
            session->delivered_.fetch_add(1, std::memory_order_relaxed);
            const int overrun = info->si_overrun;
            if (overrun > 0) {
                session->overruns_.fetch_add(static_cast<std::uint64_t>(overrun),
                                            std::memory_order_relaxed);
                session->overrunSignals_.fetch_add(1, std::memory_order_relaxed);
            }
            const auto index = session->count_.load(std::memory_order_relaxed);
            if (index < kCapacity) {
                auto& slot = session->slots_[index];
                slot.pc.store(static_cast<std::uintptr_t>(
                                  static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP]),
                              std::memory_order_relaxed);
                slot.completedStep.store(session->completedStep_.load(std::memory_order_relaxed),
                                         std::memory_order_relaxed);
                slot.overruns.store(overrun, std::memory_order_relaxed);
                session->count_.store(index + 1, std::memory_order_release);
            } else {
                session->dropped_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        errno = savedErrno;
    }

    void ReleaseSlot() noexcept {
        if (ownsSlot_) {
            Session* mine = this;
            active_.compare_exchange_strong(mine, nullptr, std::memory_order_acq_rel);
            ownsSlot_ = false;
        }
    }
    void CleanupBlocked() noexcept {
        // Must run on the owner thread with our private signal blocked. Stop new
        // expirations and drain queued delivery before restoring SIG_DFL. If
        // either cannot be established, retain the inert handler for safety.
        bool timerQuiescent = !timerCreated_;
        if (timerCreated_) {
            const struct itimerspec zero{};
            const bool disarmed = ::timer_settime(timer_, 0, &zero, nullptr) == 0;
            if (!disarmed)
                Fail("cannot disarm CPU sampling timer", errno);
            timerDeleted_ = ::timer_delete(timer_) == 0;
            if (!timerDeleted_)
                Fail("cannot delete CPU sampling timer", errno);
            timerQuiescent = disarmed || timerDeleted_;
            timerCreated_ = !timerDeleted_;
        }
        ReleaseSlot();
        if (handlerInstalled_) {
            sigset_t only{};
            ::sigemptyset(&only);
            ::sigaddset(&only, signal_);
            const struct timespec zero{};
            siginfo_t pending{};
            bool drainedAll = false;
            while (timerQuiescent) {
                const int result = ::sigtimedwait(&only, &pending, &zero);
                if (result == signal_) {
                    ++drained_;
                    continue;
                }
                if (result < 0 && errno == EINTR)
                    continue;
                if (result < 0 && errno == EAGAIN)
                    drainedAll = true;
                else
                    Fail("cannot drain pending sampler signal", errno);
                break;
            }
            if (timerQuiescent && drainedAll) {
                handlerRestored_ = ::sigaction(signal_, &previousAction_, nullptr) == 0;
                if (!handlerRestored_)
                    Fail("cannot restore prior sampler signal disposition", errno);
                handlerInstalled_ = !handlerRestored_;
            }
        }
    }
    static void ReadClocks(std::uint64_t& cpu, std::uint64_t& wall) noexcept {
        struct timespec value{};
        if (::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) == 0)
            cpu = static_cast<std::uint64_t>(value.tv_sec) * 1000000000 + value.tv_nsec;
        if (::clock_gettime(CLOCK_MONOTONIC, &value) == 0)
            wall = static_cast<std::uint64_t>(value.tv_sec) * 1000000000 + value.tv_nsec;
    }
    struct ModuleWriter {
        std::ostream* out;
        const std::string* executable;
        std::uint64_t module = 0, segments = 0;
        bool failed = false;
    };
    static int Module(struct dl_phdr_info* info, std::size_t, void* opaque) noexcept {
        auto& writer = *static_cast<ModuleWriter*>(opaque);
        try {
            const std::string_view path = info->dlpi_name && *info->dlpi_name
                                              ? std::string_view(info->dlpi_name)
                                              : std::string_view(*writer.executable);
            for (unsigned index = 0; index < info->dlpi_phnum; ++index) {
                const auto& segment = info->dlpi_phdr[index];
                if (segment.p_type != PT_LOAD)
                    continue;
                auto& out = *writer.out;
                out << std::dec << writer.module << ',';
                CsvString(out, path);
                out << ",0x" << std::hex << info->dlpi_addr << ",0x" << segment.p_vaddr
                    << ",0x" << segment.p_memsz << ",0x" << segment.p_offset << ",0x"
                    << segment.p_flags << ",0x" << info->dlpi_addr + segment.p_vaddr << ",0x"
                    << info->dlpi_addr + segment.p_vaddr + segment.p_memsz << '\n';
                ++writer.segments;
            }
            ++writer.module;
            return 0;
        } catch (...) {
            writer.failed = true;
            return 1;
        }
    }
#endif

    static void CsvString(std::ostream& out, std::string_view text) {
        out << '"';
        for (char c : text) {
            if (c == '"')
                out << '"';
            out << c;
        }
        out << '"';
    }
    static void JsonString(std::ostream& out, std::string_view text) {
        constexpr char hex[] = "0123456789abcdef";
        out << '"';
        for (unsigned char c : text) {
            if (c == '"' || c == '\\')
                out << '\\' << char(c);
            else if (c < 32)
                out << "\\u00" << hex[c >> 4] << hex[c & 15];
            else
                out << char(c);
        }
        out << '"';
    }
    void WriteOutputs() noexcept {
        try {
            if (directory_.empty())
                return;
            std::filesystem::create_directories(directory_);
            std::ofstream csv(directory_ / "cpu-pc.csv");
            csv << "sample,pc,last_completed_step,reported_signal_overruns\n";
            const auto count = count_.load(std::memory_order_acquire);
            for (std::uint32_t index = 0; index < count; ++index) {
                const auto& slot = slots_[index];
                csv << std::dec << index << ",0x" << std::hex
                    << slot.pc.load(std::memory_order_relaxed) << std::dec << ','
                    << slot.completedStep.load(std::memory_order_relaxed) << ','
                    << slot.overruns.load(std::memory_order_relaxed) << '\n';
            }
            csv.flush();
            const bool samplesWritten = csv.good();
            bool mapsWritten = false, modulesWritten = false;
            std::uint64_t moduleCount = 0, segmentCount = 0;
#if defined(__linux__)
            {
                std::ifstream maps("/proc/self/maps", std::ios::binary);
                std::ofstream copy(directory_ / "maps.txt", std::ios::binary);
                if (maps && copy) {
                    copy << maps.rdbuf();
                    copy.flush();
                    mapsWritten = copy.good();
                }
            }
#if defined(__x86_64__) && defined(REG_RIP) && defined(SIGEV_THREAD_ID)
            {
                char buffer[4096];
                const auto bytes = ::readlink("/proc/self/exe", buffer, sizeof(buffer));
                const std::string executable = bytes > 0 && bytes < static_cast<ssize_t>(sizeof(buffer))
                                                   ? std::string(buffer, static_cast<std::size_t>(bytes))
                                                   : std::string("<main-executable-unavailable>");
                std::ofstream modules(directory_ / "modules.csv");
                modules << "module,path,load_bias,segment_vaddr,segment_memsz,file_offset,flags,"
                           "runtime_begin,runtime_end\n";
                ModuleWriter writer{&modules, &executable};
                ::dl_iterate_phdr(&Session::Module, &writer);
                modules.flush();
                moduleCount = writer.module;
                segmentCount = writer.segments;
                modulesWritten = modules.good() && !writer.failed;
            }
#endif
#endif
            if (!samplesWritten || !mapsWritten || !modulesWritten)
                Fail("one or more CPU sampler outputs could not be written", 0);
            std::ofstream meta(directory_ / "metadata.json");
            meta << "{\n  \"schema\": \"judas.main-thread-software-cpu-pc.v1\",\n"
                 << "  \"hardware_cycles\": false,\n  \"call_graph\": false,\n"
                 << "  \"clock\": \"CLOCK_THREAD_CPUTIME_ID\",\n"
                 << "  \"pid\": " << pid_ << ",\n  \"tid\": " << tid_
                 << ",\n  \"signal\": " << signal_ << ",\n  \"period_us\": " << periodUs_
                 << ",\n  \"capacity\": " << kCapacity << ",\n  \"started\": "
                 << (started_ ? "true" : "false") << ",\n  \"samples\": " << count
                 << ",\n  \"delivered_timer_signals\": " << delivered_.load()
                 << ",\n  \"capacity_drops\": " << dropped_.load()
                 << ",\n  \"observed_overruns\": " << overruns_.load()
                 << ",\n  \"signals_with_overruns\": " << overrunSignals_.load()
                 << ",\n  \"pending_signals_drained_at_stop\": " << drained_
                 << ",\n  \"cpu_start_ns\": " << cpuStartNs_ << ",\n  \"cpu_end_ns\": " << cpuEndNs_
                 << ",\n  \"monotonic_start_ns\": " << wallStartNs_
                 << ",\n  \"monotonic_end_ns\": " << wallEndNs_
                 << ",\n  \"handler_restored\": " << (handlerRestored_ ? "true" : "false")
                 << ",\n  \"timer_deleted\": " << (timerDeleted_ ? "true" : "false")
                 << ",\n  \"mask_restored\": " << (maskRestored_ ? "true" : "false")
                 << ",\n  \"samples_written\": " << (samplesWritten ? "true" : "false")
                 << ",\n  \"maps_written\": " << (mapsWritten ? "true" : "false")
                 << ",\n  \"modules_written\": " << (modulesWritten ? "true" : "false")
                 << ",\n  \"modules\": " << moduleCount << ",\n  \"load_segments\": " << segmentCount
                 << ",\n  \"error\": ";
            JsonString(meta, Error());
            meta << ",\n  \"error_code\": " << errorCode_
                 << ",\n  \"limitations\": ["
                    "\"Last completed step is observed at signal delivery, not an exact current-step attribution.\","
                    "\"Timer signals can coalesce or be delayed; overruns are observed only on delivered signals.\","
                    "\"Queued signals drained at stop have no recorded instruction pointer.\","
                    "\"Only the main thread is sampled; worker CPU and blocked wall time are absent.\","
                    "\"PC resolution uses ELF virtual address = runtime PC minus recorded module load bias.\","
                    "\"Samples are bounded and periodic software interrupts can be biased.\""
                    "]\n}\n";
            meta.flush();
            if (!samplesWritten || !mapsWritten || !modulesWritten || !meta.good())
                Fail("one or more CPU sampler outputs could not be written", 0);
        } catch (...) {
            Fail("CPU sampler output failed", 0);
        }
    }

    inline static std::atomic<Session*> active_{nullptr};
    std::filesystem::path directory_;
    std::unique_ptr<Slot[]> storage_;
    Slot* slots_ = nullptr;
    std::atomic<std::uint64_t> completedStep_{0}, delivered_{0}, dropped_{0}, overruns_{0},
        overrunSignals_{0};
    std::atomic<std::uint32_t> count_{0};
    std::uint64_t periodUs_ = 0, cpuStartNs_ = 0, cpuEndNs_ = 0, wallStartNs_ = 0, wallEndNs_ = 0,
                  drained_ = 0;
    const char* error_ = nullptr;
    int errorCode_ = 0, signal_ = 0;
    bool requested_ = false, started_ = false, stopped_ = false, ownsSlot_ = false,
         handlerInstalled_ = false, timerCreated_ = false, handlerRestored_ = false,
         timerDeleted_ = false, maskRestored_ = false;
    long pid_ = 0, tid_ = 0;
#if defined(__linux__) && defined(__x86_64__) && defined(REG_RIP) && defined(SIGEV_THREAD_ID)
    timer_t timer_{};
    struct sigaction previousAction_{};
#endif
};

} // namespace sampler
