#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace AssetBinary {
using Bytes = std::vector<uint8_t>;
inline uint64_t Digest(const uint8_t *p, size_t n) {
  uint64_t h = 14695981039346656037ull;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}
inline void U32(Bytes &b, uint32_t n) {
  for (int k = 0; k < 4; ++k)
    b.push_back(uint8_t(n >> (8 * k)));
}
inline void Float(Bytes &b, float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  U32(b, u);
}
inline void Blob(Bytes &b, const Bytes &x) {
  U32(b, uint32_t(x.size()));
  b.insert(b.end(), x.begin(), x.end());
}
inline void Seal(Bytes &b) {
  auto h = Digest(b.data(), b.size());
  for (int k = 0; k < 8; ++k)
    b.push_back(uint8_t(h >> (k * 8)));
}
struct Reader {
  const uint8_t *p;
  size_t n, i = 0;
  Reader(const void *data, size_t size, const char *magic)
      : p(static_cast<const uint8_t *>(data)), n(size) {
    if (!p || n < 16 || n > 512 * 1024 * 1024 || std::memcmp(p, magic, 4))
      throw std::runtime_error("invalid cooked asset header/size");
    uint64_t expected = 0;
    for (int k = 0; k < 8; ++k)
      expected |= uint64_t(p[n - 8 + k]) << (8 * k);
    n -= 8;
    if (Digest(p, n) != expected)
      throw std::runtime_error("cooked asset checksum mismatch");
    i = 4;
    if (U32() != 1)
      throw std::runtime_error("unsupported cooked asset version");
  }
  uint32_t U32() {
    if (n - i < 4)
      throw std::runtime_error("truncated cooked asset");
    uint32_t v = 0;
    for (int k = 0; k < 4; ++k)
      v |= uint32_t(p[i++]) << (8 * k);
    return v;
  }
  float Float() {
    auto u = U32();
    float f;
    std::memcpy(&f, &u, 4);
    if (!std::isfinite(f))
      throw std::runtime_error("nonfinite cooked value");
    return f;
  }
  Bytes Blob(size_t limit = 128 * 1024 * 1024) {
    auto count = U32();
    if (count > limit || count > n - i)
      throw std::runtime_error("cooked blob bound");
    Bytes b(p + i, p + i + count);
    i += count;
    return b;
  }
  void End() {
    if (i != n)
      throw std::runtime_error("trailing cooked bytes");
  }
};
} // namespace AssetBinary
