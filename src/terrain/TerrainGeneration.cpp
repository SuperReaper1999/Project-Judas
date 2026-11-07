#include "TerrainGeneration.h"
#include "../../third_party/stb_image.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace TerrainV2 {
namespace {
uint32_t Hash(int x, int z, unsigned seed) {
    uint32_t v = uint32_t(x) * 0x8da6b343u ^ uint32_t(z) * 0xd8163841u ^ seed;
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    return v ^ (v >> 16);
}
double Noise(double x, double z, unsigned seed) {
    const int ix = int(std::floor(x)), iz = int(std::floor(z));
    auto fade = [](double t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    const double u = fade(x - ix), v = fade(z - iz);
    auto at = [&](int a, int b) { return double(Hash(a, b, seed)) / 4294967295.0; };
    return (1 - v) * ((1 - u) * at(ix, iz) + u * at(ix + 1, iz)) +
           v * ((1 - u) * at(ix, iz + 1) + u * at(ix + 1, iz + 1));
}
void CheckRecipe(const LandscapeRecipe &r) {
    if (r.samples < 17 || r.samples > 1025 || (r.samples - 1) % 16 || !std::isfinite(r.spacing) ||
        r.spacing < .125f || r.spacing > 128 || !std::isfinite(r.base) ||
        std::abs(r.base) > 30000 || !std::isfinite(r.height) || r.height < 0 || r.height > 65535 ||
        (r.preset != LandscapeRecipe::Preset::Flat &&
         r.preset != LandscapeRecipe::Preset::Highlands &&
         r.preset != LandscapeRecipe::Preset::Island))
        throw std::runtime_error("landscape recipe requires aligned17..1025 samples, finite "
                                 ".125..128 m spacing and bounded height/seed settings");
}
bool Finish(Source next, Source &output, std::string &error, bool paint, bool coastline = false) {
    const auto bounds = std::minmax_element(next.heights.begin(), next.heights.end());
    next.bottom = std::min(-4096.f, *bounds.first - std::max(16.f, next.spacing * 64));
    next.top = std::max(4096.f, *bounds.second + std::max(16.f, next.spacing * 64));
    for (unsigned z = 0; paint && z < next.samplesZ; ++z)
        for (unsigned x = 0; x < next.samplesX; ++x) {
            auto at = [&](unsigned xx, unsigned zz) {
                return next.heights[size_t(zz) * next.samplesX + xx];
            };
            const float sx = (at(std::min(x + 1, next.samplesX - 1), z) - at(x ? x - 1 : x, z)) /
                             (next.spacing * (x && x + 1 < next.samplesX ? 2 : 1));
            const float sz = (at(x, std::min(z + 1, next.samplesZ - 1)) - at(x, z ? z - 1 : z)) /
                             (next.spacing * (z && z + 1 < next.samplesZ ? 2 : 1));
            const float rock = std::clamp((std::hypot(sx, sz) - .35f) * .8f, 0.f, .95f);
            const float sand =
                coastline ? std::clamp((1.5f - at(x, z)) * .5f, 0.f, 1.f) * (1 - rock) : 0;
            const float earth = (1 - rock - sand) * .15f;
            next.weights[size_t(z) * next.samplesX + x] = {1 - rock - sand - earth, earth, rock,
                                                           sand};
        }
    if (!Validate(next, error))
        return false;
    output = std::move(next);
    return true;
}
} // namespace
bool Generate(const LandscapeRecipe &r, Source &output, std::string &error) {
    try {
        CheckRecipe(r);
        auto next = Create(r.samples, r.samples, r.spacing);
        if (r.preset == LandscapeRecipe::Preset::Flat) {
            std::fill(next.heights.begin(), next.heights.end(), r.base);
            return Finish(std::move(next), output, error, false);
        }
        const double extent = (r.samples - 1) * r.spacing;
        for (unsigned z = 0; z < r.samples; ++z)
            for (unsigned x = 0; x < r.samples; ++x) {
                const double u = double(x) / (r.samples - 1), v = double(z) / (r.samples - 1);
                double h = 0, scale = 1, weight = 0;
                for (unsigned octave = 0; octave < 6; ++octave) {
                    const double n = Noise((u + 2.37) * scale * extent / 96,
                                           (v - 1.19) * scale * extent / 96, r.seed + octave);
                    const double ridge = 1 - std::abs(2 * n - 1);
                    h += (octave < 2 ? ridge * ridge : n) / scale;
                    weight += 1 / scale;
                    scale *= 2;
                }
                h /= weight;
                if (r.preset == LandscapeRecipe::Preset::Island) {
                    const double shore =
                        std::clamp((.9 - std::hypot((u - .5) * 2, (v - .5) * 2)) * 3, 0., 1.);
                    h = h * shore * shore - .12 * (1 - shore);
                }
                next.heights[size_t(z) * r.samples + x] = float(r.base + r.height * h);
            }
        return Finish(std::move(next), output, error, true,
                      r.preset == LandscapeRecipe::Preset::Island);
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool ImportHeightmap(std::string_view bytes, const LandscapeRecipe &r, Source &output,
                     std::string &error) {
    try {
        CheckRecipe(r);
        int width = 0, height = 0, channels = 0;
        if (bytes.empty() || bytes.size() > 64 * 1024 * 1024 ||
            !stbi_info_from_memory(reinterpret_cast<const stbi_uc *>(bytes.data()),
                                   int(bytes.size()), &width, &height, &channels) ||
            width < 2 || height < 2 || uint64_t(width) * height > 4194304 || channels > 2)
            throw std::runtime_error(
                "heightmap requires a bounded grayscale image (up to4194304 pixels)");
        stbi_set_flip_vertically_on_load_thread(0);
        std::unique_ptr<stbi_us, void (*)(void *)> pixels(
            stbi_load_16_from_memory(reinterpret_cast<const stbi_uc *>(bytes.data()),
                                     int(bytes.size()), &width, &height, &channels, 1),
            stbi_image_free);
        if (!pixels)
            throw std::runtime_error("heightmap decode failed");
        auto next = Create(r.samples, r.samples, r.spacing);
        for (unsigned z = 0; z < r.samples; ++z)
            for (unsigned x = 0; x < r.samples; ++x) {
                const double gx = double(x) * (width - 1) / (r.samples - 1),
                             gz = double(z) * (height - 1) / (r.samples - 1);
                const unsigned ix = std::min(unsigned(gx), unsigned(width - 2)),
                               iz = std::min(unsigned(gz), unsigned(height - 2));
                const double u = gx - ix, v = gz - iz;
                auto at = [&](unsigned a, unsigned b) {
                    return double(pixels.get()[size_t(b) * width + a]);
                };
                const double value = (1 - v) * ((1 - u) * at(ix, iz) + u * at(ix + 1, iz)) +
                                     v * ((1 - u) * at(ix, iz + 1) + u * at(ix + 1, iz + 1));
                next.heights[size_t(z) * r.samples + x] = float(r.base + r.height * value / 65535);
            }
        return Finish(std::move(next), output, error, true);
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
} // namespace TerrainV2
