#include "TerrainGeometrySpill.h"
#include "TerrainGeometryPages.h"
#include "TerrainTemporary.h"
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace TerrainV2 {
namespace {
uint32_t Checksum(const std::string &bytes) {
    static const auto table = [] {
        std::array<uint32_t, 256> result{};
        for (uint32_t i = 0; i < 256; ++i) {
            auto c = i;
            for (unsigned bit = 0; bit < 8; ++bit)
                c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0u);
            result[i] = c;
        }
        return result;
    }();
    uint32_t crc = ~0u;
    for (unsigned char byte : bytes)
        crc = table[(crc ^ byte) & 255] ^ (crc >> 8);
    return ~crc;
}
template <class T> void Append(std::string &bytes, const std::vector<T> &values) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (!values.empty())
        bytes.append(reinterpret_cast<const char *>(values.data()), values.size() * sizeof(T));
}
template <class T>
void Read(std::string_view bytes, size_t &offset, size_t count, std::vector<T> &out) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (count > (bytes.size() - offset) / sizeof(T))
        throw std::runtime_error("terrain derived page block bounds");
    out.resize(count);
    if (count)
        std::memcpy(out.data(), bytes.data() + offset, count * sizeof(T));
    offset += count * sizeof(T);
}
} // namespace
GeometrySpill::~GeometrySpill() {
    if (path.empty())
        return;
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::lock_guard<std::mutex> lock(state->mutex);
    state->bytes -= bytes;
}
std::shared_ptr<const GeometrySpill>
GeometrySpill::Store(const GeometryPage &page, uint64_t identity,
                     std::shared_ptr<GeometrySpillState> state) {
    const auto &c = page.collision;
    if (c.convex || !c.polygons.empty() || !c.edges.empty() || !c.satEdges.empty())
        throw std::runtime_error("terrain derived page requires triangle collision");
    auto spill = std::make_shared<GeometrySpill>();
    spill->state = std::move(state);
    auto header = std::make_shared<GeometryPage>();
    header->chunk.key = page.chunk.key;
    header->chunk.anchor = page.chunk.anchor;
    header->chunk.sampledBytes = page.chunk.sampledBytes;
    header->chunk.volumetric = page.chunk.volumetric;
    header->triangles = page.triangles;
    header->chunk.mesh.primitives = page.chunk.mesh.primitives;
    auto &h = header->collision;
    h.twoSided = c.twoSided;
    h.weldTolerance = c.weldTolerance;
    h.sourceAsset = c.sourceAsset;
    h.sourceFingerprint = c.sourceFingerprint;
    h.settingsFingerprint = c.settingsFingerprint;
    h.selectedPrimitive = c.selectedPrimitive;
    h.sourceTransform = c.sourceTransform;
    h.minimum = c.minimum;
    h.maximum = c.maximum;
    h.centerOfMass = c.centerOfMass;
    h.volume = c.volume;
    h.unitInertia = c.unitInertia;
    h.warnings = c.warnings;
    spill->header = std::move(header);
    spill->counts[0] = c.vertices.size();
    spill->counts[1] = c.faces.size();
    spill->counts[2] = c.nodes.size();
    spill->counts[3] = c.order.size();
    spill->counts[4] = page.chunk.mesh.vertices.size();
    spill->counts[5] = page.chunk.mesh.indices.size();
    std::string payload;
    Append(payload, c.vertices);
    Append(payload, c.faces);
    Append(payload, c.nodes);
    Append(payload, c.order);
    Append(payload, page.chunk.mesh.vertices);
    Append(payload, page.chunk.mesh.indices);
    if (payload.size() > 32 * 1024 * 1024)
        throw std::runtime_error("terrain derived page exceeds 32 MiB");
    spill->checksum = Checksum(payload);
    {
        std::lock_guard<std::mutex> lock(spill->state->mutex);
        if (payload.size() > GeometrySpillState::budget - spill->state->bytes)
            throw std::runtime_error("terrain derived spill exceeds its 512 MiB disk budget");
        if (!spill->state->directory)
            spill->state->directory = std::make_shared<TemporaryDirectory>("judas-terrain-cooks");
        spill->path =
            (spill->state->directory->path / (std::to_string(identity) + ".page")).string();
        spill->bytes = payload.size();
        spill->state->bytes += spill->bytes;
    }
    std::ofstream file(spill->path, std::ios::binary | std::ios::trunc);
    file.write(payload.data(), payload.size());
    file.flush();
    if (!file)
        throw std::runtime_error("cannot persist prepared terrain geometry page");
    return spill;
}
std::shared_ptr<const GeometryPage> GeometrySpill::Load() const {
    std::ifstream file(path, std::ios::binary);
    std::string payload(bytes, '\0');
    if (!file.read(payload.data(), payload.size()) ||
        file.peek() != std::char_traits<char>::eof() || Checksum(payload) != checksum)
        throw std::runtime_error("terrain derived geometry page is missing, truncated or corrupt");
    auto page = std::make_shared<GeometryPage>(*header);
    size_t offset = 0;
    Read(payload, offset, counts[0], page->collision.vertices);
    Read(payload, offset, counts[1], page->collision.faces);
    Read(payload, offset, counts[2], page->collision.nodes);
    Read(payload, offset, counts[3], page->collision.order);
    Read(payload, offset, counts[4], page->chunk.mesh.vertices);
    Read(payload, offset, counts[5], page->chunk.mesh.indices);
    if (offset != payload.size())
        throw std::runtime_error("terrain derived page has trailing data");
    return page;
}
} // namespace TerrainV2
