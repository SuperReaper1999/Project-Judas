#include "editor/BrowserExportWorkflow.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char* message) {
    ++checks; failures += !value;
    std::cout << (value ? "PASS " : "FAIL ") << message << '\n';
}
bool Finish(BrowserToolJob& job, int milliseconds = 5000) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (job.Running() && std::chrono::steady_clock::now() < end) {
        job.Poll(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return !job.Running();
}
}
int main() {
#if !defined(__linux__) || defined(__ANDROID__)
    std::cout << "SKIP: browser compiler process ownership currently targets Linux\n";
    return 0;
#else
    const auto root = fs::temp_directory_path() / ("judas-browser-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    {
        BrowserToolJob job;
        const std::string literal = "space ' ; $(touch should-never-exist)";
        Check(job.Start("python3", {"-c", "import json,sys;print(json.dumps(sys.argv[1:]));print('stderr retained',file=sys.stderr)", literal}, root/"argv.log"), "start without a shell");
        Check(!job.Start("python3", {"-c", "print('wrong')"}, root/"other.log"), "running job cannot be silently replaced");
        Check(Finish(job) && job.Succeeded(), "own child completes and is reaped");
        Check(job.Output().find(literal) != std::string::npos && job.Output().find("stderr retained") != std::string::npos, "quotes/metacharacters remain literal and stderr is retained");
        Check(!fs::exists("should-never-exist"), "no command substitution executed");
        Check(!job.Start("/no/such/judas-tool", {}, root/"missing.log") && !job.Running(), "missing tool reports launch failure without a child");
        Check(!job.Start("python3", {}, root/"argv.log"), "an existing log is never overwritten");
    }
    {
        BrowserToolJob stopped, independent;
        const auto forbidden = root/"descendant-survived", allowed = root/"independent-finished";
        const auto child = "import signal,time,pathlib;signal.signal(signal.SIGTERM,signal.SIG_IGN);time.sleep(1);pathlib.Path(" +
            nlohmann::json(forbidden.string()).dump() + ").write_text('bad')";
        const auto parent = "import subprocess,sys,time;subprocess.Popen([sys.executable,'-c'," +
            nlohmann::json(child).dump() + "]);print('descendant started',flush=True);time.sleep(10)";
        Check(stopped.Start("python3", {"-c",parent}, root/"stopped.log"), "start compiler-like descendant tree");
        Check(independent.Start("python3", {"-c", "import pathlib,time;time.sleep(.7);pathlib.Path(" +
            nlohmann::json(allowed.string()).dump() + ").write_text('ok')"}, root/"independent.log"), "start unrelated owned job in a separate group");
        for(int i=0;i<100 && stopped.Output().find("descendant started")==std::string::npos;++i) {
            stopped.Poll();std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        Check(stopped.Output().find("descendant started")!=std::string::npos,"real descendant is running before cancellation");
        stopped.Stop();
        Check(!stopped.Running() && !stopped.Succeeded() && fs::exists(stopped.Log()), "cancellation reaps parent and preserves log");
        Check(Finish(independent) && independent.Succeeded() && fs::exists(allowed), "cancelling one group preserves other processes");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        Check(!fs::exists(forbidden), "SIGTERM-resistant compiler descendant does not survive cancellation");
    }
    {
        BrowserToolJob timeout;
        Check(timeout.Start("python3", {"-c","import time;time.sleep(10)"}, root/"timeout.log", std::chrono::seconds(1)), "start bounded tool");
        Check(Finish(timeout) && !timeout.Succeeded() && timeout.Status().find("bound")!=std::string::npos, "timeout terminates its owned process group");
    }
    {
        BrowserToolJob noisy;
        Check(noisy.Start("python3", {"-c","import sys,time;sys.stdout.write('x'*(17*1024*1024));sys.stdout.flush();time.sleep(5)"}, root/"noisy.log"), "start excessive-output control");
        Check(Finish(noisy) && !noisy.Succeeded() && noisy.Output().size()<=64*1024, "excessive logs stop safely with a bounded display tail");
    }
    fs::remove_all(root);
    std::cout << "BrowserWorkflowTests: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
#endif
}
