#pragma once

// Native test fixtures from scripts/m93_assets.py. Keep the original source
// geometry, UV seam, normals and painted tile; these gates must not depend on
// someone first extracting a historical archive into the repository's .cache.
#include "../../third_party/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <string>
#include <vector>

namespace M93AssetFixtures {

inline bool WriteColumn(const std::filesystem::path &path, std::string &error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "Cannot create column fixture directory: " + ec.message();
        return false;
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.imbue(std::locale::classic());
    file << std::fixed << std::setprecision(8);
    constexpr double pi = 3.141592653589793238462643383279502884;
    // 8,385 authored vertices, each with a normal and UV; 16,384 triangles.
    // There are deliberately no material groups in this original fixture.
    for (int y = 0; y < 65; ++y) {
        const double t = y / 64.0;
        const double radius = .7 + .18 * std::cos(t * pi * 8) + .05 * std::sin(t * pi * 20);
        for (int x = 0; x < 129; ++x) {
            const double a = x / 128.0 * (2 * pi);
            file << "v " << radius * std::cos(a) << ' ' << t * 4 << ' '
                 << radius * std::sin(a) << '\n';
        }
    }
    for (int y = 0; y < 65; ++y)
        for (int x = 0; x < 129; ++x)
            file << "vt " << x / 128.0 << ' ' << y / 64.0 << '\n';
    for (int y = 0; y < 65; ++y) {
        const double t = y / 64.0;
        const double dy =
            (-.18 * 8 * pi * std::sin(t * 8 * pi) + .05 * 20 * pi * std::cos(t * 20 * pi)) / 4;
        for (int x = 0; x < 129; ++x) {
            const double a = x / 128.0 * (2 * pi);
            const double n = std::sqrt(1 + dy * dy);
            file << "vn " << std::cos(a) / n << ' ' << -dy / n << ' ' << std::sin(a) / n
                 << '\n';
        }
    }
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 128; ++x) {
            const int a = y * 129 + x + 1, b = a + 1, c = a + 129, d = c + 1;
            file << "f " << a << '/' << a << '/' << a << ' ' << c << '/' << c << '/' << c << ' '
                 << b << '/' << b << '/' << b << '\n';
            file << "f " << b << '/' << b << '/' << b << ' ' << c << '/' << c << '/' << c << ' '
                 << d << '/' << d << '/' << d << '\n';
        }
    }
    file.close();
    if (!file) {
        error = "Cannot write original M93 column fixture: " + path.string();
        return false;
    }
    return true;
}

inline bool WriteTile1(const std::filesystem::path &path, std::string &error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "Cannot create tile fixture directory: " + ec.message();
        return false;
    }
    constexpr int size = 1024;
    constexpr int base[3] = {124, 65, 145};
    std::vector<unsigned char> pixels(size * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool mortar = x % 128 < 4 || y % 128 < 4;
            const double v = mortar ? 0 : 18 * std::sin(x * .018) * std::cos(y * .023) +
                                                5 * std::sin((x + y) * .35);
            const auto at = static_cast<std::size_t>((y * size + x) * 4);
            for (int channel = 0; channel < 3; ++channel)
                pixels[at + channel] = static_cast<unsigned char>(std::clamp(base[channel] + v, 0., 255.));
            pixels[at + 3] = 255;
        }
    }
    // Compression bytes may differ from Pillow's archived PNG. Dimensions,
    // decoded RGBA content and material/alpha semantics are unchanged.
    if (!stbi_write_png(path.string().c_str(), size, size, 4, pixels.data(), size * 4)) {
        error = "Cannot write original M93 painted tile fixture: " + path.string();
        return false;
    }
    return true;
}

} // namespace M93AssetFixtures
