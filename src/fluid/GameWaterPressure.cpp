#include "GameWater.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

namespace JudasWater {
bool Grid::Project(float dt, std::string &error) {
    const auto start = std::chrono::steady_clock::now();
    const int n = int(cells_.size());
    const float h = settings_.cellSize, area = h * h, rho = settings_.density;
    const auto gravity = glm::conjugate(settings_.rotation) * settings_.gravity;
    std::vector<int> node(n, -1), airNode(air_.size(), -1);
    int unknowns = 0;
    for (int i = 0; i < n; ++i)
        if (cells_[i].capacity > 0 &&
            cells_[i].water >= (1. - settings_.minimumAirFraction) * cells_[i].capacity &&
            !(cells_[i].air >= 0 && !air_[cells_[i].air].atmosphere &&
              !air_[cells_[i].air].unresolved && air_[cells_[i].air].mass > 0))
            node[i] = unknowns++;
    for (int i = 0; i < int(air_.size()); ++i)
        if (!air_[i].atmosphere && !air_[i].unresolved && air_[i].mass > 0)
            airNode[i] = unknowns++;
    std::vector<double> diag(unknowns), rhs(unknowns), p(unknowns), offset(n);
    std::vector<Edge> edges;
    auto waterNode = [&](int i) {
        return node[i] >= 0 ? node[i] : (cells_[i].air >= 0 ? airNode[cells_[i].air] : -1);
    };
    for (int i = 0; i < n; ++i)
        if (node[i] < 0 && cells_[i].capacity > 0)
            offset[i] = .5 * rho * std::abs(gravity.y) * h *
                        std::min(1., cells_[i].water / cells_[i].capacity);
    for (int k = 0; k < int(air_.size()); ++k)
        if (airNode[k] >= 0) {
            auto pocket = air_[k];
            int a = airNode[k];
            const double compliance =
                rho * pocket.volume / (1.4 * std::max(100., 101325. + pocket.pressure) * dt * dt);
            diag[a] += compliance;
            rhs[a] += compliance * pocket.pressure;
            p[a] = pocket.pressure;
        }
    auto edge = [&](int a, int b, double w, double flow, double difference) {
        if (a == b)
            return;
        edges.push_back({a, b, w});
        if (a >= 0) {
            diag[a] += w;
            rhs[a] -= rho / dt * flow + w * difference;
        }
        if (b >= 0) {
            diag[b] += w;
            rhs[b] += rho / dt * flow + w * difference;
        }
    };
    std::vector<float> weights(faces_.size());
    for (std::size_t f = 0; f < faces_.size(); ++f) {
        auto &face = faces_[f];
        if (face.open <= 0 || cells_[face.a].capacity <= 0 || cells_[face.b].capacity <= 0) {
            face.velocity = 0;
            continue;
        }
        if (cells_[face.a].water <= settings_.minimumMobileFraction * cells_[face.a].capacity &&
            cells_[face.b].water <= settings_.minimumMobileFraction * cells_[face.b].capacity)
            continue;
        float distance = 1;
        if (face.axis == 1 && (node[face.a] >= 0) != (node[face.b] >= 0)) {
            int partial = node[face.a] < 0 ? face.a : face.b;
            distance = .5f + .5f * float(cells_[partial].water / cells_[partial].capacity);
        }
        weights[f] = face.open * area / (h * distance);
        const int a = waterNode(face.a), b = waterNode(face.b);
        edge(a, b, weights[f], face.velocity * face.open * area, offset[face.a] - offset[face.b]);
    }
    // Small openings are finite air resistors; their area is not promoted to
    // an instantaneous pressure union. Linearize a standard orifice law.
    struct Leak {
        int cellA, cellB, a, b;
        double conductance;
    };
    std::vector<Leak> leaks;
    for (std::size_t f = 0; f < faces_.size(); ++f) {
        const auto &face = faces_[f];
        int ka = cells_[face.a].air, kb = cells_[face.b].air;
        if (ka < 0 || kb < 0 || ka == kb || face.open <= 0)
            continue;
        double opening = area * DryArea(f);
        if (opening <= 0)
            continue;
        double dp = air_[ka].pressure - air_[kb].pressure;
        double c = .62 * opening * std::sqrt(2. / (1.2 * std::max(1., std::abs(dp))));
        const int a = airNode[ka], b = airNode[kb];
        double w = rho / dt * c;
        edge(a, b, w, 0, 0);
        leaks.push_back({face.a, face.b, a, b, c});
    }
    auto multiply = [&](const std::vector<double> &x, std::vector<double> &y) {
        y.resize(unknowns);
        for (int i = 0; i < unknowns; ++i)
            y[i] = diag[i] * x[i];
        for (auto e : edges)
            if (e.a >= 0 && e.b >= 0) {
                y[e.a] -= e.weight * x[e.b];
                y[e.b] -= e.weight * x[e.a];
            }
    };
    auto dot = [](const std::vector<double> &a, const std::vector<double> &b) {
        return std::inner_product(a.begin(), a.end(), b.begin(), 0.);
    };
    std::vector<double> r(unknowns), z(unknowns), direction(unknowns), ap;
    multiply(p, ap);
    for (int i = 0; i < unknowns; ++i) {
        r[i] = rhs[i] - ap[i];
        z[i] = r[i] / std::max(diag[i], 1e-30);
    }
    direction = z;
    double rz = dot(r, z), initial = std::sqrt(dot(r, r)), residual = initial;
    unsigned iteration = 0;
    for (; iteration < settings_.pressureIterations &&
           residual > std::max(1e-6, initial * settings_.pressureTolerance);
         ++iteration) {
        multiply(direction, ap);
        double denominator = dot(direction, ap);
        if (!(denominator > 0) || !std::isfinite(denominator))
            break;
        double alpha = rz / denominator;
        for (int i = 0; i < unknowns; ++i) {
            p[i] += alpha * direction[i];
            r[i] -= alpha * ap[i];
            z[i] = r[i] / std::max(diag[i], 1e-30);
        }
        double next = dot(r, z), beta = next / rz;
        rz = next;
        for (int i = 0; i < unknowns; ++i)
            direction[i] = z[i] + beta * direction[i];
        residual = std::sqrt(dot(r, r));
    }
    multiply(p, ap);
    double actual = 0;
    for (int i = 0; i < unknowns; ++i)
        actual += (rhs[i] - ap[i]) * (rhs[i] - ap[i]);
    residual = std::sqrt(actual);
    stats_.iterations += iteration;
    stats_.pressureResidual =
        std::max(stats_.pressureResidual, residual / std::max(initial, 1e-30));
    for (double v : p)
        if (!std::isfinite(v)) {
            error = "Non-finite game-water pressure.";
            return false;
        }
    if (residual > std::max(1e-4, initial * .01)) {
        error = "Game-water pressure failed its bounded residual gate: " +
                std::to_string(residual / std::max(initial, 1e-30));
        return false;
    }
    for (int i = 0; i < n; ++i) {
        int a = waterNode(i);
        cells_[i].pressure = float(offset[i] + (a >= 0 ? p[a] : 0));
    }
    for (std::size_t f = 0; f < faces_.size(); ++f)
        if (weights[f] > 0) {
            auto &face = faces_[f];
            face.velocity += dt / rho * weights[f] *
                             (cells_[face.a].pressure - cells_[face.b].pressure) /
                             (face.open * area);
        }
    // Retain gas mass exactly as paired exchanges. The final volume determines
    // the next reservoir pressure; atmospheric exchange has its own receipt.
    std::vector<double> out(n), flow(leaks.size());
    for (std::size_t f = 0; f < leaks.size(); ++f) {
        auto e = leaks[f];
        flow[f] = 1.2 * dt * e.conductance * ((e.a >= 0 ? p[e.a] : 0) - (e.b >= 0 ? p[e.b] : 0));
        out[flow[f] >= 0 ? e.cellA : e.cellB] += std::abs(flow[f]);
    }
    for (std::size_t f = 0; f < leaks.size(); ++f) {
        auto e = leaks[f];
        int donor = flow[f] >= 0 ? e.cellA : e.cellB;
        double amount = flow[f] * std::min(1., cells_[donor].airMass / std::max(out[donor], 1e-30));
        cells_[e.cellA].airMass -= amount;
        cells_[e.cellB].airMass += amount;
    }
    gasReserve_.assign(n, 0);
    for (int i = 0; i < n; ++i)
        if (cells_[i].air >= 0) {
            int k = cells_[i].air;
            if (airNode[k] < 0)
                continue;
            double absolute = 101325. + p[airNode[k]];
            if (!(absolute > 0)) {
                error = "Game-water gas linearization crossed zero absolute pressure.";
                return false;
            }
            double volume = air_[k].mass / (1.2 * std::pow(absolute / 101325., 1. / 1.4));
            gasReserve_[i] =
                std::min(double(cells_[i].capacity),
                         volume * std::max(0., double(cells_[i].capacity) - cells_[i].water) /
                             air_[k].volume);
        }
    stats_.pressureMs +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}
} // namespace JudasWater
