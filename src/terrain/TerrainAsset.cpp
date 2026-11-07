#include "TerrainAsset.h"
#include "SceneFingerprint.h"
#include "TerrainMetadata.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace TerrainV2 {
namespace {
constexpr std::string_view Magic("JTERR2\0\0", 8);
constexpr uint32_t Format = 1, PageSide = 64, Prefix = 24, Header = 88, Entry = 92;
constexpr uint64_t MaximumBytes = 96ull * 1024 * 1024;
struct Cursor {
    std::string_view bytes;
    size_t p = 0;
    std::string_view Take(size_t n) {
        if (n > bytes.size() - p)
            throw std::runtime_error("truncated terrain asset");
        auto s = bytes.substr(p, n);
        p += n;
        return s;
    }
    uint64_t Integer(unsigned n) {
        auto s = Take(n);
        uint64_t v = 0;
        for (unsigned i = 0; i < n; ++i)
            v |= uint64_t(uint8_t(s[i])) << (8 * i);
        return v;
    }
};
void Put(std::vector<uint8_t> &out, size_t &p, uint64_t v, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        out.at(p++) = uint8_t(v >> (8 * i));
}
void PutFloat(std::vector<uint8_t> &out, size_t &p, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    Put(out, p, bits, 4);
}
float GetFloat(Cursor &cursor) {
    uint32_t bits = uint32_t(cursor.Integer(4));
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
bool Digest(std::string_view s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string_view::npos;
}
uint64_t HeaderLength(std::string_view bytes) {
    Cursor c{bytes};
    if (c.Take(8) != Magic || c.Integer(4) != Format)
        throw std::runtime_error("terrain asset format/version");
    uint64_t meta = c.Integer(4), side = c.Integer(4), pages = c.Integer(4);
    if (side != PageSide || meta > 2 * 1024 * 1024 || !pages || pages > 4225)
        throw std::runtime_error("terrain asset header budget");
    return Header + meta + pages * Entry;
}
AssetIndex ReadIndex(std::string_view bytes, uint64_t total) {
    if (total > MaximumBytes || total < HeaderLength(bytes))
        throw std::runtime_error("terrain asset byte budget");
    Cursor c{bytes};
    c.Take(12);
    const auto metaSize = c.Integer(4);
    c.Take(4);
    const auto count = c.Integer(4);
    auto digest = c.Take(64), metadata = c.Take(size_t(metaSize));
    if (!Digest(digest) || SceneFingerprintSha256(metadata) != digest)
        throw std::runtime_error("terrain metadata checksum mismatch");
    AssetIndex index;
    index.fingerprint = SceneFingerprintSha256(bytes.substr(0, size_t(HeaderLength(bytes))));
    std::string error;
    if (!ParseMetadata(std::string(metadata), index.metadata, error))
        throw std::runtime_error(error);
    const auto &s = index.metadata;
    const unsigned px = (s.samplesX + PageSide - 1) / PageSide,
                   pz = (s.samplesZ + PageSide - 1) / PageSide;
    if (count != uint64_t(px) * pz)
        throw std::runtime_error("terrain page count does not cover the source");
    uint64_t offset = HeaderLength(bytes);
    for (unsigned z = 0; z < pz; ++z)
        for (unsigned x = 0; x < px; ++x) {
            PageRecord r;
            r.x = unsigned(c.Integer(4));
            r.z = unsigned(c.Integer(4));
            r.width = unsigned(c.Integer(4));
            r.height = unsigned(c.Integer(4));
            r.offset = c.Integer(8);
            r.bytes = uint32_t(c.Integer(4));
            r.digest = c.Take(64);
            const unsigned w = std::min(PageSide, s.samplesX - x * PageSide),
                           h = std::min(PageSide, s.samplesZ - z * PageSide);
            if (r.x != x * PageSide || r.z != z * PageSide || r.width != w || r.height != h ||
                r.bytes != uint64_t(w) * h * 20 || r.offset != offset || !Digest(r.digest))
                throw std::runtime_error(
                    "terrain page index has overlap, gaps or invalid dimensions");
            offset += r.bytes;
            if (offset > total)
                throw std::runtime_error("terrain page exceeds file bounds");
            index.pages.push_back(std::move(r));
        }
    if (offset != total)
        throw std::runtime_error("terrain asset has missing or trailing payload");
    return index;
}
SamplePage ReadSamples(const PageRecord &r, std::string_view bytes, const Source &s) {
    if (bytes.size() != r.bytes || SceneFingerprintSha256(bytes) != r.digest)
        throw std::runtime_error("terrain sample page checksum mismatch");
    SamplePage out;
    out.x = r.x;
    out.z = r.z;
    out.width = r.width;
    out.height = r.height;
    out.heights.reserve(size_t(r.width) * r.height);
    out.weights.reserve(size_t(r.width) * r.height);
    Cursor c{bytes};
    for (unsigned i = 0; i < r.width * r.height; ++i) {
        float h = GetFloat(c);
        glm::vec4 w;
        for (unsigned j = 0; j < 4; ++j)
            w[j] = GetFloat(c);
        if (!std::isfinite(h) || h <= s.bottom || h >= s.top ||
            !std::isfinite(w.x + w.y + w.z + w.w) || glm::any(glm::lessThan(w, glm::vec4(0))) ||
            std::abs(w.x + w.y + w.z + w.w - 1) > 1e-4f)
            throw std::runtime_error("terrain sample page has invalid height/material data");
        out.heights.push_back(h);
        out.weights.push_back(w);
    }
    return out;
}
void Install(Source &out, const SamplePage &p) {
    for (unsigned z = 0; z < p.height; ++z)
        for (unsigned x = 0; x < p.width; ++x) {
            size_t to = size_t(p.z + z) * out.samplesX + p.x + x, from = size_t(z) * p.width + x;
            out.heights[to] = p.heights[from];
            out.weights[to] = p.weights[from];
        }
}
Source Allocate(const AssetIndex &index) {
    auto s = index.metadata;
    s.heights.resize(size_t(s.samplesX) * s.samplesZ);
    s.weights.resize(s.heights.size());
    return s;
}
} // namespace
bool IsAsset(std::string_view bytes) { return bytes.substr(0, 8) == Magic; }
bool EncodeAsset(const Source &source, std::vector<uint8_t> &output, std::string &error) {
    try {
        if (!Validate(source, error))
            return false;
        const auto meta = SerializeMetadata(source);
        const unsigned nx = (source.samplesX + PageSide - 1) / PageSide,
                       nz = (source.samplesZ + PageSide - 1) / PageSide;
        uint64_t offset = Header + meta.size() + uint64_t(nx) * nz * Entry,
                 total = offset + source.heights.size() * 20;
        if (meta.size() > 2 * 1024 * 1024 || total > MaximumBytes)
            throw std::runtime_error("terrain asset byte budget");
        std::vector<uint8_t> bytes(size_t(total), 0);
        size_t header = 0;
        for (char c : Magic)
            bytes.at(header++) = uint8_t(c);
        Put(bytes, header, Format, 4);
        Put(bytes, header, meta.size(), 4);
        Put(bytes, header, PageSide, 4);
        Put(bytes, header, uint64_t(nx) * nz, 4);
        auto digest = SceneFingerprintSha256(meta);
        for (char c : digest)
            bytes.at(header++) = uint8_t(c);
        for (char c : meta)
            bytes.at(header++) = uint8_t(c);
        for (unsigned z = 0; z < nz; ++z)
            for (unsigned x = 0; x < nx; ++x) {
                unsigned sx = x * PageSide, sz = z * PageSide,
                         w = std::min(PageSide, source.samplesX - sx),
                         h = std::min(PageSide, source.samplesZ - sz);
                const uint64_t count = uint64_t(w) * h * 20;
                size_t data = size_t(offset);
                for (unsigned j = 0; j < h; ++j)
                    for (unsigned i = 0; i < w; ++i) {
                        size_t at = size_t(sz + j) * source.samplesX + sx + i;
                        PutFloat(bytes, data, source.heights[at]);
                        for (unsigned k = 0; k < 4; ++k)
                            PutFloat(bytes, data, source.weights[at][k]);
                    }
                Put(bytes, header, sx, 4);
                Put(bytes, header, sz, 4);
                Put(bytes, header, w, 4);
                Put(bytes, header, h, 4);
                Put(bytes, header, offset, 8);
                Put(bytes, header, count, 4);
                auto hash = SceneFingerprintSha256(std::string_view(
                    reinterpret_cast<const char *>(bytes.data() + offset), size_t(count)));
                for (char c : hash)
                    bytes.at(header++) = uint8_t(c);
                offset += count;
            }
        output = std::move(bytes);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool DecodeAsset(std::string_view bytes, Source &output, std::string &error) {
    try {
        auto index = ReadIndex(bytes, bytes.size());
        auto source = Allocate(index);
        for (const auto &r : index.pages)
            Install(source, ReadSamples(r, bytes.substr(size_t(r.offset), r.bytes), source));
        if (!Validate(source, error))
            return false;
        output = std::move(source);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool AssetPages::Open(const std::string &path, std::string &error) {
    try {
        std::ifstream next(path, std::ios::binary | std::ios::ate);
        if (!next || next.tellg() < 0)
            throw std::runtime_error("cannot open terrain asset: " + path);
        auto total = uint64_t(next.tellg());
        if (total > MaximumBytes || total < Prefix)
            throw std::runtime_error("terrain asset byte budget");
        next.seekg(0);
        std::string header(Prefix, '\0');
        next.read(header.data(), header.size());
        if (!next)
            throw std::runtime_error("terrain prefix read failed");
        auto size = HeaderLength(header);
        if (size > total)
            throw std::runtime_error("terrain index exceeds file size");
        header.resize(size_t(size));
        next.read(header.data() + Prefix, std::streamsize(size - Prefix));
        if (!next)
            throw std::runtime_error("terrain index read failed");
        auto decoded = ReadIndex(header, total);
        file = std::move(next);
        index = std::move(decoded);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool AssetPages::ReadPage(size_t i, SamplePage &output, std::string &error) {
    try {
        if (!file.is_open() || i >= index.pages.size())
            throw std::runtime_error("terrain page outside opened asset");
        const auto &r = index.pages[i];
        file.clear();
        file.seekg(std::streamoff(r.offset));
        std::string bytes(r.bytes, '\0');
        file.read(bytes.data(), bytes.size());
        if (!file)
            throw std::runtime_error("terrain page read failed");
        auto page = ReadSamples(r, bytes, index.metadata);
        output = std::move(page);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool AssetPages::ReadAll(Source &output, std::string &error) {
    try {
        if (!file.is_open())
            throw std::runtime_error("terrain asset is not open");
        auto source = Allocate(index);
        for (size_t i = 0; i < index.pages.size(); ++i) {
            SamplePage page;
            if (!ReadPage(i, page, error))
                return false;
            Install(source, page);
        }
        if (!Validate(source, error))
            return false;
        output = std::move(source);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool LoadAsset(const std::string &path, Source &source, std::string &error) {
    AssetPages pages;
    return pages.Open(path, error) && pages.ReadAll(source, error);
}
} // namespace TerrainV2
