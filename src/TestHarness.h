#pragma once
#include <string>
#include <functional>
class Window;
class Renderer;
class InteractivePlay;
// Opt-in deterministic input through the project's real M35 bindings. Frames,
// pause, presentation scripts, cameras and UI use InteractivePlay, as normal play.
// Legacy STEPS/HOLD/TAP/LOOK remain; see docs/TEST_HARNESS.md for named controls.
int RunTestHarness(Window&,Renderer&,InteractivePlay&,const std::string& scriptPath, const std::function<void()>& beforeFrame={}, const std::function<void()>& afterFrame={}, const std::function<bool(const std::string&)>& servicesReady={},const std::function<void(bool load)>& debugSave={});
