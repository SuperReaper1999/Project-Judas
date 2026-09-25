#include "EditorApplication.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "TerrainLibrary.h"
#include "WorldCoordinates.h"
#include "WorldPresentation.h"
#include "WorldState.h"
#include "ScreenshotWriter.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;

// A loose bounding radius for viewport picking — generous for empties and
// lights so they can still be clicked.
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

void ListAssets(const std::string& directory, const std::vector<std::string>& extensions,
                std::vector<std::string>& out) {
    out.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) return;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string extension = entry.path().extension().string();
        for (const std::string& wanted : extensions) {
            if (extension == wanted) out.push_back(entry.path().generic_string());
        }
    }
    std::sort(out.begin(), out.end());
}
}  // namespace

EditorApplication::EditorApplication() = default;
EditorApplication::~EditorApplication() = default;

void EditorApplication::RefreshAssetLists() {
    ListAssets("assets/models", {".obj"}, m_panels.modelAssets);
    ListAssets("assets/textures", {".png", ".jpg", ".jpeg", ".bmp", ".tga"}, m_panels.textureAssets);
    m_panels.terrainSurfaces = KnownTerrainSurfaces();
}

bool EditorApplication::StartPlay(std::string& outError) {
    m_world = std::make_unique<RuntimeWorld>();
    if (!m_world->Build(m_document.GetScene(), &m_host->Assets(), outError)) {
        m_world.reset();
        return false;
    }
    // Milestone 29: a saved world-state delta for this scene file layers
    // over the freshly instantiated baseline, exactly as the runtime does.
    m_panels.worldStatePath = DefaultWorldStatePath(m_document.Path());
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
    m_panels.runtime = m_world.get();
    m_panels.mode = EditorMode::Play;
    m_panels.status = "Playing: Escape pauses (menu) and frees the mouse; Stop restores the authored scene";
    // Keys pressed while editing (F to focus, R, Space, ...) must not fire
    // as gameplay requests on the first played frame.
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
    if (r.newScene) {
        m_document.NewScene();
        m_panels.status = "New scene";
    }
    if (r.open) ImGui::OpenPopup("Open scene");
    if (r.saveAs) ImGui::OpenPopup("Save scene as");
    if (r.save) {
        if (m_document.Path().empty()) ImGui::OpenPopup("Save scene as");
        else m_panels.status = m_document.Save(error) ? "Saved " + m_document.Path() : "Save failed: " + error;
    }
    if (r.undo) m_document.Undo();
    if (r.redo) m_document.Redo();
    if (!r.createKind.empty()) {
        CreateObjectOfKind(m_document, r.createKind, r.createPosition,
                           m_panels.modelAssets.empty() ? std::string() : m_panels.modelAssets.front());
        m_panels.status = "Created " + r.createKind;
    }
    if (r.focusSelection) {
        if (const SceneObject* o = m_document.SelectedObject()) {
            m_camera.LookAt(o->transform.position, std::max(4.0f, PickRadius(*o) * 3.0f));
        }
    }

    // Path popups (the editor has no OS file dialog; a path field is enough
    // for M28 — see docs/ARCHITECTURE.md, "Milestone 28, Limitations").
    const auto pathPopup = [&](const char* title, const char* button, auto&& action) {
        if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
        char buffer[512];
        std::strncpy(buffer, m_panels.pathInput.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText("Path (.judas)", buffer, sizeof(buffer))) m_panels.pathInput = buffer;
        ImGui::TextDisabled("Relative to the working directory, e.g. assets/scenes/my_scene.judas");
        if (ImGui::Button(button) && !m_panels.pathInput.empty()) {
            action(m_panels.pathInput);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    };
    pathPopup("Open scene", "Open", [&](const std::string& path) {
        std::string e;
        m_panels.status = m_document.Load(path, e) ? "Opened " + path : "Open failed: " + e;
    });
    pathPopup("Save scene as", "Save", [&](const std::string& path) {
        std::string e;
        m_panels.status = m_document.SaveAs(path, e) ? "Saved " + path : "Save failed: " + e;
    });
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

    // Left click in the viewport selects.
    if (!io.WantCaptureMouse && !m_lookActive && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int mx = 0, my = 0;
        window.GetMousePosition(mx, my);
        PickAtPixel(mx, my);
    }

    const Scene& scene = m_document.GetScene();
    const int height = std::max(window.Height(), 1);
    const float aspect = static_cast<float>(window.Width()) / static_cast<float>(height);
    renderer.SetLighting(glm::normalize(scene.Settings().sunDirection), scene.Settings().sunColor,
                         scene.Settings().ambientColor);
    renderer.BeginFrame(window.Width(), window.Height());
    renderer.SetCamera(m_camera.ViewMatrix(), m_camera.ProjectionMatrix(aspect));
    renderer.SetDynamicLights(BuildAuthoredLights(scene));
    DrawAuthoredScene(renderer, scene, m_host->Assets());
    // Selection marker: a translucent shell around the selected object.
    if (const SceneObject* selected = m_document.SelectedObject()) {
        renderer.BeginTransparentPass();
        renderer.DrawSphere(selected->transform.position, PickRadius(*selected) * 1.05f,
                            glm::vec3(1.0f, 0.85f, 0.2f), 0.18f);
        renderer.EndTransparentPass();
    }
    renderer.EndFrame();
}

int EditorApplication::Run(int argc, char** argv) {
    if (argc > 2) {
        std::fprintf(stderr, "usage: judas_editor [scene.judas]\n");
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

    RefreshAssetLists();
    if (argc == 2) {
        m_panels.status = m_document.Load(argv[1], error) ? "Opened " + std::string(argv[1]) : "Open failed: " + error;
        m_panels.pathInput = argv[1];
    } else {
        m_document.NewScene();
        m_panels.status = "New scene. Create > objects; right-drag to look, WASD/QE to fly; click to select.";
    }
    if (const SceneObject* start = m_document.GetScene().Find(1)) {
        m_camera.LookAt(start->transform.position, 40.0f);
    }

    // Developer/automation hook (see docs/ARCHITECTURE.md, "Milestone 28,
    // Automated evidence"): JUDAS_EDITOR_AUTOTEST=<prefix> renders the
    // edit view, enters Play, runs, screenshots both, stops, verifies the
    // authored scene is untouched, saves it and quits.
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
        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        const float deltaSeconds = static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        EditorRequests requests;
        // Keyboard shortcuts (edit mode, no text field focused).
        if (m_panels.mode == EditorMode::Edit && !io.WantCaptureKeyboard) {
            const bool ctrl = io.KeyCtrl;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) requests.save = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) requests.undo = true;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) requests.redo = true;
            if (ImGui::IsKeyPressed(ImGuiKey_F5)) requests.play = true;
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_document.Selected() != kInvalidSceneObjectId) {
                m_document.BeginEdit();
                m_document.GetScene().DestroyObject(m_document.Selected());
                m_document.CommitEdit();
                m_document.Select(kInvalidSceneObjectId);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F) && !m_lookActive) requests.focusSelection = true;
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

        DrawEditorMainMenu(m_document, m_panels, requests);
        // While playing, the engine's own HUD occupies the top-left corner
        // and the authored panels are read-only anyway; only the menu bar
        // (Stop), the inspector and the status bar stay up.
        // The hierarchy is also shown while Play is paused (Escape), with
        // each entity's live fidelity, so the M29 state can be inspected.
        if (m_panels.mode == EditorMode::Edit || m_panels.playPaused) {
            DrawHierarchyPanel(m_document, m_panels, requests);
        }
        if (m_panels.mode == EditorMode::Edit) {
            DrawSceneSettingsPanel(m_document, m_panels);
            DrawAssetPanel(m_document, m_panels);
        }
        DrawInspectorPanel(m_document, m_panels);
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
            if (autotestFrame == 20) {
                screenshot(prefix + ".edit.png");
                deferredRequests.play = true;
            } else if (autotestFrame == 140) {
                screenshot(prefix + ".play.png");
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
    window.SetEventHook(nullptr);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_host = nullptr;
    return 0;
}
