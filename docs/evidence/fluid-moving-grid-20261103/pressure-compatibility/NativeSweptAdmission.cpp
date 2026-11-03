#include "PhysicsConstraintBatch.h"
#include "PhysicsWorld.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

constexpr float tick = 1.f / 60.f;
constexpr glm::vec3 initial{0.f, .5f, 0.f};
constexpr glm::vec3 half{.10f, .10f, .10f};

enum class Fixture { Clear, Wall, RemoteMoving, Sensor };

struct RunResult {
    float firstX = 0., firstVelocity = 0.;
    unsigned proposals = 0, publications = 0, completed = 0;
    unsigned initialRows = 0;
    std::string rejection;
};

bool CanOwnFreePath(PhysicsWorld& world, const PhysicsConstraintBatch& batch,
                    const glm::dvec3& impulse, std::string& error) {
    if (batch.bodies.size() != 1 || !batch.contacts.empty() || !batch.joints.empty()) {
        error = "pressure step requires one contact-free translation owner";
        return false;
    }
    const auto& body = batch.bodies.front();
    if (body.angularVelocity != glm::vec3(0) || body.orientation != glm::quat(1, 0, 0, 0)) {
        error = "pressure step cannot admit angular motion";
        return false;
    }
    Shape geometry;
    BodyTransform pose;
    if (!world.GetBodyShape(body.handle, geometry, pose) || geometry.type != ShapeType::Box ||
        geometry.pivotOffset != glm::vec3(0) || pose.position != body.center) {
        error = "pressure step requires a centered box";
        return false;
    }
    for (const auto other : world.AliveBodies()) {
        if (other.world == body.handle.world && other.id == body.handle.id)
            continue;
        if (world.IsBodyEnabled(other) && !world.IsBodySensor(other) &&
            world.GetMotionType(other) != BodyMotionType::Static) {
            error = "pressure step cannot forecast another moving collision body";
            return false;
        }
    }
    const glm::vec3 velocity = glm::vec3(glm::dvec3(body.linearVelocity) + double(body.inverseMass) * impulse);
    const glm::vec3 projected = body.center + velocity * batch.context.deltaTime;
    glm::vec3 lower = glm::min(body.center, projected) - geometry.halfExtents;
    glm::vec3 upper = glm::max(body.center, projected) + geometry.halfExtents;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(lower[axis]) || !std::isfinite(upper[axis])) {
            error = "pressure step's candidate sweep is not representable";
            return false;
        }
        lower[axis] = std::nextafter(lower[axis], -std::numeric_limits<float>::infinity());
        upper[axis] = std::nextafter(upper[axis], std::numeric_limits<float>::infinity());
    }
    PhysicsQueryFilter filter;
    filter.ignoredBodies = {body.handle};
    if (!world.QueryBodiesInAabb(lower, upper, filter).empty()) {
        error = "pressure step's swept bound has a native collision candidate";
        return false;
    }
    return true;
}

RunResult Run(Fixture fixture) {
    PhysicsWorld world;
    if (!world.Init())
        throw std::runtime_error("MK2 initialization failed");
    const auto body = world.CreateDynamicBox(initial, half, 1.f, 0.f, 0.f);
    if (!body.IsValid())
        throw std::runtime_error("MK2 dynamic body creation failed");
    if (fixture == Fixture::Wall || fixture == Fixture::Sensor) {
        const auto wall = world.CreateStaticBox({.32f, .5f, 0.f}, {.025f, 2.f, 2.f}, 0.f, 0.f);
        if (!wall.IsValid())
            throw std::runtime_error("MK2 wall creation failed");
        if (fixture == Fixture::Sensor)
            world.SetBodySensor(wall, true);
    } else if (fixture == Fixture::RemoteMoving) {
        if (!world.CreateDynamicBox({10.f, .5f, 0.f}, half, 1.f, 0.f, 0.f).IsValid())
            throw std::runtime_error("MK2 remote body creation failed");
    }

    RunResult result;
    PhysicsConstraintParticipant participant;
    participant.bodies = {body};
    participant.propose = [&](const auto& batch, auto& proposal) {
        ++result.proposals;
        if (batch.context.index != 0)
            return false;
        result.initialRows = static_cast<unsigned>(batch.contacts.size() + batch.joints.size());
        const glm::dvec3 impulse{100., 0., 0.};
        std::string error;
        if (!CanOwnFreePath(world, batch, impulse, error))
            throw std::runtime_error(error);
        proposal.linearImpulses = {impulse};
        proposal.angularImpulses = {glm::dvec3(0)};
        return true;
    };
    participant.published = [&](const auto&) { ++result.publications; };
    PhysicsWorld::VelocityCoupling coupling;
    coupling.physicalBatch = &participant;
    coupling.afterSubstep = [&](const auto& context) {
        ++result.completed;
        if (context.index == 0) {
            result.firstX = world.GetTransform(body).position.x;
            result.firstVelocity = world.GetLinearVelocity(body).x;
        }
    };
    try {
        world.Step(tick, &coupling);
    } catch (const std::runtime_error& error) {
        result.rejection = error.what();
    }
    if (result.completed == 0) {
        result.firstX = world.GetTransform(body).position.x;
        result.firstVelocity = world.GetLinearVelocity(body).x;
    }
    return result;
}

} // namespace

int main() {
    unsigned failures = 0;
    std::cout << std::setprecision(17);
    for (const auto fixture : {Fixture::Clear, Fixture::Wall, Fixture::RemoteMoving, Fixture::Sensor}) {
        const auto r = Run(fixture);
        const bool expectedPass = fixture == Fixture::Clear || fixture == Fixture::Sensor;
        const bool accepted = expectedPass
            ? r.initialRows == 0 && r.publications == 1 && r.completed == 4 &&
              r.firstVelocity == 100.f && std::abs(r.firstX - 100.f * tick / 4) < 1e-7f
            : r.initialRows == 0 && r.publications == 0 && r.completed == 0 &&
              r.firstX == initial.x && r.firstVelocity == 0.f && !r.rejection.empty();
        failures += !accepted;
        std::cout << (accepted ? "PASS " : "FAIL ") << int(fixture)
                  << " rows=" << r.initialRows << " proposals=" << r.proposals
                  << " accepted=" << r.publications << " completed=" << r.completed
                  << " x=" << r.firstX << " vx=" << r.firstVelocity
                  << " reason=" << r.rejection << '\n';
    }
    return failures ? 1 : 0;
}
