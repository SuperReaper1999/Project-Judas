#include "Prefab.h"
#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <set>
#include <sstream>

using namespace EditorPanelWidgets;
#include "../../third_party/nlohmann/json.hpp"
#include "../AssetCook.h"
#include "AssetBrowserPanel.h"
#include "AssetCookPanelTask.h"
#include "CollisionAsset.h"
#include "ContentReferences.h"
#include "EditorAssetActions.h"
#include "EditorScriptWorkflow.h"
#include "Environment.h"
#include "ModelCook.h"
#include "NamedAuthoring.h"
#include "PoseComposition.h"
#include "SpriteSheetEditor.h"

void DrawAssetDetails(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests,
                      const AssetDatabase &db) {
    ImGui::BeginChild("assetDetails", {0, 0});
    if (state.browserSelection.empty())
        ImGui::TextWrapped("Select an asset to inspect its source, dependencies and editing actions.");
    if (!state.browserSelection.empty()) {
        if (const AssetRecord *record = db.Find(state.browserSelection)) {
            ImGui::Separator();
            ImGui::TextWrapped("Selected: %s", record->relativePath.c_str());
            ImGui::BeginDisabled(record->missing);
            if (ImGui::Button("Open file"))
                requests.openExternalPath = record->path;
            ImGui::EndDisabled();
            ToolbarHint("Open this file with your desktop's associated application");
            ImGui::SameLine();
            if (ImGui::Button("Copy path"))
                ImGui::SetClipboardText(record->path.c_str());
            ImGui::TextWrapped("Provenance: %s",
                               record->source.empty() ? "project-owned" : record->source.c_str());
            if ((record->type == AssetType::Mesh || record->type == AssetType::Texture) &&
                ImGui::TreeNode("Cook / LOD / residency")) {
                static int maxSize = 2048, levels = 4, semantic = 0;
                static float error = .1f, pixels = .5f;
                static bool lossless = false;
                auto ext = std::filesystem::path(record->path).extension();
                bool cooked = ext == ".judasmesh" || ext == ".judastex";
                static std::string recipeSelection;
                if (recipeSelection != record->id) {
                    recipeSelection = record->id;
                    if (cooked)
                        try {
                            std::ifstream f(record->path + ".judascook");
                            nlohmann::json j;
                            f >> j;
                            maxSize = j.at("textureMax");
                            levels = j.at("levels");
                            semantic = j.at("semantic");
                            error = j.at("error");
                            pixels = j.at("pixels");
                            lossless = j.at("textureMode").get<unsigned>() != 0;
                        } catch (...) {
                        }
                }
                ImGui::TextWrapped(
                    "Cooked resources are separate assets. Source files and existing scene references "
                    "remain intact. Drag the cooked asset onto a normal mesh/texture field.");
                if (record->type == AssetType::Texture) {
                    ImGui::InputInt("Maximum dimension", &maxSize);
                    ImGui::Combo("Texture role", &semantic,
                                 "sRGB colour\0Linear data\0Tangent normal\0");
                    ImGui::Checkbox("Lossless RGBA (no BC)", &lossless);
                } else {
                    ImGui::SliderInt("LOD levels", &levels, 1, 8);
                    ImGui::DragFloat("Maximum object-space error", &error, .005f, 0, 10);
                    ImGui::DragFloat("Screen error pixels", &pixels, .1f, 0, 32);
                }
                ImGui::BeginDisabled(record->missing || record->readOnly ||
                                     state.mode != EditorMode::Edit || bool(state.assetCookTask) ||
                                     !state.importJobs);
                if (ImGui::Button(cooked ? "Recook recorded source" : "Cook optimized copy")) {
                    auto task = std::make_shared<AssetCookPanelTask>();
                    task->root = state.project->RootDir();
                    task->source = record->path;
                    if (cooked) {
                        try {
                            std::ifstream f(record->path + ".judascook");
                            nlohmann::json j;
                            f >> j;
                            task->source = (std::filesystem::path(record->path).parent_path() /
                                            j.at("source").get<std::string>())
                                               .lexically_normal()
                                               .string();
                            task->destination = record->path;
                        } catch (...) {
                            state.status = "No authoring cook recipe; use the original source asset";
                        }
                    } else {
                        auto path = std::filesystem::path(record->path);
                        task->destination =
                            (path.parent_path() /
                             (path.stem().string() + ".packed" +
                              (record->type == AssetType::Mesh ? ".judasmesh" : ".judastex")))
                                .string();
                    }
                    if (!task->destination.empty()) {
                        AssetCookSettings settings;
                        settings.maxTextureSize = unsigned(std::max(1, maxSize));
                        settings.lodLevels = unsigned(levels);
                        settings.lodMaxError = error;
                        settings.lodPixels = pixels;
                        settings.textureSemantic = unsigned(semantic);
                        settings.textureMode = lossless ? 1 : 0;
                        task->job = state.importJobs->Submit(
                            [task, settings](JobContext &context) {
                                if (!context.CancelRequested())
                                    PrepareAssetCook(task->source, task->destination, settings,
                                                     task->bytes, task->recipe, task->error);
                                else
                                    task->error = "Cook cancelled";
                                task->ready.store(true, std::memory_order_release);
                            },
                            JobPriority::Normal, "Asset pipeline cook");
                        if (task->job.IsValid())
                            state.assetCookTask = task;
                        else
                            state.status = "Cook worker unavailable";
                    }
                }
                ImGui::EndDisabled();
                if (state.assetCookTask)
                    ImGui::TextDisabled("Cooking on a worker; publication waits for Edit mode.");
                if (cooked && ImGui::Button("Verify cook source/generation")) {
                    std::string reason;
                    bool fresh = CookedAssetFresh(record->path, reason);
                    state.status =
                        fresh
                            ? "Cook generation current (or runtime-only asset without authoring recipe)"
                            : reason;
                }
                ImGui::TreePop();
            }

            static uint64_t usersGeneration = ~uint64_t(0);
            static std::string usersAsset;
            static std::vector<SceneObjectId> users;
            if (usersGeneration != doc.Generation() || usersAsset != record->id) {
                users.clear();
                usersGeneration = doc.Generation();
                usersAsset = record->id;
                for (auto &o : doc.GetScene().Objects())
                    for (auto &[key, value] : ObjectProperties(o))
                        if (value.find(record->id) != std::string::npos) {
                            users.push_back(o.id);
                            break;
                        }
            }
            if (ImGui::TreeNode("Current scene users")) {
                for (auto id : users)
                    if (auto *user = doc.GetScene().Find(id))
                        if (ImGui::Selectable(
                                (user->name + " ##asset-user" + std::to_string(id)).c_str()))
                            doc.Select(id);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Asset dependencies")) {
                static std::string dependencyKey, dependencyError;
                static std::set<AssetId> dependencies;
                auto key = record->id + record->path;
                if (dependencyKey != key) {
                    dependencyKey = key;
                    DirectAssetDependencies(*record, dependencies, dependencyError);
                }
                if (ImGui::SmallButton("Refresh dependency source"))
                    DirectAssetDependencies(*record, dependencies, dependencyError);
                for (auto &id : dependencies) {
                    auto *target = db.Find(id);
                    ImGui::TextWrapped("%s / %s", id.c_str(),
                                       target ? target->relativePath.c_str() : "MISSING");
                }
                if (!dependencyError.empty())
                    ImGui::TextWrapped("%s", dependencyError.c_str());
                ImGui::TextWrapped("Typed prefab/UI/material/model/collision references. Dynamic script "
                                   "IDs use the registered-asset export policy.");
                ImGui::TreePop();
            }
            if (record->type == AssetType::Mesh && state.resources) {
                if (auto model = state.resources->TryGetSkeletal(record->id)) {
                    if (ImGui::TreeNode("Shared skeleton")) {
                        for (auto &name : model->skeleton.names)
                            ImGui::TextUnformatted(name.c_str());
                        ImGui::TreePop();
                    }
                }
            }

            if (record->type == AssetType::Collision && state.mode == EditorMode::Edit) {
                CollisionAsset asset;
                std::string error;
                if (LoadCollisionAsset(record->path, asset, error)) {
                    ImGui::Text("Physical %s: %zu vertices / %zu faces",
                                asset.convex ? "hull" : "triangle surface", asset.vertices.size(),
                                asset.faces.size());
                    auto source = db.Find(asset.sourceAsset);
                    bool stale =
                        !source || source->missing || CollisionAssetStale(asset, source->path, error);
                    ImGui::TextWrapped("%s",
                                       stale ? error.c_str() : "Source/settings fingerprint current");
                    if (ImGui::Button("Rebake saved source/settings") && source) {
                        CollisionCookSettings settings;
                        settings.convex = asset.convex;
                        settings.twoSided = asset.twoSided;
                        settings.primitive = asset.selectedPrimitive;
                        settings.transform = asset.sourceTransform;
                        ModelCollisionCleanup cleanup;
                        ModelCollisionCleanupReport cleanupReport;
                        CollisionDiagnostic diagnostic;
                        if (ReadModelCollisionCleanup(record->path, cleanup, error) &&
                            CookImportedCollisionFile(source->path, source->id, settings, cleanup,
                                                      record->path, asset, cleanupReport, diagnostic,
                                                      error)) {
                            if (state.resources)
                                state.resources->Invalidate(record->id);
                            state.status = "Rebaked physical collider";
                        } else
                            state.status = error;
                    }
                    for (auto &warning : asset.warnings)
                        ImGui::TextWrapped("%s", warning.c_str());
                } else
                    ImGui::TextWrapped("%s", error.c_str());
            }
            if (record->type == AssetType::Material && state.mode == EditorMode::Edit) {
                static std::string selected, error;
                static MaterialDefinition draft;
                if (selected != record->id) {
                    selected = record->id;
                    LoadMaterial(record->path, draft, error);
                }
                if (ImGui::CollapsingHeader("Edit SHARED material source",
                                            ImGuiTreeNodeFlags_DefaultOpen)) {
                    int model = int(draft.model), alpha = int(draft.alpha);
                    const char *models[] = {"Legacy", "PBR metallic / roughness", "Unlit"};
                    const char *modes[] = {"Opaque", "Alpha cutout", "Alpha blend"};
                    if (ImGui::Combo("Model", &model, models, 3))
                        draft.model = MaterialModel(model);
                    if (ImGui::Combo("Alpha mode", &alpha, modes, 3))
                        draft.alpha = MaterialAlpha(alpha);
                    ImGui::ColorEdit4("Base colour factor (linear)", &draft.baseColor.x);
                    ImGui::SliderFloat("Metallic", &draft.metallic, 0, 1);
                    ImGui::SliderFloat("Roughness", &draft.roughness, 0, 1);
                    ImGui::ColorEdit3("Emission (linear)", &draft.emissive.x);
                    ImGui::DragFloat("Emission intensity", &draft.emissiveIntensity, .05f, 0, 100000);
                    ImGui::SliderFloat("Normal strength", &draft.normalStrength, 0, 8);
                    ImGui::SliderFloat("Occlusion strength", &draft.occlusionStrength, 0, 1);
                    ImGui::SliderFloat("Alpha cutoff", &draft.alphaCutoff, 0, 1);
                    ImGui::Checkbox("Double sided", &draft.doubleSided);
                    ImGui::Checkbox("Flip texture V", &draft.flipV);
                    ImGui::DragFloat2("UV tiling", &draft.uvScale.x, .01f);
                    ImGui::DragFloat2("UV offset", &draft.uvOffset.x, .01f);
                    const char *labels[] = {"Base colour sRGB", "Metallic B / roughness G linear",
                                            "Normal linear", "Occlusion R linear", "Emission sRGB"};
                    for (size_t i = 0; i < 5; ++i) {
                        auto &map = draft.maps[i];
                        std::string label = map.asset.empty() ? "(none)" : map.asset;
                        if (ImGui::BeginCombo(labels[i], label.c_str())) {
                            if (ImGui::Selectable("(none)", map.asset.empty()))
                                map.asset.clear();
                            for (const auto &[id, a] : db.Records())
                                if (a.type == AssetType::Texture &&
                                    ImGui::Selectable(a.relativePath.c_str(), map.asset == id))
                                    map.asset = id;
                            ImGui::EndCombo();
                        }
                        ImGui::PushID(int(i));
                        const char *wrap[] = {"Repeat", "Clamp", "Mirror"};
                        int values[] = {10497, 33071, 33648};
                        int ws = map.sampler.wrapS == 33071   ? 1
                                 : map.sampler.wrapS == 33648 ? 2
                                                              : 0,
                            wt = map.sampler.wrapT == 33071   ? 1
                                 : map.sampler.wrapT == 33648 ? 2
                                                              : 0;
                        if (ImGui::Combo("Wrap U", &ws, wrap, 3))
                            map.sampler.wrapS = values[ws];
                        if (ImGui::Combo("Wrap V", &wt, wrap, 3))
                            map.sampler.wrapT = values[wt];
                        ImGui::PopID();
                    }
                    ImGui::TextWrapped("In-scene preview: assign this asset to a render slot. Saving "
                                       "intentionally updates all non-overridden users.");
                    if (ImGui::Button("Save shared material")) {
                        if (SaveMaterial(record->path, draft, error)) {
                            if (state.resources)
                                state.resources->Invalidate(record->id);
                            state.status = "Saved shared material; instances reload normally";
                        } else
                            state.status = error;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reload source"))
                        LoadMaterial(record->path, draft, error);
                    if (!error.empty())
                        ImGui::TextWrapped("%s", error.c_str());
                }
            }
            if (record->type == AssetType::AudioEffect && state.mode == EditorMode::Edit &&
                ImGui::CollapsingHeader("Edit shared reverb settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                static std::string selected, error;
                static AudioEnvironmentSettings draft;
                if (selected != record->id) {
                    selected = record->id;
                    LoadAudioEnvironment(record->path, draft, error);
                }
                ImGui::SliderFloat("Room feedback", &draft.roomSize, 0, 1);
                ImGui::SliderFloat("High frequency damping", &draft.damping, 0, 1);
                ImGui::SliderFloat("Stereo width", &draft.width, 0, 1);
                ImGui::SliderFloat("Wet level", &draft.wet, 0, 1);
                if (ImGui::Button("Save shared reverb")) {
                    std::ofstream f(record->path);
                    f << SerializeAudioEnvironment(draft);
                    if (f) {
                        if (state.resources)
                            state.resources->Invalidate(record->id);
                        state.status = "Saved reverb settings";
                        error.clear();
                    } else
                        error = "Cannot write reverb settings";
                }
                ImGui::SameLine();
                if (ImGui::Button("Reload reverb"))
                    LoadAudioEnvironment(record->path, draft, error);
                if (!error.empty())
                    ImGui::TextWrapped("%s", error.c_str());
            }
            if (record->type == AssetType::World && state.mode == EditorMode::Edit &&
                ImGui::CollapsingHeader("World manifest source", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped(
                    "Edit this registered manifest through the shared named document editor. "
                    "Validation, undo and external-change protection use the same service as the CLI.");
                if (ImGui::Button("Open world/source authoring tools"))
                    state.showWorldBuilding = true;
                ImGui::TextWrapped("Path: %s", record->path.c_str());
            }
            if (record->type == AssetType::Catalog && state.mode == EditorMode::Edit &&
                ImGui::CollapsingHeader("Catalog UTF-8 source")) {
                static std::string catalogPath;
                static std::vector<char> buffer(2 * 1024 * 1024 + 1);
                if (catalogPath != record->path) {
                    catalogPath = record->path;
                    std::ifstream f(catalogPath);
                    std::string text(std::istreambuf_iterator<char>(f), {});
                    std::snprintf(buffer.data(), buffer.size(), "%s", text.c_str());
                }
                ImGui::InputTextMultiline("ICU messages", buffer.data(), buffer.size(), {0, 220});
                if (ImGui::Button("Validate and save catalog")) {
                    Catalog parsed;
                    std::string error;
                    if (ParseCatalog(buffer.data(), parsed, error)) {
                        std::ofstream f(record->path, std::ios::binary);
                        f << buffer.data();
                        state.status = f ? "Saved UTF-8 catalog" : "Catalog write failed";
                        state.textReload = true;
                        if (state.resources)
                            state.resources->Invalidate(record->id);
                    } else
                        state.status = error;
                }
                ImGui::SameLine();
                if (ImGui::Button("Reload preview catalog"))
                    state.textReload = true;
            }
            if (record->type == AssetType::UI && state.mode == EditorMode::Edit) {
                if (ImGui::Button("Edit UI document"))
                    state.showUIDocument = true;
                if (state.showUIDocument) {
                    ImGui::SetNextWindowPos({120, 45}, ImGuiCond_FirstUseEver);
                    ImGui::SetNextWindowSize({1040, 700}, ImGuiCond_FirstUseEver);
                    if (ImGui::Begin("UI document", &state.showUIDocument))
                        DrawUIAssetDocument(*record, state);
                    ImGui::End();
                }
            }
            if (record->type == AssetType::Prefab && state.mode == EditorMode::Edit &&
                ImGui::Button("Place prefab instance")) {
                Scene source;
                std::string error;
                SceneObjectId root = 0;
                if (LoadPrefab(db, record->id, source, error)) {
                    doc.BeginEdit();
                    if (InstantiatePrefab(doc.GetScene(), source, record->id, SceneTransform{}, root,
                                          error))
                        doc.Select(root);
                    doc.CommitEdit(false);
                }
                state.status = error.empty() ? "Placed linked prefab instance" : error;
            }
            if (ImGui::CollapsingHeader("Organize asset (rename / move / remove)")) {
                char moveTo[256];
                CopyToBuffer(state.moveAssetInput, moveTo, sizeof(moveTo));
                if (ImGui::InputText("Rename / move to (in assets)", moveTo, sizeof(moveTo)))
                    state.moveAssetInput = moveTo;
                if (ImGui::Button("Apply move") && !state.moveAssetInput.empty()) {
                    requests.moveAssetId = state.browserSelection;
                    requests.moveAssetTo = state.moveAssetInput;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove asset"))
                    requests.removeAssetId = state.browserSelection;
                ImGui::SameLine();
                ImGui::TextDisabled("Scene references use the id, so a move keeps them valid.");
            }
        }
    }
    ImGui::EndChild();
}
