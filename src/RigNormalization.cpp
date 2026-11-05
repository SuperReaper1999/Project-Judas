#include "RigNormalization.h"
#include "SkeletalAnimation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
glm::mat4 Units(float factor) {
    return glm::scale(glm::mat4(1), glm::vec3(factor));
}
float FrameScale(const glm::mat4 &matrix) {
    const float scale = glm::length(glm::vec3(matrix[0]));
    if (!std::isfinite(scale) || scale < 1e-6f || scale > 1e6f)
        throw std::runtime_error("rig scale is non-finite, singular or outside 1e-6..1e6");
    const glm::mat3 rotation = glm::mat3(matrix) / scale;
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            if (std::abs(glm::dot(rotation[a], rotation[b]) - (a == b ? 1.f : 0.f)) > 1e-4f)
                throw std::runtime_error("rig rest frame has non-uniform scale or shear; review in the source modeller");
    if (glm::determinant(rotation) < 0)
        throw std::runtime_error("reflected rig rest frames require source-side review");
    return scale;
}
void ComparePose(const Skeleton &before, const SkeletalPose &oldPose,
                 const Skeleton &after, const SkeletalPose &newPose,
                 float meshScale, RigNormalizationReport &report) {
    const auto oldGlobals = ResolveJointMatrices(before, oldPose);
    const auto newGlobals = ResolveJointMatrices(after, newPose);
    const auto oldSkin = ResolveSkinMatrices(before, oldPose);
    const auto newSkin = ResolveSkinMatrices(after, newPose);
    auto check = [&](float a, float b) {
        const double difference = std::abs(double(a) - double(b));
        if (!std::isfinite(difference) || difference > 1e-4 * std::max(1., std::abs(double(a))))
            throw std::runtime_error("normalisation failed the original-versus-converted pose equivalence check");
        report.maximumMatrixError = std::max(report.maximumMatrixError, difference);
    };
    for (size_t i = 0; i < oldGlobals.size(); ++i)
        for (int axis = 0; axis < 3; ++axis)
            check(oldGlobals[i][3][axis], newGlobals[i][3][axis]);
    for (size_t i = 0; i < oldSkin.size(); ++i) {
        const auto restored = newSkin[i] * Units(meshScale);
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                check(oldSkin[i][col][row], restored[col][row]);
    }
    ++report.posesChecked;
}
} // namespace

bool NormalizeModelRig(const MeshData &source, MeshData &output,
                       RigNormalizationReport &report, std::string &error) {
    try {
        if (!source.skeletal || source.skeletal->skeleton.skinNodes.empty())
            throw std::runtime_error("normalise rig requires a skinned model");
        if (!source.lods.empty() || !source.lodIndices.empty())
            throw std::runtime_error("normalise the base model before generating mesh LODs");
        const auto &old = source.skeletal->skeleton;
        const auto globals = ResolveJointMatrices(old, old.rest);
        std::vector<float> scales;
        for (const auto &matrix : globals) scales.push_back(FrameScale(matrix));
        float meshScale = 0;
        for (const auto &matrix : ResolveSkinMatrices(old, old.rest))
            meshScale = std::max(meshScale, FrameScale(matrix));
        MeshData candidate = source;
        auto asset = std::make_shared<SkeletalAsset>(*source.skeletal);
        auto &rig = asset->skeleton;
        for (size_t i = 0; i < scales.size(); ++i) {
            const float parent = old.parents[i] < 0 ? 1.f : scales.at(old.parents[i]);
            rig.rest.local[i].translation *= parent;
            rig.rest.local[i].scale *= parent / scales[i];
            if (!rig.affine.empty())
                rig.affine[i] = Units(parent) * rig.affine[i] * Units(1 / parent);
        }
        for (size_t i = 0; i < rig.skinNodes.size(); ++i)
            rig.inverseBind[i] = Units(scales.at(rig.skinNodes[i])) * old.inverseBind[i] * Units(1 / meshScale);
        for (auto &clip : asset->clips)
            for (auto &track : clip.tracks) {
                const auto node = size_t(track.node);
                const float parent = old.parents.at(node) < 0 ? 1.f : scales.at(old.parents[node]);
                const float factor = track.path == TrackPath::Translation ? parent :
                                     track.path == TrackPath::Scale ? parent / scales.at(node) : 1.f;
                // Cubic tangents are derivatives and use the same coordinate conversion.
                for (auto &value : track.values)
                    for (int axis = 0; axis < 3; ++axis) value[axis] *= factor;
            }
        for (auto &vertex : candidate.vertices) {
            vertex.position *= meshScale;
            for (int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(vertex.position[axis]))
                    throw std::runtime_error("normalised vertex coordinates overflow");
        }
        candidate.skeletal = asset;
        RigNormalizationReport result;
        result.meshScale = meshScale;
        ComparePose(old, old.rest, rig, rig.rest, meshScale, result);
        for (size_t i = 0; i < asset->clips.size(); ++i) {
            const auto &before = source.skeletal->clips[i];
            const auto &after = asset->clips[i];
            // Full key times plus interval samples exercise STEP/LINEAR/CUBICSPLINE.
            std::vector<float> times{0, before.duration};
            for (const auto &track : before.tracks)
                times.insert(times.end(), track.times.begin(), track.times.end());
            std::sort(times.begin(), times.end());
            times.erase(std::unique(times.begin(), times.end()), times.end());
            const auto keys = times;
            for (size_t k = 1; k < keys.size(); ++k)
                for (float fraction : {.25f, .5f, .75f})
                    times.push_back(keys[k-1] + (keys[k] - keys[k-1]) * fraction);
            for (float time : times)
                ComparePose(old, SampleClip(old, before, time), rig, SampleClip(rig, after, time), meshScale, result);
        }
        // This is a derived standalone asset, not the unmodified recipe product.
        candidate.importRecord.clear();
        output = std::move(candidate);
        report = result;
        error.clear();
        return true;
    } catch (const std::exception &exception) {
        error = exception.what();
        return false;
    }
}
