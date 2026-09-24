// Static radial terrain uses the same player sweep and rigid contact paths
// as every other ordinary PhysicsWorld body. No world-up direction is passed
// into either query or solver.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "PhysicsWorld.h"
#include "PlayerController.h"
#include "RadialTerrain.h"
#include "RadicalGravity.h"
#include "TerrainDemo.h"
#include "Window.h"

namespace {
int failures = 0;

void Check(bool condition, const char* description) {
    if (!condition) {
        std::printf("  FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance) {
    return glm::length(a - b) <= tolerance;
}

std::shared_ptr<const RadialTerrain> MakeSlope() {
    return std::make_shared<RadialTerrain>(20.0f,
        [](const glm::vec3& direction) { return 2.0f * direction.x; }, 2.0f);
}

void TestAuthoredGeometry() {
    const auto terrain = TerrainDemo::CreateSurface();
    const std::array<glm::vec2, 5> locations{{
        {-4.0f, 3.0f}, {0.0f, 3.0f}, {5.0f, 3.0f},
        {-17.0f, 8.0f}, {16.0f, -7.0f}}};
    float minimumRadius = 1.0e9f;
    float maximumRadius = -1.0e9f;
    bool repeatable = true;
    bool neighboursContinuous = true;
    for (const glm::vec2& location : locations) {
        const glm::vec3 direction = glm::normalize(glm::vec3(
            location.x, TerrainDemo::kBaseRadius, location.y));
        const glm::vec3 nearbyDirection = glm::normalize(glm::vec3(
            location.x + 0.01f, TerrainDemo::kBaseRadius, location.y + 0.01f));
        const float radius = terrain->RadiusAt(direction);
        const float nearbyRadius = terrain->RadiusAt(nearbyDirection);
        repeatable &= radius == terrain->RadiusAt(direction);
        minimumRadius = std::min(minimumRadius, radius);
        maximumRadius = std::max(maximumRadius, radius);
        const TerrainSample sample = terrain->Sample(direction * radius);
        const TerrainSample nearby = terrain->Sample(nearbyDirection * nearbyRadius);
        neighboursContinuous &= std::abs(radius - nearbyRadius) < 0.05f &&
            glm::dot(sample.outwardNormal, nearby.outwardNormal) > 0.999f &&
            std::abs(sample.signedDistance) < 0.001f &&
            std::abs(nearby.signedDistance) < 0.001f;
    }
    Check(repeatable, "authored elevation repeats for the same local direction");
    Check(maximumRadius - minimumRadius > 1.0f,
          "authored planet has measurable hills and depressions");
    Check(neighboursContinuous,
          "nearby authored samples have continuous elevation and outward normals");

    const MeshData mesh = terrain->BuildMesh(96, 128);
    Check(!mesh.vertices.empty() && !mesh.indices.empty() && mesh.indices.size() % 3 == 0,
          "authored terrain produces an indexed triangle mesh");
    bool verticesFinite = true;
    for (const MeshVertex& vertex : mesh.vertices) {
        verticesFinite &= std::isfinite(vertex.position.x) &&
            std::isfinite(vertex.position.y) && std::isfinite(vertex.position.z) &&
            std::isfinite(vertex.normal.x) && std::isfinite(vertex.normal.y) &&
            std::isfinite(vertex.normal.z) && std::isfinite(vertex.uv.x) &&
            std::isfinite(vertex.uv.y) &&
            glm::dot(vertex.normal, vertex.position) > 0.0f;
    }
    Check(verticesFinite, "mesh positions, UVs and outward normals remain finite");

    bool indicesValid = true;
    for (std::uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) indicesValid = false;
    }
    Check(indicesValid, "every terrain triangle index references a vertex");
    if (!indicesValid) return;
    int nondegenerateTriangles = 0;
    bool outwardWinding = true;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const glm::vec3& a = mesh.vertices[mesh.indices[i]].position;
        const glm::vec3& b = mesh.vertices[mesh.indices[i + 1]].position;
        const glm::vec3& c = mesh.vertices[mesh.indices[i + 2]].position;
        const glm::vec3 faceNormal = glm::cross(b - a, c - a);
        if (glm::length(faceNormal) < 1.0e-6f) continue;
        ++nondegenerateTriangles;
        outwardWinding &= glm::dot(faceNormal, (a + b + c) / 3.0f) > 0.0f;
    }
    Check(nondegenerateTriangles > 1000 && outwardWinding,
          "representative nondegenerate terrain faces wind outward");
}

void TestPlayerSupport(const glm::vec3& translation, const glm::quat& rotation) {
    PhysicsWorld world;
    Check(world.Init(), "physics world initializes");
    const auto terrain = MakeSlope();
    const BodyHandle ground = world.CreateStaticTerrain(
        translation, rotation, terrain, 0.8f, 0.0f);
    Check(ground.IsValid() && !world.IsDynamicBody(ground), "terrain is an ordinary static body");
    Check(world.GetBodySupportDistance(ground, rotation * glm::vec3(0.0f, 1.0f, 0.0f)) >=
          terrain->RadiusAt(glm::vec3(0.0f, 1.0f, 0.0f)),
          "terrain reports a conservative body support bound");
    Check(world.CreatePlayerShape(0.3f, 0.6f), "player capsule query shape created");

    const glm::vec3 localStart(0.0f, 21.6f, 0.0f);
    const glm::vec3 localTravel(0.0f, -2.0f, 0.0f);
    const ShapeSweepHit hit = world.SweepPlayerShape(
        translation + rotation * localStart, rotation, rotation * localTravel);
    Check(hit.hit && hit.hitBody.id == ground.id, "capsule finds the terrain support");
    Check(hit.distance > 0.5f && hit.distance < 1.0f,
          "capsule travel ends at terrain clearance, not the undisplaced sphere");
    const glm::vec3 expectedNormal = rotation * terrain->Sample(glm::vec3(0.0f, 20.0f, 0.0f)).outwardNormal;
    Check(Near(hit.normal, expectedNormal, 0.08f), "support normal follows terrain slope");
    Check(std::abs(glm::dot(hit.normal, rotation * glm::vec3(1.0f, 0.0f, 0.0f))) > 0.05f,
          "support normal differs from radial gravity direction");
}

void TestDynamicContact(const glm::vec3& translation, const glm::quat& rotation) {
    constexpr float dt = 1.0f / 60.0f;
    PhysicsWorld world;
    world.Init();
    const auto terrain = MakeSlope();
    world.CreateStaticTerrain(translation, rotation, terrain, 0.9f, 0.0f);

    const BodyHandle ball = world.CreateDynamicSphere(
        translation + rotation * glm::vec3(0.0f, 21.2f, 0.0f),
        0.3f, 2.0f, 0.8f, 0.0f);
    for (int step = 0; step < 180; ++step) {
        const glm::vec3 local = glm::conjugate(rotation) *
            (world.GetTransform(ball).position - translation);
        const glm::vec3 radial = glm::normalize(local);
        world.ApplyLinearAcceleration(ball, rotation * (-9.81f * radial), dt);
        world.Step(dt);
    }
    const glm::vec3 localResult = glm::conjugate(rotation) *
        (world.GetTransform(ball).position - translation);
    const float clearance = terrain->Sample(localResult).signedDistance;
    Check(std::isfinite(clearance), "dynamic terrain contact remains finite");
    Check(clearance > 0.24f && clearance < 0.55f,
          "sphere rests above terrain at its physical radius");

    const BodyHandle box = world.CreateDynamicBox(
        translation + rotation * glm::vec3(0.7f, 21.0f, 0.0f),
        glm::vec3(0.4f), 3.0f, 0.8f, 0.0f);
    world.ResetBody(box, world.GetTransform(box).position, rotation);
    for (int step = 0; step < 120; ++step) {
        const glm::vec3 local = glm::conjugate(rotation) *
            (world.GetTransform(box).position - translation);
        world.ApplyLinearAcceleration(box, rotation * (-9.81f * glm::normalize(local)), dt);
        world.Step(dt);
    }
    const glm::vec3 boxLocal = glm::conjugate(rotation) *
        (world.GetTransform(box).position - translation);
    Check(std::isfinite(boxLocal.x) && std::isfinite(boxLocal.y) && std::isfinite(boxLocal.z),
          "box/terrain contact remains finite");
    Check(terrain->Sample(boxLocal).signedDistance > 0.18f,
          "box/terrain contact prevents body centre sinking through the surface");

    const BodyTransform boxPose = world.GetTransform(box);
    const glm::vec3 halfExtents(0.4f);
    float minimumSurfaceClearance = 1.0e9f;
    const auto measureSurfacePoint = [&](const glm::vec3& boxPoint) {
        const glm::vec3 terrainPoint = glm::conjugate(rotation) *
            (boxPose.position + boxPose.rotation * boxPoint - translation);
        minimumSurfaceClearance = std::min(minimumSurfaceClearance,
            terrain->Sample(terrainPoint).signedDistance);
    };
    for (int x : {-1, 1})
        for (int y : {-1, 1})
            for (int z : {-1, 1})
                measureSurfacePoint(glm::vec3(x * halfExtents.x,
                                              y * halfExtents.y, z * halfExtents.z));
    for (int axis = 0; axis < 3; ++axis) {
        for (int sign : {-1, 1}) {
            glm::vec3 faceCentre(0.0f);
            faceCentre[axis] = sign * halfExtents[axis];
            measureSurfacePoint(faceCentre);
        }
    }
    std::printf("  settled box minimum corner/face clearance %.4f m\n",
                minimumSurfaceClearance);
    Check(minimumSurfaceClearance > -0.015f,
          "settled box corners and face centres remain outside terrain within contact slop");
}

struct WalkResult {
    glm::vec3 localPosition{0.0f};
    int airborneSteps = 0;
    float minClearance = 1.0e9f;
    float maxClearance = -1.0e9f;
    float minimumElevation = 1.0e9f;
    float maximumElevation = -1.0e9f;
    float maximumCameraStep = 0.0f;
    float maximumThirdPersonCameraStep = 0.0f;
    float maximumOrientationStep = 0.0f;
    float maximumClearanceStep = 0.0f;
    float maximumSupportVsRadialAngle = 0.0f;
    float stillDrift = 0.0f;
    double playerStepMilliseconds = 0.0;
    bool landedAfterJump = false;
};

WalkResult WalkAuthoredTerrain(const glm::vec3& translation, const glm::quat& rotation) {
    constexpr float dt = 1.0f / 60.0f;
    const auto terrain = TerrainDemo::CreateSurface();
    PhysicsWorld world;
    world.Init();
    world.CreateStaticTerrain(translation, rotation, terrain, 0.8f, 0.0f);
    const glm::vec3 localSpawn = TerrainDemo::LocalPointAbove(*terrain, -13.0f, 3.0f, 0.92f);
    const glm::vec3 spawn = translation + rotation * localSpawn;

    // Compensate the player's shortest local-up alignment for the arbitrary
    // scene rotation so the same input starts in the same tangent direction.
    const glm::quat aligned = glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f),
                                             rotation * glm::normalize(localSpawn));
    const glm::vec3 desiredForward = glm::inverse(aligned) *
                                     (rotation * glm::vec3(1.0f, 0.0f, 0.0f));
    const float yaw = glm::degrees(std::atan2(-desiredForward.x, -desiredForward.z));
    PlayerController player(spawn, yaw);
    Check(player.Spawn(world), "real player controller spawns on authored terrain");
    Window input;
    input.SetTestInputMode(true);
    RadicalGravity gravity(translation, 9.81f);
    for (int i = 0; i < 180; ++i) player.FixedUpdate(input, world, gravity, dt);

    WalkResult result;
    input.SetTestActionState(Action::MoveForward, true);
    glm::vec3 previousCamera(0.0f);
    glm::vec3 previousThirdCamera(0.0f);
    glm::quat previousOrientation = player.GetOrientation();
    float previousClearance = 0.0f;
    bool havePrevious = false;
    double totalPlayerStepMilliseconds = 0.0;
    for (int i = 0; i < 480; ++i) {
        if (i == 240) input.SetTestActionState(Action::StrafeRight, true);
        if (i == 360) input.SetTestActionState(Action::StrafeRight, false);
        const auto stepStart = std::chrono::steady_clock::now();
        player.FixedUpdate(input, world, gravity, dt);
        totalPlayerStepMilliseconds += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - stepStart).count();
        if (!player.IsGrounded()) ++result.airborneSteps;

        const glm::vec3 localPosition = glm::inverse(rotation) *
                                        (player.GetPosition() - translation);
        const glm::vec3 localBottom = glm::inverse(rotation) *
            (player.GetPosition() + player.GetOrientation() * glm::vec3(0.0f, -0.6f, 0.0f)
             - translation);
        const float clearance = terrain->Sample(localBottom).signedDistance - 0.3f;
        result.minClearance = std::min(result.minClearance, clearance);
        result.maxClearance = std::max(result.maxClearance, clearance);
        const float elevation = terrain->RadiusAt(glm::normalize(localPosition)) -
                                TerrainDemo::kBaseRadius;
        result.minimumElevation = std::min(result.minimumElevation, elevation);
        result.maximumElevation = std::max(result.maximumElevation, elevation);
        const glm::vec3 radial = glm::normalize(localPosition);
        const glm::vec3 supportNormal = terrain->Sample(localBottom).outwardNormal;
        result.maximumSupportVsRadialAngle = std::max(result.maximumSupportVsRadialAngle,
            glm::degrees(std::acos(glm::clamp(glm::dot(radial, supportNormal), -1.0f, 1.0f))));
        const glm::vec3 camera = glm::vec3(glm::inverse(
            player.GetViewMatrix(0.5f, PlayerViewMode::FirstPerson))[3]);
        const glm::vec3 thirdCamera = glm::vec3(glm::inverse(
            player.GetViewMatrix(0.5f, PlayerViewMode::ThirdPerson))[3]);
        if (havePrevious) {
            result.maximumCameraStep = std::max(result.maximumCameraStep,
                glm::length(camera - previousCamera));
            result.maximumThirdPersonCameraStep = std::max(result.maximumThirdPersonCameraStep,
                glm::length(thirdCamera - previousThirdCamera));
            result.maximumClearanceStep = std::max(result.maximumClearanceStep,
                std::abs(clearance - previousClearance));
        }
        const float orientationStep = glm::degrees(glm::angle(glm::normalize(
            player.GetOrientation() * glm::inverse(previousOrientation))));
        result.maximumOrientationStep = std::max(result.maximumOrientationStep, orientationStep);
        previousCamera = camera;
        previousThirdCamera = thirdCamera;
        previousClearance = clearance;
        previousOrientation = player.GetOrientation();
        havePrevious = true;
    }
    result.playerStepMilliseconds = totalPlayerStepMilliseconds / 480.0;
    result.localPosition = glm::inverse(rotation) * (player.GetPosition() - translation);
    input.SetTestActionState(Action::MoveForward, false);
    for (int i = 0; i < 180; ++i) player.FixedUpdate(input, world, gravity, dt);
    const glm::vec3 stillStart = player.GetPosition();
    for (int i = 0; i < 90; ++i) player.FixedUpdate(input, world, gravity, dt);
    result.stillDrift = glm::length(player.GetPosition() - stillStart);

    input.RequestTestJump();
    player.UpdateFrameInput(input);
    int airborneAfterJump = 0;
    for (int i = 0; i < 240; ++i) {
        player.FixedUpdate(input, world, gravity, dt);
        if (!player.IsGrounded()) ++airborneAfterJump;
    }
    result.landedAfterJump = airborneAfterJump > 5 && player.IsGrounded();
    return result;
}

void TestPlayerTraversal(const glm::quat& rotation) {
    std::printf("Player traversal over authored hills/basins\n");
    const WalkResult baseline = WalkAuthoredTerrain(glm::vec3(0.0f),
                                                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    const WalkResult transformed = WalkAuthoredTerrain(glm::vec3(28.0f, -19.0f, 37.0f), rotation);
    std::printf("  clearance [%.4f, %.4f] m, delta %.4f m; camera steps %.4f/%.4f m, "
                "up step %.4f deg, slope %.2f deg, elevation span %.3f m, airborne %d, still drift %.5f m\n",
                baseline.minClearance, baseline.maxClearance, baseline.maximumClearanceStep,
                baseline.maximumCameraStep, baseline.maximumThirdPersonCameraStep,
                baseline.maximumOrientationStep, baseline.maximumSupportVsRadialAngle,
                baseline.maximumElevation - baseline.minimumElevation,
                baseline.airborneSteps, baseline.stillDrift);
    std::printf("  transformed local endpoint error %.4f m, clearance delta %.4f m; "
                "player update %.3f ms/step\n",
                glm::length(baseline.localPosition - transformed.localPosition),
                transformed.maximumClearanceStep, baseline.playerStepMilliseconds);
    Check(baseline.airborneSteps == 0 && transformed.airborneSteps == 0,
          "walking across terrain stays grounded without chatter");
    Check(baseline.maximumElevation - baseline.minimumElevation > 0.4f,
          "walk genuinely crosses changing terrain elevation");
    Check(baseline.maxClearance - baseline.minClearance < 0.075f &&
          transformed.maxClearance - transformed.minClearance < 0.075f,
          "support clearance remains bounded over hills and valleys");
    Check(baseline.maximumClearanceStep < 0.015f &&
          transformed.maximumClearanceStep < 0.015f,
          "support clearance does not jump between adjacent fixed steps");
    Check(baseline.maximumCameraStep < 0.12f && transformed.maximumCameraStep < 0.12f,
          "presented first-person camera has no position jump on terrain");
    Check(baseline.maximumThirdPersonCameraStep < 0.12f &&
          transformed.maximumThirdPersonCameraStep < 0.12f,
          "presented third-person camera has no position jump on terrain");
    Check(baseline.maximumSupportVsRadialAngle > 3.0f,
          "terrain support normal differs measurably from radial gravity");
    Check(baseline.maximumOrientationStep < 0.15f && transformed.maximumOrientationStep < 0.15f,
          "local frame evolves smoothly while terrain normal differs from gravity");
    Check(baseline.stillDrift < 0.003f && transformed.stillDrift < 0.003f,
          "player stands still after traversing terrain");
    Check(baseline.landedAfterJump && transformed.landedAfterJump,
          "player jumps and lands on terrain in both orientations");
    Check(Near(baseline.localPosition, transformed.localPosition, 0.04f),
          "rotated and translated terrain walk has equivalent local result");
}
}  // namespace

int main() {
    std::printf("Terrain physics: normal, rotated and translated worlds\n");
    TestAuthoredGeometry();
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::quat rotation = glm::normalize(glm::angleAxis(
        glm::radians(57.0f), glm::normalize(glm::vec3(0.6f, -0.3f, 0.7f))));
    TestPlayerSupport(glm::vec3(0.0f), identity);
    TestPlayerSupport(glm::vec3(32.0f, -14.0f, 61.0f), rotation);
    TestDynamicContact(glm::vec3(0.0f), identity);
    TestDynamicContact(glm::vec3(32.0f, -14.0f, 61.0f), rotation);
    TestPlayerTraversal(rotation);
    std::printf("Terrain physics: %s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
