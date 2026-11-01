#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace FluidHydrostaticDetail {

struct SurfaceDatum {
    double x, z, height, weight;
};
struct CurvedSurfacePatch {
    double height = 0;
    double confidence = 0;
    double residual = 0;
};

// Query-local height reconstruction, not a pressure solver. The caller must
// establish single-layer exterior data, enclosing coverage and actual wet
// contact independently. Never use the fitted cap to admit otherwise dry space.
inline std::optional<CurvedSurfacePatch> FitCurvedSurface(
    const std::vector<SurfaceDatum>& data, double scale, double spacing,
    double inputError = 0) {
    if (data.size() < 6 || !std::isfinite(scale) || !(scale > 0) ||
        !std::isfinite(spacing) || !(spacing > 0) ||
        !std::isfinite(inputError) || inputError < 0) return {};

    double sumWeight = 0, sumSquared = 0, maxWeight = 0;
    for (const auto& d : data) {
        if (!std::isfinite(d.weight) || d.weight < 0 || !std::isfinite(d.x) ||
            !std::isfinite(d.z) || !std::isfinite(d.height)) return {};
        sumWeight += d.weight;
        sumSquared += d.weight * d.weight;
        maxWeight = std::max(maxWeight, d.weight);
    }
    if (!(sumSquared > 0) || !(maxWeight > 0) || !std::isfinite(sumWeight) ||
        !std::isfinite(sumSquared)) return {};
    const double effective = sumWeight * sumWeight / sumSquared;
    if (!std::isfinite(effective)) return {};

    std::vector<std::array<double, 7>> a;
    a.reserve(data.size());
    for (const auto& d : data) {
        const double x = d.x / scale, z = d.z / scale, w = std::sqrt(d.weight);
        a.push_back({w, w*x, w*z, w*x*x, w*x*z, w*z*z, w*d.height});
    }
    // Householder QR avoids squaring the condition number in normal equations.
    // Normalize the basis [1,x,z,x*x,x*z,z*z] in the query's tangent frame.
    double largest = 0, smallest = std::numeric_limits<double>::infinity();
    std::vector<double> reflector(data.size());
    for (std::size_t k = 0; k < 6; ++k) {
        double norm = 0;
        for (std::size_t i = k; i < a.size(); ++i) norm = std::hypot(norm, a[i][k]);
        if (!(norm > 0) || !std::isfinite(norm)) return {};
        const double diagonal = -std::copysign(norm, a[k][k]);
        largest = std::max(largest, norm);
        smallest = std::min(smallest, norm);
        if (norm <= 128 * std::numeric_limits<double>::epsilon() * largest) return {};
        for (std::size_t i = k; i < a.size(); ++i) reflector[i-k] = a[i][k];
        reflector[0] -= diagonal;
        double squared = 0;
        for (std::size_t i = 0; i < a.size()-k; ++i) squared += reflector[i]*reflector[i];
        if (!(squared > 0) || !std::isfinite(squared)) return {};
        for (std::size_t j = k; j < 7; ++j) {
            double product = 0;
            for (std::size_t i = k; i < a.size(); ++i) product += reflector[i-k]*a[i][j];
            const double factor = 2*product/squared;
            for (std::size_t i = k; i < a.size(); ++i) a[i][j] -= factor*reflector[i-k];
        }
        a[k][k] = diagonal;
    }
    std::array<double, 6> coefficients{};
    for (int k = 5; k >= 0; --k) {
        const auto row = static_cast<std::size_t>(k);
        double value = a[row][6];
        for (std::size_t j = row+1; j < 6; ++j) value -= a[row][j]*coefficients[j];
        coefficients[row] = value/a[row][row];
        if (!std::isfinite(coefficients[row])) return {};
    }
    const auto& c = coefficients;
    double residual = 0;
    for (const auto& d : data) {
        const double x = d.x/scale, z = d.z/scale;
        const double error = std::abs(c[0]+c[1]*x+c[2]*z+c[3]*x*x+c[4]*x*z+c[5]*z*z-d.height);
        residual = std::max(residual, error*std::sqrt(d.weight/maxWeight));
    }
    // R^T e = evaluation at the query. An almost weightless donor can complete
    // a missing curvature mode: propagate input error through that sensitivity
    // instead of trusting a small residual or a fixed minimum donor count.
    std::array<double, 6> evaluation{};
    double normEvaluation = 0;
    for (std::size_t k = 0; k < 6; ++k) {
        double value = k == 0 ? 1.0 : 0.0;
        for (std::size_t j = 0; j < k; ++j) value -= a[j][k]*evaluation[j];
        evaluation[k] = value/a[k][k];
        normEvaluation = std::hypot(normEvaluation, evaluation[k]);
    }
    const double amplification = std::sqrt(sumWeight)*normEvaluation;
    const double slope = 1+(std::abs(c[1])+std::abs(c[2])+3*(std::abs(c[3])+std::abs(c[4])+std::abs(c[5])))/scale;
    const double uncertainty = amplification*(residual+inputError*slope);
    if (!std::isfinite(uncertainty)) return {};
    const auto smooth = [](double x) {
        x = std::clamp(x, 0., 1.);
        return x*x*(3-2*x);
    };
    const double threshold = std::sqrt(std::numeric_limits<double>::epsilon());
    // Quarter-spacing is the reconstruction's spatial error budget, independent
    // of test acceptance thresholds. Redundancy and uncertainty fade smoothly.
    const double confidence = smooth((smallest/largest-threshold)/threshold) *
        smooth((effective-6)/6) * smooth(1-uncertainty/(spacing*.25));
    return CurvedSurfacePatch{c[0], confidence, residual};
}

} // namespace FluidHydrostaticDetail
