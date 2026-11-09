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
#include "RadialTerrain.h"
#include "RadicalGravity.h"
#include "fixtures/terrain/RadialFixture.h"
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
    const auto terrain = TerrainFixture::CreateSurface();
    const std::array<glm::vec2, 5> locations{{
        {-4.0f, 3.0f}, {0.0f, 3.0f}, {5.0f, 3.0f},
        {-17.0f, 8.0f}, {16.0f, -7.0f}}};
    float minimumRadius = 1.0e9f;
    float maximumRadius = -1.0e9f;
    bool repeatable = true;
    bool neighboursContinuous = true;
    for (const glm::vec2& location : locations) {
        const glm::vec3 direction = glm::normalize(glm::vec3(
            location.x, TerrainFixture::kBaseRadius, location.y));
        const glm::vec3 nearbyDirection = glm::normalize(glm::vec3(
            location.x + 0.01f, TerrainFixture::kBaseRadius, location.y + 0.01f));
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
    std::printf("Terrain physics: %s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
