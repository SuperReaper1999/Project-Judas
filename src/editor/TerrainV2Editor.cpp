#include "TerrainV2Editor.h"
#include "../../third_party/nlohmann/json.hpp"
#include "EditorDesign.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "WorldPresentation.h"
#include "authoring/AuthoringDocument.h"
#include "authoring/TerrainAuthoring.h"
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
    work->job = state.importJobs->Submit(
        [work, captured, keys](JobContext &context) {
            if (!TerrainV2::BuildChunks(
                    *captured, keys, work->chunks, work->error,
                    [&] { return context.CancelRequested(); }, 1048576)) {
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
}
bool TerrainV2Editor::PaletteDirty() const {
    return loaded &&
           (paletteTextures != source.layerTextures || paletteColours != source.layerColours);
}
void TerrainV2Editor::ApplyPalette() {
    if (!PaletteDirty())
        return;
    undo.push_back(source);
    redo.clear();
    source.layerTextures = paletteTextures;
    source.layerColours = paletteColours;
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
        ImGui::Text("Layer %u", i + 1);
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
        ImGui::PopID();
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
        legacy = false;
        activeProject = false;
        resetPending = true;
        preview.reset();
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
                legacy = false;
                activeProject = true;
                preview.reset();
                resetPending = true;
                loaded = false;
                if (Open(state))
                    selectOwner();
            }
            state.terrainEditAsset.clear();
        }
    }
    if (!Active())
        return false;
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
                preview.reset();
                resetPending = true;
                loaded = false;
                if (Open(state))
                    selectOwner();
            }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Legacy terrain tools")) {
        legacy = true;
        editing = false;
    }
    ImGui::EndDisabled();
    creation.Panel(doc, state, requests);
    if (auto created = creation.TakeCreatedPath(); !created.empty()) {
        path = std::move(created);
        activeProject = true;
        preview.reset();
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
    if (ImGui::Button("Undo") && !undo.empty()) {
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
    ImGui::SameLine();
    if (ImGui::Button("Redo") && !redo.empty()) {
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
    if (EditorDesign::ActionButton("Save and update playable terrain",
                                   EditorDesign::Tone::Primary)) {
        ApplyPalette();
        const auto bytes = TerrainV2::Serialize(source);
        if (ValidateTerrainWritePath(path, error) &&
            PublishAuthoringOutputs(fs::path(path).parent_path().string(),
                                    {{path, bytes, disk, true}}, error)) {
            disk = bytes;
            dirty = false;
            const auto *asset = state.assets->Find(meshId);
            if (asset) {
                auto work = std::make_shared<Cook>();
                const auto sourcePath = path, output = asset->path;
                const bool native =
                    std::any_of(doc.GetScene().Objects().begin(), doc.GetScene().Objects().end(),
                                [&](const auto &o) {
                                    return o.landscape && o.render && o.render->meshAsset == meshId;
                                });
                work->job = state.importJobs->Submit(
                    [work, sourcePath, output, native](JobContext &context) {
                        std::string a = "terrain", b = native ? "v2-native" : "v2-cook",
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
                chunks.size(), dirty ? "Unsaved" : "Saved");
    if (Busy())
        ImGui::TextUnformatted("Preparing changed chunks in the background…");
    if (!error.empty())
        ImGui::TextWrapped("%s", error.c_str());
    ImGui::End();
    return true;
}
void TerrainV2Editor::Prepare(Renderer &renderer, const EditorPanelState &state) {
    if (resetPending) {
        for (auto &[_, r] : chunks)
            if (r.mesh.IsValid())
                renderer.DestroyMesh(r.mesh);
        chunks.clear();
        if (weights.IsValid())
            renderer.DestroyTexture(weights);
        weights = {};
        resetPending = false;
    }
    if (task && state.importJobs->IsFinished(task->job)) {
        if (state.importJobs->StateOf(task->job) == JobState::Completed &&
            task->revision == revision) {
            for (auto &chunk : task->chunks) {
                auto &r = chunks[chunk.key];
                if (r.mesh.IsValid())
                    renderer.DestroyMesh(r.mesh);
                r.mesh = {};
                r.chunk = std::move(chunk);
            }
            auto mesh = std::make_shared<MeshData>();
            for (const auto &[_, r] : chunks) {
                unsigned offset = unsigned(mesh->vertices.size());
                for (auto v : r.chunk.mesh.vertices) {
                    v.position += r.chunk.anchor;
                    mesh->vertices.push_back(v);
                }
                for (auto i : r.chunk.mesh.indices)
                    mesh->indices.push_back(offset + i);
            }
            preview = std::move(mesh);
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
    std::vector<TerrainV2::ChunkKey> changed;
    if (mode >= 5) {
        TerrainV2::Edit e;
        e.kind = mode == 5 ? TerrainV2::Edit::Kind::Remove : TerrainV2::Edit::Kind::Add;
        e.point = point;
        e.radius = brush.radius;
        auto next = source;
        next.edits.push_back(e);
        if (!TerrainV2::Validate(next, error))
            return false;
        changed = TerrainV2::Affected(next, e);
        source = std::move(next);
    } else {
        brush.kind = TerrainV2::SurfaceBrush::Kind(mode);
        if (!TerrainV2::ApplySurfaceBrush(source, point, brush, changed, error))
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
    if (!editing || !focused || !preview)
        return false;
    auto *object = scene.Find(id);
    if (!object || !object->render || object->render->meshAsset != meshId)
        return down;
    glm::vec3 hit;
    const auto inverse = glm::inverse(object->transform.rotation);
    if (!PickTerrain(*preview, inverse * (origin - object->transform.position), inverse * direction,
                     hit))
        return down;
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
        brush.height = hit.y;
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
    for (auto &object : scene.Objects())
        if (object.render && object.render->visible && object.render->meshAsset == meshId) {
            renderer.SetRenderLayer(object.renderLayer);
            renderer.SetMaterialBindings(BuildRenderMaterialBindings(&resources, *object.render));
            renderer.BeginTerrainMaterial(
                weights, layers, source.layerColours,
                glm::inverse(glm::translate(glm::mat4(1), object.transform.position) *
                             glm::mat4_cast(object.transform.rotation)));
            for (auto &[_, r] : chunks) {
                if (r.chunk.mesh.indices.empty())
                    continue;
                if (!r.mesh.IsValid())
                    r.mesh = renderer.CreateMesh(r.chunk.mesh);
                renderer.DrawMesh(
                    r.mesh, object.transform.position + object.transform.rotation * r.chunk.anchor,
                    object.transform.rotation, glm::vec3(1), {}, object.render->color, 1);
            }
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
    for (auto &[_, r] : chunks)
        if (r.mesh.IsValid())
            renderer.DestroyMesh(r.mesh);
    chunks.clear();
    if (weights.IsValid())
        renderer.DestroyTexture(weights);
    weights = {};
}
