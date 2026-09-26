#include "EditorApplication.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include <glm/gtc/quaternion.hpp>

#include "EngineHost.h"
#include "EnginePaths.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "TerrainLibrary.h"
#include "WorldCoordinates.h"
#include "WorldDebugView.h"
#include "WorldPresentation.h"
#include "WorldState.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

namespace fs = std::filesystem;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr const char* kProjectFileHint = "Relative to the working directory or absolute, e.g. projects/tiny_game/tiny_game.judasproj";

// Viewport picking uses a bounding SPHERE per object — generous for
// empties and lights so they can still be clicked, and deliberately
// approximate: a long thin plank picks as a big ball. Precise mesh/box
// picking is a documented M30 limitation (docs/ARCHITECTURE.md).
float PickRadius(const SceneObject& o) {
    float radius = 0.75f;
    if (o.render) {
        if (o.render->shape == SceneShape::Sphere) radius = std::max(radius, o.render->radius);
        else if (o.render->shape == SceneShape::Box) radius = std::max(radius, glm::length(o.render->halfExtents));
        else if (o.render->shape == SceneShape::Mesh) radius = std::max(radius, 2.0f * std::max(o.transform.scale.x, std::max(o.transform.scale.y, o.transform.scale.z)));
    }
    if (o.body) {
        if (o.body->shape == SceneShape::Sphere) radius = std::max(radius, o.body->radius);
        else if (o.body->shape == SceneShape::Box) radius = std::max(radius, glm::length(o.body->halfExtents));
        else if (o.body->shape == SceneShape::Terrain) radius = std::max(radius, 90.0f);
    }
    if (o.gravity) {
        radius = std::max(radius, o.gravity->regionShape == SceneRegionShape::Sphere ? o.gravity->regionRadius * 0.1f
                                                                                    : glm::length(o.gravity->regionHalfExtents) * 0.1f);
    }
    return radius;
}

bool RaySphere(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& centre, float radius,
               float& outDistance) {
    const glm::vec3 oc = origin - centre;
    const float b = glm::dot(oc, direction);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) return false;
    const float t = -b - std::sqrt(discriminant);
    if (t < 0.0f) return false;
    outDistance = t;
    return true;
}

// The selection outline: the object's own shape where it has one.
void BuildSelectionLines(const SceneObject& o, DebugLineList& out) {
    const glm::vec3 color(1.0f, 0.85f, 0.2f);
    const glm::vec3 position = o.transform.position;
    const glm::quat rotation = glm::normalize(o.transform.rotation);
    bool drawn = false;
    if (o.render) {
        if (o.render->shape == SceneShape::Box) { out.Box(position, rotation, o.render->halfExtents * 1.02f, color); drawn = true; }
        else if (o.render->shape == SceneShape::Sphere) { out.Sphere(position, o.render->radius * 1.02f, color); drawn = true; }
    }
    if (!drawn && o.body) {
        if (o.body->shape == SceneShape::Box) { out.Box(position, rotation, o.body->halfExtents * 1.02f, color); drawn = true; }
        else if (o.body->shape == SceneShape::Sphere) { out.Sphere(position, o.body->radius * 1.02f, color); drawn = true; }
        else if (o.body->shape == SceneShape::Compound) {
            for (const CompoundBox& box : o.body->compoundBoxes) out.Box(position + rotation * box.localCenter, rotation, box.halfExtents * 1.02f, color);
            drawn = true;
        }
    }
    if (!drawn) out.Sphere(position, PickRadius(o), color, 16);
}

Scene MakeStarterScene() {
    Scene scene;
    scene.Settings().name = "Main";
    SceneObject& ground = scene.CreateObject("Ground");
    ground.transform.position = glm::vec3(0.0f, -0.5f, 0.0f);
    ground.render = SceneRenderComponent{};
    ground.render->shape = SceneShape::Box;
    ground.render->halfExtents = glm::vec3(20.0f, 0.5f, 20.0f);
    ground.render->color = glm::vec3(0.4f, 0.44f, 0.36f);
    ground.body = SceneBodyComponent{};
    ground.body->shape = SceneShape::Box;
    ground.body->halfExtents = glm::vec3(20.0f, 0.5f, 20.0f);
    ground.body->friction = 0.8f;
    ground.body->restitution = 0.05f;
    ground.gravity = SceneGravityComponent{};
    ground.gravity->kind = SceneGravityKind::Uniform;
    ground.gravity->regionShape = SceneRegionShape::Box;
    ground.gravity->regionHalfExtents = glm::vec3(30.0f);
    SceneObject& start = scene.CreateObject("Player start");
    start.transform.position = glm::vec3(0.0f, 1.0f, 5.0f);
    start.playerStart = ScenePlayerStartComponent{};
    SceneObject& lamp = scene.CreateObject("Lamp");
    lamp.transform.position = glm::vec3(0.0f, 4.0f, 0.0f);
    lamp.light = SceneLightComponent{};
    lamp.light->color = glm::vec3(2.5f, 2.3f, 2.0f);
    lamp.light->range = 18.0f;
    return scene;
}
}  // namespace

EditorApplication::EditorApplication() = default;
EditorApplication::~EditorApplication() = default;

void EditorApplication::RefreshProjectLists() {
    m_panels.terrainSurfaces = KnownTerrainSurfaces();
    m_panels.sceneFiles.clear();
    m_panels.project = &m_project;
    m_panels.assets = m_host ? &m_host->Assets() : nullptr;
    m_panels.resources = m_host ? &m_host->Resources() : nullptr;
    if (!m_project.IsLoaded()) return;
    std::error_code ec;
    if (!fs::is_directory(m_project.ScenesDir(), ec)) return;
    for (const auto& entry : fs::recursive_directory_iterator(m_project.ScenesDir(), ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".judas") {
            m_panels.sceneFiles.push_back(m_project.MakeRelative(entry.path().generic_string()));
        }
    }
    std::sort(m_panels.sceneFiles.begin(), m_panels.sceneFiles.end());
}

bool EditorApplication::OpenProject(const std::string& projectFile, std::string& outError) {
    Project project;
    if (!project.Load(projectFile, outError)) return false;
    m_project = project;
    m_host->OpenProjectAssets(m_project.RootDir(), m_project.AssetsDir());
    RefreshProjectLists();
    m_panels.showProjectSettings = false;
    m_panels.browserSelection.clear();
    return true;
}

bool EditorApplication::CreateProject(const std::string& directory, const std::string& name, std::string& outError) {
    Project project;
    if (!Project::CreateNew(directory, name, project, outError)) return false;
    // A starter scene so the new project runs immediately.
    const std::string sceneRelative = project.Settings().scenesDir + "/main.judas";
    if (!SaveSceneToFile(MakeStarterScene(), project.Resolve(sceneRelative), outError)) return false;
    project.Settings().startupScene = sceneRelative;
    if (!project.Save(outError)) return false;
    if (!OpenProject(project.ProjectFile(), outError)) return false;
    return OpenScene(sceneRelative, outError);
}

std::string EditorApplication::ResolveScenePath(const std::string& input) const {
    if (input.empty() || fs::path(input).is_absolute()) return input;
    std::error_code ec;
    if (m_project.IsLoaded() && fs::exists(m_project.Resolve(input), ec)) return m_project.Resolve(input);
    if (fs::exists(input, ec)) return fs::absolute(input, ec).lexically_normal().generic_string();
    return m_project.IsLoaded() ? m_project.Resolve(input) : input;
}

std::string EditorApplication::WorldStatePathFor(const std::string& scenePath) const {
    if (scenePath.empty()) return std::string();
    return m_project.IsLoaded() ? m_project.WorldStatePathForScene(scenePath) : DefaultWorldStatePath(scenePath);
}

bool EditorApplication::OpenScene(const std::string& path, std::string& outError) {
    const std::string resolved = ResolveScenePath(path);
    if (!m_document.Load(resolved, outError)) return false;
    m_panels.pathInput = m_project.IsLoaded() ? m_project.MakeRelative(resolved) : resolved;
    if (const SceneObject* first = m_document.GetScene().Objects().empty() ? nullptr : &m_document.GetScene().Objects().front()) {
        m_camera.LookAt(first->transform.position, 40.0f);
    }
    return true;
}

bool EditorApplication::RunProject(std::string& outMessage) {
    if (!m_project.IsLoaded()) { outMessage = "no project open"; return false; }
    if (m_project.Settings().startupScene.empty()) { outMessage = "the project has no startup scene (Project settings)"; return false; }
    std::string saveError;
    if (!m_project.Save(saveError)) { outMessage = "could not save the project before running: " + saveError; return false; }
#if defined(__unix__) || defined(__APPLE__)
    // The runtime binary sits beside the editor binary in every build tree.
    const std::string runtime = EngineExecutableDir() + "/judas";
    std::error_code ec;
    if (!fs::is_regular_file(runtime, ec)) { outMessage = "runtime not found beside the editor: " + runtime; return false; }
    // The child resolves engine data (the UI font) the way this editor
    // did; pass that root on explicitly so it never depends on the CWD.
    const std::string fontPath = ResolveEngineDataPath("assets/fonts/DejaVuSans.ttf");
    const std::string engineRoot = fs::path(fontPath).parent_path().parent_path().parent_path().generic_string();
    std::vector<std::string> envStrings;
    for (char** e = environ; e && *e; ++e) {
        if (std::strncmp(*e, "JUDAS_ENGINE_ROOT=", 18) == 0) continue;
        // The editor's own automation hook must not leak into the game.
        if (std::strncmp(*e, "JUDAS_EDITOR_AUTOTEST=", 22) == 0) continue;
        envStrings.push_back(*e);
    }
    envStrings.push_back("JUDAS_ENGINE_ROOT=" + engineRoot);
    std::vector<char*> envp;
    for (std::string& s : envStrings) envp.push_back(s.data());
    envp.push_back(nullptr);
    std::string argv0 = runtime, argv1 = m_project.ProjectFile();
    char* argv[] = {argv0.data(), argv1.data(), nullptr};
    pid_t pid = 0;
    const int result = posix_spawn(&pid, runtime.c_str(), nullptr, nullptr, argv, envp.data());
    if (result != 0) { outMessage = "could not launch the runtime: " + std::string(std::strerror(result)); return false; }
    outMessage = "Launched " + runtime + " " + m_project.ProjectFile() + " (pid " + std::to_string(pid) + ") on " +
                 m_project.Settings().startupScene;
    return true;
#else
    outMessage = "Run project launches ./judas as a separate process; not implemented on this platform. Run: judas " +
                 m_project.ProjectFile();
    return false;
#endif
}

bool EditorApplication::StartPlay(std::string& outError) {
    m_world = std::make_unique<RuntimeWorld>();
    if (!m_world->Build(m_document.GetScene(), &m_host->Resources(), outError)) {
        m_world.reset();
        return false;
    }
    // Milestone 29: a saved world-state delta for this scene file layers
    // over the freshly instantiated baseline, exactly as the runtime does.
    m_panels.worldStatePath = WorldStatePathFor(m_document.Path());
    bool stateApplied = false;
    if (!ApplyWorldStateFileIfPresent(*m_world, m_panels.worldStatePath, stateApplied, outError)) {
        m_world.reset();
        return false;
    }
    m_play = std::make_unique<InteractivePlay>();
    if (!m_play->Begin(*m_world, WorldCoordinates(m_document.GetScene().Settings().worldOrigin), outError)) {
        m_play.reset();
        m_world.reset();
        return false;
    }
    m_play->SetWorldStatePath(m_panels.worldStatePath, stateApplied);
    // The debug view over the played world, inside the 3D frame.
    m_play->SetWorldOverlay([this](Renderer& renderer, float alpha) {
        if (!m_panels.debug.AnyEnabled() || !m_world) return;
        m_debugLines.Clear();
        BuildWorldDebugLines(*m_world, &m_play->Session(), alpha, m_panels.debug, m_debugLines);
        renderer.DrawDebugLines(m_debugLines.Lines());
    });
    m_panels.runtime = m_world.get();
    m_panels.mode = EditorMode::Play;
    m_panels.status = "Playing: Escape pauses (menu) and frees the mouse; Stop restores the authored scene";
    // Keys pressed while editing (F to focus, R, Space, ...) must not fire
    // as gameplay requests on the first played frame, and a gizmo drag in
    // progress is abandoned.
    m_drag = GizmoDrag{};
    m_hoverAxis = GizmoAxis::None;
    if (m_document.EditInProgress()) m_document.CancelEdit();
    m_host->GetWindow().ClearPendingRequests();
    m_host->GetWindow().SetMouseCaptured(true);
    return true;
}

void EditorApplication::StopPlay() {
    if (m_play) m_play->End();
    m_play.reset();
    m_world.reset();  // the authored Scene was never written; nothing to revert
    m_panels.runtime = nullptr;
    m_panels.mode = EditorMode::Edit;
    m_panels.runtimeInfo.clear();
    m_panels.status = "Stopped: authored scene restored";
    m_host->GetWindow().SetMouseCaptured(false);
}

void EditorApplication::HandleRequests(EditorRequests& r) {
    std::string error;
    if (r.quit) m_quit = true;
    if (r.play && m_panels.mode == EditorMode::Edit) {
        if (!StartPlay(error)) m_panels.status = "Play failed: " + error;
    }
    if (r.stop && m_panels.mode == EditorMode::Play) StopPlay();
    if (m_panels.mode == EditorMode::Play && m_play) {
        std::string message;
        if (r.saveWorldState) { m_play->SaveWorldStateNow(message); m_panels.status = message; }
        if (r.deleteWorldState) { m_play->DeleteWorldStateNow(message); m_panels.status = message; }
    }
    if (m_panels.mode != EditorMode::Edit) return;

    // --- Project ---
    if (r.newProject) ImGui::OpenPopup("New project");
    if (r.openProject) ImGui::OpenPopup("Open project");
    if (r.saveProject) m_panels.status = m_project.Save(error) ? "Saved project " + m_project.ProjectFile() : "Project save failed: " + error;
    if (r.runProject) {
        std::string message;
        RunProject(message);
        m_panels.status = message;
        m_panels.runProjectInfo = message;
    }
    if (!r.openSceneRelative.empty()) {
        m_panels.status = OpenScene(r.openSceneRelative, error) ? "Opened " + r.openSceneRelative : "Open failed: " + error;
    }

    // --- Assets ---
    bool assetsChanged = false;
    if (!r.importSource.empty()) {
        AssetRecord record;
        if (m_host->Assets().Import(r.importSource, r.importDestination, record, error)) {
            m_panels.status = "Imported " + record.relativePath + " as " + AssetTypeName(record.type) + " " + record.id;
            m_panels.browserSelection = record.id;
            m_panels.importSourceInput.clear();
            m_panels.importDestinationInput.clear();
        } else {
            m_panels.status = "Import failed: " + error;
        }
        assetsChanged = true;
    }
    if (!r.trackAssetPath.empty()) {
        AssetRecord record;
        m_panels.status = m_host->Assets().Track(r.trackAssetPath, record, error) ? "Tracked " + record.relativePath + " as " + record.id
                                                                                   : "Track failed: " + error;
        assetsChanged = true;
    }
    if (!r.moveAssetId.empty()) {
        // The GPU copy keyed by this id stays valid: the id did not change.
        m_panels.status = m_host->Assets().Move(r.moveAssetId, r.moveAssetTo, error) ? "Moved asset to " + r.moveAssetTo
                                                                                     : "Move failed: " + error;
        assetsChanged = true;
    }
    if (!r.removeAssetId.empty()) {
        m_host->Resources().Invalidate(r.removeAssetId);
        m_panels.status = m_host->Assets().Remove(r.removeAssetId, error) ? "Removed asset" : "Remove failed: " + error;
        m_panels.browserSelection.clear();
        assetsChanged = true;
    }
    if (r.rescanAssets || assetsChanged) {
        if (r.rescanAssets) m_host->OpenProjectAssets(m_project.RootDir(), m_project.AssetsDir());
        RefreshProjectLists();
    }

    // --- Scene ---
    if (r.newScene) {
        m_document.NewScene();
        m_panels.status = "New scene";
    }
    if (r.open) ImGui::OpenPopup("Open scene");
    if (r.saveAs) ImGui::OpenPopup("Save scene as");
    if (r.save) {
        if (m_document.Path().empty()) ImGui::OpenPopup("Save scene as");
        else {
            m_panels.status = m_document.Save(error) ? "Saved " + m_document.Path() : "Save failed: " + error;
            RefreshProjectLists();
        }
    }
    if (r.undo) m_document.Undo();
    if (r.redo) m_document.Redo();
    if (!r.createKind.empty()) {
        std::string defaultMesh;
        for (const auto& [id, record] : m_host->Assets().Records()) {
            if (record.type == AssetType::Mesh && !record.missing) { defaultMesh = id; break; }
        }
        CreateObjectOfKind(m_document, r.createKind, r.createPosition, defaultMesh);
        m_panels.status = "Created " + r.createKind;
    }
    if (!r.dropMeshAssetId.empty()) {
        CreateObjectOfKind(m_document, "mesh", r.createPosition, r.dropMeshAssetId);
        m_panels.status = "Placed mesh asset from the Asset Browser";
    }
    if (r.duplicateId != kInvalidSceneObjectId) {
        if (DuplicateObject(m_document, r.duplicateId) != kInvalidSceneObjectId) m_panels.status = "Duplicated object";
    }
    if (r.focusSelection) {
        if (const SceneObject* o = m_document.SelectedObject()) {
            m_camera.LookAt(o->transform.position, std::max(4.0f, PickRadius(*o) * 3.0f));
        }
    }

    // Path popups (the editor has no OS file dialog; a path field is enough
    // — see docs/ARCHITECTURE.md, "Milestone 30, Limitations").
    const auto pathPopup = [&](const char* title, const char* button, const char* label, const char* hint, auto&& action) {
        if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
        char buffer[512];
        std::strncpy(buffer, m_panels.pathInput.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText(label, buffer, sizeof(buffer))) m_panels.pathInput = buffer;
        ImGui::TextDisabled("%s", hint);
        if (ImGui::Button(button) && !m_panels.pathInput.empty()) {
            action(m_panels.pathInput);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    };
    pathPopup("Open scene", "Open", "Path (.judas)", "Relative to the project root, e.g. Scenes/main.judas",
              [&](const std::string& path) {
                  std::string e;
                  m_panels.status = OpenScene(path, e) ? "Opened " + path : "Open failed: " + e;
              });
    pathPopup("Save scene as", "Save", "Path (.judas)", "Relative to the project root, e.g. Scenes/level2.judas",
              [&](const std::string& path) {
                  std::string e;
                  const std::string resolved = m_project.IsLoaded() && !fs::path(path).is_absolute() ? m_project.Resolve(path) : path;
                  m_panels.status = m_document.SaveAs(resolved, e) ? "Saved " + resolved : "Save failed: " + e;
                  RefreshProjectLists();
              });
    pathPopup("Open project", "Open", "Project file (.judasproj)", kProjectFileHint, [&](const std::string& path) {
        std::string e;
        if (!OpenProject(path, e)) { m_panels.status = "Open project failed: " + e; return; }
        m_panels.status = "Opened project " + m_project.Settings().name;
        if (!m_project.Settings().startupScene.empty()) {
            if (!OpenScene(m_project.Settings().startupScene, e)) m_panels.status = "Project opened; startup scene failed: " + e;
        } else {
            m_document.NewScene();
        }
    });
    if (ImGui::BeginPopupModal("New project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char directory[512] = "";
        static char name[128] = "";
        ImGui::InputTextWithHint("Directory", "projects/my_game (created if missing)", directory, sizeof(directory));
        ImGui::InputTextWithHint("Name", "My Game", name, sizeof(name));
        ImGui::TextDisabled("Creates <dir>/<name>.judasproj with Assets/, Scenes/main.judas and Saves/.");
        if (ImGui::Button("Create") && directory[0] && name[0]) {
            std::string e;
            m_panels.status = CreateProject(directory, name, e) ? "Created project " + m_project.ProjectFile()
                                                                 : "New project failed: " + e;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void EditorApplication::PickAtPixel(int x, int y) {
    glm::vec3 origin, direction;
    m_camera.PixelRay(x, y, m_host->GetWindow().Width(), m_host->GetWindow().Height(), origin, direction);
    SceneObjectId best = kInvalidSceneObjectId;
    float bestDistance = 1.0e30f;
    for (const SceneObject& o : m_document.GetScene().Objects()) {
        float distance = 0.0f;
        if (RaySphere(origin, direction, o.transform.position, PickRadius(o), distance) && distance < bestDistance) {
            bestDistance = distance;
            best = o.id;
        }
    }
    m_document.Select(best);
}

void EditorApplication::UpdateGizmo(bool allowInteraction) {
    m_gizmoLines.Clear();
    SceneObject* selected = m_document.SelectedObject();
    if (!selected) {
        if (m_drag.Active()) { m_drag = GizmoDrag{}; m_document.CancelEdit(); }
        m_hoverAxis = GizmoAxis::None;
        return;
    }
    Window& window = m_host->GetWindow();
    int mx = 0, my = 0;
    window.GetMousePosition(mx, my);
    glm::vec3 rayOrigin, rayDirection;
    m_camera.PixelRay(mx, my, window.Width(), window.Height(), rayOrigin, rayDirection);
    const glm::vec3 origin = selected->transform.position;
    // Handles keep a roughly constant on-screen size.
    m_gizmoHandleLength = std::max(0.2f, glm::length(origin - m_camera.Position()) * 0.14f);
    const bool snap = m_panels.gizmoSnap || ImGui::GetIO().KeyCtrl;

    if (m_drag.Active()) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            selected->transform = UpdateGizmoDrag(m_drag, rayOrigin, rayDirection, snap);
        } else {
            // Release: one undo step for the whole drag.
            m_document.CommitEdit();
            m_drag = GizmoDrag{};
        }
    } else if (allowInteraction) {
        m_hoverAxis = PickGizmoAxis(m_panels.gizmoMode, rayOrigin, rayDirection, origin, glm::normalize(selected->transform.rotation),
                                    m_panels.gizmoSpace, m_gizmoHandleLength, m_gizmoHandleLength * 0.12f);
        if (m_hoverAxis != GizmoAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            GizmoDrag drag;
            if (BeginGizmoDrag(m_panels.gizmoMode, m_hoverAxis, rayOrigin, rayDirection, selected->transform,
                               m_panels.gizmoSpace, m_gizmoHandleLength, drag)) {
                m_drag = drag;
                m_document.BeginEdit();
            }
        }
    } else {
        m_hoverAxis = GizmoAxis::None;
    }
    BuildGizmoLines(m_panels.gizmoMode, selected->transform.position, glm::normalize(selected->transform.rotation),
                    m_panels.gizmoSpace, m_gizmoHandleLength, m_drag.Active() ? m_drag.axis : m_hoverAxis, m_gizmoLines);
}

void EditorApplication::DrawEditOverlay(Renderer& renderer, const Scene& scene) {
    m_debugLines.Clear();
    BuildAuthoredDebugLines(scene, m_panels.debug, m_debugLines);
    if (const SceneObject* selected = m_document.SelectedObject()) BuildSelectionLines(*selected, m_debugLines);
    renderer.DrawDebugLines(m_debugLines.Lines(), /*depthTest=*/true);
    renderer.DrawDebugLines(m_gizmoLines.Lines(), /*depthTest=*/false);
}

void EditorApplication::FrameEditMode(float deltaSeconds) {
    Window& window = m_host->GetWindow();
    Renderer& renderer = m_host->GetRenderer();
    const ImGuiIO& io = ImGui::GetIO();

    // Right mouse held over the viewport: capture the mouse and fly.
    const bool rightHeld = (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    if (rightHeld && !m_lookActive && !io.WantCaptureMouse) {
        m_lookActive = true;
        window.SetMouseCaptured(true);
    } else if (!rightHeld && m_lookActive) {
        m_lookActive = false;
        window.SetMouseCaptured(false);
    }
    int dx = 0, dy = 0;
    window.GetMouseDelta(dx, dy);
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool fast = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    m_camera.Update(window, deltaSeconds, m_lookActive && !io.WantCaptureKeyboard, dx, dy, fast);
    m_panels.cameraFocus = m_camera.Position() + m_camera.Forward() * 8.0f;

    // Gizmo first: a click on a handle is a drag, not a pick. The viewport
    // owns the mouse only when no panel does and the camera is not flying.
    const bool viewportOwnsMouse = !io.WantCaptureMouse && !m_lookActive;
    UpdateGizmo(viewportOwnsMouse);
    if (viewportOwnsMouse && !m_drag.Active() && m_hoverAxis == GizmoAxis::None &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int mx = 0, my = 0;
        window.GetMousePosition(mx, my);
        PickAtPixel(mx, my);
    }

    // Asset Browser drag released over the viewport: place the mesh where
    // the mouse ray crosses the plane through the camera focus.
    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
        if (payload->IsDataType(kAssetDragPayload)) {
            m_pendingDropAsset.assign(static_cast<const char*>(payload->Data), payload->DataSize);
        }
    } else if (!m_pendingDropAsset.empty()) {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            const AssetRecord* record = m_host->Assets().Find(m_pendingDropAsset);
            if (record && record->type == AssetType::Mesh) {
                int mx = 0, my = 0;
                window.GetMousePosition(mx, my);
                glm::vec3 rayOrigin, rayDirection;
                m_camera.PixelRay(mx, my, window.Width(), window.Height(), rayOrigin, rayDirection);
                glm::vec3 hit = m_panels.cameraFocus;
                RayPlaneIntersection(rayOrigin, rayDirection, m_panels.cameraFocus, -m_camera.Forward(), hit);
                CreateObjectOfKind(m_document, "mesh", hit, m_pendingDropAsset);
                m_panels.status = "Placed " + record->relativePath;
            } else {
                m_panels.status = "Only mesh assets can be dropped into the viewport";
            }
        }
        m_pendingDropAsset.clear();
    }

    const Scene& scene = m_document.GetScene();
    RefreshAssetDemand();
    const int height = std::max(window.Height(), 1);
    const float aspect = static_cast<float>(window.Width()) / static_cast<float>(height);
    renderer.SetLighting(glm::normalize(scene.Settings().sunDirection), scene.Settings().sunColor,
                         scene.Settings().ambientColor);
    renderer.BeginFrame(window.Width(), window.Height());
    renderer.SetCamera(m_camera.ViewMatrix(), m_camera.ProjectionMatrix(aspect));
    renderer.SetDynamicLights(BuildAuthoredLights(scene));
    DrawAuthoredScene(renderer, scene, m_host->Resources());
    DrawEditOverlay(renderer, scene);
    renderer.EndFrame();
}

void EditorApplication::RefreshAssetDemand() {
    std::vector<std::string> wanted;
    for (const SceneObject& o : m_document.GetScene().Objects()) {
        if (!o.render || o.render->shape != SceneShape::Mesh) continue;
        if (!o.render->meshAsset.empty()) wanted.push_back(o.render->meshAsset);
        if (!o.render->textureAsset.empty()) wanted.push_back(o.render->textureAsset);
    }
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
    if (wanted == m_heldAssets) return;
    ResourceManager& resources = m_host->Resources();
    for (const std::string& id : wanted) {
        if (!std::binary_search(m_heldAssets.begin(), m_heldAssets.end(), id)) resources.AddRef(id);
    }
    for (const std::string& id : m_heldAssets) {
        if (!std::binary_search(wanted.begin(), wanted.end(), id)) resources.ReleaseRef(id);
    }
    m_heldAssets = wanted;
}

void EditorApplication::CollectProfilerData(float frameDeltaSeconds) {
    ProfilerData& p = m_panels.profiler;
    m_frameAverageMilliseconds = glm::mix(m_frameAverageMilliseconds, frameDeltaSeconds * 1000.0f, 0.1f);
    p.frameMilliseconds = m_frameAverageMilliseconds;
    p.framesPerSecond = m_frameAverageMilliseconds > 0.0f ? 1000.0f / m_frameAverageMilliseconds : 0.0f;
    const RenderStats& stats = m_host->GetRenderer().Stats();
    p.drawCalls = stats.drawCalls;
    p.triangles = stats.triangles;
    p.shadowPasses = stats.shadowPasses;
    p.dynamicLights = stats.dynamicLights;
    p.debugLines = stats.debugLines;
    p.resources = m_host->Resources().Stats();
    p.jobs = m_host->Jobs().Stats();
    p.playing = m_panels.mode == EditorMode::Play && m_play && m_world;
    if (!p.playing) {
        p.fixedStepsThisFrame = 0;
        p.fixedStepMilliseconds = p.physicsMilliseconds = p.fluidMilliseconds = p.surfaceMilliseconds = p.sceneMilliseconds = 0.0f;
        p.physicsBodies = p.dynamicBodies = p.contacts = 0;
        p.entitiesFull = p.entitiesCoarse = p.entitiesDormant = p.entitiesDestroyed = 0;
        p.fluidParticles = 0;
        p.physics = PhysicsWorld::StepStats{};
        return;
    }
    p.fixedStepsThisFrame = m_play->LastFixedStepsThisFrame();
    p.fixedStepMilliseconds = static_cast<float>(m_play->LastFixedStepMilliseconds());
    p.physicsBodies = m_world->Physics().AliveBodyCount();
    p.dynamicBodies = m_world->Physics().DynamicBodyCount();
    p.contacts = m_world->Physics().LastStepContactCount();
    const RuntimeWorld::LifecycleCounts counts = m_world->CountLifecycle();
    p.entitiesFull = counts.full;
    p.entitiesCoarse = counts.coarse;
    p.entitiesDormant = counts.dormant;
    p.entitiesDestroyed = counts.destroyed;
    p.fluidParticles = m_world->HasFluid() ? m_world->Fluid().Particles().size() : 0;
    const FixedStepMeasurements& m = m_play->LastMeasurements();
    p.fluidMilliseconds = m.fluidMeasured ? static_cast<float>(m.fluidMilliseconds) : 0.0f;
    p.surfaceMilliseconds = static_cast<float>(m_play->LastSurfaceMilliseconds());
    p.sceneMilliseconds = static_cast<float>(m_play->LastSceneMilliseconds());
    p.physics = m_world->Physics().LastStepStats();
}

int EditorApplication::Run(int argc, char** argv) {
    if (argc > 2) {
        std::fprintf(stderr, "usage: judas_editor [project.judasproj | scene.judas]\n");
        return 1;
    }
    std::string error;
    EngineHost host;
    if (!host.Init("Project Judas Editor", kWindowWidth, kWindowHeight, true, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    m_host = &host;
    Window& window = host.GetWindow();
    Renderer& renderer = host.GetRenderer();
    window.SetMouseCaptured(false);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // fixed layout each launch; no files written beside the scene
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window.NativeWindow(), window.NativeGLContext());
    ImGui_ImplOpenGL3_Init("#version 330 core");
    window.SetEventHook([](const SDL_Event& event) { ImGui_ImplSDL2_ProcessEvent(&event); });

    // --- Milestone 30: open a project, then a scene ---
    std::string argument = argc == 2 ? argv[1] : std::string();
    std::string projectFile;
    std::string sceneArgument;
    if (!argument.empty() && argument.size() > 10 && argument.compare(argument.size() - 10, 10, ".judasproj") == 0) {
        projectFile = argument;
    } else if (!argument.empty()) {
        sceneArgument = argument;
        projectFile = Project::FindProjectFileFor(argument);
    } else {
        projectFile = Project::FindProjectFileFor(".");
    }
    if (const char* mode = std::getenv("JUDAS_RESOURCE_MODE")) {
        if (std::string(mode) == "blocking") host.Resources().SetBlockingMode(true);
    }
    RefreshProjectLists();
    if (!projectFile.empty()) {
        if (!OpenProject(projectFile, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        m_panels.status = "Opened project " + m_project.Settings().name;
    }
    if (!sceneArgument.empty()) {
        m_panels.status = OpenScene(sceneArgument, error) ? "Opened " + sceneArgument : "Open failed: " + error;
    } else if (m_project.IsLoaded() && !m_project.Settings().startupScene.empty()) {
        if (!OpenScene(m_project.Settings().startupScene, error)) m_panels.status = "Startup scene failed: " + error;
        else m_panels.status = "Opened project " + m_project.Settings().name + " at " + m_project.Settings().startupScene;
    } else {
        m_document.NewScene();
        m_panels.status = m_project.IsLoaded() ? "Project has no startup scene; new scene. Create > objects; right-drag to look."
                                               : "No project. File > New project or Open project. Right-drag to look, WASD/QE to fly.";
    }
    if (m_panels.status.rfind("Opened", 0) == 0) {
        m_panels.status += ". Right-drag looks, WASD/QE fly; click selects; W/E/R move/rotate/scale; X local/world; Ctrl snaps.";
    }

    // Developer/automation hook (see docs/ARCHITECTURE.md, "Milestone 30,
    // Automated evidence"): JUDAS_EDITOR_AUTOTEST=<prefix> selects the first
    // object, enables the debug view, renders the edit view, duplicates and
    // undoes (checking the scene is unchanged), enters Play, runs,
    // screenshots both, stops, verifies the authored scene is untouched,
    // saves it, prints profiler and resource lines, and quits.
    const char* autotest = std::getenv("JUDAS_EDITOR_AUTOTEST");
    int autotestFrame = 0;
    std::string autotestBaseline;
    if (autotest) SaveSceneToString(m_document.GetScene(), autotestBaseline);
    const auto screenshot = [&](const std::string& path) {
        std::vector<unsigned char> pixels;
        renderer.CaptureFrame(window.Width(), window.Height(), pixels);
        const bool written = WriteRgbPng(path, window.Width(), window.Height(), pixels);
        std::fprintf(stderr, "[editor autotest] %s: %s\n", written ? "wrote" : "FAILED", path.c_str());
    };

    EditorRequests deferredRequests;
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    while (!window.ShouldClose() && !m_quit) {
        // Last frame's UI focus decides whether the engine's own key/mouse
        // reading is suppressed this frame (a text field must not walk the
        // player or fly the camera).
        window.SetInputClaimed(io.WantCaptureKeyboard, io.WantCaptureMouse && !window.IsMouseCaptured());
        window.PollEvents();
        host.PumpResources();  // M31: GPU upload of finished loads, budget eviction
        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        renderer.ResetStats();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        EditorRequests requests;
        // Keyboard shortcuts (edit mode, no text field focused, not flying).
        if (m_panels.mode == EditorMode::Edit && !io.WantCaptureKeyboard) {
            const bool ctrl = io.KeyCtrl;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) requests.save = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) requests.undo = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) requests.redo = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) requests.duplicateId = m_document.Selected();
            if (ImGui::IsKeyPressed(ImGuiKey_F5)) requests.play = true;
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_document.Selected() != kInvalidSceneObjectId) {
                m_document.BeginEdit();
                m_document.GetScene().DestroyObject(m_document.Selected());
                m_document.CommitEdit();
                m_document.Select(kInvalidSceneObjectId);
            }
            if (!m_lookActive && !ctrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_F)) requests.focusSelection = true;
                if (ImGui::IsKeyPressed(ImGuiKey_W)) m_panels.gizmoMode = GizmoMode::Translate;
                if (ImGui::IsKeyPressed(ImGuiKey_E)) m_panels.gizmoMode = GizmoMode::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_R)) m_panels.gizmoMode = GizmoMode::Scale;
                if (ImGui::IsKeyPressed(ImGuiKey_X)) {
                    m_panels.gizmoSpace = m_panels.gizmoSpace == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
                }
            }
        } else if (m_panels.mode == EditorMode::Play && ImGui::IsKeyPressed(ImGuiKey_F5)) {
            requests.stop = true;
        }

        if (m_panels.mode == EditorMode::Play) {
            // The identical frame the runtime runs. Escape opens the M13
            // pause menu, which releases the mouse for the editor panels.
            m_play->Frame(window, renderer, deltaSeconds, /*drawHud=*/true);
            m_panels.playPaused = m_play->IsPaused();
            if (m_play->QuitRequested()) requests.stop = true;
            const GameSession& session = m_play->Session();
            const RuntimeWorld::LifecycleCounts counts = m_world->CountLifecycle();
            char info[200];
            std::snprintf(info, sizeof(info), "entities %zu full / %zu coarse / %zu dormant / %zu destroyed | %zu physics bodies | %zu particles | %s",
                          counts.full, counts.coarse, counts.dormant, counts.destroyed, counts.physicsBodies,
                          m_world->HasFluid() ? m_world->Fluid().Particles().size() : std::size_t{0},
                          session.IsPiloting() ? "piloting" : session.Player().IsGrounded() ? "grounded" : "airborne");
            m_panels.runtimeInfo = info;
        } else {
            FrameEditMode(deltaSeconds);
        }
        CollectProfilerData(deltaSeconds);

        DrawEditorMainMenu(m_document, m_panels, requests);
        // While playing, the engine's own HUD occupies the top-left corner
        // and the authored panels are read-only anyway; only the menu bar
        // (Stop), the inspector, profiler and the status bar stay up.
        // The hierarchy is also shown while Play is paused (Escape), with
        // each entity's live fidelity, so the M29 state can be inspected.
        if (m_panels.mode == EditorMode::Edit || m_panels.playPaused) {
            DrawHierarchyPanel(m_document, m_panels, requests);
        }
        if (m_panels.mode == EditorMode::Edit) {
            DrawSceneSettingsPanel(m_document, m_panels);
            DrawAssetBrowserPanel(m_document, m_panels, requests);
            DrawProjectSettingsPanel(m_document, m_panels, requests);
        }
        DrawInspectorPanel(m_document, m_panels);
        DrawProfilerPanel(m_panels);
        DrawStatusBar(m_document, m_panels);

        // Requests raised by the automation hook on the previous frame,
        // after that frame's UI had already been rendered.
        if (deferredRequests.play) requests.play = true;
        if (deferredRequests.stop) requests.stop = true;
        deferredRequests = EditorRequests{};
        HandleRequests(requests);

        ImGui::Render();
        // The 3D frame already sits in the default framebuffer; the UI
        // composites over it through ImGui's own GL backend.
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (autotest) {
            ++autotestFrame;
            const std::string prefix = autotest;
            if (autotestFrame == 5) {
                if (!m_document.GetScene().Objects().empty()) m_document.Select(m_document.GetScene().Objects().front().id);
                DebugViewOptions all;
                all.collisionShapes = all.playerCapsule = all.contacts = all.gravity = all.frameAxes = all.lights =
                    all.interactionRanges = all.lifecycle = all.terrainNormals = all.fluidParticles = all.atmosphere = true;
                m_panels.debug = all;
                m_panels.showProfiler = true;
            } else if (autotestFrame == 10) {
                // Duplicate + undo must leave the scene exactly as authored.
                const SceneObjectId selected = m_document.Selected();
                if (selected != kInvalidSceneObjectId) {
                    const SceneObjectId copy = DuplicateObject(m_document, selected);
                    std::string afterDuplicate;
                    SaveSceneToString(m_document.GetScene(), afterDuplicate);
                    m_document.Undo();
                    std::string afterUndo;
                    SaveSceneToString(m_document.GetScene(), afterUndo);
                    std::fprintf(stderr, "[editor autotest] duplicate created id %llu: scene %s; undo restores: %s\n",
                                 static_cast<unsigned long long>(copy), afterDuplicate != autotestBaseline ? "CHANGED" : "unchanged(!)",
                                 afterUndo == autotestBaseline ? "IDENTICAL" : "DIFFERENT");
                    m_document.Select(selected);
                }
            } else if (autotestFrame == 15 && std::getenv("JUDAS_EDITOR_AUTOTEST_RUN")) {
                // Launch the runtime on the project exactly as Run project
                // does; with JUDAS_TEST_SCRIPT inherited, the child runs the
                // harness on the startup scene and exits by itself.
                std::string message;
                const bool launched = RunProject(message);
                std::fprintf(stderr, "[editor autotest] run project: %s: %s\n", launched ? "LAUNCHED" : "FAILED", message.c_str());
            } else if (autotestFrame == 20) {
                screenshot(prefix + ".edit.png");
                std::fprintf(stderr, "[editor autotest] edit frame: %u draw calls, %u triangles, %u debug lines\n",
                             m_panels.profiler.drawCalls, m_panels.profiler.triangles, m_panels.profiler.debugLines);
                deferredRequests.play = true;
            } else if (autotestFrame == 140) {
                screenshot(prefix + ".play.png");
                const ProfilerData& p = m_panels.profiler;
                std::fprintf(stderr, "[editor autotest] play frame: %d steps, step %.3f ms, %zu bodies, %zu contacts, %u draw calls, %u triangles, %u shadow passes, %u lights, %u debug lines, %zu particles\n",
                             p.fixedStepsThisFrame, p.fixedStepMilliseconds, p.physicsBodies, p.contacts, p.drawCalls, p.triangles,
                             p.shadowPasses, p.dynamicLights, p.debugLines, p.fluidParticles);
                std::fprintf(stderr, "[editor autotest] resources: %zu meshes, %zu textures, %zu terrain meshes, hits %llu, misses %llu, failed %zu, uploads %llu, resident %llu bytes\n",
                             p.resources.loadedMeshes, p.resources.loadedTextures, p.resources.loadedTerrainMeshes,
                             p.resources.hits, p.resources.misses, p.resources.failed, p.resources.uploads,
                             static_cast<unsigned long long>(p.resources.bytesResident));
                std::fprintf(stderr, "[editor autotest] jobs: %u workers, %llu completed, %llu failed, %llu cancelled\n",
                             p.jobs.workers, p.jobs.completed, p.jobs.failed, p.jobs.cancelled);
                deferredRequests.stop = true;
            } else if (autotestFrame == 150) {
                std::string after;
                SaveSceneToString(m_document.GetScene(), after);
                std::fprintf(stderr, "[editor autotest] authored scene after play/stop is %s\n",
                             after == autotestBaseline ? "IDENTICAL" : "DIFFERENT");
                std::string e;
                const bool saved = SaveSceneToFile(m_document.GetScene(), prefix + ".saved.judas", e);
                std::fprintf(stderr, "[editor autotest] save %s\n", saved ? "ok" : e.c_str());
                m_quit = true;
            }
        }
        window.SwapBuffers();
    }

    if (m_panels.mode == EditorMode::Play) StopPlay();
    for (const std::string& id : m_heldAssets) host.Resources().ReleaseRef(id);
    m_heldAssets.clear();
    window.SetEventHook(nullptr);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_host = nullptr;
    return 0;
}
