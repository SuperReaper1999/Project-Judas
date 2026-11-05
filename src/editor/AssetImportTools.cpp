#include <glm/gtc/matrix_transform.hpp>
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
#include "AssetPublicationWorkflow.h"
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
#include "RetargetPanel.h"
#include "EditorDesign.h"
#include "ModelImportScale.h"

void DrawAssetImportTools(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests,
                          const AssetDatabase &db, bool revealModelTools) {
    ImGui::BeginChild("assetTools", {0, 0});
    if (ImGui::Button("Animation retargeting: map / preview / bake"))
        OpenRetargetPanel(state, {}, state.modelRecipe);
    if (revealModelTools) {
        ImGui::SetNextItemOpen(true);
        ImGui::SetScrollY(0.0f);
    }
    if (ImGui::CollapsingHeader("Model import / reimport", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (EditorModelScaleReviewPending(state)) {
            EditorDesign::Surface review;
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram));
            ImGui::TextUnformatted("CHECK SCALE: possible 100x units mismatch");
            ImGui::PopStyleColor();
            const auto *warning = JudasImport::ScaleWarning(state.importTask->report);
            ImGui::TextWrapped("%s", warning->message.c_str());
            ImGui::TextWrapped("%s", warning->action.c_str());
            ImGui::TextWrapped("The current model is unchanged. Cancel to review source units below, "
                               "then retry; or keep this scale if it is intentional.");
            ImGui::BeginDisabled(state.mode != EditorMode::Edit);
            if (EditorDesign::ActionButton("Keep this scale and import", EditorDesign::Tone::Primary))
                state.importScaleConfirmed = state.importTask;
            NextActionOnRow("Cancel / review units");
            if (EditorDesign::ActionButton("Cancel / review units")) {
                state.importTask->cancel = true;
                state.importPublished = true;
                state.status = "Scale review cancelled; last accepted model retained. Review units and retry.";
            }
            ImGui::EndDisabled();
        }
        if (ImGui::TreeNode("Source and recipe settings")) {
            auto text = [](const char *label, std::string &value) {
                std::vector<char> buffer(std::max(size_t(4096), value.size() + 256), 0);
                std::copy(value.begin(), value.end(), buffer.begin());
                if (ImGui::InputText(label, buffer.data(), buffer.size()))
                    value = buffer.data();
            };
            ImGui::TextWrapped("FBX / glTF / GLB / OBJ to an ordinary cooked model. Sources and recipes "
                               "are author-owned; the game loads only cooked content. Reimport "
                               "publishes only in Edit mode.");
            text("Original source file", state.modelSource);
            text("Project-relative runtime output", state.modelOutput);
            if (state.mode == EditorMode::Edit && !EditorModelImportPending(state) &&
                ImGui::Button("Copy source and create import recipe")) {
                std::string error;
                if (CreateModelRecipe(state.project->RootDir(), state.modelSource, state.modelOutput,
                                      state.modelRecipe, error))
                    state.status = "Recipe created; review settings then Import";
                else
                    state.status = error;
            }
            text("Import recipe (.judasimport)", state.modelRecipe);
            if (!state.modelRecipe.empty() && std::filesystem::is_regular_file(state.modelRecipe))
                try {
                    using Json = nlohmann::json;
                    std::ifstream file(state.modelRecipe);
                    if (std::filesystem::file_size(state.modelRecipe) > 4 * 1024 * 1024)
                        throw std::runtime_error("Recipe exceeds 4 MiB");
                    auto recipe = Json::parse(file, [](int depth, Json::parse_event_t, const Json &) {
                        if (depth >= 48)
                            throw std::runtime_error("Recipe nesting bound");
                        return true;
                    });
                    bool changed = false;
                    auto &settings = recipe["settings"];
                    double units = settings.value("unitMeters", 0.0),
                           rate = settings.value("sampleRate", 60.0);
                    bool base = settings.value("allowBaseMesh", false);
                    if (ImGui::InputDouble("Source metres per unit (0 = metadata)", &units)) {
                        settings["unitMeters"] = units;
                        changed = true;
                    }
                    if (ImGui::InputDouble("Nonlinear bake samples / second", &rate)) {
                        settings["sampleRate"] = rate;
                        changed = true;
                    }
                    if (ImGui::Checkbox("Explicit base mesh only (lossy)", &base)) {
                        settings["allowBaseMesh"] = base;
                        changed = true;
                    }
                    text("Compatible motion source", state.modelMotionSource);
                    text("Source take", state.modelMotionTake);
                    text("Clip name", state.modelMotionName);
                    if (state.mode == EditorMode::Edit && ImGui::Button("Copy / add compatible motion")) {
                        auto root = std::filesystem::path(state.project->RootDir());
                        auto directory = (root / recipe.at("source").get<std::string>()).parent_path();
                        auto source = std::filesystem::path(state.modelMotionSource);
                        auto owned = directory / source.filename();
                        if (std::filesystem::weakly_canonical(source) !=
                            std::filesystem::weakly_canonical(owned))
                            std::filesystem::copy_file(source, owned,
                                                       std::filesystem::copy_options::overwrite_existing);
                        recipe["motions"].push_back(
                            {{"source", owned.lexically_relative(root).generic_string()},
                             {"take", state.modelMotionTake},
                             {"name", state.modelMotionName},
                             {"jointRemaps", Json::object()}});
                        changed = true;
                    }
                    glm::vec4 basis(0, 0, 0, 1);
                    if (settings.contains("basisRotation")) {
                        auto q = settings.at("basisRotation").get<std::vector<float>>();
                        if (q.size() == 4)
                            basis = {q[0], q[1], q[2], q[3]};
                    }
                    if (ImGui::InputFloat4("Explicit post-metadata basis rotation x/y/z/w", &basis.x)) {
                        settings["basisRotation"] = {basis.x, basis.y, basis.z, basis.w};
                        changed = true;
                    }
                    text("Remap source name/path", state.modelMapSource);
                    text("Project-relative file or stable alias", state.modelMapTarget);
                    const char *maps[] = {"dependencyRemaps", "materialAliases", "nodeAliases"};
                    for (auto field : maps) {
                        ImGui::PushID(field);
                        if (ImGui::Button(field) && !state.modelMapSource.empty()) {
                            settings[field][state.modelMapSource] = state.modelMapTarget;
                            changed = true;
                        }
                        ImGui::PopID();
                    }
                    if (state.importAccepted && state.importAccepted->preview) {
                        auto &preview = *state.importAccepted->preview;
                        if (ImGui::TreeNode("Import part selection (empty = all)")) {
                            auto parts = settings.value("selectedParts", std::vector<std::string>{});
                            bool all = parts.empty();
                            if (ImGui::Checkbox("All source parts", &all) && all) {
                                settings["selectedParts"] = Json::array();
                                changed = true;
                            }
                            for (auto &part : preview.primitives) {
                                bool selected =
                                    all || std::find(parts.begin(), parts.end(), part.part) != parts.end();
                                if (ImGui::Checkbox(part.part.c_str(), &selected)) {
                                    if (all) {
                                        for (auto &p : preview.primitives)
                                            parts.push_back(p.part);
                                    }
                                    if (selected) {
                                        if (std::find(parts.begin(), parts.end(), part.part) == parts.end())
                                            parts.push_back(part.part);
                                    } else
                                        parts.erase(std::remove(parts.begin(), parts.end(), part.part),
                                                    parts.end());
                                    settings["selectedParts"] = parts;
                                    changed = true;
                                }
                            }
                            ImGui::TreePop();
                        }
                        if (ImGui::TreeNode("Source material slots")) {
                            for (size_t i = 0; i < preview.materialKeys.size(); ++i)
                                ImGui::BulletText("%zu: %s", i, preview.materialKeys[i].c_str());
                            ImGui::TextWrapped("Author material overrides in the normal Render "
                                               "inspector; imported defaults are separate.");
                            ImGui::TreePop();
                        }
                    }
                    if (recipe.contains("clips"))
                        for (auto &clip : recipe["clips"]) {
                            ImGui::PushID(clip.at("sourceClip").get<std::string>().c_str());
                            std::string name = clip.value("name", clip.at("sourceClip").get<std::string>());
                            text("Clip output name", name);
                            if (clip.value("name", std::string()) != name) {
                                clip["name"] = name;
                                changed = true;
                            }
                            bool loop = clip.value("loop", false);
                            if (ImGui::Checkbox("Loop metadata", &loop)) {
                                clip["loop"] = loop;
                                changed = true;
                            }
                            auto &root = clip["rootMotion"];
                            std::string policy = root.value("policy", std::string("preserve")),
                                        node = root.value("node", std::string());
                            if (ImGui::BeginCombo("Root policy", policy.c_str())) {
                                for (auto value : {"preserve", "inPlace", "extract"})
                                    if (ImGui::Selectable(value, policy == value)) {
                                        root["policy"] = value;
                                        changed = true;
                                    }
                                ImGui::EndCombo();
                            }
                            auto rig = state.importTask && state.importPublished
                                           ? state.resources->TryGetSkeletal(state.importTask->assetId)
                                           : nullptr;
                            if (rig && ImGui::BeginCombo("Motion node", node.c_str())) {
                                for (size_t i = 0; i < rig->skeleton.names.size(); ++i) {
                                    auto key = SkeletonJointKey(rig->skeleton, int(i));
                                    if (ImGui::Selectable(key.c_str(), node == key)) {
                                        root["node"] = key;
                                        changed = true;
                                    }
                                }
                                ImGui::EndCombo();
                            }
                            auto axes = root.value("translation", std::vector<bool>{true, false, true});
                            if (axes.size() == 3)
                                for (int k = 0; k < 3; ++k) {
                                    bool axis = axes[k];
                                    ImGui::PushID(k);
                                    if (ImGui::Checkbox(k == 0   ? "Model X"
                                                        : k == 1 ? "Model Y"
                                                                 : "Model Z",
                                                        &axis)) {
                                        axes[k] = axis;
                                        root["translation"] = axes;
                                        changed = true;
                                    }
                                    ImGui::PopID();
                                    ImGui::SameLine();
                                }
                            ImGui::NewLine();
                            glm::vec3 rotationAxis(0);
                            if (root.contains("rotationAxis")) {
                                auto v = root["rotationAxis"].get<std::vector<float>>();
                                if (v.size() == 3)
                                    rotationAxis = {v[0], v[1], v[2]};
                            }
                            if (ImGui::InputFloat3("Extract twist around model axis (zero = none)",
                                                   &rotationAxis.x)) {
                                if (glm::length(rotationAxis) > 1e-6f)
                                    root["rotationAxis"] = {rotationAxis.x, rotationAxis.y, rotationAxis.z};
                                else
                                    root.erase("rotationAxis");
                                changed = true;
                            }
                            glm::vec2 trim(0, 1);
                            if (clip.contains("trim"))
                                trim = {clip["trim"][0], clip["trim"][1]};
                            if (ImGui::InputFloat2("Trim begin/end seconds", &trim.x)) {
                                clip["trim"] = {trim.x, trim.y};
                                changed = true;
                            }
                            ImGui::PopID();
                        }
                    if (state.importTask && state.importTask->preview &&
                        state.importTask->preview->skeletal &&
                        ImGui::Button("Populate editable clip selections")) {
                        recipe["clips"] = Json::array();
                        for (auto &clip : state.importTask->preview->skeletal->clips)
                            recipe["clips"].push_back({{"sourceClip", clip.name},
                                                       {"name", clip.name},
                                                       {"loop", clip.loop},
                                                       {"rootMotion", {{"policy", "preserve"}}}});
                        changed = true;
                    }
                    if (changed && state.mode == EditorMode::Edit) {
                        std::ofstream out(state.modelRecipe);
                        out << recipe.dump(2) << '\n';
                        state.status =
                            "Import recipe saved; accepted runtime content is unchanged until reimport";
                    }
                    if (ImGui::TreeNode("Named recipe / selections / remaps")) {
                        auto pretty = recipe.dump(2);
                        ImGui::TextUnformatted(pretty.c_str());
                        ImGui::TreePop();
                    }
                } catch (const std::exception &e) {
                    ImGui::TextWrapped("Recipe: %s", e.what());
                }
            ImGui::TreePop();
        }
        bool busy = state.importTask && !state.importTask->done.load();
        ImGui::BeginDisabled(state.mode != EditorMode::Edit || EditorModelImportPending(state) ||
                             state.modelRecipe.empty() || !state.importJobs);
        if (ImGui::Button("Import / reimport / retry"))
            QueueEditorModelCook(state);
        ImGui::EndDisabled();
        if (busy) {
            ImGui::ProgressBar(float(state.importTask->progress.load()) / 100);
            if (ImGui::Button("Cancel import"))
                state.importTask->cancel = true;
        }
        if (state.importTask && state.importTask->done.load(std::memory_order_acquire)) {
            if (!state.importTask->success)
                ImGui::TextWrapped("Import failed; last good model retained: %s",
                                   state.importTask->error.c_str());
            else if (!state.importPublished && !EditorModelScaleReviewPending(state))
                ImGui::TextWrapped(
                    "Cook ready. Stop Play to publish; live skeleton buffers remain unchanged.");
        }
        if (state.importAccepted) {
            auto &t = *state.importAccepted;
            {
                ImGui::TextWrapped("Accepted model: %s",
                                   std::filesystem::u8path(t.output).filename().u8string().c_str());
                ImGui::Text("Source bones: %zu; hierarchy nodes: %zu; palette entries: %zu; parts: %zu",
                            t.report.sourceBones, t.report.hierarchyNodes, t.report.skinJoints,
                            t.report.parts);
                if (ImGui::Button("Place accepted model")) {
                    requests.dropMeshAssetId = t.assetId;
                    requests.createPosition = state.cameraFocus;
                }
                auto rig = state.resources->TryGetSkeletal(t.assetId);
                if (rig) {
                    ImGui::Text("Runtime hierarchy: %zu nodes", rig->skeleton.names.size());
                    if (!rig->clips.empty()) {
                        state.modelPreviewClip =
                            unsigned(std::min(size_t(state.modelPreviewClip), rig->clips.size() - 1));
                        if (ImGui::BeginCombo("Preview clip",
                                              rig->clips[state.modelPreviewClip].name.c_str())) {
                            for (size_t i = 0; i < rig->clips.size(); ++i)
                                if (ImGui::Selectable(rig->clips[i].name.c_str(),
                                                      i == state.modelPreviewClip)) {
                                    state.modelPreviewClip = unsigned(i);
                                    state.modelPreviewTime = 0;
                                }
                            ImGui::EndCombo();
                        }
                        ImGui::Checkbox("Preview playback", &state.modelPreviewPlaying);
                        ImGui::SliderFloat("Scrub seconds", &state.modelPreviewTime, 0,
                                           rig->clips[state.modelPreviewClip].duration);
                    }
                    ImGui::Checkbox("Skeleton overlay", &state.modelPreviewSkeleton);
                    ImGui::SliderFloat("Orbit view", &state.modelPreviewYaw, -3.14f, 3.14f);
                    if (ImGui::TreeNode("Hierarchy / stable joint identities")) {
                        for (size_t i = 0; i < rig->skeleton.names.size(); ++i)
                            ImGui::BulletText("%s", SkeletonJointKey(rig->skeleton, int(i)).c_str());
                        ImGui::TreePop();
                    }
                }
                if (auto parts = state.resources->TryGetModelParts(t.assetId))
                    for (auto &p : *parts) {
                        bool visible =
                            std::find(state.modelPreviewHidden.begin(), state.modelPreviewHidden.end(),
                                      p.part) == state.modelPreviewHidden.end();
                        if (ImGui::Checkbox(p.part.c_str(), &visible)) {
                            if (visible)
                                state.modelPreviewHidden.erase(std::remove(state.modelPreviewHidden.begin(),
                                                                           state.modelPreviewHidden.end(),
                                                                           p.part),
                                                               state.modelPreviewHidden.end());
                            else
                                state.modelPreviewHidden.push_back(p.part);
                        }
                    }
                ImGui::TextWrapped(
                    "White ruler = one metre; RGB axes = import model basis; green = hierarchy; orange "
                    "= extracted root track. These are preview axes, not simulation gravity.");
                if (state.modelPreviewToken)
                    ImGui::Image((ImTextureID)(intptr_t)state.modelPreviewToken,
                                 ImVec2(std::min(600.f, ImGui::GetContentRegionAvail().x), 360), ImVec2(0, 1),
                                 ImVec2(1, 0));
                for (auto &diagnostic : t.report.diagnostics)
                    ImGui::TextWrapped("%s [%s]: %s. %s", diagnostic.severity.c_str(),
                                       diagnostic.code.c_str(), diagnostic.message.c_str(),
                                       diagnostic.action.c_str());
            }
        }
    }

    if (state.mode == EditorMode::Edit && ImGui::Button("Create material")) {
        auto directory = std::filesystem::path(db.AssetsDir()) / "materials";
        std::filesystem::create_directories(directory);
        auto path = directory / "material.judasmat";
        unsigned n = 2;
        while (std::filesystem::exists(path))
            path = directory / ("material" + std::to_string(n++) + ".judasmat");
        std::string error;
        if (SaveMaterial(path.string(), MaterialDefinition{}, error))
            requests.trackAssetPath = path.string();
        else
            state.status = error;
    }
    if (state.mode == EditorMode::Edit && ImGui::Button("Create reverb settings")) {
        auto directory = std::filesystem::path(db.AssetsDir()) / "audio";
        std::filesystem::create_directories(directory);
        auto path = directory / "environment.judasreverb";
        unsigned n = 2;
        while (std::filesystem::exists(path))
            path = directory / ("environment" + std::to_string(n++) + ".judasreverb");
        std::ofstream f(path);
        f << SerializeAudioEnvironment({});
        if (f)
            requests.trackAssetPath = path.string();
        else
            state.status = "Cannot write reverb settings";
    }
    if (state.mode == EditorMode::Edit && ImGui::CollapsingHeader("Bake HDR environment")) {
        static char source[1024]{};
        static int width = 128, samples = 128;
        ImGui::InputText("Radiance .hdr source", source, sizeof(source));
        ImGui::InputInt("Bake width (power of two)", &width);
        ImGui::InputInt("Bake samples", &samples);
        ImGui::TextWrapped("Offline bake; may briefly block editor. Derived data is exported, never "
                           "regenerated during play.");
        if (ImGui::Button("Bake and register environment")) {
            auto directory = std::filesystem::path(db.AssetsDir()) / "environments";
            std::filesystem::create_directories(directory);
            auto path = directory / (std::filesystem::path(source).stem().string() + ".judasenv");
            unsigned n = 2;
            while (std::filesystem::exists(path))
                path = directory /
                       (std::filesystem::path(source).stem().string() + std::to_string(n++) + ".judasenv");
            EnvironmentData data;
            std::string error;
            if (BakeEnvironment(source, unsigned(width), unsigned(samples), data, error) &&
                SaveEnvironment(path.string(), data, error))
                requests.trackAssetPath = path.string();
            else
                state.status = error;
        }
    }

    if (state.mode == EditorMode::Edit && ImGui::CollapsingHeader("Cook physical collider")) {
        static std::string sourceId;
        static char output[256] = "collision/physical.judascollision";
        static int primitive = 0, kind = 0;
        static bool twoSided = false;
        static glm::vec3 scale(1), offset(0), angles(0);
        const auto selected = db.Find(sourceId);
        if (ImGui::BeginCombo("Registered source",
                              selected ? selected->relativePath.c_str() : "Select OBJ/glTF/GLB")) {
            for (auto &[id, a] : db.Records())
                if (a.type == AssetType::Mesh && ImGui::Selectable(a.relativePath.c_str(), id == sourceId))
                    sourceId = id;
            ImGui::EndCombo();
        }
        ImGui::InputInt("Object/group or node/primitive ordinal", &primitive);
        const char *kinds[] = {"Static triangle surface", "Convex hull (fills recesses)"};
        ImGui::Combo("Cook kind", &kind, kinds, 2);
        ImGui::Checkbox("Two-sided triangle surface", &twoSided);
        ImGui::DragFloat3("Baked positive scale", &scale.x, .01f);
        ImGui::DragFloat3("Baked translation", &offset.x, .01f);
        ImGui::DragFloat3("Baked rotation degrees", &angles.x, .5f);
        ImGui::InputText("Output relative to assets", output, sizeof(output));
        ImGui::Checkbox("Explicit removal of true degenerates", &state.collisionCleanup.removeDegenerates);
        ImGui::Checkbox("Explicit orient connected patches", &state.collisionCleanup.orientPatches);
        ImGui::InputDouble("Collision-only weld metres (0..0.001)", &state.collisionCleanup.weldTolerance);
        if (!state.collisionDiagnostic.code.empty()) {
            auto &d = state.collisionDiagnostic;
            ImGui::TextWrapped("%s: %s / source face %u / point %.4f %.4f %.4f", d.code.c_str(),
                               d.node.c_str(), d.sourceFace, d.point.x, d.point.y, d.point.z);
            if (ImGui::Button("Highlight diagnostic on selected model"))
                state.modelDiagnosticVisible = true;
        }
        ImGui::TextWrapped("Explicit single source selection; glTF node transform is included. Failed "
                           "cooks preserve the previous file. Use a low-detail physical model where "
                           "appropriate. Convex mass sums child volumes, including overlaps.");
        if (ImGui::Button("Cook / replace and register")) {
            std::string error;
            CollisionAsset asset;
            ModelCollisionCleanupReport cleanupReport;
            state.collisionDiagnosticAsset = sourceId;
            CollisionCookSettings settings;
            settings.convex = kind == 1;
            settings.twoSided = twoSided;
            settings.primitive = unsigned(std::max(primitive, 0));
            settings.transform = glm::translate(glm::dmat4(1), glm::dvec3(offset)) *
                                 glm::mat4_cast(glm::dquat(glm::radians(glm::dvec3(angles)))) *
                                 glm::scale(glm::dmat4(1), glm::dvec3(scale));
            auto relative = std::filesystem::path(output).lexically_normal();
            bool valid = !relative.empty() && !relative.is_absolute();
            for (auto &part : relative)
                if (part == "..")
                    valid = false;
            auto destination = std::filesystem::path(db.AssetsDir()) / relative;
            if (!selected || selected->missing)
                error = "select an available registered model";
            else if (!valid || relative.extension() != ".judascollision")
                error = "destination must be an assets-relative .judascollision path";
            else {
                std::error_code ec;
                std::filesystem::create_directories(destination.parent_path(), ec);
                if (ec)
                    error = ec.message();
                else if (CookImportedCollisionFile(selected->path, sourceId, settings, state.collisionCleanup,
                                                   destination.string(), asset, cleanupReport,
                                                   state.collisionDiagnostic, error)) {
                    requests.trackAssetPath = destination.string();
                    for (auto &[id, a] : db.Records())
                        if (a.path == destination.string() && state.resources)
                            state.resources->Invalidate(id);
                }
            }
            state.status = error.empty() ? "Cooked collision; assign it in Body / Cooked geometry" : error;
        }
        if (ImGui::Button("Cook and assign selected Body")) {
            CollisionCookSettings settings;
            settings.convex = kind == 1;
            settings.twoSided = twoSided;
            settings.primitive = unsigned(std::max(primitive, 0));
            settings.transform = glm::translate(glm::dmat4(1), glm::dvec3(offset)) *
                                 glm::mat4_cast(glm::dquat(glm::radians(glm::dvec3(angles)))) *
                                 glm::scale(glm::dmat4(1), glm::dvec3(scale));
            BakeEditorCollision(doc, doc.Selected(), state, sourceId, settings, output);
        }
    }

    if (state.mode == EditorMode::Edit && ImGui::Button("Create UI document")) {
        UIDocument d;
        UIElement root;
        root.id = "canvas";
        root.kind = UIKind::Canvas;
        d.elements.push_back(root);
        UIElement panel;
        panel.id = "panel";
        panel.parent = "canvas";
        panel.background = {.1f, .12f, .18f, .95f};
        d.elements.push_back(panel);
        auto directory = std::filesystem::path(db.AssetsDir()) / "ui";
        std::filesystem::create_directories(directory);
        auto path = directory / "layout.judasui";
        int n = 2;
        while (std::filesystem::exists(path))
            path = directory / ("layout" + std::to_string(n++) + ".judasui");
        std::string error;
        if (SaveUIDocument(path.string(), d, error))
            requests.trackAssetPath = path.string();
        else
            state.status = error;
    }
    if (ImGui::CollapsingHeader("Import", ImGuiTreeNodeFlags_DefaultOpen)) {
        char source[512], destination[256];
        CopyToBuffer(state.importSourceInput, source, sizeof(source));
        CopyToBuffer(state.importDestinationInput, destination, sizeof(destination));
        if (ImGui::InputTextWithHint("Source file", "/path/to/model.obj | texture.png | font.ttf | sound.wav",
                                     source, sizeof(source)))
            state.importSourceInput = source;
        if (ImGui::InputTextWithHint("Destination (in assets)",
                                     "models/model.obj (empty keeps the file name)", destination,
                                     sizeof(destination)))
            state.importDestinationInput = destination;
        if (ImGui::Button("Import") && !state.importSourceInput.empty()) {
            requests.importSource = state.importSourceInput;
            requests.importDestination = state.importDestinationInput;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse source..."))
            requests.browseImport = true;
        ImGui::TextDisabled(
            "Import copies and tracks the source. Cook when a platform/runtime format is needed.");
    }

    if (!db.Untracked().empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Untracked files (no .judasmeta; not referenceable until tracked):");
        for (const std::string &path : db.Untracked()) {
            ImGui::PushID(path.c_str());
            ImGui::BulletText("%s", path.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Track"))
                requests.trackAssetPath = path;
            ImGui::PopID();
        }
    }
    if (!db.Problems().empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Problems:");
        for (const AssetProblem &problem : db.Problems())
            ImGui::BulletText("%s: %s", problem.path.c_str(), problem.message.c_str());
    }
    ImGui::EndChild();
}
