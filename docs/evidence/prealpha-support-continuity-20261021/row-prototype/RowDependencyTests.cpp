#include "RowDependency.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
RigidBody Dynamic() {
    RigidBody body;
    body.inverseMass = 1;
    body.inverseInertiaLocal = glm::mat3(1);
    return body;
}
Contact ContactAt(glm::vec3 normal, glm::vec3 offsetA = glm::vec3(0),
                  glm::vec3 offsetB = glm::vec3(0)) {
    Contact contact;
    contact.hit = true;
    contact.hasLocalAnchors = true;
    contact.signedSeparation = 0;
    contact.normal = normal;
    contact.preciseNormal = normal;
    contact.localAnchorA = offsetA;
    contact.localAnchorB = offsetB;
    return contact;
}
struct Pair {
    std::vector<RigidBody> full, tested;
    ContactSolver reference, candidate;
    row_dependency::Graph graph{&candidate};
    row_dependency::Scope scope{graph};
    explicit Pair(std::vector<RigidBody> bodies) : full(bodies), tested(bodies) {}
    void Add(unsigned a, unsigned b, Contact contact, float friction = 0, float warm = 0) {
        reference.AddContact(full[a],full[b],contact,friction,0,warm);
        candidate.AddContact(tested[a],tested[b],contact,friction,0,warm);
    }
    void Prepare() { reference.Prepare(0); candidate.Prepare(0); }
    void Solve(int iterations = ContactSolver::kVelocityIterations, std::size_t first = 0) {
        std::vector<RigidBody*> x, y;
        reference.SolveTrackedVelocityRange(first,iterations,x);
        candidate.SolveTrackedVelocityRange(first,iterations,y);
        bool sameActivation = x.size() == y.size();
        if (sameActivation)
            for (std::size_t i = 0; i < x.size(); ++i)
                sameActivation &= x[i] - full.data() == y[i] - tested.data();
        Check(sameActivation,"exact transient activation sequence");
    }
    bool Equal() const {
        bool equal = reference.Constraints().size() == candidate.Constraints().size();
        for (std::size_t i = 0; i < full.size(); ++i)
            equal &= row_dependency::SameVector(full[i].linearVelocity,tested[i].linearVelocity) &&
                     row_dependency::SameVector(full[i].angularVelocity,tested[i].angularVelocity);
        if (reference.Constraints().size() == candidate.Constraints().size())
            for (std::size_t i = 0; i < reference.Constraints().size(); ++i) {
                const auto& a = reference.Constraints()[i];
                const auto& b = candidate.Constraints()[i];
                equal &= row_dependency::SameFloat(a.normalImpulse,b.normalImpulse) &&
                         row_dependency::SameVector(a.tangentImpulse,b.tangentImpulse);
            }
        return equal;
    }
};
} // namespace

int main(int argc, char** argv) {
    using row_dependency::Control;
    if (argc > 1) {
        const std::string argument = argv[1];
        if (argument == "--broken-endpoint") row_dependency::SetControl(Control::ForgetEndpointInvalidation);
        else if (argument == "--broken-own") row_dependency::SetControl(Control::ForgetOwnAccumulator);
        else if (argument == "--broken-target") row_dependency::SetControl(Control::ForgetTargetInvalidation);
        else { std::cerr << "unknown private negative control\n"; return 2; }
    }
    row_dependency::SetStatsEnabled(true);
    row_dependency::ResetStats();
    {
        Pair pair({Dynamic(),RigidBody{}});
        pair.Add(0,1,ContactAt({1,0,0})); pair.Prepare();
        const auto before = row_dependency::stats.skipped;
        pair.Solve();
        Check(pair.Equal(),"idempotent quiescent rows preserve exact state");
        Check(row_dependency::stats.skipped - before == 9,
              "first evaluation certifies subsequent nine ordered visits");
    }
    {
        auto a = Dynamic(), b = Dynamic(), c = Dynamic(); c.linearVelocity.x = -1;
        Pair pair({a,b,c});
        pair.Add(0,1,ContactAt({-1,0,0}));
        pair.Add(1,2,ContactAt({-1,0,0})); pair.Prepare(); pair.Solve();
        Check(pair.Equal(),"later neighbor response reactivates earlier clean row next sweep");
    }
    {
        auto heavy = Dynamic();
        heavy.linearVelocity = {1e8f,0,0};
        heavy.angularVelocity = {0,0,1e8f};
        RigidBody moving; moving.linearVelocity = {1,0,0};
        Pair pair({heavy,moving});
        pair.Add(0,1,ContactAt({1,0,0},{0,1,0})); pair.Prepare();
        const auto before = row_dependency::stats.accumulatorOnlyChanges;
        pair.Solve();
        Check(pair.Equal(),"own-accumulator changes cannot be declared clean from rounded body updates");
        Check(row_dependency::SameVector(pair.tested[0].linearVelocity,heavy.linearVelocity) &&
              row_dependency::SameVector(pair.tested[0].angularVelocity,heavy.angularVelocity),
              "accumulator-only fixture preserves represented body velocities");
        Check(pair.reference.Constraints()[0].normalImpulse == 5,
              "unchanged exact ten-sweep oracle accumulates all ten half-unit increments");
        Check(row_dependency::stats.accumulatorOnlyChanges - before == 10,
              "all ten own-state-only evaluations remain necessary");
    }
    {
        auto body = Dynamic(); RigidBody moving, stationary;
        moving.linearVelocity.x = 1;
        Pair pair({body,moving,stationary});
        pair.Add(0,1,ContactAt({1,0,0}));
        pair.Add(0,2,ContactAt({-1,0,0})); pair.Prepare(); pair.Solve();
        Check(pair.Equal(),"opposing transient responses retain exact accumulators");
        Check(pair.tested[0].linearVelocity == glm::vec3(0),
              "transiently activated body's final velocity cancels without losing discovery");
    }
    {
        auto body = Dynamic(); body.linearVelocity.x = -1;
        Pair pair({body,RigidBody{}});
        pair.Add(0,1,ContactAt({1,0,0})); pair.Prepare(); pair.Solve(3);
        pair.reference.SetInelasticVelocityTarget(0,-.25f);
        pair.candidate.SetInelasticVelocityTarget(0,-.25f);
        pair.Solve(2);
        Check(pair.Equal(),"explicit retarget invalidates otherwise identical endpoint certificate");
        Check(pair.reference.Constraints()[0].normalImpulse == .75f &&
              pair.full[0].linearVelocity.x == -.25f,"retarget fixture changes real reaction");
    }
    {
        Pair pair({Dynamic(),RigidBody{}});
        pair.Add(0,1,ContactAt({1,0,0})); pair.Prepare(); pair.Solve(3);
        pair.full[0].linearVelocity.x = pair.tested[0].linearVelocity.x = -1;
        pair.Solve(2);
        Check(pair.Equal(),"external body velocity edits are caught by exact lazy observation");
    }
    {
        Pair pair({Dynamic(),Dynamic(),RigidBody{}});
        pair.Add(0,1,ContactAt({-1,0,0})); pair.Prepare(); pair.Solve(3);
        const auto firstFull = pair.reference.FrameNodeRequests();
        const auto firstTested = pair.candidate.FrameNodeRequests();
        pair.Add(1,2,ContactAt({-1,0,0}),0,1);
        pair.reference.PrepareAppended(0,1,firstFull);
        pair.candidate.PrepareAppended(0,1,firstTested);
        pair.Solve();
        Check(pair.Equal(),"new warm row invalidates old endpoint dependencies and remains ordered");
    }
    {
        Pair pair({Dynamic(),RigidBody{}});
        pair.Add(0,1,ContactAt({1,0,0})); pair.Prepare(); pair.Solve(3);
        pair.reference.Clear(); pair.candidate.Clear();
        pair.full[0].linearVelocity.x = pair.tested[0].linearVelocity.x = -1;
        pair.Add(0,1,ContactAt({1,0,0})); pair.Prepare(); pair.Solve();
        Check(pair.Equal(),"Clear and reused body/frame indices cannot retain stale certificates");
    }
    std::cout << "{\"checks\":" << checks << ",\"failures\":" << failures
              << ",\"considered\":" << row_dependency::stats.considered
              << ",\"evaluated\":" << row_dependency::stats.evaluated
              << ",\"skipped\":" << row_dependency::stats.skipped
              << ",\"accumulator_only\":" << row_dependency::stats.accumulatorOnlyChanges << "}\n";
    return failures ? 1 : 0;
}
