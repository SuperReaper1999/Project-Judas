// Explicit local integration command; never installs an SDK or runs in default CI.
#include "editor/BrowserExportWorkflow.h"
#include "Project.h"
#include "ToolProcess.h"
#include <filesystem>
#include <iostream>
#include <thread>

bool Finish(BrowserToolJob& job) {
    while (job.Running()) { job.Poll(); std::this_thread::sleep_for(std::chrono::milliseconds(50)); }
    std::cout << job.Output() << '\n';
    return job.Succeeded();
}
int main(int argc, char** argv) {
    if (argc != 9) {
        std::cerr << "usage: browser-export-smoke PROJECT COMPILER_SCRIPT EMSDK CACHE EXPORTER RUNTIME ICU_BUILD_OR_DASH FRESH_OUTPUT\n";
        return 2;
    }
    Project project; std::string error;
    if (!project.Load(argv[1], error)) { std::cerr << error; return 1; }
    BrowserExportWorkflow flow;
    flow.Initialize(project);
    flow.script = std::filesystem::absolute(argv[2]).string();
    flow.sdk = std::filesystem::absolute(argv[3]).string();
    flow.cache = std::filesystem::absolute(argv[4]).string();
    flow.exporter = std::filesystem::absolute(argv[5]).string();
    flow.runtime = std::filesystem::absolute(argv[6]).string();
    if (std::string(argv[7]) != "-") flow.icu = std::filesystem::absolute(argv[7]).string();
    flow.destination = std::filesystem::absolute(argv[8]).string();
    flow.Start(project, true);
    if (!Finish(flow.job)) return 1;
    // Cancel the actual compiler after it acquires its cache lock, then prove
    // that a subsequent build can use the cache without manual lock deletion.
    flow.Start(project, false);
    const auto lock=std::filesystem::path(flow.cache)/".browser-export.lock";
    const auto cancelDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while (flow.job.Running() && !std::filesystem::exists(lock) && std::chrono::steady_clock::now()<cancelDeadline) {
        flow.Poll();std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!std::filesystem::exists(lock)) {std::cerr<<flow.job.Output();return 1;}
    flow.job.Stop();
    if(std::filesystem::exists(lock) || std::filesystem::exists(flow.destination)) {
        std::cerr<<"Cancelled build left an owned lock or published incomplete output";return 1;
    }
    std::cout<<"PASS actual compiler cancellation removes cache lock and does not publish output\n";
    flow.Start(project, false);
    if (!Finish(flow.job)) return 1;
    flow.preview.Start(flow.python, {(std::filesystem::path(flow.script).parent_path()/"serve_browser.py").string(),
        flow.destination, "--port", "0"}, flow.destination+"-preview.log", std::chrono::seconds(20));
    const auto end = std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while (flow.preview.Running() && flow.previewURL.empty() && std::chrono::steady_clock::now()<end) {
        flow.Poll(); std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (flow.previewURL.empty()) { std::cerr << flow.preview.Output(); return 1; }
    std::string output;
    const auto command = "import sys,urllib.request;r=urllib.request.urlopen(sys.argv[1],timeout=3);assert r.status==200;assert r.headers['Cross-Origin-Opener-Policy']=='same-origin';assert r.headers['Cross-Origin-Embedder-Policy']=='require-corp';assert b'<canvas' in r.read();print('PASS real editor-owned package preview and isolation headers')";
    if (!CaptureToolOutput("python3", {"-c",command,flow.previewURL}, output,error)) { std::cerr<<error;return 1; }
    std::cout << output;
    flow.preview.Stop();
    if (CaptureToolOutput("python3", {"-c","import sys,urllib.request;urllib.request.urlopen(sys.argv[1],timeout=1)",flow.previewURL}, output,error)) {
        std::cerr << "Preview survived Stop"; return 1;
    }
    std::cout << "PASS editor-owned check, actual build, verified preview and clean shutdown\n";
    return 0;
}
