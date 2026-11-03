#include "CutHydroacousticGraph.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <tuple>

namespace judas::volume_grid {
namespace {

using Index = std::uint32_t;
constexpr Index absent = std::numeric_limits<Index>::max();
constexpr int iterationLimit = 6000;

bool Fail(std::string& error, const char* reason) {
    error = reason;
    return false;
}

struct Sum {
    double value = 0., correction = 0.;

    void Add(double term) {
        const double next = value + term;
        correction += std::abs(value) >= std::abs(term) ? (value - next) + term : (term - next) + value;
        value = next;
    }

    double Value() const { return value + correction; }
};

struct Component {
    Sum source, magnitude;
    Index gauge = absent;
    bool grounded = false;
};

struct Link {
    Index left, right;
    double conductance;
};

struct CompactGraph {
    std::vector<Index> indices, original;
    std::vector<double> storage, diagonal, source;
    std::vector<Link> links;

    bool Build(const std::vector<double>& groundStorage,
               const std::vector<CutHydroacousticGraphLink>& originalLinks,
               const std::vector<double>& rhs, std::string& error) {
        const auto count = rhs.size();
        std::vector<Index> parent(count), size(count, 1);
        std::vector<bool> active(count);
        std::iota(parent.begin(), parent.end(), Index(0));
        const auto root = [&](Index i) {
            while (parent[i] != i) {
                parent[i] = parent[parent[i]];
                i = parent[i];
            }
            return i;
        };
        for (const auto& [left, right, conductance] : originalLinks) {
            if (left >= count || right >= count || left == right ||
                !(conductance > 0. && std::isfinite(conductance)))
                return Fail(error, "invalid hydroacoustic pressure link");
            active[left] = active[right] = true;
            auto a = root(static_cast<Index>(left));
            auto b = root(static_cast<Index>(right));
            if (a != b) {
                if (size[a] < size[b])
                    std::swap(a, b);
                parent[b] = a;
                size[a] += size[b];
            }
        }
        std::vector<Component> components(count);
        for (std::size_t i = 0; i < count; ++i) {
            const double ground = groundStorage[i];
            if (!(ground >= 0. && std::isfinite(ground) && std::isfinite(rhs[i])))
                return Fail(error, "invalid hydroacoustic pressure storage or source");
            active[i] = active[i] || ground > 0.;
            if (!active[i] && rhs[i] != 0.)
                return Fail(error, "hydroacoustic source has no pressure owner");
            if (!active[i])
                continue;
            auto& component = components[root(static_cast<Index>(i))];
            component.source.Add(rhs[i]);
            component.magnitude.Add(std::abs(rhs[i]));
            component.grounded = component.grounded || ground > 0.;
            if (component.gauge == absent)
                component.gauge = static_cast<Index>(i);
        }
        for (const auto& component : components) {
            if (component.gauge == absent || component.grounded)
                continue;
            const double source = component.source.Value(), magnitude = component.magnitude.Value();
            const double roundoff = (128. * std::numeric_limits<double>::epsilon()) * magnitude;
            if (!(std::isfinite(source) && std::isfinite(magnitude) && std::abs(source) <= roundoff))
                return Fail(error, "ungrounded hydroacoustic component has incompatible volume source");
            active[component.gauge] = false;
        }

        indices.assign(count, absent);
        for (std::size_t i = 0; i < count; ++i) {
            if (!active[i])
                continue;
            indices[i] = static_cast<Index>(original.size());
            original.push_back(static_cast<Index>(i));
            storage.push_back(groundStorage[i]);
            source.push_back(rhs[i]);
        }
        links.reserve(originalLinks.size());
        diagonal = storage;
        for (const auto& [left, right, conductance] : originalLinks) {
            const auto a = indices[left], b = indices[right];
            if (a != absent && b != absent)
                links.push_back({a, b, conductance});
            else if (a != absent)
                storage[a] += conductance;
            else if (b != absent)
                storage[b] += conductance;
            if (a != absent)
                diagonal[a] += conductance;
            if (b != absent)
                diagonal[b] += conductance;
        }
        if (std::any_of(diagonal.begin(), diagonal.end(),
                        [](double v) { return !(v > 0. && std::isfinite(v)); }))
            return Fail(error, "hydroacoustic pressure diagonal is outside representable bounds");
        return true;
    }

    void Multiply(const std::vector<double>& x, std::vector<double>& result) const {
        for (std::size_t i = 0; i < x.size(); ++i)
            result[i] = storage[i] * x[i];
        for (const auto& [a, b, conductance] : links) {
            const double transfer = conductance * (x[a] - x[b]);
            result[a] += transfer;
            result[b] -= transfer;
        }
    }
};

class IncompleteCholesky {
public:
    bool Build(const CompactGraph& graph) {
        std::vector<Link> lower;
        lower.reserve(graph.links.size());
        for (const auto& [a, b, conductance] : graph.links)
            lower.push_back({std::max(a, b), std::min(a, b), -conductance});
        std::sort(lower.begin(), lower.end(), [](const Link& a, const Link& b) {
            return std::tie(a.left, a.right) < std::tie(b.left, b.right);
        });
        offsets_.resize(graph.diagonal.size() + 1);
        for (std::size_t i = 0; i < lower.size(); ++i) {
            const auto& entry = lower[i];
            if (i > 0 && lower[i - 1].left == entry.left && lower[i - 1].right == entry.right) {
                values_.back() += entry.conductance;
            } else {
                ++offsets_[entry.left + 1];
                columns_.push_back(entry.right);
                values_.push_back(entry.conductance);
            }
        }
        std::partial_sum(offsets_.begin(), offsets_.end(), offsets_.begin());
        inversePivots_.resize(graph.diagonal.size());
        for (std::size_t i = 0; i < graph.diagonal.size(); ++i) {
            const double diagonal = graph.diagonal[i];
            double pivot = diagonal;
            for (std::size_t entry = offsets_[i]; entry < offsets_[i + 1]; ++entry) {
                const auto j = columns_[entry];
                double value = values_[entry];
                auto a = offsets_[i], b = offsets_[j];
                while (a < entry && b < offsets_[j + 1]) {
                    if (columns_[a] < columns_[b]) {
                        ++a;
                    } else if (columns_[a] > columns_[b]) {
                        ++b;
                    } else {
                        value -= values_[a++] * values_[b++];
                    }
                }
                value *= inversePivots_[j];
                values_[entry] = value;
                pivot -= value * value;
            }
            if (!std::isfinite(pivot))
                return false;
            // Only the preconditioner pivot is stabilized. The physical
            // rows are unchanged and audited after the solve.
            const double floor = (64. * std::numeric_limits<double>::epsilon()) * diagonal;
            pivot = std::max(pivot, floor);
            if (!(pivot > 0.))
                return false;
            inversePivots_[i] = 1. / std::sqrt(pivot);
        }
        return true;
    }

    void Apply(const std::vector<double>& input, std::vector<double>& output) const {
        for (std::size_t i = 0; i < input.size(); ++i) {
            double value = input[i];
            for (std::size_t entry = offsets_[i]; entry < offsets_[i + 1]; ++entry)
                value -= values_[entry] * output[columns_[entry]];
            output[i] = value * inversePivots_[i];
        }
        for (std::size_t i = input.size(); i-- > 0;) {
            output[i] *= inversePivots_[i];
            for (std::size_t entry = offsets_[i]; entry < offsets_[i + 1]; ++entry)
                output[columns_[entry]] -= values_[entry] * output[i];
        }
    }

private:
    std::vector<Index> offsets_, columns_;
    std::vector<double> values_, inversePivots_;
};

double Dot(const std::vector<double>& a, const std::vector<double>& b) {
    std::array<double, 4> sums{};
    std::size_t i = 0;
    for (; i + sums.size() <= a.size(); i += sums.size())
        for (std::size_t lane = 0; lane < sums.size(); ++lane)
            sums[lane] += a[i + lane] * b[i + lane];
    for (; i < a.size(); ++i)
        sums[0] += a[i] * b[i];
    return (sums[0] + sums[1]) + (sums[2] + sums[3]);
}

double CheckPhysicalRows(const CompactGraph& graph,
                         const std::vector<double>& groundStorage,
                         const std::vector<CutHydroacousticGraphLink>& links,
                         const std::vector<double>& rhs, const std::vector<double>& pressure,
                         std::vector<Sum>& rows) {
    const auto head = [&](std::size_t i) {
        const auto index = graph.indices[i];
        return index == absent ? 0. : pressure[index];
    };
    for (std::size_t i = 0; i < rhs.size(); ++i) {
        rows[i] = {};
        rows[i].Add(rhs[i]);
        rows[i].Add(-groundStorage[i] * head(i));
    }
    for (const auto& [a, b, conductance] : links) {
        const double transfer = conductance * (head(a) - head(b));
        rows[a].Add(-transfer);
        rows[b].Add(transfer);
    }
    double maximum = 0.;
    for (const auto& row : rows) {
        const double residual = row.Value();
        if (!std::isfinite(residual))
            return std::numeric_limits<double>::infinity();
        maximum = std::max(maximum, std::abs(residual));
    }
    return maximum;
}

} // namespace

bool SolveCutHydroacousticGraph(const std::vector<double>& groundStorage,
                               const std::vector<CutHydroacousticGraphLink>& links,
                               const std::vector<double>& rhs, double volumeTolerance,
                               CutHydroacousticGraphResult& output, std::string& error) {
    const auto count = rhs.size();
    if (count == 0 || count >= absent || links.size() >= absent ||
        groundStorage.size() != count || !(volumeTolerance > 0. && std::isfinite(volumeTolerance)))
        return Fail(error, "invalid hydroacoustic pressure graph dimensions or tolerance");

    CompactGraph graph;
    if (!graph.Build(groundStorage, links, rhs, error))
        return false;
    const auto active = graph.source.size();
    std::vector<double> pressure(active);
    std::vector<Sum> physicalRows(count);
    CutHydroacousticGraphResult result;
    const auto audit = [&] {
        result.maximumResidual = CheckPhysicalRows(graph, groundStorage, links, rhs, pressure, physicalRows);
        return result.maximumResidual <= volumeTolerance;
    };

    if (!audit()) {
        IncompleteCholesky factor;
        if (!factor.Build(graph))
            return Fail(error, "hydroacoustic incomplete factorization is not representable");
        std::vector<double> residual = graph.source, scaled(active), direction(active), applied(active);
        factor.Apply(residual, scaled);
        direction = scaled;
        double product = Dot(residual, scaled);
        bool converged = false;
        for (; result.iterations < iterationLimit; ++result.iterations) {
            if (!(product > 0. && std::isfinite(product)))
                break;
            graph.Multiply(direction, applied);
            const double curvature = Dot(direction, applied);
            if (!(curvature > 0. && std::isfinite(curvature)))
                break;
            const double step = product / curvature;
            double largest = 0.;
            for (std::size_t i = 0; i < active; ++i) {
                pressure[i] += step * direction[i];
                residual[i] -= step * applied[i];
                largest = std::max(largest, std::abs(residual[i]));
            }
            const bool refresh = largest <= volumeTolerance || (result.iterations + 1) % 256 == 0;
            if (refresh) {
                if (audit()) {
                    ++result.iterations;
                    converged = true;
                    break;
                }
                for (std::size_t i = 0; i < active; ++i)
                    residual[i] = physicalRows[graph.original[i]].Value();
            }
            factor.Apply(residual, scaled);
            const double next = Dot(residual, scaled);
            const double ratio = refresh ? 0. : next / product;
            for (std::size_t i = 0; i < active; ++i)
                direction[i] = scaled[i] + ratio * direction[i];
            product = next;
        }
        if (!converged && !audit())
            return Fail(error, "hydroacoustic pressure graph failed original row balance");
    }

    result.pressure.resize(count);
    for (std::size_t i = 0; i < active; ++i)
        result.pressure[graph.original[i]] = pressure[i];
    output = std::move(result);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
