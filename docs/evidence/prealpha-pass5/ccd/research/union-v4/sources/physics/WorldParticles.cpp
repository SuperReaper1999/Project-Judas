#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

namespace {
// Algebraic preconditioner for the auxiliary normal constraints only. The
// existing solver still owns their target, nonnegative impulse and friction.
// A = J M^-1 J^T is applied from prepared lever arms/inertias, without a
// dense contact matrix or any body-mass classification.
bool AccelerateParticleNormals(const std::vector<ContactConstraint> &rows,
                               PhysicsWorld::ParticleContactStats &stats,
                               std::vector<float> &normal, std::vector<glm::vec3> &tangent) {
    struct Frame {
        const RigidBody *body = nullptr;
        glm::dmat3 inverseInertia{0};
    };
    struct Row {
        std::size_t original, a, b;
        glm::dvec3 n, angularA, angularB;
        double diagonal;
    };
    struct Wrench {
        glm::dvec3 linear{0}, angular{0};
    };
    std::size_t frameCount = 0;
    for (const auto &c : rows)
        frameCount = std::max(frameCount, std::max(c.frameA, c.frameB) + 1);
    std::vector<Frame> frames(frameCount);
    std::vector<Row> active;
    std::vector<double> residual;
    normal.reserve(rows.size());
    tangent.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &c = rows[i];
        normal.push_back(c.normalImpulse);
        tangent.push_back(c.tangentImpulse);
        frames[c.frameA] = {c.bodyA, glm::dmat3(c.inverseInertiaA)};
        frames[c.frameB] = {c.bodyB, glm::dmat3(c.inverseInertiaB)};
        const glm::dvec3 n(c.normal), ra(c.velocityOffsetA), rb(c.velocityOffsetB);
        const glm::dvec3 angularA = glm::cross(ra, n), angularB = glm::cross(rb, n);
        const double speed =
            glm::dot(n, glm::dvec3(c.bodyA->linearVelocity) - glm::dvec3(c.bodyB->linearVelocity)) +
            glm::dot(angularA, glm::dvec3(c.bodyA->angularVelocity)) -
            glm::dot(angularB, glm::dvec3(c.bodyB->angularVelocity));
        const double r = double(c.restitutionBias) - speed;
        if (!(c.normalImpulse > 0 || r > 0))
            continue;
        // Optional PCG is restricted to homogeneous prescribed normal motion.
        // A moving infinite-mass boundary can supply unbounded constraint work
        // when nearly opposing coarse particle rows form a narrow wedge. The
        // ordinary bounded PGS still processes every such row unchanged.
        const double prescribedA = glm::dot(n, glm::dvec3(c.bodyA->linearVelocity)) +
                                   glm::dot(angularA, glm::dvec3(c.bodyA->angularVelocity));
        const double prescribedB = glm::dot(n, glm::dvec3(c.bodyB->linearVelocity)) +
                                   glm::dot(angularB, glm::dvec3(c.bodyB->angularVelocity));
        if ((c.bodyA->IsStatic() && prescribedA != 0) ||
            (c.bodyB->IsStatic() && prescribedB != 0)) {
            ++stats.normalAccelerationBoundaryDeclines;
            return false;
        }
        const double diagonal =
            (double(c.bodyA->inverseMass) + c.bodyB->inverseMass) * glm::dot(n, n) +
            glm::dot(angularA, frames[c.frameA].inverseInertia * angularA) +
            glm::dot(angularB, frames[c.frameB].inverseInertia * angularB);
        if (!(diagonal > 0) || !std::isfinite(diagonal)) {
            ++stats.normalAccelerationBreakdowns;
            return false;
        }
        active.push_back({i, c.frameA, c.frameB, n, angularA, angularB, diagonal});
        residual.push_back(r);
    }
    if (active.empty())
        return false;
    std::vector<Wrench> velocity(frames.size());
    const auto multiply = [&](const std::vector<double> &x, std::vector<double> &output) {
        std::fill(velocity.begin(), velocity.end(), Wrench{});
        for (std::size_t i = 0; i < active.size(); ++i) {
            const auto &c = active[i];
            velocity[c.a].linear += c.n * x[i];
            velocity[c.a].angular += c.angularA * x[i];
            velocity[c.b].linear -= c.n * x[i];
            velocity[c.b].angular -= c.angularB * x[i];
        }
        for (std::size_t i = 0; i < frames.size(); ++i)
            if (frames[i].body) {
                velocity[i].linear *= double(frames[i].body->inverseMass);
                velocity[i].angular = frames[i].inverseInertia * velocity[i].angular;
            }
        for (std::size_t i = 0; i < active.size(); ++i) {
            const auto &c = active[i];
            output[i] = glm::dot(c.n, velocity[c.a].linear - velocity[c.b].linear) +
                        glm::dot(c.angularA, velocity[c.a].angular) -
                        glm::dot(c.angularB, velocity[c.b].angular);
        }
        ++stats.normalMatrixProducts;
    };
    const auto dot = [](const std::vector<double> &a, const std::vector<double> &b) {
        double value = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
            value += a[i] * b[i];
        return value;
    };
    const auto initialResidual = residual;
    std::vector<double> correction(active.size(), 0), z(active.size()), direction(active.size()),
        product(active.size());
    for (std::size_t i = 0; i < active.size(); ++i)
        z[i] = residual[i] / active[i].diagonal;
    direction = z;
    double rz = dot(residual, z);
    const double tolerance = 1.0e-10 * stats.speedScale;
    // Exact CG terminates in at most the active operator's rank. Redundant
    // manifold rows make it positive semidefinite; zero/nonfinite curvature
    // stops this optional acceleration, never inserts artificial stiffness.
    for (std::size_t iteration = 0; iteration < active.size(); ++iteration) {
        double largest = 0;
        for (double r : residual)
            largest = std::max(largest, std::abs(r));
        if (largest <= tolerance)
            break;
        multiply(direction, product);
        const double curvature = dot(direction, product);
        if (!(curvature > 0) || !std::isfinite(curvature) || !std::isfinite(rz)) {
            ++stats.normalAccelerationBreakdowns;
            return false;
        }
        const double step = rz / curvature;
        for (std::size_t i = 0; i < active.size(); ++i) {
            correction[i] += step * direction[i];
            residual[i] -= step * product[i];
        }
        for (std::size_t i = 0; i < active.size(); ++i)
            z[i] = residual[i] / active[i].diagonal;
        const double next = dot(residual, z);
        if (rz == 0)
            break;
        for (std::size_t i = 0; i < active.size(); ++i)
            direction[i] = z[i] + (next / rz) * direction[i];
        rz = next;
    }
    // A line search preserves the same accumulated-normal-impulse cone.
    // Also require strict descent of the frozen-tangent normal quadratic.
    double fraction = 1;
    for (std::size_t i = 0; i < active.size(); ++i) {
        if (!std::isfinite(correction[i])) {
            ++stats.normalAccelerationBreakdowns;
            return false;
        }
        if (correction[i] < 0) {
            const auto row = active[i].original;
            // Tangential impulse is frozen during the normal acceleration.
            // Its already-satisfied Coulomb load must remain available.
            const double minimum = rows[row].friction > 0
                                       ? glm::length(glm::dvec3(tangent[row])) / rows[row].friction
                                       : 0;
            fraction =
                std::min(fraction, std::max(0.0, double(normal[row]) - minimum) / -correction[i]);
        }
    }
    if (!(fraction > 0))
        return false;
    multiply(correction, product);
    const double decrease = fraction * dot(initialResidual, correction) -
                            .5 * fraction * fraction * dot(correction, product);
    if (!(decrease > 0) || !std::isfinite(decrease))
        return false;
    bool changed = false;
    for (std::size_t i = 0; i < active.size(); ++i) {
        const auto row = active[i].original;
        // Nonnegative accumulated impulse is the existing contact law, not
        // a velocity repair. The line search reaches its boundary exactly.
        const float value =
            static_cast<float>(std::max(0.0, double(normal[row]) + fraction * correction[i]));
        if (!std::isfinite(value)) {
            ++stats.normalAccelerationBreakdowns;
            return false;
        }
        changed = changed || value != normal[row];
        normal[row] = value;
    }
    return changed;
}
} // namespace

void PhysicsWorld::SolveParticleContacts(std::vector<ContactParticle> &particles,
                                         const std::vector<ParticleBoundaryContact> &contacts,
                                         std::vector<glm::vec3> &particleImpulses,
                                         glm::vec3 *staticSupportImpulse, float remainingRigidDt) {
    for (const auto &contact : contacts)
        if (contact.twoWay)
            Wake(contact.body);
    m_impl->particleContactStats = {};
    particleImpulses.assign(contacts.size(), glm::vec3(0));
    if (staticSupportImpulse)
        *staticSupportImpulse = glm::vec3(0);
    if (!std::isfinite(remainingRigidDt) || remainingRigidDt < 0)
        throw std::invalid_argument("invalid remaining rigid contact interval");
    if (contacts.empty())
        return;
    Impl &w = *m_impl;
    const auto finite = [](const glm::vec3 &v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    std::set<unsigned> islandSlots;
    std::vector<unsigned> pending;
    for (const auto &c : contacts) {
        if (c.particleIndex >= particles.size() || !finite(c.point) || !finite(c.normal) ||
            !finite(c.wallVelocity) || std::abs(glm::dot(c.normal, c.normal) - 1.0f) > 1.0e-3f)
            throw std::invalid_argument("invalid particle boundary contact");
        const auto &particle = particles[c.particleIndex];
        if (!(particle.mass > 0) || !std::isfinite(particle.mass) ||
            !std::isfinite(1.0f / particle.mass) || !finite(particle.position) ||
            !finite(particle.velocity))
            throw std::invalid_argument("invalid finite particle state");
        if (c.twoWay) {
            auto *body = w.Get(c.body);
            if (!body)
                throw std::invalid_argument("stale particle boundary owner");
            const unsigned slot = c.body.id & Impl::kSlotMask;
            if (islandSlots.insert(slot).second && !body->rigidBody.IsStatic())
                pending.push_back(slot);
        }
    }
    float maximumParticleSpeed = 0, maximumBodySpeed = 0;
    for (const auto &p : particles)
        maximumParticleSpeed = std::max(maximumParticleSpeed, glm::length(p.velocity));
    const auto surfaceSpeed = [](const Impl::Body &body) {
        return glm::length(body.rigidBody.linearVelocity) +
               glm::length(body.rigidBody.angularVelocity) * body.boundingRadius;
    };
    if (remainingRigidDt > 0)
        for (const auto slot : w.aliveSlots)
            maximumBodySpeed = std::max(maximumBodySpeed, surfaceSpeed(w.bodies[slot]));
    struct RigidRow {
        unsigned a, b;
        Contact contact;
    };
    std::vector<RigidRow> rigidRows;
    std::set<std::pair<unsigned, unsigned>> inspected;
    std::vector<char> supportCacheUsed(w.contactCache.size(), 0);
    // Expand only through dynamic bodies. A static floor is an external
    // boundary, not a connection to every otherwise unrelated body on it.
    for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
        const unsigned slot = pending[cursor];
        const float queryMargin =
            (surfaceSpeed(w.bodies[slot]) + maximumBodySpeed + maximumParticleSpeed) *
            remainingRigidDt;
        const auto neighbors = w.QuerySlots(w.CurrentBound(w.bodies[slot]).Expanded(queryMargin));
        for (const unsigned other : neighbors) {
            if (other == slot || !w.CanRespond(slot, other))
                continue;
            const auto pair = std::minmax(slot, other);
            if (!inspected.emplace(pair.first, pair.second).second)
                continue;
            auto &a = w.bodies[pair.first];
            auto &b = w.bodies[pair.second];
            const auto &oa = w.Orientation(a);
            const auto &ob = w.Orientation(b);
            bool touching = false;
            const float margin =
                (surfaceSpeed(a) + surfaceSpeed(b) + maximumParticleSpeed) * remainingRigidDt;
            for (int pa = 0; pa < PrimitiveCount(a.shape); ++pa) {
                const auto childA = PrimitiveAt(a.shape, a.rigidBody, pa, &oa);
                for (int pb = 0; pb < PrimitiveCount(b.shape); ++pb) {
                    const auto childB = PrimitiveAt(b.shape, b.rigidBody, pb, &ob);
                    const auto manifold = ComputeContacts(childA, childB, margin, &oa, &ob);
                    for (int k = 0; k < manifold.count; ++k) {
                        const auto &c = manifold.points[k];
                        if (!c.hit)
                            continue;
                        const double separation =
                            c.hasLocalAnchors ? c.signedSeparation : -double(c.penetration);
                        if (separation > 0) {
                            // Reuse only the ordinary solver's existing support
                            // identity/anchor/normal contract. New separated pairs
                            // still belong to normal impact timing, not this batch.
                            if (!(remainingRigidDt > 0))
                                continue;
                            const ContactKey key{pair.first,   pair.second, a.generation,
                                                 b.generation, pa,          pb};
                            const auto range = std::equal_range(
                                w.contactCache.begin(), w.contactCache.end(),
                                CachedContact{key, {}, {}, 0, {}},
                                [](const auto &x, const auto &y) { return x.key < y.key; });
                            const glm::dvec3 anchor =
                                c.hasLocalAnchors
                                    ? c.localAnchorA
                                    : glm::transpose(oa.rotation) *
                                          (glm::dvec3(c.point) - glm::dvec3(a.rigidBody.position));
                            std::ptrdiff_t best = -1;
                            double bestDistance = kWarmStartAnchorDistance;
                            for (auto old = range.first; old != range.second; ++old) {
                                const auto oldIndex = old - w.contactCache.begin();
                                if (supportCacheUsed[std::size_t(oldIndex)] ||
                                    !old->key.SameBodies(key) ||
                                    glm::dot(old->normal, c.normal) < kWarmStartNormalDot)
                                    continue;
                                const double distance = glm::distance(old->localAnchorA, anchor);
                                if (distance < bestDistance) {
                                    best = oldIndex;
                                    bestDistance = distance;
                                }
                            }
                            if (best < 0)
                                continue;
                            supportCacheUsed[std::size_t(best)] = 1;
                            ++w.particleContactStats.persistentGapRows;
                        }
                        touching = true;
                        rigidRows.push_back({pair.first, pair.second, c});
                    }
                }
            }
            if (touching && islandSlots.insert(other).second &&
                !w.bodies[other].rigidBody.IsStatic())
                pending.push_back(other);
        }
    }
    std::stable_sort(rigidRows.begin(), rigidRows.end(), [](const auto &a, const auto &b) {
        return std::tie(a.a, a.b) < std::tie(b.a, b.b);
    });
    // Finish allocation before any solver stores pointers into these arrays.
    std::vector<RigidBody> particleBodies(particles.size());
    for (std::size_t i = 0; i < particles.size(); ++i) {
        particleBodies[i].position = particles[i].position;
        particleBodies[i].linearVelocity = particles[i].velocity;
        particleBodies[i].inverseMass = particles[i].mass > 0 ? 1.0f / particles[i].mass : 0;
    }
    std::vector<RigidBody> rigidBodies;
    rigidBodies.reserve(islandSlots.size() + contacts.size());
    std::map<unsigned, std::size_t> index;
    for (const unsigned slot : islandSlots) {
        index.emplace(slot, rigidBodies.size());
        rigidBodies.push_back(w.bodies[slot].rigidBody);
    }
    std::vector<std::size_t> boundaryIndex;
    boundaryIndex.reserve(contacts.size());
    for (const auto &c : contacts) {
        if (c.twoWay)
            boundaryIndex.push_back(index.at(c.body.id & Impl::kSlotMask));
        else {
            boundaryIndex.push_back(rigidBodies.size());
            RigidBody wall;
            wall.position = c.point;
            wall.linearVelocity = c.wallVelocity;
            rigidBodies.push_back(wall);
        }
    }
    auto &solver = w.particleSolver;
    solver.Clear();
    struct ClearOnExit {
        ContactSolver &solver;
        ~ClearOnExit() {
            solver.Clear();
        }
    } clear{solver};
    struct BatchRow {
        RigidBody *a;
        RigidBody *b;
        Contact geometry;
        float friction;
    };
    std::vector<BatchRow> batchRows;
    batchRows.reserve(rigidRows.size() + contacts.size());
    for (const auto &row : rigidRows) {
        const auto &a = w.bodies[row.a];
        const auto &b = w.bodies[row.b];
        const float friction = std::sqrt(std::max(a.friction, 0.0f) * std::max(b.friction, 0.0f));
        batchRows.push_back(
            {&rigidBodies[index.at(row.a)], &rigidBodies[index.at(row.b)], row.contact, friction});
    }
    const std::size_t particleRowStart = batchRows.size();
    w.particleContactStats.rigidContactRows = particleRowStart;
    for (std::size_t i = 0; i < contacts.size(); ++i) {
        const auto &c = contacts[i];
        Contact geometry;
        geometry.hit = true;
        geometry.point = c.point;
        geometry.normal = c.normal;
        geometry.preciseNormal = glm::dvec3(c.normal);
        geometry.signedSeparation = 0;
        geometry.hasLocalAnchors = true;
        const auto &particle = particleBodies[c.particleIndex];
        const auto &body = rigidBodies[boundaryIndex[i]];
        geometry.localAnchorA = glm::dvec3(c.point) - glm::dvec3(particle.position);
        geometry.localAnchorB = glm::transpose(ContactRotation(body.orientation)) *
                                (glm::dvec3(c.point) - glm::dvec3(body.position));
        batchRows.push_back(
            {&particleBodies[c.particleIndex], &rigidBodies[boundaryIndex[i]], geometry, 0});
    }
    struct OriginalVelocity {
        RigidBody *body;
        glm::vec3 linear, angular;
    };
    std::vector<OriginalVelocity> original;
    original.reserve(particleBodies.size() + rigidBodies.size());
    for (auto &body : particleBodies)
        original.push_back({&body, body.linearVelocity, body.angularVelocity});
    for (auto &body : rigidBodies)
        original.push_back({&body, body.linearVelocity, body.angularVelocity});
    const auto prepare = [&](const std::vector<float> *normal = nullptr,
                             const std::vector<glm::vec3> *tangent = nullptr) {
        solver.Clear();
        for (const auto &v : original) {
            v.body->linearVelocity = v.linear;
            v.body->angularVelocity = v.angular;
        }
        for (std::size_t i = 0; i < batchRows.size(); ++i) {
            const auto &row = batchRows[i];
            solver.AddContact(*row.a, *row.b, row.geometry, row.friction, 0,
                              normal ? (*normal)[i] : 0, tangent ? (*tangent)[i] : glm::vec3(0));
        }
        solver.Prepare(remainingRigidDt);
    };
    prepare();
    const auto &solved = solver.Constraints();
    const auto pointVelocity = [](const RigidBody &b, const glm::vec3 &r) {
        return b.linearVelocity + glm::cross(b.angularVelocity, r);
    };
    auto &convergence = w.particleContactStats;
    for (const auto &c : solved) {
        convergence.speedScale = std::max(convergence.speedScale,
                                          glm::length(pointVelocity(*c.bodyA, c.velocityOffsetA)));
        convergence.speedScale = std::max(convergence.speedScale,
                                          glm::length(pointVelocity(*c.bodyB, c.velocityOffsetB)));
    }
    // Fluid contacts include finite particles and their actual rigid support
    // island. Refine this auxiliary solve to a declared velocity residual;
    // ordinary rigid step iteration counts and warm starts remain unchanged.
    do {
        const int count = std::min(10, 64 - convergence.iterations);
        solver.SolveVelocities(count);
        convergence.iterations += count;
        const auto closingResidual = [&]() {
            float largest = 0;
            for (const auto &c : solved) {
                const float normalSpeed = glm::dot(pointVelocity(*c.bodyA, c.velocityOffsetA) -
                                                       pointVelocity(*c.bodyB, c.velocityOffsetB),
                                                   c.normal);
                if (!std::isfinite(normalSpeed))
                    return std::numeric_limits<float>::infinity();
                largest = std::max(largest, c.restitutionBias - normalSpeed);
            }
            return largest;
        };
        convergence.maximumClosingSpeed = closingResidual();
        if (convergence.maximumClosingSpeed > 1.0e-5f * convergence.speedScale &&
            convergence.iterations < 64) {
            std::vector<float> normal;
            std::vector<glm::vec3> tangent;
            if (AccelerateParticleNormals(solved, convergence, normal, tangent)) {
                // This is an optional linear-solver proposal. Its double-
                // precision model is not permission to commit a worse actual
                // float constraint state, particularly for inconsistent PSD
                // rows generated by intersecting prescribed boundaries.
                const auto previousRows = solved;
                std::vector<OriginalVelocity> previous;
                previous.reserve(original.size());
                for (const auto &v : original)
                    previous.push_back({v.body, v.body->linearVelocity, v.body->angularVelocity});
                const float baseline = convergence.maximumClosingSpeed;
                prepare(&normal, &tangent);
                const float candidate = closingResidual();
                bool admissible = std::isfinite(candidate);
                const float rounding = 32 * std::numeric_limits<float>::epsilon();
                for (const auto &v : original)
                    admissible = admissible && finite(v.body->linearVelocity) &&
                                 finite(v.body->angularVelocity);
                for (const auto &c : solved) {
                    const float limit = c.friction * c.normalImpulse;
                    admissible =
                        admissible && std::isfinite(c.normalImpulse) && c.normalImpulse >= 0 &&
                        finite(c.tangentImpulse) &&
                        glm::length(c.tangentImpulse) <= limit + rounding * std::max(1.0f, limit);
                }
                admissible =
                    admissible &&
                    candidate <= baseline + rounding * std::max(convergence.speedScale, baseline);
                if (admissible) {
                    convergence.maximumClosingSpeed = candidate;
                    ++convergence.normalAccelerations;
                } else {
                    // Roll back bit-for-bit, including the impulses. Replaying
                    // them would repeat float rounding and tangent projection.
                    // Only this private auxiliary solver's freshly prepared
                    // rows are restored; ordinary rigid cache/state is absent.
                    auto &writable =
                        const_cast<std::vector<ContactConstraint> &>(solver.Constraints());
                    std::copy(previousRows.begin(), previousRows.end(), writable.begin());
                    for (const auto &v : previous) {
                        v.body->linearVelocity = v.linear;
                        v.body->angularVelocity = v.angular;
                    }
                    convergence.maximumClosingSpeed = baseline;
                    ++convergence.normalAccelerationRejections;
                }
            }
        }
    } while (convergence.maximumClosingSpeed > 1.0e-5f * convergence.speedScale &&
             convergence.iterations < 64);
    convergence.iterationCapReached =
        convergence.maximumClosingSpeed > 1.0e-5f * convergence.speedScale;
    for (std::size_t i = 0; i < contacts.size(); ++i) {
        const auto &c = solved[particleRowStart + i];
        particleImpulses[i] = c.normal * c.normalImpulse + c.tangentImpulse;
    }
    if (staticSupportImpulse)
        for (std::size_t i = 0; i < particleRowStart; ++i) {
            const auto &c = solved[i];
            const glm::vec3 impulse = c.normal * c.normalImpulse + c.tangentImpulse;
            if (c.bodyA->IsStatic())
                *staticSupportImpulse -= impulse;
            if (c.bodyB->IsStatic())
                *staticSupportImpulse += impulse;
        }
    for (std::size_t i = 0; i < particles.size(); ++i)
        particles[i].velocity = particleBodies[i].linearVelocity;
    for (const auto &[slot, copy] : index)
        if (!w.bodies[slot].rigidBody.IsStatic()) {
            w.bodies[slot].rigidBody.linearVelocity = rigidBodies[copy].linearVelocity;
            w.bodies[slot].rigidBody.angularVelocity = rigidBodies[copy].angularVelocity;
        }
}

PhysicsWorld::ParticleContactStats PhysicsWorld::GetParticleContactStats() const {
    return m_impl ? m_impl->particleContactStats : ParticleContactStats{};
}
