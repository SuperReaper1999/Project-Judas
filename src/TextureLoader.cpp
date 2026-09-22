#include "TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

bool LoadTextureFromFile(const std::string& path, TextureData& outTexture, std::string& outError) {
    stbi_set_flip_vertically_on_load(true);

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    unsigned char* decoded =
        stbi_load(path.c_str(), &width, &height, &sourceChannels, TextureData::kChannels);

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
