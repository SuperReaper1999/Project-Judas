#include "render/SubmissionQueue.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const std::string &label) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", label.c_str());
}
MaterialBinding FullBinding(unsigned variant = 0) {
    MaterialBinding result;
    result.handle = {7 + variant, 3};
    result.explicitAsset = true;
    result.failed = false;
    for (unsigned i = 0; i < 5; ++i) {
        result.textures[i] = {20 + i + variant * 10, 4 + i};
        result.textureOverrides[i] = i % 2 == 0;
        char asset[33];
        std::snprintf(asset, sizeof(asset), "%032x", i + variant * 10 + 1);
        result.overrides.textures[i] = asset;
    }
    auto &o = result.overrides;
    o.baseColor = glm::vec4(.2f, .4f, .6f, .8f);
    o.metallic = .3f;
    o.roughness = .5f;
    o.emissiveIntensity = 1.5f;
    o.emissive = glm::vec3(.1f, .2f, .3f);
    o.uvScale = glm::vec2(2, 3);
    o.uvOffset = glm::vec2(.25f, .75f);
    o.alpha = MaterialAlpha::Opaque;
    o.alphaCutoff = .4f;
    o.normalStrength = .75f;
    o.occlusionStrength = .9f;
    o.doubleSided = false;
    return result;
}
struct Draw {
    MeshHandle mesh{3, 2};
    TextureHandle texture{4, 5};
    glm::mat4 model{1};
    glm::vec4 tint{1};
    unsigned lod = 2;
    bool reversed = false;
    std::vector<MaterialBinding> bindings{FullBinding(), FullBinding(1)};
    bool Add(JudasRender::SubmissionQueue &queue, bool ordered = false) const {
        return ordered ? queue.AddOrdered(mesh, texture, model, tint, lod, reversed, bindings)
                       : queue.Add(mesh, texture, model, tint, lod, reversed, bindings);
    }
};
void Different(const std::string &name, const std::function<void(Draw &)> &change) {
    for (bool ordered : {false, true}) {
        JudasRender::SubmissionQueue queue;
        Draw original, changed;
        original.Add(queue, ordered);
        change(changed);
        changed.Add(queue, ordered);
        Check(queue.Batches().size() == 2 && queue.Batches()[0].instances.size() == 1 &&
                  queue.Batches()[1].instances.size() == 1,
              name + (ordered ? " [ordered]" : " [opaque]") +
                  " splits real output batches rather than aliasing another material/draw state");
    }
}
void BindingDifferences(const std::string &name, const std::function<void(MaterialBinding &)> &change) {
    for (unsigned slot = 0; slot < 2; ++slot)
        Different(name + " in material slot " + std::to_string(slot),
                  [=](Draw &draw) { change(draw.bindings[slot]); });
}
bool SameMatrix(const glm::mat4 &a, const glm::mat4 &b) {
    for (unsigned c = 0; c < 4; ++c)
        if (a[c] != b[c])
            return false;
    return true;
}
} // namespace

int main() {
    Different("mesh identity", [](Draw &d) { ++d.mesh.id; });
    Different("mesh generation", [](Draw &d) { ++d.mesh.generation; });
    Different("whole-draw texture identity", [](Draw &d) { ++d.texture.id; });
    Different("whole-draw texture generation", [](Draw &d) { ++d.texture.generation; });
    Different("render LOD", [](Draw &d) { ++d.lod; });
    Different("mirrored winding", [](Draw &d) { d.reversed = true; });
    Different("material slot count", [](Draw &d) { d.bindings.pop_back(); });
    Different("material slot order", [](Draw &d) { std::swap(d.bindings[0], d.bindings[1]); });
    Different("an additional otherwise identical slot",
              [](Draw &d) { d.bindings.push_back(d.bindings.back()); });
    BindingDifferences("material resource identity", [](auto &b) { ++b.handle.id; });
    BindingDifferences("material resource generation", [](auto &b) { ++b.handle.generation; });
    BindingDifferences("explicit-asset ownership state", [](auto &b) { b.explicitAsset = false; });
    BindingDifferences("failed-material state", [](auto &b) { b.failed = true; });
    for (unsigned map = 0; map < 5; ++map) {
        BindingDifferences("map " + std::to_string(map) + " resource identity",
                           [=](auto &b) { ++b.textures[map].id; });
        BindingDifferences("map " + std::to_string(map) + " resource generation",
                           [=](auto &b) { ++b.textures[map].generation; });
        BindingDifferences("map " + std::to_string(map) + " override-enable flag",
                           [=](auto &b) { b.textureOverrides[map] = !b.textureOverrides[map]; });
        BindingDifferences("map " + std::to_string(map) + " authored asset override",
                           [=](auto &b) { b.overrides.textures[map] = std::string(32, 'a'); });
        BindingDifferences("map " + std::to_string(map) + " explicit-removal override",
                           [=](auto &b) { b.overrides.textures[map] = std::string{}; });
        BindingDifferences("map " + std::to_string(map) + " absent override",
                           [=](auto &b) { b.overrides.textures[map].reset(); });
    }
    for (unsigned component = 0; component < 4; ++component)
        BindingDifferences("base-colour component " + std::to_string(component),
                           [=](auto &b) { (*b.overrides.baseColor)[component] += .05f; });
    BindingDifferences("base-colour presence", [](auto &b) { b.overrides.baseColor.reset(); });
    for (unsigned component = 0; component < 3; ++component)
        BindingDifferences("emissive component " + std::to_string(component),
                           [=](auto &b) { (*b.overrides.emissive)[component] += .05f; });
    BindingDifferences("emissive presence", [](auto &b) { b.overrides.emissive.reset(); });
    for (unsigned component = 0; component < 2; ++component) {
        BindingDifferences("UV scale component " + std::to_string(component),
                           [=](auto &b) { (*b.overrides.uvScale)[component] += .05f; });
        BindingDifferences("UV offset component " + std::to_string(component),
                           [=](auto &b) { (*b.overrides.uvOffset)[component] += .05f; });
    }
    BindingDifferences("UV-scale presence", [](auto &b) { b.overrides.uvScale.reset(); });
    BindingDifferences("UV-offset presence", [](auto &b) { b.overrides.uvOffset.reset(); });
    using Scalar = std::optional<float> MaterialOverride::*;
    for (const auto &entry : std::vector<std::pair<const char *, Scalar>>{
             {"metallic", &MaterialOverride::metallic},
             {"roughness", &MaterialOverride::roughness},
             {"emissive intensity", &MaterialOverride::emissiveIntensity},
             {"alpha cutoff", &MaterialOverride::alphaCutoff},
             {"normal strength", &MaterialOverride::normalStrength},
             {"occlusion strength", &MaterialOverride::occlusionStrength}}) {
        const auto *name = entry.first;
        const auto member = entry.second;
        BindingDifferences(std::string(name) + " value",
                           [=](auto &b) { *(b.overrides.*member) += .05f; });
        BindingDifferences(std::string(name) + " presence",
                           [=](auto &b) { (b.overrides.*member).reset(); });
    }
    BindingDifferences("masked alpha mode", [](auto &b) { b.overrides.alpha = MaterialAlpha::Mask; });
    BindingDifferences("blended alpha mode", [](auto &b) { b.overrides.alpha = MaterialAlpha::Blend; });
    BindingDifferences("alpha-mode presence", [](auto &b) { b.overrides.alpha.reset(); });
    BindingDifferences("double-sided value", [](auto &b) { b.overrides.doubleSided = true; });
    BindingDifferences("explicit false versus absent double-sided",
                       [](auto &b) { b.overrides.doubleSided.reset(); });

    {
        JudasRender::SubmissionQueue queue;
        Draw first, second;
        first.model = glm::translate(glm::mat4(1), {2, 3, 4});
        first.tint = {.1f, .2f, .3f, .4f};
        second.model = glm::rotate(glm::translate(glm::mat4(1), {-5, 6, 7}), .7f, glm::vec3(0, 1, 0));
        second.model = glm::scale(second.model, {.5f, 2, 1.5f});
        second.tint = {.9f, .8f, .7f, .6f};
        first.Add(queue);
        second.Add(queue);
        const auto &batches = queue.Batches();
        Check(batches.size() == 1 && batches[0].instances.size() == 2,
              "different translations, rotations, positive nonuniform scales and tint remain "
              "compatible instances");
        if (batches.size() == 1 && batches[0].instances.size() == 2) {
            Check(
                SameMatrix(batches[0].instances[0].model, first.model) &&
                    SameMatrix(batches[0].instances[1].model, second.model) &&
                    batches[0].instances[0].tint == first.tint &&
                    batches[0].instances[1].tint == second.tint,
                "coalescing retains both exact model/tint payloads in their original submission order");
        }
        first.bindings[0].handle.id = 999;
        first.bindings[0].overrides.baseColor = glm::vec4(.9f);
        first.bindings[0].overrides.textures[0] = std::string(32, 'f');
        Check(!batches.empty() && !batches[0].bindings.empty() &&
                  batches[0].bindings[0].handle.id == 7 &&
                  batches[0].bindings[0].overrides.baseColor == FullBinding().overrides.baseColor &&
                  batches[0].bindings[0].overrides.textures[0] == FullBinding().overrides.textures[0],
              "batch owns a copied material snapshot rather than a mutable caller reference");
        queue.Clear();
        Check(queue.Batches().empty(), "Clear retires the pass's records and compatibility lookup");
        first.Add(queue);
        Check(
            queue.Batches().size() == 1 && queue.Batches()[0].bindings[0].handle.id == 999 &&
                queue.Batches()[0].instances.size() == 1,
            "a subsequent pass observes changed caller state without resurrecting previous instances");
    }
    {
        JudasRender::SubmissionQueue queue;
        Draw first, second;
        second.lod = 3;
        first.Add(queue);
        second.Add(queue);
        first.Add(queue);
        Check(queue.Batches().size() == 2 && queue.Batches()[0].lod == 2 &&
                  queue.Batches()[1].lod == 3 && queue.Batches()[0].instances.size() == 2 &&
                  queue.Batches()[1].instances.size() == 1,
              "nonadjacent compatible opaque submissions coalesce while first-seen group order remains "
              "stable");
    }
    {
        JudasRender::SubmissionQueue queue;
        Draw first, other;
        other.lod = 3;
        bool earlyFlush = false;
        for (unsigned i = 0; i < 16383; ++i)
            earlyFlush |= first.Add(queue);
        earlyFlush |= other.Add(queue); // Total pass count is now 16,384, but neither group is full.
        Check(!earlyFlush, "existing 16,384-instance flush boundary applies per compatible group, not "
                           "total pass count");
        Check(first.Add(queue) && queue.Batches()[0].instances.size() == 16384 &&
                  queue.Batches()[1].instances.size() == 1,
              "the exact per-group boundary requests the coordinator's existing bounded flush");
        queue.Clear();
        Check(!first.Add(queue) && queue.Batches().size() == 1 &&
                  queue.Batches()[0].instances.size() == 1,
              "clearing after a flush resets both group count and flush eligibility");
    }
    {
        JudasRender::SubmissionQueue queue;
        Draw empty;
        empty.bindings.clear();
        empty.Add(queue);
        empty.Add(queue);
        Check(queue.Batches().size() == 1 && queue.Batches()[0].bindings.empty() &&
                  queue.Batches()[0].instances.size() == 2,
              "ordinary default draws with no material slots remain a compatible fast-path control");
    }
    {
        JudasRender::SubmissionQueue queue;
        Draw a, b, a2;
        b.bindings[1].overrides.roughness = .8f;
        a.model = glm::translate(glm::mat4(1), {1, 2, 3});
        a.tint = {.1f, .2f, .3f, .4f};
        a2.model = glm::scale(glm::mat4(1), {.5f, 2, 1.5f});
        a2.tint = {.8f, .7f, .6f, .5f};
        a.Add(queue, true);
        b.Add(queue, true);
        a2.Add(queue, true);
        const auto &batches = queue.Batches();
        Check(batches.size() == 3 && batches[0].instances.size() == 1 &&
                  batches[1].instances.size() == 1 && batches[2].instances.size() == 1,
              "ordered A/B/A cannot regroup the final A across a distinct material's alpha paint "
              "barrier");
        Check(batches.size() == 3 && SameMatrix(batches[0].instances[0].model, a.model) &&
                  SameMatrix(batches[2].instances[0].model, a2.model) &&
                  batches[0].instances[0].tint == a.tint && batches[2].instances[0].tint == a2.tint &&
                  batches[1].bindings[1].overrides.roughness == b.bindings[1].overrides.roughness,
              "ordered alpha groups preserve exact transform, tint and independent material payloads");
        queue.Clear();
        a.Add(queue, true);
        a2.Add(queue, true);
        b.Add(queue, true);
        b.Add(queue, true);
        a.Add(queue, true);
        a2.Add(queue, true);
        Check(queue.Batches().size() == 3 && queue.Batches()[0].instances.size() == 2 &&
                  queue.Batches()[1].instances.size() == 2 && queue.Batches()[2].instances.size() == 2,
              "ordered A/A/B/B/A/A merges adjacent compatible runs and preserves all three run "
              "boundaries");
        queue.Clear();
        Check(!a.Add(queue, true) && queue.Batches().size() == 1 &&
                  queue.Batches()[0].instances.size() == 1,
              "Clear retires the previous ordered tail rather than reusing a nonexistent last batch");
    }
    {
        JudasRender::SubmissionQueue queue;
        Draw a, b;
        b.texture.generation++;
        a.Add(queue, true);
        b.Add(queue);
        a.Add(queue, true);
        Check(queue.Batches().size() == 3 && queue.Batches()[0].instances.size() == 1 &&
                  queue.Batches()[1].instances.size() == 1 &&
                  queue.Batches()[2].instances.size() == 1 &&
                  queue.Batches()[2].texture.generation == a.texture.generation,
              "an intervening opaque call cannot make ordered A append its payload to incompatible B");
        queue.Clear();
        a.Add(queue);
        b.Add(queue, true);
        a.Add(queue);
        Check(queue.Batches().size() == 3 && queue.Batches()[0].instances.size() == 1 &&
                  queue.Batches()[1].instances.size() == 1 && queue.Batches()[2].instances.size() == 1,
              "an ordered paint barrier invalidates earlier opaque regrouping eligibility");
    }
    {
        JudasRender::SubmissionQueue queue;
        Draw a, b;
        b.lod++;
        bool earlyFlush = false;
        for (unsigned i = 0; i < 8192; ++i)
            earlyFlush |= a.Add(queue, true);
        earlyFlush |= b.Add(queue, true);
        for (unsigned i = 0; i < 8192; ++i)
            earlyFlush |= a.Add(queue, true);
        Check(
            !earlyFlush && queue.Batches().size() == 3 && queue.Batches()[0].instances.size() == 8192 &&
                queue.Batches()[1].instances.size() == 1 && queue.Batches()[2].instances.size() == 8192,
            "ordered run limits never add compatible instances across the intervening B barrier");
        queue.Clear();
        for (unsigned i = 0; i < 16383; ++i)
            earlyFlush |= a.Add(queue, true);
        Check(!earlyFlush && a.Add(queue, true) && queue.Batches().size() == 1 &&
                  queue.Batches()[0].instances.size() == 16384,
              "a single ordered run requests a flush at the same existing 16,384-instance boundary");
    }
    std::printf("Submission queue: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
