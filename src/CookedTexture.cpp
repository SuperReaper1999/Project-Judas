#include "CookedTexture.h"
#include "../third_party/stb_image.h"
#include "AssetBinary.h"
#include <algorithm>
size_t TexturePayloadBytes(const TextureData &t) {
  size_t b = t.pixels.size();
  for (auto &l : t.levels)
    b += l.bytes.size();
  return b;
}
bool DecodeCookedTexture(const void *data, size_t size, TextureData &out,
                         std::string &error) {
  try {
    AssetBinary::Reader r(data, size, "JTX1");
    TextureData t;
    auto format = r.U32();
    if (format > 2)
      throw std::runtime_error("texture storage format");
    t.storage = TextureStorage(format);
    t.width = int(r.U32());
    t.height = int(r.U32());
    auto count = r.U32();
    if (t.width <= 0 || t.height <= 0 ||
        uint64_t(t.width) * t.height * 4 > 64 * 1024 * 1024 || !count ||
        count > 16)
      throw std::runtime_error("cooked texture dimensions");
    r.Blob(4096); // source/settings provenance, not a runtime dependency
    int w = t.width, h = t.height;
    for (unsigned i = 0; i < count; ++i) {
      auto method = r.U32(), unpacked = r.U32();
      auto stored = r.Blob(64 * 1024 * 1024);
      TextureMip l{w, h, {}};
      auto bytes = format == 0 ? size_t(w) * h * 4
                               : size_t((w + 3) / 4) * ((h + 3) / 4) *
                                     (format == 1 ? 8 : 16);
      if (unpacked != bytes || method > 1)
        throw std::runtime_error("cooked mip byte count/codec");
      if (method == 0)
        l.bytes = std::move(stored);
      else {
        l.bytes.resize(bytes);
        if (stbi_zlib_decode_buffer(
                reinterpret_cast<char *>(l.bytes.data()), int(bytes),
                reinterpret_cast<const char *>(stored.data()),
                int(stored.size())) != int(bytes))
          throw std::runtime_error("invalid compressed mip payload");
      }
      if (l.bytes.size() != bytes)
        throw std::runtime_error("cooked mip byte count");
      t.levels.push_back(std::move(l));
      if (w == 1 && h == 1 && i + 1 != count)
        throw std::runtime_error("excess mip levels");
      w = std::max(1, w / 2);
      h = std::max(1, h / 2);
    }
    if (t.levels.back().width != 1 || t.levels.back().height != 1)
      throw std::runtime_error("incomplete mip chain");
    r.End();
    out = std::move(t);
    error.clear();
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
TextureMip ExpandTextureMip(const TextureMip &input, TextureStorage storage) {
  if (storage == TextureStorage::Rgba8)
    return input;
  TextureMip out{input.width, input.height, {}};
  out.bytes.resize(size_t(out.width) * out.height * 4);
  const unsigned blockBytes = storage == TextureStorage::BC1 ? 8 : 16;
  size_t offset = 0;
  for (int by = 0; by < out.height; by += 4)
    for (int bx = 0; bx < out.width; bx += 4) {
      const auto *block = input.bytes.data() + offset;
      offset += blockBytes;
      const auto *color = block + (blockBytes == 16 ? 8 : 0);
      auto u16 = [](const uint8_t *p) {
        return unsigned(p[0]) | unsigned(p[1]) << 8;
      };
      unsigned a = u16(color), b = u16(color + 2);
      unsigned c[4][4]{};
      auto unpack = [](unsigned n, unsigned *v) {
        v[0] = ((n >> 11) & 31) * 255 / 31;
        v[1] = ((n >> 5) & 63) * 255 / 63;
        v[2] = (n & 31) * 255 / 31;
        v[3] = 255;
      };
      unpack(a, c[0]);
      unpack(b, c[1]);
      for (int k = 0; k < 3; ++k) {
        c[2][k] = (2 * c[0][k] + c[1][k]) / 3;
        c[3][k] = (c[0][k] + 2 * c[1][k]) / 3;
      }
      c[2][3] = c[3][3] = 255;
      if (blockBytes == 8 && a <= b)
        for (int k = 0; k < 3; ++k) {
          c[2][k] = (c[0][k] + c[1][k]) / 2;
          c[3][k] = 0;
        }
      unsigned alpha[8] = {255, 255};
      uint64_t alphaBits = 0;
      if (blockBytes == 16) {
        alpha[0] = block[0];
        alpha[1] = block[1];
        if (alpha[0] > alpha[1])
          for (int i = 1; i <= 6; ++i)
            alpha[i + 1] = ((7 - i) * alpha[0] + i * alpha[1]) / 7;
        else {
          for (int i = 1; i <= 4; ++i)
            alpha[i + 1] = ((5 - i) * alpha[0] + i * alpha[1]) / 5;
          alpha[6] = 0;
          alpha[7] = 255;
        }
        for (int k = 0; k < 6; ++k)
          alphaBits |= uint64_t(block[k + 2]) << (8 * k);
      }
      uint32_t bits = uint32_t(color[4]) | uint32_t(color[5]) << 8 |
                      uint32_t(color[6]) << 16 | uint32_t(color[7]) << 24;
      for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
          int pixel = y * 4 + x;
          if (bx + x >= out.width || by + y >= out.height)
            continue;
          auto *dst =
              out.bytes.data() + (size_t(by + y) * out.width + bx + x) * 4;
          auto *rgb = c[(bits >> (2 * pixel)) & 3];
          for (int k = 0; k < 3; ++k)
            dst[k] = uint8_t(rgb[k]);
          dst[3] = uint8_t(
              blockBytes == 16 ? alpha[(alphaBits >> (3 * pixel)) & 7] : 255);
        }
    }
  return out;
}
