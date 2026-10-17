#include "EditorMilestoneNotice.h"
#include "BuildIdentity.h"
#include "ToolProcess.h"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <chrono>
#include <cstring>

EditorMilestoneNotice::~EditorMilestoneNotice() {
    m_cancelled = true;
    if (m_pending.valid()) m_pending.wait();
}

EditorMilestoneNotice::Result EditorMilestoneNotice::Query(const std::atomic<bool>& cancelled) {
    Result result;
    const auto& identity = GetBuildIdentity();
    if (cancelled) return result;
    if (std::strcmp(identity.sourceOrigin, "unknown") == 0 ||
        (std::strcmp(identity.channel, "main") != 0 && std::strcmp(identity.channel, "Experimental") != 0)) {
        result.diagnostic = "Update check requires known provenance and a main or Experimental channel";
        return result;
    }
    // Public, read-only HTTPS; no GitHub CLI, login, repository write or updater.
    // Curl is an optional platform utility (included with supported Windows).
    // Its absence, timeout, rate limiting and offline operation are nonfatal.
    const auto endpoint = std::string("https://api.github.com/repos/SuperReaper1999/Project-Judas/compare/") +
        identity.sourceRevision + "..." + identity.channel + "?per_page=1";
    std::string response, error;
    const auto curl = SystemHttpsProbeExecutable();
    if (curl.empty()) {
        result.diagnostic = "Update check unavailable (system HTTPS utility unavailable)";
        return result;
    }
    if (!CaptureToolOutput(curl, {"--disable", "--proto", "=https", "--proto-redir", "=https",
        "--tlsv1.2", "--fail", "--silent", "--location", "--max-redirs", "3", "--connect-timeout", "3",
        "--max-time", "6", "--max-filesize", "1048576", "--header", "Accept: application/vnd.github+json",
        "--header", "User-Agent: Project-Judas-Update-Notice", endpoint}, response, error,
        std::chrono::milliseconds(7000), 1048576)) {
        result.diagnostic = "Update check unavailable (offline, service limit, or HTTPS utility unavailable)";
        return result;
    }
    if (cancelled) return result;
    return ParseBuildUpdateComparison(response);
}

void EditorMilestoneNotice::Check(bool manual) {
    if (m_pending.valid()) { m_manual = m_manual || manual; return; }
    m_manual = manual;
    m_pending = std::async(std::launch::async, [this] {
        try { return Query(m_cancelled); } catch (...) { return Result{}; }
    });
}

void EditorMilestoneNotice::Draw(std::string& status) {
    if (m_pending.valid() && m_pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_result = m_pending.get();
        m_visible = m_result.available && m_result.newer;
        if (m_manual) status = m_result.diagnostic.empty() ? "Update check unavailable" : m_result.diagnostic;
    }
    if (!m_visible) return;
    const auto& identity = GetBuildIdentity();
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 20,
        viewport->WorkPos.y + 75), ImGuiCond_Appearing, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);
    if (ImGui::Begin("New Judas build available", &m_visible,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("%u newer commits on %s", m_result.commitsAhead, identity.channel);
        ImGui::TextWrapped("Current build: %s", identity.buildId);
        ImGui::TextWrapped("Notification only. Your installation and projects are unchanged.");
        if (ImGui::Button("View changes")) {
            const auto url = std::string("https://github.com/SuperReaper1999/Project-Judas/compare/") +
                identity.sourceRevision + "..." + identity.channel;
            SDL_OpenURL(url.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Dismiss")) m_visible = false;
    }
    ImGui::End();
}
