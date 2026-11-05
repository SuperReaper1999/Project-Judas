#pragma once

#include "ModelImport.h"
#include "SkeletalAnimation.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

// Authoring-only advice. Never changes vertices, hierarchy, unit settings or
// collision. Bounds use the selected rendered parts at their normalized rest
// pose, rather than raw FBX centimetres or a guessed character height.
namespace JudasImport {
inline const ModelImportDiagnostic *ScaleWarning(const ModelImportReport &report) {
    const auto found = std::find_if(report.diagnostics.begin(), report.diagnostics.end(),
                                   [](const auto &d) { return d.code == "import-scale-review"; });
    return found == report.diagnostics.end() ? nullptr : &*found;
}

inline void InspectScale(const std::string &source, const ModelImportSettings &settings,
                         const MeshData &mesh, ModelImportReport &report) {
    report.diagnostics.erase(
        std::remove_if(report.diagnostics.begin(), report.diagnostics.end(),
                       [](const auto &d) { return d.code == "import-scale-review"; }),
        report.diagnostics.end());
    glm::dvec3 lo(std::numeric_limits<double>::infinity()), hi(-lo);
    const auto palette = mesh.skeletal
                             ? ResolveSkinMatrices(mesh.skeletal->skeleton, mesh.skeletal->skeleton.rest)
                             : std::vector<glm::mat4>{};
    double largestTransform = 1;
    bool valid = true, any = false;
    std::vector<std::uint8_t> visited(mesh.vertices.size(), 0);
    auto visit = [&](std::uint32_t index) {
        if (index >= mesh.vertices.size()) { valid = false; return; }
        if (visited[index]) return;
        visited[index] = 1;
        glm::dvec3 position(mesh.vertices[index].position);
        if (!palette.empty()) {
            if (index >= mesh.skinVertices.size()) { valid = false; return; }
            const auto &skin = mesh.skinVertices[index];
            glm::dmat4 transform(0);
            for (int axis = 0; axis < 4; ++axis) {
                if ((skin.weights[axis] > 0 && skin.joints[axis] >= palette.size()) ||
                    (skin.weights1[axis] > 0 && skin.joints1[axis] >= palette.size())) {
                    valid = false; return;
                }
                if (skin.weights[axis] > 0)
                    transform += glm::dmat4(palette[skin.joints[axis]]) * double(skin.weights[axis]);
                if (skin.weights1[axis] > 0)
                    transform += glm::dmat4(palette[skin.joints1[axis]]) * double(skin.weights1[axis]);
            }
            position = glm::dvec3(transform * glm::dvec4(position, 1));
            for (int axis = 0; axis < 3; ++axis)
                largestTransform = std::max(largestTransform, glm::length(glm::dvec3(transform[axis])));
        }
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
            valid = false; return;
        }
        lo = glm::min(lo, position); hi = glm::max(hi, position); any = true;
    };
    if (!mesh.primitives.empty()) {
        for (const auto &part : mesh.primitives) {
            if (part.first > mesh.indices.size() || part.count > mesh.indices.size() - part.first) {
                valid = false; break;
            }
            for (std::size_t i = part.first; i < std::size_t(part.first) + part.count; ++i)
                visit(mesh.indices[i]);
        }
    } else if (!mesh.indices.empty()) {
        for (auto index : mesh.indices) visit(index);
    } else {
        for (std::size_t i = 0; i < mesh.vertices.size(); ++i) visit(std::uint32_t(i));
    }
    if (!valid || !any) return; // The importer owns malformed-geometry errors.
    const auto size = hi - lo;
    const double largest = std::max({size.x, size.y, size.z});
    const double ratio = settings.sourceUnitMeters > 0 && report.sourceUnitMeters > 0
                             ? settings.sourceUnitMeters / report.sourceUnitMeters : 1;
    const bool overrideMismatch = ratio >= 99 || ratio <= 1.0 / 99;
    const bool unusualSize = largest >= 100 || (largest > 0 && largest <= .01);
    // Centimetre FBX can legitimately retain a 100x rig transform with a
    // compensating unit/bind conversion. Correct physical bounds and metadata
    // take precedence over that raw scale; do not scare ordinary character imports.
    const bool transformMismatch = largestTransform >= 99 &&
        (report.sourceUnitMeters >= .5 || overrideMismatch || unusualSize);
    // An explicit 0.01 override that produces an ordinary metre-size model is
    // a successful correction, not another warning solely for choosing units.
    if (!transformMismatch && !unusualSize) return;
    std::ostringstream message;
    message << std::setprecision(6)
            << "Check import scale: normalized rest-pose dimensions are " << size.x << " x "
            << size.y << " x " << size.z << " metres (X/Y/Z). ";
    if (transformMismatch)
        message << "The rendered geometry includes a " << largestTransform << "x effective transform. ";
    if (overrideMismatch)
        message << "The chosen metres-per-unit override is " << ratio << "x the source units. ";
    message << "A centimetres/metres mismatch can make an asset 100x too large or small. "
               "Large levels and tiny objects can be intentional; no automatic rescaling was applied.";
    report.diagnostics.push_back({"warning", "import-scale-review", source, "",
        "Compare against the one-metre ruler. Review source unit metadata / Source metres per unit, "
        "or apply scale in your modelling tool and re-export. Continue at this scale if intentional.",
        message.str()});
}
} // namespace JudasImport
