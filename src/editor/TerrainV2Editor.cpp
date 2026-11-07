#include "TerrainV2Editor.h"
#include "../../third_party/nlohmann/json.hpp"
#include "Project.h"
#include "SceneFingerprint.h"
#include "WorldPresentation.h"
#include "authoring/AuthoringDocument.h"
#include "authoring/TerrainAuthoring.h"
#include "imgui.h"
#include "terrain/TerrainCook.h"
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
    for (const auto &[id, asset] : state.assets->Records())
        if (asset.type == AssetType::Mesh && fs::path(asset.path).stem() == model) {
            meshId = id;
            break;
        }
    if (meshId.empty()) {
        error = "Cook this source to a matching .judasmodel asset before viewport editing.";
        return false;
    }
    source = std::move(next);
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
            size_t triangles = 0;
            for (auto key : keys) {
                TerrainV2::Chunk chunk;
                if (!TerrainV2::BuildChunk(*captured, key, chunk, work->error,
                                           [&] { return context.CancelRequested(); })) {
                    if (context.CancelRequested())
                        context.ReportCancelled();
                    else
                        context.SetError(work->error);
                    return;
                }
                triangles += chunk.mesh.indices.size() / 3;
                if (triangles > 524288) {
                    work->error = "Preview exceeds 524288 prepared triangles; choose a smaller "
                                  "editable region.";
                    context.SetError(work->error);
                    return;
                }
                work->chunks.push_back(std::move(chunk));
            }
        },
        JobPriority::High, "Terrain V2 editor dirty chunks");
    if (work->job.IsValid())
        task = std::move(work);
    else
        error = "Terrain preview job could not be queued.";
}
bool TerrainV2Editor::Panel(EditorDocument &doc, EditorPanelState &state,
                            EditorRequests &requests) {
    if (!state.project || !state.assets)
        return false;
    if (projectKey != state.project->ProjectFile()) {
        if (dirty || Busy())
            return activeProject;
        projectKey = state.project->ProjectFile();
        activeProject = false;
        resetPending = true;
        preview.reset();
        loaded = false;
        path.clear();
        const auto root = fs::path(state.project->RootDir()) / "Authoring";
        std::error_code ec;
        if (fs::is_directory(root, ec))
            for (const auto &file : fs::directory_iterator(root, ec))
                if (file.path().extension() == ".judasterrain2") {
                    path = file.path().string();
                    activeProject = true;
                    break;
                }
    }
    if (!activeProject)
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
    if (!loaded) {
        ImGui::TextWrapped("%s", path.c_str());
        if (ImGui::Button("Edit landscape"))
            Open(state);
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
    ImGui::SliderFloat("Radius (metres)", &brush.radius, source.spacing, 8);
    ImGui::SliderFloat("Strength", &brush.strength, .01f, 1);
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
        undo.pop_back();
        dirty = true;
        productsDirty = true;
        ++revision;
        appearanceDirty = true;
        Queue(All(source), state);
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo") && !redo.empty()) {
        undo.push_back(source);
        source = std::move(redo.back());
        redo.pop_back();
        dirty = true;
        productsDirty = true;
        ++revision;
        appearanceDirty = true;
        Queue(All(source), state);
    }
    if (ImGui::Button("Save and update playable terrain")) {
        const auto bytes = TerrainV2::Serialize(source);
        if (PublishAuthoringOutputs(fs::path(path).parent_path().string(),
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
    Queue(changed, state);
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
        if (undo.size() > 16)
            undo.erase(undo.begin());
        redo.clear();
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
    if (activeProject && (dirty || productsDirty || Busy() || gesture)) {
        state.showTerrain = true;
        error = "Save and update terrain, or discard the source edits, before Play or closing.";
        state.status = error;
        return false;
    }
    return true;
}
void TerrainV2Editor::Shutdown(Renderer &renderer, JobSystem *jobs) {
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
