#include "PhysicsConstraintBatch.h"
#include "PhysicsWorld.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

constexpr float dt = 1.f / 60.f;
constexpr float kick = 100.f;
constexpr float wallCenter = .32f, wallHalf = .025f, bodyHalf = .10f;
constexpr glm::vec3 initial{0.f, .5f, 0.f};

enum class Mode { Free, Wall, RejectSecond };

struct Substep {
    float beforePosition = -1.f, beforeVelocity = -1.f;
    float publishedPosition = -1.f, publishedVelocity = -1.f;
    float afterPosition = -1.f, afterVelocity = -1.f;
    unsigned contacts = 0, joints = 0;
    std::vector<PhysicsWorld::BodyMotionSegment> segments;
};

struct Result {
    unsigned proposals = 0, publications = 0, observations = 0;
    std::array<Substep, 4> boundaries;
    std::vector<PhysicsWorld::BodyMotionSegment> macro;
    float finalPosition = 0.f, finalVelocity = 0.f;
    std::string error;
};

void PrintSegments(std::string_view phase, unsigned index,
                   const std::vector<PhysicsWorld::BodyMotionSegment>& segments) {
    for (const auto& path : segments)
        std::cout << "path " << phase << ' ' << index << " interval " << path.begin << ' '
                  << path.end << " x " << path.start.position.x << " -> " << path.endPose.position.x
                  << " vx " << path.linearVelocity.x << '\n';
}

Result Run(Mode mode) {
    PhysicsWorld world;
    if (!world.Init()) throw std::runtime_error("could not initialize native world");
    const auto body = world.CreateDynamicBox(initial, {bodyHalf, bodyHalf, bodyHalf}, 1.f, 0.f, 0.f);
    if (!body.IsValid()) throw std::runtime_error("could not create body");
    if (mode == Mode::Wall &&
        !world.CreateStaticBox({wallCenter, .5f, 0.f}, {wallHalf, 2.f, 2.f}, 0.f, 0.f).IsValid())
        throw std::runtime_error("could not create wall");
    Result result;
    PhysicsConstraintParticipant participant;
    participant.bodies = {body};
    participant.propose = [&](const auto& batch, auto& proposal) {
        const unsigned step = batch.context.index;
        auto& observation = result.boundaries.at(step);
        ++result.proposals;
        unsigned owner = 0;
        while (batch.bodies.at(owner).handle.id != body.id)
            ++owner;
        observation.beforePosition = batch.bodies[owner].center.x;
        observation.beforeVelocity = batch.bodies[owner].linearVelocity.x;
        observation.contacts = unsigned(batch.contacts.size());
        observation.joints = unsigned(batch.joints.size());
        proposal.linearImpulses.assign(batch.bodies.size(), glm::dvec3(0));
        proposal.angularImpulses.assign(batch.bodies.size(), glm::dvec3(0));
        proposal.contactImpulses.assign(batch.contacts.size(), 0);
        proposal.jointImpulses.assign(batch.joints.size(), 0);
        if (step == 0)
            proposal.linearImpulses[owner].x = kick;
        if (mode == Mode::RejectSecond && step == 1)
            proposal.linearImpulses[owner].x = std::numeric_limits<double>::quiet_NaN();
        // Subsequent contacts in the wall trace use ordinary native response.
        // Declining here never stages or commits a second material impulse.
        return step == 0 || mode != Mode::Wall;
    };
    participant.published = [&](const auto& context) {
        auto& observation = result.boundaries.at(context.index);
        ++result.publications;
        observation.publishedPosition = world.GetTransform(body).position.x;
        observation.publishedVelocity = world.GetLinearVelocity(body).x;
    };
    PhysicsWorld::VelocityCoupling coupling;
    coupling.physicalBatch = &participant;
    coupling.afterSubstep = [&](const auto& context) {
        auto& observation = result.boundaries.at(context.index);
        ++result.observations;
        observation.afterPosition = world.GetTransform(body).position.x;
        observation.afterVelocity = world.GetLinearVelocity(body).x;
        observation.segments = world.GetBodyMotionSegments(body);
    };
    try {
        world.Step(dt, &coupling);
    } catch (const std::runtime_error& error) {
        result.error = error.what();
    }
    result.finalPosition = world.GetTransform(body).position.x;
    result.finalVelocity = world.GetLinearVelocity(body).x;
    result.macro = world.GetBodyMotionSegments(body);
    return result;
}

void Print(std::string_view name, const Result& result) {
    std::cout << name << " proposed " << result.proposals << " published " << result.publications
              << " observed " << result.observations << " final x " << result.finalPosition
              << " vx " << result.finalVelocity << " error " << result.error << '\n';
    for (unsigned i = 0; i < std::max(result.proposals, result.observations); ++i) {
        const auto& row = result.boundaries.at(i);
        std::cout << "substep " << i << " rows " << row.contacts << ' ' << row.joints
                  << " start " << row.beforePosition << ' ' << row.beforeVelocity
                  << " published " << row.publishedPosition << ' ' << row.publishedVelocity
                  << " observed " << row.afterPosition << ' ' << row.afterVelocity << '\n';
        PrintSegments("after", i, row.segments);
    }
    PrintSegments("after-Step", 0, result.macro);
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    const auto free = Run(Mode::Free);
    const auto wall = Run(Mode::Wall);
    const auto reject = Run(Mode::RejectSecond);
    Print("free", free);
    Print("wall", wall);
    Print("reject-second", reject);
    const auto& a = free.boundaries.front();
    const auto& b = wall.boundaries.front();
    const auto& c = reject.boundaries.front();
    const bool freeFlight = free.error.empty() && free.observations == 4 &&
        a.contacts == 0 && a.joints == 0 &&
        a.publishedPosition == a.beforePosition && a.publishedVelocity == kick &&
        a.afterPosition == kick * (dt / 4) && a.afterVelocity == kick;
    const bool lateWall = wall.observations > 0 && b.contacts == 0 && b.joints == 0 &&
        b.publishedPosition == a.publishedPosition && b.publishedVelocity == a.publishedVelocity &&
        b.afterPosition < a.afterPosition - .05f && b.afterPosition <= wallCenter - wallHalf - bodyHalf + .002f &&
        std::abs(b.afterVelocity) < kick * .1f && b.segments.size() >= 2;
    const bool partialStep = reject.error.find("nonfinite material impulse") != std::string::npos &&
        reject.publications == 1 && reject.observations == 1 && reject.proposals == 2 &&
        reject.finalPosition == c.afterPosition && reject.finalVelocity == c.afterVelocity &&
        reject.finalPosition != initial.x;
    std::cout << "SUMMARY free " << freeFlight << " late-CCD " << lateWall
              << " partial-abort " << partialStep << '\n';
    return freeFlight && lateWall && partialStep ? EXIT_SUCCESS : EXIT_FAILURE;
}
