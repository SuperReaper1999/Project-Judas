#include "SubmissionQueue.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <tuple>

namespace JudasRender {
namespace {
struct BindingKey {
    std::array<std::uint32_t, 64> values{};
    std::array<std::optional<std::string>, 5> textures;
    bool operator<(const BindingKey& b) const {
        // Exact equality matters; key ordering never chooses draw order. A byte
        // comparison avoids dozens of scalar comparisons of mostly empty fields.
        const int order=std::memcmp(values.data(),b.values.data(),sizeof(values));
        return order ? order<0 : textures<b.textures;
    }
};
struct BatchKey {
    std::array<unsigned, 6> identity;
    std::vector<BindingKey> bindings;
    bool operator<(const BatchKey& b) const { return std::tie(identity, bindings) < std::tie(b.identity, b.bindings); }
};
std::uint32_t Bits(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
struct KeyWriter {
    BindingKey key;
    std::size_t at = 0;
    void Add(float value) { key.values[at++] = Bits(value); }
    void Add(unsigned value) { key.values[at++] = value; }
    void Add(bool value) { Add(unsigned(value)); }
    void Add(MaterialAlpha value) { Add(unsigned(value)); }
    template<glm::length_t N, glm::qualifier Q>
    void Add(const glm::vec<N, float, Q>& value) { for (glm::length_t i = 0; i < N; ++i) Add(value[i]); }
    template<class T> void Optional(const std::optional<T>& value) {
        Add(value.has_value());
        if (value) Add(*value);
    }
};
BindingKey Key(const MaterialBinding& binding) {
    KeyWriter k;
    k.Add(binding.handle.id); k.Add(binding.handle.generation);
    k.Add(binding.explicitAsset); k.Add(binding.failed);
    for (unsigned i = 0; i < 5; ++i) {
        k.Add(binding.textures[i].id); k.Add(binding.textures[i].generation);
        k.Add(binding.textureOverrides[i]);
    }
    const auto& o = binding.overrides;
    k.Optional(o.baseColor); k.Optional(o.metallic); k.Optional(o.roughness);
    k.Optional(o.emissiveIntensity); k.Optional(o.emissive); k.Optional(o.uvScale);
    k.Optional(o.uvOffset); k.Optional(o.alpha); k.Optional(o.alphaCutoff);
    k.Optional(o.normalStrength); k.Optional(o.occlusionStrength); k.Optional(o.doubleSided);
    k.key.textures = o.textures;
    return std::move(k.key);
}
} // namespace
struct SubmissionQueue::Storage {
    std::vector<OpaqueBatch> batches;
    std::map<BatchKey, std::size_t> lookup;
    std::optional<BatchKey> lastOrdered;
};
SubmissionQueue::SubmissionQueue() : storage(std::make_unique<Storage>()) {}
SubmissionQueue::~SubmissionQueue() = default;
bool SubmissionQueue::Add(MeshHandle mesh, TextureHandle texture, const glm::mat4& model,
                           glm::vec4 tint, unsigned lod, bool reversed,
                           const std::vector<MaterialBinding>& bindings) {
    storage->lastOrdered.reset();
    BatchKey key{{mesh.id, mesh.generation, texture.id, texture.generation, lod, unsigned(reversed)}, {}};
    key.bindings.reserve(bindings.size());
    for (const auto& binding : bindings) key.bindings.push_back(Key(binding));
    auto [it, inserted] = storage->lookup.emplace(std::move(key), storage->batches.size());
    if (inserted) storage->batches.push_back({mesh, texture, bindings, reversed, lod, {}});
    auto& instances = storage->batches[it->second].instances;
    instances.push_back({model, tint});
    return instances.size() == 16384;
}
bool SubmissionQueue::AddOrdered(MeshHandle mesh, TextureHandle texture, const glm::mat4& model,
                                glm::vec4 tint, unsigned lod, bool reversed,
                                const std::vector<MaterialBinding>& bindings) {
    storage->lookup.clear(); // Ordered submissions form a barrier to global regrouping.
    BatchKey key{{mesh.id,mesh.generation,texture.id,texture.generation,lod,unsigned(reversed)}, {}};
    key.bindings.reserve(bindings.size());
    for(const auto& binding:bindings)key.bindings.push_back(Key(binding));
    if(!storage->lastOrdered || key<*storage->lastOrdered || *storage->lastOrdered<key)
        storage->batches.push_back({mesh,texture,bindings,reversed,lod,{}});
    storage->lastOrdered=std::move(key);
    auto& instances=storage->batches.back().instances;
    instances.push_back({model,tint});
    return instances.size()==16384;
}
const std::vector<OpaqueBatch>& SubmissionQueue::Batches() const { return storage->batches; }
void SubmissionQueue::Clear() { storage->batches.clear(); storage->lookup.clear(); storage->lastOrdered.reset(); }
} // namespace JudasRender
