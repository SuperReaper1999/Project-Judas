#pragma once
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <utility>

struct EditorPanelState;
struct EditorRequests;
class Project;

// One editor-owned Linux tool process group. Never invokes a shell and never
// adopts/terminates another editor, compiler or preview server.
class BrowserToolJob {
public:
    ~BrowserToolJob();
    BrowserToolJob() = default;
    BrowserToolJob(const BrowserToolJob&) = delete;
    BrowserToolJob& operator=(const BrowserToolJob&) = delete;
    bool Start(const std::string& executable, const std::vector<std::string>& arguments,
               const std::filesystem::path& log,
               std::chrono::seconds limit = std::chrono::hours(2));
    void Poll();
    void Stop();
    bool Running() const { return m_pid > 0; }
    bool Succeeded() const { return m_success; }
    const std::string& Output() const { return m_output; }
    const std::string& Status() const { return m_status; }
    const std::filesystem::path& Log() const { return m_log; }
private:
    void ReadLog();
    int m_pid = -1;
    bool m_success = false;
    std::filesystem::path m_log;
    std::string m_output, m_status;
    std::chrono::steady_clock::time_point m_started{}, m_readAt{};
    std::chrono::seconds m_limit{7200};
};

struct BrowserExportWorkflow {
    BrowserToolJob job, preview;
    std::string python = "python3", script, sdk, cache, exporter, runtime, icu, destination;
    std::string operation, previewURL, projectFile, activeDestination, builtPackage;
    std::vector<std::pair<bool, std::string>> checks;
    std::string parsedCheckOutput;
    int jobs = 2, previewPort = 8081;
    bool initialized = false, ready = false, automaticCheck = false;
    bool playAfterBuild = true, buildHandled = true, openPreviewWhenReady = false;
    bool prepareNative = true, refreshExistingTab = false;
    void Initialize(const Project&);
    std::vector<std::string> Arguments(const Project&, bool check) const;
    void Start(const Project&, bool check);
    void Poll();
    std::string NextDestination() const;
    void StartPreview(bool openWhenReady);
};

void DrawBrowserExportWorkflow(EditorPanelState&, EditorRequests&);
void PollBrowserExportWorkflow(EditorPanelState&);
void DrawBrowserExportWindow(EditorPanelState&, EditorRequests&);
