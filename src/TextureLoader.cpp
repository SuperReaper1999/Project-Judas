#include "TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

namespace {
bool Finish(unsigned char* decoded, int width, int height, const std::string& name, TextureData& outTexture,
            std::string& outError);
}

bool DecodeTextureFromMemory(const std::uint8_t* data, std::size_t size, const std::string& nameForErrors,
                             TextureData& outTexture, std::string& outError) {
    // stb_image's flip flag is thread-local in this version (stbi__vertically_flip_on_load_local), so
    // setting it on a worker never affects another thread's decode.
    stbi_set_flip_vertically_on_load_thread(1);
    int width = 0, height = 0, sourceChannels = 0;
    unsigned char* decoded = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &sourceChannels,
                                                   TextureData::kChannels);
    return Finish(decoded, width, height, nameForErrors, outTexture, outError);
}

bool LoadTextureFromFile(const std::string& path, TextureData& outTexture, std::string& outError) {
    stbi_set_flip_vertically_on_load(true);

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    unsigned char* decoded =
        stbi_load(path.c_str(), &width, &height, &sourceChannels, TextureData::kChannels);
    return Finish(decoded, width, height, path, outTexture, outError);
}

namespace {
bool Finish(unsigned char* decoded, int width, int height, const std::string& path, TextureData& outTexture,
            std::string& outError) {
    if (!decoded) {
        outError = "Failed to load texture '" + path + "': " +
                   std::string(stbi_failure_reason() ? stbi_failure_reason() : "unknown error");
        return false;
    }

    outTexture.width = width;
    outTexture.height = height;
    const size_t byteCount =
        static_cast<size_t>(width) * static_cast<size_t>(height) * TextureData::kChannels;
    outTexture.pixels.assign(decoded, decoded + byteCount);

    stbi_image_free(decoded);
    return true;
}
}  // namespace
