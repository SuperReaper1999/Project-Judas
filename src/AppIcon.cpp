#include "AppIcon.h"
#include "JudasAppIcon.h"
#include "TextureLoader.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <fstream>
#include <vector>

namespace {
bool DecodeIcon(const std::string& path, TextureData& texture, std::string& error) {
    const unsigned char* bytes = kJudasAppIconPNG;
    std::size_t size = sizeof(kJudasAppIconPNG);
    std::vector<unsigned char> fileBytes;
    if (!path.empty()) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const auto length = file ? file.tellg() : std::streampos(-1);
        if (length <= 0 || length > 16 * 1024 * 1024) {
            error = "Cannot read app icon (maximum 16 MiB): " + path;
            return false;
        }
        fileBytes.resize(static_cast<std::size_t>(length));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(fileBytes.data()), fileBytes.size())) {
            error = "Cannot read app icon: " + path;
            return false;
        }
        bytes = fileBytes.data();
        size = fileBytes.size();
    }
    if (!DecodeTextureFromMemory(bytes, size, path.empty() ? "Judas app icon" : path,
                                 texture, error)) return false;
    if (texture.width > 4096 || texture.height > 4096) {
        error = "App icon dimensions must be at most 4096 x 4096";
        return false;
    }
    return true;
}
}

bool ApplyAppIcon(SDL_Window* window, const std::string& path, std::string& error) {
    if (!window) { error = "App icon requires a window"; return false; }
    TextureData texture;
    if (!DecodeIcon(path, texture, error)) return false;
    // TextureLoader uses GL's bottom-up convention; desktop icons are top-down.
    const std::size_t stride = static_cast<std::size_t>(texture.width) * 4;
    for (int y = 0; y < texture.height / 2; ++y) {
        auto first = texture.pixels.begin() + y * stride;
        auto last = texture.pixels.begin() + (texture.height - 1 - y) * stride;
        std::swap_ranges(first, first + stride, last);
    }
    SDL_Surface* icon = SDL_CreateRGBSurfaceWithFormatFrom(texture.pixels.data(),
        texture.width, texture.height, 32, static_cast<int>(stride), SDL_PIXELFORMAT_RGBA32);
    if (!icon) { error = SDL_GetError(); return false; }
    // Bound the native window-manager property; retain the original PNG for
    // desktop launchers/export. Preserve aspect ratio and transparent pixels.
    const int edge = std::max(texture.width, texture.height);
    if (edge > 512) {
        SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0,
            std::max(1, texture.width * 512 / edge),
            std::max(1, texture.height * 512 / edge), 32, SDL_PIXELFORMAT_RGBA32);
        SDL_SetSurfaceBlendMode(icon, SDL_BLENDMODE_NONE);
        if (!scaled || SDL_BlitScaled(icon, nullptr, scaled, nullptr) != 0) {
            error = SDL_GetError();
            if (scaled) SDL_FreeSurface(scaled);
            SDL_FreeSurface(icon);
            return false;
        }
        SDL_FreeSurface(icon);
        icon = scaled;
    }
    SDL_SetWindowIcon(window, icon); // SDL copies pixels before we release storage.
    SDL_FreeSurface(icon);
    error.clear();
    return true;
}

bool LoadDefaultAppIcon(TextureData& texture, std::string& error) {
    return DecodeIcon("", texture, error);
}

bool ValidateAppIcon(const std::string& path, std::string& error) {
    TextureData texture;
    const bool valid = DecodeIcon(path, texture, error);
    if (valid) error.clear();
    return valid;
}

bool WriteDefaultAppIcon(const std::string& path, std::string& error) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(kJudasAppIconPNG), sizeof(kJudasAppIconPNG));
    if (!file) { error = "Cannot write app icon: " + path; return false; }
    error.clear();
    return true;
}
