#include "BasinMk2.h"

namespace judas::volume_grid::basin_probe {
namespace {

double MaxAbs(glm::dvec3 x) {
    return std::max({std::abs(x.x), std::abs(x.y), std::abs(x.z)});
}

double HalfUlp(float x) {
    const auto lo = std::nextafter(x, -std::numeric_limits<float>::infinity());
    const auto hi = std::nextafter(x, std::numeric_limits<float>::infinity());
    return .5 * std::max(double(x) - lo, double(hi) - x);
}

struct Anchor {
    glm::vec3 center{0}, velocity{0};
    double begin = 0.;
    bool valid = false;
};

Anchor AnchorFor(Anchor previous, glm::vec3 center, glm::vec3 velocity, double begin) {
    if (!previous.valid || previous.velocity != velocity ||
        previous.center + previous.velocity * float(begin - previous.begin) != center)
        return {center, velocity, begin, true};
    return previous;
}

bool CanOwnSweep(PhysicsWorld& world, const PhysicsConstraintBatch::Body& body,
                 const Forecast& endpoint) {
    const glm::mat3 r = glm::mat3_cast(body.orientation);
    const glm::vec3 extent = glm::abs(r[0]) * bodyHalf.x + glm::abs(r[1]) * bodyHalf.y +
        glm::abs(r[2]) * bodyHalf.z;
    glm::vec3 lower = glm::min(body.center, endpoint.center) - extent;
    glm::vec3 upper = glm::max(body.center, endpoint.center) + extent;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(lower[axis]) || !std::isfinite(upper[axis]))
            return false;
        lower[axis] = std::nextafter(lower[axis], -std::numeric_limits<float>::infinity());
        upper[axis] = std::nextafter(upper[axis], std::numeric_limits<float>::infinity());
    }
    PhysicsQueryFilter filter;
    filter.ignoredBodies = {body.handle};
    if (world.QueryBodiesInAabb(lower, upper, filter).empty())
        return true;
    // An axis-aligned broadphase hull can cover the basin interior when
    // rotated. The native fixed-orientation cast then tests actual walls.
    const glm::vec3 delta = endpoint.center - body.center;
    const float distance = glm::length(delta);
    const auto direction = distance > 0.f ? delta : glm::vec3(0, 1, 0);
    return !world.BoxCast({body.center, body.orientation}, bodyHalf, direction, distance, filter).hit;
}

} // namespace

NativeResult RunMk2(const Fixture& fixture, unsigned warmupSteps, unsigned rigidSteps,
                    const WarmupStep& warmup, const BuildProposal& stage, const Observe& observe) {
    NativeResult result;
    PhysicsWorld world;
    if (!world.Init()) {
        result.error = "native basin cannot initialize MK2";
        return result;
    }
    for (const auto& wall : Fixture::wallDefinitions) {
        if (!world.CreateStaticBox(fixture.rotation * wall.center, fixture.rotation,
                                   wall.half, 0, 0).IsValid()) {
            result.error = "native basin cannot create an authored wall";
            return result;
        }
    }
    const auto start = fixture.NativeInitialCenter();
    const auto placeholder = world.CreateStaticBox(start, fixture.rotation, bodyHalf, 0, 0);
    if (!placeholder.IsValid()) {
        result.error = "native basin cannot create the authored static placeholder";
        return result;
    }
    const GridVec expectedChart = Array(glm::dvec3(initialCenter) - Vector(fixture.origin));
    result.chartInitialError = MaxAbs(Vector(fixture.ChartPoint(start)) - Vector(expectedChart));
    for (unsigned i = 0; i < warmupSteps; ++i) {
        std::string error;
        if (warmup && !warmup({fixture.InitialBox(), tick, i}, error)) {
            result.error = "warmup " + std::to_string(i) + ": " + error;
            return result;
        }
        world.Step(tick);
        ++result.warmupSteps;
    }
    world.DestroyBody(placeholder);
    Shape shape;
    BodyTransform old;
    result.placeholderDestroyed = !world.GetBodyShape(placeholder, shape, old);
    if (!result.placeholderDestroyed) {
        result.error = "native basin placeholder survived release";
        return result;
    }
    shape.type = ShapeType::Box;
    shape.halfExtents = bodyHalf;
    const auto body = world.CreateShape(shape, {start, fixture.rotation}, true, Fixture::BodyMass(), 0, 0);
    if (!body.IsValid()) {
        result.error = "native basin cannot create the authored dynamic body";
        return result;
    }
    const auto initialOrientation = world.GetTransform(body).rotation;
    const auto nativeGravity = fixture.WorldGravity();

    Anchor anchor, pendingAnchor;
    Proposal pending;
    glm::vec3 predictor{0};
    float inverseMass = 0.;
    unsigned pendingIndex = 0;
    bool impulsePublished = false;

    PhysicsConstraintParticipant participant;
    participant.bodies = {body};
    participant.maximumBodies = 1;
    participant.maximumContacts = 1;
    participant.maximumJointRows = 1;
    participant.propose = [&](const PhysicsConstraintBatch& batch, PhysicsConstraintProposal& proposal) {
        ++result.proposals;
        if (batch.bodies.size() != 1 || batch.bodies.front().handle.id != body.id ||
            batch.bodies.front().handle.world != body.world || !batch.contacts.empty() || !batch.joints.empty())
            throw std::runtime_error("native basin participant encountered a rigid constraint");
        const auto& native = batch.bodies.front();
        if (native.angularVelocity != glm::vec3(0) || native.orientation != initialOrientation)
            throw std::runtime_error("native basin translation-only proposal encountered rotation");
        if (pending.commit || impulsePublished)
            throw std::runtime_error("prior native basin proposal was not consumed");

        predictor = native.linearVelocity;
        inverseMass = native.inverseMass;
        pendingIndex = batch.context.index;
        const double begin = double(batch.context.stepDeltaTime) * batch.context.index / batch.context.count;
        const auto chartCenter = fixture.ChartPoint(native.center);
        const auto predict = [&](glm::dvec3 impulse) {
            const glm::vec3 velocity(glm::dvec3(predictor) + double(inverseMass) * impulse);
            const Anchor next = AnchorFor(anchor, native.center, velocity, begin);
            const glm::vec3 endpoint = next.center + next.velocity *
                float(double(batch.context.deltaTime) - (next.begin - begin));
            return Forecast{endpoint, velocity, Array(Vector(fixture.ChartPoint(endpoint)) - Vector(chartCenter))};
        };
        const GridVec chartGravity{0., -double(gravity), 0.};
        std::string error;
        if (!stage || !stage({batch.context, native, fixture.BoxAtChartCenter(chartCenter), chartGravity, predict},
                             pending, error))
            throw std::runtime_error(error.empty() ? "native basin has no material proposal" : error);
        if (!pending.commit)
            throw std::runtime_error("native basin proposal has no staged material commit");
        const auto forecast = predict(pending.worldImpulse);
        if (forecast.center != pending.forecast.center || forecast.velocity != pending.forecast.velocity ||
            forecast.translation != pending.forecast.translation)
            throw std::runtime_error("native basin proposal does not match its own pressure impulse");
        if (!CanOwnSweep(world, native, forecast))
            throw std::runtime_error("native basin proposed sweep may require CCD or contact");
        pendingAnchor = AnchorFor(anchor, native.center, forecast.velocity, begin);
        proposal.linearImpulses = {pending.worldImpulse};
        proposal.angularImpulses = {glm::dvec3(0)};
        return true;
    };
    participant.published = [&](const auto&) noexcept {
        impulsePublished = true;
        ++result.published;
    };
    PhysicsWorld::VelocityCoupling coupling;
    coupling.physicalBatch = &participant;
    coupling.afterSubstep = [&](const auto& context) {
        if (!impulsePublished || pendingIndex != context.index)
            throw std::runtime_error("native basin material impulse was not published");
        const auto pose = world.GetTransform(body);
        const auto velocity = world.GetLinearVelocity(body);
        if (pose.position != pending.forecast.center || velocity != pending.forecast.velocity ||
            pose.rotation != initialOrientation || world.GetAngularVelocity(body) != glm::vec3(0))
            throw std::runtime_error("native basin path changed after material impulse publication");
        const glm::dvec3 measured = (glm::dvec3(velocity) - glm::dvec3(predictor)) / double(inverseMass);
        const auto difference = glm::abs(measured - pending.worldImpulse);
        for (int axis = 0; axis < 3; ++axis) {
            const double budget = HalfUlp(velocity[axis]) / inverseMass +
                8. * std::numeric_limits<double>::epsilon() * (1. + std::abs(pending.worldImpulse[axis]));
            if (difference[axis] > budget)
                throw std::runtime_error("native basin impulse rounding exceeded one representable velocity");
        }
        result.maxNativeImpulseError = std::max(result.maxNativeImpulseError, MaxAbs(difference));
        pending.commit();
        pending = {};
        anchor = pendingAnchor;
        impulsePublished = false;
        ++result.accepted;
    };

    try {
        for (unsigned step = 0; step < rigidSteps; ++step) {
            anchor = {};
            world.ApplyLinearAcceleration(body, nativeGravity, tick);
            world.Step(tick, &coupling);
            ++result.completedRigidSteps;
            if (observe)
                observe(step, world.GetTransform(body), world.GetLinearVelocity(body));
        }
    } catch (const std::exception& failure) {
        result.error = failure.what();
    }
    result.finalPose = world.GetTransform(body);
    result.finalVelocity = world.GetLinearVelocity(body);
    result.finalAngularVelocity = world.GetAngularVelocity(body);
    return result;
}

} // namespace judas::volume_grid::basin_probe
