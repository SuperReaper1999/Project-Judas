// Headless checks for generic compound-box rigid bodies. An open five-box
// vessel is test geometry, not a special shape or a stored fluid amount.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"

namespace {

int failures = 0;

void Check(bool condition, const char* description) {
    if (!condition) {
        std::printf("  FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance = 1.0e-4f) {
    return glm::length(a - b) <= tolerance;
}

std::vector<CompoundBox> OpenVesselBoxes() {
    return {
        {{0.0f, -0.45f, 0.0f}, {0.60f, 0.05f, 0.60f}},  // bottom
        {{-0.55f, 0.05f, 0.0f}, {0.05f, 0.50f, 0.60f}}, // left wall
        {{ 0.55f, 0.05f, 0.0f}, {0.05f, 0.50f, 0.60f}}, // right wall
        {{0.0f, 0.05f, -0.55f}, {0.50f, 0.50f, 0.05f}}, // back wall
        {{0.0f, 0.05f,  0.55f}, {0.50f, 0.50f, 0.05f}}, // front wall
    };
}

void TestGeometryAndPreviousPose() {
    std::printf("Compound geometry, orientation and prior pose\n");
    PhysicsWorld world;
    Check(world.Init(), "world initializes");
    const std::vector<CompoundBox> localBoxes = OpenVesselBoxes();
    const BodyHandle cup = world.CreateDynamicCompoundBoxes(
        glm::vec3(2.0f, -3.0f, 4.0f), localBoxes, 2.0f, 0.5f, 0.0f);
    Check(cup.IsValid(), "compound body created");
    Check(world.IsDynamicBody(cup), "compound body remains dynamic");
    Check(std::abs(world.GetMass(cup) - 2.0f) < 1.0e-5f, "mass retained");
    Check(world.GetBodyBoxes(cup).size() == localBoxes.size(), "all five physical pieces exposed");
    Check(!world.CreateDynamicCompoundBoxes(glm::vec3(0.0f), {}, 1.0f, 0.5f, 0.0f).IsValid(),
          "empty compound rejected");

    const glm::quat rotation = glm::angleAxis(0.83f, glm::normalize(glm::vec3(1.0f, 2.0f, -3.0f)));
    const glm::vec3 position(-1.0f, 5.0f, 2.0f);
    world.ResetBody(cup, position, rotation);
    const std::vector<BodyBox> boxes = world.GetBodyBoxes(cup);
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        Check(Near(boxes[i].center, position + rotation * localBoxes[i].localCenter),
              "child centre follows the full parent rotation");
        Check(Near(boxes[i].rotation * glm::vec3(0.0f, 1.0f, 0.0f),
                   rotation * glm::vec3(0.0f, 1.0f, 0.0f)),
              "child orientation follows parent orientation");
        Check(Near(boxes[i].halfExtents, localBoxes[i].halfExtents),
              "child half extents retained");
    }

    const std::vector<BodyBox> initialPrevious = world.GetPreviousBodyBoxes(cup);
    Check(Near(initialPrevious[0].center, boxes[0].center), "reset synchronizes prior pose");
    world.SetLinearVelocity(cup, glm::vec3(3.0f, 0.0f, 0.0f));
    world.Step(1.0f / 60.0f);
    const std::vector<BodyBox> previous = world.GetPreviousBodyBoxes(cup);
    const std::vector<BodyBox> current = world.GetBodyBoxes(cup);
    Check(Near(previous[0].center, boxes[0].center), "prior child pose is pre-step pose");
    Check(Near(current[0].center - previous[0].center, glm::vec3(0.05f, 0.0f, 0.0f), 1.0e-4f),
          "current child pose follows integrated parent velocity");

    const glm::vec3 direction = glm::normalize(glm::vec3(0.7f, 0.2f, -0.3f));
    float geometricSupport = -1.0e10f;
    for (const BodyBox& box : current) {
        const glm::vec3 localDirection = glm::conjugate(box.rotation) * direction;
        geometricSupport = std::max(geometricSupport,
            glm::dot(box.center - world.GetTransform(cup).position, direction) +
            glm::dot(glm::abs(localDirection), box.halfExtents));
    }
    Check(std::abs(world.GetBodySupportDistance(cup, direction) - geometricSupport) < 1.0e-4f,
          "support distance includes displaced compound pieces");
}

void TestOpenTopAndSolidWalls() {
    std::printf("Physical bottom/walls with no invisible top\n");
    constexpr float dt = 1.0f / 60.0f;

    {
        PhysicsWorld world;
        world.Init();
        world.CreateStaticBox(glm::vec3(0.0f, -1.0f, 0.0f),
                              glm::vec3(4.0f, 0.5f, 4.0f), 0.8f, 0.0f);
        const BodyHandle cup = world.CreateDynamicCompoundBoxes(
            glm::vec3(0.0f), OpenVesselBoxes(), 2.0f, 0.8f, 0.0f);
        for (int step = 0; step < 240; ++step) {
            world.ApplyLinearAcceleration(cup, glm::vec3(0.0f, -9.81f, 0.0f), dt);
            world.Step(dt);
        }
        Check(std::abs(world.GetTransform(cup).position.y) < 0.06f,
              "compound bottom rests on ordinary static ground");
        Check(glm::length(world.GetAngularVelocity(cup)) < 0.1f,
              "symmetric resting cup does not spin uncontrollably");
    }

    {
        PhysicsWorld world;
        world.Init();
        const BodyHandle cup = world.CreateDynamicCompoundBoxes(
            glm::vec3(0.0f), OpenVesselBoxes(), 1000.0f, 0.5f, 0.0f);
        const BodyHandle drop = world.CreateDynamicSphere(glm::vec3(0.0f, 1.0f, 0.0f),
                                                           0.1f, 1.0f, 0.5f, 0.0f);
        for (int step = 0; step < 100; ++step) {
            world.ApplyLinearAcceleration(drop, glm::vec3(0.0f, -9.81f, 0.0f), dt);
            world.Step(dt);
        }
        const glm::vec3 relative = world.GetTransform(drop).position - world.GetTransform(cup).position;
        Check(relative.y > -0.36f && relative.y < 0.0f,
              "sphere entering through open top rests on geometric bottom");
    }

    {
        PhysicsWorld world;
        world.Init();
        const BodyHandle cup = world.CreateDynamicCompoundBoxes(
            glm::vec3(0.0f), OpenVesselBoxes(), 1000.0f, 0.5f, 0.0f);
        const BodyHandle sphere = world.CreateDynamicSphere(glm::vec3(0.0f), 0.1f,
                                                              1.0f, 0.5f, 0.0f);
        world.SetLinearVelocity(sphere, glm::vec3(3.0f, 0.0f, 0.0f));
        for (int step = 0; step < 40; ++step) world.Step(dt);
        const glm::vec3 relative = world.GetTransform(sphere).position - world.GetTransform(cup).position;
        Check(relative.x < 0.46f, "geometric side wall blocks outward sphere motion");
        Check(world.GetLinearVelocity(cup).x > 0.0f,
              "wall contact transfers momentum to the dynamic vessel");
    }

    {
        PhysicsWorld world;
        world.Init();
        const BodyHandle cup = world.CreateDynamicCompoundBoxes(
            glm::vec3(0.0f), OpenVesselBoxes(), 1000.0f, 0.5f, 0.0f);
        const BodyHandle sphere = world.CreateDynamicSphere(glm::vec3(0.0f), 0.1f,
                                                              1.0f, 0.5f, 0.0f);
        world.SetLinearVelocity(sphere, glm::vec3(0.0f, 2.0f, 0.0f));
        for (int step = 0; step < 40; ++step) world.Step(dt);
        Check(world.GetTransform(sphere).position.y - world.GetTransform(cup).position.y > 0.8f,
              "body leaves through genuinely open rim");
    }

    {
        PhysicsWorld world;
        world.Init();
        const BodyHandle a = world.CreateDynamicCompoundBoxes(
            glm::vec3(-1.5f, 0.0f, 0.0f), OpenVesselBoxes(), 2.0f, 0.5f, 0.0f);
        const BodyHandle b = world.CreateDynamicCompoundBoxes(
            glm::vec3(1.5f, 0.0f, 0.0f), OpenVesselBoxes(), 2.0f, 0.5f, 0.0f);
        world.SetLinearVelocity(a, glm::vec3(2.0f, 0.0f, 0.0f));
        world.SetLinearVelocity(b, glm::vec3(-2.0f, 0.0f, 0.0f));
        for (int step = 0; step < 60; ++step) world.Step(dt);
        Check(world.GetTransform(a).position.x < world.GetTransform(b).position.x,
              "two moving compound bodies collide rather than pass through one another");
        Check(world.GetLinearVelocity(a).x < 1.0f && world.GetLinearVelocity(b).x > -1.0f,
              "compound contact changes both bodies' momenta");
    }
}

void TestPlayerSweepAndPointImpulse() {
    std::printf("Player query and point impulse use compound parent\n");
    PhysicsWorld world;
    world.Init();
    const BodyHandle cup = world.CreateDynamicCompoundBoxes(
        glm::vec3(0.0f), OpenVesselBoxes(), 2.0f, 0.5f, 0.0f);
    world.CreatePlayerShape(0.15f, 0.20f);
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const ShapeSweepHit baseline = world.SweepPlayerShape(glm::vec3(2.0f, 0.0f, 0.0f),
                                                           identity, glm::vec3(-3.0f, 0.0f, 0.0f));
    Check(baseline.hit && baseline.hitBody.id == cup.id, "player sweep hits a cup wall");
    Check(baseline.normal.x > 0.9f, "wall sweep returns outward normal");

    const glm::quat rotation = glm::angleAxis(0.67f, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
    world.ResetBody(cup, glm::vec3(2.0f, -1.0f, 3.0f), rotation);
    const ShapeSweepHit rotated = world.SweepPlayerShape(
        glm::vec3(2.0f, -1.0f, 3.0f) + rotation * glm::vec3(2.0f, 0.0f, 0.0f),
        rotation, rotation * glm::vec3(-3.0f, 0.0f, 0.0f));
    Check(rotated.hit && rotated.hitBody.id == cup.id, "rotated player sweep hits same cup wall");
    Check(std::abs(rotated.distance - baseline.distance) < 0.02f,
          "player sweep distance rotates with scenario");
    Check(Near(rotated.normal, rotation * baseline.normal, 0.02f),
          "player sweep normal rotates with scenario");

    world.ResetBody(cup, glm::vec3(0.0f), identity);
    world.ApplyImpulseAtPoint(cup, glm::vec3(2.0f, 0.0f, 0.0f),
                              glm::vec3(0.0f, 0.5f, 0.0f));
    Check(Near(world.GetLinearVelocity(cup), glm::vec3(1.0f, 0.0f, 0.0f)),
          "off-centre impulse changes linear momentum by impulse / mass");
    Check(world.GetAngularVelocity(cup).z < 0.0f,
          "off-centre impulse also creates physical angular motion");
}

}  // namespace

int main() {
    TestGeometryAndPreviousPose();
    TestOpenTopAndSolidWalls();
    TestPlayerSweepAndPointImpulse();
    if (failures != 0) {
        std::printf("Compound body tests: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("Compound body tests: all passed\n");
    return 0;
}
