#include "EngineHost.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "Simulation.h"
#include "WorldPresentation.h"
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <thread>

// Measurement driver, not a timing gate. Every measured sample renders the
// ordinary game path; screenshot-only scripted frames cannot measure GPU cost.
int main(int argc, char **argv) {
    try {
        if (argc != 2 && !(argc == 3 && std::string(argv[2]) == "--traverse"))
            throw std::runtime_error("Supply an editable terrain Workshop project.");
        const bool traverse = argc == 3;
        using Clock = std::chrono::steady_clock;
        auto milliseconds = [](auto begin) {
            return std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        };
        std::string error;
        Project project;
        if (!project.Load(argv[1], error))
            throw std::runtime_error(error);
        EngineHost host;
        if (!host.Init("Terrain V2 actual-game measurement", 1280, 800, false, error))
            throw std::runtime_error(error);
        host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
        auto &window = host.GetWindow();
        auto &renderer = host.GetRenderer();
        window.SetTestInputMode(true);
        if (!window.Input().SetMap(project.Settings().input, error))
            throw std::runtime_error(error);
        Scene scene;
        if (!LoadSceneFromFile(project.StartupScenePath(), scene, error))
            throw std::runtime_error(error);
        RuntimeWorld world;
        world.ConfigureProject(project.Settings());
        world.SetSceneControl(std::make_shared<SceneSession>(project, project.StartupScenePath()));
        auto begin = Clock::now();
        if (!world.Build(scene, &host.Resources(), error, &project.Settings().classification,
                         &project.Settings().navigation))
            throw std::runtime_error(error);
        const auto buildMs = milliseconds(begin);

        if (!world.IsBuilt())
            throw std::runtime_error(error);
        host.Resources().WaitForAll();
        GLuint query;
        glGenQueries(1, &query);
        auto frame = [&](const char *phase, unsigned index, bool record) {
            host.PumpResources();
            window.Input().BeginFixedStep();
            const auto frameStart = Clock::now();
            auto start = frameStart;
            world.AdvanceRuntimeTerrain();
            world.UpdateUIScripts(&window.Input(), 1.f / 60);
            world.UpdateScripts(&window.Input(), 1.f / 60);
            StepPlayedWorld(world, window, 1.f / 60);
            world.PresentationScripts(&window.Input(), 1.f / 60, 1);
            const auto simulation = milliseconds(start);
            const auto view = world.view.value();
            auto eye = view.pose.position;
            auto matrix = glm::mat4_cast(glm::inverse(view.pose.rotation)) *
                          glm::translate(glm::mat4(1), -view.pose.position);
            double queryMs = 0;
            if (std::string_view(phase) == "traversal") {
                // Fast editor-style serpentine travel visits every native
                // chunk repeatedly, while the real player remains supported
                // off-screen. Collision demand is deliberately independent of
                // the visual camera and its LOD selection.
                const unsigned cell = (index / 8) % 1024, row = cell / 32, column = cell % 32;
                const float x = -496 + 32 * float(row % 2 ? 31 - column : column),
                            z = -496 + 32 * float(row);
                eye = {x, 50, z};
                matrix = glm::lookAt(eye, eye + glm::vec3(0, -.8f, -1), glm::vec3(0, 1, 0));
                world.view->pose.position = eye;
                world.view->pose.rotation = glm::quat_cast(glm::inverse(matrix));
                const auto queryStart = Clock::now();
                const auto hit = world.Physics().Raycast({x, 100, z}, {0, -1, 0}, 250);
                queryMs = milliseconds(queryStart);
                if (!hit.hit || world.EntityIdOfBody(hit.body) != 1)
                    throw std::runtime_error(
                        "off-screen terrain query lost collision during traversal");
            }
            const auto projection = glm::perspective(glm::radians(view.fov), 1.6f, .08f, 2200.f);
            renderer.ResetStats();
            glFinish();
            glBeginQuery(GL_TIME_ELAPSED, query);
            start = Clock::now();
            RenderWorldFrame(renderer, 1280, 800, world, matrix, projection, eye, 1,
                             1.f / 60);
            if (std::string_view(phase) == "traversal") {
                const auto actualEye = glm::vec3(glm::inverse(renderer.ViewMatrix())[3]);
                if (glm::distance(actualEye, eye) > .001f)
                    throw std::runtime_error(
                        "traversal camera was overridden by runtime presentation");
            }
            const auto submission = milliseconds(start);
            glEndQuery(GL_TIME_ELAPSED);
            GLuint64 ns;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
            if (record)
                std::cout << "{\"phase\":\"" << phase << "\",\"index\":" << index
                          << ",\"simulation_scripts_publication_ms\":" << simulation
                          << ",\"collision_query_ms\":" << queryMs
                          << ",\"frame_wall_ms\":" << milliseconds(frameStart)
                          << ",\"render_submission_ms\":" << submission
                          << ",\"gpu_elapsed_ms\":" << ns / 1e6
                          << ",\"draws\":" << renderer.Stats().drawCalls
                          << ",\"triangles\":" << renderer.Stats().triangles;
            if (record) {
                const auto *state = world.LiveTerrain().Status(1);
                std::cout << ",\"cpu_terrain_bytes\":" << state->bytes
                          << ",\"source_page_bytes\":" << state->sourcePageBytes
                          << ",\"source_pages\":" << state->sourcePages
                          << ",\"source_evictions\":" << state->sourceEvictions
                          << ",\"collision_page_bytes\":" << state->collisionPageBytes
                          << ",\"collision_pages\":" << state->collisionPages
                          << ",\"collision_evictions\":" << state->collisionEvictions
                          << ",\"gpu_page_bytes\":" << state->gpuPageBytes
                          << ",\"gpu_pages\":" << state->gpuPages
                          << ",\"gpu_evictions\":" << state->gpuEvictions
                          << ",\"page_prepare_ms\":" << state->pagePrepareMs << "}\n";
            }
            window.Input().BeginFrame();
        };
        std::cout << "{\"scene_build_ms\":" << buildMs
                  << ",\"resolution\":[1280,800],\"samples_per_phase\":180,"
                     "\"gpu_scope\":\"whole ordinary render including submission bubbles; "
                     "synchronous query measurement, not uncapped FPS\"}\n";
        for (unsigned i = 0; i < 60; ++i)
            frame("warm", i, false);
        if (!world.Scripts()->Diagnostics().empty())
            throw std::runtime_error("Game script diagnostic before measurement.");
        for (unsigned i = 0; i < 180; ++i)
            frame("intact", i, true);
        window.Input().SetPhysical("key:T", 1);
        frame("trigger", 0, false);
        window.Input().SetPhysical("key:T", 0);
        // Let the actual project brush queue finish before comparing steady
        // rendering. This is a worker deadline, not a frame-count workaround.
        bool ready = false;
        for (unsigned i = 0; i < 10000; ++i) {
            frame("excavating", i, true);
            const auto records = world.Scripts()->Capture();
            for (const auto &record : records)
                if (record.entity == 64 && record.json.find("\"queue\":[]") != std::string::npos)
                    ready = world.LiveTerrain().ReadyToSave();
            if (ready)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!ready)
            throw std::runtime_error("Actual project excavation queue did not finish.");
        for (unsigned i = 0; i < 60; ++i)
            frame("warm-edited", i, false);
        for (unsigned i = 0; i < 180; ++i)
            frame("excavated", i, true);
        if (!world.Scripts()->Diagnostics().empty())
            throw std::runtime_error("Game script diagnostic.");
        if (traverse)
            for (unsigned i = 0; i < 3 * 1024 * 8; ++i)
                frame("traversal", i, true);
        if (!world.Scripts()->Diagnostics().empty())
            throw std::runtime_error("Traversal game script diagnostic.");
        if (glGetError() != GL_NO_ERROR)
            throw std::runtime_error("OpenGL measurement error.");
        glDeleteQueries(1, &query);

        world.Destroy();
        host.Shutdown();
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "FAIL " << exception.what() << '\n';
        return 1;
    }
}
