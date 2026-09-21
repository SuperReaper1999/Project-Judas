#include "TestHarness.h"

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
namespace {

constexpr float kFixedTimestep = 1.0f / 60.0f;

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
        } else {
            std::fprintf(stderr, "[TestHarness] Unknown directive: %s\n", directive.c_str());
        }
    }
    return true;
}

}  // namespace

int RunTestHarness(Window& window, Renderer& renderer, PhysicsWorld& physicsWorld,
                    PlayerController& player, const GravityField& gravity,
                    const std::function<void(Renderer&)>& drawScene,
                    const std::string& scriptPath) {
    Script script;
    if (!LoadScript(scriptPath, script)) {
        return 1;
    }

    window.SetTestInputMode(true);

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

        physicsWorld.Step(kFixedTimestep);
        player.FixedUpdate(window, physicsWorld, gravity, kFixedTimestep);

        if (script.logEvery > 0 && step % script.logEvery == 0) {
            const glm::vec3 pos = player.GetPosition();
            const glm::vec3 vel = player.GetVelocity();
            const glm::vec3 up = player.GetRenderOrientation() * glm::vec3(0.0f, 1.0f, 0.0f);
            std::printf("%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%.4f,%.4f,%.4f\n", step,
                        step * kFixedTimestep, pos.x, pos.y, pos.z, up.x, up.y, up.z,
                        player.IsGrounded() ? 1 : 0, vel.x, vel.y, vel.z);
        }

        for (const ScreenshotEvent& shot : script.screenshots) {
            if (shot.step != step) continue;

            const int width = window.Width();
            const int height = window.Height();
            const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

            renderer.BeginFrame(width, height);
            renderer.SetCamera(player.GetViewMatrix(), player.GetProjectionMatrix(aspectRatio));
            drawScene(renderer);
            renderer.EndFrame();

            std::vector<unsigned char> pixels;
            renderer.CaptureFrame(width, height, pixels);
            const int written =
                stbi_write_png(shot.filename.c_str(), width, height, 3, pixels.data(), width * 3);
            std::printf("[TestHarness] %s screenshot: %s\n", written ? "Wrote" : "FAILED to write",
                        shot.filename.c_str());
        }
    }

    window.SetTestInputMode(false);
    return 0;
}
