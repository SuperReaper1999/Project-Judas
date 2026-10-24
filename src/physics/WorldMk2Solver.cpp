#include "WorldInternal.h"

#include <queue>
#include <tuple>
#include <unordered_map>


using namespace JudasPhysicsDetail;

// MK2's four chronological intervals share the contact/joint solve, physical
// velocities and anchored motion ledger. Immediate contacts resolve at the
// current pose; elastic open-gap arrivals resolve through chronological CCD.
// Bias is relaxed at its own configuration, then retained only as geometric
// projection. See docs/MK2_PHYSICS_ARCHITECTURE.md for the numerical contract.
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
// the same-pose relax removes that bias from physical velocity, retaining
// only geometric recovery, so recovery never becomes rebound. Soft (spring-like) rows were measured to sink under stacked load.
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
    double radius = 0, angularSpeedScale = 1;
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
    bool arriving = false, arrivesNext = false, deferredImpact = false;
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
// A conservative point-speed bound over a force-free rotation. The inertia
// condition bound follows |omega(t)| <= ||I^-1|| ||I|| |omega(0)| from fixed
// world angular momentum. Re-evaluate after solver impulses or powered inputs.
// When this cannot reach the existing restitution cutoff, support remains an
// ordinary speculative constraint rather than a non-existent bouncing event.
bool MayRebound(const std::vector<SolverBody>& bodies, const Row& row) {
    const auto& a=bodies[row.a];const auto& b=bodies[row.b];
    const auto linear=[](const SolverBody& body) {
        return body.dynamic || body.kinematic ? body.rb->linearVelocity : glm::vec3(0);
    };
    const auto angular=[](const SolverBody& body) {
        if (!body.dynamic && !body.kinematic) return 0.;
        const double speed=glm::length(glm::dvec3(body.rb->angularVelocity));
        return speed>0 ? speed*body.angularSpeedScale*body.radius : 0.;
    };
    return glm::length(glm::dvec3(linear(a)-linear(b)))+angular(a)+angular(b) > kMk2RestitutionThreshold;
}

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
    if (!impact && row.deferredImpact && MayRebound(bodies,row)) return -kMk2FreeSpeed;
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
    mk2ImpactObservations.clear();
    mk2ContinuousAwakened = false;
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
        const glm::dmat3 inverse(body.rigidBody.inverseInertiaLocal);
        if (glm::determinant(inverse)>0) {
            const auto norm=[](const glm::dmat3& matrix) {
                double result=0;
                for (int row=0;row<3;++row) {
                    double sum=0;
                    for (int column=0;column<3;++column) sum+=std::abs(matrix[column][row]);
                    result=std::max(result,sum);
                }
                return result;
            };
            s.angularSpeedScale=std::max(1.,norm(inverse)*norm(glm::inverse(inverse)));
        }
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
        s.radius = body.boundingRadius;
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
    // Every awake body shares the chronological intervals. An unchanged
    // linear flight keeps one velocity-episode anchor across those intervals.
    for (auto &s : state) s.constrained |= s.dynamic;

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
    struct LinearAnchor {
        glm::vec3 origin{0}, velocity{0};
        double time = 0;
        bool valid = false;
    };
    std::vector<LinearAnchor> linearAnchors(state.size());
    std::vector<std::vector<RigidMotionSegment>> segments(state.size());
    std::vector<glm::vec3> pseudoLinear(state.size()), pseudoAngular(state.size());
    std::vector<glm::vec3> contactProjectionLinear, contactProjectionAngular;
    std::vector<char> requiredContinuous(bodies.size());
    std::vector<unsigned> continuousArrivals(bodies.size(), 0);
    unsigned continuousEvents = 0;
    std::vector<RigidMotion> kinematicPaths(bodies.size());
    for (auto slot : aliveSlots) if (bodies[slot].isKinematic) kinematicPaths[slot] = bodies[slot].motion;
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
        std::fill(requiredContinuous.begin(), requiredContinuous.end(), char(0));
        for (auto& row : rows) {
            const float gap = glm::dot(AnchorGap(state[row.a], state[row.b], row), row.n) + row.separation;
            const float speed = RowNormalSpeed(state, row);
            row.deferredImpact = gap > kMk2Slop && row.restitution > 0;
            if (row.deferredImpact && MayRebound(state,row) && speed < 0 && gap <= -speed*h+kMk2Slop) {
                requiredContinuous[state[row.a].slot] = 1;
                requiredContinuous[state[row.b].slot] = 1;
            }
        }
        // Resolve contacts already present at this configuration. Open gaps
        // belong to the chronological CCD pass, never to a future trial pose
        // followed by a rewind. Poisson restitution uses one material factor
        // for each manifold's compression impulse.
        std::vector<char> impacting(blocks.size(), 0);
        std::vector<float> impactNormal;
        bool anyImpact = false;
        for (std::size_t index = 0; index < blocks.size(); ++index)
            for (std::size_t k = 0; k < blocks[index].count; ++k) {
                auto &row = rows[blocks[index].first + k];
                const float gap =
                    glm::dot(AnchorGap(state[row.a], state[row.b], row), row.n) + row.separation;
                const float v = RowNormalSpeed(state, row);
                // Open gaps are left to the represented motion and CCD.
                if (!(v < -kMk2RestitutionThreshold) || gap > kMk2Slop)
                    continue;
                row.arriving = true;
                impacting[index] = 1;
                anyImpact = true;
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
            const std::vector<char> unchanged(state.size(), 0);
            std::vector<char> recollided(blocks.size(), 0);
            // Refresh immediate impacts at this same configuration. Contact
            // anchors from an earlier interval must not supply a false lever arm.
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
            refreshChanged(unchanged, recollided);
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
                for (std::size_t k = 0; k < block.count; ++k) {
                    const auto& row=rows[block.first+k];
                    const float gap=glm::dot(AnchorGap(state[row.a],state[row.b],row),row.n)+row.separation;
                    if(row.arriving || gap<=kMk2Slop)
                        v = std::min(v, RowNormalSpeed(state,row));
                }
                return v;
            };
            const auto resolve = [&](std::size_t index, bool elastic, bool frictionless = false) {
                PhysicsWorkTimer timer(work.impactSolve, work.active);
                const auto &block = blocks[index];
                std::vector<float> fast(block.count), friction(block.count);
                float patchRestitution = 0;
                if (elastic && approachOf(block) < -kMk2RestitutionThreshold)
                    for (std::size_t k=0;k<block.count;++k)
                        patchRestitution = std::max(patchRestitution, rows[block.first+k].restitution);
                for (std::size_t k = 0; k < block.count; ++k) {
                    auto &row = rows[block.first + k];
                    fast[k] = patchRestitution;
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
            each([&](std::size_t i) {
                rows[i].normalImpulse = warmNormal[i];
                rows[i].tangent1 = warmT1[i];
                rows[i].tangent2 = warmT2[i];
            });
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
            if (step > 0 && mk2ContinuousAwakened)
                for (const auto& input : inputs)
                    if (std::none_of(jointTotals.begin(),jointTotals.end(),
                        [&](const JointTotals& total) { return total.state==input.state; }))
                        jointTotals.push_back({input.state});
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
        const auto joinAwakened = [&](bool applyCurrentShare = true) {
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
                if (applyCurrentShare) s.rb->linearVelocity += s.gravityStep;
                stateOf.emplace(s.rb, i);
                joined = true;
            }
            if (!joined)
                return;
            segments.resize(state.size());
            linearAnchors.resize(state.size());
            pseudoLinear.resize(state.size());
            pseudoAngular.resize(state.size());
            jointMember.resize(state.size());
            for (auto &row : rows)
                PrepareRow(state[row.a], state[row.b], row);
            for (auto &block : blocks)
                PrepareBlock(state, rows, block);
            // A post-CCD publication joins the next interval. Do not apply
            // force shares or old-configuration joint impulses retroactively.
            if (!applyCurrentShare) return;
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
        contactProjectionLinear.resize(state.size());
        contactProjectionAngular.resize(state.size());
        for (std::size_t i=0; i<state.size(); ++i) {
            contactProjectionLinear[i] = state[i].rb->linearVelocity;
            contactProjectionAngular[i] = state[i].rb->angularVelocity;
        }
        // Relax removes every positional bias velocity, joints' included, so
        // neither joint nor contact correction outlives its substep as motion.
        for (int iteration = 0; iteration < kMk2RelaxIterations; ++iteration) {
            if (haveJoints)
                jointSolver.SolveIteration(0, false);
            SolveRows(state, rows, blocks, h, false);
        }
        for (std::size_t i=0; i<state.size(); ++i) {
            contactProjectionLinear[i] -= state[i].rb->linearVelocity;
            contactProjectionAngular[i] -= state[i].rb->angularVelocity;
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
        for (const auto& row:rows) if (row.deferredImpact && MayRebound(state,row)) {
            const float gap=glm::dot(AnchorGap(state[row.a],state[row.b],row),row.n)+row.separation;
            const float speed=RowNormalSpeed(state,row);
            if (speed<0 && gap<=-speed*h+kMk2Slop) {
                requiredContinuous[state[row.a].slot]=1;
                requiredContinuous[state[row.b].slot]=1;
            }
        }
        std::vector<std::vector<RigidMotionSegment>> interval(state.size());
        for (std::size_t i = 0; i < state.size(); ++i) {
            auto& s = state[i];
            auto& body = bodies[s.slot];
            if (s.dynamic) {
                const auto oldOrientation = body.rigidBody.orientation;
                const auto realSpin = body.rigidBody.angularVelocity;
                RigidBody drift = body.rigidBody;
                drift.linearVelocity += pseudoLinear[i] + contactProjectionLinear[i];
                drift.angularVelocity += pseudoAngular[i] + contactProjectionAngular[i];
                AppendDynamicRigidMotion(interval[i], MakeHandle(s.slot).id, drift, 0, h);
                auto& anchor = linearAnchors[i];
                if (!anchor.valid || anchor.velocity != drift.linearVelocity ||
                    anchor.origin + anchor.velocity * float(begin-anchor.time) != drift.position) {
                    anchor = {drift.position, drift.linearVelocity, begin, true};
                }
                for (auto& leaf : interval[i]) {
                    leaf.anchoredTranslation = true;
                    leaf.translationOrigin = anchor.origin;
                    leaf.translationTime = anchor.time-begin;
                    leaf.position = leaf.EvaluateGeometry(leaf.begin).position;
                    leaf.endPosition = leaf.EvaluateGeometry(leaf.end).position;
                    // Split impulses move geometry, never physical momentum.
                    leaf.linearProjection = pseudoLinear[i] + contactProjectionLinear[i];
                    leaf.angularProjection = pseudoAngular[i] + contactProjectionAngular[i];
                    leaf.hasPhysicalLinearVelocity = true;
                    leaf.physicalLinearVelocity = body.rigidBody.linearVelocity;
                    leaf.angularVelocity = TransportAngularVelocity(body.rigidBody.inverseInertiaLocal,
                        oldOrientation, leaf.orientation, realSpin);
                }
                const auto next = interval[i].back().Evaluate(h);
                body.rigidBody.position = next.position;
                body.rigidBody.orientation = next.orientation;
                body.rigidBody.angularVelocity = TransportAngularVelocity(body.rigidBody.inverseInertiaLocal,
                    oldOrientation, next.orientation, realSpin);
                body.motion.SetRecorded(interval[i]);
            } else if (s.kinematic && !kinematicPaths[s.slot].Segments().empty()) {
                std::vector<RigidMotionSegment> local;
                for (const auto& original : kinematicPaths[s.slot].Segments()) {
                    const double first = std::max(begin, original.begin), last = std::min(end, original.end);
                    if (first >= last) continue;
                    auto segment = original;
                    const auto pose = original.Evaluate(first);
                    segment.begin = first-begin; segment.end = last-begin;
                    segment.position = pose.position; segment.orientation = pose.orientation;
                    const auto finish = original.EvaluateGeometry(last);
                    segment.endPosition = finish.position; segment.endOrientation = finish.orientation;
                    local.push_back(segment);
                }
                body.motion.SetRecorded(std::move(local));
                const auto pose = body.motion.EvaluateGeometryAfterEvents(h);
                body.rigidBody.position = pose.position; body.rigidBody.orientation = pose.orientation;
            }
            s.dp = glm::vec3(glm::dvec3(body.rigidBody.position)-glm::dvec3(s.x0));
            s.dq = glm::mat3_cast(body.rigidBody.orientation*glm::conjugate(s.q0));
            InvalidateImpactPose(s.slot);
        }
        mk2RowPairs.clear(); mk2RowEndSeparation.clear();
        for (std::size_t i=0; i<rows.size() && i<pendingCache.size(); ++i) {
            if (rows[i].deferredImpact && MayRebound(state,rows[i])) continue;
            const auto& key = pendingCache[i].key;
            const std::array<unsigned,4> pair{key.slotA,key.slotB,unsigned(key.partA),unsigned(key.partB)};
            const float gap = glm::dot(AnchorGap(state[rows[i].a],state[rows[i].b],rows[i]), rows[i].n)+rows[i].separation;
            if (mk2RowPairs.empty() || mk2RowPairs.back()!=pair) {mk2RowPairs.push_back(pair);mk2RowEndSeparation.push_back(gap);}
            else mk2RowEndSeparation.back() = std::min(mk2RowEndSeparation.back(),gap);
        }
        Mk2Continuous(h, &requiredContinuous, &continuousArrivals, &continuousEvents);
        if (mk2ContinuousAwakened) joinAwakened(false);
        stepDuration = dt;
        for (std::size_t i=0; i<state.size(); ++i) {
            auto& s = state[i]; auto& body = bodies[s.slot];
            if (s.dynamic) {
                if (body.motion.Segments().empty()) {
                    // A joint neighbour awakened with the struck body remains
                    // stationary until the next solver boundary.
                    auto rest=body.rigidBody;
                    rest.linearVelocity=rest.angularVelocity=glm::vec3(0);
                    body.motion.Begin(MakeHandle(s.slot).id,rest,h,true);
                }
                if (segments[i].empty() && begin>0) {
                    RigidMotionSegment rest;
                    rest.owner=MakeHandle(s.slot).id;rest.begin=0;rest.end=begin;
                    rest.inverseMass=body.rigidBody.inverseMass;
                    rest.inverseInertiaLocal=body.rigidBody.inverseInertiaLocal;
                    rest.position=rest.endPosition=body.previousPosition;
                    rest.orientation=rest.endOrientation=body.previousOrientation;
                    segments[i].push_back(rest);
                }
                for (auto segment : body.motion.Segments()) {
                    segment.begin += begin; segment.end += begin;
                    if (segment.anchoredTranslation) segment.translationTime += begin;
                    segments[i].push_back(segment);
                }
            }
            s.dp = glm::vec3(glm::dvec3(body.rigidBody.position)-glm::dvec3(s.x0));
            s.dq = glm::mat3_cast(body.rigidBody.orientation*glm::conjugate(s.q0));
            if (s.kinematic) body.motion = kinematicPaths[s.slot];
        }
        // The next interval uses the newly published collision geometry.
        recollide();
        refreshGeometry();
        for (auto &row : rows) {
            const float gap =
                glm::dot(AnchorGap(state[row.a], state[row.b], row), row.n) + row.separation;
            const float v = RowNormalSpeed(state, row);
            row.arrivesNext = !row.arriving && gap > 0 && v < -kMk2RestitutionThreshold &&
                              gap <= -v * h + kMk2Slop;
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

    // Contact history: step-total physical impulses in the existing cache format.
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
    // Observers (ReadBodyContactSample) read the existing semantics: the impulse each
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
    // Repeated responses at one physical patch are accumulated loads, not
    // additional supports. Coalesce only parallel normals and nearby anchors
    // of the same primitive pair. Preserve every impulse and its pressure
    // centre, without changing the warm-start cache or clipping query output.
    for (const auto& impact : mk2ImpactObservations) {
        if (!(impact.normalImpulse > 0)) continue;
        auto best = observed.end();
        double distance = kWarmStartAnchorDistance;
        for (auto it = observed.begin(); it != observed.end(); ++it) {
            if (!it->key.SameBodies(impact.key) ||
                it->observationRevisionA != impact.observationRevisionA ||
                it->observationRevisionB != impact.observationRevisionB ||
                glm::dot(it->normal, impact.normal) < 1-8*std::numeric_limits<float>::epsilon())
                continue;
            const double candidate = glm::length(it->localAnchorA-impact.localAnchorA);
            if (candidate < distance) { best=it;distance=candidate; }
        }
        if (best == observed.end()) observed.push_back(impact);
        else {
            const double total = double(best->normalImpulse)+impact.normalImpulse;
            best->localAnchorA = (best->localAnchorA*double(best->normalImpulse) +
                impact.localAnchorA*double(impact.normalImpulse))/total;
            best->normalImpulse = float(total);
            best->tangentImpulse += impact.tangentImpulse;
        }
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
// fast body's recorded ledger path is swept with the shared continuous query. Arrivals
// are taken in time order: each is resolved with the exact impulse and both
// bodies' ledgers continue from that instant with their new velocities, so
// the rest of the step is not lost; the re-timed paths are swept again from
// there. A body is re-timed at most kMk2ArrivalsPerBody times per step; past
// that its pose is held for the rest of the step (velocity kept), so no
// unbounded event chain can form.
void PhysicsWorld::Impl::Mk2Continuous(float dt, const std::vector<char>* required,
    std::vector<unsigned>* arrivalCounts, unsigned* eventCount) {
    stepDuration = dt;
    mk2ContinuousAwakened = false;
    impactQueryAnchorTime = 0;
    for (auto slot : aliveSlots) {
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
    }
    std::vector<unsigned> candidates;
    const auto fastReach = [&](unsigned slot) {
        if (required && slot<required->size() && (*required)[slot]) return true;
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
    const auto retime = [&](unsigned slot, double at, bool hold, bool resolved = true,
                            glm::vec3 safeSlide = glm::vec3(0)) {
        auto &body = bodies[slot];
        if (!resolved && !body.motion.Segments().empty())
            body.rigidBody = body.motion.EvaluateAfterEvents(at);
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
        if (kept.empty()) {
            // A sleeping participant had a stationary pre-impact history.
            // Its newly published impulse must not rewrite that history.
            RigidMotionSegment rest;
            rest.owner = MakeHandle(slot).id;
            rest.begin = 0;
            rest.end = at;
            rest.position = rest.endPosition = body.rigidBody.position;
            rest.orientation = rest.endOrientation = body.rigidBody.orientation;
            rest.inverseMass = body.rigidBody.inverseMass;
            rest.inverseInertiaLocal = body.rigidBody.inverseInertiaLocal;
            kept.push_back(rest);
        }
        const auto& oldPath = body.motion.Segments();
        auto oldLeaf = std::upper_bound(oldPath.begin(), oldPath.end(), at,
            [](double time, const auto& leaf) { return time < leaf.begin; });
        const auto linearProjection = oldLeaf == oldPath.begin() ? glm::vec3(0) : (oldLeaf-1)->linearProjection;
        const auto angularProjection = oldLeaf == oldPath.begin() ? glm::vec3(0) : (oldLeaf-1)->angularProjection;
        RigidMotionSegment next = kept.back();
        next.begin = at;
        next.end = dt;
        next.position = next.endPosition;
        next.orientation = next.endOrientation;
        if (hold) {
            next.anchoredTranslation = false;
            next.linearProjection = next.angularProjection = glm::vec3(0);
            next.hasPhysicalLinearVelocity = true;
            next.physicalLinearVelocity = body.rigidBody.linearVelocity;
            next.angularVelocity = body.rigidBody.angularVelocity;
            next.linearVelocity = safeSlide;
            next.driftAngularVelocity = glm::vec3(0);
            next.endPosition = next.position + safeSlide * float(dt-at);
            next.endOrientation = next.orientation;
            kept.push_back(next);
            body.rigidBody.position = next.endPosition;
            body.rigidBody.orientation = next.orientation;
        } else {
            std::vector<RigidMotionSegment> tail;
            const auto physical = body.rigidBody;
            auto drift = physical;
            drift.linearVelocity += linearProjection;
            drift.angularVelocity += angularProjection;
            AppendDynamicRigidMotion(tail, next.owner, drift, at, dt);
            for (auto& leaf : tail) {
                leaf.linearProjection = linearProjection;
                leaf.angularProjection = angularProjection;
                leaf.hasPhysicalLinearVelocity = true;
                leaf.physicalLinearVelocity = physical.linearVelocity;
                leaf.angularVelocity = TransportAngularVelocity(physical.inverseInertiaLocal,
                    physical.orientation, leaf.orientation, physical.angularVelocity);
            }
            const auto endpoint = tail.back().Evaluate(dt);
            kept.insert(kept.end(),tail.begin(),tail.end());
            body.rigidBody.position = endpoint.position; body.rigidBody.orientation = endpoint.orientation;
            body.rigidBody.angularVelocity = endpoint.angularVelocity;
        }
        body.motion.SetRecorded(std::move(kept));
        InvalidateImpactPose(slot);
        if (slot < impactPoseSamples.size())
            impactPoseSamples[slot].tightValid = false;
        if (hold && safeSlide != glm::vec3(0)) {
            // A centered sphere may keep tangential travel at a static plane
            // even when normal arrival work is exhausted. Certify that path
            // with the ordinary CCD query. Moving/concave candidates retain
            // the conservative full hold; this is not a replacement solver.
            double stop = dt;
            const auto found = ImpactCandidates(slot, RemainingImpactBound(slot, at, dt));
            const std::vector<unsigned> nearby(found.begin(), found.end());
            for (auto other : nearby) {
                if (other == slot || !CanRespond(slot,other) || bodies[other].sensor || bodies[other].queryOnly)
                    continue;
                if (bodies[other].HasMotion() || ConcaveShape(bodies[other])) { stop=at; break; }
                const auto a=std::min(slot,other), b=std::max(slot,other);
                for (int pa=0;pa<PrimitiveCount(bodies[a].shape);++pa)
                    for (int pb=0;pb<PrimitiveCount(bodies[b].shape);++pb)
                        stop=std::min(stop,ImpactTime(a,b,pa,pb,at,stop));
            }
            if (stop < dt) {
                auto path=body.motion.Segments();
                auto& slide=path.back();
                slide.end=stop;
                slide.endPosition=slide.EvaluateGeometry(stop).position;
                auto rest=slide;
                rest.begin=stop;rest.end=dt;
                rest.position=rest.endPosition=slide.endPosition;
                rest.linearVelocity=glm::vec3(0);
                path.push_back(rest);
                body.rigidBody.position=rest.endPosition;
                body.motion.SetRecorded(std::move(path));
                InvalidateImpactPose(slot);
                if (slot<impactPoseSamples.size()) impactPoseSamples[slot].tightValid=false;
            }
        }
    };
    struct Mover {
        unsigned slot;
        double from = 0;     // swept from here
        bool firstSweep = true; // rows still describe this path
        unsigned arrivals = 0;
        double hit = 0;
        unsigned other = 0;
        int partA = 0, partB = 0;
        bool found = false;
        bool held = false;
        double lastArrival = -1;
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
            if (o.sensor || o.queryOnly || (!movable(slot) && !movable(other)))
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
                    double hit = limit+1;
                    if (touching) {
                        const auto contact = PairAt(a,b,pa,pb,m.from,0);
                        const auto aa = PoseAt(a,m.from), bb = PoseAt(b,m.from);
                        for (int k=0;k<contact.count;++k) {
                            const auto& c=contact.points[k];
                            const auto arm=[&](const RigidBody& body,bool first) {
                                if (!c.hasLocalAnchors) return glm::dvec3(c.point)-glm::dvec3(body.position);
                                const auto local=first?c.localAnchorA:c.localAnchorB;
                                return (first?c.anchorAInWorldFrame:c.anchorBInWorldFrame)?local:ContactRotation(body.orientation)*local;
                            };
                            const auto va=glm::dvec3(aa.linearVelocity)+glm::cross(glm::dvec3(aa.angularVelocity),arm(aa,true));
                            const auto vb=glm::dvec3(bb.linearVelocity)+glm::cross(glm::dvec3(bb.angularVelocity),arm(bb,false));
                            if (glm::dot(va-vb,glm::dvec3(c.normal)) < -kMk2ImpactApproach) hit=m.from;
                        }
                    }
                    if (hit != m.from) hit = ImpactTime(a, b, pa, pb, m.from, limit);
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
    const auto resweepChanged = [&](unsigned first, unsigned second, double at) {
        const bool firstChanged = movable(first), secondChanged = movable(second);
        const auto firstBound = RemainingImpactBound(first, at, dt);
        const auto secondBound = RemainingImpactBound(second, at, dt);
        for (auto& m : movers) {
            if (m.held) continue;
            const auto from = std::max(m.from, at);
            // A prior no-impact result is also stale when a changed trajectory
            // can now meet it. Disjoint conservative bounds retain that result.
            const auto bound = RemainingImpactBound(m.slot, from, dt);
            const bool affected =
                (firstChanged && (m.slot==first || (m.found && m.other==first) || bound.Overlaps(firstBound))) ||
                (secondChanged && (m.slot==second || (m.found && m.other==second) || bound.Overlaps(secondBound)));
            if (!affected) continue;
            m.from = from;
            m.firstSweep = false;
            sweep(m);
        }
    };
    // Prescribed and dynamic trajectories share one chronological queue.
    // A kinematic body retains its authored path; its dynamic counterpart is
    // retimed by the same atomic publication as every other impact.
    for (const auto slot : aliveSlots) {
        const auto& body = bodies[slot];
        const bool prescribed = body.isKinematic && !body.simulationSuspended &&
            !body.sensor && !body.queryOnly && body.HasMotion();
        if ((!movable(slot) && !prescribed) || body.sleeping || !fastReach(slot))
            continue;
        movers.push_back({slot, 0, true, arrivalCounts ? (*arrivalCounts)[slot] : 0u});
        sweep(movers.back());
    }
    for (unsigned events = eventCount ? *eventCount : 0; events < kMk2ArrivalsPerStep; ++events) {
        Mover *next = nullptr;
        for (auto &m : movers)
            if (m.found && (!next || m.hit < next->hit))
                next = &m;
        if (!next)
            break;
        if (eventCount) ++*eventCount;
        const unsigned slot = next->slot, other = next->other;
        const double at = next->hit;
        const int partA=next->partA, partB=next->partB;
        ++stats.impactEvents;
        // Without an exact response the path would continue into the contact.
        const bool resolved = Mk2ResolveArrival(slot, other, partA, partB, at);
        // Publish the entire response before querying either participant. A
        // sequential sweep could otherwise read the other body's old trajectory.
        for (const auto changed : {slot, other}) {
            if (!movable(changed)) continue;
            if (bodies[changed].sleeping) Wake(changed);
            auto* m = moverOf(changed);
            if (!m) {
                movers.push_back({changed, at, false, arrivalCounts ? (*arrivalCounts)[changed] : 0u});
                m = &movers.back();
            }
            const bool newInstant = m->lastArrival != at;
            const bool hold = !resolved || (newInstant && m->arrivals >= kMk2ArrivalsPerBody);
            if (hold) {
                ++stats.impactSafetyFallback;
                if (!resolved) ++stats.impactResponseHolds;
                else {
                    ++stats.impactEventCapFallback;
                    ++stats.impactBodyCapHolds;
                }
            } else if (newInstant) {
                ++m->arrivals;
                if (arrivalCounts) (*arrivalCounts)[changed] = m->arrivals;
            }
            m->lastArrival = at;
            glm::vec3 safeSlide(0);
            const unsigned boundary=changed==slot ? other : slot;
            if (hold && resolved && bodies[changed].shape.type==ShapeType::Sphere &&
                bodies[changed].shape.pivotOffset==glm::vec3(0) && !bodies[boundary].HasMotion() &&
                bodies[boundary].shape.type==ShapeType::Box) {
                const auto contact=PairAt(slot,other,partA,partB,at,1e-3f);
                if (contact.count && !contact.uncertain) {
                    const auto n=glm::normalize(contact.points[0].normal);
                    bool planar=true;
                    for (int k=1;k<contact.count;++k)
                        planar=planar && glm::dot(n,contact.points[k].normal)>.99999f;
                    if (planar) {
                        const auto velocity=bodies[changed].rigidBody.linearVelocity;
                        safeSlide=velocity-n*glm::dot(n,velocity);
                    }
                }
            }
            retime(changed, at, hold, resolved, safeSlide);
            m->held = hold;
            m->from = at;
            m->firstSweep = false;
            m->found = false;
        }
        if (!resolved)
            for (auto& m : movers)
                if ((m.slot == slot || m.slot == other) && bodies[m.slot].isKinematic)
                    m.held = true; // The prescribed path itself is never modified.
        resweepChanged(slot, other, at);
    }
    // Terminal holds are also chronological and invalidate affected queries.
    // Each iteration retires at least its primary mover, so this tail is
    // bounded by mover count without replaying a response into past history.
    for (std::size_t retired=0; retired<movers.size(); ++retired) {
        auto next=movers.end();
        for (auto it=movers.begin(); it!=movers.end(); ++it)
            if (it->found && (next==movers.end() || it->hit<next->hit)) next=it;
        if (next==movers.end()) break;
        const auto slot=next->slot,other=next->other;
        const auto at=next->hit;
        const int partA=next->partA,partB=next->partB;
        ++stats.impactEvents;
        const bool resolved=Mk2ResolveArrival(slot,other,partA,partB,at);
        for (const auto changed : {slot,other}) {
            if (movable(changed)) {
                ++stats.impactSafetyFallback;
                ++stats.impactEventCapFallback;
                ++stats.impactStepCapHolds;
                retime(changed,at,true,resolved);
            }
            if (auto* mover=moverOf(changed)) { mover->held=true;mover->found=false; }
        }
        resweepChanged(slot,other,at);
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
    // An asleep body is an ordinary dynamic participant at an actual impact.
    for (const auto slot : {a, b})
        if (bodies[slot].isDynamic && !bodies[slot].rigidBody.IsStatic() &&
            !bodies[slot].simulationSuspended && bodies[slot].sleeping) {
            Wake(slot);
            mk2ContinuousAwakened = true;
        }
    // Private first-touch copies: actual velocities, poses at the arrival.
    RigidBody bodyA = PoseAt(a, hit), bodyB = PoseAt(b, hit);
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
    std::vector<SolverBody> participants(2);
    for (unsigned i=0;i<2;++i) {
        auto& solver = participants[i];
        solver.rb = i == 0 ? &bodyA : &bodyB;
        solver.dynamic = movable(i == 0 ? a : b);
        solver.invMass = solver.rb->inverseMass;
        solver.invI = solver.rb->InverseInertiaWorld();
    }
    std::vector<Row> response;
    float approach = 0;
    for (const auto& contact : rows) {
        Row row{}; row.a=0; row.b=1; row.arriving=true;
        const auto& c=contact.geometry;
        const auto arm=[&](const RigidBody& rb,bool first) {
            if (!c.hasLocalAnchors) return glm::vec3(glm::dvec3(c.point)-glm::dvec3(rb.position));
            const auto local=first?c.localAnchorA:c.localAnchorB;
            return glm::vec3((first?c.anchorAInWorldFrame:c.anchorBInWorldFrame)?local:ContactRotation(rb.orientation)*local);
        };
        row.rA=row.rA0=arm(bodyA,true); row.rB=row.rB0=arm(bodyB,false);
        row.n=c.normal;row.t1=Orthogonal(row.n);row.t2=glm::cross(row.n,row.t1);
        row.friction=friction;row.restitution=contact.restitution;
        PrepareRow(participants[0],participants[1],row);
        approach=std::min(approach,RowNormalSpeed(participants,row));
        response.push_back(row);
    }
    Block patch;patch.first=0;patch.count=response.size();
    patch.usable=patch.count>=2 && patch.count<=4;
    for(const auto& row:response)patch.usable=patch.usable && glm::dot(row.n,response.front().n)>.99999f;
    PrepareBlock(participants,response,patch);
    const auto kinetic=[&]{double result=0;for(const auto& part:participants)if(part.dynamic){
        const glm::dvec3 v(part.rb->linearVelocity),w(part.rb->angularVelocity);
        result += .5*glm::dot(v,v)/part.invMass + .5*glm::dot(w,glm::inverse(glm::dmat3(part.invI))*w);
    }return result;};
    const auto savedA=bodyA,savedB=bodyB;
    const double incoming=kinetic();
    const float restitution=approach < -kMk2RestitutionThreshold ? std::max(bodies[a].restitution,bodies[b].restitution) : 0;
    const auto solve=[&](bool frictionless) {
        bodyA=savedA;bodyB=savedB;
        for(auto& row:response){row.normalImpulse=row.tangent1=row.tangent2=0;row.friction=frictionless?0:friction;}
        for(int iteration=0;iteration<kMk2ImpactSweeps;++iteration)
            SolveBlock(participants,response,patch,1,false,true);
        for(auto& row:response){const float extra=restitution*row.normalImpulse;
            ApplyAxis(participants[0],participants[1],row.n,row.wnA,row.wnB,extra);
            row.normalImpulse += extra;
        }
        double boundaryWork=0;
        for(const auto& row:response){const glm::vec3 impulse=row.n*row.normalImpulse+row.t1*row.tangent1+row.t2*row.tangent2;
            if(!participants[0].dynamic)boundaryWork -= glm::dot(impulse,bodyA.linearVelocity+glm::cross(bodyA.angularVelocity,row.rA));
            if(!participants[1].dynamic)boundaryWork += glm::dot(impulse,bodyB.linearVelocity+glm::cross(bodyB.angularVelocity,row.rB));
        }
        return kinetic() <= incoming+boundaryWork+64*std::numeric_limits<float>::epsilon()*std::max({1.,incoming,std::abs(boundaryWork)});
    };
    if(!solve(false) && !solve(true)) {
        ++stats.impactEnergyRejections;
        return false;
    }
    ++stats.impactResponsesApplied;
    for (auto [copy, slot] : {std::pair<RigidBody *, unsigned>{&bodyA, a}, {&bodyB, b}})
        if (movable(slot)) {
            bodies[slot].rigidBody.position = copy->position;
            bodies[slot].rigidBody.orientation = copy->orientation;
            bodies[slot].rigidBody.linearVelocity = copy->linearVelocity;
            bodies[slot].rigidBody.angularVelocity = copy->angularVelocity;
        }
    // The arrival is a touch of this step, as for MK1's impact events.
    for (std::size_t i=0;i<rows.size();++i) {
        const auto& row=rows[i]; const auto& impulse=response[i];
        Observe(a,b,row.geometry,impulse.normalImpulse);
        const auto handleA=MakeHandle(a).id, handleB=MakeHandle(b).id;
        const auto pair=std::minmax(handleA,handleB);
        stepTouches[pair].impulseAvailable=true;
        const glm::dvec3 anchor=glm::transpose(ContactRotation(bodyA.orientation))*glm::dvec3(impulse.rA);
        CachedContact observation;
        observation.key={a,b,bodies[a].generation,bodies[b].generation,pa,pb};
        observation.localAnchorA=anchor;observation.normal=impulse.n;
        observation.normalImpulse=impulse.normalImpulse;
        observation.tangentImpulse=impulse.t1*impulse.tangent1+impulse.t2*impulse.tangent2;
        observation.observationRevisionA=bodies[a].contactObservationRevision;
        observation.observationRevisionB=bodies[b].contactObservationRevision;
        mk2ImpactObservations.push_back(observation);
    }
    return true;
}
