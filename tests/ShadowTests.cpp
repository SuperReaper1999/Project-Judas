// Milestone 15: standalone, headless tests for the shadow light-view/
// projection transform math (src/ShadowTransforms.*) — pure CPU matrix
// construction, no window, no GL context. Mirrors tests/LightingTests.cpp's
// own header comment: actual GLSL shadow-mapping correctness (does the
// real fragment shader's PCF/bias/shadow-map sampling produce a correct-
// looking shadow) is verified by a one-time offscreen-rendering spot-check
// plus human visual validation instead — see docs/ARCHITECTURE.md,
// "Milestone 15, Automated evidence." Likewise, shadow FBO/depth-texture
// creation and destruction (Renderer::Init/Shutdown) needs a real GL
// context to exercise at all, so it is covered the same way, not by a
// headless unit test — consistent with every prior milestone's own
// GL-resource-lifecycle testing approach (see tests/UITests.cpp/
// tests/LightingTests.cpp's own equivalent notes).
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ShadowTransforms.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        std::printf("  OK   %s\n", description);
    } else {
        std::printf("  FAIL %s\n", description);
        ++g_failures;
    }
}

void CheckNear(float a, float b, float tolerance, const char* description) {
    Check(std::abs(a - b) <= tolerance, description);
}

// ChooseShadowUpHint (see ShadowTransforms.cpp) picks its "up" reference
// from FIXED world +Y, not from anything that rotates along with a
// scenario — so rotating an entire scenario's light/points and rebuilding
// the shadow matrix from scratch generally produces a camera basis that
// differs from "the original basis, rotated" by an extra ROLL (rotation
// around the light's own forward axis): a real, harmless degree of
// freedom shadow mapping never depends on (it's a depth comparison, not
// an image a person views "the right way up"). What must stay invariant
// under a full-scenario rotation is therefore not the exact NDC (x, y) —
// which a roll difference can freely change — but the DEPTH (how far
// along the light's own view axis the point is) and the RADIAL offset
// from the frustum's own center axis (how far off-axis the point is,
// regardless of which direction "off" points) — both are roll-
// independent, and both are exactly the quantities shadow comparison and
// spotlight-cone coverage actually depend on.
void CheckClipSpaceInvariantUnderRoll(const glm::vec4& reference, const glm::vec4& rotated,
                                       float tolerance, const char* description) {
    const float referenceDepth = reference.z / reference.w;
    const float rotatedDepth = rotated.z / rotated.w;
    const float referenceRadius = glm::length(glm::vec2(reference) / reference.w);
    const float rotatedRadius = glm::length(glm::vec2(rotated) / rotated.w);
    Check(std::abs(referenceDepth - rotatedDepth) <= tolerance &&
              std::abs(referenceRadius - rotatedRadius) <= tolerance,
          description);
}

bool IsFiniteMatrix(const glm::mat4& m) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(m[col][row])) return false;
        }
    }
    return true;
}

// Same role as every other standalone suite's own kArbitraryRotation.
const glm::quat kArbitraryRotation =
    glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f)));

}  // namespace

// --- Section A: light view/projection transform construction ---
void TestDirectionalShadowMatrixIsFinite() {
    std::printf("Section A: directional shadow matrix construction\n");

    const glm::mat4 m = ComputeDirectionalShadowMatrix(glm::vec3(0.0f, 20.0f, 0.0f),
                                                        glm::normalize(glm::vec3(0.4f, 0.7f, 0.35f)),
                                                        25.0f, 40.0f);
    Check(IsFiniteMatrix(m), "a typical directional shadow matrix has no NaN/Inf entries");

    // A point exactly at the shadow frustum's own focus should land at
    // NDC (0, 0) horizontally/vertically (dead center of the frustum) —
    // the whole point of "recentered on the focus position."
    const glm::vec4 clip = m * glm::vec4(0.0f, 20.0f, 0.0f, 1.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    CheckNear(ndc.x, 0.0f, 1.0e-3f, "the focus point itself projects to NDC x = 0 (frustum center)");
    CheckNear(ndc.y, 0.0f, 1.0e-3f, "the focus point itself projects to NDC y = 0 (frustum center)");
}

void TestSpotShadowMatrixIsFinite() {
    std::printf("Section A: spotlight shadow matrix construction\n");

    const glm::mat4 m = ComputeSpotShadowMatrix(glm::vec3(1.0f, 2.0f, 3.0f),
                                                glm::normalize(glm::vec3(0.0f, -0.2f, -1.0f)), 22.0f,
                                                40.0f);
    Check(IsFiniteMatrix(m), "a typical spotlight shadow matrix has no NaN/Inf entries");

    // A point straight ahead of the light, well within its cone, should
    // land near NDC (0, 0) and within the [-1, 1] depth range.
    const glm::vec3 lightPos(1.0f, 2.0f, 3.0f);
    const glm::vec3 dir = glm::normalize(glm::vec3(0.0f, -0.2f, -1.0f));
    const glm::vec4 clip = m * glm::vec4(lightPos + dir * 10.0f, 1.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    CheckNear(ndc.x, 0.0f, 1.0e-3f, "a point straight ahead of the spotlight projects near NDC x = 0");
    CheckNear(ndc.y, 0.0f, 1.0e-3f, "a point straight ahead of the spotlight projects near NDC y = 0");
    Check(ndc.z >= -1.0f && ndc.z <= 1.0f, "a point within range projects within the valid depth range");
}

// --- Section D (finite matrices at valid + edge-case configurations) ---
void TestFiniteMatricesAtEdgeCaseConfigurations() {
    std::printf("Section D: finite matrices at edge-case light configurations\n");

    // A light direction nearly parallel to world +Y — the one case
    // ChooseShadowUpHint has to fall back away from its usual +Y
    // reference (see ShadowTransforms.cpp) to avoid a near-singular
    // camera basis.
    Check(IsFiniteMatrix(ComputeDirectionalShadowMatrix(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                                                        25.0f, 40.0f)),
          "directional matrix stays finite when the light direction is exactly world +Y");
    Check(IsFiniteMatrix(ComputeSpotShadowMatrix(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 20.0f, 30.0f)),
          "spotlight matrix stays finite when the light direction is exactly world -Y");

    // A very narrow cone and a very wide one.
    Check(IsFiniteMatrix(ComputeSpotShadowMatrix(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), 1.0f, 10.0f)),
          "spotlight matrix stays finite at a very narrow (1 degree) outer cone");
    Check(IsFiniteMatrix(ComputeSpotShadowMatrix(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), 89.0f, 10.0f)),
          "spotlight matrix stays finite at a very wide (89 degree) outer cone");

    // A very small range/distance.
    Check(IsFiniteMatrix(ComputeSpotShadowMatrix(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), 20.0f, 0.001f)),
          "spotlight matrix stays finite at a near-zero range");
    Check(IsFiniteMatrix(ComputeDirectionalShadowMatrix(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 25.0f,
                                                        0.001f)),
          "directional matrix stays finite at a near-zero shadow distance");
}

// --- Section C: rotate-the-universe invariance ---
//
// If the ENTIRE scenario (light position/direction, focus/target point,
// AND the world point being tested) is rotated by the same arbitrary
// quaternion, the resulting light-space (NDC) coordinates of that point
// must be UNCHANGED — the whole transform is about relative geometry
// between the light and the point, never an absolute world direction.
// This is also exactly what "the spotlight shadow transform follows the
// player/spacecraft pose" means in practice: the shadow matrix is rebuilt
// from whatever the light's current position/direction actually are, with
// no hidden reference to a fixed axis.
void TestDirectionalRotateTheUniverseInvariance() {
    std::printf("Section C: directional shadow matrix rotate-the-universe invariance\n");

    const glm::vec3 focus(2.0f, 5.0f, -3.0f);
    const glm::vec3 dirToLight = glm::normalize(glm::vec3(0.3f, 0.6f, 0.2f));  // not near +/-Y
    const glm::vec3 testPoint = focus + glm::vec3(3.0f, 1.0f, -2.0f);

    const glm::mat4 reference = ComputeDirectionalShadowMatrix(focus, dirToLight, 25.0f, 40.0f);
    const glm::vec4 referenceClip = reference * glm::vec4(testPoint, 1.0f);

    const glm::vec3 rotatedFocus = kArbitraryRotation * focus;
    const glm::vec3 rotatedDir = kArbitraryRotation * dirToLight;
    const glm::vec3 rotatedTestPoint = kArbitraryRotation * testPoint;
    const glm::mat4 rotated = ComputeDirectionalShadowMatrix(rotatedFocus, rotatedDir, 25.0f, 40.0f);
    const glm::vec4 rotatedClip = rotated * glm::vec4(rotatedTestPoint, 1.0f);

    CheckClipSpaceInvariantUnderRoll(
        referenceClip, rotatedClip, 1.0e-3f,
        "rotating the whole scenario (focus, light direction, and the test point) leaves the "
        "point's depth and off-axis radius unchanged — no hidden world-axis dependency beyond an "
        "expected, harmless roll (see CheckClipSpaceInvariantUnderRoll's own comment)");
}

void TestSpotRotateTheUniverseInvariance() {
    std::printf("Section C: spotlight shadow matrix rotate-the-universe invariance\n");

    const glm::vec3 lightPos(1.0f, 4.0f, -2.0f);
    const glm::vec3 dir = glm::normalize(glm::vec3(0.2f, -0.5f, 0.8f));  // not near +/-Y
    const glm::vec3 testPoint = lightPos + dir * 8.0f + glm::vec3(0.5f, 0.2f, -0.3f);

    const glm::mat4 reference = ComputeSpotShadowMatrix(lightPos, dir, 22.0f, 40.0f);
    const glm::vec4 referenceClip = reference * glm::vec4(testPoint, 1.0f);

    const glm::vec3 rotatedLightPos = kArbitraryRotation * lightPos;
    const glm::vec3 rotatedDir = kArbitraryRotation * dir;
    const glm::vec3 rotatedTestPoint = kArbitraryRotation * testPoint;
    const glm::mat4 rotated = ComputeSpotShadowMatrix(rotatedLightPos, rotatedDir, 22.0f, 40.0f);
    const glm::vec4 rotatedClip = rotated * glm::vec4(rotatedTestPoint, 1.0f);

    CheckClipSpaceInvariantUnderRoll(
        referenceClip, rotatedClip, 1.0e-3f,
        "rotating the whole scenario leaves the spotlight shadow-space depth/off-axis-radius "
        "unchanged — this is what makes the torch/headlight shadow follow arbitrary pitch/yaw/roll "
        "correctly (see docs/ARCHITECTURE.md, 'Milestone 15, Spotlight shadows')");
}

// --- Section B (spotlight shadow transform following a moving/rotating pose) ---
void TestSpotShadowFollowsMovingLight() {
    std::printf("Section B: spotlight shadow transform follows the light's own current pose\n");

    const glm::vec3 dir(0.0f, 0.0f, -1.0f);
    const glm::vec3 worldPoint(0.0f, 0.0f, -20.0f);  // a fixed point in the world

    // The light starts far from worldPoint...
    const glm::mat4 far = ComputeSpotShadowMatrix(glm::vec3(0.0f, 0.0f, 10.0f), dir, 20.0f, 60.0f);
    const glm::vec4 farClip = far * glm::vec4(worldPoint, 1.0f);
    const glm::vec3 farNdc = glm::vec3(farClip) / farClip.w;

    // ...then moves closer (simulating the spacecraft translating toward
    // it): the SAME world point should now sit further along the light's
    // own depth range (a larger NDC z, since it's relatively closer to
    // the far plane than before — the point is fixed but the light moved
    // toward it, closer to the near plane this time).
    const glm::mat4 near = ComputeSpotShadowMatrix(glm::vec3(0.0f, 0.0f, -5.0f), dir, 20.0f, 60.0f);
    const glm::vec4 nearClip = near * glm::vec4(worldPoint, 1.0f);
    const glm::vec3 nearNdc = glm::vec3(nearClip) / nearClip.w;

    Check(std::abs(nearNdc.x) < 1.0e-3f && std::abs(nearNdc.y) < 1.0e-3f,
          "the fixed world point stays centered (NDC x=y=0) as the light moves directly toward it "
          "along its own forward axis");
    Check(std::abs(farNdc.z - nearNdc.z) > 1.0e-4f,
          "moving the light changes the SAME world point's projected depth — the shadow transform "
          "genuinely tracks the light's current position, not a cached/stale one");
}

int main() {
    TestDirectionalShadowMatrixIsFinite();
    TestSpotShadowMatrixIsFinite();
    TestFiniteMatricesAtEdgeCaseConfigurations();
    TestDirectionalRotateTheUniverseInvariance();
    TestSpotRotateTheUniverseInvariance();
    TestSpotShadowFollowsMovingLight();

    if (g_failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
