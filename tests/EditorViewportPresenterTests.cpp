#include "EngineHost.h"
#include "InteractivePlay.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "ScreenshotWriter.h"
#include "WorldCoordinates.h"
#include "WorldPresentation.h"
#include "editor/EditorContext.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanels.h"
#include "editor/EditorPlaySession.h"
#include "editor/EditorSceneViewport.h"
#include "editor/EditorViewportPresenter.h"
#include "glad/gl.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool condition, const char *label) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    std::fflush(stdout);
}
void Key(Window &window, bool pressed) {
    SDL_Event event{};
    event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.windowID = SDL_GetWindowID(window.InputWindow());
    event.key.keysym.scancode = SDL_SCANCODE_K;
    event.key.keysym.sym = SDLK_k;
    event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    SDL_PushEvent(&event);
}
void Focus(Window &window) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = SDL_GetWindowID(window.InputWindow());
    event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    SDL_PushEvent(&event);
}

bool Pixel(const std::vector<unsigned char> &pixels, int width, int height, int x, int y, unsigned red,
           unsigned green, unsigned blue) {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return false;
    const auto offset = (std::size_t(y) * width + x) * 3;
    return offset + 2 < pixels.size() && std::abs(int(pixels[offset]) - int(red)) <= 5 &&
           std::abs(int(pixels[offset + 1]) - int(green)) <= 5 &&
           std::abs(int(pixels[offset + 2]) - int(blue)) <= 5;
}

// This is the normal native platform-window render sequence, with observation
// inserted before swap. It proves shared textures in actual secondary contexts.
void RenderSecondaryWindows(const EditorViewSurface &scene, const EditorViewSurface &game, Renderer &renderer,
                            bool blueGame, bool &scenePixels, bool &gamePixels) {
    auto &platform = ImGui::GetPlatformIO();
    for (int i = 1; i < platform.Viewports.Size; ++i) {
        auto *viewport = platform.Viewports[i];
        if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
            continue;
        if (platform.Platform_RenderWindow)
            platform.Platform_RenderWindow(viewport, nullptr);
        if (platform.Renderer_RenderWindow)
            platform.Renderer_RenderWindow(viewport, nullptr);
        const auto id = Uint32(reinterpret_cast<intptr_t>(viewport->PlatformHandle));
        auto *native = SDL_GetWindowFromID(id);
        if (!native)
            continue;
        int width = 0, height = 0;
        SDL_GL_GetDrawableSize(native, &width, &height);
        std::vector<unsigned char> pixels;
        renderer.CaptureFrame(width, height, pixels);
        const auto inspect = [&](const EditorViewSurface &surface, unsigned r, unsigned g, unsigned b) {
            if (surface.windowId != id || viewport->Size.x <= 0 || viewport->Size.y <= 0)
                return false;
            const auto center = surface.bounds.position + surface.bounds.size * .5f;
            const int x = int((center.x - viewport->Pos.x) * width / viewport->Size.x);
            const int y = int((center.y - viewport->Pos.y) * height / viewport->Size.y);
            return Pixel(pixels, width, height, x, y, r, g, b);
        };
        scenePixels |= inspect(scene, 255, 0, 0);
        gamePixels |= inspect(game, 0, blueGame ? 0 : 255, blueGame ? 255 : 0);
    }
    for (int i = 1; i < platform.Viewports.Size; ++i) {
        auto *viewport = platform.Viewports[i];
        if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
            continue;
        if (platform.Platform_SwapBuffers)
            platform.Platform_SwapBuffers(viewport, nullptr);
        if (platform.Renderer_SwapBuffers)
            platform.Renderer_SwapBuffers(viewport, nullptr);
    }
}
} // namespace

int main(int argc, char **argv) try {
    bool detached = false;
    std::filesystem::path output = "build/editor-v2-presenter";
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--detached")
            detached = true;
        else
            output = argv[i];
    std::filesystem::create_directories(output);
    EngineHost host;
    std::string error;
    Check(host.Init("Judas Editor V2 viewport qualification", 1000, 700, detached, error),
          "real native graphics host starts");
    if (failures) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const std::string driver = SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown";
    std::printf("VIDEO_DRIVER %s\n", driver.c_str());
    if (detached && driver != "x11" && driver != "windows") {
        std::puts("UNAVAILABLE detached native windows require the qualified desktop backend");
        return 77;
    }
    auto &window = host.GetWindow();
    auto &renderer = host.GetRenderer();
    SDL_SetWindowPosition(window.NativeWindow(), 120, 80);
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (detached)
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui_ImplSDL2_InitForOpenGL(window.NativeWindow(), window.NativeGLContext());
    ImGui_ImplOpenGL3_Init("#version 330 core");
    window.SetEventHook([](const SDL_Event &event) { ImGui_ImplSDL2_ProcessEvent(&event); });
    EditorDocument document;
    Project project;
    EditorPanelState panels;

    EditorRequests requests;
    EditorSceneViewport scene;
    EditorPlaySession session;
    session.world = std::make_unique<RuntimeWorld>();
    Scene physicalScene;
    auto &object = physicalScene.CreateObject("Actual physical owner");
    object.body = SceneBodyComponent{};
    object.body->motion = SceneBodyMotion::Dynamic;
    object.transform.position = {0, 4, 0};
    object.render = SceneRenderComponent{};
    object.render->color = {1, 0, 0};
    auto &authoredCamera = physicalScene.CreateObject("Authored secondary camera");
    authoredCamera.renderCamera = SceneRenderCameraComponent{};
    authoredCamera.renderCamera->width = 64;
    authoredCamera.renderCamera->height = 64;
    authoredCamera.renderCamera->updateEveryFrames = 2;
    Check(session.world->Build(physicalScene, &host.Resources(), error), "actual physical scene builds");
    if (failures) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    session.play = std::make_unique<InteractivePlay>();
    Check(session.play->Begin(*session.world, WorldCoordinates{}, error),
          "ordinary authoritative Play session begins for observer qualification");
    if (failures) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const auto body = session.world->DynamicBodies().front().Handle();
    session.world->Physics().SetLinearVelocity(body, {1, 0, 0});
    const auto original = session.world->Physics().GetTransform(body);
    EditorContext context{document, project, panels, requests, host, host.Assets(), session.world.get()};
    EditorViewportPresenter views;
    bool blueGame = false;
    glm::vec2 sceneSize{360, 290}, gameSize{360, 290};
    bool outsidePointer = false, detachedScene = false;
    std::optional<glm::vec2> pointerOverride;
    std::vector<std::pair<int, bool>> pointerButtons;
    int focusFixture = 0; // 1: real text edit, 2: real slider drag.
    bool fixtureActive = false;
    char fixtureText[64] = "Do not steal this edit";
    float fixtureValue = .5f;
    glm::vec2 fixtureControl{0};
    bool dragAsset = false;
    const std::string dragAssetId = "0123456789abcdef0123456789abcdef";
    auto frame = [&](bool detachNow = false) {
        window.PollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        // Inject after SDL's real NewFrame observation so the host pointer
        // cannot overwrite these deliberate cross-window focus interactions.
        if (pointerOverride)
            io.AddMousePosEvent(pointerOverride->x, pointerOverride->y);
        else if (outsidePointer)
            io.AddMousePosEvent(-1000, -1000);
        for (const auto &event : pointerButtons)
            io.AddMouseButtonEvent(event.first, event.second);
        pointerButtons.clear();
        ImGui::NewFrame();
        views.BeginFrame();
        const auto origin = ImGui::GetMainViewport()->Pos;
        if (focusFixture) {
            ImGui::SetNextWindowPos({origin.x + 20, origin.y + 400}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({360, 130}, ImGuiCond_Always);
            ImGui::Begin("Input ownership fixture");
            if (focusFixture == 1)
                ImGui::InputText("Editing", fixtureText, sizeof(fixtureText));
            else
                ImGui::SliderFloat("Dragging", &fixtureValue, 0, 1);
            fixtureActive = ImGui::IsItemActive();
            const auto control = ImGui::GetItemRectMin();
            // Stay inside the actual control, before its text label.
            fixtureControl = {control.x + 70, control.y + ImGui::GetItemRectSize().y * .5f};
            ImGui::End();
        }
        if (dragAsset && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern |
                                                    ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
            ImGui::SetDragDropPayload(kAssetDragPayload, dragAssetId.data(), dragAssetId.size());
            ImGui::EndDragDropSource();
        }
        if (panels.showSceneView) {
            ImGui::SetNextWindowPos(
                {origin.x + (detachedScene ? 1060 : 20), origin.y + (detachedScene ? 410 : 40)},
                ImGuiCond_Always);
            ImGui::SetNextWindowSize({sceneSize.x, sceneSize.y}, ImGuiCond_Always);
            if (detachNow)
                ImGui::SetNextWindowViewport(0);
            views.DrawScene(context, scene);
        }
        if (panels.showGameView) {
            ImGui::SetNextWindowPos({origin.x + (detachNow ? 1060 : 420), origin.y + 40}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({gameSize.x, gameSize.y}, ImGuiCond_Always);
            if (detachNow)
                ImGui::SetNextWindowViewport(0);
            views.DrawGame(context, session);
        }
        if (scene.target.IsValid() && renderer.BeginRenderTarget(scene.target)) {
            glDisable(GL_SCISSOR_TEST);
            glClearColor(1, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.EndRenderTarget();
        }
        if (views.BeginGameTarget(renderer)) {
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            Check(viewport[2] < window.Width() && viewport[3] < window.Height(),
                  "Game uses its content target rather than the whole main framebuffer");
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0, blueGame ? 0.f : 1.f, blueGame ? 1.f : 0.f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            views.EndGameTarget(renderer);
        }
        ImGui::Render();
        SDL_GL_MakeCurrent(window.NativeWindow(), window.NativeGLContext());
        renderer.BeginFrame(window.Width(), window.Height());
        renderer.EndFrame();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        int width = 0, height = 0;
        SDL_GL_GetDrawableSize(window.NativeWindow(), &width, &height);
        std::vector<unsigned char> pixels;
        renderer.CaptureFrame(width, height, pixels);
        const auto inspectMain = [&](const EditorViewSurface &surface, unsigned r, unsigned g, unsigned b) {
            if (!surface.visible || surface.windowId != SDL_GetWindowID(window.NativeWindow()))
                return false;
            const auto point = surface.bounds.position + surface.bounds.size * .5f;
            return Pixel(pixels, width, height, int((point.x - origin.x) * io.DisplayFramebufferScale.x),
                         int((point.y - origin.y) * io.DisplayFramebufferScale.y), r, g, b);
        };
        bool scenePixels = inspectMain(views.Scene(), 255, 0, 0);
        bool gamePixels = inspectMain(views.Game(), 0, blueGame ? 0 : 255, blueGame ? 255 : 0);
        if (detached) {
            ImGui::UpdatePlatformWindows();
            RenderSecondaryWindows(views.Scene(), views.Game(), renderer, blueGame, scenePixels, gamePixels);
            SDL_GL_MakeCurrent(window.NativeWindow(), window.NativeGLContext());
        }
        window.SwapBuffers();
        return std::pair<bool, bool>{scenePixels, gamePixels};
    };
    frame();
    const auto first = frame();
    Check(views.Scene().visible && views.Game().visible && views.Scene().bounds.size.x > 0 &&
              views.Game().bounds.size.x > 0 && views.Scene().bounds.position != views.Game().bounds.position,
          "Scene and Game expose independent real content rectangles");
    Check(first.first && first.second, "actual Scene and Game images sample their separate native targets");
    blueGame = true;
    const auto changed = frame();
    Check(changed.first && changed.second,
          "newly rendered Game pixels replace the previous frame without stale texture reuse");
    gameSize = {290, 350};
    const auto resized = frame();
    views.ConfigureInput(window, true);
    Check(resized.second && window.InputWidth() == int(views.Game().bounds.size.x) &&
              window.InputHeight() == int(views.Game().bounds.size.y),
          "resizing Game updates render target and actual gameplay dimensions");
    int x =
        int(views.Game().bounds.position.x - views.Game().nativeOrigin.x + views.Game().bounds.size.x * .5f);
    int y =
        int(views.Game().bounds.position.y - views.Game().nativeOrigin.y + views.Game().bounds.size.y * .5f);
    window.MapInputPosition(x, y);
    Check(std::abs(x - window.InputWidth() / 2) <= 1 && std::abs(y - window.InputHeight() / 2) <= 1,
          "offset image center maps to gameplay center instead of window origin");
    Check(renderer.RenderTargetSize(scene.target) == glm::ivec2(views.Scene().bounds.size),
          "Scene target follows its own content size");
    // Use the real ImGui payload/target lifecycle. A broken or absent drop
    // target must fail the positive control; controls overlaying that same
    // image must reject delivery even when the control is disabled.
    auto dropAt = [&](glm::vec2 point) {
        scene.pendingDropDelivered = false;
        scene.pendingDropAsset.clear();
        pointerOverride = point;
        frame();
        dragAsset = true;
        pointerButtons.emplace_back(ImGuiMouseButton_Left, true);
        frame();
        frame();
        pointerButtons.emplace_back(ImGuiMouseButton_Left, false);
        frame();
        dragAsset = false;
        const bool delivered = scene.pendingDropDelivered;
        frame();
        return delivered;
    };
    const auto ordinaryDrop = views.Scene().bounds.position + views.Scene().bounds.size * .5f;
    Check(dropAt(ordinaryDrop) && scene.pendingDropAsset == dragAssetId,
          "real image drop target delivers an ordinary asset payload");
    Check(!dropAt(views.Scene().bounds.position + glm::vec2(24, 24)),
          "dropping onto the projection overlay never publishes a world-placement request");
    panels.showGameView = false;
    sceneSize = {460, 290};
    frame();
    frame();
    const float scale = io.FontGlobalScale;
    const float overlaysWidth = ImGui::CalcTextSize("Overlays").x + ImGui::GetFontSize() +
                                ImGui::GetStyle().ItemInnerSpacing.x + 18 * scale;
    const auto disabledFrameButton =
        views.Scene().bounds.position + glm::vec2(views.Scene().bounds.size.x - 185 * scale + overlaysWidth +
                                                      ImGui::GetStyle().ItemSpacing.x + 8 * scale,
                                                  20 * scale);
    Check(!dropAt(disabledFrameButton),
          "disabled Frame overlay still rejects an asset drop instead of creating an object");
    panels.showGameView = true;
    sceneSize = {360, 290};
    pointerOverride.reset();
    scene.pendingDropDelivered = false;
    scene.pendingDropAsset.clear();
    frame();
    frame();
    // Right/middle camera gestures must focus their actual image without a
    // preceding left click. Moving over Scene alone must leave Game focused.
    const auto sceneCenter = views.Scene().bounds.position + views.Scene().bounds.size * .5f;
    views.RequestGameFocus();
    frame();
    pointerOverride = sceneCenter;
    frame();
    Check(views.Game().focused && !views.Scene().focused && views.Scene().hovered,
          "moving over Scene does not steal the focused Game window");
    pointerButtons.emplace_back(ImGuiMouseButton_Middle, true);
    frame();
    Check(views.Scene().focused && !views.Game().focused,
          "fresh middle press on the Scene image focuses its camera from Game");
    pointerButtons.emplace_back(ImGuiMouseButton_Middle, false);
    frame();
    views.RequestGameFocus();
    frame();
    pointerButtons.emplace_back(ImGuiMouseButton_Right, true);
    frame();
    Check(views.Scene().focused && !views.Game().focused,
          "fresh right press on the Scene image focuses its camera from Game");
    pointerButtons.emplace_back(ImGuiMouseButton_Right, false);
    frame();

    focusFixture = 1;
    frame();
    pointerOverride = fixtureControl;
    frame();
    pointerButtons.emplace_back(ImGuiMouseButton_Left, true);
    frame();
    pointerButtons.emplace_back(ImGuiMouseButton_Left, false);
    frame();
    // WantTextInput is published by the frame lifecycle after activation;
    // keyboard-nav requests alone do not prove an editable field owns it.
    frame();
    Check(fixtureActive && io.WantTextInput && !views.Scene().focused,
          "negative fixture owns an actual text edit outside Scene");
    pointerOverride = sceneCenter;
    frame();
    pointerButtons.emplace_back(ImGuiMouseButton_Right, true);
    frame();
    Check(fixtureActive && !views.Scene().focused, "right press over Scene cannot steal an active text edit");
    pointerButtons.emplace_back(ImGuiMouseButton_Right, false);
    frame();
    focusFixture = 0;
    views.RequestGameFocus();
    frame();
    frame();

    focusFixture = 2;
    frame();
    pointerOverride = fixtureControl;
    frame();
    pointerButtons.emplace_back(ImGuiMouseButton_Left, true);
    frame();
    Check(fixtureActive && !views.Scene().focused,
          "negative fixture owns an actual slider drag outside Scene");
    pointerOverride = sceneCenter;
    pointerButtons.emplace_back(ImGuiMouseButton_Middle, true);
    frame();
    Check(fixtureActive && !views.Scene().focused,
          "middle press over Scene cannot steal another widget's active drag");
    pointerButtons.emplace_back(ImGuiMouseButton_Middle, false);
    pointerButtons.emplace_back(ImGuiMouseButton_Left, false);
    frame();
    focusFixture = 0;
    pointerOverride.reset();
    outsidePointer = true;
    views.RequestGameFocus();
    frame();
    frame();
    views.ConfigureInput(window, true);
    Check(views.Game().focused && !views.Game().hovered,
          "fixture separates keyboard focus from pointer hover");
    InputMap map;
    map.Add("keyboard-probe", false);
    map.AddBinding("keyboard-probe", {"key:K"});
    Check(window.Input().SetMap(map, error), "game-specific input map installs through ordinary API");
    Focus(window);
    Key(window, true);
    window.PollEvents();
    Check(window.Input().Action("keyboard-probe").held,
          "focused Game accepts keyboard input while pointer is outside its image");
    Key(window, false);
    window.PollEvents();
    panels.showGameView = false;
    frame();
    views.ConfigureInput(window, true);
    Check(!views.Game().visible && !views.BeginGameTarget(renderer) && !window.IsMouseCaptured(),
          "hidden Game releases capture and performs no target presentation");
    Check(session.world->Physics().GetTransform(body).position == original.position &&
              session.world->Physics().GetLinearVelocity(body) == glm::vec3(1, 0, 0),
          "drawing, resizing, focus and visibility never advance or replace physical state");
    scene.camera.LookAt({0, 4, 0}, 8);
    session.lastAlpha = 1;
    session.world->view = RuntimeWorld::RuntimeView{};
    session.world->view->pose.position = {200, 200, 200};
    const auto expectedView = scene.camera.ViewMatrix();
    const auto beforeObserve = session.world->Physics().GetTransform(body);
    const auto targetSize = renderer.RenderTargetSize(scene.target);
    const auto projection = scene.camera.ProjectionMatrix(float(targetSize.x) / targetSize.y);
    // Capture must resume without clearing. A normal BeginRenderTarget starts a
    // fresh pass, which would erase the result and give a misleading oracle.
    auto targetPixels = [&](RenderTargetHandle target) {
        std::vector<unsigned char> result;
        const auto size = renderer.RenderTargetSize(target);
        if (renderer.BeginRenderTarget(target, true)) {
            renderer.CaptureFrame(size.x, size.y, result);
            renderer.EndRenderTarget();
        }
        return result;
    };
    auto runtimeFrame = [&] {
        RenderWorldFrame(renderer, targetSize.x, targetSize.y, *session.world,
                         expectedView, projection, scene.camera.Position(), 1, 1.f / 60,
                         WorldRenderPurpose::Runtime, &scene.target);
    };
    runtimeFrame();
    Check(session.world->PresentationCameras().size() == 1 &&
              session.world->PresentationCameras().front().updates == 1 &&
              session.world->PresentationCameras().front().error.empty(),
          "ordinary runtime presentation updates the actual authored camera");
    const auto runtimePixels = targetPixels(scene.target);
    // Independent known-camera render: inspect the image produced by the
    // observer entry point, not renderer state restored when its pass ends.
    RenderWorldFrame(renderer, targetSize.x, targetSize.y, *session.world,
                     expectedView, projection, scene.camera.Position(), 1, 1.f / 60,
                     WorldRenderPurpose::Observer, &scene.target);
    const auto expectedObserverPixels = targetPixels(scene.target);
    bool observerCamera = !expectedObserverPixels.empty(), observerCounts = true;
    for (int i = 0; i < 3; ++i) {
        scene.RenderLive(context, session, 1.f / 60);
        observerCamera &= targetPixels(scene.target) == expectedObserverPixels;
        observerCounts &= session.world->PresentationCameras().front().updates == 1;
    }
    Check(observerCamera,
          "live Scene pixels match an independent observer render despite a conflicting gameplay view");
    Check(observerCounts, "observer rendering never updates authored camera targets");
    Check(!runtimePixels.empty() && expectedObserverPixels != runtimePixels,
          "actual live Scene pixels differ from the remote gameplay camera instead of a stale frame");
    const auto debugLinesBefore = renderer.Stats().debugLines;
    panels.debug.collisionShapes = true;
    scene.RenderLive(context, session, 1.f / 60);
    const auto debugPixels = targetPixels(scene.target);
    Check(renderer.Stats().debugLines > debugLinesBefore && debugPixels != expectedObserverPixels,
          "Scene observer overlays draw actual authoritative collision geometry in their own target");
    panels.debug.collisionShapes = false;
    scene.RenderLive(context, session, 1.f / 60);
    Check(targetPixels(scene.target) == expectedObserverPixels &&
              session.world->PresentationCameras().front().updates == 1 &&
              session.world->Physics().GetTransform(body).position == beforeObserve.position,
          "disabling observer overlays restores pixels without advancing cameras or physical state");
    runtimeFrame();
    Check(session.world->PresentationCameras().front().updates == 1,
          "three observer passes do not advance the runtime camera cadence");
    runtimeFrame();
    Check(session.world->PresentationCameras().front().updates == 2,
          "next ordinary runtime cadence boundary still updates its camera");

    // Exercise the other multi-target path in a real Game destination: two
    // independent local-player cameras must render before the final composite.
    auto &players = session.world->Players();
    auto *nearPlayer = players.Create("Near camera", map, error);
    auto *farPlayer = players.Create("Remote camera", map, error);
    Check(nearPlayer && farPlayer, "two actual local-player identities are authored");
    if (nearPlayer && farPlayer) {
        SceneTransform nearPose, farPose;
        nearPose.position = scene.camera.Position();
        nearPose.rotation = glm::quat_cast(glm::inverse(expectedView));
        farPose.position = {200, 200, 200};
        Check(players.SetViewport(nearPlayer->id, {0, 0, .5f, 1}, error) &&
                  players.SetViewport(farPlayer->id, {.5f, 0, .5f, 1}, error) &&
                  players.SetView(nearPlayer->id, nearPose, 60, .1f, 500, error) &&
                  players.SetView(farPlayer->id, farPose, 60, .1f, 500, error),
              "independent split-screen camera views use the ordinary player API");
        runtimeFrame();
        const auto composite = targetPixels(scene.target);
        const auto nearPixels = targetPixels(nearPlayer->target);
        const auto farPixels = targetPixels(farPlayer->target);
        const auto nearSize = renderer.RenderTargetSize(nearPlayer->target);
        const auto farSize = renderer.RenderTargetSize(farPlayer->target);
        auto sample = [](const std::vector<unsigned char> &pixels, int width, int height, int x, int y) {
            glm::ivec3 result{-999};
            if (x >= 0 && y >= 0 && x < width && y < height &&
                pixels.size() >= std::size_t(width) * height * 3) {
                const auto offset = (std::size_t(y) * width + x) * 3;
                result = {pixels[offset], pixels[offset + 1], pixels[offset + 2]};
            }
            return result;
        };
        const auto left = sample(composite, targetSize.x, targetSize.y, targetSize.x / 4, targetSize.y / 2);
        const auto right =
            sample(composite, targetSize.x, targetSize.y, targetSize.x * 3 / 4, targetSize.y / 2);
        const auto nearCenter = sample(nearPixels, nearSize.x, nearSize.y, nearSize.x / 2, nearSize.y / 2);
        const auto farCenter = sample(farPixels, farSize.x, farSize.y, farSize.x / 2, farSize.y / 2);
        const auto closePixel = [](glm::ivec3 a, glm::ivec3 b) {
            return a.x >= 0 && b.x >= 0 && glm::all(glm::lessThanEqual(glm::abs(a - b), glm::ivec3(5)));
        };
        Check(nearPlayer->renderError.empty() && farPlayer->renderError.empty() &&
                  closePixel(left, nearCenter) && closePixel(right, farCenter) && !closePixel(left, right),
              "actual Game destination composes both distinct local-player images without nested passes");
        players.Remove(nearPlayer->id);
        players.Remove(farPlayer->id);
    }
    Check(session.world->Physics().GetTransform(body).position == beforeObserve.position &&
              session.world->Physics().GetLinearVelocity(body) == glm::vec3(1, 0, 0),
          "live observer and secondary camera rendering preserve actual physical state");
    // The observer owns a chart-relative camera, independently of the world's
    // physics. A genuine origin transaction must translate both exactly once.
    const auto authoredOrigin = document.GetScene().Settings().worldOrigin;
    const auto initialOrigin = session.world->Settings().worldOrigin;
    const auto observerPosition = scene.camera.Position();
    const auto observerFocus = panels.cameraFocus;
    const auto absoluteEye = initialOrigin + glm::dvec3(observerPosition);
    const auto absoluteBody =
        initialOrigin + glm::dvec3(session.world->Physics().GetTransform(body).position);
    const auto linearVelocity = session.world->Physics().GetLinearVelocity(body);
    const auto angularVelocity = session.world->Physics().GetAngularVelocity(body);
    const glm::dvec3 originDelta{512, -512, 512};
    const auto nearAbsolute = [](glm::dvec3 a, glm::dvec3 b) { return glm::length(a - b) < 2e-4; };
    Check(session.world->RequestOrigin(initialOrigin + originDelta, error) && session.world->CommitOrigin(),
          "actual runtime commits a three-axis origin change during observer Play");
    scene.RenderLive(context, session, 1.f / 60);
    Check(nearAbsolute(session.world->Settings().worldOrigin + glm::dvec3(scene.camera.Position()),
                       absoluteEye) &&
              nearAbsolute(session.world->Settings().worldOrigin +
                               glm::dvec3(session.world->Physics().GetTransform(body).position),
                           absoluteBody),
          "live observer eye and actual rigid body retain their independent absolute positions");
    Check(nearAbsolute(session.world->Settings().worldOrigin + glm::dvec3(panels.cameraFocus),
                       initialOrigin + glm::dvec3(observerFocus)) &&
              session.world->Physics().GetLinearVelocity(body) == linearVelocity &&
              session.world->Physics().GetAngularVelocity(body) == angularVelocity,
          "observer focus follows its chart without changing physical velocities");
    Check(session.world->RequestOrigin(initialOrigin, error) && session.world->CommitOrigin(),
          "actual runtime accepts its return origin");
    scene.RenderLive(context, session, 1.f / 60);
    Check(nearAbsolute(glm::dvec3(scene.camera.Position()), glm::dvec3(observerPosition)),
          "return origin restores the observer's local camera frame");
    Check(session.world->RequestOrigin(initialOrigin + originDelta, error) && session.world->CommitOrigin(),
          "second real origin shift prepares Stop restoration");
    scene.RenderLive(context, session, 1.f / 60);
    scene.EndObservation(context);
    Check(nearAbsolute(authoredOrigin + glm::dvec3(scene.camera.Position()), absoluteEye) &&
              nearAbsolute(authoredOrigin + glm::dvec3(panels.cameraFocus),
                           initialOrigin + glm::dvec3(observerFocus)),
          "ending observation restores camera and focus to the authored document chart");
    Check(session.world->RequestOrigin(initialOrigin, error) && session.world->CommitOrigin(),
          "fixture restores the original physical chart after Stop-like observer closure");
    EditorCamera planarCamera;
    planarCamera.SetPose({3, 5, 7}, 20, -15);
    const auto savedPerspective = planarCamera.Position();
    planarCamera.Set2D(true);
    const auto planarPosition = planarCamera.Position();
    planarCamera.ShiftOrigin(originDelta);
    Check(nearAbsolute(glm::dvec3(planarCamera.Position()) + originDelta, glm::dvec3(planarPosition)),
          "2D observer position translates in the same validated chart");
    planarCamera.Set2D(false);
    Check(nearAbsolute(glm::dvec3(planarCamera.Position()) + originDelta, glm::dvec3(savedPerspective)),
          "leaving 2D restores the translated saved perspective position");
    session.world->Physics().Step(1.f / 60);
    Check(session.world->Physics().GetTransform(body).position != original.position,
          "explicit physics step changes the same body, validating the no-step observer");
    if (detached) {
        panels.showGameView = true;
        outsidePointer = false;
        frame(true);
        frame(true);
        const auto native = frame(true);
        Check(views.Game().windowId && views.Game().windowId != SDL_GetWindowID(window.NativeWindow()) &&
                  SDL_GetWindowFromID(views.Game().windowId),
              "Game is hosted in an actual secondary SDL OS window");
        Check(native.second, "secondary OpenGL context displays the main-context Game texture");
        views.ConfigureInput(window, true);
        Check(SDL_GetWindowID(window.InputWindow()) == views.Game().windowId,
              "detached Game input belongs to its native window");
        Focus(window);
        Key(window, true);
        window.PollEvents();
        Key(window, false);
        window.PollEvents();
        Check(window.Width() == 1000 && window.Height() == 700 && !window.ShouldClose(),
              "secondary view does not mutate the main native window lifecycle");
        detachedScene = true;
        frame(true);
        frame(true);
        const auto detachedViews = frame(true);
        Check(views.Scene().windowId && views.Scene().windowId != views.Game().windowId &&
                  views.Scene().windowId != SDL_GetWindowID(window.NativeWindow()) && detachedViews.first,
              "detached Scene owns another SDL window and displays its shared Scene texture");
        views.ConfigureInput(window, false);
        Check(SDL_GetWindowID(window.InputWindow()) == views.Scene().windowId,
              "detached Scene camera capture is routed to its own native window");
        SDL_Event close{};
        close.type = SDL_WINDOWEVENT;
        close.window.windowID = views.Scene().windowId;
        close.window.event = SDL_WINDOWEVENT_CLOSE;
        SDL_PushEvent(&close);
        frame(true);
        frame(true);
        Check(!window.ShouldClose() && !panels.showSceneView,
              "closing a native Scene tool leaves the main editor alive");
    }
    window.ClearInputRegion();
    views.Destroy(renderer);
    if (scene.target.IsValid())
        renderer.DestroyRenderTarget(scene.target);
    session.play->End();
    session.play.reset();
    session.world->Destroy();
    session.world.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    std::printf("SUMMARY %u checks %u failures\n", checks, failures);
    return failures ? 1 : 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "FAIL viewport fixture: %s\n", error.what());
    return 1;
}
