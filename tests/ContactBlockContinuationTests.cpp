#include "ContactSolver.h"
#include "RigidBody.h"
#include <cstdio>
#include <random>

// A continuing contact may retain its genuine incoming closing-speed target.
// If no impact has changed either velocity, resolving that SAME target must
// apply no impulse and must not activate neighbours, including at oblique normals.
int main() {
    unsigned checks = 0, failures = 0;
    std::mt19937 random(0x22c0ffee);
    const auto value = [&] { return (int(random() % 20001) - 10000) / 10000.f; };
    for (unsigned sample = 0; sample < 256; ++sample) {
        RigidBody body, floor;
        body.inverseMass = 1;
        body.inverseInertiaLocal = SolidBoxInverseInertia(1, {.3f, .4f, .2f});
        body.linearVelocity = {value(), value(), value()};
        body.angularVelocity = {value(), value(), value()};
        const auto normal = glm::normalize(glm::vec3(value(), value(), value()));
        ContactSolver solver;
        solver.EnableBlockNormals(true);
        for (unsigned corner = 0; corner < 4; ++corner) {
            Contact contact;
            contact.hit = contact.hasLocalAnchors = true;
            contact.normal = normal;
            contact.preciseNormal = glm::dvec3(normal);
            contact.localAnchorA = {corner & 1 ? .2 : -.2, -.4, corner & 2 ? .15 : -.15};
            contact.localAnchorB = contact.localAnchorA;
            solver.AddContact(body, floor, contact, 0, 0);
        }
        solver.Prepare();
        for (unsigned i = 0; i < 4; ++i) {
            const auto& row = solver.Constraints()[i];
            const auto relative = body.linearVelocity +
                                  glm::cross(body.angularVelocity, row.velocityOffsetA);
            solver.SetInelasticVelocityTarget(i, std::min(glm::dot(relative, normal), 0.f));
        }
        const auto linear = body.linearVelocity, angular = body.angularVelocity;
        std::vector<RigidBody*> activated;
        solver.SolveTrackedVelocities(1, activated);
        ++checks;
        if (body.linearVelocity != linear || body.angularVelocity != angular || !activated.empty()) {
            ++failures;
            if (failures <= 8)
                std::printf("FAIL unchanged oblique baseline activated a body at sample %u\n", sample);
        }
    }
    // A valid accumulated normal solution must not acquire a new reaction
    // solely because the matrix is factorized again in a later sweep.
    for (unsigned sample = 0; sample < 128; ++sample) {
        RigidBody body, floor;
        body.inverseMass = 1;
        body.inverseInertiaLocal = SolidBoxInverseInertia(1, {.3f, .4f, .2f});
        const auto normal = glm::normalize(glm::vec3(value(), value(), value()));
        body.linearVelocity = -normal;
        ContactSolver solver;
        solver.EnableBlockNormals(true);
        for (unsigned corner = 0; corner < 4; ++corner) {
            Contact contact;
            contact.hit = contact.hasLocalAnchors = true;
            contact.normal = normal;
            contact.preciseNormal = glm::dvec3(normal);
            contact.localAnchorA = {corner & 1 ? .2 : -.2, -.4, corner & 2 ? .15 : -.15};
            contact.localAnchorB = contact.localAnchorA;
            solver.AddContact(body, floor, contact, 0, 0);
        }
        solver.Prepare();
        solver.SolveVelocities(1);
        body.linearVelocity = -normal;
        body.angularVelocity = {0, 0, 0};
        for (unsigned row = 0; row < 4; ++row)
            solver.SetInelasticVelocityTarget(row, glm::dot(body.linearVelocity, normal));
        const auto linear = body.linearVelocity;
        std::vector<RigidBody*> activated;
        solver.SolveTrackedVelocities(1, activated);
        ++checks;
        if (body.linearVelocity != linear || body.angularVelocity != glm::vec3(0) || !activated.empty()) {
            ++failures;
            if (failures <= 8)
                std::printf("FAIL valid accumulated normal solution drifted at sample %u\n", sample);
        }
    }
    {
        RigidBody body, floor;
        body.inverseMass = 1;
        body.linearVelocity = {0, -std::numeric_limits<float>::epsilon(), 0};
        ContactSolver solver;
        solver.EnableBlockNormals(true);
        for (unsigned corner = 0; corner < 4; ++corner) {
            Contact contact;
            contact.hit = contact.hasLocalAnchors = true;
            contact.normal = contact.preciseNormal = {0, 1, 0};
            contact.localAnchorA = {corner & 1 ? .2 : -.2, 0, corner & 2 ? .2 : -.2};
            contact.localAnchorB = contact.localAnchorA;
            solver.AddContact(body, floor, contact, 0, 0);
        }
        solver.Prepare();
        std::vector<RigidBody*> activated;
        solver.SolveTrackedVelocities(1, activated);
        ++checks;
        if (activated.empty() || std::abs(body.linearVelocity.y) > 1e-12f) {
            ++failures;
            std::printf("FAIL tiny genuine closing velocity was not resolved\n");
        }
    }
    std::printf("Block continuation: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
