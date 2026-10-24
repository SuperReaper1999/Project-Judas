#include "WorldInternal.h"

#include <queue>
#include <tuple>
#include <unordered_map>


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
ProfileLabel mk2RowPreparationLabel("Physics contact row preparation");
constexpr int kMk2Iterations = 2;      // biased sweeps per substep
// Sweeps for joints and the contact blocks touching jointed bodies: an
// articulation resting on contacts needs these to converge (at 2 a resting
// ragdoll kept a residual solver velocity above the sleep threshold that the
// position projection cancelled every substep).
constexpr int kMk2JointIterations = 4;
constexpr int kMk2RelaxIterations = 1; // unbiased sweeps per substep
// Rows are rigid (exact non-approach). Overlap deeper than the slop is
// recovered by a gentle velocity bias (a fixed fraction per substep, capped);
// the relax sweep then removes that bias velocity, so recovery never becomes
// rebound. Soft (spring-like) rows were measured to sink under stacked load.
constexpr float kMk2Slop = 0.0005f;
constexpr float kMk2RecoveryPerSubstep = 0.02f;
constexpr float kMk2MaxPushVelocity = 3.0f;
constexpr float kMk2RestitutionThreshold = 0.5f; // MK1 kRestitutionVelocityThreshold
constexpr float kMk2FreeSpeed = 1e6f; // a target no physical approach reaches
// Sweeps that converge each phase of a substep's coupled impacts (a body
// struck from two sides at once is one system, not one sequential sweep), and
// the bound on compression/restitution cycles a rebound may set off.
constexpr int kMk2ImpactSweeps = 8;
// Pairwise resolutions of a substep's impact set: at least kMk2ImpactPairs,
// and kMk2ImpactsPerBlock per impacting block (a pair may be struck again as
// an impulse travels down a chain), so a large simultaneous impact resolves
// every pair, not only the first 64. Also the approach speed below which a
// touching pair is no longer an impact.
constexpr int kMk2ImpactPairs = 64;
constexpr std::size_t kMk2ImpactsPerBlock = 4;
constexpr float kMk2ImpactApproach = 1e-4f;
// Bounds of the continuous pass's time-ordered arrivals: re-timings of one
// body per step, and arrivals of the whole step.
constexpr int kMk2ArrivalsPerBody = 4;
// Manifold margin when an arriving pair is re-collided at its arrival pose.
constexpr float kMk2ArrivalMargin = 0.01f;
constexpr int kMk2ArrivalsPerStep = 256;
// A body whose step motion exceeds this fraction of its smallest feature is
// swept by the exact continuous query after the solve (bounded: once per body).
constexpr double kMk2FastFraction = 0.25;
// Joint position error is corrected by a pseudo-velocity that moves the
// substep's poses but never enters the bodies' velocities (split impulse):
// a velocity bias, at any fixed rate, pumped energy into chains (gap 8).
// Contacts recover gently (they carry stacks); a joint correcting much faster
// than a contact turns a friction-locked ragdoll pose into persistent
// penetration, so the joint rate stays at MK1-contact scale.
constexpr float kMk2JointProjectionRate = 0.1f;
constexpr int kMk2JointProjectionIterations = 4;
// An approaching limit's velocity allowance (its full remaining gap per dt).
constexpr float kMk2JointLimitApproachRate = 0.2f;

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
    float friction, restitution;
    // In-substep impact (see the substep loop): an arriving row is resolved
    // at its arrival instant. `arrivesNext`: still open after this substep's
    // motion, but closing within the next at an impact speed.
    bool arriving = false, arrivesNext = false;
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
                bool useBias, bool impact) {
    const auto &a = bodies[row.a];
    const auto &b = bodies[row.b];
    // Current separation from relative anchor motion (normal held fixed).
    // Only displacements enter: a static or far-away anchor contributes an
    // exact zero, so large world coordinates never round the gap.
    const glm::vec3 d = AnchorGap(a, b, row);
    const float s = glm::dot(d, row.n) + row.separation;
    // Impact phase: arriving and touching rows are in contact at the arrival
    // instant; nothing else takes part, and nothing is biased.
    if (impact)
        return row.arriving || s <= kMk2Slop ? 0.0f : -kMk2FreeSpeed;
    float target = 0;
    // Speculative: may close exactly the gap. In the relax, a row that will
    // arrive at impact speed within the next substep is left free: slowing it
    // there would push across the open gap (a ghost impulse that also crosses
    // any touching neighbours), and the next substep resolves its arrival
    // exactly at its own speed and instant.
    if (s > 0)
        target = !useBias && row.arrivesNext ? -kMk2FreeSpeed : -s * inverseH;
    else if (useBias && s < -kMk2Slop)
        target = std::min(-kMk2RecoveryPerSubstep * inverseH * (s + kMk2Slop), kMk2MaxPushVelocity);
    return target;
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

// One manifold block's normal (patch LCP or sequential) and friction rows.
void SolveBlock(std::vector<SolverBody> &bodies, std::vector<Row> &rows, Block &block,
                float inverseH, bool useBias, bool impact) {
    {
        bool solved = false;
        if (block.usable) {
            // Patch LCP on the accumulated impulses against each row's target.
            std::array<double, 4> q{}, previous{}, next{};
            for (std::size_t i = 0; i < block.count; ++i) {
                const auto &row = rows[block.first + i];
                q[i] = double(RowNormalSpeed(bodies, row)) -
                       double(RowTarget(bodies, row, inverseH, useBias, impact));
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
                const float impulse = row.normalMass * (RowTarget(bodies, row, inverseH, useBias, impact) -
                                                        RowNormalSpeed(bodies, row));
                ApplyNormal(bodies, row, std::max(row.normalImpulse + impulse, 0.0f));
            }
        for (std::size_t i = 0; i < block.count; ++i)
            SolveFriction(bodies, rows[block.first + i]);
    }
}

void SolveRows(std::vector<SolverBody> &bodies, std::vector<Row> &rows,
               std::vector<Block> &blocks, float h, bool useBias,
               const std::vector<char> *only = nullptr, bool impact = false) {
    const float inverseH = 1.0f / h;
    for (std::size_t index = 0; index < blocks.size(); ++index)
        if (!only || (*only)[index])
            SolveBlock(bodies, rows, blocks[index], inverseH, useBias, impact);
}

// The listed blocks only, in list order (an ascending list is SolveRows with
// the same mask, without scanning every block of the world).
void SolveBlocks(std::vector<SolverBody> &bodies, std::vector<Row> &rows,
                 std::vector<Block> &blocks, const std::vector<std::size_t> &list, float h,
                 bool useBias, bool impact) {
    const float inverseH = 1.0f / h;
    for (const auto index : list)
        SolveBlock(bodies, rows, blocks[index], inverseH, useBias, impact);
}
} // namespace

void PhysicsWorld::Impl::Mk2Solve(float dt, const PhysicsWorld::VelocityCoupling *coupling) {
    // A velocity-coupled participant keeps its per-step contract: begin(dt)
    // once after row preparation, exactly `iterations` solve(dt, round) calls,
    // finish(dt) once after the last pose advance. Its rounds are spread over
    // the substeps, so its impulses act inside each substep's velocity solve.
    // (Solving a coupled step as one substep let a 24.6 m/s crash car reach
    // its barrier with half the crush and no link failure.)
    std::size_t frictionlessRetries = 0, inelasticFallbacks = 0, coupledImpactFallbacks = 0;
    int participantRound = 0;
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
    const auto makeDynamic = [&](SolverBody &s) {
        auto &body = bodies[s.slot];
        s.dynamic = true;
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
    };
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
        if (body.isDynamic && !body.sleeping && !body.simulationSuspended &&
            !body.rigidBody.IsStatic())
            makeDynamic(s);
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
    // Profiled under MK1's name: the same work (contact rows from manifolds).
    ProfileScope rowPreparation(mk2RowPreparationLabel);
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
    // A coupled participant may act on any awake body, whose pose its finish
    // then reads: every one is integrated inside the substep.
    if (coupling)
        for (auto &s : state)
            s.constrained |= s.dynamic;
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

    rowPreparation.End();

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
    // refreshGeometry restricted to what changed: bodies whose pose moved and
    // blocks re-collided. Every other row and block would be recomputed from
    // identical inputs, so the result is the full refresh's, exactly.
    const auto refreshChanged = [&](const std::vector<char> &moved,
                                    const std::vector<char> &dirtyBlock) {
        for (std::size_t i = 0; i < state.size(); ++i)
            if (moved[i] && state[i].dynamic && state[i].constrained)
                state[i].invI = state[i].rb->InverseInertiaWorld();
        for (std::size_t k = 0; k < blocks.size(); ++k) {
            auto &block = blocks[k];
            bool changed = false;
            for (std::size_t r = 0; r < block.count; ++r) {
                auto &row = rows[block.first + r];
                if (!dirtyBlock[k] && !moved[row.a] && !moved[row.b])
                    continue;
                const auto &A = state[row.a];
                const auto &B = state[row.b];
                const glm::vec3 d = AnchorGap(A, B, row) - row.gap0;
                row.rA = A.dq * row.rA0 - 0.5f * d;
                row.rB = B.dq * row.rB0 + 0.5f * d;
                PrepareRow(A, B, row);
                changed = true;
            }
            if (changed)
                PrepareBlock(state, rows, block);
        }
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
    const auto recollideGroup = [&](const Recollision &g) {
        {
            const auto &bodyA = bodies[g.slotA], &bodyB = bodies[g.slotB];
            const auto manifold =
                ComputeContacts(PrimitiveAt(bodyA.shape, bodyA.rigidBody, g.partA),
                                PrimitiveAt(bodyB.shape, bodyB.rigidBody, g.partB), g.margin,
                                nullptr, nullptr, true);
            if (manifold.uncertain)
                return;
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
    const auto recollide = [&] {
        for (const auto &g : recollisions)
            recollideGroup(g);
    };

    // Per-body substep motion: one ledger segment per substep, so queries see
    // the actually travelled path and integration matches the ledger exactly.
    std::vector<std::vector<RigidMotionSegment>> segments(state.size());
    std::vector<glm::vec3> pseudoLinear(state.size()), pseudoAngular(state.size());
    std::vector<std::pair<glm::vec3, glm::vec3>> savedVelocity;
    std::vector<char> jointMember(state.size()), jointBlocks;
    std::unordered_map<const RigidBody *, std::size_t> stateOf;
    for (std::size_t i = 0; i < state.size(); ++i)
        if (state[i].dynamic)
            stateOf.emplace(state[i].rb, i);
    for (int step = 0; step < substeps; ++step) {
        const double begin = double(dt) * step / substeps,
                     end = step + 1 == substeps ? double(dt) : double(dt) * (step + 1) / substeps;
        for (auto &s : state)
            if (s.dynamic && s.constrained)
                s.rb->linearVelocity += s.gravityStep;
        // A kinematic body acts with its prescribed motion over this substep,
        // not its commanded rate: a target reached (or a command starting)
        // inside the step moves it for only part of the step, and a body that
        // has stopped must not strike at the speed it was given.
        for (auto &s : state)
            if (s.kinematic && bodies[s.slot].HasMotion()) {
                const auto &motion = bodies[s.slot].motion;
                const auto from = motion.EvaluateGeometryAfterEvents(begin);
                const auto to = motion.EvaluateGeometryAfterEvents(end);
                s.rb->linearVelocity = glm::vec3(
                    (glm::dvec3(to.position) - glm::dvec3(from.position)) / (end - begin));
                s.rb->angularVelocity = motion.EvaluateAfterEvents(begin).angularVelocity;
            }
        // Impacts at their arrival instant. A row whose gap s closes inside
        // this substep at its pre-solve approach speed v (the physical impact
        // speed) arrives at fraction s/(|v|h). Its coupled set (every block
        // sharing a dynamic body with an arrival) is resolved instantaneously
        // from the pre-solve velocities, before the substep's ordinary solve,
        // and each struck body's ledger changes velocity at that instant
        // (below). Restitution is Poisson's: each cycle compresses the set
        // (a converged inelastic solve of fresh, push-only impulses), then adds
        // e times each fast-approaching contact's compression impulse; a
        // rebound driven into a neighbour starts another cycle, so a column
        // resolves as its sequence of binary impacts. For frictionless
        // contacts the loss is (1-e^2)/2 P'AP >= 0, so unlike simultaneous
        // Newton targets it never creates energy. (A restitution pass after the
        // substeps instead held each bounce at the contact until the next step,
        // e|v|dt late, and saw only approaches that existed at the step start.)
        std::vector<char> impacting(blocks.size(), 0);
        std::vector<float> arrival(state.size(), 2.0f), impactNormal;
        std::vector<glm::vec3> impactLinear, impactAngular;
        bool anyImpact = false;
        for (std::size_t index = 0; index < blocks.size(); ++index)
            for (std::size_t k = 0; k < blocks[index].count; ++k) {
                auto &row = rows[blocks[index].first + k];
                const float gap =
                    glm::dot(AnchorGap(state[row.a], state[row.b], row), row.n) + row.separation;
                const float v = RowNormalSpeed(state, row);
                // A gap closing exactly at the substep's end arrives in it
                // (otherwise the speculative row stops it there inelastically).
                if (!(v < -kMk2RestitutionThreshold) || !(std::max(gap, 0.0f) <= -v * h + kMk2Slop))
                    continue;
                row.arriving = true;
                impacting[index] = 1;
                anyImpact = true;
                for (const unsigned body : {row.a, row.b})
                    if (state[body].dynamic)
                        arrival[body] =
                            std::min(arrival[body], std::min(1.0f, std::max(gap, 0.0f) / (-v * h)));
            }
        if (anyImpact) {
            std::vector<char> struck(state.size(), 0);
            for (std::size_t index = 0; index < blocks.size(); ++index)
                if (impacting[index])
                    for (std::size_t k = 0; k < blocks[index].count; ++k) {
                        const auto &row = rows[blocks[index].first + k];
                        struck[row.a] |= char(state[row.a].dynamic);
                        struck[row.b] |= char(state[row.b].dynamic);
                    }
            for (std::size_t index = 0; index < blocks.size(); ++index) {
                const auto &row = rows[blocks[index].first];
                impacting[index] |= char(struck[row.a] || struck[row.b]);
            }
            // One instant per coupled set: the impulse crosses it rigidly.
            for (bool changed = true; changed;) {
                changed = false;
                for (std::size_t index = 0; index < blocks.size(); ++index) {
                    if (!impacting[index])
                        continue;
                    const auto &row = rows[blocks[index].first];
                    const float t = std::min(arrival[row.a], arrival[row.b]);
                    for (const unsigned body : {row.a, row.b})
                        if (state[body].dynamic && t < arrival[body]) {
                            arrival[body] = t;
                            changed = true;
                        }
                }
            }
            std::vector<std::size_t> impactingList;
            for (std::size_t index = 0; index < blocks.size(); ++index)
                if (impacting[index])
                    impactingList.push_back(index);
            const auto each = [&](auto &&f) {
                for (const auto index : impactingList)
                    for (std::size_t k = 0; k < blocks[index].count; ++k)
                        f(blocks[index].first + k);
            };
            std::vector<float> warmNormal(rows.size()), warmT1(rows.size()), warmT2(rows.size());
            impactNormal.assign(rows.size(), 0.0f);
            each([&](std::size_t i) {
                warmNormal[i] = rows[i].normalImpulse;
                warmT1[i] = rows[i].tangent1;
                warmT2[i] = rows[i].tangent2;
            });
            impactLinear.resize(state.size());
            impactAngular.resize(state.size());
            for (std::size_t i = 0; i < state.size(); ++i) {
                impactLinear[i] = state[i].rb->linearVelocity;
                impactAngular[i] = state[i].rb->angularVelocity;
            }
            // Solve the impact in its own configuration: each struck body at
            // its pose at the arrival instant (pre-impact free flight), so the
            // impulses act through the actual contact points and conserve
            // angular momentum. The substep then starts from the start poses.
            std::vector<std::pair<glm::vec3, glm::quat>> startPose(state.size());
            std::vector<char> moved(state.size(), 0), recollided(blocks.size(), 0),
                clean(blocks.size(), 0);
            for (std::size_t i = 0; i < state.size(); ++i) {
                auto &body = *state[i].rb;
                startPose[i] = {body.position, body.orientation};
                if (!state[i].dynamic || !(arrival[i] > 0 && arrival[i] <= 1))
                    continue;
                moved[i] = 1;
                RigidMotionSegment flight;
                flight.exactRotation = true;
                flight.begin = 0;
                flight.end = double(h) * arrival[i];
                flight.inverseMass = body.inverseMass;
                flight.inverseInertiaLocal = body.inverseInertiaLocal;
                flight.position = body.position;
                flight.orientation = body.orientation;
                flight.linearVelocity = body.linearVelocity;
                flight.angularVelocity = body.angularVelocity;
                flight.driftAngularVelocity = AngularDriftGenerator(
                    flight.inverseInertiaLocal, flight.orientation, flight.angularVelocity,
                    float(flight.end));
                const auto pose = flight.EvaluateGeometry(flight.end);
                body.position = pose.position;
                body.orientation = pose.orientation;
            }
            const auto placeAt = [&](const std::vector<char> &dirtyBlock) {
                for (std::size_t i = 0; i < state.size(); ++i)
                    if (moved[i]) {
                        auto &s = state[i];
                        s.dp = glm::vec3(glm::dvec3(s.rb->position) - glm::dvec3(s.x0));
                        s.dq = glm::mat3_cast(s.rb->orientation * glm::conjugate(s.q0));
                    }
                refreshChanged(moved, dirtyBlock);
            };
            placeAt(clean);
            // At the arrival configuration each arriving pair is re-collided:
            // a row's anchors have slid apart since the step start (by the
            // tangential travel), and an impulse through their midpoint would
            // act off the actual contact point and turn into spin.
            for (std::size_t index = 0; index < blocks.size(); ++index) {
                const auto &block = blocks[index];
                bool arrives = false;
                for (std::size_t k = 0; k < block.count; ++k)
                    arrives = arrives || rows[block.first + k].arriving;
                if (!arrives || block.first >= pendingCache.size())
                    continue;
                const auto &key = pendingCache[block.first].key;
                recollideGroup({block.first, block.count, key.slotA, key.slotB, key.partA,
                                key.partB, kMk2ArrivalMargin});
                recollided[index] = 1;
            }
            refreshChanged(moved, recollided);
            // Sequential impact propagation: repeatedly resolve the most
            // approaching pair of the set on its own (its whole manifold block,
            // so a flat landing stays symmetric): a compression of fresh,
            // push-only impulses, then e times that compression (Poisson). A
            // two-body impact with e <= 1 never gains energy, and each is
            // checked: one that would (friction) is resolved inelastically.
            // A struck column therefore resolves as its sequence of binary
            // impacts (a cradle passes the ball's speed to the far end), and
            // simultaneous arrivals rebound symmetrically. A set still
            // approaching after the bound ends with a converged inelastic
            // solve, which only dissipates.
            const auto kinetic = [&](const Block &block) {
                double k = 0;
                for (const unsigned body : {rows[block.first].a, rows[block.first].b}) {
                    const auto &sb = state[body];
                    if (!sb.dynamic)
                        continue;
                    const glm::dvec3 v(sb.rb->linearVelocity), w(sb.rb->angularVelocity);
                    k += 0.5 * glm::dot(v, v) / double(sb.invMass) +
                         0.5 * glm::dot(w, glm::dvec3(glm::inverse(sb.invI) * glm::vec3(w)));
                }
                return k;
            };
            const auto approachOf = [&](const Block &block) {
                float v = 0;
                for (std::size_t k = 0; k < block.count; ++k)
                    v = std::min(v, RowNormalSpeed(state, rows[block.first + k]));
                return v;
            };
            const auto resolve = [&](std::size_t index, bool elastic, bool frictionless = false) {
                PhysicsWorkTimer timer(work.impactSolve, work.active);
                const auto &block = blocks[index];
                std::vector<float> fast(block.count), friction(block.count);
                for (std::size_t k = 0; k < block.count; ++k) {
                    auto &row = rows[block.first + k];
                    fast[k] = RowNormalSpeed(state, row) < -kMk2RestitutionThreshold ? row.restitution : 0;
                    row.normalImpulse = row.tangent1 = row.tangent2 = 0;
                    friction[k] = row.friction;
                    if (frictionless)
                        row.friction = 0;
                }
                const float inverseH = 1.0f / h;
                for (int sweep = 0; sweep < kMk2ImpactSweeps; ++sweep)
                    SolveBlock(state, rows, blocks[index], inverseH, false, true);
                for (std::size_t k = 0; k < block.count; ++k) {
                    auto &row = rows[block.first + k];
                    row.friction = friction[k];
                    impactNormal[block.first + k] += row.normalImpulse;
                    if (elastic && fast[k] > 0 && row.normalImpulse > 0) {
                        const float extra = fast[k] * row.normalImpulse;
                        ApplyAxis(state[row.a], state[row.b], row.n, row.wnA, row.wnB, extra);
                        impactNormal[block.first + k] += extra;
                    }
                }
            };
            std::vector<std::pair<glm::vec3, glm::vec3>> before(2);
            // Most-approaching pair first, ties to the lower block index. A
            // resolution changes only its two bodies, so only blocks sharing
            // one are re-scored (stale queue entries are skipped by version).
            // (Only dynamic bodies: an impulse never changes a static or
            // kinematic velocity, and a floor would link every block.)
            std::vector<std::vector<std::size_t>> bodyBlocks(state.size());
            for (const auto index : impactingList) {
                const auto &row = rows[blocks[index].first];
                for (const unsigned body : {row.a, row.b})
                    if (state[body].dynamic)
                        bodyBlocks[body].push_back(index);
            }
            using Candidate = std::tuple<float, std::size_t, unsigned>;
            std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> queue;
            std::vector<unsigned> version(blocks.size(), 0);
            for (const auto index : impactingList)
                queue.push({approachOf(blocks[index]), index, 0u});
            const std::size_t impactBound =
                std::max<std::size_t>(kMk2ImpactPairs, kMk2ImpactsPerBlock * impactingList.size());
            for (std::size_t impact = 0; impact < impactBound; ++impact) {
                while (!queue.empty() && std::get<2>(queue.top()) != version[std::get<1>(queue.top())])
                    queue.pop();
                if (queue.empty() || !(std::get<0>(queue.top()) < -kMk2ImpactApproach))
                    break;
                const std::size_t worst = std::get<1>(queue.top());
                queue.pop();
                const auto &block = blocks[worst];
                const unsigned bodyA = rows[block.first].a, bodyB = rows[block.first].b;
                before[0] = {state[bodyA].rb->linearVelocity, state[bodyA].rb->angularVelocity};
                before[1] = {state[bodyB].rb->linearVelocity, state[bodyB].rb->angularVelocity};
                ++stats.impactEvents;
                const double k0 = kinetic(block);
                const std::vector<float> saved(impactNormal.begin() + block.first,
                                               impactNormal.begin() + block.first + block.count);
                const auto gained = [&] { return kinetic(block) > k0 * (1 + 1e-6) + 1e-9; };
                const auto undo = [&] {
                    state[bodyA].rb->linearVelocity = before[0].first;
                    state[bodyA].rb->angularVelocity = before[0].second;
                    state[bodyB].rb->linearVelocity = before[1].first;
                    state[bodyB].rb->angularVelocity = before[1].second;
                    std::copy(saved.begin(), saved.end(), impactNormal.begin() + block.first);
                };
                resolve(worst, true);
                // Friction can make a Poisson rebound gain energy at an
                // off-centre point. Frictionless, it loses (1-e^2)/2 P'AP, so
                // the bounce is kept; inelastic is the last resort. (Going
                // straight to inelastic made a struck elastic sphere stick.)
                if (gained()) {
                    ++frictionlessRetries;
                    undo();
                    resolve(worst, true, true);
                }
                if (gained()) {
                    ++inelasticFallbacks;
                    undo();
                    resolve(worst, false);
                }
                for (const unsigned body : {bodyA, bodyB})
                    for (const auto index : bodyBlocks[body])
                        queue.push({approachOf(blocks[index]), index, ++version[index]});
            }
            bool approaching = false;
            for (const auto index : impactingList)
                approaching = approaching || approachOf(blocks[index]) < -kMk2ImpactApproach;
            if (approaching) {
                ++coupledImpactFallbacks;
                each([&](std::size_t i) { rows[i].normalImpulse = rows[i].tangent1 = rows[i].tangent2 = 0; });
                for (int sweep = 0; sweep < kMk2ImpactSweeps; ++sweep)
                    SolveBlocks(state, rows, blocks, impactingList, h, false, true);
                each([&](std::size_t i) { impactNormal[i] += rows[i].normalImpulse; });
            }
            for (std::size_t i = 0; i < state.size(); ++i) {
                state[i].rb->position = startPose[i].first;
                state[i].rb->orientation = startPose[i].second;
            }
            placeAt(recollided);
            each([&](std::size_t i) {
                rows[i].normalImpulse = warmNormal[i];
                rows[i].tangent1 = warmT1[i];
                rows[i].tangent2 = warmT2[i];
            });
            for (std::size_t i = 0; i < state.size(); ++i) {
                impactLinear[i] = state[i].rb->linearVelocity - impactLinear[i];
                impactAngular[i] = state[i].rb->angularVelocity - impactAngular[i];
            }
        }
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
        std::vector<JointInput> inputs;
        if (!joints.empty()) {
            inputs = JointInputs();
            haveJoints = !inputs.empty();
            if (step == 0) {
                for (const auto &input : inputs)
                    jointTotals.push_back({input.state});
                // Warm starts are impulses per substep: rescale them when this
                // step's substep differs from the one that wrote them (a
                // coupled interval after ordinary steps, or back).
                if (mk2JointWarmH > 0 && mk2JointWarmH != h) {
                    const float scale = h / mk2JointWarmH;
                    for (auto &joint : joints) {
                        for (auto &value : joint.warm)
                            value *= scale;
                        joint.state.angularDrive.impulses *= scale;
                    }
                }
                mk2JointWarmH = h;
            }
            // Joints warm-start once per step, from the step's history, as
            // in MK1 (whose controllers also reset drive history every
            // step). Re-warming each substep from the previous substep would
            // re-apply a limit stop's impact spike (a collapsing ragdoll
            // rebounded off its own knee and hip stops) and let drives carry
            // in-step history MK1 never had.
            // No positional bias: joint error is the projection's (below).
            PrepareJoints(jointSolver, inputs, h, true, false, false, 0.0f, step > 0, false,
                          kMk2JointLimitApproachRate);
        }
        // A coupled participant's impulse may wake a sleeping body mid-solve
        // (as MK1's material rounds). It joins at this boundary: its deferred
        // external velocity is consumed, rows prepared against it as a static
        // participant are re-prepared, and its articulation's joints are
        // appended without re-applying the warm starts already solved.
        const auto joinAwakened = [&] {
            for (bool changed = true; changed;) {
                changed = false;
                for (const auto &joint : joints) {
                    if (!JointActive(joint))
                        continue;
                    auto *a = Get(joint.state.settings.bodyA);
                    auto *b = Get(joint.state.settings.bodyB);
                    if (!a || !b)
                        continue;
                    if (a->isDynamic && !a->sleeping && b->sleeping) {
                        Wake(joint.state.settings.bodyB.id & kSlotMask);
                        changed = true;
                    } else if (b->isDynamic && !b->sleeping && a->sleeping) {
                        Wake(joint.state.settings.bodyA.id & kSlotMask);
                        changed = true;
                    }
                }
            }
            bool joined = false;
            for (const auto slot : aliveSlots) {
                auto &body = bodies[slot];
                if (!body.isDynamic || body.sleeping || body.simulationSuspended ||
                    body.rigidBody.IsStatic())
                    continue;
                if (index[slot] >= 0 && state[std::size_t(index[slot])].dynamic)
                    continue;
                const unsigned i = index[slot] >= 0 ? unsigned(index[slot]) : addBody(slot);
                auto &s = state[i];
                if (!s.dynamic)
                    makeDynamic(s);
                s.constrained = true;
                s.rb->linearVelocity += s.gravityStep; // this substep's share
                stateOf.emplace(s.rb, i);
                joined = true;
            }
            if (!joined)
                return;
            segments.resize(state.size());
            pseudoLinear.resize(state.size());
            pseudoAngular.resize(state.size());
            jointMember.resize(state.size());
            arrival.resize(state.size(), 2.0f);
            if (!impactLinear.empty()) {
                impactLinear.resize(state.size());
                impactAngular.resize(state.size());
            }
            for (auto &row : rows)
                PrepareRow(state[row.a], state[row.b], row);
            for (auto &block : blocks)
                PrepareBlock(state, rows, block);
            if (!joints.empty()) {
                const auto oldRows = jointSolver.RowCount();
                inputs = JointInputs();
                PrepareJoints(jointSolver, inputs, h, true, false, true, 0.0f, step > 0, false,
                              kMk2JointLimitApproachRate);
                for (const auto &input : inputs)
                    if (std::none_of(jointTotals.begin(), jointTotals.end(),
                                     [&](const JointTotals &t) { return t.state == input.state; }))
                        jointTotals.push_back({input.state});
                haveJoints = !inputs.empty();
                if (haveJoints)
                    jointSolver.SolveIteration(oldRows);
            }
        };
        if (coupling && coupling->begin && step == 0) {
            coupling->begin(dt);
            joinAwakened();
        }
        const int share = coupling ? int(coupling->iterations) / substeps +
                                         (step < int(coupling->iterations) % substeps)
                                   : 0;
        const int rounds = std::max(kMk2Iterations, share);
        // Joints first, contacts last in every sweep (as Box2D v3): the final
        // velocity of each sweep never approaches a contact, so a joint pull
        // becomes joint error (recovered by the position projection), not penetration.
        for (int iteration = 0; iteration < rounds; ++iteration) {
            if (haveJoints)
                jointSolver.SolveIteration();
            if (coupling && coupling->solve && iteration < share) {
                coupling->solve(dt, unsigned(participantRound++));
                joinAwakened();
            }
            SolveRows(state, rows, blocks, h, true);
        }
        // Articulations take further sweeps, with only the contact blocks that
        // touch a jointed body: the rest of the world keeps its own count.
        if (haveJoints) {
            std::fill(jointMember.begin(), jointMember.end(), char(0));
            for (auto *rb : jointSolver.Bodies())
                if (const auto found = stateOf.find(rb); found != stateOf.end())
                    jointMember[found->second] = 1;
            jointBlocks.assign(blocks.size(), char(0));
            for (std::size_t k = 0; k < blocks.size(); ++k)
                for (std::size_t r = 0; r < blocks[k].count; ++r) {
                    const auto &row = rows[blocks[k].first + r];
                    if (jointMember[row.a] || jointMember[row.b])
                        jointBlocks[k] = 1;
                }
            for (int iteration = rounds; iteration < kMk2JointIterations; ++iteration) {
                jointSolver.SolveIteration();
                SolveRows(state, rows, blocks, h, true, &jointBlocks);
            }
        }
        // The joints' position correction for this substep, as pseudo-velocity
        // solved from rest on the same members; real velocities are restored.
        // (Contact rows in this solve were measured: no change in the rag-
        // doll's joint error or penetration, at extra cost.)
        std::fill(pseudoLinear.begin(), pseudoLinear.end(), glm::vec3(0));
        std::fill(pseudoAngular.begin(), pseudoAngular.end(), glm::vec3(0));
        if (haveJoints) {
            const auto &members = jointSolver.Bodies();
            savedVelocity.clear();
            for (auto *rb : members) {
                savedVelocity.push_back({rb->linearVelocity, rb->angularVelocity});
                rb->linearVelocity = rb->angularVelocity = glm::vec3(0);
            }
            PrepareJoints(mk2JointProjection, inputs, h, false, false, false,
                          kMk2JointProjectionRate, false, true);
            for (int iteration = 0; iteration < kMk2JointProjectionIterations; ++iteration)
                mk2JointProjection.SolveIteration();
            for (std::size_t m = 0; m < members.size(); ++m) {
                auto *rb = members[m];
                if (const auto found = stateOf.find(rb); found != stateOf.end()) {
                    pseudoLinear[found->second] = rb->linearVelocity;
                    pseudoAngular[found->second] = rb->angularVelocity;
                }
                rb->linearVelocity = savedVelocity[m].first;
                rb->angularVelocity = savedVelocity[m].second;
            }
        }
        for (std::size_t i = 0; i < state.size(); ++i) {
            auto &s = state[i];
            auto &body = bodies[s.slot];
            if (s.dynamic && s.constrained) {
                // A struck body moves without its impact's velocity change
                // until the arrival instant, then with it.
                const auto advance = [&](double from, double to, const glm::vec3 &v,
                                         const glm::vec3 &w) {
                    RigidMotionSegment seg;
                    seg.exactRotation = true;
                    seg.owner = MakeHandle(s.slot).id;
                    seg.begin = from;
                    seg.end = to;
                    seg.inverseMass = body.rigidBody.inverseMass;
                    seg.inverseInertiaLocal = body.rigidBody.inverseInertiaLocal;
                    seg.position = body.rigidBody.position;
                    seg.orientation = body.rigidBody.orientation;
                    seg.linearVelocity = v + pseudoLinear[i];
                    seg.angularVelocity = w + pseudoAngular[i];
                    seg.driftAngularVelocity =
                        AngularDriftGenerator(seg.inverseInertiaLocal, seg.orientation,
                                              seg.angularVelocity, float(to - from));
                    const auto pose = seg.EvaluateGeometry(to);
                    seg.endPosition = pose.position;
                    seg.endOrientation = pose.orientation;
                    const RigidBody next = seg.Evaluate(to);
                    segments[i].push_back(seg);
                    body.rigidBody.position = next.position;
                    body.rigidBody.orientation = next.orientation;
                    // The real velocity, transported (angular momentum kept) to
                    // the pose actually reached: transport is linear, so remove
                    // the pseudo-velocity's transported part, not its raw value.
                    return next.angularVelocity -
                           TransportAngularVelocity(seg.inverseInertiaLocal, seg.orientation,
                                                    next.orientation, pseudoAngular[i]);
                };
                const glm::vec3 v = body.rigidBody.linearVelocity;
                glm::vec3 w = body.rigidBody.angularVelocity;
                if (arrival[i] == 1.0f) { // at the substep's end (none is 2)
                    body.rigidBody.angularVelocity =
                        advance(begin, end, v - impactLinear[i], w - impactAngular[i]) +
                        impactAngular[i];
                } else if (arrival[i] > 0 && arrival[i] < 1) {
                    const double at = begin + (end - begin) * double(arrival[i]);
                    const glm::vec3 spin = advance(begin, at, v - impactLinear[i],
                                                   w - impactAngular[i]);
                    w = spin + impactAngular[i];
                    body.rigidBody.angularVelocity = advance(at, end, v, w);
                } else {
                    body.rigidBody.angularVelocity = advance(begin, end, v, w);
                }
                s.dp = glm::vec3(glm::dvec3(body.rigidBody.position) - glm::dvec3(s.x0));
                s.dq = glm::mat3_cast(body.rigidBody.orientation * glm::conjugate(s.q0));
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
        for (auto &row : rows) {
            const float gap =
                glm::dot(AnchorGap(state[row.a], state[row.b], row), row.n) + row.separation;
            const float v = RowNormalSpeed(state, row);
            row.arrivesNext = !row.arriving && gap > 0 && v < -kMk2RestitutionThreshold &&
                              gap <= -v * h + kMk2Slop;
        }
        // Relax removes every positional bias velocity, joints' included, so
        // neither joint nor contact correction outlives its substep as motion.
        for (int iteration = 0; iteration < kMk2RelaxIterations; ++iteration) {
            if (haveJoints)
                jointSolver.SolveIteration(0, false);
            SolveRows(state, rows, blocks, h, false);
        }
        if (haveJoints)
            collectJoints();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            auto &row = rows[i];
            row.totalNormal += row.normalImpulse + (impactNormal.empty() ? 0.0f : impactNormal[i]);
            row.totalTangent += row.t1 * row.tangent1 + row.t2 * row.tangent2;
            // An impact is a spike, not a support load: never a steady estimate.
            if (row.arriving) {
                row.arriving = false;
                continue;
            }
            // A row that just engaged has no carried load yet; its first
            // loaded relaxed substep becomes the steady estimate. Only at
            // substep 0, a contact that closed later kept a zero cap, so its
            // warm start was erased every substep and its cache every step:
            // a ragdoll leg resting on such rows rocked without friction.
            if (row.steadyNormal == 0)
                row.steadyNormal = row.normalImpulse;
        }
        if (coupling && coupling->finish && step + 1 == substeps)
            coupling->finish(dt);
    }
    for (auto &row : rows)
        row.cachedNormal = row.normalImpulse;
    for (auto &t : jointTotals) {
        t.state->reactionImpulse = t.reaction;
        t.state->reactionAngularImpulse = t.angular;
        t.state->motorImpulse = t.motor;
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
            body.motion.Begin(MakeHandle(slot).id, body.rigidBody, dt, true);
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
        // A speculative row that closed its gap and carried load this step is
        // a touch of this step (MK1 records the same arrival as an impact).
        if (rows[i].totalNormal > 0 && i < mk2Points.size() &&
            !stepTouches.count({pair.first, pair.second}))
            Observe(mk2Points[i].a, mk2Points[i].b, mk2Points[i].contact);
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
    // Frame sums naturally include every coupled interval and fixed step. Batch
    // telemetry here instead of recording a profiler event per retry/contact.
    JUDAS_PROFILE_COUNTER("Physics restitution frictionless retries", double(frictionlessRetries), ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics restitution inelastic fallbacks", double(inelasticFallbacks), ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics restitution coupled fallbacks", double(coupledImpactFallbacks), ProfileCounterMode::Sum);
}

// Bounded continuous pass for fast bodies. The substep solve's speculative
// rows close only the gaps they were built from; a fast or spinning body can
// still meet geometry its linearized rows did not represent, and a body set
// moving by an impact inside the step meets bodies it had no rows with. Each
// fast body's recorded ledger path is swept with the exact MK1 query. Arrivals
// are taken in time order: each is resolved with the exact impulse and both
// bodies' ledgers continue from that instant with their new velocities, so
// the rest of the step is not lost; the re-timed paths are swept again from
// there. A body is re-timed at most kMk2ArrivalsPerBody times per step; past
// that its pose is held for the rest of the step (velocity kept), so no
// unbounded event chain can form.
void PhysicsWorld::Impl::Mk2Continuous(float dt) {
    stepDuration = dt;
    impactQueryAnchorTime = 0;
    for (auto slot : aliveSlots) {
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
    }
    std::vector<unsigned> candidates;
    const auto fastReach = [&](unsigned slot) {
        const auto &body = bodies[slot];
        double feature = std::numeric_limits<double>::infinity(), reach = 0;
        for (int part = 0; part < PrimitiveCount(body.shape); ++part)
            feature = std::min(feature, PrimitiveFeature(slot, part));
        for (const auto &segment : body.motion.Segments())
            reach = std::max(reach, (double(glm::length(segment.linearVelocity)) +
                                     double(glm::length(segment.angularVelocity)) *
                                         double(body.boundingRadius)) *
                                        double(dt));
        return reach > kMk2FastFraction * feature;
    };
    const auto movable = [&](unsigned slot) {
        const auto &body = bodies[slot];
        return body.isDynamic && !body.isKinematic && !body.rigidBody.IsStatic() &&
               !body.simulationSuspended && !body.sensor && !body.queryOnly;
    };
    // Continue (or hold) a body's ledger from `at` with its current velocity.
    const auto retime = [&](unsigned slot, double at, bool hold) {
        auto &body = bodies[slot];
        std::vector<RigidMotionSegment> kept;
        for (auto segment : body.motion.Segments()) {
            if (segment.begin >= at && !kept.empty())
                break;
            if (segment.end > at) {
                const auto pose = segment.EvaluateGeometry(at);
                segment.end = at;
                segment.endPosition = pose.position;
                segment.endOrientation = pose.orientation;
            }
            kept.push_back(segment);
        }
        if (kept.empty())
            return;
        RigidMotionSegment next = kept.back();
        next.begin = at;
        next.end = dt;
        next.position = next.endPosition;
        next.orientation = next.endOrientation;
        if (hold) {
            next.linearVelocity = next.angularVelocity = next.driftAngularVelocity = glm::vec3(0);
        } else {
            next.linearVelocity = body.rigidBody.linearVelocity;
            next.angularVelocity = body.rigidBody.angularVelocity;
            next.driftAngularVelocity =
                AngularDriftGenerator(next.inverseInertiaLocal, next.orientation,
                                      next.angularVelocity, float(double(dt) - at));
        }
        const auto pose = next.EvaluateGeometry(dt);
        next.endPosition = pose.position;
        next.endOrientation = pose.orientation;
        if (!hold)
            body.rigidBody.angularVelocity = next.Evaluate(dt).angularVelocity;
        kept.push_back(next);
        body.rigidBody.position = next.endPosition;
        body.rigidBody.orientation = next.endOrientation;
        body.motion.SetRecorded(std::move(kept));
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
    };
    struct Mover {
        unsigned slot;
        double from = 0;     // swept from here
        bool firstSweep = true; // rows still describe this path
        int arrivals = 0;
        double hit = 0;
        unsigned other = 0;
        int partA = 0, partB = 0;
        bool found = false;
    };
    std::vector<Mover> movers;
    const auto moverOf = [&](unsigned slot) -> Mover * {
        for (auto &m : movers)
            if (m.slot == slot)
                return &m;
        return nullptr;
    };
    // The earliest arrival on a mover's path from `from`.
    const auto sweep = [&](Mover &m) {
        const unsigned slot = m.slot;
        m.found = false;
        const auto &found = ImpactCandidates(slot, RemainingImpactBound(slot, m.from, dt));
        candidates.assign(found.begin(), found.end());
        double earliest = double(dt) + 1;
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
                    // quarter feature deep; otherwise its rows own it. On the
                    // first sweep, a pair with rows arrived legitimately unless
                    // it then went deep: behind its own row plane, behind the
                    // first-touch plane carried to the step end, deeply
                    // overlapped at the end, or past the same depth search from
                    // first touch. A pair with neither had no response at all,
                    // and neither has any pair on a re-timed path.
                    const bool touching = PairTouches(a, b, pa, pb, m.from);
                    const double limit = std::min(earliest, double(dt));
                    const double hit = ImpactTime(a, b, pa, pb, m.from, limit);
                    if (!(hit < limit))
                        continue;
                    const std::array<unsigned, 4> key{a, b, unsigned(pa), unsigned(pb)};
                    const auto row = std::lower_bound(mk2RowPairs.begin(), mk2RowPairs.end(), key);
                    const bool solved = m.firstSweep && row != mk2RowPairs.end() && *row == key;
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
                    m.found = true;
                    m.hit = hit;
                    m.other = other;
                    m.partA = slot == a ? pa : pb;
                    m.partB = slot == a ? pb : pa;
                }
        }
    };
    // A struck dynamic body continues from the arrival with its new velocity
    // and is swept from there (it may now be fast).
    const auto struck = [&](unsigned other, double at) {
        if (!movable(other))
            return;
        if (bodies[other].sleeping)
            Wake(other);
        Mover *m = moverOf(other);
        if (!m) {
            movers.push_back({other, at, false});
            m = &movers.back();
        }
        if (m->arrivals >= kMk2ArrivalsPerBody) {
            ++stats.impactSafetyFallback;
            ++stats.impactEventCapFallback;
            ++stats.impactBodyCapHolds;
            retime(other, at, true);
            m->found = false;
            return;
        }
        ++m->arrivals;
        retime(other, at, false);
        m->from = at;
        m->firstSweep = false;
        sweep(*m);
    };
    // A fast kinematic mover cannot be held; it strikes. Its recorded path is
    // swept against each dynamic candidate not touching it at the start. A
    // speculative row is linearized at one anchor and cannot follow a spinning
    // surface (an arm's equal endpoint poses miss a body its sweep passes
    // through), so rows do not own these arrivals; the exact impulse acts on
    // the post-solve velocities and adds nothing where rows already separate.
    for (const auto slot : aliveSlots) {
        const auto &body = bodies[slot];
        if (!body.isKinematic || body.simulationSuspended || body.sensor || body.queryOnly ||
            !body.HasMotion() || !fastReach(slot))
            continue;
        const auto &found = ImpactCandidates(slot, RemainingImpactBound(slot, 0, dt));
        candidates.assign(found.begin(), found.end());
        const std::vector<unsigned> struckBy(candidates);
        for (const auto other : struckBy) {
            const auto &o = bodies[other];
            if (other == slot || !movable(other) || !CanRespond(slot, other))
                continue;
            const unsigned a = std::min(slot, other), b = std::max(slot, other);
            double earliest = double(dt);
            int hitPartA = 0, hitPartB = 0;
            for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb) {
                    if (PairTouches(a, b, pa, pb, 0))
                        continue;
                    const double hit = ImpactTime(a, b, pa, pb, 0, earliest);
                    if (hit < earliest) {
                        earliest = hit;
                        hitPartA = slot == a ? pa : pb;
                        hitPartB = slot == a ? pb : pa;
                    }
                }
            if (!(earliest < double(dt)))
                continue;
            if (o.sleeping)
                Wake(other);
            ++stats.impactEvents;
            if (Mk2ResolveArrival(slot, other, hitPartA, hitPartB, earliest))
                struck(other, earliest);
        }
    }
    for (const auto slot : aliveSlots) {
        const auto &body = bodies[slot];
        if (!movable(slot) || body.sleeping || moverOf(slot) || !fastReach(slot))
            continue;
        movers.push_back({slot, 0, true});
        sweep(movers.back());
    }
    for (int events = 0; events < kMk2ArrivalsPerStep; ++events) {
        Mover *next = nullptr;
        for (auto &m : movers)
            if (m.found && (!next || m.hit < next->hit))
                next = &m;
        if (!next)
            break;
        const unsigned slot = next->slot, other = next->other;
        const double at = next->hit;
        ++stats.impactEvents;
        // Without an exact response the path would continue into the contact.
        const bool resolved = Mk2ResolveArrival(slot, other, next->partA, next->partB, at);
        if (!resolved || next->arrivals >= kMk2ArrivalsPerBody) {
            ++stats.impactSafetyFallback;
            if (!resolved) {
                ++stats.impactResponseHolds;
            } else {
                ++stats.impactEventCapFallback;
                ++stats.impactBodyCapHolds;
            }
            retime(slot, at, true);
            next->found = false;
        } else {
            ++next->arrivals;
            retime(slot, at, false);
            next->from = at;
            next->firstSweep = false;
            sweep(*next);
        }
        if (resolved)
            struck(other, at);
        // Paths swept against a body that has just been re-timed are stale.
        for (auto &m : movers)
            if (m.found && m.slot != slot && m.slot != other && (m.other == slot || m.other == other))
                sweep(m);
    }
    // Arrivals left once the step's bound is spent are held there.
    for (auto &m : movers)
        if (m.found) {
            ++stats.impactSafetyFallback;
            ++stats.impactEventCapFallback;
            ++stats.impactStepCapHolds;
            ++stats.impactEvents;
            Mk2ResolveArrival(m.slot, m.other, m.partA, m.partB, m.hit);
            retime(m.slot, m.hit, true);
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
// Velocities and the step's touch only; poses and ledgers are not changed here.
// False when no first-touch manifold or no energy-safe response exists.
bool PhysicsWorld::Impl::Mk2ResolveArrival(unsigned fast, unsigned other, int partFast,
                                           int partOther, double hit) {
    ++stats.impactResponseAttempts;
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
    if (!manifold.count || manifold.uncertain) {
        ++stats.impactMissingManifolds;
        return false;
    }
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
    if (!result.energyBudgetSatisfied) {
        ++stats.impactEnergyRejections;
        return false;
    }
    ++stats.impactResponsesApplied;
    for (auto [copy, slot] : {std::pair<RigidBody *, unsigned>{&bodyA, a}, {&bodyB, b}})
        if (movable(slot)) {
            bodies[slot].rigidBody.linearVelocity = copy->linearVelocity;
            bodies[slot].rigidBody.angularVelocity = copy->angularVelocity;
        }
    // The arrival is a touch of this step, as for MK1's impact events.
    for (const auto &row : rows)
        Observe(a, b, row.geometry);
    return true;
}
