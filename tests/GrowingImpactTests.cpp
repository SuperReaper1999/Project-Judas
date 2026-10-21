#include "../third_party/nlohmann/json.hpp"
#include "ImpactSolver.h"
#include "RigidBody.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace {
bool Bits(glm::vec3 a, glm::vec3 b) { return !std::memcmp(&a, &b, sizeof(a)); }
std::vector<RigidBody> Bodies(unsigned count) {
    std::vector<RigidBody> b(count);
    for (unsigned i = 0; i < count; ++i) {
        b[i].position = {float(i), 0, 0};
        b[i].inverseMass = 1;
        b[i].inverseInertiaLocal = SolidSphereInverseInertia(1, .5f);
    }
    return b;
}
ImpactContact Row(RigidBody &a, RigidBody &b) {
    Contact c;
    c.hit = true;
    c.hasLocalAnchors = true;
    c.signedSeparation = 0;
    c.penetration = 0;
    c.normal = {-1, 0, 0};
    c.preciseNormal = c.normal;
    c.localAnchorA = {.5, 0, 0};
    c.localAnchorB = {-.5, 0, 0};
    c.point = (a.position + b.position) * .5f;
    c.separationState = SeparationState::Touching;
    return {&a, &b, c, 0, 0, false};
}
} // namespace
int main() {
    unsigned checks = 0, failures = 0;
    nlohmann::json details = nlohmann::json::array();
    auto check = [&](bool condition, const char *name) {
        ++checks;
        if (!condition) {
            ++failures;
            std::cerr << "FAIL " << name << '\n';
        }
    };
    for (unsigned count : {2u, 3u, 32u, 100u}) {
        auto full = Bodies(count), growing = full;
        full[0].linearVelocity.x = growing[0].linearVelocity.x = 1;
        std::vector<ImpactContact> all, front;
        for (unsigned i = 0; i + 1 < count; ++i)
            all.push_back(Row(full[i], full[i + 1]));
        front.push_back(Row(growing[0], growing[1]));
        std::vector<unsigned char> expanded(count, 0);
        expanded[0] = expanded[1] = 1;
        ImpactSolver reference, incremental;
        const auto original = reference.Solve(all, 1.f / 60, true);
        ImpactResult result;
        const auto expand = [&](const std::vector<RigidBody *> &touched) {
            for (auto *body : touched) {
                unsigned i = unsigned(body - growing.data());
                if (i + 1 < count && !expanded[i + 1]) {
                    front.push_back(Row(growing[i], growing[i + 1]));
                    expanded[i + 1] = 1;
                }
            }
            return true;
        };
        bool accepted = incremental.SolveGrowing(front, 1.f / 60, expand, result), equal = true;
        for (unsigned i = 0; i < count; ++i)
            equal &= Bits(full[i].linearVelocity, growing[i].linearVelocity) &&
                     Bits(full[i].angularVelocity, growing[i].angularVelocity);
        check(accepted, "growing chain accepted");
        check(equal, "appended contacts retain exact full-chain physical solve");
        check(front.size() == count - 1, "no artificial propagation or island cap");
        check(original.energyBudgetSatisfied && result.energyBudgetSatisfied, "chain energy guard");
        bool impulseEqual = incremental.Constraints().size() == reference.Constraints().size();
        if (impulseEqual)
            for (unsigned i = 0; i < front.size(); ++i)
                impulseEqual &= incremental.Constraints()[i].normalImpulse ==
                                reference.Constraints()[i].normalImpulse;
        check(impulseEqual, "earlier impulses neither discarded nor applied twice");
        details.push_back({{"chain_bodies", count},
                           {"equal", equal},
                           {"far_velocity", growing.back().linearVelocity.x}});
    }
    {
        auto b = Bodies(3);
        b[0].linearVelocity.x = -1;
        b[2].linearVelocity.x = -.001f;
        const auto before = b;
        std::vector<ImpactContact> rows{Row(b[0], b[1]), Row(b[1], b[2])};
        rows[1].continuing = true;
        ImpactSolver s;
        ImpactResult result;
        auto nop = [](const auto &) { return true; };
        check(s.SolveGrowing(rows, 1.f / 60, nop, result), "continuing residual admissible");
        bool unchanged = true;
        for (unsigned i = 0; i < b.size(); ++i)
            unchanged &= Bits(b[i].linearVelocity, before[i].linearVelocity) &&
                         Bits(b[i].angularVelocity, before[i].angularVelocity);
        check(unchanged, "zero-effect arrival does not reproject unrelated accepted rest defect");
        check(std::all_of(s.Constraints().begin(), s.Constraints().end(),
                          [](const auto &c) { return c.normalImpulse == 0; }),
              "no hidden impact impulse on zero-effect arrival");
        b = before;
        rows = {Row(b[0], b[1]), Row(b[1], b[2])};
        check(s.SolveGrowing(rows, 1.f / 60, nop, result), "new small contact accepted");
        check(b[1].linearVelocity.x < 0,
              "same defect on NEW contact must produce physical response");
        b = Bodies(3);
        b[0].linearVelocity.x = 1;
        b[2].linearVelocity.x = -1;
        rows = {Row(b[0], b[1]), Row(b[1], b[2])};
        rows[1].continuing = true;
        check(s.SolveGrowing(rows, 1.f / 60, nop, result),
              "large old closure uses strict response");
        check(std::abs(b[1].linearVelocity.x - b[2].linearVelocity.x) < .00001f,
              "reject difference-projection 1m/s counterexample without weakening gate");
    }
    {
        auto b = Bodies(2);
        b[0].linearVelocity.x = .001f;
        std::vector<ImpactContact> rows{Row(b[0], b[1])};
        rows[0].continuing = true;
        rows[0].maxDefectDisplacement = .000001f;
        ImpactSolver solver;
        ImpactResult result;
        auto nop = [](const auto &) { return true; };
        check(solver.SolveGrowing(rows, 1.f / 60, nop, result),
              "small feature error bound accepted");
        check(std::abs(b[0].linearVelocity.x - b[1].linearVelocity.x) < .000001f,
              "smaller physical allowance forces strict correction rather than dropping collision");
        rows[0].maxDefectDisplacement = -1;
        bool rejected = false;
        try {
            solver.SolveGrowing(rows, 1.f / 60, nop, result);
        } catch (const std::invalid_argument &) {
            rejected = true;
        }
        check(rejected, "invalid negative allowance is rejected");
    }
    {
        auto b = Bodies(3);
        b[0].linearVelocity.x = 1;
        b[2].linearVelocity.x = -.001f;
        std::vector<ImpactContact> rows{Row(b[0], b[1])};
        bool added = false;
        const auto expand = [&](const std::vector<RigidBody *> &activated) {
            if (!added && std::find(activated.begin(), activated.end(), &b[1]) != activated.end()) {
                rows.push_back(Row(b[1], b[2]));
                rows.back().continuing = true;
                added = true;
            }
            return true;
        };
        ImpactSolver solver;
        ImpactResult result;
        check(solver.SolveGrowing(rows, 1.f / 60, expand, result),
              "late continuing contact accepted");
        check(std::abs((b[1].linearVelocity.x - b[2].linearVelocity.x) - .001f) < .00001f,
              "newly appended row references immutable PRE-impact momentum, not partially solved "
              "velocity");
        check(b[2].linearVelocity.x > .33f,
              "real new impact transmitted beyond continuing contact");
    }
    {
        const auto rotation = glm::angleAxis(1.13f, glm::normalize(glm::vec3(.7f, -.3f, .6f)));
        auto b = Bodies(3);
        b[0].linearVelocity.x = 1;
        for (auto &body : b) {
            body.position = rotation * body.position;
            body.orientation = rotation;
            body.linearVelocity = rotation * body.linearVelocity;
        }
        std::vector<ImpactContact> rows;
        for (unsigned i = 0; i < 2; ++i) {
            rows.push_back(Row(b[i], b[i + 1]));
            rows.back().geometry.normal = rotation * glm::vec3(-1, 0, 0);
            rows.back().geometry.preciseNormal = rows.back().geometry.normal;
        }
        ImpactSolver solver;
        ImpactResult result;
        check(solver.SolveGrowing(
                  rows, 1.f / 60, [](const auto &) { return true; }, result),
              "rotated physical response accepted");
        const auto expected = rotation * glm::vec3(1.f / 3, 0, 0);
        bool physical = true;
        for (auto &body : b)
            physical &= glm::length(body.linearVelocity - expected) < .00001f;
        check(physical, "contact propagation has no world-axis assumption");
    }
    {
        RigidBody moving, stationary, body;
        moving.linearVelocity.x = 1;
        body.inverseMass = 1;
        body.inverseInertiaLocal = glm::mat3(0);
        Contact one;
        one.hit = true;
        one.normal = {1, 0, 0};
        Contact two = one;
        two.normal = {-1, 0, 0};
        ContactSolver s;
        s.AddContact(body, moving, one, 0, 0);
        s.AddContact(body, stationary, two, 0, 0);
        s.Prepare(0);
        std::vector<RigidBody *> touched;
        s.SolveTrackedVelocities(10, touched);
        check(body.linearVelocity == glm::vec3(0),
              "balanced transient impulse restores final velocity");
        check(std::find(touched.begin(), touched.end(), &body) != touched.end(),
              "activation tracks transient response even when final velocity is unchanged");
    }
    {
        for (bool rotated : {false, true}) {
            auto b = Bodies(2);
            b[0].position = {1, 0, 0};
            b[1].position = {0, 0, 0};
            b[1].inverseMass = 0;
            b[1].inverseInertiaLocal = glm::mat3(0);
            b[1].linearVelocity = {3, 0, 0};
            auto c = Row(b[0], b[1]);
            c.geometry.normal = {1, 0, 0};
            c.geometry.preciseNormal = c.geometry.normal;
            c.geometry.localAnchorA = {-.5, 0, 0};
            c.geometry.localAnchorB = {.5, 0, 0};
            if (rotated) {
                const auto q = glm::angleAxis(.93f, glm::normalize(glm::vec3(.2f, .8f, -.3f)));
                for (auto &body : b) {
                    body.position = q * body.position;
                    body.linearVelocity = q * body.linearVelocity;
                    body.orientation = q;
                }
                c.geometry.normal = q * c.geometry.normal;
                c.geometry.preciseNormal = c.geometry.normal;
            }
            const auto prescribed = b[1];
            std::vector<ImpactContact> rows{c};
            ImpactSolver solver;
            ImpactResult result;
            check(solver.SolveGrowing(
                      rows, 1.f / 60, [](const auto &) { return true; }, result),
                  "prescribed boundary work accepted");
            check(glm::length(b[0].linearVelocity - prescribed.linearVelocity) < .00001f,
                  "moving boundary produces real body motion");
            check(result.kineticAfter > result.kineticBefore &&
                      std::abs(result.prescribedBoundaryWork - 9) < .0001,
                  "external boundary work measured, not hidden as free energy");
            check(Bits(b[1].linearVelocity, prescribed.linearVelocity) &&
                      Bits(b[1].angularVelocity, prescribed.angularVelocity),
                  "infinite-mass boundary is unaffected by reaction impulse");
        }
        auto b = Bodies(2);
        b[0].position = {-1, .5, 0};
        b[1].position = {0, 0, 0};
        b[1].inverseMass = 0;
        b[1].inverseInertiaLocal = glm::mat3(0);
        b[1].angularVelocity = {0, 0, 4};
        auto c = Row(b[0], b[1]);
        c.geometry.localAnchorB = {-.5, .5, 0};
        c.geometry.point = {-.5, .5, 0};
        std::vector<ImpactContact> rows{c};
        ImpactSolver solver;
        ImpactResult result;
        check(solver.SolveGrowing(
                  rows, 1.f / 60, [](const auto &) { return true; }, result),
              "rotating boundary work accepted");
        check(std::abs(b[0].linearVelocity.x + 2) < .00001f &&
                  std::abs(result.prescribedBoundaryWork - 4) < .0001,
              "rotating boundary uses contact-point surface velocity");
    }
    {
        auto b = Bodies(2);
        b[0].inverseMass = 1e-20f;
        b[0].inverseInertiaLocal = glm::mat3(0);
        b[0].linearVelocity.x = 1;
        const auto incoming = b[0].linearVelocity;
        const auto row = Row(b[0], b[1]);
        ContactSolver solver;
        solver.AddContact(b[0], b[1], row.geometry, 0, 0);
        solver.Prepare(0);
        std::vector<RigidBody *> touched;
        solver.SolveTrackedVelocities(1, touched);
        check(Bits(incoming, b[0].linearVelocity),
              "rounded-away reaction leaves heavy body unchanged");
        check(std::find(touched.begin(), touched.end(), &b[0]) == touched.end(),
              "unchanged represented body creates no downstream velocity dependency");
        check(std::find(touched.begin(), touched.end(), &b[1]) != touched.end() &&
                  b[1].linearVelocity.x > .99f,
              "the same real impulse still activates its responsive endpoint");
    }
    std::cout
        << nlohmann::json{{"checks", checks}, {"failures", failures}, {"details", details}}.dump(2)
        << '\n';
    return failures ? 1 : 0;
}
