#include "CutFastDirectedTransport.h"
#include "CutFastResidual.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
constexpr double infinity = std::numeric_limits<double>::infinity();
}

bool CutFastDirectedTransport::Build(const std::vector<double>& capacity,
    const std::vector<CutDirectedEdge>& edges, const std::vector<Real>& exterior,
    std::string& error) {
    CutFastDirectedTransport next;
    if (!next.reference_.Build(capacity, edges, exterior, error)) return false;
    const auto n = capacity.size();
    next.row_.resize(n + 1);
    next.diagonal_ = next.reference_.ground_;
    next.reciprocal_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        next.row_[i] = next.entries_.size();
        if (next.reference_.removed_[i]) continue;
        next.nodes_.push_back(i);
        for (const auto& [j, q] : next.reference_.out_[i]) {
            (void)j;
            next.diagonal_[i] = next.diagonal_[i] + q;
        }
        next.reciprocal_[i] = 1. / next.diagonal_[i].Value();
        for (const auto& [j, q] : next.reference_.in_[i])
            next.entries_.push_back({j, q.Value()});
    }
    next.row_[n] = next.entries_.size();
    *this = std::move(next);
    return true;
}

bool CutFastDirectedTransport::Correction(const std::vector<Real>& source,
    std::vector<Real>& output) {
    auto rhs = source;
    for (const auto& step : reference_.steps_)
        for (const auto& [j, q] : step.out)
            rhs[j] = rhs[j] + (q / step.diagonal) * rhs[step.node];
    const auto n = rhs.size();
    std::vector<double> delta(n);
    double magnitude = 0.;
    for (auto i : nodes_) magnitude += std::abs(rhs[i].Value());
    bool solved = magnitude == 0.;
    for (int sweep = 0; !solved && sweep < 20000; ++sweep) {
        ++stats_.sweeps;
        for (auto i : nodes_) {
            double value = rhs[i].Value();
            for (auto j = row_[i]; j < row_[i + 1]; ++j)
                value += entries_[j].value * delta[entries_[j].column];
            delta[i] = value * reciprocal_[i];
        }
        double error = 0.;
        for (auto i : nodes_) {
            double value = rhs[i].Value() - diagonal_[i].Value() * delta[i];
            for (auto j = row_[i]; j < row_[i + 1]; ++j)
                value += entries_[j].value * delta[entries_[j].column];
            error += std::abs(value);
        }
        solved = std::isfinite(error) && error <= magnitude * 1e-5;
    }
    if (!solved) return false;
    output.assign(delta.begin(), delta.end());
    for (std::size_t j = reference_.steps_.size(); j-- > 0;) {
        const auto& step = reference_.steps_[j];
        Real value = rhs[step.node];
        for (const auto& [k, q] : step.in) value = value + q * output[k];
        output[step.node] = value / step.diagonal;
    }
    return true;
}

bool CutFastDirectedTransport::SolveChecked(const std::vector<Real>& rhs,
    double budget, std::vector<Real>& output, std::string& error, double quantityBudget) {
    const auto n = reference_.capacity_.size();
    if (rhs.size() != n || !(budget > 0. && std::isfinite(budget))) return false;
    for (auto x : rhs)
        if (!std::isfinite(x.hi) || !std::isfinite(x.lo)) return false;
    if (lower_.empty()) {
        std::vector<Real> capacity(reference_.capacity_.begin(), reference_.capacity_.end());
        std::vector<Real> barrier;
        double sum = 0.;
        for (double c : reference_.capacity_) sum += c;
        // Build a positive supersolution with the fast corrections. If its
        // original-row enclosure cannot prove positivity, use the reference.
        std::vector<CutResidualInterval> bounds;
        std::vector<double> lower(n);
        double maximum = 0.;
        bool positive = false;
        std::vector<Real> residual = capacity;
        barrier.assign(n, Real());
        for (int iteration = 0; iteration < 6 && !positive; ++iteration) {
            std::vector<Real> correction;
            if (!Correction(residual, correction)) break;
            for (std::size_t i = 0; i < n; ++i) barrier[i] = barrier[i] + correction[i];
            if (!BoundCutDirectedResidualFiltered(reference_.capacity_, reference_.edges_,
                reference_.exterior_, std::vector<Real>(n), barrier, std::vector<double>(n), bounds, error, true)) break;
            positive = true;
            maximum = 0.;
            for (std::size_t i = 0; i < n; ++i) {
                lower[i] = -bounds[i].hi;
                positive = positive && lower[i] > 0. && barrier[i].Value() > 0.;
                maximum = std::max(maximum, std::nextafter(std::abs(barrier[i].Value()), infinity));
            }
            if (!positive) {
                residual = capacity;
                for (std::size_t i = 0; i < n; ++i)
                    residual[i] = residual[i] - (Real(reference_.capacity_[i]) + reference_.exterior_[i]) * barrier[i];
                for (const auto& edge : reference_.edges_) {
                    auto q = edge.amount * barrier[edge.from];
                    residual[edge.from] = residual[edge.from] - q;
                    residual[edge.to] = residual[edge.to] + q;
                }
            }
        }
        if (!positive) {
            if (!reference_.Solve(capacity, sum * 1e-14, barrier, error)) return false;
            if (!BoundCutDirectedResidualFiltered(reference_.capacity_, reference_.edges_,
                reference_.exterior_, std::vector<Real>(n), barrier, std::vector<double>(n), bounds, error, true)) return false;
            maximum = 0.;
            for (std::size_t i = 0; i < n; ++i) {
                lower[i] = -bounds[i].hi;
                if (!(lower[i] > 0. && barrier[i].Value() > 0.)) return false;
                maximum = std::max(maximum, std::nextafter(std::abs(barrier[i].Value()), infinity));
            }
        }
        lower_ = std::move(lower);
        barrierMaximum_ = maximum;
    }
    std::vector<double> targets(n);
    for(std::size_t i=0;i<n;++i)targets[i]=std::nextafter(.5*budget*(lower_[i]/barrierMaximum_),0.);
    std::vector<Real> result(n);
    const double minimum = *std::min_element(reference_.capacity_.begin(), reference_.capacity_.end());
    for (int iteration = 0; iteration < 12; ++iteration) {
        std::vector<Real> residual = rhs;
        for (std::size_t i = 0; i < n; ++i)
            residual[i] = residual[i] - (Real(reference_.capacity_[i]) + reference_.exterior_[i]) * result[i];
        for (const auto& edge : reference_.edges_) {
            Real q = edge.amount * result[edge.from];
            residual[edge.from] = residual[edge.from] - q;
            residual[edge.to] = residual[edge.to] + q;
        }
        // A cheap residual only schedules the expensive certificate. It never
        // admits output. Always audit on the last refinement to avoid turning
        // a pessimistic floating estimate into a numerical rejection.
        double roughRatio = 0., roughTotal = 0.;
        for (std::size_t i = 0; i < n; ++i) {
            const double r = std::abs(residual[i].Value());
            roughTotal += r;
            roughRatio = std::max(roughRatio, r / lower_[i]);
        }
        bool zero = std::all_of(rhs.begin(), rhs.end(), [](Real x) { return x.hi == 0. && x.lo == 0.; });
        const bool tryAudit = iteration == 11 || zero || (iteration > 0 &&
            std::min(roughRatio * barrierMaximum_, roughTotal / minimum) <= budget);
        if (tryAudit) {
            std::vector<CutResidualInterval> bounds;
            if (!BoundCutDirectedResidualFiltered(reference_.capacity_, reference_.edges_,
                reference_.exterior_, rhs, result, targets, bounds, error)) return false;
            double ratio = 0., total = 0.;
            for (std::size_t i = 0; i < n; ++i) {
                double r = std::max(std::abs(bounds[i].lo), std::abs(bounds[i].hi));
                total = std::nextafter(total + r, infinity);
                ratio = std::max(ratio, std::nextafter(r / lower_[i], infinity));
            }
            bool certified = std::nextafter(ratio * barrierMaximum_, infinity) <= budget ||
                             std::nextafter(total / minimum, infinity) <= budget;
            Real quantityDefect;
            for(std::size_t i=0;i<n;++i)quantityDefect=quantityDefect+
                (Real(reference_.capacity_[i])+reference_.exterior_[i])*result[i]-rhs[i];
            certified=certified&&std::isfinite(quantityDefect.Value())&&std::abs(quantityDefect.Value())<=quantityBudget;
            if (certified && (iteration > 0 || zero)) {
                output = std::move(result);
                error.clear();
                return true;
            }
        }
        std::vector<Real> correction;
        ++stats_.corrections;
        if (!Correction(residual, correction)) return false;
        for (std::size_t i = 0; i < n; ++i) result[i] = result[i] + correction[i];
    }
    return false;
}

bool CutFastDirectedTransport::SolveFraction(const std::vector<Real>& rhs,
    double budget, std::vector<Real>& output, std::string& error, double quantityBudget) {
    ++stats_.solves;
    if (!(quantityBudget > 0.)) { error="invalid transport quantity budget"; return false; }
    if (SolveChecked(rhs, budget, output, error, quantityBudget)) return true;
    ++stats_.fallbacks;
    std::vector<Real> candidate;
    if(!reference_.SolveFraction(rhs,budget,candidate,error))return false;
    Real defect;
    for(std::size_t i=0;i<candidate.size();++i)defect=defect+(Real(reference_.capacity_[i])+reference_.exterior_[i])*candidate[i]-rhs[i];
    if(!std::isfinite(defect.Value())||std::abs(defect.Value())>quantityBudget){error="reference fallback exceeds requested quantity budget";return false;}
    output=std::move(candidate);return true;
}
}
