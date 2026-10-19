#pragma once

// Offline numerical investigation, never linked into the running engine.
// A = J_n M^-1 J_n^T, with the event's original physical masses/inertias.
// Tangential impulses are frozen while this normal-only QP is solved.
#include <numeric>

namespace normal_schur {
struct System {
    struct Body {
        double inverseMass;
        glm::dmat3 inverseInertia;
    };
    struct Row {
        std::size_t a, b;
        glm::dvec3 normal, angularA, angularB;
    };
    std::vector<Body> bodies;
    std::vector<Row> rows;
    std::vector<double> diagonal;
    mutable std::vector<glm::dvec3> linear, angular;
    std::size_t multiplies = 0;

    explicit System(const std::vector<ContactConstraint>& constraints) {
        std::unordered_map<RigidBody*, std::size_t> indices;
        auto index = [&](RigidBody* body, const glm::mat3& invI) {
            const auto found = indices.find(body);
            if (found != indices.end()) return found->second;
            const auto id = bodies.size();
            indices.emplace(body, id);
            bodies.push_back({body->inverseMass, glm::dmat3(invI)});
            return id;
        };
        for (const auto& c : constraints) {
            const auto a = index(c.bodyA, c.inverseInertiaA);
            const auto b = index(c.bodyB, c.inverseInertiaB);
            const glm::dvec3 n(c.normal);
            const auto ja = glm::cross(glm::dvec3(c.velocityOffsetA), n);
            const auto jb = glm::cross(glm::dvec3(c.velocityOffsetB), n);
            rows.push_back({a, b, n, ja, jb});
            diagonal.push_back((bodies[a].inverseMass+bodies[b].inverseMass)*glm::dot(n,n) +
                glm::dot(ja,bodies[a].inverseInertia*ja) + glm::dot(jb,bodies[b].inverseInertia*jb));
        }
        linear.resize(bodies.size());
        angular.resize(bodies.size());
    }
    std::vector<double> Multiply(const std::vector<double>& x) {
        ++multiplies;
        std::fill(linear.begin(), linear.end(), glm::dvec3(0));
        std::fill(angular.begin(), angular.end(), glm::dvec3(0));
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            const auto impulse = r.normal*x[i];
            linear[r.a] += impulse*bodies[r.a].inverseMass;
            linear[r.b] -= impulse*bodies[r.b].inverseMass;
            angular[r.a] += bodies[r.a].inverseInertia*r.angularA*x[i];
            angular[r.b] -= bodies[r.b].inverseInertia*r.angularB*x[i];
        }
        std::vector<double> result(rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            result[i] = glm::dot(r.normal, linear[r.a]-linear[r.b]) +
                glm::dot(r.angularA, angular[r.a]) - glm::dot(r.angularB, angular[r.b]);
        }
        return result;
    }
};
inline double Dot(const std::vector<double>& a, const std::vector<double>& b) {
    double value = 0;
    for (std::size_t i = 0; i < a.size(); ++i) value += a[i]*b[i];
    return value;
}
inline std::vector<double> Gradient(System& a, const std::vector<double>& x,
                                    const std::vector<double>& b) {
    auto g = a.Multiply(x);
    for (std::size_t i = 0; i < g.size(); ++i) g[i] += b[i];
    return g;
}
inline double ProjectedResidual(const std::vector<double>& x, const std::vector<double>& g) {
    double result = 0;
    for (std::size_t i = 0; i < x.size(); ++i)
        result = std::max(result, x[i] > 0 ? std::abs(g[i]) : std::max(-g[i],0.0));
    return result;
}
inline double Objective(const std::vector<double>& x, const std::vector<double>& g,
                         const std::vector<double>& b) {
    return .5*(Dot(x,g)+Dot(x,b)); // g=A*x+b
}
struct SolveResult {
    std::vector<double> impulse;
    unsigned outer = 0, cgIterations = 0, blocked = 0, backtracks = 0;
    double projectedResidual = 0, objectiveBefore = 0, objectiveAfter = 0;
    bool converged = false;
};
inline SolveResult Solve(System& a, const std::vector<double>& b,
                         const std::vector<double>& initial,
                         unsigned outerLimit = 64, unsigned cgLimit = 256) {
    SolveResult result;
    auto x = initial;
    const auto size = x.size();
    auto g = Gradient(a,x,b);
    result.objectiveBefore = Objective(x,g,b);
    double spectral = 1;
    const double tolerance = 1e-8; // Offline diagnostic target, no engine threshold.
    for (unsigned outer = 0; outer < outerLimit; ++outer) {
        result.outer = outer;
        result.projectedResidual = ProjectedResidual(x,g);
        if (result.projectedResidual < tolerance) { result.converged = true; break; }

        // Projected gradient activates violated zero-load contacts. Backtracking
        // uses the actual quadratic, rather than an assumed graph degree bound.
        const double oldObjective = Objective(x,g,b);
        std::vector<double> projected(size), delta(size);
        for (unsigned retry = 0; retry < 32; ++retry) {
            for (std::size_t i = 0; i < size; ++i) {
                projected[i] = std::max(0.0,x[i]-g[i]/(spectral*a.diagonal[i]));
                delta[i] = projected[i]-x[i];
            }
            const auto ad = a.Multiply(delta);
            if (Dot(delta,g)+.5*Dot(delta,ad) <= 1e-20) break;
            spectral *= 2;
            ++result.backtracks;
        }
        x = projected;
        g = Gradient(a,x,b);

        // Newton step on the currently positive active set. CG applies the
        // physical Schur operator without ever constructing a dense matrix.
        std::vector<unsigned char> free(size);
        std::vector<double> d(size,0), residual(size), z(size), p(size);
        double rz = 0;
        for (std::size_t i = 0; i < size; ++i) {
            free[i] = x[i] > 0;
            residual[i] = free[i] ? -g[i] : 0;
            z[i] = residual[i]/a.diagonal[i];
            p[i] = z[i];
            rz += residual[i]*z[i];
        }
        const double rzStart = rz;
        for (unsigned iteration = 0; iteration < cgLimit && rz > std::max(1e-28,rzStart*1e-16); ++iteration) {
            auto ap = a.Multiply(p);
            for (std::size_t i = 0; i < size; ++i) if (!free[i]) ap[i] = 0;
            const double curvature = Dot(p,ap);
            if (!(curvature > 1e-30) || !std::isfinite(curvature)) break;
            const double alpha = rz/curvature;
            for (std::size_t i = 0; i < size; ++i) {
                d[i] += alpha*p[i];
                residual[i] -= alpha*ap[i];
                z[i] = residual[i]/a.diagonal[i];
            }
            const double nextRz = Dot(residual,z);
            const double beta = nextRz/rz;
            for (std::size_t i = 0; i < size; ++i) p[i] = z[i]+beta*p[i];
            rz = nextRz;
            ++result.cgIterations;
        }
        const double derivative = Dot(g,d);
        if (derivative < 0 && std::isfinite(derivative)) {
            const auto ad = a.Multiply(d);
            const double curvature = Dot(d,ad);
            double alpha = 1;
            for (std::size_t i = 0; i < size; ++i)
                if (d[i] < 0 && -x[i]/d[i] < alpha) {
                    alpha = -x[i]/d[i];
                    ++result.blocked;
                }
            if (curvature > 0) alpha = std::min(alpha,-derivative/curvature);
            // A tiny inward rounding protects feasibility; exact positive rows
            // that hit a bound are explicitly zeroed below.
            for (std::size_t i = 0; i < size; ++i) x[i] = std::max(0.0,x[i]+alpha*d[i]);
            g = Gradient(a,x,b);
        }
        const double objective = Objective(x,g,b);
        if (!std::isfinite(objective) || objective > oldObjective+1e-12*std::max(1.0,std::abs(oldObjective)))
            throw std::runtime_error("offline active-set objective did not decrease");
        spectral = std::max(1.0,spectral*.9);
    }
    result.impulse = std::move(x);
    result.projectedResidual = ProjectedResidual(result.impulse,g);
    result.objectiveAfter = Objective(result.impulse,g,b);
    return result;
}

inline SolveResult SolveFista(System& a, const std::vector<double>& b,
                              const std::vector<double>& initial,
                              unsigned iterations = 1000) {
    SolveResult result;
    auto x = initial, y = initial;
    const auto size = x.size();
    auto gx = Gradient(a,x,b);
    double objective = Objective(x,gx,b);
    result.objectiveBefore = objective;
    double lip = 4, momentum = 1;
    for (unsigned iteration = 0; iteration < iterations; ++iteration) {
        result.outer = iteration;
        auto gy = Gradient(a,y,b);
        std::vector<double> candidate(size), delta(size), gc(size);
        double candidateObjective = 0;
        for (unsigned retry = 0; retry < 32; ++retry) {
            double norm = 0;
            for (std::size_t i=0; i<size; ++i) {
                candidate[i] = std::max(0.0,y[i]-gy[i]/(lip*a.diagonal[i]));
                delta[i] = candidate[i]-y[i];
                norm += a.diagonal[i]*delta[i]*delta[i];
            }
            const auto ad = a.Multiply(delta);
            const auto change = Dot(gy,delta)+.5*Dot(delta,ad);
            if (.5*Dot(delta,ad) <= .5*lip*norm+1e-24) {
                candidateObjective = Objective(y,gy,b)+change;
                for (std::size_t i=0; i<size; ++i) gc[i] = gy[i]+ad[i];
                break;
            }
            lip *= 2;
            ++result.backtracks;
            if (retry == 31) throw std::runtime_error("offline FISTA quadratic backtracking failed");
        }
        if (candidateObjective > objective+1e-20) {
            y = x;
            momentum = 1;
            ++result.blocked; // Explicit objective-based restart, no physics retuning.
            continue;
        }
        result.projectedResidual = ProjectedResidual(candidate,gc);
        const double nextMomentum = .5*(1+std::sqrt(1+4*momentum*momentum));
        const double scale = (momentum-1)/nextMomentum;
        for (std::size_t i=0; i<size; ++i) y[i] = candidate[i]+scale*(candidate[i]-x[i]);
        x = std::move(candidate);
        gx = std::move(gc);
        momentum = nextMomentum;
        objective = candidateObjective;
        if (result.projectedResidual < 1e-8) { result.converged = true; break; }
    }
    result.impulse = std::move(x);
    result.projectedResidual = ProjectedResidual(result.impulse,gx);
    result.objectiveAfter = objective;
    return result;
}
} // namespace normal_schur
