#include "WaterBulkCondensation.h"
#include <cmath>
namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
bool Fail(std::string &error, const char *why) {
    error = why;
    return false;
}
} // namespace
bool WaterBulkCondensation::Build(const std::vector<std::map<std::size_t, Real>> &source,
                                  const std::vector<bool> &selected, std::string &error) {
    const auto n = source.size();
    if (!n || selected.size() != n)
        return Fail(error, "invalid gas condensation dimensions");
    WaterBulkCondensation next;
    next.size_ = n;
    auto matrix = source;
    std::vector<std::set<std::size_t>> incoming(n);
    std::vector<bool> eliminated(n);
    for (std::size_t i = 0; i < n; ++i) {
        for (auto it = matrix[i].begin(); it != matrix[i].end();) {
            if (it->first >= n || !std::isfinite(it->second.Value()))
                return Fail(error, "invalid gas condensation entry");
            if (it->second.Value() == 0.)
                it = matrix[i].erase(it);
            else {
                incoming[it->first].insert(i);
                ++it;
            }
        }
        auto d = matrix[i].find(i);
        if (d == matrix[i].end() || !(d->second.Value() > 0.))
            return Fail(error, "gas condensation lacks a positive pivot");
    }
    std::size_t work = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (selected[i] && matrix[i].size() <= 13 && incoming[i].size() <= 13) {
            Step step;
            step.node = i;
            step.diagonal = matrix[i].at(i);
            if (!(step.diagonal.Value() > 0. && std::isfinite(step.diagonal.Value())))
                return Fail(error, "gas condensation pivot is not positive");
            for (auto [j, a] : matrix[i])
                if (j != i)
                    step.row.push_back({j, a});
            std::vector<std::size_t> column(incoming[i].begin(), incoming[i].end());
            for (auto j : column)
                if (j != i) {
                    Real factor = matrix[j].at(i) / step.diagonal;
                    step.multipliers.push_back({j, factor});
                    for (auto [k, a] : step.row) {
                        if (++work > 20000000)
                            return Fail(error, "gas condensation work budget exceeded");
                        auto entry = matrix[j].find(k);
                        if (entry == matrix[j].end()) {
                            entry = matrix[j].emplace(k, Real()).first;
                            incoming[k].insert(j);
                        }
                        entry->second = entry->second - factor * a;
                    }
                    matrix[j].erase(i);
                }
            for (auto [j, a] : matrix[i]) {
                (void)a;
                incoming[j].erase(i);
            }
            matrix[i].clear();
            incoming[i].clear();
            eliminated[i] = true;
            next.steps_.push_back(std::move(step));
        }
    std::vector<std::size_t> index(n, SIZE_MAX);
    for (std::size_t i = 0; i < n; ++i)
        if (!eliminated[i]) {
            index[i] = next.remaining_.size();
            next.remaining_.push_back(i);
        }
    std::vector<std::map<std::size_t, Real>> reduced(next.remaining_.size());
    for (std::size_t i = 0; i < next.remaining_.size(); ++i)
        for (auto [j, a] : matrix[next.remaining_[i]]) {
            if (index[j] == SIZE_MAX)
                return Fail(error, "gas condensation retained a removed dependency");
            reduced[i][index[j]] = a;
        }
    if (!reduced.empty() && !next.reduced_.Build(reduced, error, 1))
        return false;
    *this = std::move(next);
    error.clear();
    return true;
}
bool WaterBulkCondensation::Apply(const std::vector<double> &rhs, std::vector<double> &output,
                                  std::string &error) const {
    if (rhs.size() != size_ || size_ == 0)
        return Fail(error, "gas condensation RHS mismatch");
    std::vector<Real> residual(rhs.begin(), rhs.end()), solution(size_);
    for (double value : rhs)
        if (!std::isfinite(value))
            return Fail(error, "nonfinite gas condensation RHS");
    for (const auto &step : steps_)
        for (auto [j, factor] : step.multipliers)
            residual[j] = residual[j] - factor * residual[step.node];
    if (!remaining_.empty()) {
        std::vector<double> reduced, solved;
        for (auto i : remaining_)
            reduced.push_back(residual[i].Value());
        if (!reduced_.Apply(reduced, solved, error))
            return false;
        for (std::size_t j = 0; j < remaining_.size(); ++j)
            solution[remaining_[j]] = Real(solved[j]);
    }
    for (std::size_t j = steps_.size(); j-- > 0;) {
        const auto &step = steps_[j];
        Real value = residual[step.node];
        for (auto [k, a] : step.row)
            value = value - a * solution[k];
        solution[step.node] = value / step.diagonal;
    }
    std::vector<double> next(size_);
    for (std::size_t i = 0; i < size_; ++i) {
        next[i] = solution[i].Value();
        if (!std::isfinite(next[i]))
            return Fail(error, "nonfinite gas condensation correction");
    }
    output = std::move(next);
    error.clear();
    return true;
}
} // namespace judas::volume_grid
