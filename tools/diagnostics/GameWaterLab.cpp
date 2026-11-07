#include "BuildIdentity.h"
#include "EngineHost.h"
#include "GameWaterLabScene.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "fluid/GameWaterSurface.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(int argc, char **argv) try {
    if (HandleBuildIdentityArguments(argc, argv, "game-water-preview"))
        return 0;
    ProfileRun profileRun("game-water-preview");
    unsigned mode = 0, steps = 0, saveAt = 0, captureStep = 120;
    bool headless = false, rotated = false, noBodies = false;
    std::string logPath, frameLogPath, screenshot, saveDirectory = ".cache/game-water-lab-saves",
                                                   loadDirectory;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing value for " + arg);
            return std::string(argv[++i]);
        };
        if (arg == "--headless")
            headless = true;
        else if (arg == "--flow")
            mode = 1;
        else if (arg == "--rotated")
            rotated = true;
        else if (arg == "--no-bodies")
            noBodies = true;
        else if (arg == "--mode") {
            const auto v = value();
            if (v == "pool")
                mode = 0;
            else if (v == "flow")
                mode = 1;
            else if (v == "air")
                mode = 2;
            else if (v == "terrain")
                mode = 3;
            else if (v == "original")
                mode = 4;
            else
                throw std::runtime_error("Unknown scene: " + v);
        } else if (arg == "--steps")
            steps = unsigned(std::stoul(value()));
        else if (arg == "--save-at")
            saveAt = unsigned(std::stoul(value()));
        else if (arg == "--save-dir")
            saveDirectory = value();
        else if (arg == "--load")
            loadDirectory = value();
        else if (arg == "--log")
            logPath = value();
        else if (arg == "--frame-log")
            frameLogPath = value();
        else if (arg == "--screenshot")
            screenshot = value();
        else if (arg == "--screenshot-step")
            captureStep = unsigned(std::stoul(value()));
        else
            throw std::runtime_error("Unknown water lab option: " + arg);
    }
    if (steps > 100000)
        throw std::runtime_error("Water lab step budget exceeds 100000.");
    std::string error;
    auto state = loadDirectory.empty() ? std::make_unique<WaterLab::Scene>(mode, rotated, noBodies)
                                       : WaterLab::Scene::Load(loadDirectory, error);
    if (!state)
        throw std::runtime_error(error);
    std::ofstream file;
    if (!logPath.empty()) {
        file.open(logPath);
        if (!file)
            throw std::runtime_error("Cannot open water lab log.");
    }
    std::ostream &log = logPath.empty() ? std::cout : file;
    log << std::setprecision(12)
        << "step,water,stored,fluid_ms,geometry_ms,pressure_ms,transport_ms,"
           "iterations,residual,speed,"
           "rms_speed,energy,unresolved,float_y,float_speed,sink_y,pockets,max_"
           "air_pressure,left_water,"
           "primary_air_pressure,primary_air_volume,scene_ms,air_accounted,air_"
           "closed_mass,resolved_"
           "air,owned_bytes\n";
    auto row = [&] {
        auto &lab = *state;
        const auto &s = lab.water.Stats();
        double maximum = 0, left = 0, primary = 0, volume = 0;
        for (auto pocket : lab.water.Air())
            if (!pocket.atmosphere) {
                maximum = std::max(maximum, double(std::abs(pocket.pressure)));
                if (pocket.mass > 0 && pocket.volume > volume) {
                    volume = pocket.volume;
                    primary = pocket.pressure;
                }
            }
        const auto &cells = lab.water.Cells();
        for (int i = 0; i < int(cells.size()); ++i)
            if (lab.water.Coordinates(i).x < lab.settings.cells.x / 2)
                left += cells[i].water + cells[i].displaced;
        auto height = [&](std::size_t i) {
            return i < lab.bodies.size()
                       ? lab.water.ToLocal(lab.physics.GetTransform(lab.bodies[i].handle).position)
                             .y
                       : 0.f;
        };
        log << lab.step << ',' << s.water << ',' << s.storedDisplacement << ',' << s.milliseconds
            << ',' << s.geometryMs << ',' << s.pressureMs << ',' << s.transportMs << ','
            << s.iterations << ',' << s.pressureResidual << ',' << s.maximumSpeed << ','
            << s.rmsSpeed << ',' << s.kineticEnergy << ',' << s.unresolvedWater << ',' << height(0)
            << ','
            << (lab.bodies.empty()
                    ? 0
                    : glm::length(lab.physics.GetLinearVelocity(lab.bodies[0].handle)))
            << ',' << height(1) << ',' << lab.water.Air().size() << ',' << maximum << ',' << left
            << ',' << primary << ',' << volume << ',' << lab.lastStepMs << ',' << s.totalAirMass
            << ',' << s.sealedAirMass << ',' << s.resolvedAirPockets << ',' << s.bytes << '\n';
    };
    auto advance = [&] {
        ProfileFixedStep fixedStep(1. / 60);
        JUDAS_PROFILE_SCOPE("Game water and MK2");
        state->Advance();
        row();
        if (saveAt && state->step == saveAt && !state->Save(saveDirectory, error))
            throw std::runtime_error(error);
    };
    if (headless) {
        for (unsigned i = 0; i < (steps ? steps : 600); ++i)
            advance();
        return 0;
    }
    EngineHost host;
    if (!host.Init("Game Water | real-time fluid replacement preview", 1280, 800, true, error))
        throw std::runtime_error(error);
    auto &window = host.GetWindow();
    auto &renderer = host.GetRenderer();
    window.SetMouseCaptured(true);
    glm::vec3 eye;
    float yaw = 0, pitch = 0;
    auto camera = [&] {
        auto size = glm::vec3(state->settings.cells) * state->settings.cellSize;
        auto localEye = size * glm::vec3(1.3f, 2.1f, 1.7f);
        auto look = glm::normalize(glm::vec3(size.x * .5f, size.y * .3f, size.z * .5f) - localEye);
        yaw = std::atan2(look.x, look.z);
        pitch = std::asin(look.y);
        eye = state->water.ToWorld(localEye);
    };
    camera();
    MeshHandle terrainMesh;
    auto uploadTerrain = [&] {
        if (terrainMesh.IsValid())
            renderer.DestroyMesh(terrainMesh);
        terrainMesh = {};
        if (!state->terrainMesh.vertices.empty())
            terrainMesh = renderer.CreateMesh(state->terrainMesh);
    };
    uploadTerrain();
    bool paused = false, toggleGate = false, spawn = false, capture = false, save = false,
         load = false, automaticCaptureDone = false;
    int requested = -1;
    bool rotate = state->rotated;
    std::string status;
    window.SetEventHook([&](const SDL_Event &e) {
        if (e.type != SDL_KEYDOWN || e.key.repeat)
            return;
        switch (e.key.keysym.scancode) {
        case SDL_SCANCODE_1:
            requested = 0;
            break;
        case SDL_SCANCODE_2:
            requested = 1;
            break;
        case SDL_SCANCODE_3:
            requested = 2;
            break;
        case SDL_SCANCODE_4:
            requested = 3;
            break;
        case SDL_SCANCODE_5:
            requested = 4;
            break;
        case SDL_SCANCODE_X:
            rotate = !rotate;
            requested = int(state->mode);
            break;
        case SDL_SCANCODE_R:
            requested = int(state->mode);
            break;
        case SDL_SCANCODE_P:
            paused = !paused;
            break;
        case SDL_SCANCODE_G:
            toggleGate = true;
            break;
        case SDL_SCANCODE_B:
            spawn = true;
            break;
        case SDL_SCANCODE_F5:
            save = true;
            break;
        case SDL_SCANCODE_F9:
            load = true;
            break;
        case SDL_SCANCODE_TAB:
            window.SetMouseCaptured(!window.IsMouseCaptured());
            break;
        case SDL_SCANCODE_F12:
            capture = true;
            break;
        default:
            break;
        }
    });
    auto previous = std::chrono::steady_clock::now();
    const auto presentationStart = previous;
    std::ofstream frameLog;
    if (!frameLogPath.empty()) {
        frameLog.open(frameLogPath);
        if (!frameLog)
            throw std::runtime_error("Cannot open water presentation log.");
        frameLog << std::setprecision(12)
                 << "frame,wall_seconds,step,frame_ms,draw_submit_ms,swap_capture_"
                    "ms,backlog_seconds\n";
    }
    double accumulated = 0;
    unsigned frames = 0;
    const unsigned startStep = state->step;
    while (!window.ShouldClose()) {
        ProfileFrame profileFrame("game-water-preview");
        const auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - previous).count();
        previous = now;
        window.PollEvents();
        host.PumpResources();
        if (requested >= 0) {
            state = std::make_unique<WaterLab::Scene>(unsigned(requested), rotate, noBodies);
            requested = -1;
            accumulated = 0;
            camera();
            uploadTerrain();
        }
        if (toggleGate) {
            if (state->mode == 1 || state->mode == 2)
                state->SetGate(!state->gate);
            else
                status = "Choose the sluice or air chamber to operate a gate.";
            toggleGate = false;
        }
        if (save) {
            status = state->Save(saveDirectory, error)
                         ? "Saved water, air, gate and physical bodies."
                         : error;
            save = false;
        }
        if (load) {
            auto restored = WaterLab::Scene::Load(saveDirectory, error);
            if (restored) {
                state = std::move(restored);
                status = "Loaded the saved pool.";
                accumulated = 0;
                camera();
                uploadTerrain();
            } else
                status = error;
            load = false;
        }
        int dx = 0, dy = 0;
        window.GetMouseDelta(dx, dy);
        if (window.IsMouseCaptured()) {
            yaw -= dx * .0025f;
            pitch = glm::clamp(pitch - dy * .0025f, -1.4f, 1.4f);
        }
        auto q = state->settings.rotation;
        auto up = q * glm::vec3(0, 1, 0);
        glm::vec3 direction = q * glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                                            std::cos(pitch) * std::cos(yaw));
        auto keys = SDL_GetKeyboardState(nullptr);
        auto right = glm::normalize(glm::cross(direction, up));
        float speed = std::min(elapsed, .1f) * 7;
        eye += speed * (direction * float(keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]) +
                        right * float(keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]) +
                        up * float(keys[SDL_SCANCODE_SPACE] - keys[SDL_SCANCODE_LCTRL]));
        if (spawn && state->bodies.size() < 64) {
            state->Spawn(state->water.ToLocal(eye + direction * 2.f), glm::vec3(.3f), 500,
                         {.9f, .55f, .2f});
            state->physics.SetLinearVelocity(state->bodies.back().handle, direction * 6.f);
            spawn = false;
        }
        if (spawn) {
            status = "The workshop's 64-body spawn budget is full. Reset to clear it.";
            spawn = false;
        }
        if (!paused)
            accumulated += double(elapsed);
        unsigned catchup = 0;
        while (accumulated >= 1. / 60 && catchup < 6) {
            advance();
            accumulated -= 1. / 60;
            ++catchup;
        }
        PerformanceProfiler::Get().FixedState(accumulated, catchup == 6, 0, paused);
        const auto drawStart = std::chrono::steady_clock::now();
        static ProfileLabel submitLabel("Game water frame submission");
        ProfileScope submission(submitLabel);
        renderer.BeginFrame(window.Width(), window.Height());
        renderer.SetCamera(glm::lookAt(eye, eye + direction, up),
                           glm::perspective(glm::radians(65.f),
                                            float(window.Width()) / window.Height(), .1f, 200.f));
        renderer.SetLighting(glm::normalize(q * glm::vec3(-.3f, 1, .4f)), {1, .97f, .9f},
                             {.35f, .4f, .46f});
        if (terrainMesh.IsValid())
            renderer.DrawMesh(terrainMesh, state->water.ToWorld({6, 0, 4}), q, {1, 1, 1}, {},
                              {.28f, .4f, .22f});
        auto glass = [&](const WaterLab::Wall &wall) {
            return wall.half.z < .3f || (state->mode == 2 && wall.half.y <= .71f);
        };
        for (const auto &wall : state->walls)
            if (!glass(wall))
                renderer.DrawBox(wall.center, wall.rotation, wall.half, {.55f, .61f, .65f});
        if (state->gate)
            renderer.DrawBox(state->gateShape.center, state->gateShape.rotation,
                             state->gateShape.half, {.8f, .47f, .12f});
        for (auto b : state->bodies) {
            auto pose = state->physics.GetTransform(b.handle);
            renderer.DrawBox(pose.position, pose.rotation, b.half, b.color);
        }
        renderer.BeginTransparentPass();
        const auto waterPass = renderer.BeginProfilePass("Game water surface");
        renderer.DrawTransientSurface(JudasWater::BuildPoolSurface(state->water),
                                      {.07f, .46f, .64f}, .75f);
        renderer.EndProfilePass(waterPass);
        for (const auto &wall : state->walls)
            if (glass(wall))
                renderer.DrawBox(wall.center, wall.rotation, wall.half, {.65f, .8f, .84f}, .16f);
        renderer.EndTransparentPass();
        renderer.EndFrame();
        renderer.BeginUIFrame(window.Width(), window.Height());
        renderer.DrawUIRect({18, 18}, {940, 156}, {.025f, .04f, .06f, .92f});
        const char *names[] = {"Filled pool", "Sluice flow", "Trapped-air chamber",
                               "Authored heightfield",
                               "Original authored basin at game resolution"};
        renderer.DrawUIText(std::string("GAME WATER | ") + names[state->mode] +
                                (state->rotated ? " | rotated gravity" : ""),
                            {30, 26}, .43f, {.65f, 1, 1, 1});
        renderer.DrawUIText("1 pool | 2 sluice | 3 air | 4 terrain | 5 original | "
                            "X rotate | R reset",
                            {30, 57}, .36f, {1, 1, 1, 1});
        renderer.DrawUIText("WASD + mouse: fly | Space/Ctrl: up/down | B: throw box | G: gate/vent",
                            {30, 82}, .34f, {1, 1, 1, 1});
        renderer.DrawUIText("P pause | F5 save | F9 load | Tab cursor | F12 screenshot", {30, 105},
                            .34f, {.8f, .85f, .9f, 1});
        std::ostringstream text;
        text << std::fixed << std::setprecision(2) << state->water.Stats().water << " m3 | fluid "
             << state->water.Stats().milliseconds << " ms | step " << state->step;
        if (state->mode == 2) {
            double volume = 0, pressure = 0;
            for (auto pocket : state->water.Air())
                if (!pocket.atmosphere && !pocket.unresolved && pocket.volume > volume) {
                    volume = pocket.volume;
                    pressure = pocket.pressure;
                }
            text << " | chamber " << pressure / 1000 << " kPa | vent "
                 << (state->gate ? "CLOSED" : "OPEN");
        }
        renderer.DrawUIText(text.str(), {30, 131}, .36f, {1, .85f, .55f, 1});
        if (!status.empty())
            renderer.DrawUIText(status, {30, 180}, .36f, {1, 1, .7f, 1});
        renderer.EndUIFrame();
        submission.End();
        const auto drawEnd = std::chrono::steady_clock::now();
        if (capture ||
            (!screenshot.empty() && !automaticCaptureDone && state->step >= captureStep)) {
            std::vector<unsigned char> pixels;
            renderer.CaptureFrame(window.Width(), window.Height(), pixels);
            if (!WriteRgbPng(screenshot.empty() ? "/tmp/judas-game-water.png" : screenshot,
                             window.Width(), window.Height(), pixels))
                throw std::runtime_error("Screenshot failed");
            capture = false;
            automaticCaptureDone = true;
        }
        window.SwapBuffers();
        renderer.PollProfileGPU();
        const auto frameEnd = std::chrono::steady_clock::now();
        ++frames;
        if (frameLog)
            frameLog << frames << ','
                     << std::chrono::duration<double>(frameEnd - presentationStart).count() << ','
                     << state->step << ','
                     << std::chrono::duration<double, std::milli>(frameEnd - now).count() << ','
                     << std::chrono::duration<double, std::milli>(drawEnd - drawStart).count()
                     << ',' << std::chrono::duration<double, std::milli>(frameEnd - drawEnd).count()
                     << ',' << accumulated << '\n';
        if (steps && state->step >= startStep + steps)
            break;
    }
    if (terrainMesh.IsValid())
        renderer.DestroyMesh(terrainMesh);
    std::cerr << "Presentation: " << frames << " frames, " << state->step - startStep
              << " fixed steps, "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - presentationStart)
                     .count()
              << " wall seconds.\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
