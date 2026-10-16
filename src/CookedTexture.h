#pragma once
#include "TextureData.h"
#include <cstddef>
#include <string>
bool DecodeCookedTexture(const void *, size_t, TextureData &,
                         std::string &error);
TextureMip ExpandTextureMip(const TextureMip &, TextureStorage);
size_t TexturePayloadBytes(const TextureData &);
