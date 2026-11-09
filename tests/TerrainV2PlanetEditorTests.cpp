#include "EngineHost.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "ScreenshotWriter.h"
#include "editor/TerrainPlanetPreview.h"
#include "editor/TerrainV2Editor.h"
#include "editor/TerrainV2Editor.h"
#include "editor/TerrainV2Inspector.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "terrain/TerrainPlanetBinding.h"
#include <chrono>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <thread>

int main(int argc, char **argv) {
    unsigned checks = 0;
    try {
        auto check = [&](bool ok, const std::string &why) {
            ++checks;
            if (!ok)
                throw std::runtime_error(why);
        };
        namespace fs = std::filesystem;
        const auto root =
            argc > 1
                ? fs::path(argv[1])
                : fs::temp_directory_path() /
                      ("terrain-planet-editor-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(!fs::exists(root), "fresh editor project required");
        Project project;
        std::string error;
        check(Project::CreateNew(root.string(), "Planet editor workflow", project, error), error);
        EngineHost host;
        check(host.Init("Planetary terrain authoring", 256, 256, false, error), error);
        host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
        auto *context = ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = io.LogFilename = nullptr;
        io.DisplaySize = {1000, 1000};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        PlanetSettings planet;
        planet.radius = 1000;
        planet.collisionSpacing = 1;
        TerrainV2::Source known;
        check(TerrainV2::CreatePlanetPatch(planet, {1. / 16, 1, 1. / 16}, 128, known, error),
              error);
        const auto chart = *known.chart;
        EditorDocument document;
        document.NewScene();
        document.GetScene().Settings().worldOrigin = chart.ModelOrigin();
        document.GetScene().CreateObject("Planet").planet = planet;
        document.Select(1);
        EditorPanelState state;
        state.project = &project;
        state.assets = &host.Assets();
        state.resources = &host.Resources();
        state.importJobs = &host.Jobs();
        state.showTerrain = true;
        EditorRequests requests;
        TerrainV2Editor tools;
        auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({800, 950}, ImGuiCond_Always);
            tools.Panel(document, state, requests);
            ImGui::Render();
            tools.Prepare(host.GetRenderer(), state);
        };
        auto click = [&](const char *label) {
            auto *window = ImGui::FindWindowByName("Terrain - sculpt / paint");
            for (const auto &popup : context->OpenPopupStack)
                if (popup.Window && popup.Window->Active)
                    window = popup.Window;
            ImGui::ActivateItemByID(window->GetID(label));
            frame();
        };
        auto combo = [&](const char *label, const char *item, int index) {
            click(label);
            auto *popup = context->OpenPopupStack.back().Window;
            ImGui::ActivateItemByID(ImHashStr(item, 0, popup->GetID(index)));
            frame();
        };
        auto wait = [&] {
            for (unsigned i = 0; i < 10000; ++i) {
                frame();
                if (!tools.Busy())
                    return;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            throw std::runtime_error("planet creation/edit preparation timeout");
        };
        frame();
        frame();
        click("New landscape");
        frame();
        combo("Landscape type", "Planetary patch", 1);
        click("Create landscape");
        wait();
        const auto id = document.Selected();
        const auto *owner = document.GetScene().Find(id);
        check(id != 1 && owner && owner->landscape && owner->landscape->planet == 1,
              "normal project UI creates a geographic native owner: " + state.status);
        check(tools.Loaded() && tools.Preview() && tools.Sources().size() == 1,
              "new planetary source opens directly in the existing terrain tools");
        check(TerrainV2::ValidatePlanetTerrains(document.GetScene(), state.assets, error), error);
        const auto pose = owner->transform;
        const auto point = [&](glm::dvec3 p) {
            return pose.position + pose.rotation * glm::vec3(chart.LocalPoint(p));
        };
        const auto a = point({0, 8, 0}), b = point({0, -8, 0});
        combo("Tool", "Excavate", 5);
        check(tools.Stroke(document.GetScene(), id, a, glm::normalize(b - a), true, true, false,
                               true, error),
              "actual planetary viewport excavation: " + error);
        wait();
        tools.Stroke(document.GetScene(), id, a, glm::normalize(b - a), false, false, false,
                         true, error);
        check(!tools.Guard(state), "unpublished source changes cannot silently enter Play");
        click("Save and update playable terrain");
        wait();
        check(tools.Guard(state), "normal publish action releases the existing Play guard");
        auto inspector = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowSize({700, 900}, ImGuiCond_Always);
            ImGui::Begin("Landscape card");
            TerrainV2ComponentEditor().draw(document, *document.GetScene().Find(id), state);
            ImGui::End();
            ImGui::Render();
        };
        inspector();
        inspector();
        auto *card = ImGui::FindWindowByName("Landscape card");
        ImGui::ActivateItemByID(card->GetID("Live terrain navigation"));
        inspector();
        check(document.GetScene().Find(id)->navigationSurface.has_value(),
              "Inspector enables source-owned navigation on projected terrain");
        check(document.SaveAs((root / "Scenes/main.judas").string(), error), error);
        RuntimeWorld world;

        check(world.Build(document.GetScene(), &host.Resources(), error), error);
        check(world.LiveTerrain().Status(id)->navigation,
              "Inspector-authored coverage builds actual projected navigation");
        auto hit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
        check(hit.hit && world.EntityIdOfBody(hit.body) == id &&
                  chart.ChartPoint(glm::inverse(pose.rotation) * (hit.point - pose.position)).y <
                      -.5,
              "ordinary Play uses actual excavated collision rather than the base planet");
        world.Destroy();
        auto &renderer = host.GetRenderer();
        TerrainPlanetPreview preview;
        const auto view =
            glm::lookAt(point({0, 30, 0}), point({0, 0, 0}), pose.rotation * glm::vec3(0, 0, -1));
        const auto projection = glm::ortho(-8.f, 8.f, -8.f, 8.f, 1.f, 60.f);
        auto draw = [&](bool native) {
            auto scene = document.GetScene();
            renderer.BeginFrame(256, 256);
            renderer.SetCamera(view, projection);
            renderer.SetLighting(pose.rotation * glm::normalize(glm::vec3(.2f, 1, .3f)), {1, 1, 1},
                                 {.25f, .25f, .25f});
            if (native)
                tools.Draw(renderer, scene, host.Resources());
            error.clear();
            preview.Draw(renderer, scene, host.Resources(), error);
            check(error.empty(), error);
            renderer.EndMeshBatch();
            float depth = 0;
            glReadPixels(128, 128, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
            if (native) {
                std::vector<unsigned char> rgb;
                renderer.CaptureFrame(256, 256, rgb);
                check(WriteRgbPng((root / "native-pit-editor.png").string(), 256, 256, rgb),
                      "capture actual editor-preview composition");
            }
            renderer.EndFrame();
            return depth;
        };
        check(draw(false) == 1, "scene view removes base render over the actual native footprint");
        check(draw(true) > 0 && draw(true) < 1, "scene view shows the native excavation floor");
        check(glGetError() == GL_NO_ERROR, "native creation and editor rendering leave GL clean");
        preview.Clear();
        tools.Shutdown(renderer, state.importJobs);
        ImGui::DestroyContext(context);
        host.Shutdown();
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "FAIL " << exception.what() << " after " << checks << " checks\n";
        return 1;
    }
}
