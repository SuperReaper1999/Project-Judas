#include "AssetPublicationWorkflow.h"
#include "../AssetCook.h"
#include "AssetCookPanelTask.h"
#include "EditorPanels.h"
#include "ModelCook.h"
#include "ModelImportScale.h"
#include "Project.h"
#include <filesystem>

namespace {
void PublishFinishedEditorImport(EditorPanelState &state, EditorRequests &requests) {
    if (!state.importTask || !state.importTask->done.load(std::memory_order_acquire))
        return;
    auto &task = *state.importTask;
    if (task.job.IsValid() && state.importJobs && state.importJobs->IsFinished(task.job)) {
        state.importJobs->Forget(task.job);
        task.job = {};
    }
    if (!task.success) {
        if (!state.importPublished)
            state.status = "Import failed; last good model retained: " + task.error;
        state.importPublished = true;
        return;
    }
    if (state.mode != EditorMode::Edit || state.importPublished)
        return;
    std::string error;
    const auto relative = std::filesystem::path(task.output).lexically_relative(state.project->RootDir());
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
        task.cancel = true;
        state.status = "Import belongs to previous project; publication cancelled";
        state.importPublished = true;
        return;
    }
    if (EditorModelScaleReviewPending(state)) {
        state.status = "Model ready: review the possible 100x scale mismatch under Import & tools before publication.";
        if (state.importScalePrompted.lock() != state.importTask) {
            state.showAssetBrowser = true;
            state.selectModelImportTools = true;
            state.importScalePrompted = state.importTask;
        }
        return;
    }
    if (PublishModelImport(task, error)) {
        state.importAccepted = state.importTask;
        state.status = task.unchanged ? "Unchanged import; reused accepted model"
                                      : "Published complete model; sources retained";
        requests.rescanAssets = true;
        if (state.resources)
            state.resources->Invalidate(task.assetId);
        state.browserSelection = task.assetId;
    } else
        state.status = error;
    state.importPublished = true;
}
} // namespace

bool EditorModelScaleReviewPending(const EditorPanelState &state) {
    return state.importTask && state.importTask->done.load(std::memory_order_acquire) &&
           state.importTask->success && !state.importPublished &&
           state.importScaleConfirmed.lock() != state.importTask &&
           JudasImport::ScaleWarning(state.importTask->report);
}

void PumpEditorAssetWorkflow(EditorPanelState &state, EditorRequests &requests) {
    if (!state.project || !state.project->IsLoaded() || !state.assets || state.mode != EditorMode::Edit)
        return;
    PublishFinishedEditorImport(state, requests);
    if (auto task = state.assetCookTask;
        task && task->ready.load(std::memory_order_acquire) && state.mode == EditorMode::Edit) {
        if (task->root != state.project->RootDir())
            state.status = "Cook belongs to previous project; result discarded";
        else if (!task->error.empty())
            state.status = task->error;
        else {
            std::string error;
            if (PublishAssetCook(task->destination, task->bytes, task->recipe, error)) {
                auto relative = std::filesystem::path(task->destination)
                                    .lexically_relative(state.project->RootDir())
                                    .generic_string();
                if (auto *existing = state.assets->FindByRelativePath(relative)) {
                    if (state.resources)
                        state.resources->Invalidate(existing->id);
                    state.status = "Updated cooked generation; stable ID retained";
                } else
                    requests.trackAssetPath = task->destination;
            } else
                state.status = error;
        }
        state.assetCookTask.reset();
    }
}
