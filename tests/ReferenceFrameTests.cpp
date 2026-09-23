#include "ReferenceFrame.h"

#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"
#include "PilotAttachment.h"

namespace {
int gFailures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::printf("  OK   %s\n", message);
    } else {
        std::fprintf(stderr, "  FAIL %s\n", message);
        ++gFailures;
    }
}

void CheckVecNear(const glm::vec3& actual, const glm::vec3& expected, float tolerance,
                  const char* message) {
    Check(glm::length(actual - expected) <= tolerance, message);
}

bool IsFinite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

glm::vec3 IndependentCross(const glm::vec3& a, const glm::vec3& b) {
    return glm::vec3(a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x);
}

const glm::quat kFrameRotation = glm::normalize(
    glm::angleAxis(0.83f, glm::normalize(glm::vec3(2.0f, -1.0f, 3.0f))));

void TestStationaryAndTranslatedFrames() {
    std::printf("Section A: stationary and translating frames\n");
    const ReferenceFrame worldFrame;
    const glm::vec3 worldPosition(4.0f, -7.0f, 2.5f);
    const glm::vec3 worldVelocity(-3.0f, 1.25f, 9.0f);
    CheckVecNear(PositionFromWorld(worldFrame, worldPosition), worldPosition, 1.0e-6f,
                 "identity stationary frame leaves positions unchanged");
    CheckVecNear(DirectionFromWorld(worldFrame, worldVelocity), worldVelocity, 1.0e-6f,
                 "identity stationary frame leaves directions unchanged");
    CheckVecNear(VelocityFromWorld(worldFrame, worldPosition, worldVelocity), worldVelocity,
                 1.0e-6f, "world velocity is relative velocity in a stationary world frame");

    const ReferenceFrame translating{glm::vec3(12.0f, -4.0f, 7.0f),
                                     glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                     glm::vec3(3.0f, 4.0f, 5.0f), glm::vec3(0.0f)};
    const glm::vec3 localPosition(-2.0f, 1.0f, 4.0f);
    const glm::vec3 localVelocity(2.0f, -1.0f, 0.5f);
    const glm::vec3 expectedWorldPosition(10.0f, -3.0f, 11.0f);
    const glm::vec3 expectedWorldVelocity(5.0f, 3.0f, 5.5f);
    CheckVecNear(PositionToWorld(translating, localPosition), expectedWorldPosition, 1.0e-6f,
                 "translated frame adds its origin to frame position");
    CheckVecNear(VelocityToWorld(translating, localPosition, localVelocity),
                 expectedWorldVelocity, 1.0e-6f,
                 "translating frame adds its linear velocity to local velocity");
    CheckVecNear(PositionFromWorld(translating, expectedWorldPosition), localPosition, 1.0e-6f,
                 "translated position round-trips");
    CheckVecNear(VelocityFromWorld(translating, expectedWorldPosition, expectedWorldVelocity),
                 localVelocity, 1.0e-6f, "translated velocity round-trips");
}

void TestRotatedFrameRoundTrips() {
    std::printf("Section B: arbitrary rotated-frame transforms and round trips\n");
    const ReferenceFrame frame{glm::vec3(-8.0f, 2.0f, 11.0f), kFrameRotation,
                               glm::vec3(6.0f, -2.0f, 4.0f), glm::vec3(0.0f)};
    const glm::vec3 localPosition(3.5f, -2.25f, 0.75f);
    const glm::vec3 localDirection(-0.2f, 0.9f, 0.4f);
    const glm::vec3 localVelocity(1.5f, -4.0f, 2.25f);
    const glm::vec3 worldPosition = PositionToWorld(frame, localPosition);
    const glm::vec3 worldDirection = DirectionToWorld(frame, localDirection);
    const glm::vec3 worldVelocity = VelocityToWorld(frame, localPosition, localVelocity);
    const glm::vec3 restoredPosition = PositionFromWorld(frame, worldPosition);
    const glm::vec3 restoredDirection = DirectionFromWorld(frame, worldDirection);
    const glm::vec3 restoredVelocity = VelocityFromWorld(frame, worldPosition, worldVelocity);

    CheckVecNear(restoredPosition, localPosition, 2.0e-5f,
                 "world-to-frame-to-world position round trip is stable");
    CheckVecNear(restoredDirection, localDirection, 2.0e-5f,
                 "world-to-frame-to-world direction round trip is stable");
    CheckVecNear(restoredVelocity, localVelocity, 2.0e-5f,
                 "world-to-frame-to-world velocity round trip is stable");
    Check(IsFinite(worldPosition) && IsFinite(worldDirection) && IsFinite(worldVelocity) &&
              IsFinite(restoredPosition) && IsFinite(restoredDirection) &&
              IsFinite(restoredVelocity) &&
              IsFinite(FramePointVelocity(frame, worldPosition)) &&
              IsFinite(RelativeVelocityToFrame(frame, worldPosition, worldVelocity)),
          "arbitrary valid rotated-frame conversions remain finite");
}

void TestRotatingFramePointVelocity() {
    std::printf("Section C: rotating-frame point velocity includes omega cross r\n");
    const ReferenceFrame frame{glm::vec3(5.0f, -3.0f, 8.0f), kFrameRotation,
                               glm::vec3(4.0f, 1.0f, -2.0f),
                               glm::vec3(0.8f, -1.1f, 0.45f)};
    const glm::vec3 localOffset(2.0f, -0.5f, 3.0f);
    const glm::vec3 worldPoint = frame.originPosition + frame.orientation * localOffset;
    const glm::vec3 r = worldPoint - frame.originPosition;
    const glm::vec3 expectedPointVelocity = frame.linearVelocity +
                                            IndependentCross(frame.angularVelocity, r);
    const glm::vec3 measuredPointVelocity = FramePointVelocity(frame, worldPoint);
    CheckVecNear(measuredPointVelocity, expectedPointVelocity, 1.0e-6f,
                 "frame point velocity equals translation plus independently evaluated omega cross r");

    const glm::vec3 objectVelocity = expectedPointVelocity;
    CheckVecNear(RelativeVelocityToFrame(frame, worldPoint, objectVelocity), glm::vec3(0.0f),
                 1.0e-6f, "object moving with a rotating-frame point has zero relative velocity");
    CheckVecNear(VelocityFromWorld(frame, worldPoint, objectVelocity), glm::vec3(0.0f),
                 1.0e-6f, "rotating-frame velocity conversion removes point rotation speed");
    Check(glm::length(measuredPointVelocity) > 0.1f,
          "a stationary-in-frame offset point can have substantial world velocity");
}

void TestEqualWorldVelocitiesInMovingFrame() {
    std::printf("Section D: equal world velocities imply zero relative motion\n");
    const ReferenceFrame frame{glm::vec3(-2.0f, 6.0f, 1.0f), kFrameRotation,
                               glm::vec3(100.0f, -20.0f, 5.0f), glm::vec3(0.0f)};
    const glm::vec3 positionA(8.0f, -2.0f, 3.0f);
    const glm::vec3 positionB(-4.0f, 7.0f, 11.0f);
    const glm::vec3 sharedWorldVelocity(100.0f, -20.0f, 5.0f);
    const glm::vec3 velocityA = VelocityFromWorld(frame, positionA, sharedWorldVelocity);
    const glm::vec3 velocityB = VelocityFromWorld(frame, positionB, sharedWorldVelocity);
    CheckVecNear(velocityA, glm::vec3(0.0f), 2.0e-5f,
                 "object matching a translating frame has zero frame-relative velocity");
    CheckVecNear(velocityA - velocityB, glm::vec3(0.0f), 2.0e-5f,
                 "two objects with identical world velocity have zero relative velocity");

    const glm::vec3 changedWorldVelocity(96.0f, -17.0f, 8.0f);
    const glm::vec3 expectedDifference = glm::inverse(frame.orientation) *
                                        (sharedWorldVelocity - changedWorldVelocity);
    const glm::vec3 changedRelative = VelocityFromWorld(frame, positionB, changedWorldVelocity);
    CheckVecNear(velocityB - changedRelative, expectedDifference, 2.0e-5f,
                 "unequal velocities preserve their correctly rotated relative result");
}

ReferenceFrame FrameFromBody(const PhysicsWorld& physics, BodyHandle body) {
    const BodyTransform transform = physics.GetTransform(body);
    return ReferenceFrame{transform.position, transform.rotation,
                          physics.GetLinearVelocity(body), physics.GetAngularVelocity(body)};
}

void TestPhysicsBodyRelativeVelocity() {
    std::printf("Section E: spacecraft and pilot motion relative to live physics bodies\n");
    PhysicsWorld physics;
    Check(physics.Init(), "reference-frame physics world initializes");

    const glm::vec3 planetPosition(-6.0f, 3.0f, 4.0f);
    const glm::quat planetOrientation = kFrameRotation;
    const glm::vec3 planetVelocity(8.0f, -3.0f, 2.0f);
    const glm::vec3 planetAngularVelocity(0.2f, -0.6f, 1.1f);
    const BodyHandle planet = physics.CreateDynamicSphere(planetPosition, 3.0f, 1.0e12f, 0.0f, 0.0f);
    physics.ResetBody(planet, planetPosition, planetOrientation);
    physics.SetLinearVelocity(planet, planetVelocity);
    physics.SetAngularVelocity(planet, planetAngularVelocity);

    const glm::vec3 shipPositionInFrame(4.0f, 2.0f, -1.0f);
    const glm::vec3 expectedShipRelativeVelocity(3.0f, -2.0f, 0.5f);
    const glm::vec3 shipOffset = planetOrientation * shipPositionInFrame;
    const glm::vec3 expectedShipWorldVelocity = planetVelocity +
        IndependentCross(planetAngularVelocity, shipOffset) +
        planetOrientation * expectedShipRelativeVelocity;
    const glm::vec3 shipWorldPosition = planetPosition + shipOffset;
    const BodyHandle ship = physics.CreateDynamicBox(shipWorldPosition, glm::vec3(1.0f),
                                                     80.0f, 0.0f, 0.0f);
    const glm::quat shipOrientation = glm::normalize(
        glm::angleAxis(0.47f, glm::normalize(glm::vec3(1.0f, 3.0f, -2.0f))));
    physics.ResetBody(ship, shipWorldPosition, shipOrientation);
    physics.SetLinearVelocity(ship, expectedShipWorldVelocity);
    physics.SetAngularVelocity(ship, glm::vec3(-0.35f, 0.8f, 0.6f));

    const ReferenceFrame planetFrame = FrameFromBody(physics, planet);
    Check(glm::length(physics.GetLinearVelocity(planet)) > 1.0f &&
              glm::length(physics.GetLinearVelocity(ship)) > 1.0f,
          "both celestial body and spacecraft retain nonzero world velocity");
    CheckVecNear(RelativeVelocityToFrame(planetFrame, physics.GetTransform(ship).position,
                                         physics.GetLinearVelocity(ship)),
                 expectedShipRelativeVelocity, 3.0e-5f,
                 "spacecraft velocity is reported relative to a moving, rotating celestial body");

    const glm::vec3 pilotLocalOffset(0.3f, 1.2f, -0.5f);
    const ReferenceFrame shipFrame = FrameFromBody(physics, ship);
    const glm::vec3 pilotWorldPosition = PositionToWorld(shipFrame, pilotLocalOffset);
    PilotAttachment attachment;
    BeginPilotAttachment(attachment, physics.GetTransform(ship), pilotWorldPosition,
                         physics.GetTransform(ship).rotation);
    glm::vec3 attachedPosition;
    glm::quat attachedOrientation;
    ApplyPilotAttachment(attachment, physics.GetTransform(ship), attachedPosition,
                         attachedOrientation);
    const glm::vec3 attachedPilotWorldVelocity =
        VelocityToWorld(shipFrame, attachment.localOffset, glm::vec3(0.0f));
    const glm::vec3 releaseVelocity = ComputePilotReleaseVelocity(
        physics.GetTransform(ship), physics.GetLinearVelocity(ship),
        physics.GetAngularVelocity(ship), attachedPosition);
    CheckVecNear(attachedPilotWorldVelocity, releaseVelocity, 3.0e-5f,
                 "attached pilot world velocity matches spacecraft point velocity used on release");
    CheckVecNear(RelativeVelocityToFrame(shipFrame, attachedPosition,
                                         attachedPilotWorldVelocity), glm::vec3(0.0f),
                 3.0e-5f, "attached pilot is stationary in the moving spacecraft frame");
    Check(glm::length(attachedPilotWorldVelocity) > 1.0f,
          "attached pilot can be stationary relative to a spacecraft moving in world space");

    physics.ResetBody(planet, planetPosition, planetOrientation);
    physics.ResetBody(ship, shipWorldPosition, shipOrientation);
    const ReferenceFrame resetPlanetFrame = FrameFromBody(physics, planet);
    Check(glm::length(resetPlanetFrame.linearVelocity) == 0.0f &&
              glm::length(resetPlanetFrame.angularVelocity) == 0.0f &&
              glm::length(physics.GetLinearVelocity(ship)) == 0.0f,
          "reset physics state produces fresh zero-speed frame telemetry");
    physics.Shutdown();
}

void TestRotateTheUniverseEquivalence() {
    std::printf("Section F: rotate-the-universe reference-frame equivalence\n");
    const ReferenceFrame frame{glm::vec3(7.0f, -5.0f, 12.0f), kFrameRotation,
                               glm::vec3(5.0f, -1.0f, 3.0f),
                               glm::vec3(-0.5f, 0.9f, 1.3f)};
    const glm::vec3 localPosition(-1.0f, 2.5f, 4.0f);
    const glm::vec3 localVelocity(0.7f, -2.0f, 1.1f);
    const glm::vec3 position = PositionToWorld(frame, localPosition);
    const glm::vec3 velocity = VelocityToWorld(frame, localPosition, localVelocity);
    const glm::quat universeRotation = glm::normalize(
        glm::angleAxis(1.17f, glm::normalize(glm::vec3(-2.0f, 4.0f, 1.0f))));

    const ReferenceFrame rotatedFrame{
        universeRotation * frame.originPosition,
        glm::normalize(universeRotation * frame.orientation),
        universeRotation * frame.linearVelocity,
        universeRotation * frame.angularVelocity};
    const glm::vec3 rotatedPosition = PositionToWorld(rotatedFrame, localPosition);
    const glm::vec3 rotatedVelocity = VelocityToWorld(rotatedFrame, localPosition, localVelocity);
    CheckVecNear(glm::inverse(universeRotation) * rotatedPosition, position, 3.0e-5f,
                 "rotating frame, origin, and point rotates the world position identically");
    CheckVecNear(glm::inverse(universeRotation) * rotatedVelocity, velocity, 3.0e-5f,
                 "rotating frame and all velocities preserves frame-relative velocity");
    CheckVecNear(RelativeVelocityToFrame(rotatedFrame, rotatedPosition, rotatedVelocity),
                 localVelocity, 3.0e-5f,
                 "rotated scenario reports the same local relative velocity");
}
}  // namespace

int main() {
    TestStationaryAndTranslatedFrames();
    TestRotatedFrameRoundTrips();
    TestRotatingFramePointVelocity();
    TestEqualWorldVelocitiesInMovingFrame();
    TestPhysicsBodyRelativeVelocity();
    TestRotateTheUniverseEquivalence();
    if (gFailures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d test(s) failed\n", gFailures);
    return 1;
}
