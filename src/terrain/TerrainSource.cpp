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
    if (s.version != 2 && s.version != 3)
        return fail(
            "unsupported terrain source format; use2 for ordinary or3 for planetary sources");
    if (bool(s.chart) != (s.version == 3))
        return fail("terrain source3 requires planetChart; source2 must not carry one");
    if (s.samplesX < 3 || s.samplesZ < 3 || s.samplesX > 4097 || s.samplesZ > 4097 ||
        uint64_t(s.samplesX) * s.samplesZ > 4194304 || s.chunkCells < 4 || s.chunkCells > 64 ||
        (s.samplesX - 1) % s.chunkCells || (s.samplesZ - 1) % s.chunkCells)
        return fail("terrain V2 requires aligned 4..64-cell chunks and <=4194304 samples");
    if (!std::isfinite(s.spacing) || s.spacing < .125f || s.spacing > 128 ||
        !std::isfinite(s.bottom) || !std::isfinite(s.top) || s.top <= s.bottom ||
        std::max(std::abs(s.top), std::abs(s.bottom)) > 100000)
        return fail("terrain spacing/vertical excavation budget exceeded");
    if (s.chart) {
        if (!ValidatePlanetChart(*s.chart, error))
            return false;
        if (s.chart->surfaceFingerprint.size() != 64 ||
            s.chart->surfaceFingerprint.find_first_not_of("0123456789abcdef") !=
                std::string::npos ||
            s.chart->rim.size() != 4 * s.samplesX)
            return fail(
                "projected terrain requires a serialized planet source identity and protected rim");
        for (float height : s.chart->rim)
            if (!std::isfinite(height) || height <= s.bottom || height >= s.top)
                return fail("projected terrain rim height is outside source bounds");
        if (s.samplesX != s.samplesZ || s.chart->span != double(s.samplesX - 1) * s.spacing ||
            s.chart->radius + s.bottom <= s.chart->radius * .5 ||
            s.chart->originHeight <= s.bottom || s.chart->originHeight >= s.top)
            return fail("planet terrain metadata requires a square geographic grid and local "
                        "radial bounds");
    }
    for (unsigned i = 0; i < 4; ++i) {
        if (s.layerTextures[i].size() > 128 || !Finite(s.layerColours[i]) ||
            glm::any(glm::lessThan(s.layerColours[i], glm::vec3(0))) ||
            glm::any(glm::greaterThan(s.layerColours[i], glm::vec3(1))))
            return fail("invalid terrain material layer");
        if (!std::isfinite(s.materialDetail.tileMetres[i]) ||
            s.materialDetail.tileMetres[i] < .125f || s.materialDetail.tileMetres[i] > 1024 ||
            !std::isfinite(s.materialDetail.roughness[i]) || s.materialDetail.roughness[i] < 0 ||
            s.materialDetail.roughness[i] > 1)
            return fail("terrain layers require finite 0.125..1024 metre tiles and 0..1 roughness");
    }
    const auto &detail = s.materialDetail;
    if (detail.normalTexture.size() > 128 || !std::isfinite(detail.normalMetres) ||
        detail.normalMetres < .125f || detail.normalMetres > 1024 ||
        !std::isfinite(detail.normalStrength) || detail.normalStrength < 0 ||
        detail.normalStrength > 2)
        return fail("terrain normal detail requires 0.125..1024 metre tiles and 0..2 strength");
    if (s.edits.size() > 4096)
        return fail("terrain edit journal budget exceeded");
    for (const auto &e : s.edits)
        if (!Finite(e.point) || !std::isfinite(e.radius) || e.radius < s.spacing || e.radius > 64 ||
            (e.kind != Edit::Kind::Add && e.kind != Edit::Kind::Remove))
            return fail(
                "terrain brush requires finite point and radius between spacing and 64 metres");
    if (s.chart) {
        const double half = s.chart->span * .5 - 2 * s.spacing;
        for (const auto &edit : s.edits) {
            const auto reach = BrushReach(s, edit.radius);
            if (std::abs(double(edit.point.x)) + reach >= half ||
                std::abs(double(edit.point.z)) + reach >= half)
                return fail(
                    "planet excavation touches its protected rim; create a larger editable patch");
        }
    }
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
    if (s.chart && PlanetSourceRim(s) != s.chart->rim)
        return fail("planet source boundary heights changed; regenerate its geographic patch");
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
double EditDistance(const Source &source, glm::vec3 point, const Edit &edit) {
    if (!source.chart)
        return glm::length(glm::dvec3(point) - glm::dvec3(edit.point)) - double(edit.radius);
    return glm::length(source.chart->LocalPoint(point) - source.chart->LocalPoint(edit.point)) -
           double(edit.radius);
}
double BrushReach(const Source &source, double radius) {
    if (!source.chart)
        return radius;
    const auto &c = *source.chart;
    const double count = double(uint64_t(1) << c.tile.level);
    const double padding = 4 * source.spacing / (c.span * count);
    const double u =
        std::max(std::abs(2 * c.tile.x / count - 1), std::abs(2 * (c.tile.x + 1) / count - 1)) +
        padding;
    const double v =
        std::max(std::abs(2 * c.tile.y / count - 1), std::abs(2 * (c.tile.y + 1) / count - 1)) +
        padding;
    const double scale = c.span * count / (2 * (c.radius + source.bottom));
    // Frobenius upper bound of the inverse radial/cube-face Jacobian across
    // this source. This expands candidates; it never reduces collision coverage.
    return radius * std::sqrt(1 + scale * scale * (1 + u * u + v * v) * (2 + u * u + v * v));
}
bool SourcePoint(const Source &source, glm::vec3 local, glm::vec3 &out, std::string &error) {
    try {
        const auto point = source.chart ? source.chart->ChartPoint(local) : glm::dvec3(local);
        if (!std::isfinite(glm::dot(point, point)))
            throw std::invalid_argument("terrain point must be finite in its source frame");
        out = glm::vec3(point);
        error.clear();
        return true;
    } catch (const std::exception &exception) {
        error = exception.what();
        return false;
    }
}
float Density(const Source &s, glm::vec3 p) {
    float d = p.y - Height(s, p.x, p.z);
    for (const auto &e : s.edits) {
        const float sphere =
            s.chart ? float(EditDistance(s, p, e)) : glm::length(p - e.point) - e.radius;
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
    return s.chart ? double(glm::length(delta)) <= BrushReach(s, e.radius) + s.spacing
                   : glm::length(delta) <= e.radius + s.spacing;
}
std::vector<ChunkKey> Affected(const Source &s, const Edit &e) {
    std::vector<ChunkKey> out;
    const double width = double(s.chunkCells) * s.spacing;
    const double radius =
        s.chart ? BrushReach(s, e.radius) + s.spacing : double(e.radius + s.spacing);
    auto range = [&](double point, unsigned samples, unsigned count) {
        const double half = (samples - 1) * double(s.spacing) * .5;
        return std::pair<int, int>{
            int(std::clamp(std::floor((point - radius + half) / width) - 1, 0., double(count))),
            int(std::clamp(std::floor((point + radius + half) / width) + 1, -1.,
                           double(count) - 1))};
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
    Json j = {{"format", "JudasTerrainV2"}, {"version", s.version}, {"samplesX", s.samplesX},
              {"samplesZ", s.samplesZ},     {"spacing", s.spacing}, {"chunkCells", s.chunkCells},
              {"bottom", s.bottom},         {"top", s.top},         {"edits", Json::array()}};
    for (auto e : s.edits)
        j["edits"].push_back({{"kind", e.kind == Edit::Kind::Remove ? "remove" : "add"},
                              {"point", {e.point.x, e.point.y, e.point.z}},
                              {"radius", e.radius}});
    if (s.chart) {
        const auto &c = *s.chart;
        j["planetChart"] = {{"radius", c.radius},
                            {"face", c.tile.face},
                            {"level", c.tile.level},
                            {"x", c.tile.x},
                            {"y", c.tile.y},
                            {"span", c.span},
                            {"originHeight", c.originHeight},
                            {"surfaceFingerprint", c.surfaceFingerprint},
                            {"rim", c.rim}};
    }
    j["layerTextures"] = s.layerTextures;
    j["layerColours"] = Json::array();
    for (auto c : s.layerColours)
        j["layerColours"].push_back({c.x, c.y, c.z});
    // Omit defaults to retain existing source identities and native receipts.
    if (s.materialDetail != MaterialDetail{}) {
        const auto &d = s.materialDetail;
        j["materialDetail"] = {
            {"tileMetres", {d.tileMetres.x, d.tileMetres.y, d.tileMetres.z, d.tileMetres.w}},
            {"roughness", {d.roughness.x, d.roughness.y, d.roughness.z, d.roughness.w}},
            {"normalTexture", d.normalTexture},
            {"normalMetres", d.normalMetres},
            {"normalStrength", d.normalStrength}};
    }
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
    if (j.at("format") != "JudasTerrainV2" || (j.at("version") != 2 && j.at("version") != 3))
        throw std::invalid_argument("terrain format/version");
    for (const auto *key : {"version", "samplesX", "samplesZ", "chunkCells"})
        if (!j.at(key).is_number_integer() || j.at(key).get<int64_t>() < 0 ||
            j.at(key).get<uint64_t>() > 4097)
            throw std::invalid_argument("terrain integer field");
    Source s;
    s.version = j.at("version");
    if (j.contains("planetChart")) {
        const auto &p = j.at("planetChart");
        if (!p.is_object() || p.size() != 9)
            throw std::invalid_argument(
                "planet chart requires exactly nine coordinate/ownership fields");
        if (!p.at("rim").is_array() || p.at("rim").size() != 4 * j.at("samplesX").get<uint64_t>() ||
            !p.at("surfaceFingerprint").is_string() ||
            p.at("surfaceFingerprint").get_ref<const std::string &>().size() != 64)
            throw std::invalid_argument("planet chart ownership metadata exceeds its grid budget");
        for (const auto *key : {"face", "level", "x", "y"})
            if (!p.at(key).is_number_integer() || p.at(key).get<int64_t>() < 0 ||
                p.at(key).get<uint64_t>() > UINT32_MAX)
                throw std::invalid_argument("planet chart identity requires unsigned integers");
        for (const auto *key : {"radius", "span", "originHeight"})
            if (!p.at(key).is_number())
                throw std::invalid_argument("planet chart coordinate requires a number");
        s.chart = PlanetChart{p.at("radius"),
                              {p.at("face"), p.at("level"), p.at("x"), p.at("y")},
                              p.at("span"),
                              p.at("originHeight"),
                              p.at("surfaceFingerprint").get<std::string>(),
                              p.at("rim").get<std::vector<float>>()};
    }
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
    if (j.contains("materialDetail")) {
        const auto &value = j.at("materialDetail");
        if (!value.is_object())
            throw std::invalid_argument("terrain material detail must be an object");
        for (const auto &entry : value.items())
            if (entry.key() != "tileMetres" && entry.key() != "roughness" &&
                entry.key() != "normalTexture" && entry.key() != "normalMetres" &&
                entry.key() != "normalStrength")
                throw std::invalid_argument("unknown terrain material detail field: " +
                                            entry.key());
        auto vector = [&](const char *key, glm::vec4 &target) {
            if (!value.contains(key))
                return;
            if (!value.at(key).is_array() || value.at(key).size() != 4)
                throw std::invalid_argument("terrain material vectors require exactly four values");
            const auto values = value.at(key).get<std::array<float, 4>>();
            target = {values[0], values[1], values[2], values[3]};
        };
        auto &detail = s.materialDetail;
        vector("tileMetres", detail.tileMetres);
        vector("roughness", detail.roughness);
        detail.normalTexture = value.value("normalTexture", std::string{});
        detail.normalMetres = value.value("normalMetres", 1.f);
        detail.normalStrength = value.value("normalStrength", 1.f);
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
