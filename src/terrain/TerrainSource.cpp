#include "TerrainSource.h"
#include "../../third_party/nlohmann/json.hpp"
#include "TerrainMetadata.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace TerrainV2 {
namespace {
bool Finite(glm::vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
struct Cell {
    unsigned x, z;
    float u, v;
};
Cell Locate(const Source &s, float x, float z) {
    const float gx =
        std::clamp(x / s.spacing + float(s.samplesX - 1) * .5f, 0.f, float(s.samplesX - 1));
    const float gz =
        std::clamp(z / s.spacing + float(s.samplesZ - 1) * .5f, 0.f, float(s.samplesZ - 1));
    const auto ix = std::min(unsigned(gx), s.samplesX - 2),
               iz = std::min(unsigned(gz), s.samplesZ - 2);
    return {ix, iz, gx - ix, gz - iz};
}
} // namespace
Source Create(unsigned nx, unsigned nz, float spacing, unsigned cells) {
    Source s;
    s.samplesX = nx;
    s.samplesZ = nz;
    s.spacing = spacing;
    s.chunkCells = cells;
    if (nx < 3 || nz < 3 || uint64_t(nx) * nz > 4194304)
        throw std::invalid_argument("terrain sample budget");
    s.heights.resize(size_t(nx) * nz);
    s.weights.resize(s.heights.size(), glm::vec4(1, 0, 0, 0));
    std::string error;
    if (!Validate(s, error))
        throw std::invalid_argument(error);
    return s;
}
bool ValidateMetadata(const Source &s, std::string &error) {
    auto fail = [&](const char *text) {
        error = text;
        return false;
    };
    if (s.version != 2 || s.samplesX < 3 || s.samplesZ < 3 || s.samplesX > 4097 ||
        s.samplesZ > 4097 || uint64_t(s.samplesX) * s.samplesZ > 4194304 || s.chunkCells < 4 ||
        s.chunkCells > 64 || (s.samplesX - 1) % s.chunkCells || (s.samplesZ - 1) % s.chunkCells)
        return fail("terrain V2 requires aligned 4..64-cell chunks and <=4194304 samples");
    if (!std::isfinite(s.spacing) || s.spacing < .125f || s.spacing > 128 ||
        !std::isfinite(s.bottom) || !std::isfinite(s.top) || s.top <= s.bottom ||
        std::max(std::abs(s.top), std::abs(s.bottom)) > 100000)
        return fail("terrain spacing/vertical excavation budget exceeded");
    for (unsigned i = 0; i < 4; ++i) {
        if (s.layerTextures[i].size() > 128 || !Finite(s.layerColours[i]) ||
            glm::any(glm::lessThan(s.layerColours[i], glm::vec3(0))) ||
            glm::any(glm::greaterThan(s.layerColours[i], glm::vec3(1))))
            return fail("invalid terrain material layer");
    }
    if (s.edits.size() > 4096)
        return fail("terrain edit journal budget exceeded");
    for (const auto &e : s.edits)
        if (!Finite(e.point) || !std::isfinite(e.radius) || e.radius < s.spacing || e.radius > 64 ||
            (e.kind != Edit::Kind::Add && e.kind != Edit::Kind::Remove))
            return fail(
                "terrain brush requires finite point and radius between spacing and 64 metres");
    error.clear();
    return true;
}
bool Validate(const Source &s, std::string &error) {
    if (!ValidateMetadata(s, error))
        return false;
    auto fail = [&](const char *text) {
        error = text;
        return false;
    };
    if (s.heights.size() != size_t(s.samplesX) * s.samplesZ || s.weights.size() != s.heights.size())
        return fail("terrain height/material sample count mismatch");
    for (size_t i = 0; i < s.heights.size(); ++i) {
        if (!std::isfinite(s.heights[i]) || s.heights[i] <= s.bottom || s.heights[i] >= s.top)
            return fail("terrain height outside excavation bounds");
        const auto w = s.weights[i];
        if (!std::isfinite(w.x + w.y + w.z + w.w) || glm::any(glm::lessThan(w, glm::vec4(0))) ||
            std::abs(w.x + w.y + w.z + w.w - 1) > 1e-4f)
            return fail("invalid terrain material weights");
    }
    error.clear();
    return true;
}
float Height(const Source &s, float x, float z) {
    const auto c = Locate(s, x, z);
    auto at = [&](unsigned dx, unsigned dz) {
        return s.heights[size_t(c.z + dz) * s.samplesX + c.x + dx];
    };
    // Keep the authored V1 10--01 diagonal. The chunk tetrahedra use
    // the same face topology, so conversion does not change the surface.
    return c.u + c.v <= 1
               ? at(0, 0) * (1 - c.u - c.v) + at(1, 0) * c.u + at(0, 1) * c.v
               : at(1, 1) * (c.u + c.v - 1) + at(1, 0) * (1 - c.v) + at(0, 1) * (1 - c.u);
}
glm::vec4 Weights(const Source &s, float x, float z) {
    const auto c = Locate(s, x, z);
    auto at = [&](unsigned dx, unsigned dz) {
        return s.weights[size_t(c.z + dz) * s.samplesX + c.x + dx];
    };
    return glm::mix(glm::mix(at(0, 0), at(1, 0), c.u), glm::mix(at(0, 1), at(1, 1), c.u), c.v);
}
float Density(const Source &s, glm::vec3 p) {
    float d = p.y - Height(s, p.x, p.z);
    for (const auto &e : s.edits) {
        const float sphere = glm::length(p - e.point) - e.radius;
        d = e.kind == Edit::Kind::Remove ? std::max(d, -sphere) : std::min(d, sphere);
    }
    return d;
}
bool Touches(const Source &s, ChunkKey key, const Edit &e) {
    const float width = s.chunkCells * s.spacing;
    const glm::vec2 lo(key.x * width - (s.samplesX - 1) * s.spacing * .5f,
                       key.z * width - (s.samplesZ - 1) * s.spacing * .5f),
        hi = lo + glm::vec2(width);
    const auto point = glm::vec2(e.point.x, e.point.z);
    const auto delta = glm::max(glm::max(lo - point, point - hi), glm::vec2(0));
    return glm::length(delta) <= e.radius + s.spacing;
}
std::vector<ChunkKey> Affected(const Source &s, const Edit &e) {
    std::vector<ChunkKey> out;
    const double width = double(s.chunkCells) * s.spacing, radius = e.radius + s.spacing;
    auto range = [&](double point, unsigned samples, unsigned count) {
        const double half = (samples - 1) * double(s.spacing) * .5;
        return std::pair<int, int>{
            int(std::clamp(std::floor((point - radius + half) / width) - 1, 0., double(count))),
            int(std::clamp(std::floor((point + radius + half) / width)+1, -1., double(count) - 1))};
    };
    auto xr = range(e.point.x, s.samplesX, s.ChunksX()),
         zr = range(e.point.z, s.samplesZ, s.ChunksZ());
    // Preserve the original exact halo test and iteration order; only avoid
    // scanning unrelated chunks (and allocating their keys for every brush).
    for (int z = zr.first; z <= zr.second; ++z)
        for (int x = xr.first; x <= xr.second; ++x) {
            ChunkKey key{unsigned(x), unsigned(z)};
            if (Touches(s, key, e))
                out.push_back(key);
        }
    return out;
}
namespace {
using Json = nlohmann::json;
Json MetadataJson(const Source &s) {
    Json j = {{"format", "JudasTerrainV2"}, {"version", 2},         {"samplesX", s.samplesX},
              {"samplesZ", s.samplesZ},     {"spacing", s.spacing}, {"chunkCells", s.chunkCells},
              {"bottom", s.bottom},         {"top", s.top},         {"edits", Json::array()}};
    for (auto e : s.edits)
        j["edits"].push_back({{"kind", e.kind == Edit::Kind::Remove ? "remove" : "add"},
                              {"point", {e.point.x, e.point.y, e.point.z}},
                              {"radius", e.radius}});
    j["layerTextures"] = s.layerTextures;
    j["layerColours"] = Json::array();
    for (auto c : s.layerColours)
        j["layerColours"].push_back({c.x, c.y, c.z});
    return j;
}
Json ReadJson(const std::string &bytes) {
    if (bytes.size() > 64 * 1024 * 1024)
        throw std::invalid_argument("terrain source byte budget");
    return Json::parse(bytes, [](int depth, Json::parse_event_t, const Json &) {
        if (depth > 10)
            throw std::invalid_argument("terrain nesting budget");
        return true;
    });
}
Source ReadMetadata(const Json &j) {
    if (j.at("format") != "JudasTerrainV2" || j.at("version") != 2)
        throw std::invalid_argument("terrain format/version");
    for (const auto *key : {"version", "samplesX", "samplesZ", "chunkCells"})
        if (!j.at(key).is_number_integer() || j.at(key).get<int64_t>() < 0 ||
            j.at(key).get<uint64_t>() > 4097)
            throw std::invalid_argument("terrain integer field");
    Source s;
    s.samplesX = j.at("samplesX");
    s.samplesZ = j.at("samplesZ");
    s.chunkCells = j.at("chunkCells");
    s.spacing = j.at("spacing");
    s.bottom = j.at("bottom");
    s.top = j.at("top");
    if (j.at("edits").size() > 4096)
        throw std::invalid_argument("terrain edit journal budget");
    for (const auto &e : j.at("edits")) {
        Edit edit;
        const auto kind = e.at("kind").get<std::string>();
        if (kind != "remove" && kind != "add")
            throw std::invalid_argument("terrain edit kind");
        edit.kind = kind == "remove" ? Edit::Kind::Remove : Edit::Kind::Add;
        const auto &p = e.at("point");
        if (p.size() != 3)
            throw std::invalid_argument("terrain edit point");
        edit.point = {p.at(0), p.at(1), p.at(2)};
        edit.radius = e.at("radius");
        s.edits.push_back(edit);
    }
    if (j.contains("layerTextures")) {
        if (j["layerTextures"].size() != 4)
            throw std::invalid_argument("terrain layers");
        s.layerTextures = j["layerTextures"].get<std::array<std::string, 4>>();
    }
    if (j.contains("layerColours")) {
        if (j["layerColours"].size() != 4)
            throw std::invalid_argument("terrain colours");
        for (unsigned i = 0; i < 4; ++i) {
            auto c = j["layerColours"].at(i);
            if (c.size() != 3)
                throw std::invalid_argument("terrain colour");
            s.layerColours[i] = {c.at(0), c.at(1), c.at(2)};
        }
    }
    std::string error;
    if (!ValidateMetadata(s, error))
        throw std::invalid_argument(error);
    return s;
}
} // namespace
std::string SerializeMetadata(const Source &s) {
    std::string error;
    if (!ValidateMetadata(s, error))
        throw std::invalid_argument(error);
    return MetadataJson(s).dump();
}
bool ParseMetadata(const std::string &bytes, Source &output, std::string &error) {
    try {
        auto s = ReadMetadata(ReadJson(bytes));
        output = std::move(s);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
std::string Serialize(const Source &s) {
    std::string error;
    if (!Validate(s, error))
        throw std::invalid_argument(error);
    auto j = MetadataJson(s);
    j["heights"] = s.heights;
    j["weights"] = Json::array();
    for (auto w : s.weights)
        j["weights"].push_back({w.x, w.y, w.z, w.w});
    return j.dump();
}
bool Parse(const std::string &bytes, Source &output, std::string &error) {
    try {
        auto j = ReadJson(bytes);
        auto s = ReadMetadata(j);
        if (j.at("heights").size() != uint64_t(s.samplesX) * s.samplesZ ||
            j.at("weights").size() != j.at("heights").size())
            throw std::invalid_argument("terrain source sample budget");
        s.heights = j.at("heights").get<std::vector<float>>();
        for (const auto &w : j.at("weights")) {
            if (w.size() != 4)
                throw std::invalid_argument("terrain weight dimensions");
            s.weights.emplace_back(w.at(0), w.at(1), w.at(2), w.at(3));
        }
        if (!Validate(s, error))
            return false;
        output = std::move(s);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
} // namespace TerrainV2
