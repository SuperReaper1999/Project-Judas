#include "TextureLoader.h"
#include "CookedTexture.h"
#include <cstring>
#include <fstream>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

namespace {
bool Finish(unsigned char* decoded, int width, int height, const std::string& name, TextureData& outTexture,
            std::string& outError);
}

bool DecodeTextureFromMemory(const std::uint8_t* data, std::size_t size, const std::string& nameForErrors,
                             TextureData& outTexture, std::string& outError) {
    if(data&&size>=4&&!std::memcmp(data,"JTX1",4))return DecodeCookedTexture(data,size,outTexture,outError);
    // stb_image's flip flag is thread-local in this version (stbi__vertically_flip_on_load_local), so
    // setting it on a worker never affects another thread's decode.
    if(!data||size==0||size>64*1024*1024){outError="encoded texture exceeds bounded input size";return false;}
    int previewWidth=0,previewHeight=0,previewChannels=0;
    if(!stbi_info_from_memory(data,static_cast<int>(size),&previewWidth,&previewHeight,&previewChannels)||previewWidth<=0||previewHeight<=0||uint64_t(previewWidth)*previewHeight*4>64*1024*1024){outError="texture dimensions exceed 64 MiB decoded RGBA bound";return false;}
    stbi_set_flip_vertically_on_load_thread(1);
    int width = 0, height = 0, sourceChannels = 0;
    unsigned char* decoded = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &sourceChannels,
                                                   TextureData::kChannels);
    return Finish(decoded, width, height, nameForErrors, outTexture, outError);
}

bool LoadTextureFromFile(const std::string& path, TextureData& outTexture, std::string& outError) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);auto n=f.tellg();if(n<=0||n>96*1024*1024){outError="cannot read bounded texture: "+path;return false;}std::vector<uint8_t> bytes(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(n))){outError="texture read failed: "+path;return false;}return DecodeTextureFromMemory(bytes.data(),bytes.size(),path,outTexture,outError);
}

namespace {
bool Finish(unsigned char* decoded, int width, int height, const std::string& path, TextureData& outTexture,
            std::string& outError) {
    if (!decoded) {
        outError = "Failed to load texture '" + path + "': " +
                   std::string(stbi_failure_reason() ? stbi_failure_reason() : "unknown error");
        return false;
    }

    outTexture=TextureData{};
    outTexture.width = width;
    outTexture.height = height;
    const size_t byteCount =
        static_cast<size_t>(width) * static_cast<size_t>(height) * TextureData::kChannels;
    outTexture.pixels.assign(decoded, decoded + byteCount);

    stbi_image_free(decoded);
    return true;
}
}  // namespace
