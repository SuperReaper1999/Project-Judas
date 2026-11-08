#include "AssetBrowserPanel.h"
#include "EditorPanels.h"
#include "EditorVisuals.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "ResourceManager.h"
#include "EditorTheme.h"
#include <algorithm>
#include "imgui.h"
#include "imgui_internal.h"

EditorAssetThumbnailDemand::~EditorAssetThumbnailDemand() {
    Reset();
}

void EditorAssetThumbnailDemand::Reset() {
    // Requests can replace project resources after panels were drawn but before
    // ImGui submits their images. Retire only this frame's sampled image tokens
    // while the renderer still exists; no draw command may bind a deleted name.
    if (auto *context = ImGui::GetCurrentContext();
        resources && context && imageFrame == context->FrameCount && !frameImages.empty()) {
        if (auto *renderer = resources->GetRenderer()) {
            const auto fallback = ImTextureID(renderer->EditorImageToken({}));
            for (auto *window : context->Windows)
                if (window->LastFrameActive == imageFrame && window->DrawList)
                    for (auto &command : window->DrawList->CmdBuffer)
                        if (frameImages.count(std::uintptr_t(command.GetTexID())))
                            command.TextureId = fallback;
        }
    }
    frameImages.clear();
    imageFrame = -1;
    if (resources)
        for (const auto &id : held)
            resources->ReleaseRef(id);
    held.clear();
    wanted.clear();
    requested = 0;
    resources = nullptr;
}

void EditorAssetThumbnailDemand::Begin(ResourceManager *manager) {
    if (resources != manager)
        Reset();
    resources = manager;
    const auto *context = ImGui::GetCurrentContext();
    const int currentFrame = context ? context->FrameCount : -1;
    if (currentFrame != imageFrame) {
        frameImages.clear();
        imageFrame = currentFrame;
    }
    wanted.clear();
    requested = 0;
}

bool EditorAssetThumbnailDemand::FitsPinBudget(const std::string &id) const {
    if (!resources)
        return false;
    const auto limit = std::min(MaxPinnedBytes, resources->BudgetBytes() / 4);
    auto bytes = resources->BytesOf(id);
    if (bytes > limit)
        return false;
    for (const auto &other : held) {
        if (other == id)
            continue;
        const auto amount = resources->BytesOf(other);
        if (amount > limit - bytes)
            return false;
        bytes += amount;
    }
    return true;
}

unsigned EditorAssetThumbnailDemand::PendingCount() const {
    unsigned pending = 0;
    if (resources)
        for (const auto &id : held) {
            const auto state = resources->StateOf(id);
            pending += state == ResourceState::Queued || state == ResourceState::Loading ||
                       state == ResourceState::CpuReady;
        }
    return pending;
}

bool EditorAssetThumbnailDemand::Want(const std::string &id, bool sheet) {
    if (!resources || id.empty())
        return false;
    const auto state = resources->StateOf(id);
    if (state == ResourceState::Failed)
        return false;
    wanted.insert(id);
    if (state == ResourceState::Ready) {
        // Large already-resident images may still be shown, without pinning
        // them beyond the manager's ordinary residency/eviction policy.
        if (!held.count(id) && held.size() < MaxActive && FitsPinBudget(id)) {
            resources->AddRef(id);
            held.insert(id);
        }
        return true;
    }
    if (!resources->Jobs() || resources->BlockingMode())
        return false;
    if (!held.count(id)) {
        if (held.size() >= MaxActive || requested >= RequestsPerFrame || PendingCount() >= MaxPending)
            return false;
        resources->AddRef(id);
        held.insert(id);
    }
    if (state == ResourceState::Unloaded || state == ResourceState::Cancelled) {
        if (requested >= RequestsPerFrame || PendingCount() >= MaxPending)
            return false;
        ++requested;
        if (sheet) {
            std::string error;
            resources->GetSpriteSheet(id, error);
        } else
            resources->RequestTexture(id, JobPriority::Low);
    }
    return true;
}

EditorAssetThumbnail EditorAssetThumbnailDemand::Resolve(const AssetRecord &record) {
    if (!resources || record.missing || !resources->GetRenderer())
        return {};
    std::string texture = record.id;
    if (record.type == AssetType::SpriteSheet) {
        if (!Want(record.id, true) || resources->StateOf(record.id) != ResourceState::Ready)
            return {};
        std::string error;
        const auto sheet = resources->GetSpriteSheet(record.id, error);
        if (!sheet)
            return {};
        texture = sheet->texture;
    } else if (record.type != AssetType::Texture)
        return {};
    if (!Want(texture))
        return {};
    auto handle = resources->TryGetTexture(texture);
    auto *renderer = resources->GetRenderer();
    const auto size = renderer->TextureSize(handle);
    const auto token = renderer->EditorImageToken(handle);
    if (size.x > 0 && size.y > 0 && ImGui::GetCurrentContext())
        frameImages.insert(token);
    return {token, size.x, size.y};
}

void EditorAssetThumbnailDemand::End() {
    for (auto it = held.begin(); it != held.end();) {
        if (!wanted.count(*it) || (resources->StateOf(*it) == ResourceState::Ready && !FitsPinBudget(*it))) {
            resources->ReleaseRef(*it);
            it = held.erase(it);
        } else
            ++it;
    }
}

void ResetEditorAssetThumbnails(EditorPanelState &state) {
    if (state.assetBrowser && state.assetBrowser->thumbnails)
        state.assetBrowser->thumbnails->Reset();
}

void PumpEditorAssetThumbnails(EditorPanelState &state) {
    if (!state.showAssetBrowser || state.mode != EditorMode::Edit || !state.assetBrowser ||
        state.assetBrowser->page != 0 || !state.assetBrowser->grid)
        ResetEditorAssetThumbnails(state);
}

void DrawAssetBrowserPanel(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests) {
    if (!state.showAssetBrowser)
        return;
    if (state.selectAssetDetails || state.selectModelImportTools)
        ImGui::SetNextWindowFocus();
    if (!BeginEditorPanel("Asset Browser", &state.showAssetBrowser, state)) {
        ResetEditorAssetThumbnails(state);
        ImGui::End();
        return;
    }
    const bool hasProject = state.project && state.project->IsLoaded() && state.assets;
    if (!hasProject) {
        ResetEditorAssetThumbnails(state);
        ImGui::Spacing();
        EditorVisuals::IconText(EditorVisuals::Icon::Folder, "Your project's content", 22);
        ImGui::TextWrapped("Open or create a project to import models, images, scripts and audio. "
                           "Assets stay with your project and can be dragged into the scene.");
        ImGui::Spacing();
        if (ImGui::Button("Create project..."))
            requests.newProject = true;
        ImGui::SameLine();
        if (ImGui::Button("Open project..."))
            requests.openProject = true;
        ImGui::End();
        return;
    }
    const AssetDatabase &db = *state.assets;
    if (!state.assetBrowser)
        state.assetBrowser = std::make_shared<EditorAssetBrowserState>();
    auto &browser = *state.assetBrowser;
    if (state.selectAssetDetails) {
        browser.page = 1;
        state.selectAssetDetails = false;
    }
    if (state.selectModelImportTools)
        browser.page = 2;
    if (browser.page != 0) {
        ResetEditorAssetThumbnails(state);
        if (EditorVisuals::ActionButton("Back to content", EditorVisuals::Icon::Folder)) {
            browser.page = 0;
            state.revealModelStructure = false;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", browser.page == 1 ? "Selected asset" : "Asset tools");
        ImGui::Separator();
    }
    if (browser.page == 0)
        DrawAssetList(doc, state, requests, db);
    else if (browser.page == 1)
        DrawAssetDetails(doc, state, requests, db);
    else {
        const bool revealModelTools = state.selectModelImportTools;
        state.selectModelImportTools = false;
        DrawAssetImportTools(doc, state, requests, db, revealModelTools);
    }
    ImGui::End();
}
