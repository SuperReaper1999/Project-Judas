#include <cmath>
#include <cstdio>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

#include "Window.h"
#include "editor/EditorCamera.h"

namespace {
int checks = 0;
int failures = 0;
void Check(bool condition, const char* description) {
    ++checks;
    if (!condition) ++failures;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", description);
}
bool Near(glm::vec3 actual, glm::vec3 expected, float tolerance = 2e-5f) {
    return glm::length(actual - expected) <= tolerance;
}
bool Near(glm::mat4 actual, glm::mat4 expected, float tolerance = 2e-5f) {
    for (int column = 0; column < 4; ++column)
        if (glm::length(actual[column] - expected[column]) > tolerance) return false;
    return true;
}
glm::vec2 Project(const EditorCamera& camera, glm::vec3 point, int width, int height) {
    const auto clip = camera.ProjectionMatrix(float(width) / float(height)) *
        camera.ViewMatrix() * glm::vec4(point, 1.0f);
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return {(ndc.x + 1.0f) * float(width) * 0.5f, (1.0f - ndc.y) * float(height) * 0.5f};
}
void PanMath() {
    EditorCamera camera;
    camera.SetPose({4, 8, -12}, 37.0f, -26.0f);
    const auto forward = camera.Forward();
    const glm::vec3 target{2, 5, 7};
    camera.LookAt(target, 12.0f);
    const auto startPosition = camera.Position();
    const auto startFocus = camera.Focus();
    Check(Near(startFocus, target), "frame selection retains target as focus anchor");
    Check(Near(camera.Forward(), forward), "frame selection preserves the established camera orientation");
    const auto before = Project(camera, target, 1200, 800);
    camera.Pan(80, -40, 800);
    const auto shift = camera.Position() - startPosition;
    const auto after = Project(camera, target, 1200, 800);
    Check(Near(camera.Forward(), forward), "pan leaves camera orientation unchanged");
    Check(std::abs(glm::dot(shift, forward)) < 1e-5f, "pitched and yawed pan stays in the camera view plane");
    Check(Near(camera.Focus() - startFocus, shift), "pan translates camera and focus anchor equally");
    Check(glm::length(after - before - glm::vec2(80, -40)) < 2e-3f,
          "perspective projection moves the scene by the requested horizontal and vertical pixels");
    camera.Pan(-80, 40, 800);
    Check(Near(camera.Position(), startPosition), "reverse pan restores camera position without frame-time dependence");
    const auto unchanged = camera.Position();
    camera.Pan(10, 20, 0);
    Check(Near(camera.Position(), unchanged), "zero-height viewport does not move or divide by zero");
    camera.Pan(0, 0, 800);
    Check(Near(camera.Position(), unchanged), "unchanged pointer does not manufacture camera motion");

    EditorCamera close, far, doubledHeight;
    close.SetPose({0, 0, 0}, 0, 0);
    far.SetPose({0, 0, 0}, 0, 0);
    doubledHeight.SetPose({0, 0, 0}, 0, 0);
    close.LookAt({0, 0, 10}, 10);
    far.LookAt({0, 0, 20}, 20);
    doubledHeight.LookAt({0, 0, 10}, 10);
    close.Pan(100, 50, 800);
    far.Pan(100, 50, 800);
    doubledHeight.Pan(100, 50, 1600);
    Check(Near(far.Position(), close.Position() * 2.0f), "pan scale follows framed depth");
    Check(Near(doubledHeight.Position(), close.Position() * 0.5f), "pan scale follows actual viewport pixel height");

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    const auto beforeLook = camera.Forward();
    camera.Update(window, 0.5f, true, 20, -10, false);
    Check(glm::length(camera.Forward() - beforeLook) > 0.01f, "existing right-drag camera look still rotates");
    Check(glm::length(camera.Position() - startPosition) > 5.9f, "existing fly input still translates the camera");
}
void DollyMath() {
    EditorCamera camera;
    camera.SetPose({4, 8, -12}, 37.0f, -26.0f);
    const glm::vec3 target{2, 5, 7};
    camera.LookAt(target, 16.0f);
    const auto position = camera.Position();
    const auto forward = camera.Forward();
    const auto projection = camera.ProjectionMatrix(1.5f);
    camera.Dolly(0.25f);
    Check(std::abs(camera.FocusDistance() - 16.0f * std::pow(0.8f, 0.25f)) < 2e-5f,
          "fractional wheel steps scale focus distance without integer rounding");
    const auto displacement = camera.Position() - position;
    Check(glm::dot(displacement, forward) > 0.0f &&
          glm::length(glm::cross(displacement, forward)) < 2e-5f,
          "forward wheel dollies toward focus along the pitched and yawed view direction");
    Check(Near(camera.Focus(), target), "wheel dolly retains the framed focus anchor");
    Check(Near(camera.Forward(), forward), "wheel dolly preserves camera orientation");
    Check(Near(camera.ProjectionMatrix(1.5f), projection), "wheel dolly preserves perspective lens and field of view");
    camera.Dolly(-0.25f);
    Check(Near(camera.Position(), position) && std::abs(camera.FocusDistance() - 16.0f) < 2e-5f,
          "inverse fractional wheel restores position and distance away from bounds");

    EditorCamera fractional, whole;
    fractional.SetPose({0, 0, 0}, 0, 0); whole.SetPose({0, 0, 0}, 0, 0);
    fractional.LookAt({0, 0, 10}, 10); whole.LookAt({0, 0, 10}, 10);
    fractional.Dolly(0.25f); fractional.Dolly(0.75f); whole.Dolly(1.0f);
    Check(Near(fractional.Position(), whole.Position()) &&
          std::abs(fractional.FocusDistance() - whole.FocusDistance()) < 2e-5f,
          "fractional wheel accumulation agrees with a whole step");
    EditorCamera farther;
    farther.SetPose({0, 0, 0}, 0, 0); farther.LookAt({0, 0, 20}, 20); farther.Dolly(1.0f);
    Check(Near(farther.Position(), whole.Position() * 2.0f), "dolly movement scales with focus distance");

    EditorCamera close;
    close.SetPose({0, 0, 0}, 0, 0); close.LookAt({0, 0, 0}, 1.0f);
    close.Dolly(1.0f);
    const auto closePosition = close.Position();
    close.Dolly(1.0f);
    Check(std::abs(close.FocusDistance() - 0.64f) < 2e-5f &&
          glm::dot(close.Position() - closePosition, close.Forward()) > 0.0f,
          "repeated inward wheel continues approaching close objects with smaller steps");
    close.Dolly(std::numeric_limits<float>::max());
    Check(std::abs(close.FocusDistance() - 0.1f) < 1e-6f && close.Position().z < 0.0f,
          "extreme inward wheel saturates at the near distance without crossing focus");
    const auto nearest = close.Position();
    close.Dolly(1.0f);
    Check(Near(close.Position(), nearest), "additional inward wheel stays at the lower distance bound");
    close.Dolly(-std::numeric_limits<float>::max());
    Check(std::abs(close.FocusDistance() - 5000.0f) < 1e-3f && std::isfinite(close.Position().z),
          "extreme outward wheel saturates at a finite maximum distance");
    const auto farthest = close.Position();
    close.Dolly(-1.0f);
    Check(Near(close.Position(), farthest), "additional outward wheel stays at the upper distance bound");

    const auto validPosition = camera.Position();
    const auto validDistance = camera.FocusDistance();
    camera.Dolly(0.0f);
    camera.Dolly(std::numeric_limits<float>::quiet_NaN());
    camera.Dolly(std::numeric_limits<float>::infinity());
    camera.Dolly(-std::numeric_limits<float>::infinity());
    Check(Near(camera.Position(), validPosition) && camera.FocusDistance() == validDistance,
          "zero and nonfinite wheel values leave the camera unchanged");

    EditorCamera oversizedFrame;
    oversizedFrame.SetPose({0, 0, 0}, 0, 0); oversizedFrame.LookAt({0, 0, 0}, 6000.0f);
    const auto oversizedPosition = oversizedFrame.Position();
    oversizedFrame.Dolly(-1.0f);
    Check(Near(oversizedFrame.Position(), oversizedPosition),
          "outward wheel does not reverse direction when an existing frame exceeds the normal limit");
    oversizedFrame.Dolly(1.0f);
    Check(std::abs(oversizedFrame.FocusDistance() - 4800.0f) < 1e-3f,
          "inward wheel smoothly approaches an existing frame beyond the normal limit");

    camera.Dolly(2.0f);
    const auto zoomPosition = camera.Position();
    const auto zoomFocus = camera.Focus();
    const auto before = Project(camera, target, 1200, 800);
    camera.Pan(80, -40, 800);
    const auto shift = camera.Position() - zoomPosition;
    const auto after = Project(camera, target, 1200, 800);
    Check(Near(camera.Focus() - zoomFocus, shift), "pan after dolly translates the updated focus and camera equally");
    Check(glm::length(after - before - glm::vec2(80, -40)) < 2e-3f,
          "pan after dolly uses the new focus depth for the requested screen-space movement");
    camera.Pan(-80, 40, 800);
    Check(Near(camera.Position(), zoomPosition), "inverse pan after dolly restores the dolly position");
}
void WheelEligibility() {
    EditorCameraGestureInput input;
    Check(EditorCameraCanDolly(input, EditorCameraGesture::None, true),
          "focused editable viewport accepts unclaimed wheel input");
    input.canStart = false;
    Check(!EditorCameraCanDolly(input, EditorCameraGesture::None, true),
          "panel, capture or gizmo eligibility denies wheel dolly");
    input.canStart = true; input.focused = false;
    Check(!EditorCameraCanDolly(input, EditorCameraGesture::None, true), "unfocused window denies wheel dolly");
    input.focused = true; input.cancel = true;
    Check(!EditorCameraCanDolly(input, EditorCameraGesture::None, true), "text input or cancellation denies wheel dolly");
    input.cancel = false;
    Check(!EditorCameraCanDolly(input, EditorCameraGesture::None, false), "Play mode leaves wheel input with the game");
    Check(!EditorCameraCanDolly(input, EditorCameraGesture::Look, true) &&
          !EditorCameraCanDolly(input, EditorCameraGesture::Pan, true),
          "captured look and pan gestures exclude wheel dolly");
    input.leftDown = true;
    Check(!EditorCameraCanDolly(input, EditorCameraGesture::None, true), "held selection button excludes wheel dolly");
    input.leftDown = false; input.rightDown = true;
    Check(!EditorCameraCanDolly(input, EditorCameraGesture::None, true), "held look button excludes wheel dolly");
    input.rightDown = false; input.middleDown = true;
    Check(!EditorCameraCanDolly(input, EditorCameraGesture::None, true), "held pan button excludes wheel dolly");
}
void GestureOwnership() {
    EditorCameraGestureState state;
    EditorCameraGestureInput input;
    input.middleDown = true;
    state.Update(input);
    Check(state.Active() == EditorCameraGesture::Pan, "fresh middle press in viewport owns pan");
    input.canStart = false;
    state.Update(input);
    Check(state.Active() == EditorCameraGesture::Pan, "captured pan remains owned when pointer crosses a panel");
    input.middleDown = false;
    state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "middle release ends pan capture");
    input.middleDown = true;
    state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "middle press begun over a panel cannot start pan");
    input.canStart = true;
    state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "drag out of panel does not steal held middle press");
    input.middleDown = false; state.Update(input);
    input.middleDown = true; state.Update(input);
    input.focused = false; state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "window focus loss cancels an active pan");
    input.focused = true; state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "focus regain with middle held cannot reacquire capture");
    input.middleDown = false; state.Update(input);
    input.middleDown = true; state.Update(input);
    Check(state.Active() == EditorCameraGesture::Pan, "new press after focus loss restores pan normally");
    input.cancel = true; state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "text-input claim or Escape cancellation releases pan");
    input.cancel = false; state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "cancelled held press cannot restart after text input ends");
    input.middleDown = false; state.Update(input);
    input.middleDown = true; input.leftDown = true; state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "left-button selection or gizmo drag excludes camera pan");
    input.middleDown = false; input.leftDown = false; state.Update(input);
    input.middleDown = true; input.canStart = false; state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "active gizmo or text field denies a fresh camera press");
    input.middleDown = false; input.canStart = true; state.Update(input);
    input.rightDown = true; state.Update(input);
    Check(state.Active() == EditorCameraGesture::Look, "fresh right press retains existing look binding");
    input.middleDown = true; state.Update(input);
    Check(state.Active() == EditorCameraGesture::Look, "second button does not replace an active right-drag gesture");
    input.rightDown = false; state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "right release ends look without switching to already-held middle");
    state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "already-held middle does not start on the following frame");
    input.middleDown = false; state.Update(input);
    input.middleDown = true; state.Update(input);
    state.Cancel();
    Check(state.Active() == EditorCameraGesture::None, "Play or Stop cancellation retires viewport ownership");
    state.Update(input);
    Check(state.Active() == EditorCameraGesture::None, "held press after Play or Stop cannot resume old navigation");
}
}
int main() {
    PanMath();
    DollyMath();
    WheelEligibility();
    GestureOwnership();
    std::printf("M71 editor navigation: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
