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

EditorMilestoneNotice::Result EditorMilestoneNotice::Query(const std::atomic<bool>& cancelled, const std::string& channel) {
    Result result;
    const auto& identity = GetBuildIdentity();
    if (cancelled) return result;
    const auto endpoint = ReleaseManifestEndpoint(channel);
    if (endpoint.empty()) {
        result.diagnostic = "Select an explicit release channel in Editor preferences";
        return result;
    }
    std::string response, error;
    const auto curl = SystemHttpsProbeExecutable();
    if (curl.empty()) {
        result.diagnostic = "Update check unavailable (system HTTPS utility unavailable)";
        return result;
    }
    if (!CaptureToolOutput(curl, {"--disable", "--proto", "=https", "--proto-redir", "=https",
        "--tlsv1.2", "--fail", "--silent", "--location", "--max-redirs", "3", "--connect-timeout", "3",
        "--max-time", "6", "--max-filesize", "262144", "--header", "Accept: application/vnd.github+json",
        "--header", "User-Agent: Project-Judas-Update-Notice", endpoint}, response, error,
        std::chrono::milliseconds(7000), 262144)) {
        result.diagnostic = "Update check unavailable (channel not published yet, offline, service limit, or HTTPS utility unavailable)";
        return result;
    }
    if (cancelled) return result;
    return ParseReleaseUpdate(response, identity.version, channel, identity.platform,
        std::strcmp(identity.architecture, "AMD64") == 0 ? "x86_64" : identity.architecture);
}

void EditorMilestoneNotice::Check(bool manual, const std::string& requestedChannel) {
    if (m_pending.valid()) { m_manual = m_manual || manual; return; }
    m_manual = manual;
    const auto channel = requestedChannel.empty() ? std::string(GetBuildIdentity().channel) : requestedChannel;
    m_pending = std::async(std::launch::async, [this, channel] {
        try { return Query(m_cancelled, channel); } catch (...) { return Result{}; }
    });
}

void EditorMilestoneNotice::Draw(std::string& status, const std::string& channel) {
    if (m_pending.valid() && m_pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        m_result = m_pending.get();
        m_visible = m_result.valid && m_result.newer;
        if (m_manual) status = m_result.diagnostic.empty() ? "Update check unavailable" : m_result.diagnostic;
    }
    const auto selected = channel.empty() ? std::string(GetBuildIdentity().channel) : channel;
    if (m_result.channel != selected) m_visible = false;
    if (!m_visible) return;
    const auto& identity = GetBuildIdentity();
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 20,
        viewport->WorkPos.y + 75), ImGuiCond_Appearing, ImVec2(1, 0));
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);
    if (ImGui::Begin("Judas update available", &m_visible,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Available: %s [%s]", m_result.version.c_str(), m_result.channel.c_str());
        ImGui::Text("Installed: %s", identity.version);
        ImGui::TextWrapped("%s", m_result.notes.c_str());
        ImGui::TextWrapped("Current build: %s", identity.buildId);
        ImGui::TextWrapped("Notification only. Your installation and projects are unchanged.");
        ImGui::TextWrapped("Verify the downloaded file with the SDK release_manifest.py tool before extraction. SHA-256:");
        ImGui::TextWrapped("%s", m_result.sha256.c_str());
        if (ImGui::Button("Release notes")) SDL_OpenURL(m_result.notesUrl.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Download (browser)")) SDL_OpenURL(m_result.downloadUrl.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Dismiss")) m_visible = false;
    }
    ImGui::End();
}
