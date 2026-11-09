#include "TerrainV2Editor.h"
#include "../../third_party/nlohmann/json.hpp"
#include "EditorDesign.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "WorldPresentation.h"
#include "authoring/AuthoringDocument.h"
#include "imgui.h"
#include "terrain/TerrainCook.h"
#include "terrain/TerrainMesher.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

namespace fs = std::filesystem;
namespace {
std::string Read(const fs::path &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f || f.tellg() < 0 || f.tellg() > 64 * 1024 * 1024)
        return {};
    std::string bytes(size_t(f.tellg()), '\0');
    f.seekg(0);
    f.read(bytes.data(), bytes.size());
    return f ? bytes : std::string{};
}
std::vector<TerrainV2::ChunkKey> All(const TerrainV2::Source &s) {
    std::vector<TerrainV2::ChunkKey> keys;
    for (unsigned z = 0; z < s.ChunksZ(); ++z)
        for (unsigned x = 0; x < s.ChunksX(); ++x)
            keys.push_back({x, z});
    return keys;
}
} // namespace
bool TerrainV2Editor::Open(EditorPanelState &state) {
    TerrainV2::Source next;
    auto bytes = Read(path);
    if (!TerrainV2::Parse(bytes, next, error))
        return false;
    const auto model = fs::path(path).stem().string();
    meshId.clear();
    std::vector<std::string> exact, fallback;
    const auto relative =
        fs::path(path).lexically_relative(state.project->RootDir()).generic_string();
    for (const auto &[id, asset] : state.assets->Records())
        if (asset.type == AssetType::Mesh) {
            if (asset.source == relative)
                exact.push_back(id);
            if (fs::path(asset.path).stem() == model)
                fallback.push_back(id);
        }
    const auto &matches = exact.empty() ? fallback : exact;
    if (matches.size() == 1)
        meshId = matches.front();
    else if (matches.size() > 1) {
        error = "Multiple terrain previews match this source. Keep one registered editable preview "
                "per source, with project-relative source metadata.";
        return false;
    }
    if (meshId.empty()) {
        error = "Cook this source to a matching .judasmodel asset before viewport editing.";
        return false;
    }
    source = std::move(next);
    ReadPalette();
    brush.radius = std::clamp(std::max(brush.radius, source.spacing * 2), source.spacing, 128.f);
    ++revision;
    disk = std::move(bytes);
    loaded = true;
    editing = true;
    dirty = false;
    productsDirty = true;
    auto receipt = fs::path(state.assets->Find(meshId)->path);
    receipt.replace_extension(".terrain-v2-receipt.json");
    try {
        auto j = nlohmann::json::parse(Read(receipt));
        productsDirty = j.at("source") != SceneFingerprintSha256(disk);
    } catch (...) {
    }
    undo.clear();
    redo.clear();
    appearanceDirty = true;
    ClearPreview();
    Queue(All(source), state);
    return true;
}
void TerrainV2Editor::Queue(const std::vector<TerrainV2::ChunkKey> &keys, EditorPanelState &state) {
    if (keys.empty())
        return;
    if (!state.importJobs) {
        error = "Terrain preview needs the editor job system.";
        return;
    }
    auto work = std::make_shared<Task>();
    work->revision = revision;
    auto captured = std::make_shared<const TerrainV2::Source>(source);
    auto previous = previewData;
    work->job = state.importJobs->Submit(
        [work, captured, previous, keys](JobContext &context) {
            if (!TerrainV2::PrepareEditorPreview(*captured, keys, previous, work->preview,
                                                 work->error,
                                                 [&] { return context.CancelRequested(); })) {
                if (context.CancelRequested())
                    context.ReportCancelled();
                else
                    context.SetError(work->error);
                return;
            }
        },
        JobPriority::High, "Terrain V2 editor dirty chunks");
    if (work->job.IsValid())
        task = std::move(work);
    else
        error = "Terrain preview job could not be queued.";
}
void TerrainV2Editor::DiscoverSources(const EditorPanelState &state) {
    sources.clear();
    const auto root = fs::path(state.project->RootDir()) / "Authoring";
    std::error_code ec;
    if (fs::is_directory(root, ec))
        for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec))
            if (it->is_regular_file(ec) && !it->is_symlink(ec) &&
                it->path().extension() == ".judasterrain2") {
                sources.push_back(it->path().string());
                if (sources.size() == 512)
                    break;
            }
    std::sort(sources.begin(), sources.end());
}
void TerrainV2Editor::TrimHistory() {
    auto bytes = [](const auto &history) {
        size_t count = 0;
        for (const auto &s : history)
            count += s.heights.size() * 20 + s.edits.size() * sizeof(TerrainV2::Edit);
        return count;
    };
    // Keep the immediate undo/redo snapshot even at the maximum source size.
    // Other history is bounded by bytes as well as count, across BOTH stacks.
    while (undo.size() + redo.size() > 1 &&
           (bytes(undo) + bytes(redo) > 64 * 1024 * 1024 || undo.size() + redo.size() > 16)) {
        if (!undo.empty() && (undo.size() > 1 || redo.empty()))
            undo.erase(undo.begin());
        else if (!redo.empty())
            redo.erase(redo.begin());
    }
}
void TerrainV2Editor::ReadPalette() {
    paletteTextures = source.layerTextures;
    paletteColours = source.layerColours;
    paletteDetail = source.materialDetail;
}
bool TerrainV2Editor::PaletteDirty() const {
    return loaded &&
           (paletteTextures != source.layerTextures || paletteColours != source.layerColours ||
            paletteDetail != source.materialDetail);
}
void TerrainV2Editor::ApplyPalette() {
    if (!PaletteDirty())
        return;
    undo.push_back(source);
    redo.clear();
    source.layerTextures = paletteTextures;
    source.layerColours = paletteColours;
    source.materialDetail = paletteDetail;
    TrimHistory();
    dirty = productsDirty = appearanceDirty = true;
    ++revision;
}
void TerrainV2Editor::PalettePanel(const EditorPanelState &state) {
    if (!ImGui::CollapsingHeader("Surface materials"))
        return;
    ImGui::TextWrapped("Select textures or drag them from Assets. Apply updates the preview; Save "
                       "and update also applies pending material choices.");
    for (unsigned i = 0; i < 4; ++i) {
        ImGui::PushID(int(i));
        const auto title = "Layer " + std::to_string(i + 1);
        if (!ImGui::CollapsingHeader(title.c_str(), i == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            ImGui::PopID();
            continue;
        }
        const auto *current = state.assets->Find(paletteTextures[i]);
        if (ImGui::BeginCombo("Texture", current ? current->relativePath.c_str() : "Colour only")) {
            if (ImGui::Selectable("Colour only", paletteTextures[i].empty()))
                paletteTextures[i].clear();
            for (const auto &[id, asset] : state.assets->Records())
                if (asset.type == AssetType::Texture && !asset.missing &&
                    ImGui::Selectable(asset.relativePath.c_str(), paletteTextures[i] == id))
                    paletteTextures[i] = id;
            ImGui::EndCombo();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const auto *payload = ImGui::AcceptDragDropPayload(kAssetDragPayload)) {
                std::string id(static_cast<const char *>(payload->Data), payload->DataSize);
                const auto *asset = state.assets->Find(id);
                if (asset && asset->type == AssetType::Texture && !asset->missing)
                    paletteTextures[i] = id;
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::ColorEdit3("Tint", &paletteColours[i].x);
        ImGui::SliderFloat("Tile size (metres)", &paletteDetail.tileMetres[i], .125f, 1024.f,
                           "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("PBR roughness multiplier", &paletteDetail.roughness[i], 0.f, 1.f);
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Shared normal detail")) {
        ImGui::TextWrapped("Optional linear tangent-space normal texture. Adds surface detail to "
                           "every layer and cave wall without changing collision. PBR roughness "
                           "applies when the landscape owner uses a PBR material.");
        const auto *normal = state.assets->Find(paletteDetail.normalTexture);
        if (ImGui::BeginCombo("Normal texture", normal ? normal->relativePath.c_str() : "None")) {
            if (ImGui::Selectable("None", paletteDetail.normalTexture.empty()))
                paletteDetail.normalTexture.clear();
            for (const auto &[id, asset] : state.assets->Records())
                if (asset.type == AssetType::Texture && !asset.missing &&
                    ImGui::Selectable(asset.relativePath.c_str(),
                                      paletteDetail.normalTexture == id))
                    paletteDetail.normalTexture = id;
            ImGui::EndCombo();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const auto *payload = ImGui::AcceptDragDropPayload(kAssetDragPayload)) {
                std::string id(static_cast<const char *>(payload->Data), payload->DataSize);
                const auto *asset = state.assets->Find(id);
                if (asset && asset->type == AssetType::Texture && !asset->missing)
                    paletteDetail.normalTexture = id;
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SliderFloat("Normal tile size (metres)", &paletteDetail.normalMetres, .125f, 1024.f,
                           "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Normal strength", &paletteDetail.normalStrength, 0.f, 2.f);
    }
    if (ImGui::Button("Apply surface materials"))
        ApplyPalette();
    ImGui::SameLine();
    if (ImGui::Button("Revert material choices"))
        ReadPalette();
}
bool TerrainV2Editor::Panel(EditorDocument &doc, EditorPanelState &state,
                            EditorRequests &requests) {
    if (!state.project || !state.assets)
        return false;
    if (projectKey != state.project->ProjectFile()) {
        if (dirty || Busy())
            return Active();
        projectKey = state.project->ProjectFile();

        activeProject = false;
        resetPending = true;
        ClearPreview();
        loaded = false;
        path.clear();
        DiscoverSources(state);
        if (!sources.empty()) {
            path = sources.front();
            activeProject = true;
        }
    }
    auto selectOwner = [&] {
        const auto *selected = doc.GetScene().Find(doc.Selected());
        if (selected && selected->render && selected->render->meshAsset == meshId)
            return;
        for (const auto &object : doc.GetScene().Objects())
            if (object.render && object.render->meshAsset == meshId) {
                doc.Select(object.id);
                break;
            }
    };
    if (state.terrainCreateRequested) {
        if (dirty || PaletteDirty() || productsDirty || Busy() || gesture)
            state.status =
                "Save/update or discard current terrain edits before creating another source.";
        else {
            BeginCreation();
            state.terrainCreateRequested = false;
        }
    }
    if (!state.terrainEditAsset.empty()) {
        if (dirty || PaletteDirty() || productsDirty || Busy() || gesture)
            state.status =
                "Save/update or discard current terrain edits before opening another source.";
        else {
            const auto *asset = state.assets->Find(state.terrainEditAsset);
            DiscoverSources(state);
            auto found = sources.end();
            if (asset && asset->type == AssetType::Terrain)
                found = std::find_if(sources.begin(), sources.end(), [&](const auto &candidate) {
                    return fs::path(candidate)
                               .lexically_relative(state.project->RootDir())
                               .generic_string() == asset->source;
                });
            if (found == sources.end() && asset && asset->type == AssetType::Terrain) {
                const auto count =
                    std::count_if(sources.begin(), sources.end(), [&](const auto &candidate) {
                        return fs::path(candidate).stem() == fs::path(asset->path).stem();
                    });
                if (count == 1)
                    found =
                        std::find_if(sources.begin(), sources.end(), [&](const auto &candidate) {
                            return fs::path(candidate).stem() == fs::path(asset->path).stem();
                        });
            }
            if (found == sources.end())
                state.status = "This native terrain has no matching editable source under "
                               "Authoring. Keep the source files with your project.";
            else {
                path = *found;

                activeProject = true;
                ClearPreview();
                resetPending = true;
                loaded = false;
                if (Open(state))
                    selectOwner();
            }
            state.terrainEditAsset.clear();
        }
    }
    activeProject = true;
    frameState = &state;
    if (!state.showTerrain)
        return true;
    ImGui::SetNextWindowSize({480, 650}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Terrain - sculpt / paint", &state.showTerrain)) {
        ImGui::End();
        return true;
    }
    ImGui::TextUnformatted("Terrain V2 — landscape and caves");
    ImGui::TextWrapped("One source owns the landscape, painted materials and excavations. Save and "
                       "update publishes the editor preview and the playable source together.");
    ImGui::BeginDisabled(dirty || PaletteDirty() || productsDirty || Busy() || gesture ||
                         creation.Active());
    if (ImGui::Button("New landscape"))
        creation.Show();
    ImGui::SameLine();
    if (ImGui::Button("Refresh terrain sources"))
        DiscoverSources(state);
    if (!sources.empty() &&
        ImGui::BeginCombo("Source", fs::path(path).filename().string().c_str())) {
        for (const auto &candidate : sources)
            if (ImGui::Selectable(fs::path(candidate).filename().string().c_str(),
                                  candidate == path)) {
                path = candidate;
                ClearPreview();
                resetPending = true;
                loaded = false;
                if (Open(state))
                    selectOwner();
            }
        ImGui::EndCombo();
    }

    ImGui::EndDisabled();
    creation.Panel(doc, state, requests);
    if (auto created = creation.TakeCreatedPath(); !created.empty()) {
        path = std::move(created);
        activeProject = true;
        ClearPreview();
        resetPending = true;
        loaded = false;
        DiscoverSources(state);
        Open(state);
    }
    if (creation.Active()) {
        ImGui::End();
        return true;
    }
    if (!loaded) {
        ImGui::TextWrapped("%s", path.c_str());
        if (ImGui::Button("Edit landscape"))
            Open(state);
        if (!error.empty())
            ImGui::TextWrapped("%s", error.c_str());
        ImGui::End();
        return true;
    }
    auto *selected = doc.GetScene().Find(doc.Selected());
    const bool matching = selected && selected->render && selected->render->meshAsset == meshId;
    if (!matching && ImGui::Button("Select landscape")) {
        for (const auto &o : doc.GetScene().Objects())
            if (o.render && o.render->meshAsset == meshId) {
                doc.Select(o.id);
                break;
            }
    }
    ImGui::Checkbox("Brush in scene view", &editing);
    ImGui::BeginDisabled(Busy());
    const char *modes[] = {"Raise", "Lower", "Flatten", "Smooth", "Paint", "Excavate", "Add rock"};
    ImGui::Combo("Tool", &mode, modes, 7);
    const float maximumRadius = mode >= 5 ? 64.f : 128.f;
    if (source.spacing <= maximumRadius) {
        brush.radius = std::clamp(brush.radius, source.spacing, maximumRadius);
        ImGui::SliderFloat("Radius (metres)", &brush.radius, source.spacing, maximumRadius);
    } else
        ImGui::TextWrapped("This source is too coarse for excavation; use a local detailed "
                           "landscape patch with spacing of 64 metres or less.");
    ImGui::SliderFloat("Strength", &brush.strength, .01f, 1);
    PalettePanel(state);
    if (mode == 4) {
        std::array<std::string, 4> names;
        for (unsigned i = 0; i < 4; ++i) {
            auto *a = state.assets->Find(source.layerTextures[i]);
            names[i] = a ? fs::path(a->path).stem().string() : "Material " + std::to_string(i + 1);
        }
        if (ImGui::BeginCombo("Paint material", names[brush.layer].c_str())) {
            for (unsigned i = 0; i < 4; ++i) {
                ImGui::PushID(int(i));
                const auto c = source.layerColours[i];
                ImGui::ColorButton("Colour", {c.r, c.g, c.b, 1});
                ImGui::SameLine();
                if (ImGui::Selectable(names[i].c_str(), brush.layer == i))
                    brush.layer = i;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::TextWrapped("Drag on the selected landscape. One gesture = one undo. Escape cancels. "
                       "Excavation works through the actual hillside, including chunk boundaries.");
    if (ImGui::Button("Undo")) Undo(state);
    ImGui::SameLine();
    if (ImGui::Button("Redo")) Redo(state);
    if (EditorDesign::ActionButton("Save and update playable terrain",
                                   EditorDesign::Tone::Primary)) {
        ApplyPalette();
        const auto bytes = TerrainV2::Serialize(source);
        const auto journal = fs::path(path).parent_path().string();
        const std::vector<AuthoringOutput> outputs{{path, bytes, disk, true}};
        if (ValidateTerrainWritePath(path, error) &&
            PublishAuthoringOutputs(journal, outputs, error)) {
            disk = bytes;
            dirty = false;
            const auto *asset = state.assets->Find(meshId);
            if (asset) {
                auto work = std::make_shared<Cook>();
                const auto sourcePath = path, output = asset->path;
                work->job = state.importJobs->Submit(
                    [work, sourcePath, output](JobContext &context) {
                        std::string a = "terrain", b = "v2-native",
                                    c = sourcePath, d = output;
                        char *argv[] = {a.data(), b.data(), c.data(), d.data()};
                        if (TerrainV2AuthorCommand(4, argv) != 0) {
                            work->error =
                                "Terrain cook failed; see the console. Saved source retained.";
                            context.SetError(work->error);
                        }
                    },
                    JobPriority::Normal, "Terrain V2 publish authoring products");
                cook = std::move(work);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard edits"))
        Open(state);
    ImGui::EndDisabled();
    if (cook && state.importJobs->IsFinished(cook->job)) {
        if (state.importJobs->StateOf(cook->job) != JobState::Completed)
            error = state.importJobs->ErrorOf(cook->job);
        else {
            requests.rescanAssets = true;
            error.clear();
            productsDirty = false;
        }
        state.importJobs->Forget(cook->job);
        cook.reset();
    }
    ImGui::Text("%u x %u samples | %zu chunks | %s", source.samplesX, source.samplesZ,
                previewData ? previewData->Chunks().size() : size_t(0), dirty ? "Unsaved" : "Saved");
    if (Busy())
        ImGui::TextUnformatted("Preparing changed chunks in the background…");
    if (!error.empty())
        ImGui::TextWrapped("%s", error.c_str());
    ImGui::End();
    return true;
}
void TerrainV2Editor::Prepare(Renderer &renderer, const EditorPanelState &state) {
    if (resetPending) {
        previewRenderer.Clear(renderer);
        if (weights.IsValid())
            renderer.DestroyTexture(weights);
        weights = {};
        resetPending = false;
    }
    if (task && state.importJobs->IsFinished(task->job)) {
        if (state.importJobs->StateOf(task->job) == JobState::Completed &&
            task->revision == revision) {
            previewRenderer.Publish(renderer, task->preview);
            previewData = task->preview;
            preview = previewData->Mesh();
            error.clear();
        } else
            error = state.importJobs->ErrorOf(task->job);
        const bool stale = task->revision != revision;
        state.importJobs->Forget(task->job);
        task.reset();
        if (stale)
            Queue(All(source), *frameState);
    }
    if (appearanceDirty && loaded) {
        TextureData image;
        image.width = int(source.samplesX);
        image.height = int(source.samplesZ);
        for (auto w : source.weights)
            for (unsigned k = 0; k < 4; ++k)
                image.pixels.push_back(uint8_t(std::lround(w[k] * 255)));
        if (weights.IsValid())
            renderer.DestroyTexture(weights);
        weights = renderer.CreateTexture(image);
        appearanceDirty = false;
    }
}
bool TerrainV2Editor::Dab(glm::vec3 point, EditorPanelState &state) {
    glm::vec3 samplePoint;
    if (!TerrainV2::SourcePoint(source, point, samplePoint, error))
        return false;
    std::vector<TerrainV2::ChunkKey> changed;
    if (mode >= 5) {
        TerrainV2::Edit e;
        e.kind = mode == 5 ? TerrainV2::Edit::Kind::Remove : TerrainV2::Edit::Kind::Add;
        e.point = samplePoint;
        e.radius = brush.radius;
        auto next = source;
        next.edits.push_back(e);
        if (!TerrainV2::Validate(next, error))
            return false;
        changed = TerrainV2::Affected(next, e);
        source = std::move(next);
    } else {
        brush.kind = TerrainV2::SurfaceBrush::Kind(mode);
        auto applied = brush;
        if (source.chart)
            applied.height += float(source.chart->originHeight);
        if (!TerrainV2::ApplySurfaceBrush(source, samplePoint, applied, changed, error))
            return false;
        appearanceDirty |= mode == 4;
    }
    dirty = true;
    productsDirty = true;
    ++revision;
    if (mode != 4)
        Queue(changed, state); // Paint changes material samples, not triangles.
    lastDab = point;
    return true;
}
bool TerrainV2Editor::Stroke(const Scene &scene, SceneObjectId id, glm::vec3 origin,
                             glm::vec3 direction, bool pressed, bool down, bool cancel,
                             bool focused, std::string &status) {
    footprint.Clear();
    brushHint.reset();
    if (!activeProject || !loaded || !frameState)
        return false;
    if (cancel && gesture) {
        source = before;
        gesture = false;
        dirty = true;
        productsDirty = true;
        ++revision;
        appearanceDirty = true;
        if (!Busy())
            Queue(All(source), *frameState);
        return true;
    }
    if ((!down || !focused) && gesture) {
        gesture = false;
        undo.push_back(std::move(before));
        redo.clear();
        TrimHistory();
    }
    if (!editing || !focused || !previewData)
        return false;
    auto *object = scene.Find(id);
    if (!object || !object->render || object->render->meshAsset != meshId)
        return down;
    glm::vec3 hit;
    const auto inverse = glm::inverse(object->transform.rotation);
    if (!previewData->Pick(inverse * (origin - object->transform.position), inverse * direction,
                           hit))
        return down;
    brushHint = TerrainV2::Edit{TerrainV2::Edit::Kind::Remove, hit, brush.radius};
    for (unsigned axis = 0; axis < 3; ++axis)
        for (unsigned i = 0; i < 48; ++i) {
            glm::vec3 p[2];
            for (unsigned j = 0; j < 2; ++j) {
                float a = (i + j) * 6.2831853f / 48;
                p[j] = hit;
                p[j][(axis + 1) % 3] += std::cos(a) * brush.radius;
                p[j][(axis + 2) % 3] += std::sin(a) * brush.radius;
                p[j] = object->transform.position + object->transform.rotation * p[j];
            }
            footprint.Line(p[0], p[1], {.2f, .95f, .8f});
        }
    if (!down || Busy())
        return down;
    if (pressed) {
        before = source;
        gesture = true;
        glm::vec3 chartHit;
        if (!TerrainV2::SourcePoint(source, hit, chartHit, error))
            return down;
        brush.height = chartHit.y - (source.chart ? float(source.chart->originHeight) : 0.f);
        Dab(hit, *frameState);
    } else if (gesture && glm::length(hit - lastDab) > brush.radius * .2f)
        Dab(hit, *frameState);
    if (!error.empty())
        status = error;
    return down;
}
void TerrainV2Editor::Draw(Renderer &renderer, Scene &scene, ResourceManager &resources) {
    if (!loaded || !preview)
        return;
    std::array<TextureHandle, 4> layers;
    for (unsigned i = 0; i < 4; ++i)
        if (!source.layerTextures[i].empty()) {
            resources.RequestTexture(source.layerTextures[i]);
            layers[i] = resources.TryGetTexture(source.layerTextures[i]);
        }
    TextureHandle normal;
    if (!source.materialDetail.normalTexture.empty()) {
        resources.RequestTexture(source.materialDetail.normalTexture);
        normal = resources.TryGetTexture(source.materialDetail.normalTexture);
    }
    for (auto &object : scene.Objects())
        if (object.render && object.render->visible && object.render->meshAsset == meshId) {
            renderer.SetRenderLayer(object.renderLayer);
            renderer.SetMaterialBindings(BuildRenderMaterialBindings(&resources, *object.render));
            renderer.BeginTerrainMaterial(
                weights, layers, source.layerColours,
                glm::inverse(glm::translate(glm::mat4(1), object.transform.position) *
                             glm::mat4_cast(object.transform.rotation)),
                source.materialDetail, normal);
            previewRenderer.Draw(renderer, object.transform.position, object.transform.rotation,
                                 object.render->color, brushHint);
            renderer.EndTerrainMaterial();
            object.render->visible = false;
        }
}
bool TerrainV2Editor::Guard(EditorPanelState &state) {
    if (Active() && (dirty || PaletteDirty() || productsDirty || Busy() || gesture)) {
        state.showTerrain = true;
        error = "Save and update terrain, or discard the source edits, before Play or closing.";
        state.status = error;
        return false;
    }
    return true;
}
void TerrainV2Editor::Shutdown(Renderer &renderer, JobSystem *jobs) {
    creation.Shutdown(jobs);
    for (auto h : {task ? task->job : JobHandle{}, cook ? cook->job : JobHandle{}})
        if (h.IsValid() && jobs) {
            jobs->Cancel(h);
            jobs->Wait(h);
            jobs->Forget(h);
        }
    task.reset();
    cook.reset();
    previewRenderer.Clear(renderer);
    if (weights.IsValid())
        renderer.DestroyTexture(weights);
    weights = {};
    ClearPreview();
}

void TerrainV2Editor::Undo(EditorPanelState &state) {
    if (undo.empty() || Busy()) return;

        redo.push_back(source);
        source = std::move(undo.back());
        ReadPalette();
        undo.pop_back();
        dirty = true;
        productsDirty = true;
        ++revision;
        appearanceDirty = true;
        Queue(All(source), state);
        TrimHistory();
    }

void TerrainV2Editor::Redo(EditorPanelState &state) {
    if (redo.empty() || Busy()) return;

        undo.push_back(source);
        source = std::move(redo.back());
        ReadPalette();
        redo.pop_back();
        dirty = true;
        productsDirty = true;
        ++revision;
        appearanceDirty = true;
        Queue(All(source), state);
        TrimHistory();
    }
