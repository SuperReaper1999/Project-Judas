#include "TerrainV2Creation.h"
#include "../../third_party/nlohmann/json.hpp"
#include "EditorDesign.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "authoring/AuthoringDocument.h"
#include "imgui.h"
#include "terrain/TerrainComponent.h"
#include "terrain/TerrainCook.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
namespace {
std::string ReadHeightmap(const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() <= 0 || file.tellg() > 64 * 1024 * 1024)
        throw std::runtime_error("Choose a readable grayscale heightmap below64 MiB.");
    std::string bytes(size_t(file.tellg()), '\0');
    file.seekg(0);
    if (!file.read(bytes.data(), bytes.size()))
        throw std::runtime_error("Heightmap read failed.");
    return bytes;
}
void Require(bool ok, const std::string &error) {
    if (!ok)
        throw std::runtime_error(error);
}
bool CanReplaceGround(const SceneObject *object) {
    if (!object || object->landscape || !object->render || !object->render->visible ||
        !object->body || !object->body->enabled || object->body->sensor)
        return false;
    auto candidate = *object;
    candidate.landscape = SceneLandscapeSettings{};
    std::string error;
    return TerrainV2::ValidateOwnerPlacement(candidate, error);
}
} // namespace
void TerrainV2Creation::Start(const EditorDocument &document, const EditorPanelState &state) {
    if (!state.importJobs || !state.project || !state.assets || !name[0]) {
        error = "Open a project and give the landscape a name.";
        return;
    }
    const unsigned sizes[] = {65, 129, 257, 513};
    TerrainV2::LandscapeRecipe recipe;
    recipe.samples = sizes[resolution];
    recipe.spacing = spacing;
    recipe.base = base;
    recipe.height = height;
    recipe.seed = unsigned(seed);
    recipe.preset = TerrainV2::LandscapeRecipe::Preset(std::min(preset, 2));
    auto pending = std::make_shared<Work>();
    pending->name = name;
    pending->meshId = MintAssetId();
    pending->terrainId = MintAssetId();
    pending->previewId = MintAssetId();
    if (replaceGround) {
        const auto *selected = document.GetScene().Find(document.Selected());
        if (!CanReplaceGround(selected)) {
            error = "Select a root static ground object before replacing it.";
            return;
        }
        pending->replacedGround = *selected;
    }
    std::string stem;
    for (unsigned char c : pending->name)
        stem += std::isalnum(c) ? char(std::tolower(c)) : '-';
    stem += '-' + pending->terrainId.substr(0, 8);
    const auto projectRoot = fs::path(state.project->RootDir());
    const auto source = projectRoot / "Authoring" / (stem + ".judasterrain2");
    const auto directory = fs::path(state.project->AssetsDir()) / "Terrain" / pending->terrainId;
    const auto output = directory / (stem + ".judasmodel");
    pending->path = source.string();
    const bool imported = preset == 3;
    const auto inputPath = fs::path(heightmap);
    const std::string input =
        (inputPath.is_absolute() ? inputPath : projectRoot / inputPath).string();
    pending->job = state.importJobs->Submit(
        [pending, recipe, source, output, imported, input, projectRoot](JobContext &context) {
            try {
                TerrainV2::Source generated;
                std::string why, original;
                if (imported) {
                    original = ReadHeightmap(input);
                    Require(TerrainV2::ImportHeightmap(original, recipe, generated, why), why);
                } else
                    Require(TerrainV2::Generate(recipe, generated, why), why);
                if (context.CancelRequested()) {
                    context.ReportCancelled();
                    return;
                }
                pending->width = (generated.samplesX - 1) * generated.spacing;
                pending->depth = (generated.samplesZ - 1) * generated.spacing;
                const auto range =
                    std::minmax_element(generated.heights.begin(), generated.heights.end());
                pending->halfHeight =
                    std::max(1.f, std::max(std::abs(*range.first), std::abs(*range.second)) + 1);
                auto recipePath = source;
                recipePath.replace_extension(".recipe.json");
                nlohmann::json provenance = {{"format", "JudasLandscapeRecipe"},
                                             {"version", 1},
                                             {"preset", int(recipe.preset)},
                                             {"samples", recipe.samples},
                                             {"spacing", recipe.spacing},
                                             {"base", recipe.base},
                                             {"height", recipe.height},
                                             {"seed", recipe.seed},
                                             {"heightmap", imported}};
                std::vector<AuthoringOutput> files = {
                    {source.string(), TerrainV2::Serialize(generated), {}, false}};
                if (imported) {
                    auto originalPath = source;
                    originalPath.replace_extension(".heightmap" +
                                                   fs::path(input).extension().string());
                    provenance["input"] = originalPath.filename().string();
                    provenance["sha256"] = SceneFingerprintSha256(original);
                    files.push_back({originalPath.string(), original, {}, false});
                }
                files.push_back({recipePath.string(), provenance.dump(2) + "\n", {}, false});
                Require(ValidateTerrainWritePath(source.string(), why), why);
                Require(PublishAuthoringOutputs(source.parent_path().string(), files, why), why);
                std::string a = "terrain", b = "v2-native", c = source.string(),
                            d = output.string();
                char *args[] = {a.data(), b.data(), c.data(), d.data()};
                Require(TerrainV2AuthorCommand(4, args) == 0,
                        "Native terrain cooking failed; saved authoring source is retained.");
                auto meta = [&](fs::path path, const std::string &id, AssetType type) {
                    Require(AssetDatabase::WriteMeta(
                                path.string() + ".judasmeta", id, type,
                                fs::relative(source, projectRoot).generic_string(), why),
                            why);
                };
                meta(output, pending->meshId, AssetType::Mesh);
                auto native = output;
                native.replace_extension(".judasterrain");
                meta(native, pending->terrainId, AssetType::Terrain);
                auto preview = output;
                preview.replace_extension(".preview.tga");
                meta(preview, pending->previewId, AssetType::Texture);
            } catch (const std::exception &e) {
                pending->error = e.what();
                context.SetError(pending->error);
            }
        },
        JobPriority::Normal, "Create native landscape");
    if (pending->job.IsValid()) {
        work = std::move(pending);
        error.clear();
    } else
        error = "The editor preparation queue is unavailable; try again.";
}
void TerrainV2Creation::Panel(EditorDocument &doc, EditorPanelState &state,
                              EditorRequests &requests) {
    if (work && state.importJobs->IsFinished(work->job)) {
        if (state.importJobs->StateOf(work->job) == JobState::Completed) {
            const auto *ground =
                work->replacedGround ? doc.GetScene().Find(work->replacedGround->id) : nullptr;
            if (work->replacedGround &&
                (!ground || !SceneObjectsEqual(*ground, *work->replacedGround))) {
                error = "The selected ground changed during cooking. Your edits are preserved; the "
                        "generated source remains at " +
                        work->path;
                state.importJobs->Forget(work->job);
                work.reset();
                return;
            }
            state.assets->Scan(state.project->RootDir(), state.project->AssetsDir());
            doc.BeginEdit();
            auto &object = doc.GetScene().CreateObject(work->name);
            object.landscape = SceneLandscapeSettings{work->terrainId};
            object.body = SceneBodyComponent{};
            object.body->friction = .75f;
            object.body->restitution = 0;
            object.body->halfExtents = {work->width / 2, work->halfHeight, work->depth / 2};
            object.render = SceneRenderComponent{};
            object.render->color = glm::vec3(1);
            object.render->shape = SceneShape::Mesh;
            object.render->meshAsset = work->meshId;
            object.render->textureAsset = work->previewId;
            object.render->halfExtents = object.body->halfExtents;
            if (work->replacedGround) {
                const auto &old = *work->replacedGround;
                object.transform = old.transform;
                object.transform.position +=
                    old.transform.rotation * glm::vec3(0, old.body->halfExtents.y, 0);
                object.tags = old.tags;
                object.renderLayer = old.renderLayer;
                auto settings = *old.body;
                settings.halfExtents = object.body->halfExtents;
                settings.pickable = false;
                object.body = settings;
                auto *previous = doc.GetScene().Find(old.id);
                previous->render->visible = false;
                previous->body->enabled = false;
                // Gravity, scripts, children and other authored state stay on
                // their original owner; no second gravity region is invented.
            }
            const auto id = object.id;
            doc.CommitEdit();
            doc.Select(id);
            createdPath = work->path;
            requests.rescanAssets = true;
            state.status = "Landscape created and selected. Sculpt in scene view; save the scene "
                           "to keep its placement.";
            open = false;
        } else
            error = work->error.empty() ? state.importJobs->ErrorOf(work->job) : work->error;
        state.importJobs->Forget(work->job);
        work.reset();
    }
    if (!open)
        return;
    ImGui::SeparatorText("New landscape");
    ImGui::BeginDisabled(Busy());
    ImGui::InputText("Name", name, sizeof(name));
    const auto *selected = doc.GetScene().Find(doc.Selected());
    if (CanReplaceGround(selected)) {
        ImGui::Checkbox("Replace selected ground", &replaceGround);
        ImGui::TextWrapped(
            "%s: replaces its visible ground and collision in one undoable edit. Other components, "
            "including gravity, stay intact with their existing bounds.",
            selected->name.c_str());
    } else
        replaceGround = false;
    const char *presets[] = {"Flat — start sculpting", "Highlands — ridges and valleys",
                             "Island — low coast", "Import heightmap"};
    ImGui::Combo("Start from", &preset, presets, 4);
    if (preset == 3) {
        ImGui::InputText("Grayscale image", heightmap, sizeof(heightmap));
        if (ImGui::Button("Browse heightmap"))
            browser.Open(EditorFileBrowser::Target::Import, EditorFileBrowser::Mode::OpenFile,
                         state.project->RootDir(), ".png");
        ImGui::TextWrapped("16-bit grayscale detail is retained. Image top maps to negative Z; the "
                           "image is resampled to the chosen grid.");
    }
    ImGui::Combo("Samples per side", &resolution,
                 "65\0"
                 "129\0"
                 "257\0"
                 "513\0");
    ImGui::DragFloat("Sample spacing (m)", &spacing, .1f, .125f, 128, "%.3f",
                     ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Base / minimum height (m)", &base, .1f, -1000, 1000);
    if (preset != 0)
        ImGui::DragFloat("Height range (m)", &height, .5f, 0, 4096);
    if (preset == 1 || preset == 2)
        ImGui::InputInt("Seed", &seed);
    const unsigned sizes[] = {65, 129, 257, 513};
    ImGui::Text("Footprint: %.0f x %.0f m | %u chunks", (sizes[resolution] - 1) * spacing,
                (sizes[resolution] - 1) * spacing,
                ((sizes[resolution] - 1) / 16) * ((sizes[resolution] - 1) / 16));
    ImGui::TextWrapped("Creates editable source, native terrain and preview assets, then adds one "
                       "selected landscape. Existing files are preserved.");
    if (EditorDesign::ActionButton("Create landscape", EditorDesign::Tone::Primary))
        Start(doc, state);
    ImGui::SameLine();
    if (EditorDesign::ActionButton("Cancel new landscape", EditorDesign::Tone::Quiet))
        open = false;
    ImGui::EndDisabled();
    if (Busy())
        ImGui::TextUnformatted("Generating and cooking landscape in the background…");
    if (!error.empty())
        ImGui::TextWrapped("%s", error.c_str());
    if (auto selection = browser.Draw())
        std::snprintf(heightmap, sizeof(heightmap), "%s", selection->path.c_str());
}
void TerrainV2Creation::Shutdown(JobSystem *jobs) {
    if (work && jobs) {
        jobs->Cancel(work->job);
        jobs->Wait(work->job);
        jobs->Forget(work->job);
    }
    work.reset();
}
std::string TerrainV2Creation::TakeCreatedPath() {
    auto result = std::move(createdPath);
    createdPath.clear();
    return result;
}
