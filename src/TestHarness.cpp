#include "TestHarness.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// stb_image_write triggers -Wmissing-field-initializers under this
// project's own warning flags in a few of its internal functions we don't
// even call (BMP/TGA/HDR/JPG writers) — a lint characteristic of a
// third-party header we don't control, not of this project's code, so it's
// suppressed only for this one include rather than loosening -Wall/-Wextra
// project-wide (same reasoning as Jolt's own ENABLE_ALL_WARNINGS override
// in CMakeLists.txt).
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "GravityField.h"
#include "PhysicsWorld.h"
#include "PlayerController.h"
#include "Renderer.h"
#include "SimulationTiming.h"
#include "Window.h"

// Script format (see docs/ARCHITECTURE.md, "Automated testing"): one
// directive per line, `#` starts a comment, blank lines ignored.
//
//   STEPS <n>                        total fixed steps to run (default 600)
//   LOG_EVERY <n>                    print state every n steps (default 1)
//   HOLD <W|A|S|D> <fromStep> <toStepExclusive>
//   LOOK <dx> <dy> <atStep>          one queued mouse delta
//   TAP <SPACE|R> <atStep>           one-shot jump/reset request
//   SCREENSHOT <atStep> <filename>   render + write a PNG at that step
//   REALTIME <renderFrameCount>      switch to real-time diagnostic mode
//                                    (see "Real-time mode" below); HOLD/
//                                    LOOK/TAP step numbers then mean
//                                    render-frame index, not fixed-step index
namespace {

Action ParseHoldKey(const std::string& key, bool& outOk) {
    outOk = true;
    if (key == "W") return Action::MoveForward;
    if (key == "S") return Action::MoveBackward;
    if (key == "A") return Action::StrafeLeft;
    if (key == "D") return Action::StrafeRight;
    outOk = false;
    return Action::MoveForward;
}

struct HoldEvent {
    Action action;
    int fromStep;
    int toStep;
};
struct LookEvent {
    int step;
    int dx;
    int dy;
};
struct TapEvent {
    int step;
    bool isJump;  // false means reset
};
struct ScreenshotEvent {
    int step;
    std::string filename;
};

struct Script {
    int totalSteps = 600;
    int logEvery = 1;
    bool realtime = false;
    int renderFrames = 300;
    std::vector<HoldEvent> holds;
    std::vector<LookEvent> looks;
    std::vector<TapEvent> taps;
    std::vector<ScreenshotEvent> screenshots;
};

bool LoadScript(const std::string& path, Script& outScript) {
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "[TestHarness] Could not open script: %s\n", path.c_str());
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string directive;
        iss >> directive;
        if (directive.empty() || directive[0] == '#') continue;

        if (directive == "STEPS") {
            iss >> outScript.totalSteps;
        } else if (directive == "LOG_EVERY") {
            iss >> outScript.logEvery;
        } else if (directive == "HOLD") {
            std::string key;
            int fromStep = 0;
            int toStep = 0;
            iss >> key >> fromStep >> toStep;
            bool ok = false;
            const Action action = ParseHoldKey(key, ok);
            if (ok) {
                outScript.holds.push_back({action, fromStep, toStep});
            } else {
                std::fprintf(stderr, "[TestHarness] Unknown HOLD key: %s\n", key.c_str());
            }
        } else if (directive == "LOOK") {
            int dx = 0;
            int dy = 0;
            int step = 0;
            iss >> dx >> dy >> step;
            outScript.looks.push_back({step, dx, dy});
        } else if (directive == "TAP") {
            std::string key;
            int step = 0;
            iss >> key >> step;
            if (key == "SPACE" || key == "R") {
                outScript.taps.push_back({step, key == "SPACE"});
            } else {
                std::fprintf(stderr, "[TestHarness] Unknown TAP key: %s\n", key.c_str());
            }
        } else if (directive == "SCREENSHOT") {
            int step = 0;
            std::string filename;
            iss >> step >> filename;
            outScript.screenshots.push_back({step, filename});
        } else if (directive == "REALTIME") {
            outScript.realtime = true;
            iss >> outScript.renderFrames;
        } else {
            std::fprintf(stderr, "[TestHarness] Unknown directive: %s\n", directive.c_str());
        }
    }
    return true;
}

void TakeScreenshotIfRequested(int index, const std::vector<ScreenshotEvent>& screenshots,
                                Window& window, Renderer& renderer, PlayerController& player,
                                const std::function<void(Renderer&, float)>& drawScene,
                                float presentationAlpha) {
    for (const ScreenshotEvent& shot : screenshots) {
        if (shot.step != index) continue;

        const int width = window.Width();
        const int height = window.Height();
        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

        renderer.BeginFrame(width, height);
        renderer.SetCamera(player.GetViewMatrix(presentationAlpha),
                            player.GetProjectionMatrix(aspectRatio));
        drawScene(renderer, presentationAlpha);
        renderer.EndFrame();

        std::vector<unsigned char> pixels;
        renderer.CaptureFrame(width, height, pixels);
        const int written =
            stbi_write_png(shot.filename.c_str(), width, height, 3, pixels.data(), width * 3);
        std::printf("[TestHarness] %s screenshot: %s\n", written ? "Wrote" : "FAILED to write",
                    shot.filename.c_str());
    }
}

// Fixed-step mode: runs exactly `script.totalSteps` fixed physics steps as
// fast as possible — no real-time pacing, no vsync wait. One "step" here is
// one fixed simulation step; this is what Milestone 5 used to verify
// gameplay/physics logic in isolation from any rendering-timing question.
int RunFixedStepMode(Window& window, Renderer& renderer, PhysicsWorld& physicsWorld,
                      PlayerController& player, const GravityField& gravity,
                      const std::function<void(Renderer&, float)>& drawScene,
                      const Script& script) {
    std::printf("step,time,posX,posY,posZ,upX,upY,upZ,grounded,velX,velY,velZ\n");

    for (int step = 0; step < script.totalSteps; ++step) {
        for (const HoldEvent& hold : script.holds) {
            if (step == hold.fromStep) window.SetTestActionState(hold.action, true);
            if (step == hold.toStep) window.SetTestActionState(hold.action, false);
        }
        for (const LookEvent& look : script.looks) {
            if (look.step == step) window.QueueTestMouseDelta(look.dx, look.dy);
        }
        for (const TapEvent& tap : script.taps) {
            if (tap.step == step) {
                if (tap.isJump) {
                    window.RequestTestJump();
                } else {
                    window.RequestTestReset();
                }
            }
        }

        player.UpdateFrameInput(window);
        if (window.ConsumeResetRequest()) {
            player.Reset();
        }

        physicsWorld.Step(SimulationTiming::kFixedTimestep);
        player.FixedUpdate(window, physicsWorld, gravity, SimulationTiming::kFixedTimestep);

        if (script.logEvery > 0 && step % script.logEvery == 0) {
            const glm::vec3 pos = player.GetPosition();
            const glm::vec3 vel = player.GetVelocity();
            const glm::vec3 up = player.GetOrientation() * glm::vec3(0.0f, 1.0f, 0.0f);
            std::printf("%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%.4f,%.4f,%.4f\n", step,
                        step * SimulationTiming::kFixedTimestep, pos.x, pos.y, pos.z, up.x, up.y,
                        up.z, player.IsGrounded() ? 1 : 0, vel.x, vel.y, vel.z);
        }

        // Fixed-step mode never has an "in between" — we're always exactly
        // at a step boundary here, so presented == authoritative (alpha=1).
        TakeScreenshotIfRequested(step, script.screenshots, window, renderer, player, drawScene,
                                   1.0f);
    }
    return 0;
}

// Real-time diagnostic mode (Milestone 6): mirrors Application::Run's own
// interactive accumulator loop exactly (same SimulationTiming constants,
// same clamp/accumulate/catch-up-cap shape) instead of running fixed steps
// back-to-back — so it reproduces the actual render-frame-to-fixed-step
// relationship the interactive path produces, headlessly and measurably.
// One logged row is one RENDERED frame, not one fixed step: `stepsThisFrame`
// and the position/orientation actually handed to the renderer that frame
// are exactly what this mode exists to expose. See docs/ARCHITECTURE.md,
// "Diagnosis," for what this was used to find in Milestone 6.
int RunRealtimeMode(Window& window, Renderer& renderer, PhysicsWorld& physicsWorld,
                     PlayerController& player, const GravityField& gravity,
                     const std::function<void(Renderer&, float)>& drawScene,
                     const Script& script) {
    // "pres*" columns are what's actually presented that frame (see
    // PlayerController::GetPresentedPosition/Orientation) alongside the raw
    // authoritative "pos"/"up" columns, so interpolation can be checked
    // directly: presented values should lie between the previous and
    // current authoritative state, never coincide with a stale repeated
    // authoritative value the way direct presentation of "pos"/"up" would
    // on a zero-step frame.
    std::printf(
        "frame,wallDeltaMs,stepsThisFrame,alpha,posX,posY,posZ,upX,upY,upZ,presX,presY,presZ,"
        "presUpX,presUpY,presUpZ,grounded\n");

    float physicsAccumulator = 0.0f;
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previousCounter = SDL_GetPerformanceCounter();

    for (int frame = 0; frame < script.renderFrames; ++frame) {
        for (const HoldEvent& hold : script.holds) {
            if (frame == hold.fromStep) window.SetTestActionState(hold.action, true);
            if (frame == hold.toStep) window.SetTestActionState(hold.action, false);
        }
        for (const LookEvent& look : script.looks) {
            if (look.step == frame) window.QueueTestMouseDelta(look.dx, look.dy);
        }
        for (const TapEvent& tap : script.taps) {
            if (tap.step == frame) {
                if (tap.isJump) {
                    window.RequestTestJump();
                } else {
                    window.RequestTestReset();
                }
            }
        }

        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        float frameDeltaTime =
            static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        if (frameDeltaTime > SimulationTiming::kMaxFrameDeltaTime) {
            frameDeltaTime = SimulationTiming::kMaxFrameDeltaTime;
        }

        player.UpdateFrameInput(window);
        if (window.ConsumeResetRequest()) {
            player.Reset();
            physicsAccumulator = 0.0f;
        }

        physicsAccumulator += frameDeltaTime;
        int stepsThisFrame = 0;
        while (physicsAccumulator >= SimulationTiming::kFixedTimestep &&
               stepsThisFrame < SimulationTiming::kMaxPhysicsStepsPerFrame) {
            physicsWorld.Step(SimulationTiming::kFixedTimestep);
            player.FixedUpdate(window, physicsWorld, gravity, SimulationTiming::kFixedTimestep);
            physicsAccumulator -= SimulationTiming::kFixedTimestep;
            ++stepsThisFrame;
        }
        if (stepsThisFrame == SimulationTiming::kMaxPhysicsStepsPerFrame) {
            physicsAccumulator = 0.0f;
        }

        const float presentationAlpha = physicsAccumulator / SimulationTiming::kFixedTimestep;

        const int width = window.Width();
        const int height = window.Height();
        const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        renderer.BeginFrame(width, height);
        renderer.SetCamera(player.GetViewMatrix(presentationAlpha),
                            player.GetProjectionMatrix(aspectRatio));
        drawScene(renderer, presentationAlpha);
        renderer.EndFrame();
        window.SwapBuffers();

        const glm::vec3 pos = player.GetPosition();
        const glm::vec3 up = player.GetOrientation() * glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 presPos = player.GetPresentedPosition(presentationAlpha);
        const glm::vec3 presUp =
            player.GetPresentedOrientation(presentationAlpha) * glm::vec3(0.0f, 1.0f, 0.0f);
        std::printf("%d,%.4f,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
                    "%d\n",
                    frame, frameDeltaTime * 1000.0f, stepsThisFrame, presentationAlpha, pos.x,
                    pos.y, pos.z, up.x, up.y, up.z, presPos.x, presPos.y, presPos.z, presUp.x,
                    presUp.y, presUp.z, player.IsGrounded() ? 1 : 0);

        TakeScreenshotIfRequested(frame, script.screenshots, window, renderer, player, drawScene,
                                   presentationAlpha);
    }
    return 0;
}

}  // namespace

int RunTestHarness(Window& window, Renderer& renderer, PhysicsWorld& physicsWorld,
                    PlayerController& player, const GravityField& gravity,
                    const std::function<void(Renderer&, float)>& drawScene,
                    const std::string& scriptPath) {
    Script script;
    if (!LoadScript(scriptPath, script)) {
        return 1;
    }

    window.SetTestInputMode(true);

    const int exitCode = script.realtime
                              ? RunRealtimeMode(window, renderer, physicsWorld, player, gravity,
                                                 drawScene, script)
                              : RunFixedStepMode(window, renderer, physicsWorld, player, gravity,
                                                  drawScene, script);

    window.SetTestInputMode(false);
    return exitCode;
}
