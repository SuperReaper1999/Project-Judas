#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/gtc/quaternion.hpp>

#include "BoxVolume.h"
#include "CelestialGravity.h"
#include "DynamicBody.h"
#include "FaithfulGravity.h"
#include "GravityContextMap.h"
#include "RadicalGravity.h"
#include "ReferenceFrame.h"
#include "ShadowTransforms.h"
#include "SphericalVolume.h"
#include "Window.h"
#include "WorldCoordinates.h"

namespace {
constexpr float kDt = 1.0f / 60.0f;
const glm::dvec3 kFarOrigin(1.0e9, -2.0e9, 3.0e9);
int failures = 0;

void Check(bool condition, const char* label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label);
    if (!condition) ++failures;
}

bool Near(glm::vec3 a, glm::vec3 b, float tolerance) {
    return glm::length(a - b) <= tolerance;
}

bool Finite(glm::vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// The source position is an ABSOLUTE double coordinate. This conversion
// deliberately crosses the real global/local boundary before every scenario
// is handed to the existing float simulation.
glm::vec3 Placed(const WorldCoordinates& coordinates, glm::vec3 authoredLocal) {
    return coordinates.ToLocal(coordinates.Origin() + glm::dvec3(authoredLocal));
}

double RunSlowBody(const WorldCoordinates& coordinates) {
    PhysicsWorld physics;
    physics.Init();
    const glm::vec3 start = Placed(coordinates, glm::vec3(2.0f, -3.0f, 4.0f));
    const BodyHandle body = physics.CreateDynamicSphere(start, 0.2f, 1.0f, 0.0f, 0.0f);
    physics.SetLinearVelocity(body, glm::vec3(0.04f, 0.0f, 0.0f));
    for (int step = 0; step < 60; ++step) physics.Step(kDt);
    const double displacement = coordinates.ToGlobal(physics.GetTransform(body).position).x -
                                coordinates.ToGlobal(start).x;
    physics.Shutdown();
    return displacement;
}

glm::vec3 SampleContext(const WorldCoordinates& coordinates, glm::vec3 localShift,
                        glm::vec3 sampleFromCenter) {
    const glm::vec3 center = Placed(coordinates, localShift + glm::vec3(3, -2, 5));
    RadicalGravity radial(center, 9.81f);
    FaithfulGravity uniform;
    const SphericalVolume sphere(center, 5.0f);
    const BoxVolume box(center + glm::vec3(0, 6, 0), glm::vec3(2, 1, 2));
    GravityContextMap contexts;
    contexts.AddRegion(uniform, box);
    contexts.AddRegion(radial, sphere);
    return contexts.Sample(center + sampleFromCenter);
}

struct BodyResult {
    glm::vec3 position;
    glm::vec3 velocity;
    float contactDistance = 0.0f;
};

BodyResult RunBody(const WorldCoordinates& coordinates, const glm::quat& rotation,
                   glm::vec3 localShift = glm::vec3(0.0f)) {
    PhysicsWorld physics;
    Check(physics.Init(), "physics initializes");
    const glm::vec3 center = Placed(coordinates, localShift +
                                     rotation * glm::vec3(6.0f, -4.0f, 9.0f));
    const glm::vec3 direction = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    physics.CreateStaticSphere(center, 2.0f, 0.8f, 0.0f);
    const BodyHandle body = physics.CreateDynamicSphere(center + direction * 3.0f,
                                                        0.5f, 2.0f, 0.4f, 0.0f);
    const glm::vec3 acceleration = rotation * glm::vec3(0.0f, -9.81f, 0.0f);
    for (int step = 0; step < 120; ++step) {
        physics.ApplyLinearAcceleration(body, acceleration, kDt);
        physics.Step(kDt);
    }
    const BodyResult result{physics.GetTransform(body).position,
                            physics.GetLinearVelocity(body),
                            glm::length(physics.GetTransform(body).position - center)};
    physics.Shutdown();
    return result;
}

} // namespace

int main() {
    const WorldCoordinates near;
    const WorldCoordinates far(kFarOrigin);
    const glm::vec3 centimetres(0.01f, -0.02f, 0.03f);
    std::printf("centimetre round-trip error: %.9g m\n",
                glm::length(far.ToLocal(far.ToGlobal(centimetres)) - centimetres));
    Check(Near(far.ToLocal(far.ToGlobal(centimetres)), centimetres, 3.0e-7f),
          "centimetre displacement survives a billion-metre world offset");
    Check(glm::vec3(kFarOrigin + glm::dvec3(centimetres)) == glm::vec3(kFarOrigin),
          "direct float conversion is diagnosed");
    const double nearSlowDistance = RunSlowBody(near);
    const double farSlowDistance = RunSlowBody(far);
    std::printf("slow-body displacement near/far: %.9g / %.9g m (expected 0.04)\n",
                nearSlowDistance, farSlowDistance);
    Check(std::abs(nearSlowDistance - 0.04) < 1.0e-5 &&
          std::abs(farSlowDistance - nearSlowDistance) < 1.0e-6,
          "4 cm of authoritative motion survives billion-metre translation");

    const glm::quat identity(1, 0, 0, 0);
    const glm::quat rotation = glm::angleAxis(0.83f, glm::normalize(glm::vec3(1, -2, 3)));
    const glm::vec3 smallShift(32.0f, -48.0f, 16.0f);
    const BodyResult localBody = RunBody(near, identity);
    const BodyResult smallShiftBody = RunBody(near, identity, smallShift);
    const BodyResult farBody = RunBody(far, identity);
    Check(Near(smallShiftBody.position - smallShift, localBody.position, 0.003f) &&
          Near(smallShiftBody.velocity, localBody.velocity, 0.003f),
          "raw float rigid-body physics has no special zero at small translation");
    Check(Near(localBody.position, farBody.position, 1.0e-4f) &&
          Near(localBody.velocity, farBody.velocity, 1.0e-4f) &&
          std::abs(localBody.contactDistance - farBody.contactDistance) < 1.0e-4f &&
          localBody.contactDistance >= 2.49f,
          "rigid-body integration and contact match after global translation");
    const BodyResult rotatedBody = RunBody(far, rotation);
    Check(Near(glm::inverse(rotation) * rotatedBody.position, localBody.position, 0.03f) &&
          Near(glm::inverse(rotation) * rotatedBody.velocity, localBody.velocity, 0.03f),
          "rigid-body result matches after combined rotation and translation");

    RadicalGravity nearGravity(Placed(near, glm::vec3(3, -4, 5)), 9.81f);
    RadicalGravity farGravity(Placed(far, glm::vec3(3, -4, 5)), 9.81f);
    Check(Near(nearGravity.Sample(Placed(near, glm::vec3(7, 2, 9))),
               farGravity.Sample(Placed(far, glm::vec3(7, 2, 9))), 1.0e-5f),
          "local radial gravity matches after large translation");
    for (const glm::vec3 sample : {glm::vec3(1, 2, 1), glm::vec3(0, 6, 0),
                                   glm::vec3(5, 0, 0), glm::vec3(5.01f, 0, 0),
                                   glm::vec3(30, 0, 0)}) {
        Check(Near(SampleContext(near, glm::vec3(0), sample),
                   SampleContext(near, smallShift, sample), 1.0e-5f) &&
              Near(SampleContext(near, glm::vec3(0), sample),
                   SampleContext(far, glm::vec3(0), sample), 1.0e-5f),
              "gravity context and boundary classification survive translation");
    }

    std::printf("SUMMARY %d failures\n", failures);
    return failures?1:0;
}
