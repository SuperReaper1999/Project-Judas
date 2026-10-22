#include "ContactSolver.h"
#include "RigidBody.h"
#include <algorithm>
#include <cstdio>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("FAIL %s\n", message);
    }
}
void Add(ContactSolver& solver, RigidBody& body, RigidBody& frame,
         glm::vec3 normal, float preload) {
    Contact contact;
    contact.hit = contact.hasLocalAnchors = true;
    contact.normal = normal;
    contact.preciseNormal = glm::dvec3(normal);
    // Opposite faces of a prescribed enclosing frame. Axial lever arms
    // introduce no angular response; every expected cancellation is exact.
    contact.localAnchorA = glm::dvec3(normal) * .5;
    contact.localAnchorB = contact.localAnchorA;
    solver.AddContact(body, frame, contact, 0, 0, preload);
}
bool Contains(const std::vector<RigidBody*>& activated, RigidBody& body) {
    return std::find(activated.begin(), activated.end(), &body) != activated.end();
}
}

int main() {
    {
        RigidBody body, frame;
        body.inverseMass = 1;
        ContactSolver solver;
        solver.EnableBlockNormals(true);
        Add(solver, body, frame, {0, 1, 0}, 1);
        Add(solver, body, frame, {0, -1, 0}, 1);
        solver.Prepare();
        Check(body.linearVelocity == glm::vec3(0), "balanced preload leaves velocity unchanged");
        // Release both previous compressive reactions. The relaxed targets
        // make zero reaction the valid solution. The two represented changes
        // are -1 and +1 m/s inside ONE pair operation: no third constraint
        // observes the temporary -1 m/s velocity.
        solver.SetInelasticVelocityTarget(0, -1);
        solver.SetInelasticVelocityTarget(1, -1);
        std::vector<RigidBody*> activated;
        solver.SolveTrackedVelocities(1, activated);
        Check(body.linearVelocity == glm::vec3(0) && body.angularVelocity == glm::vec3(0),
              "released opposing reactions cancel exactly at block boundary");
        Check(solver.Constraints()[0].normalImpulse == 0 && solver.Constraints()[1].normalImpulse == 0,
              "both actual normal reactions are released");
        Check(activated.empty(), "unobservable internal block transient does not activate neighbors");
    }
    {
        RigidBody body, lowerFrame, upperFrame;
        body.inverseMass = 1;
        ContactSolver solver;
        solver.EnableBlockNormals(true);
        for (unsigned point = 0; point < 2; ++point) Add(solver, body, lowerFrame, {0, 1, 0}, .5f);
        for (unsigned point = 0; point < 2; ++point) Add(solver, body, upperFrame, {0, -1, 0}, .5f);
        solver.Prepare();
        for (unsigned row = 0; row < 4; ++row) solver.SetInelasticVelocityTarget(row, -1);
        std::vector<RigidBody*> activated;
        solver.SolveTrackedVelocities(1, activated);
        Check(body.linearVelocity == glm::vec3(0) && body.angularVelocity == glm::vec3(0),
              "separate block responses cancel over full iteration");
        Check(Contains(activated, body), "observable first block change survives later block cancellation");
        Check(activated.size() == 1, "dynamic participant reported once and static boundaries excluded");
        for (const auto& row : solver.Constraints())
            Check(row.normalImpulse == 0, "separate block reactions released without changing arithmetic");
    }
    {
        RigidBody body, frame;
        body.inverseMass = 1;
        body.linearVelocity = {0, -0x1p-23f, 0};
        ContactSolver solver;
        solver.EnableBlockNormals(true);
        Add(solver, body, frame, {0, 1, 0}, 0);
        Add(solver, body, frame, {0, 1, 0}, 0);
        solver.Prepare();
        std::vector<RigidBody*> activated;
        solver.SolveTrackedVelocities(1, activated);
        Check(Contains(activated, body), "tiny genuine block output activates without magnitude cutoff");
        Check(body.linearVelocity.y > -0x1p-23f, "tiny genuine closing speed is resolved");
    }
    {
        RigidBody body, frame;
        body.inverseMass = 1;
        ContactSolver solver;
        solver.EnableBlockNormals(true);
        Add(solver, body, frame, {0, 1, 0}, 1);
        Add(solver, body, frame, {0, -1, 0}, 1);
        // Joint/coupled consumers deliberately retain individual point rows.
        solver.PreservePointNormals(body);
        solver.Prepare();
        solver.SetInelasticVelocityTarget(0, -1);
        solver.SetInelasticVelocityTarget(1, -1);
        std::vector<RigidBody*> activated;
        solver.SolveTrackedVelocities(1, activated);
        Check(body.linearVelocity == glm::vec3(0), "point rows retain original exact cancellation");
        Check(Contains(activated, body), "preserved point rows still publish their intermediate outputs");
    }
    std::printf("Block activation: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
