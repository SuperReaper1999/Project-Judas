#include "CutPressurePreconditioner.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
namespace judas::volume_grid {
namespace { using Real = CutCompensated; }
bool CutPressurePreconditioner::Build(const std::vector<double>& storage,
    const std::vector<CutHydroacousticGraphLink>& links,
    const std::vector<bool>& condense, std::string& error) {
    const auto n = storage.size();
    if (n == 0 || condense.size() != n) { error = "invalid pressure accelerator dimensions"; return false; }
    CutPressurePreconditioner next; next.size_ = n;
    std::vector<std::map<std::size_t, Real>> adjacency(n);
    std::vector<Real> ground(n); std::vector<bool> removed(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (!(storage[i] >= 0. && std::isfinite(storage[i]))) { error = "invalid accelerator storage"; return false; }
        ground[i] = storage[i];
    }
    for (const auto& edge : links) {
        if (edge.left >= n || edge.right >= n || edge.left == edge.right ||
            !(edge.conductance > 0. && std::isfinite(edge.conductance))) {
            error = "invalid accelerator edge"; return false;
        }
        adjacency[edge.left][edge.right] = adjacency[edge.left][edge.right] + edge.conductance;
        adjacency[edge.right][edge.left] = adjacency[edge.right][edge.left] + edge.conductance;
    }
    for (std::size_t i = 0; i < n; ++i) if (condense[i] && adjacency[i].size() <= 16) {
        Elimination step{i, ground[i], {adjacency[i].begin(), adjacency[i].end()}};
        for (const auto& [j, q] : step.edges) { (void)j; step.diagonal = step.diagonal + q; }
        if (!(step.diagonal.Value() > 0.)) continue;
        for (const auto& [j, q] : step.edges) {
            ground[j] = ground[j] + q * (ground[i] / step.diagonal);
            adjacency[j].erase(i);
        }
        for (std::size_t a = 0; a < step.edges.size(); ++a)
            for (std::size_t b = a + 1; b < step.edges.size(); ++b) {
                auto [j, qj] = step.edges[a]; auto [k, qk] = step.edges[b];
                Real q = qj * (qk / step.diagonal);
                adjacency[j][k] = adjacency[j][k] + q;
                adjacency[k][j] = adjacency[k][j] + q;
            }
        adjacency[i].clear(); removed[i] = true; next.steps_.push_back(std::move(step));
    }
    std::vector<std::size_t> index(n, n);
    for (std::size_t i = 0; i < n; ++i) if (!removed[i]) {
        index[i] = next.original_.size(); next.original_.push_back(i);
    }
    next.storage_.resize(next.original_.size());
    for (std::size_t i = 0; i < next.original_.size(); ++i) {
        const auto node = next.original_[i]; next.storage_[i] = ground[node].Value();
        for (const auto& [j, q] : adjacency[node]) if (j > node)
            next.links_.push_back({i, index[j], q.Value()});
    }
    std::vector<std::size_t> component(next.original_.size());
    std::iota(component.begin(), component.end(), 0);
    auto root = [&](std::size_t i) { while (component[i] != i) { component[i] = component[component[i]]; i = component[i]; } return i; };
    for (const auto& edge : next.links_) { auto a = root(edge.left), b = root(edge.right); if (a != b) component[b] = a; }
    std::vector<bool> anchored(component.size()); auto diagonal = next.storage_;
    for (const auto& edge : next.links_) { diagonal[edge.left] += edge.conductance; diagonal[edge.right] += edge.conductance; }
    for (std::size_t i = 0; i < component.size(); ++i) if (next.storage_[i] > 0.) anchored[root(i)] = true;
    // Only the accelerator gets a gauge. The physical operator and final
    // volume equations remain unanchored and independently checked.
    for (std::size_t i = 0; i < component.size(); ++i) if (!anchored[root(i)]) {
        next.storage_[i] = diagonal[i] > 0. ? diagonal[i] : 1.; anchored[root(i)] = true;
    }
    *this = std::move(next); error.clear(); return true;
}

bool CutPressurePreconditioner::Apply(const std::vector<double>& rhs,
    std::vector<double>& output, std::string& error) const {
    if (size_ == 0 || rhs.size() != size_) { error = "invalid pressure accelerator RHS"; return false; }
    std::vector<Real> residual(rhs.begin(), rhs.end());
    for (double x : rhs) if (!std::isfinite(x)) { error = "nonfinite pressure accelerator RHS"; return false; }
    for (const auto& step : steps_)
        for (const auto& [j, q] : step.edges)
            residual[j] = residual[j] + (q / step.diagonal) * residual[step.node];
    std::vector<double> source(original_.size()); double magnitude = 0.;
    for (std::size_t i = 0; i < original_.size(); ++i) {
        source[i] = residual[original_[i]].Value(); magnitude = std::max(magnitude, std::abs(source[i]));
    }
    CutHydroacousticGraphResult reduced;
    if (!source.empty() && !SolveCutHydroacousticGraph(storage_, links_, source,
        std::max(magnitude * 1e-4, 1e-300), reduced, error)) return false;
    std::vector<Real> result(size_);
    for (std::size_t i = 0; i < original_.size(); ++i) result[original_[i]] = reduced.pressure[i];
    for (std::size_t j = steps_.size(); j-- > 0;) {
        const auto& step = steps_[j]; Real value = residual[step.node];
        for (const auto& [k, q] : step.edges) value = value + q * result[k];
        result[step.node] = value / step.diagonal;
    }
    std::vector<double> next(size_);
    for (std::size_t i = 0; i < size_; ++i) {
        next[i] = result[i].Value();
        if (!std::isfinite(next[i])) { error = "nonfinite pressure accelerator correction"; return false; }
    }
    output = std::move(next); error.clear(); return true;
}
}
