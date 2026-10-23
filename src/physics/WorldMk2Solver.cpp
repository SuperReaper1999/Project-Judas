#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

// Judas Physics MK2 substepped simultaneous contact solve. See
// docs/MK2_PHYSICS_ARCHITECTURE.md. Every manifold point of the step is one
// row of a single system solved over kMk2Substeps substeps: external
// acceleration, warm start, biased solve, ledger pose integration and an
// unbiased relax per substep, then one restitution pass. Separation is
// re-evaluated per substep from relative anchor motion, so an approaching
// speculative row engages in the substep its gap actually closes. Joint rows
// (JointSolver) are prepared and solved inside the same substeps.
namespace {
constexpr int kMk2Substeps = 4;
constexpr int kMk2Iterations = 2;      // biased sweeps per substep
constexpr int kMk2RelaxIterations = 1; // unbiased sweeps per substep
// Rows are rigid (exact non-approach). Overlap deeper than the slop is
// recovered by a gentle velocity bias (a fixed fraction per substep, capped);
// the relax sweep then removes that bias velocity, so recovery never becomes
// rebound. Soft (spring-like) rows were measured to sink under stacked load.
constexpr float kMk2Slop = 0.0005f;
constexpr float kMk2RecoveryPerSubstep = 0.02f;
constexpr float kMk2MaxPushVelocity = 3.0f;
constexpr float kMk2RestitutionThreshold = 0.5f; // MK1 kRestitutionVelocityThreshold
// A body whose step motion exceeds this fraction of its smallest feature is
// swept by the exact continuous query after the solve (bounded: once per body).
constexpr double kMk2FastFraction = 0.25;
constexpr int kMk2RestitutionSweeps = 8;
// Joint position error corrected per substep. Contacts recover gently (they
// carry stacks); a joint correcting much faster than a contact turns a
// friction-locked ragdoll pose into persistent penetration (the ratio of the
// two rates sets how a conflict is shared), so MK2 halves MK1's 0.2.
constexpr float kMk2JointPositionalRate = 0.1f;

// Velocities live in the bodies' own RigidBody storage, so joint rows and
// contact rows act on the same state. Only step-local derived data is here.
struct SolverBody {
    RigidBody *rb = nullptr;
    glm::vec3 dp{0}, gravityStep{0}, x0{0};
    glm::quat q0{1, 0, 0, 0};
    glm::mat3 invI{0}, dq{1};
    float invMass = 0;
    unsigned slot = 0;
    bool dynamic = false, kinematic = false;
    bool constrained = false; // in a contact or joint row this step
};

struct Row {
    unsigned a, b; // solver body indices
    glm::vec3 rA0, rB0; // step-start arms to the common contact point
    glm::vec3 gap0{0};  // AnchorGap when the anchors last coincided (re-collision)
    glm::vec3 rA, rB;   // current substep arms, both to one current common point
    glm::vec3 n, t1, t2;
    float separation, normalMass, tangentMass1, tangentMass2;
    float friction, restitution, relativeVelocity;
    float normalImpulse, tangent1, tangent2, maxNormalImpulse = 0;
    float steadyNormal = 0; // per-substep support load carried from the last step
    float cachedNormal = 0; // final relaxed substep load, before restitution
    // Precomputed Jacobians: angular arms r x d and their inertia responses.
    glm::vec3 rnA, rnB, wnA, wnB, r1A, r1B, w1A, w1B, r2A, r2B, w2A, w2B;
    float totalNormal = 0;
    glm::vec3 totalTangent{0};
};

// One manifold's points. Two to four points share a symmetric normal block
// solve, so a symmetric patch receives a symmetric response (sequential point
// order otherwise injects spin that topples balanced stacks).
// A coplanar four-point patch has a rank-3 response, so its normal LCP is
// solved with a tiny relative Tikhonov term: symmetric inputs then give the
// minimum-norm, symmetric distribution instead of an order-dependent one.
struct Block {
    std::size_t first = 0, count = 0;
    std::array<std::array<double, 4>, 4> k{};
    unsigned preferred = 0;
    bool usable = false;
    std::array<double, 4> null{}; // unit null direction of k, when hasNull
    bool hasNull = false;
};

// Active-set enumeration over at most 16 masks, preferred (last) set first.
// Returns false only if no complementary set exists; callers then fall back.
bool SolveBlockLcp(Block &block, const std::array<double, 4> &q, std::array<double, 4> &x) {
    const unsigned n = unsigned(block.count), all = (1u << n) - 1;
    double scale = 0, qScale = 0;
    for (unsigned i = 0; i < n; ++i) {
        scale = std::max(scale, block.k[i][i]);
        qScale = std::max(qScale, std::abs(q[i]));
    }
    const double regular = 1e-9 * scale, tolerance = 1e-9 * (qScale + 1e-12);
    const auto attempt = [&](unsigned mask) {
        unsigned idx[4], m = 0;
        for (unsigned i = 0; i < n; ++i)
            if (mask & (1u << i))
                idx[m++] = i;
        double a[4][4], b[4];
        for (unsigned i = 0; i < m; ++i) {
            for (unsigned j = 0; j < m; ++j)
                a[i][j] = block.k[idx[i]][idx[j]] + (i == j ? regular : 0);
            b[i] = -q[idx[i]];
        }
        // Cholesky (SPD after regularisation).
        for (unsigned j = 0; j < m; ++j) {
            double d = a[j][j];
            for (unsigned k = 0; k < j; ++k)
                d -= a[j][k] * a[j][k];
            if (!(d > 0))
                return false;
            a[j][j] = std::sqrt(d);
            for (unsigned i = j + 1; i < m; ++i) {
                double v = a[i][j];
                for (unsigned k = 0; k < j; ++k)
                    v -= a[i][k] * a[j][k];
                a[i][j] = v / a[j][j];
            }
        }
        for (unsigned i = 0; i < m; ++i) {
            for (unsigned k = 0; k < i; ++k)
                b[i] -= a[i][k] * b[k];
            b[i] /= a[i][i];
        }
        for (unsigned i = m; i-- > 0;) {
            for (unsigned k = i + 1; k < m; ++k)
                b[i] -= a[k][i] * b[k];
            b[i] /= a[i][i];
        }
        std::array<double, 4> candidate{};
        for (unsigned i = 0; i < m; ++i) {
            if (b[i] < 0)
                return false;
            candidate[idx[i]] = b[i];
        }
        for (unsigned i = 0; i < n; ++i) {
            if (mask & (1u << i))
                continue;
            double w = q[i];
            for (unsigned j = 0; j < n; ++j)
                w += block.k[i][j] * candidate[j];
            if (w < -tolerance)
                return false;
        }
        x = candidate;
        return true;
    };
    if (block.preferred && attempt(block.preferred))
        return true;
    for (unsigned mask = all; mask; --mask)
        if (mask != block.preferred && attempt(mask)) {
            block.preferred = mask;
            return true;
        }
    x = {};
    // Zero impulse is complementary when every row is already separating.
    for (unsigned i = 0; i < n; ++i)
        if (q[i] < -tolerance)
            return false;
    block.preferred = 0;
    return true;
}

// A coplanar patch is statically indeterminate: K has a null direction (for
// a box face, +1 -1 +1 -1 over its corners) in which impulses exert no force
// or torque. The LCP may return any point of that line (a box carried on two
// diagonal corners) and keeps it through its preferred set. Motion is the
// same along the whole line, but everything that reads per-point loads is
// not: friction cones, warm starts, ReadBodyContactSample (a balance
// controller's support polygon). Move to the minimum-norm nonnegative point.
void SpreadNullSpace(const Block &block, std::array<double, 4> &x) {
    if (!block.hasNull)
        return;
    const unsigned n = unsigned(block.count);
    // Minimise |x + t u|^2 over the t that keep every impulse >= 0.
    double xu = 0, low = -INFINITY, high = INFINITY;
    for (unsigned i = 0; i < n; ++i) {
        const double u = block.null[i];
        xu += x[i] * u;
        if (u > 1e-12)
            low = std::max(low, -x[i] / u);
        else if (u < -1e-12)
            high = std::min(high, -x[i] / u);
    }
    if (!(low <= high))
        return;
    const double t = std::clamp(-xu, low, high);
    for (unsigned i = 0; i < n; ++i)
        x[i] = std::max(0.0, x[i] + t * block.null[i]);
}

// The null direction of a rank n-1 block: every nonzero column of its
// adjugate lies along it (K adj(K) = det(K) I = 0). A full-rank block, or one
// of lower rank (a zero adjugate), has none to spread along.
void FindNullSpace(Block &block) {
    block.hasNull = false;
    const unsigned n = unsigned(block.count);
    if (!block.usable || n < 3)
        return;
    const auto &k = block.k;
    const auto minor = [&](unsigned row, unsigned col) {
        unsigned r[3], c[3], m = 0, q = 0;
        for (unsigned i = 0; i < n; ++i)
            if (i != row)
                r[m++] = i;
        for (unsigned j = 0; j < n; ++j)
            if (j != col)
                c[q++] = j;
        if (n == 3)
            return k[r[0]][c[0]] * k[r[1]][c[1]] - k[r[0]][c[1]] * k[r[1]][c[0]];
        return k[r[0]][c[0]] * (k[r[1]][c[1]] * k[r[2]][c[2]] - k[r[1]][c[2]] * k[r[2]][c[1]]) -
               k[r[0]][c[1]] * (k[r[1]][c[0]] * k[r[2]][c[2]] - k[r[1]][c[2]] * k[r[2]][c[0]]) +
               k[r[0]][c[2]] * (k[r[1]][c[0]] * k[r[2]][c[1]] - k[r[1]][c[1]] * k[r[2]][c[0]]);
    };
    double scale = 0;
    for (unsigned i = 0; i < n; ++i)
        scale = std::max(scale, k[i][i]);
    std::array<double, 4> best{};
    double bestNorm = 0;
    for (unsigned col = 0; col < n; ++col) {
        // Column `col` of adj(K): cofactors of row `col` (K is symmetric).
        std::array<double, 4> u{};
        double norm = 0;
        for (unsigned i = 0; i < n; ++i) {
            u[i] = ((i + col) % 2 ? -1 : 1) * minor(col, i);
            norm += u[i] * u[i];
        }
        if (norm > bestNorm) {
            bestNorm = norm;
            best = u;
        }
    }
    if (!(bestNorm > 0) || !(scale > 0))
        return;
    const double length = std::sqrt(bestNorm);
    for (unsigned i = 0; i < n; ++i)
        best[i] /= length;
    // Accept only a genuine null direction: |K u| negligible against K.
    double residual = 0;
    for (unsigned i = 0; i < n; ++i) {
        double r = 0;
        for (unsigned j = 0; j < n; ++j)
            r += k[i][j] * best[j];
        residual = std::max(residual, std::abs(r));
    }
    if (residual > 1e-6 * scale)
        return;
    block.null = best;
    block.hasNull = true;
}

glm::vec3 Orthogonal(const glm::vec3 &n) {
    // Construction aid only: the least-aligned axis gives a stable basis.
    const glm::vec3 a = std::abs(n.x) < 0.57735f   ? glm::vec3(1, 0, 0)
                        : std::abs(n.y) < 0.57735f ? glm::vec3(0, 1, 0)
                                                   : glm::vec3(0, 0, 1);
    return glm::normalize(glm::cross(n, a));
}

float InverseMassAlong(const SolverBody &a, const SolverBody &b, const glm::vec3 &rA,
                       const glm::vec3 &rB, const glm::vec3 &d) {
    const glm::vec3 ra = glm::cross(rA, d), rb = glm::cross(rB, d);
    return a.invMass + b.invMass + glm::dot(ra, a.invI * ra) + glm::dot(rb, b.invI * rb);
}

void Apply(SolverBody &a, SolverBody &b, const glm::vec3 &rA, const glm::vec3 &rB,
           const glm::vec3 &impulse) {
    if (a.dynamic) {
        a.rb->linearVelocity += impulse * a.invMass;
        a.rb->angularVelocity += a.invI * glm::cross(rA, impulse);
    }
    if (b.dynamic) {
        b.rb->linearVelocity -= impulse * b.invMass;
        b.rb->angularVelocity -= b.invI * glm::cross(rB, impulse);
    }
}

float AxisSpeed(const SolverBody &a, const SolverBody &b, const glm::vec3 &d, const glm::vec3 &armA,
                const glm::vec3 &armB) {
    return glm::dot(a.rb->linearVelocity - b.rb->linearVelocity, d) +
           glm::dot(a.rb->angularVelocity, armA) - glm::dot(b.rb->angularVelocity, armB);
}
float RowNormalSpeed(const std::vector<SolverBody> &bodies, const Row &row) {
    return AxisSpeed(bodies[row.a], bodies[row.b], row.n, row.rnA, row.rnB);
}
void ApplyAxis(SolverBody &a, SolverBody &b, const glm::vec3 &d, const glm::vec3 &responseA,
               const glm::vec3 &responseB, float impulse) {
    if (a.dynamic) {
        a.rb->linearVelocity += d * (a.invMass * impulse);
        a.rb->angularVelocity += responseA * impulse;
    }
    if (b.dynamic) {
        b.rb->linearVelocity -= d * (b.invMass * impulse);
        b.rb->angularVelocity -= responseB * impulse;
    }
}

// World offset of A's contact anchor from B's, relative to the step start.
glm::vec3 AnchorGap(const SolverBody &a, const SolverBody &b, const Row &row) {
    return (a.dp + (a.dq * row.rA0 - row.rA0)) - (b.dp + (b.dq * row.rB0 - row.rB0));
}

// Jacobians from the current arms and current world inertia. Both arms end at
// one common point and the inertia matches the current orientation, so every
// equal-and-opposite impulse conserves linear AND angular momentum exactly
// (fixed step-start arms would apply the pair at two drifting points).
void PrepareRow(const SolverBody &A, const SolverBody &B, Row &row) {
    const float kn = InverseMassAlong(A, B, row.rA, row.rB, row.n);
    const float k1 = InverseMassAlong(A, B, row.rA, row.rB, row.t1);
    const float k2 = InverseMassAlong(A, B, row.rA, row.rB, row.t2);
    row.normalMass = kn > 0 ? 1.0f / kn : 0;
    row.tangentMass1 = k1 > 0 ? 1.0f / k1 : 0;
    row.tangentMass2 = k2 > 0 ? 1.0f / k2 : 0;
    row.rnA = glm::cross(row.rA, row.n);
    row.rnB = glm::cross(row.rB, row.n);
    row.r1A = glm::cross(row.rA, row.t1);
    row.r1B = glm::cross(row.rB, row.t1);
    row.r2A = glm::cross(row.rA, row.t2);
    row.r2B = glm::cross(row.rB, row.t2);
    row.wnA = A.invI * row.rnA;
    row.wnB = B.invI * row.rnB;
    row.w1A = A.invI * row.r1A;
    row.w1B = B.invI * row.r1B;
    row.w2A = A.invI * row.r2A;
    row.w2B = B.invI * row.r2B;
}

void PrepareBlock(const std::vector<SolverBody> &state, const std::vector<Row> &rows, Block &block) {
    if (!block.usable)
        return;
    auto &k = block.k;
    for (std::size_t x = 0; x < block.count; ++x)
        for (std::size_t y = x; y < block.count; ++y) {
            const auto &rx = rows[block.first + x], &ry = rows[block.first + y];
            const auto &A = state[rx.a], &B = state[rx.b];
            // Exact mirror symmetry is required by the LCP factorization.
            k[x][y] = k[y][x] = double(A.invMass + B.invMass) + double(glm::dot(rx.rnA, ry.wnA)) +
                                double(glm::dot(rx.rnB, ry.wnB));
        }
    FindNullSpace(block);
}

// The velocity target for a row in this sweep (vn >= target).
float RowTarget(const std::vector<SolverBody> &bodies, const Row &row, float inverseH,
                bool useBias) {
    const auto &a = bodies[row.a];
    const auto &b = bodies[row.b];
    // Current separation from relative anchor motion (normal held fixed).
    // Only displacements enter: a static or far-away anchor contributes an
    // exact zero, so large world coordinates never round the gap.
    const glm::vec3 d = AnchorGap(a, b, row);
    const float s = glm::dot(d, row.n) + row.separation;
    if (s > 0)
        return -s * inverseH; // speculative: may close exactly the gap
    if (useBias && s < -kMk2Slop)
        return std::min(-kMk2RecoveryPerSubstep * inverseH * (s + kMk2Slop), kMk2MaxPushVelocity);
    return 0;
}

void ApplyNormal(std::vector<SolverBody> &bodies, Row &row, float next) {
    const float applied = next - row.normalImpulse;
    row.normalImpulse = next;
    row.maxNormalImpulse = std::max(row.maxNormalImpulse, applied);
    ApplyAxis(bodies[row.a], bodies[row.b], row.n, row.wnA, row.wnB, applied);
}

void SolveFriction(std::vector<SolverBody> &bodies, Row &row) {
    auto &a = bodies[row.a];
    auto &b = bodies[row.b];
    // Isotropic Coulomb cone on the two-tangent accumulated impulse.
    float t1 = row.tangent1 - row.tangentMass1 * AxisSpeed(a, b, row.t1, row.r1A, row.r1B);
    float t2 = row.tangent2 - row.tangentMass2 * AxisSpeed(a, b, row.t2, row.r2A, row.r2B);
    const float limit = row.friction * row.normalImpulse;
    const float length = std::sqrt(t1 * t1 + t2 * t2);
    if (length > limit && length > 0) {
        t1 *= limit / length;
        t2 *= limit / length;
    }
    const float d1 = t1 - row.tangent1, d2 = t2 - row.tangent2;
    row.tangent1 = t1;
    row.tangent2 = t2;
    ApplyAxis(a, b, row.t1, row.w1A, row.w1B, d1);
    ApplyAxis(a, b, row.t2, row.w2A, row.w2B, d2);
}

void SolveRows(std::vector<SolverBody> &bodies, std::vector<Row> &rows,
               std::vector<Block> &blocks, float h, bool useBias) {
    const float inverseH = 1.0f / h;
    for (auto &block : blocks) {
        bool solved = false;
        if (block.usable) {
            // Patch LCP on the accumulated impulses against each row's target.
            std::array<double, 4> q{}, previous{}, next{};
            for (std::size_t i = 0; i < block.count; ++i) {
                const auto &row = rows[block.first + i];
                q[i] = double(RowNormalSpeed(bodies, row)) -
                       double(RowTarget(bodies, row, inverseH, useBias));
                previous[i] = row.normalImpulse;
            }
            for (std::size_t i = 0; i < block.count; ++i)
                for (std::size_t j = 0; j < block.count; ++j)
                    q[i] -= block.k[i][j] * previous[j];
            if (SolveBlockLcp(block, q, next)) {
                SpreadNullSpace(block, next);
                for (std::size_t i = 0; i < block.count; ++i)
                    ApplyNormal(bodies, rows[block.first + i], float(next[i]));
                solved = true;
            }
        }
        if (!solved)
            for (std::size_t i = 0; i < block.count; ++i) {
                auto &row = rows[block.first + i];
                const float impulse = row.normalMass * (RowTarget(bodies, row, inverseH, useBias) -
                                                        RowNormalSpeed(bodies, row));
                ApplyNormal(bodies, row, std::max(row.normalImpulse + impulse, 0.0f));
            }
        for (std::size_t i = 0; i < block.count; ++i)
            SolveFriction(bodies, rows[block.first + i]);
    }
}
} // namespace

void PhysicsWorld::Impl::Mk2Solve(float dt) {
    const int substeps = kMk2Substeps;
    const float h = dt / float(substeps);

    // A contact or joint with an awake participant wakes the other dynamic
    // body, so no constraint ever acts on a frozen (unintegrated) body.
    for (const auto &p : mk2Points)
        for (auto slot : {p.a, p.b})
            if (bodies[slot].sleeping)
                Wake(slot);
    for (bool changed = true; changed;) {
        changed = false;
        for (const auto &joint : joints) {
            if (!JointActive(joint))
                continue;
            auto *a = Get(joint.state.settings.bodyA);
            auto *b = Get(joint.state.settings.bodyB);
            if (!a || !b)
                continue;
            const bool awakeA = a->isDynamic && !a->sleeping, awakeB = b->isDynamic && !b->sleeping;
            if (awakeA && b->sleeping) {
                Wake(joint.state.settings.bodyB.id & kSlotMask);
                changed = true;
            } else if (awakeB && a->sleeping) {
                Wake(joint.state.settings.bodyA.id & kSlotMask);
                changed = true;
            }
        }
    }

    std::vector<int> index(bodies.size(), -1);
    std::vector<SolverBody> state;
    state.reserve(aliveSlots.size());
    const auto addBody = [&](unsigned slot) -> unsigned {
        if (index[slot] >= 0)
            return unsigned(index[slot]);
        auto &body = bodies[slot];
        SolverBody s;
        s.rb = &body.rigidBody;
        s.slot = slot;
        s.x0 = body.rigidBody.position;
        s.q0 = body.rigidBody.orientation;
        s.kinematic = body.isKinematic && !body.simulationSuspended;
        s.dynamic = body.isDynamic && !body.sleeping && !body.simulationSuspended &&
                    !body.rigidBody.IsStatic();
        if (s.dynamic) {
            if (body.deferredAcceleration) {
                body.rigidBody.linearVelocity += body.lastAcceleration * dt;
                body.stepVelocityDelta += body.lastAcceleration * dt;
                body.deferredAcceleration = false;
            }
            s.invMass = body.rigidBody.inverseMass;
            s.invI = body.rigidBody.InverseInertiaWorld();
            // External acceleration is redistributed evenly across substeps.
            s.gravityStep = body.stepVelocityDelta / float(substeps);
            body.rigidBody.linearVelocity -= body.stepVelocityDelta;
        }
        index[slot] = int(state.size());
        state.push_back(s);
        return unsigned(state.size() - 1);
    };
    for (auto slot : aliveSlots) {
        auto &body = bodies[slot];
        if (body.isDynamic && !body.sleeping && !body.simulationSuspended)
            addBody(slot);
    }

    std::vector<Row> rows;
    rows.reserve(mk2Points.size());
    for (const auto &p : mk2Points) {
        const unsigned ia = addBody(p.a), ib = addBody(p.b);
        const auto &A = state[ia];
        const auto &B = state[ib];
        Row row;
        row.a = ia;
        row.b = ib;
        const auto &c = p.contact;
        // Lever arms from the manifold's precise parent-local anchors (as in
        // ContactSolver), so mirrored patch points get mirrored arms; a float
        // world-point difference rounds them unevenly and seeds spin.
        if (c.hasLocalAnchors) {
            row.rA0 = glm::vec3(c.anchorAInWorldFrame
                                    ? c.localAnchorA
                                    : ContactRotation(A.rb->orientation) * c.localAnchorA);
            row.rB0 = glm::vec3(c.anchorBInWorldFrame
                                    ? c.localAnchorB
                                    : ContactRotation(B.rb->orientation) * c.localAnchorB);
        } else {
            row.rA0 = glm::vec3(glm::dvec3(c.point) - glm::dvec3(A.x0));
            row.rB0 = glm::vec3(glm::dvec3(c.point) - glm::dvec3(B.x0));
        }
        row.rA = row.rA0;
        row.rB = row.rB0;
        row.n = c.normal;
        row.t1 = Orthogonal(row.n);
        row.t2 = glm::cross(row.n, row.t1);
        row.separation = float(c.hasLocalAnchors ? c.signedSeparation : -double(c.penetration));
        PrepareRow(A, B, row);
        row.friction = p.friction;
        row.restitution = p.restitution;
        // Approach speed before any contact response, including the step's
        // external acceleration: the physical impact speed.
        const auto pointVelocity = [&](const SolverBody &s, const glm::vec3 &r) {
            return s.rb->linearVelocity + s.gravityStep * float(substeps) +
                   glm::cross(s.rb->angularVelocity, r);
        };
        row.relativeVelocity = glm::dot(pointVelocity(A, row.rA) - pointVelocity(B, row.rB), row.n);
        row.normalImpulse = row.steadyNormal = p.warmNormal / float(substeps);
        row.tangent1 = glm::dot(p.warmTangent, row.t1) / float(substeps);
        row.tangent2 = glm::dot(p.warmTangent, row.t2) / float(substeps);
        rows.push_back(row);
    }

    for (const auto &row : rows)
        state[row.a].constrained = state[row.b].constrained = true;
    for (const auto &joint : joints)
        if (JointActive(joint))
            for (const auto handle : {joint.state.settings.bodyA, joint.state.settings.bodyB})
                if (handle.IsValid() && index[handle.id & kSlotMask] >= 0)
                    state[std::size_t(index[handle.id & kSlotMask])].constrained = true;
    // An unconstrained body gains nothing from substeps: it keeps MK1's exact
    // single-segment free flight (identical endpoint and ledger).
    for (auto &s : state)
        if (s.dynamic && !s.constrained)
            s.rb->linearVelocity += s.gravityStep * float(substeps);

    // Manifold blocks: consecutive points of one primitive pair (cache order).
    std::vector<Block> blocks;
    for (std::size_t i = 0; i < rows.size();) {
        std::size_t j = i + 1;
        while (j < rows.size() && j < pendingCache.size() &&
               pendingCache[j].key.SameBodies(pendingCache[i].key))
            ++j;
        Block block;
        block.first = i;
        block.count = j - i;
        block.usable = block.count >= 2 && block.count <= 4;
        PrepareBlock(state, rows, block);
        blocks.push_back(std::move(block));
        i = j;
    }

    // Joint observations are per-step totals; Prepare resets them per substep.
    struct JointTotals {
        JointState *state;
        glm::vec3 reaction{0}, angular{0};
        float motor = 0;
    };
    std::vector<JointTotals> jointTotals;
    const auto collectJoints = [&] {
        for (auto &t : jointTotals) {
            t.reaction += t.state->reactionImpulse;
            t.angular += t.state->reactionAngularImpulse;
            t.motor += t.state->motorImpulse;
        }
    };

    const auto refreshGeometry = [&] {
        for (auto &s : state)
            if (s.dynamic && s.constrained)
                s.invI = s.rb->InverseInertiaWorld();
        for (auto &row : rows) {
            const auto &A = state[row.a];
            const auto &B = state[row.b];
            const glm::vec3 d = AnchorGap(A, B, row) - row.gap0;
            row.rA = A.dq * row.rA0 - 0.5f * d;
            row.rB = B.dq * row.rB0 + 0.5f * d;
            PrepareRow(A, B, row);
        }
        for (auto &block : blocks)
            PrepareBlock(state, rows, block);
    };

    // Concave pairs (triangle mesh, terrain) are re-collided every substep. A
    // row's fixed normal is exact only for convex pairs: a body sliding along
    // a curved mesh would follow the tangent plane of the face it started on
    // while the surface rises ahead, and each crease it crosses would be
    // missed until the next step. Each row is moved to the nearest new contact
    // of its pair (impulses kept, friction re-projected); a row left without
    // one becomes far speculative, so the solver releases its impulse.
    struct Recollision {
        std::size_t first, count;
        unsigned slotA, slotB;
        int partA, partB;
        float margin;
    };
    std::vector<Recollision> recollisions;
    for (std::size_t i = 0; i < rows.size() && i < pendingCache.size();) {
        const auto &key = pendingCache[i].key;
        std::size_t j = i + 1;
        while (j < rows.size() && j < pendingCache.size() && pendingCache[j].key.SameBodies(key))
            ++j;
        const auto &a = bodies[key.slotA], &b = bodies[key.slotB];
        if (ConcaveShape(a) || ConcaveShape(b))
            recollisions.push_back(
                {i, j - i, key.slotA, key.slotB, key.partA, key.partB,
                 ContactMargin((glm::length(a.rigidBody.linearVelocity - b.rigidBody.linearVelocity) +
                                glm::length(a.rigidBody.angularVelocity) * a.boundingRadius +
                                glm::length(b.rigidBody.angularVelocity) * b.boundingRadius) *
                               dt)});
        i = j;
    }
    const auto recollide = [&] {
        for (const auto &g : recollisions) {
            const auto &bodyA = bodies[g.slotA], &bodyB = bodies[g.slotB];
            const auto manifold =
                ComputeContacts(PrimitiveAt(bodyA.shape, bodyA.rigidBody, g.partA),
                                PrimitiveAt(bodyB.shape, bodyB.rigidBody, g.partB), g.margin,
                                nullptr, nullptr, true);
            if (manifold.uncertain)
                continue;
            bool used[4] = {false, false, false, false};
            for (std::size_t k = 0; k < g.count; ++k) {
                auto &row = rows[g.first + k];
                const auto &A = state[row.a];
                const auto &B = state[row.b];
                const glm::vec3 d = AnchorGap(A, B, row) - row.gap0;
                const glm::vec3 armA = A.dq * row.rA0 - 0.5f * d;
                int best = -1;
                float nearest = std::numeric_limits<float>::infinity();
                std::array<glm::vec3, 4> relA{}, relB{};
                for (int m = 0; m < manifold.count && m < 4; ++m) {
                    const auto &c = manifold.points[m];
                    if (c.hasLocalAnchors) {
                        relA[m] = glm::vec3(c.anchorAInWorldFrame
                                                ? c.localAnchorA
                                                : ContactRotation(A.rb->orientation) * c.localAnchorA);
                        relB[m] = glm::vec3(c.anchorBInWorldFrame
                                                ? c.localAnchorB
                                                : ContactRotation(B.rb->orientation) * c.localAnchorB);
                    } else {
                        relA[m] = glm::vec3(glm::dvec3(c.point) - glm::dvec3(A.rb->position));
                        relB[m] = glm::vec3(glm::dvec3(c.point) - glm::dvec3(B.rb->position));
                    }
                    const float distance = glm::length(relA[m] - armA);
                    if (!used[m] && distance < nearest) {
                        nearest = distance;
                        best = m;
                    }
                }
                if (best < 0) {
                    row.separation = 1e3f; // far speculative: released by the solver
                    continue;
                }
                used[best] = true;
                const auto &c = manifold.points[best];
                const glm::vec3 tangent = row.t1 * row.tangent1 + row.t2 * row.tangent2;
                row.rA0 = glm::transpose(A.dq) * relA[best];
                row.rB0 = glm::transpose(B.dq) * relB[best];
                row.n = c.normal;
                row.t1 = Orthogonal(row.n);
                row.t2 = glm::cross(row.n, row.t1);
                const glm::vec3 planar = tangent - row.n * glm::dot(tangent, row.n);
                row.tangent1 = glm::dot(planar, row.t1);
                row.tangent2 = glm::dot(planar, row.t2);
                // The anchors coincide now: later gaps are measured from here.
                row.gap0 = AnchorGap(A, B, row);
                row.separation =
                    float(c.hasLocalAnchors ? c.signedSeparation : -double(c.penetration)) -
                    glm::dot(row.gap0, row.n);
            }
        }
    };

    // Per-body substep motion: one ledger segment per substep, so queries see
    // the actually travelled path and integration matches the ledger exactly.
    std::vector<std::vector<RigidMotionSegment>> segments(state.size());
    for (int step = 0; step < substeps; ++step) {
        const double begin = double(dt) * step / substeps,
                     end = step + 1 == substeps ? double(dt) : double(dt) * (step + 1) / substeps;
        for (auto &s : state)
            if (s.dynamic && s.constrained)
                s.rb->linearVelocity += s.gravityStep;
        // Warm start with at most the steady support load. Re-applying an
        // impact substep's spike would have to be removed again by the next
        // sweep; down a chain one sweep cannot, and the stack would pop. A
        // patch is scaled as a whole: its rows' split is free within the
        // patch null space, so clipping rows independently would create torque.
        for (auto &block : blocks) {
            if (step > 0) {
                float current = 0, steady = 0;
                for (std::size_t i = 0; i < block.count; ++i) {
                    current += rows[block.first + i].normalImpulse;
                    steady += rows[block.first + i].steadyNormal;
                }
                const float scale = current > steady && current > 0 ? steady / current : 1.0f;
                for (std::size_t i = 0; i < block.count; ++i) {
                    auto &row = rows[block.first + i];
                    row.normalImpulse *= scale;
                    row.tangent1 *= scale;
                    row.tangent2 *= scale;
                }
            }
            for (std::size_t i = 0; i < block.count; ++i) {
                auto &row = rows[block.first + i];
                Apply(state[row.a], state[row.b], row.rA, row.rB,
                      row.n * row.normalImpulse + row.t1 * row.tangent1 + row.t2 * row.tangent2);
            }
        }
        bool haveJoints = false;
        if (!joints.empty()) {
            const auto inputs = JointInputs();
            haveJoints = !inputs.empty();
            if (step == 0)
                for (const auto &input : inputs)
                    jointTotals.push_back({input.state});
            // Joints warm-start once per step, from the step's history, as
            // in MK1 (whose controllers also reset drive history every
            // step). Re-warming each substep from the previous substep would
            // re-apply a limit stop's impact spike (a collapsing ragdoll
            // rebounded off its own knee and hip stops) and let drives carry
            // in-step history MK1 never had.
            PrepareJoints(jointSolver, inputs, h, true, false, false,
                          kMk2JointPositionalRate, step > 0);
        }
        // Joints first, contacts last in every sweep (as Box2D v3): the final
        // velocity of each sweep never approaches a contact, so a joint pull
        // becomes joint error (recovered by the joint bias), not penetration.
        for (int iteration = 0; iteration < kMk2Iterations; ++iteration) {
            if (haveJoints)
                jointSolver.SolveIteration();
            SolveRows(state, rows, blocks, h, true);
        }
        for (std::size_t i = 0; i < state.size(); ++i) {
            auto &s = state[i];
            auto &body = bodies[s.slot];
            if (s.dynamic && s.constrained) {
                RigidMotionSegment seg;
                seg.owner = MakeHandle(s.slot).id;
                seg.begin = begin;
                seg.end = end;
                seg.inverseMass = body.rigidBody.inverseMass;
                seg.inverseInertiaLocal = body.rigidBody.inverseInertiaLocal;
                seg.position = body.rigidBody.position;
                seg.orientation = body.rigidBody.orientation;
                seg.linearVelocity = body.rigidBody.linearVelocity;
                seg.angularVelocity = body.rigidBody.angularVelocity;
                seg.driftAngularVelocity =
                    AngularDriftGenerator(seg.inverseInertiaLocal, seg.orientation,
                                          seg.angularVelocity, float(end - begin));
                const auto pose = seg.EvaluateGeometry(end);
                seg.endPosition = pose.position;
                seg.endOrientation = pose.orientation;
                const RigidBody next = seg.Evaluate(end);
                segments[i].push_back(seg);
                body.rigidBody.position = next.position;
                body.rigidBody.orientation = next.orientation;
                body.rigidBody.angularVelocity = next.angularVelocity;
                s.dp = glm::vec3(glm::dvec3(next.position) - glm::dvec3(s.x0));
                s.dq = glm::mat3_cast(next.orientation * glm::conjugate(s.q0));
            } else if (s.kinematic && body.HasMotion()) {
                const auto pose = body.motion.EvaluateGeometryAfterEvents(end);
                body.rigidBody.position = pose.position;
                body.rigidBody.orientation = pose.orientation;
                s.dp = glm::vec3(glm::dvec3(pose.position) - glm::dvec3(s.x0));
                s.dq = glm::mat3_cast(pose.orientation * glm::conjugate(s.q0));
            }
        }
        // Poses moved: relax (and the next substep) use the current geometry.
        recollide();
        refreshGeometry();
        // Relax removes every positional bias velocity, joints' included, so
        // neither joint nor contact correction outlives its substep as motion.
        for (int iteration = 0; iteration < kMk2RelaxIterations; ++iteration) {
            if (haveJoints)
                jointSolver.SolveIteration(0, false);
            SolveRows(state, rows, blocks, h, false);
        }
        if (haveJoints)
            collectJoints();
        for (auto &row : rows) {
            row.totalNormal += row.normalImpulse;
            row.totalTangent += row.t1 * row.tangent1 + row.t2 * row.tangent2;
            // A row that just engaged has no carried load yet; its first
            // relaxed substep value becomes the steady estimate.
            if (step == 0 && row.steadyNormal == 0)
                row.steadyNormal = row.normalImpulse;
        }
    }
    for (auto &row : rows)
        row.cachedNormal = row.normalImpulse;
    for (auto &t : jointTotals) {
        t.state->reactionImpulse = t.reaction;
        t.state->reactionAngularImpulse = t.angular;
        t.state->motorImpulse = t.motor;
    }

    // Restitution from the pre-solve approach speed, only for rows that
    // actually carried load this step. A fully bouncing patch is solved as a
    // block so a symmetric landing rebounds without manufactured spin.
    const auto bounces = [&](const Row &row) {
        return row.restitution != 0 && row.relativeVelocity < -kMk2RestitutionThreshold &&
               row.maxNormalImpulse > 0;
    };
    // Coupled bouncing patches (a body struck from two sides at once) form one
    // restitution system: iterate it to convergence, not one sequential sweep.
    for (int sweep = 0; sweep < kMk2RestitutionSweeps; ++sweep)
    for (auto &block : blocks) {
        bool all = block.usable;
        bool any = false;
        for (std::size_t i = 0; i < block.count; ++i) {
            all = all && bounces(rows[block.first + i]);
            any = any || bounces(rows[block.first + i]);
        }
        if (!any)
            continue;
        if (all) {
            std::array<double, 4> q{}, next{};
            for (std::size_t i = 0; i < block.count; ++i) {
                const auto &row = rows[block.first + i];
                q[i] = double(RowNormalSpeed(state, row)) +
                       double(row.restitution * row.relativeVelocity);
                for (std::size_t j = 0; j < block.count; ++j)
                    q[i] -= block.k[i][j] * double(rows[block.first + j].normalImpulse);
            }
            if (SolveBlockLcp(block, q, next)) {
                for (std::size_t i = 0; i < block.count; ++i) {
                    auto &row = rows[block.first + i];
                    const float before = row.normalImpulse;
                    ApplyNormal(state, row, float(next[i]));
                    row.totalNormal += row.normalImpulse - before;
                }
                continue;
            }
        }
        for (std::size_t i = 0; i < block.count; ++i) {
            auto &row = rows[block.first + i];
            if (!bounces(row))
                continue;
            auto &a = state[row.a];
            auto &b = state[row.b];
            const float vn = AxisSpeed(a, b, row.n, row.rnA, row.rnB);
            const float impulse = -row.normalMass * (vn + row.restitution * row.relativeVelocity);
            const float before = row.normalImpulse;
            ApplyNormal(state, row, std::max(row.normalImpulse + impulse, 0.0f));
            row.totalNormal += row.normalImpulse - before;
        }
    }

    // Publish ledgers and kinematic endpoints.
    for (const auto slot : aliveSlots) {
        auto &body = bodies[slot];
        if (!body.isDynamic || body.simulationSuspended)
            continue;
        const int i = index[slot];
        if (i >= 0 && state[std::size_t(i)].dynamic && state[std::size_t(i)].constrained) {
            body.motion.SetRecorded(std::move(segments[std::size_t(i)]));
        } else {
            body.motion.Begin(MakeHandle(slot).id, body.rigidBody, dt);
            if (i >= 0 && state[std::size_t(i)].dynamic) {
                const auto pose = body.motion.EvaluateAfterEvents(dt);
                body.rigidBody.position = pose.position;
                body.rigidBody.orientation = pose.orientation;
                body.rigidBody.angularVelocity = pose.angularVelocity;
            }
        }
        InvalidateImpactPose(slot);
        stats.motionSegments += body.motion.Segments().size();
        stats.motionStorageBytes += body.motion.StorageBytes();
    }
    for (const auto slot : aliveSlots) {
        auto &body = bodies[slot];
        if (!body.isKinematic || !body.HasMotion())
            continue;
        const auto pose = body.motion.EvaluateAfterEvents(dt);
        body.rigidBody.position = pose.position;
        body.rigidBody.orientation = pose.orientation;
        body.rigidBody.linearVelocity = pose.linearVelocity;
        body.rigidBody.angularVelocity = pose.angularVelocity;
        if (body.kinematic.control == KinematicControl::Target) {
            body.kinematic.remainingSeconds = std::max(
                0.f, body.kinematic.remainingSeconds - float(body.kinematicActiveDuration));
            if (body.kinematic.remainingSeconds == 0)
                body.kinematic.control = KinematicControl::Stopped;
        }
        InvalidateImpactPose(slot);
    }

    // Row pairs (cache order is sorted) with the deepest end-of-step signed
    // distance of any of their points to that point's own step-start contact
    // plane, for the continuous pass. Deeply negative: it passed the plane.
    mk2RowPairs.clear();
    mk2RowEndSeparation.clear();
    for (std::size_t i = 0; i < rows.size() && i < pendingCache.size(); ++i) {
        const auto &key = pendingCache[i].key;
        const std::array<unsigned, 4> pair{key.slotA, key.slotB, unsigned(key.partA),
                                           unsigned(key.partB)};
        const float endSeparation =
            glm::dot(AnchorGap(state[rows[i].a], state[rows[i].b], rows[i]), rows[i].n) +
            rows[i].separation;
        if (mk2RowPairs.empty() || mk2RowPairs.back() != pair) {
            mk2RowPairs.push_back(pair);
            mk2RowEndSeparation.push_back(endSeparation);
        } else {
            mk2RowEndSeparation.back() = std::min(mk2RowEndSeparation.back(), endSeparation);
        }
    }
    Mk2Continuous(dt);

    // Contact history: step-total impulses in MK1's cache format.
    for (std::size_t i = 0; i < rows.size() && i < pendingCache.size(); ++i) {
        const auto &key = pendingCache[i].key;
        const unsigned ha = MakeHandle(key.slotA).id, hb = MakeHandle(key.slotB).id;
        const auto pair = std::minmax(ha, hb);
        if (auto it = stepTouches.find({pair.first, pair.second}); it != stepTouches.end()) {
            it->second.normalImpulse += rows[i].totalNormal;
            it->second.impulseAvailable = true;
        }
        // Warm-start cache: the final substep's support load, per step. Touch
        // events above carry the step's total (impact-inclusive) impulse.
        pendingCache[i].normalImpulse = rows[i].cachedNormal * float(substeps);
        pendingCache[i].tangentImpulse =
            (rows[i].t1 * rows[i].tangent1 + rows[i].t2 * rows[i].tangent2) * float(substeps);
    }
    // Observers (ReadBodyContactSample) read MK1's semantics: the impulse each
    // point delivered over the whole step, impacts and restitution included.
    // The warm-start cache keeps the steady final-substep load.
    std::vector<CachedContact> observed(pendingCache.begin(), pendingCache.end());
    for (std::size_t i = 0; i < rows.size() && i < observed.size(); ++i) {
        observed[i].normalImpulse = rows[i].totalNormal;
        observed[i].tangentImpulse = rows[i].totalTangent;
    }
    for (const auto &cached : contactCache)
        if (cached.key.slotA < bodies.size() && cached.key.slotB < bodies.size() &&
            bodies[cached.key.slotA].alive && bodies[cached.key.slotB].alive &&
            bodies[cached.key.slotA].generation == cached.key.generationA &&
            bodies[cached.key.slotB].generation == cached.key.generationB &&
            (bodies[cached.key.slotA].sleeping || bodies[cached.key.slotA].rigidBody.IsStatic()) &&
            (bodies[cached.key.slotB].sleeping || bodies[cached.key.slotB].rigidBody.IsStatic())) {
            pendingCache.push_back(cached);
            observed.push_back(cached);
        }
    const auto byKey = [](const CachedContact &x, const CachedContact &y) { return x.key < y.key; };
    std::stable_sort(pendingCache.begin(), pendingCache.end(), byKey);
    std::stable_sort(observed.begin(), observed.end(), byKey);
    std::swap(contactCache, pendingCache);
    completedContactObservations = std::move(observed);
}

// Bounded continuous pass for fast bodies. The substep solve's speculative
// rows close only the gaps they were built from; a fast or spinning body can
// still cross geometry its linearized rows did not represent. Each fast body's
// actually recorded ledger path is swept once with the exact MK1 query against
// every candidate it did not already touch at the start of the step. At the
// earliest arrival its pose is held for the rest of the step (velocity kept),
// and the next step's rows resolve the contact. One sweep per fast body, so no
// event chain can form.
void PhysicsWorld::Impl::Mk2Continuous(float dt) {
    stepDuration = dt;
    impactQueryAnchorTime = 0;
    for (auto slot : aliveSlots) {
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
    }
    std::vector<unsigned> candidates;
    for (const auto slot : aliveSlots) {
        auto &body = bodies[slot];
        if (!body.isDynamic || body.sleeping || body.simulationSuspended || body.sensor ||
            body.queryOnly || body.rigidBody.IsStatic())
            continue;
        double feature = std::numeric_limits<double>::infinity(), reach = 0;
        for (int part = 0; part < PrimitiveCount(body.shape); ++part)
            feature = std::min(feature, PrimitiveFeature(slot, part));
        for (const auto &segment : body.motion.Segments())
            reach = std::max(reach, (double(glm::length(segment.linearVelocity)) +
                                     double(glm::length(segment.angularVelocity)) *
                                         double(body.boundingRadius)) *
                                        double(dt));
        if (!(reach > kMk2FastFraction * feature))
            continue;
        const Aabb query = RemainingImpactBound(slot, 0, dt);
        const auto &found = ImpactCandidates(slot, query);
        candidates.assign(found.begin(), found.end());
        double earliest = double(dt) + 1;
        unsigned hitOther = 0;
        int hitPartA = 0, hitPartB = 0;
        for (const auto other : candidates) {
            if (other == slot || !CanRespond(slot, other))
                continue;
            const auto &o = bodies[other];
            if (o.sensor || o.queryOnly)
                continue;
            const unsigned a = std::min(slot, other), b = std::max(slot, other);
            for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb) {
                    // A pair touching at the start is searched (as in MK1) to
                    // the first time a mesh or spinning pair penetrates a
                    // quarter feature deep; otherwise its rows own it. A pair
                    // with rows arrived legitimately unless it then went
                    // deep: behind its own row plane, behind the first-touch
                    // plane carried to the step end, deeply overlapped at the
                    // end, or past the same depth search from first touch. A
                    // pair with neither had no response at all.
                    const bool touching = PairTouches(a, b, pa, pb, 0);
                    const double limit = std::min(earliest, double(dt));
                    const double hit = ImpactTime(a, b, pa, pb, 0, limit);
                    if (!(hit < limit))
                        continue;
                    const std::array<unsigned, 4> key{a, b, unsigned(pa), unsigned(pb)};
                    const auto row = std::lower_bound(mk2RowPairs.begin(), mk2RowPairs.end(), key);
                    const bool solved = row != mk2RowPairs.end() && *row == key;
                    if (!touching && solved) {
                        const double depth =
                            0.25 * std::min(PrimitiveFeature(a, pa), PrimitiveFeature(b, pb));
                        bool passed =
                            -mk2RowEndSeparation[std::size_t(row - mk2RowPairs.begin())] > depth ||
                            Mk2Crossed(slot, other, a, b, pa, pb, hit, dt);
                        if (!passed) {
                            const auto end = PairAt(a, b, pa, pb, dt, 0);
                            passed = end.uncertain;
                            for (int k = 0; k < end.count; ++k) {
                                const auto &c = end.points[k];
                                passed = passed || -(c.hasLocalAnchors ? c.signedSeparation
                                                                       : -double(c.penetration)) >
                                                       depth;
                            }
                        }
                        passed = passed || ImpactTime(a, b, pa, pb, hit, limit) < limit;
                        if (!passed)
                            continue;
                    }
                    earliest = hit;
                    hitOther = other;
                    hitPartA = pa;
                    hitPartB = pb;
                }
        }
        if (!(earliest < double(dt)))
            continue;
        ++stats.impactEvents;
        Mk2ResolveArrival(slot, hitOther, hitPartA, hitPartB, earliest);
        auto recorded = body.motion.Segments();
        std::vector<RigidMotionSegment> clamped;
        for (auto segment : recorded) {
            if (segment.begin >= earliest && !clamped.empty())
                break;
            if (segment.end > earliest) {
                const auto pose = segment.EvaluateGeometry(earliest);
                segment.end = earliest;
                segment.endPosition = pose.position;
                segment.endOrientation = pose.orientation;
            }
            clamped.push_back(segment);
        }
        if (clamped.empty())
            continue;
        RigidMotionSegment hold = clamped.back();
        hold.begin = earliest;
        hold.end = dt;
        hold.position = hold.endPosition;
        hold.orientation = hold.endOrientation;
        hold.linearVelocity = hold.angularVelocity = hold.driftAngularVelocity = glm::vec3(0);
        clamped.push_back(hold);
        body.rigidBody.position = hold.position;
        body.rigidBody.orientation = hold.orientation;
        body.motion.SetRecorded(std::move(clamped));
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
    }
}

// Whether `fast` (one of a, b) ends the step behind the contact plane of its
// first touch with `other`. Each contact point is carried rigidly with the
// fast body and the plane with the other body, from `hit` to the step end.
bool PhysicsWorld::Impl::Mk2Crossed(unsigned fast, unsigned other, unsigned a, unsigned b, int pa,
                                     int pb, double hit, float dt) {
    const auto manifold = PairAt(a, b, pa, pb, hit, 1e-3f);
    if (manifold.uncertain)
        return true;
    const RigidBody fastHit = PoseAt(fast, hit), fastEnd = PoseAt(fast, dt);
    const RigidBody otherHit = PoseAt(other, hit), otherEnd = PoseAt(other, dt);
    const glm::dquat fastMove = glm::dquat(fastEnd.orientation) * glm::conjugate(glm::dquat(fastHit.orientation));
    const glm::dquat otherMove = glm::dquat(otherEnd.orientation) * glm::conjugate(glm::dquat(otherHit.orientation));
    const double depth = 0.25 * std::min(PrimitiveFeature(a, pa), PrimitiveFeature(b, pb));
    for (int k = 0; k < manifold.count; ++k) {
        const auto &c = manifold.points[k];
        const glm::dvec3 p(c.point);
        // Contact normals point from B toward A; orient toward the fast body.
        const glm::dvec3 toward = glm::dvec3(c.normal) * (fast == a ? 1.0 : -1.0);
        const glm::dvec3 pointEnd = glm::dvec3(fastEnd.position) + fastMove * (p - glm::dvec3(fastHit.position));
        const glm::dvec3 planeEnd = glm::dvec3(otherEnd.position) + otherMove * (p - glm::dvec3(otherHit.position));
        if (glm::dot(pointEnd - planeEnd, otherMove * toward) < -depth)
            return true;
    }
    return false;
}

// The held fast body arrived at `hit`: resolve that arrival with MK1's exact
// impact solver on the actual first-touch manifold, with both bodies at their
// first-touch poses, so the hold does not leave it approaching next step.
// Velocities only; poses and ledgers are not changed here.
void PhysicsWorld::Impl::Mk2ResolveArrival(unsigned fast, unsigned other, int partFast,
                                           int partOther, double hit) {
    const unsigned a = std::min(fast, other), b = std::max(fast, other);
    const int pa = fast == a ? partFast : partOther, pb = fast == a ? partOther : partFast;
    // The search stops at (just before) first touch: widen the margin until
    // the arriving features are found, then treat them as touching.
    ContactManifold manifold;
    for (const float margin : {1e-3f, 5e-3f, float(kMk2Slop) * 20.0f}) {
        manifold = PairAt(a, b, pa, pb, hit, margin);
        if (manifold.count || manifold.uncertain)
            break;
    }
    if (!manifold.count || manifold.uncertain)
        return;
    const auto movable = [&](unsigned slot) {
        const auto &body = bodies[slot];
        return body.isDynamic && !body.rigidBody.IsStatic() && !body.sleeping &&
               !body.simulationSuspended;
    };
    // Private first-touch copies: actual velocities, poses at the arrival.
    RigidBody bodyA = bodies[a].rigidBody, bodyB = bodies[b].rigidBody;
    for (auto [copy, slot] : {std::pair<RigidBody *, unsigned>{&bodyA, a}, {&bodyB, b}}) {
        const auto pose = PoseAt(slot, hit);
        copy->position = pose.position;
        copy->orientation = pose.orientation;
        if (!movable(slot)) {
            copy->inverseMass = 0;
            copy->inverseInertiaLocal = glm::mat3(0);
            if (!bodies[slot].isKinematic)
                copy->linearVelocity = copy->angularVelocity = glm::vec3(0);
        }
    }
    std::vector<ImpactContact> rows;
    const float friction =
        std::sqrt(std::max(0.f, bodies[a].friction) * std::max(0.f, bodies[b].friction));
    for (int k = 0; k < manifold.count; ++k) {
        auto c = manifold.points[k];
        if (c.hasLocalAnchors ? c.signedSeparation > 0 : c.penetration < 0) {
            c.signedSeparation = 0; // first touch, to bisection resolution
            c.penetration = 0;
        }
        rows.push_back({&bodyA, &bodyB, c, friction,
                        std::max(bodies[a].restitution, bodies[b].restitution), false});
        rows.back().pointNormals = ConcaveShape(bodies[a]) || ConcaveShape(bodies[b]);
    }
    const auto result =
        impactSolver.Solve(rows, stepDuration > 0 ? float(stepDuration) : 1.f / 60);
    if (!result.energyBudgetSatisfied)
        return;
    for (auto [copy, slot] : {std::pair<RigidBody *, unsigned>{&bodyA, a}, {&bodyB, b}})
        if (movable(slot)) {
            bodies[slot].rigidBody.linearVelocity = copy->linearVelocity;
            bodies[slot].rigidBody.angularVelocity = copy->angularVelocity;
        }
}
