// Vulkan backend: textures, samplers, renderbuffers, framebuffers and pixel
// transfer. Images keep OpenGL's memory convention (row 0 is the bottom row of
// a rendered picture), so texture coordinates, gl_FragCoord, readbacks and
// blits need no per-call orientation changes.
#include "VkCore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace JudasVulkan::detail {
namespace {

enum class Channel { U8, F16, F32, D16, D24, D32F, BC };
struct FormatInfo {
    VkFormat vk = VK_FORMAT_UNDEFINED;
    Channel channel = Channel::U8;
    int components = 4; // stored components
    int bytes = 4;      // bytes per stored pixel
    int blockBytes = 0; // compressed block bytes
};

bool LookupFormat(GLenum internal, FormatInfo &info) {
    switch (internal) {
    case GL_RGBA8: case GL_RGBA: case GL_RGB8: case GL_RGB: case 4: case 3:
        info = {VK_FORMAT_R8G8B8A8_UNORM, Channel::U8, 4, 4, 0}; return true;
    case GL_SRGB8_ALPHA8: case GL_SRGB8: case GL_SRGB_ALPHA: case GL_SRGB:
        info = {VK_FORMAT_R8G8B8A8_SRGB, Channel::U8, 4, 4, 0}; return true;
    case GL_R8: case GL_RED: info = {VK_FORMAT_R8_UNORM, Channel::U8, 1, 1, 0}; return true;
    case GL_RG8: case GL_RG: info = {VK_FORMAT_R8G8_UNORM, Channel::U8, 2, 2, 0}; return true;
    case GL_RGBA16F: case GL_RGB16F: info = {VK_FORMAT_R16G16B16A16_SFLOAT, Channel::F16, 4, 8, 0}; return true;
    case GL_RG16F: info = {VK_FORMAT_R16G16_SFLOAT, Channel::F16, 2, 4, 0}; return true;
    case GL_R16F: info = {VK_FORMAT_R16_SFLOAT, Channel::F16, 1, 2, 0}; return true;
    case GL_RGBA32F: case GL_RGB32F: info = {VK_FORMAT_R32G32B32A32_SFLOAT, Channel::F32, 4, 16, 0}; return true;
    case GL_RG32F: info = {VK_FORMAT_R32G32_SFLOAT, Channel::F32, 2, 8, 0}; return true;
    case GL_R32F: info = {VK_FORMAT_R32_SFLOAT, Channel::F32, 1, 4, 0}; return true;
    case GL_DEPTH_COMPONENT16: info = {VK_FORMAT_D16_UNORM, Channel::D16, 1, 2, 0}; return true;
    case GL_DEPTH_COMPONENT24: case GL_DEPTH_COMPONENT: case GL_DEPTH24_STENCIL8: case GL_DEPTH_STENCIL:
        if (g->depth24Format == VK_FORMAT_X8_D24_UNORM_PACK32) info = {VK_FORMAT_X8_D24_UNORM_PACK32, Channel::D24, 1, 4, 0};
        else info = {VK_FORMAT_D32_SFLOAT, Channel::D32F, 1, 4, 0};
        return true;
    case GL_DEPTH_COMPONENT32F: case GL_DEPTH_COMPONENT32: case GL_DEPTH32F_STENCIL8:
        info = {VK_FORMAT_D32_SFLOAT, Channel::D32F, 1, 4, 0}; return true;
    case 0x83F0: info = {VK_FORMAT_BC1_RGB_UNORM_BLOCK, Channel::BC, 4, 0, 8}; return true;
    case 0x83F1: info = {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, Channel::BC, 4, 0, 8}; return true;
    case 0x83F2: info = {VK_FORMAT_BC2_UNORM_BLOCK, Channel::BC, 4, 0, 16}; return true;
    case 0x83F3: info = {VK_FORMAT_BC3_UNORM_BLOCK, Channel::BC, 4, 0, 16}; return true;
    case 0x8C4C: info = {VK_FORMAT_BC1_RGB_SRGB_BLOCK, Channel::BC, 4, 0, 8}; return true;
    case 0x8C4D: info = {VK_FORMAT_BC1_RGBA_SRGB_BLOCK, Channel::BC, 4, 0, 8}; return true;
    case 0x8C4E: info = {VK_FORMAT_BC2_SRGB_BLOCK, Channel::BC, 4, 0, 16}; return true;
    case 0x8C4F: info = {VK_FORMAT_BC3_SRGB_BLOCK, Channel::BC, 4, 0, 16}; return true;
    default: return false;
    }
}
FormatInfo InfoForVk(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SRGB: case VK_FORMAT_B8G8R8A8_UNORM:
        return {format, Channel::U8, 4, 4, 0};
    case VK_FORMAT_R8_UNORM: return {format, Channel::U8, 1, 1, 0};
    case VK_FORMAT_R8G8_UNORM: return {format, Channel::U8, 2, 2, 0};
    case VK_FORMAT_R16G16B16A16_SFLOAT: return {format, Channel::F16, 4, 8, 0};
    case VK_FORMAT_R16G16_SFLOAT: return {format, Channel::F16, 2, 4, 0};
    case VK_FORMAT_R16_SFLOAT: return {format, Channel::F16, 1, 2, 0};
    case VK_FORMAT_R32G32B32A32_SFLOAT: return {format, Channel::F32, 4, 16, 0};
    case VK_FORMAT_R32G32_SFLOAT: return {format, Channel::F32, 2, 8, 0};
    case VK_FORMAT_R32_SFLOAT: return {format, Channel::F32, 1, 4, 0};
    case VK_FORMAT_D16_UNORM: return {format, Channel::D16, 1, 2, 0};
    case VK_FORMAT_X8_D24_UNORM_PACK32: return {format, Channel::D24, 1, 4, 0};
    case VK_FORMAT_D32_SFLOAT: return {format, Channel::D32F, 1, 4, 0};
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK: case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        return {format, Channel::BC, 4, 0, 8};
    default: return {format, Channel::BC, 4, 0, 16};
    }
}

std::uint16_t FloatToHalf(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, 4);
    const std::uint32_t sign = (bits >> 16) & 0x8000u;
    const std::int32_t exponent = std::int32_t((bits >> 23) & 0xFF) - 127 + 15;
    std::uint32_t mantissa = bits & 0x7FFFFFu;
    if (((bits >> 23) & 0xFF) == 0xFF) return std::uint16_t(sign | 0x7C00u | (mantissa ? 0x200u : 0));
    if (exponent >= 31) return std::uint16_t(sign | 0x7C00u);
    if (exponent <= 0) {
        if (exponent < -10) return std::uint16_t(sign);
        mantissa |= 0x800000u;
        const int shift = 14 - exponent;
        std::uint32_t half = mantissa >> shift;
        const std::uint32_t remainder = mantissa & ((1u << shift) - 1), midpoint = 1u << (shift - 1);
        if (remainder > midpoint || (remainder == midpoint && (half & 1))) ++half;
        return std::uint16_t(sign | half);
    }
    std::uint32_t half = (std::uint32_t(exponent) << 10) | (mantissa >> 13);
    const std::uint32_t remainder = mantissa & 0x1FFFu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (half & 1))) ++half;
    return std::uint16_t(sign | half);
}
float HalfToFloat(std::uint16_t half) {
    const std::uint32_t sign = std::uint32_t(half & 0x8000u) << 16;
    std::uint32_t exponent = (half >> 10) & 0x1F, mantissa = half & 0x3FFu, bits;
    if (exponent == 0) {
        if (!mantissa) bits = sign;
        else {
            int shift = 0;
            while (!(mantissa & 0x400u)) { mantissa <<= 1; ++shift; }
            mantissa &= 0x3FFu;
            bits = sign | (std::uint32_t(127 - 15 - shift + 1) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) bits = sign | 0x7F800000u | (mantissa << 13);
    else bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

int SourceComponents(GLenum format) {
    switch (format) {
    case GL_RED: case GL_DEPTH_COMPONENT: case GL_RED_INTEGER: return 1;
    case GL_RG: case GL_DEPTH_STENCIL: return 2;
    case GL_RGB: case GL_BGR: return 3;
    default: return 4;
    }
}
int TypeBytes(GLenum type) {
    switch (type) {
    case GL_UNSIGNED_BYTE: case GL_BYTE: return 1;
    case GL_UNSIGNED_SHORT: case GL_SHORT: case GL_HALF_FLOAT: return 2;
    default: return 4;
    }
}
float ReadChannel(const std::uint8_t *p, GLenum type) {
    switch (type) {
    case GL_UNSIGNED_BYTE: return float(*p) / 255.0f;
    case GL_UNSIGNED_SHORT: { std::uint16_t v; std::memcpy(&v, p, 2); return float(v) / 65535.0f; }
    case GL_HALF_FLOAT: { std::uint16_t v; std::memcpy(&v, p, 2); return HalfToFloat(v); }
    case GL_UNSIGNED_INT: { std::uint32_t v; std::memcpy(&v, p, 4); return float(double(v) / 4294967295.0); }
    case GL_UNSIGNED_INT_24_8: { std::uint32_t v; std::memcpy(&v, p, 4); return float(double(v >> 8) / 16777215.0); }
    default: { float v; std::memcpy(&v, p, 4); return v; }
    }
}
void WriteStored(std::uint8_t *out, const FormatInfo &info, const float *rgba) {
    for (int c = 0; c < info.components; ++c) {
        const float value = rgba[c];
        switch (info.channel) {
        case Channel::U8: out[c] = std::uint8_t(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); break;
        case Channel::F16: { const std::uint16_t h = FloatToHalf(value); std::memcpy(out + c * 2, &h, 2); break; }
        case Channel::F32: case Channel::D32F: std::memcpy(out + c * 4, &value, 4); break;
        case Channel::D16: { const std::uint16_t d = std::uint16_t(std::lround(std::clamp(value, 0.0f, 1.0f) * 65535.0f)); std::memcpy(out, &d, 2); break; }
        case Channel::D24: { const std::uint32_t d = std::uint32_t(std::llround(double(std::clamp(value, 0.0f, 1.0f)) * 16777215.0)); std::memcpy(out, &d, 4); break; }
        default: break;
        }
    }
}
void ReadStored(const std::uint8_t *in, const FormatInfo &info, float *rgba) {
    rgba[0] = rgba[1] = rgba[2] = 0;
    rgba[3] = 1;
    for (int c = 0; c < info.components; ++c) {
        switch (info.channel) {
        case Channel::U8: rgba[c] = float(in[c]) / 255.0f; break;
        case Channel::F16: { std::uint16_t h; std::memcpy(&h, in + c * 2, 2); rgba[c] = HalfToFloat(h); break; }
        case Channel::F32: case Channel::D32F: std::memcpy(&rgba[c], in + c * 4, 4); break;
        case Channel::D16: { std::uint16_t d; std::memcpy(&d, in, 2); rgba[0] = float(d) / 65535.0f; break; }
        case Channel::D24: { std::uint32_t d; std::memcpy(&d, in, 4); rgba[0] = float(double(d & 0xFFFFFFu) / 16777215.0); break; }
        default: break;
        }
    }
    if (info.vk == VK_FORMAT_B8G8R8A8_UNORM) std::swap(rgba[0], rgba[2]);
}

// Client pixels (unpack state applied) -> tightly packed stored texels.
void ConvertUpload(const void *pixels, GLenum format, GLenum type, int width, int height, int depth,
                   const FormatInfo &info, std::vector<std::uint8_t> &out) {
    const GlState &gl = g->gl;
    const int components = SourceComponents(format), channelBytes = TypeBytes(type);
    const int pixelBytes = type == GL_UNSIGNED_INT_24_8 ? 4 : components * channelBytes;
    const int rowPixels = gl.unpackRowLength > 0 ? gl.unpackRowLength : width;
    const int alignment = std::max(1, gl.unpackAlignment);
    const std::size_t rowBytes = (std::size_t(rowPixels) * pixelBytes + alignment - 1) / alignment * alignment;
    const int imageRows = gl.unpackImageHeight > 0 ? gl.unpackImageHeight : height;
    out.resize(std::size_t(width) * height * depth * info.bytes);
    const auto *source = static_cast<const std::uint8_t *>(pixels);
    const bool bgr = format == GL_BGRA || format == GL_BGR;
    const bool direct = !bgr && type != GL_UNSIGNED_INT_24_8 && components == info.components &&
                        ((type == GL_UNSIGNED_BYTE && info.channel == Channel::U8) ||
                         (type == GL_FLOAT && (info.channel == Channel::F32 || info.channel == Channel::D32F)) ||
                         (type == GL_HALF_FLOAT && info.channel == Channel::F16));
    for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y) {
            const std::uint8_t *row = source + (std::size_t(z + gl.unpackSkipImages) * imageRows + y + gl.unpackSkipRows) * rowBytes +
                                      std::size_t(gl.unpackSkipPixels) * pixelBytes;
            std::uint8_t *destination = out.data() + (std::size_t(z) * height + y) * width * info.bytes;
            if (direct) {
                std::memcpy(destination, row, std::size_t(width) * info.bytes);
                continue;
            }
            if (type == GL_UNSIGNED_BYTE && info.channel == Channel::U8 && !bgr) {
                // Channel count change only (RGB -> RGBA and similar).
                for (int x = 0; x < width; ++x) {
                    std::uint8_t texel[4] = {0, 0, 0, 255};
                    for (int c = 0; c < components; ++c) texel[c] = row[x * components + c];
                    std::memcpy(destination + x * info.bytes, texel, std::size_t(info.bytes));
                }
                continue;
            }
            for (int x = 0; x < width; ++x) {
                float rgba[4] = {0, 0, 0, 1};
                for (int c = 0; c < components; ++c) rgba[c] = ReadChannel(row + x * pixelBytes + c * channelBytes, type);
                if (bgr) std::swap(rgba[0], rgba[2]);
                WriteStored(destination + x * info.bytes, info, rgba);
            }
        }
}

// Stored texels -> client pixels (pack state applied).
void ConvertReadback(const std::uint8_t *stored, const FormatInfo &info, int width, int height, GLenum format,
                     GLenum type, void *pixels) {
    const GlState &gl = g->gl;
    const int components = SourceComponents(format), channelBytes = TypeBytes(type);
    const int pixelBytes = components * channelBytes;
    const int rowPixels = gl.packRowLength > 0 ? gl.packRowLength : width;
    const int alignment = std::max(1, gl.packAlignment);
    const std::size_t rowBytes = (std::size_t(rowPixels) * pixelBytes + alignment - 1) / alignment * alignment;
    auto *destination = static_cast<std::uint8_t *>(pixels);
    const bool bgr = format == GL_BGRA || format == GL_BGR;
    for (int y = 0; y < height; ++y) {
        std::uint8_t *row = destination + std::size_t(y + gl.packSkipRows) * rowBytes + std::size_t(gl.packSkipPixels) * pixelBytes;
        const std::uint8_t *source = stored + std::size_t(y) * width * info.bytes;
        if (!bgr && type == GL_UNSIGNED_BYTE && info.channel == Channel::U8 && info.vk != VK_FORMAT_B8G8R8A8_UNORM) {
            if (components == info.components) {
                std::memcpy(row, source, std::size_t(width) * pixelBytes);
                continue;
            }
            for (int x = 0; x < width; ++x) {
                std::uint8_t texel[4] = {0, 0, 0, 255};
                for (int c = 0; c < info.components; ++c) texel[c] = source[x * info.bytes + c];
                std::memcpy(row + x * pixelBytes, texel, std::size_t(pixelBytes));
            }
            continue;
        }
        for (int x = 0; x < width; ++x) {
            float rgba[4];
            ReadStored(source + x * info.bytes, info, rgba);
            if (bgr) std::swap(rgba[0], rgba[2]);
            for (int c = 0; c < components; ++c) {
                std::uint8_t *out = row + x * pixelBytes + c * channelBytes;
                switch (type) {
                case GL_UNSIGNED_BYTE: *out = std::uint8_t(std::lround(std::clamp(rgba[c], 0.0f, 1.0f) * 255.0f)); break;
                case GL_UNSIGNED_SHORT: { const std::uint16_t v = std::uint16_t(std::lround(std::clamp(rgba[c], 0.0f, 1.0f) * 65535.0f)); std::memcpy(out, &v, 2); break; }
                case GL_HALF_FLOAT: { const std::uint16_t v = FloatToHalf(rgba[c]); std::memcpy(out, &v, 2); break; }
                case GL_UNSIGNED_INT: { const std::uint32_t v = std::uint32_t(std::llround(double(std::clamp(rgba[c], 0.0f, 1.0f)) * 4294967295.0)); std::memcpy(out, &v, 4); break; }
                default: std::memcpy(out, &rgba[c], 4); break;
                }
            }
        }
    }
}

Texture *Bound(GLenum target) {
    const int slot = SlotForTarget(target);
    if (slot < 0) return nullptr;
    return Lookup(g->textures, g->gl.textures[std::size_t(g->gl.activeUnit)][std::size_t(slot)]);
}

std::uint32_t FullChain(std::uint32_t width, std::uint32_t height, std::uint32_t depth) {
    std::uint32_t levels = 1, size = std::max({width, height, depth});
    while (size > 1) { size >>= 1; ++levels; }
    return levels;
}
std::uint32_t LevelSize(std::uint32_t base, std::uint32_t level) { return std::max(1u, base >> level); }

void DropViews(Texture &texture) {
    Retire(Retired::ViewKind, std::uint64_t(texture.view), g->serial);
    texture.view = VK_NULL_HANDLE;
    for (auto &entry : texture.attachmentViews) Retire(Retired::ViewKind, std::uint64_t(entry.second), g->serial);
    texture.attachmentViews.clear();
}
void Unrealize(Texture &texture) {
    DropViews(texture);
    if (texture.image) {
        // The retirement entry keeps the image alive until every command that
        // may reference it has completed, whoever else still shares it.
        RetireImage(texture.image);
        texture.image.reset();
    }
    texture.isView = false;
    for (auto &framebuffer : g->framebuffers) ++framebuffer.revision;
}

void UploadRegion(Image &image, std::uint32_t level, int x, int y, int z, int width, int height, int depth,
                  bool layered, const std::uint8_t *data, std::size_t bytes) {
    if (!bytes || !data) return;
    EndPass();
    const Range staging = RingAllocate(bytes, 16);
    std::memcpy(staging.Data(), data, bytes);
    Transition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy region{};
    region.bufferOffset = staging.offset;
    region.imageSubresource.aspectMask = image.aspect & ~VkImageAspectFlags(VK_IMAGE_ASPECT_STENCIL_BIT);
    region.imageSubresource.mipLevel = level;
    region.imageSubresource.baseArrayLayer = layered ? std::uint32_t(z) : 0;
    region.imageSubresource.layerCount = layered ? std::uint32_t(depth) : 1;
    region.imageOffset = {x, y, layered ? 0 : z};
    region.imageExtent = {std::uint32_t(width), std::uint32_t(height), layered ? 1u : std::uint32_t(depth)};
    vkCmdCopyBufferToImage(g->cmd, staging.block->buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    image.lastUse = g->serial;
}

void GenerateMips(Image &image) {
    if (image.levels < 2) return;
    EndPass();
    Transition(image, VK_IMAGE_LAYOUT_GENERAL);
    for (std::uint32_t level = 1; level < image.levels; ++level) {
        VkImageBlit blit{};
        blit.srcSubresource = {image.aspect, level - 1, 0, image.layers};
        blit.dstSubresource = {image.aspect, level, 0, image.layers};
        blit.srcOffsets[1] = {int(LevelSize(image.width, level - 1)), int(LevelSize(image.height, level - 1)), int(LevelSize(image.depth, level - 1))};
        blit.dstOffsets[1] = {int(LevelSize(image.width, level)), int(LevelSize(image.height, level)), int(LevelSize(image.depth, level))};
        vkCmdBlitImage(g->cmd, image.image, VK_IMAGE_LAYOUT_GENERAL, image.image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_LINEAR);
        MemoryBarrierAll();
    }
    image.lastUse = g->serial;
}

bool IsLayeredTarget(GLenum target) {
    return target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY;
}

void StoreSpec(Texture &texture, int level, GLenum internal, const FormatInfo &info, int width, int height, int depth,
               std::vector<std::uint8_t> data) {
    if (level < 0 || level > 20) return;
    if (level == 0 || texture.internalFormat != internal) {
        if (level == 0) texture.specs.clear();
        texture.internalFormat = internal;
        texture.format = info.vk;
    }
    if (texture.specs.size() <= std::size_t(level)) texture.specs.resize(std::size_t(level) + 1);
    LevelSpec &spec = texture.specs[std::size_t(level)];
    spec.width = width;
    spec.height = height;
    spec.depth = depth;
    spec.data = std::move(data);
    spec.specified = true;
}

void SpecifyImage(GLenum target, int level, GLenum internal, int width, int height, int depth, GLenum format,
                  GLenum type, const void *pixels, std::size_t compressedBytes) {
    Texture *texture = Bound(target);
    if (!texture) return;
    FormatInfo info;
    if (!LookupFormat(internal, info)) {
        Warn("unsupported texture internal format 0x%x", internal);
        return;
    }
    std::vector<std::uint8_t> data;
    if (info.channel == Channel::BC) {
        if (pixels) data.assign(static_cast<const std::uint8_t *>(pixels), static_cast<const std::uint8_t *>(pixels) + compressedBytes);
    } else if (pixels)
        ConvertUpload(pixels, format, type, width, height, depth, info, data);
    if (texture->image && !texture->isView) {
        Image &image = *texture->image;
        const bool layered = IsLayeredTarget(target);
        const std::uint32_t imageDepth = layered ? image.layers : LevelSize(image.depth, std::uint32_t(level));
        if (texture->format == info.vk && std::uint32_t(level) < image.levels &&
            LevelSize(image.width, std::uint32_t(level)) == std::uint32_t(width) &&
            LevelSize(image.height, std::uint32_t(level)) == std::uint32_t(height) && imageDepth == std::uint32_t(depth)) {
            texture->internalFormat = internal;
            UploadRegion(image, std::uint32_t(level), 0, 0, 0, width, height, depth, layered, data.data(), data.size());
            return;
        }
        if (level != 0) Warn("respecifying mip level %d of a used texture discards its other levels", level);
    }
    if (texture->image) Unrealize(*texture);
    texture->immutable = false;
    StoreSpec(*texture, level, internal, info, width, height, depth, std::move(data));
}

VkImageView MakeView(const Image &image, VkImageViewType type, VkImageAspectFlags aspect, std::uint32_t baseLevel,
                     std::uint32_t levels, std::uint32_t baseLayer, std::uint32_t layers) {
    VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    info.image = image.image;
    info.viewType = type;
    info.format = image.format;
    info.subresourceRange = {aspect, baseLevel, levels, baseLayer, layers};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(g->device, &info, nullptr, &view) != VK_SUCCESS) Fatal("vkCreateImageView failed");
    return view;
}

// ----------------------------------------------------------- GL entries ---
void GLAD_API_PTR GenTextures(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) names[i] = AllocateName(g->textures);
}
void GLAD_API_PTR CreateTextures(GLenum target, GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        names[i] = AllocateName(g->textures);
        g->textures[names[i]].target = target;
    }
}
void GLAD_API_PTR BindTexture(GLenum target, GLuint name) {
    const int slot = SlotForTarget(target);
    if (slot < 0) return;
    if (Texture *texture = Lookup(g->textures, name))
        if (!texture->target) { texture->target = target; MarkDirty(DirtyTextures); }
    GLuint &binding = g->gl.textures[std::size_t(g->gl.activeUnit)][std::size_t(slot)];
    if (binding == name) return;
    binding = name;
    MarkDirty(DirtyBindings);
}
void GLAD_API_PTR BindTextureUnit(GLuint unit, GLuint name) {
    if (unit >= kTextureUnits) return;
    Texture *texture = Lookup(g->textures, name);
    if (!texture) {
        g->gl.textures[unit].fill(0);
        return;
    }
    const int slot = SlotForTarget(texture->target);
    if (slot >= 0) g->gl.textures[unit][std::size_t(slot)] = name;
}
void GLAD_API_PTR ActiveTexture(GLenum unit) {
    const int index = int(unit) - int(GL_TEXTURE0);
    if (index >= 0 && index < kTextureUnits) g->gl.activeUnit = index;
}
void GLAD_API_PTR DeleteTextures(GLsizei count, const GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        Texture *texture = Lookup(g->textures, names[i]);
        if (!texture) continue;
        for (auto &unit : g->gl.textures)
            for (auto &bound : unit)
                if (bound == names[i]) bound = 0;
        for (auto &framebuffer : g->framebuffers) {
            for (auto &attachment : framebuffer.color)
                if (attachment.kind == Attachment::Tex && attachment.name == names[i]) { attachment = {}; ++framebuffer.revision; }
            if (framebuffer.depth.kind == Attachment::Tex && framebuffer.depth.name == names[i]) { framebuffer.depth = {}; ++framebuffer.revision; }
        }
        if (g->pass.active) EndPass();
        Unrealize(*texture);
        *texture = Texture{};
    }
}
GLboolean GLAD_API_PTR IsTexture(GLuint name) {
    Texture *texture = Lookup(g->textures, name);
    return texture && texture->target != 0;
}

void SetSamplerParameter(SamplerState &state, GLenum pname, float value) {
    const GLint integer = GLint(value);
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER: state.minFilter = integer; break;
    case GL_TEXTURE_MAG_FILTER: state.magFilter = integer; break;
    case GL_TEXTURE_WRAP_S: state.wrapS = integer; break;
    case GL_TEXTURE_WRAP_T: state.wrapT = integer; break;
    case GL_TEXTURE_WRAP_R: state.wrapR = integer; break;
    case GL_TEXTURE_COMPARE_MODE: state.compareMode = integer; break;
    case GL_TEXTURE_COMPARE_FUNC: state.compareFunc = integer; break;
    case GL_TEXTURE_MIN_LOD: state.minLod = value; break;
    case GL_TEXTURE_MAX_LOD: state.maxLod = value; break;
    case 0x84FE: state.maxAnisotropy = value; break; // GL_TEXTURE_MAX_ANISOTROPY
    default: break;
    }
}
void TextureParameter(Texture *texture, GLenum pname, float value) {
    if (!texture) return;
    if (pname == GL_TEXTURE_MAX_LEVEL) texture->maxLevel = int(value);
    else if (pname == GL_TEXTURE_BASE_LEVEL) texture->baseLevel = int(value);
    else SetSamplerParameter(texture->sampler, pname, value);
}
void GLAD_API_PTR TexParameteri(GLenum target, GLenum pname, GLint value) { TextureParameter(Bound(target), pname, float(value)); }
void GLAD_API_PTR TexParameterf(GLenum target, GLenum pname, GLfloat value) { TextureParameter(Bound(target), pname, value); }
void GLAD_API_PTR TexParameteriv(GLenum target, GLenum pname, const GLint *values) {
    if (values && pname != GL_TEXTURE_BORDER_COLOR) TextureParameter(Bound(target), pname, float(values[0]));
}
void GLAD_API_PTR TexParameterfv(GLenum target, GLenum pname, const GLfloat *values) {
    Texture *texture = Bound(target);
    if (!texture || !values) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) std::memcpy(texture->sampler.border, values, sizeof(float) * 4);
    else TextureParameter(texture, pname, values[0]);
}
void GLAD_API_PTR TextureParameteri(GLuint name, GLenum pname, GLint value) { TextureParameter(Lookup(g->textures, name), pname, float(value)); }

void GLAD_API_PTR TexImage2D(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height, GLint,
                             GLenum format, GLenum type, const void *pixels) {
    SpecifyImage(target, level, GLenum(internal), width, height, 1, format, type, pixels, 0);
}
void GLAD_API_PTR TexImage3D(GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height, GLsizei depth,
                             GLint, GLenum format, GLenum type, const void *pixels) {
    SpecifyImage(target, level, GLenum(internal), width, height, depth, format, type, pixels, 0);
}
void GLAD_API_PTR CompressedTexImage2D(GLenum target, GLint level, GLenum internal, GLsizei width, GLsizei height,
                                       GLint, GLsizei bytes, const void *data) {
    SpecifyImage(target, level, internal, width, height, 1, 0, 0, data, std::size_t(std::max<GLsizei>(bytes, 0)));
}
void Storage(Texture *texture, GLsizei levels, GLenum internal, GLsizei width, GLsizei height, GLsizei depth) {
    if (!texture) return;
    FormatInfo info;
    if (!LookupFormat(internal, info)) {
        Warn("unsupported texture storage format 0x%x", internal);
        return;
    }
    if (texture->image) Unrealize(*texture);
    texture->specs.clear();
    for (GLsizei level = 0; level < levels; ++level) {
        const bool layered = IsLayeredTarget(texture->target);
        StoreSpec(*texture, level, internal, info, std::max(1, width >> level), std::max(1, height >> level),
                  layered ? depth : std::max(1, depth >> level), {});
    }
    texture->immutable = true;
}
void GLAD_API_PTR TexStorage2D(GLenum target, GLsizei levels, GLenum internal, GLsizei width, GLsizei height) {
    Storage(Bound(target), levels, internal, width, height, 1);
}
void GLAD_API_PTR TexStorage3D(GLenum target, GLsizei levels, GLenum internal, GLsizei width, GLsizei height, GLsizei depth) {
    Storage(Bound(target), levels, internal, width, height, depth);
}
void GLAD_API_PTR TextureStorage2D(GLuint name, GLsizei levels, GLenum internal, GLsizei width, GLsizei height) {
    Storage(Lookup(g->textures, name), levels, internal, width, height, 1);
}

void SubImage(Texture *texture, int level, int x, int y, int z, int width, int height, int depth, GLenum format,
              GLenum type, const void *pixels) {
    if (!texture || !pixels || width <= 0 || height <= 0 || depth <= 0) return;
    if (!RealizeTexture(*texture)) return;
    const FormatInfo info = InfoForVk(texture->image->format);
    if (info.channel == Channel::BC) return;
    std::vector<std::uint8_t> data;
    ConvertUpload(pixels, format, type, width, height, depth, info, data);
    UploadRegion(*texture->image, std::uint32_t(level) + texture->viewBaseLevel, x, y,
                 z + int(IsLayeredTarget(texture->target) ? texture->viewBaseLayer : 0), width, height, depth,
                 IsLayeredTarget(texture->target), data.data(), data.size());
}
void GLAD_API_PTR TexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                                GLenum format, GLenum type, const void *pixels) {
    SubImage(Bound(target), level, x, y, 0, width, height, 1, format, type, pixels);
}
void GLAD_API_PTR TexSubImage3D(GLenum target, GLint level, GLint x, GLint y, GLint z, GLsizei width, GLsizei height,
                                GLsizei depth, GLenum format, GLenum type, const void *pixels) {
    SubImage(Bound(target), level, x, y, z, width, height, depth, format, type, pixels);
}
void GLAD_API_PTR TextureSubImage2D(GLuint name, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
                                    GLenum format, GLenum type, const void *pixels) {
    SubImage(Lookup(g->textures, name), level, x, y, 0, width, height, 1, format, type, pixels);
}

void GLAD_API_PTR GenerateMipmap(GLenum target) {
    Texture *texture = Bound(target);
    if (!texture) return;
    if (!texture->image) {
        texture->wantMipmaps = true;
        return;
    }
    Image &old = *texture->image;
    const std::uint32_t full = FullChain(old.width, old.height, old.type == VK_IMAGE_TYPE_3D ? old.depth : 1);
    if (old.levels < full && !texture->isView) {
        // The image was created without a chain: move level 0 to a full one.
        EndPass();
        auto replacement = CreateImage(old.type, old.format, old.width, old.height, old.depth, old.layers, full, true);
        Transition(old, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        Transition(*replacement, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageCopy copy{};
        copy.srcSubresource = {old.aspect, 0, 0, old.layers};
        copy.dstSubresource = copy.srcSubresource;
        copy.extent = {old.width, old.height, old.depth};
        vkCmdCopyImage(g->cmd, old.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, replacement->image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        old.lastUse = g->serial;
        Unrealize(*texture);
        texture->image = std::move(replacement);
    }
    GenerateMips(*texture->image);
}

void GLAD_API_PTR TextureView(GLuint name, GLenum target, GLuint original, GLenum, GLuint minLevel, GLuint levels,
                              GLuint minLayer, GLuint layers) {
    Texture *texture = Lookup(g->textures, name), *source = Lookup(g->textures, original);
    if (!texture || !source || !RealizeTexture(*source)) return;
    if (texture->image) Unrealize(*texture);
    texture->target = target;
    texture->image = source->image;
    texture->format = source->format;
    texture->internalFormat = source->internalFormat;
    texture->isView = true;
    texture->viewBaseLevel = source->viewBaseLevel + minLevel;
    texture->viewLevels = levels;
    texture->viewBaseLayer = source->viewBaseLayer + minLayer;
    texture->viewLayers = layers;
    texture->sampler = source->sampler;
}

void GLAD_API_PTR TexBuffer(GLenum target, GLenum internal, GLuint buffer) {
    Texture *texture = Bound(target);
    if (!texture || (texture->buffer == buffer && texture->bufferFormat == internal)) return;
    texture->buffer = buffer;
    texture->bufferFormat = internal;
    MarkDirty(DirtyTextures | DirtyBuffers);
}

struct Readback {
    Range staging;
    FormatInfo info;
    int width = 0, height = 0;
};
bool ReadImage(Image &image, VkImageAspectFlags aspect, std::uint32_t level, std::uint32_t layer, int x, int y,
               int width, int height, Readback &out) {
    if (width <= 0 || height <= 0) return false;
    out.info = InfoForVk(image.format);
    const std::size_t bytes = out.info.channel == Channel::BC
        ? std::size_t((width + 3) / 4) * ((height + 3) / 4) * out.info.blockBytes
        : std::size_t(width) * height * out.info.bytes;
    out.staging = AllocateBuffer(bytes);
    std::memset(out.staging.Data(), 0, bytes);
    out.width = width;
    out.height = height;
    EndPass();
    Transition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // Clip to the image; texels outside stay zero like an undefined GL read.
    const int levelWidth = int(LevelSize(image.width, level)), levelHeight = int(LevelSize(image.height, level));
    const int x0 = std::max(x, 0), y0 = std::max(y, 0), x1 = std::min(x + width, levelWidth), y1 = std::min(y + height, levelHeight);
    if (x1 > x0 && y1 > y0) {
        VkBufferImageCopy region{};
        region.bufferOffset = out.staging.offset + (std::size_t(y0 - y) * width + (x0 - x)) * std::size_t(out.info.bytes);
        region.bufferRowLength = std::uint32_t(width);
        region.bufferImageHeight = std::uint32_t(height);
        region.imageSubresource = {aspect, level, layer, 1};
        region.imageOffset = {x0, y0, 0};
        region.imageExtent = {std::uint32_t(x1 - x0), std::uint32_t(y1 - y0), 1};
        if (out.info.channel == Channel::BC) {
            region.bufferOffset = out.staging.offset;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
        }
        vkCmdCopyImageToBuffer(g->cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, out.staging.block->buffer, 1, &region);
    }
    image.lastUse = g->serial;
    Flush(true);
    return true;
}

void GLAD_API_PTR ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
    ResolvedAttachment source;
    const bool depth = format == GL_DEPTH_COMPONENT;
    if (depth ? !ResolveFramebufferDepth(g->gl.readFbo, source) : !ResolveFramebufferColor(g->gl.readFbo, -1, source)) return;
    Readback readback;
    if (!ReadImage(*source.image, depth ? VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT) : VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT),
                   source.level, source.layer, x, y, width, height, readback))
        return;
    void *destination = pixels;
    std::vector<std::uint8_t> temporary;
    Buffer *pack = GetBuffer(g->gl.pixelPack);
    if (pack) {
        PrepareBufferWrite(*pack, false);
        destination = pack->storage.Data() + reinterpret_cast<std::uintptr_t>(pixels);
    }
    ConvertReadback(readback.staging.Data(), readback.info, width, height, format, type, destination);
    RetireRange(readback.staging, g->completedSerial);
}

void GLAD_API_PTR GetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels) {
    Texture *texture = Bound(target);
    if (!texture || !RealizeTexture(*texture)) return;
    Image &image = *texture->image;
    const std::uint32_t mip = std::uint32_t(level) + texture->viewBaseLevel;
    Readback readback;
    if (InfoForVk(image.format).channel == Channel::BC) {
        // OpenGL returns decoded texels for a compressed texture: decode on the
        // device by blitting the level into an uncompressed image of the same
        // colour encoding, then read that.
        const bool srgb = image.format == VK_FORMAT_BC1_RGB_SRGB_BLOCK || image.format == VK_FORMAT_BC1_RGBA_SRGB_BLOCK ||
                          image.format == VK_FORMAT_BC2_SRGB_BLOCK || image.format == VK_FORMAT_BC3_SRGB_BLOCK;
        const int width = int(LevelSize(image.width, mip)), height = int(LevelSize(image.height, mip));
        auto decoded = CreateImage(VK_IMAGE_TYPE_2D, srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
                                   std::uint32_t(width), std::uint32_t(height), 1, 1, 1, true);
        EndPass();
        Transition(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        Transition(*decoded, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, texture->viewBaseLayer, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[1] = blit.dstOffsets[1] = {width, height, 1};
        vkCmdBlitImage(g->cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, decoded->image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
        image.lastUse = g->serial;
        if (ReadImage(*decoded, VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 0, 0, width, height, readback)) {
            ConvertReadback(readback.staging.Data(), readback.info, readback.width, readback.height, format, type, pixels);
            RetireRange(readback.staging, g->completedSerial);
        }
        RetireImage(decoded);
        return;
    }
    const VkImageAspectFlags aspect = IsDepthFormat(image.format) ? VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT)
                                                                  : VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT);
    if (!ReadImage(image, aspect, mip, texture->viewBaseLayer, 0, 0, int(LevelSize(image.width, mip)),
                   int(LevelSize(image.height, mip)), readback))
        return;
    ConvertReadback(readback.staging.Data(), readback.info, readback.width, readback.height, format, type, pixels);
    RetireRange(readback.staging, g->completedSerial);
}
void GLAD_API_PTR GetCompressedTexImage(GLenum target, GLint level, void *pixels) {
    Texture *texture = Bound(target);
    if (!texture || !RealizeTexture(*texture)) return;
    Image &image = *texture->image;
    const std::uint32_t mip = std::uint32_t(level);
    Readback readback;
    if (!ReadImage(image, VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 0, 0, int(LevelSize(image.width, mip)),
                   int(LevelSize(image.height, mip)), readback))
        return;
    const std::size_t bytes = std::size_t((readback.width + 3) / 4) * ((readback.height + 3) / 4) * readback.info.blockBytes;
    std::memcpy(pixels, readback.staging.Data(), bytes);
    RetireRange(readback.staging, g->completedSerial);
}
void GLAD_API_PTR GetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *value) {
    Texture *texture = Bound(target);
    if (!value) return;
    *value = 0;
    if (!texture) return;
    int width = 0, height = 0, depth = 0;
    VkFormat format = texture->format;
    if (texture->image) {
        const std::uint32_t mip = std::uint32_t(level) + texture->viewBaseLevel;
        if (mip < texture->image->levels) {
            width = int(LevelSize(texture->image->width, mip));
            height = int(LevelSize(texture->image->height, mip));
            depth = int(texture->image->type == VK_IMAGE_TYPE_3D ? LevelSize(texture->image->depth, mip) : texture->image->layers);
        }
        format = texture->image->format;
    } else if (std::size_t(level) < texture->specs.size() && texture->specs[std::size_t(level)].specified) {
        width = texture->specs[std::size_t(level)].width;
        height = texture->specs[std::size_t(level)].height;
        depth = texture->specs[std::size_t(level)].depth;
    }
    const FormatInfo info = InfoForVk(format);
    switch (pname) {
    case GL_TEXTURE_WIDTH: *value = width; break;
    case GL_TEXTURE_HEIGHT: *value = height; break;
    case GL_TEXTURE_DEPTH: *value = depth; break;
    case GL_TEXTURE_INTERNAL_FORMAT: *value = GLint(texture->internalFormat); break;
    case GL_TEXTURE_COMPRESSED: *value = info.channel == Channel::BC && format != VK_FORMAT_UNDEFINED; break;
    case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
        *value = info.channel == Channel::BC ? ((width + 3) / 4) * ((height + 3) / 4) * info.blockBytes : 0;
        break;
    default: break;
    }
}

void BlitImages(Image &source, std::uint32_t sourceLevel, std::uint32_t sourceLayer, Image &destination,
                std::uint32_t destinationLevel, std::uint32_t destinationLayer, const int src[4], const int dst[4],
                VkImageAspectFlags aspect, VkFilter filter) {
    EndPass();
    const bool same = &source == &destination;
    const VkImageLayout sourceLayout = same ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkImageLayout destinationLayout = same ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Transition(source, sourceLayout);
    if (!same) Transition(destination, destinationLayout);
    const bool sameSize = src[2] - src[0] == dst[2] - dst[0] && src[3] - src[1] == dst[3] - dst[1] &&
                          src[2] > src[0] && src[3] > src[1];
    const bool depth = (aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
    if (sameSize && source.format == destination.format && (depth || filter == VK_FILTER_NEAREST)) {
        // 1:1 transfer: an exact copy, also valid for depth formats which
        // commonly cannot be blit destinations.
        int sx = src[0], sy = src[1], dx = dst[0], dy = dst[1], width = src[2] - src[0], height = src[3] - src[1];
        const int sourceWidth = int(LevelSize(source.width, sourceLevel)), sourceHeight = int(LevelSize(source.height, sourceLevel));
        const int destinationWidth = int(LevelSize(destination.width, destinationLevel)), destinationHeight = int(LevelSize(destination.height, destinationLevel));
        if (sx < 0) { dx -= sx; width += sx; sx = 0; }
        if (sy < 0) { dy -= sy; height += sy; sy = 0; }
        if (dx < 0) { sx -= dx; width += dx; dx = 0; }
        if (dy < 0) { sy -= dy; height += dy; dy = 0; }
        width = std::min({width, sourceWidth - sx, destinationWidth - dx});
        height = std::min({height, sourceHeight - sy, destinationHeight - dy});
        if (width > 0 && height > 0) {
            VkImageCopy copy{};
            copy.srcSubresource = {source.aspect, sourceLevel, sourceLayer, 1};
            copy.dstSubresource = {destination.aspect, destinationLevel, destinationLayer, 1};
            copy.srcOffset = {sx, sy, 0};
            copy.dstOffset = {dx, dy, 0};
            copy.extent = {std::uint32_t(width), std::uint32_t(height), 1};
            vkCmdCopyImage(g->cmd, source.image, sourceLayout, destination.image, destinationLayout, 1, &copy);
        }
    } else if (depth) {
        Warn("scaled depth blits are not supported by the Vulkan backend");
    } else {
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, sourceLevel, sourceLayer, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, destinationLevel, destinationLayer, 1};
        blit.srcOffsets[0] = {src[0], src[1], 0};
        blit.srcOffsets[1] = {src[2], src[3], 1};
        blit.dstOffsets[0] = {dst[0], dst[1], 0};
        blit.dstOffsets[1] = {dst[2], dst[3], 1};
        vkCmdBlitImage(g->cmd, source.image, sourceLayout, destination.image, destinationLayout, 1, &blit, filter);
    }
    source.lastUse = destination.lastUse = g->serial;
}

void GLAD_API_PTR BlitFramebuffer(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1, GLint dy1,
                                  GLbitfield mask, GLenum filter) {
    const int src[4] = {sx0, sy0, sx1, sy1}, dst[4] = {dx0, dy0, dx1, dy1};
    if (mask & GL_COLOR_BUFFER_BIT) {
        ResolvedAttachment source, destination;
        if (ResolveFramebufferColor(g->gl.readFbo, -1, source) && ResolveFramebufferColor(g->gl.drawFbo, 0, destination))
            BlitImages(*source.image, source.level, source.layer, *destination.image, destination.level, destination.layer,
                       src, dst, VK_IMAGE_ASPECT_COLOR_BIT, filter == GL_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
    }
    if (mask & GL_DEPTH_BUFFER_BIT) {
        ResolvedAttachment source, destination;
        if (ResolveFramebufferDepth(g->gl.readFbo, source) && ResolveFramebufferDepth(g->gl.drawFbo, destination))
            BlitImages(*source.image, source.level, source.layer, *destination.image, destination.level, destination.layer,
                       src, dst, VK_IMAGE_ASPECT_DEPTH_BIT, VK_FILTER_NEAREST);
    }
}

void GLAD_API_PTR CopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
                                    GLsizei width, GLsizei height) {
    Texture *texture = Bound(target);
    if (!texture || !RealizeTexture(*texture)) return;
    ResolvedAttachment source;
    const bool depth = IsDepthFormat(texture->image->format);
    if (depth ? !ResolveFramebufferDepth(g->gl.readFbo, source) : !ResolveFramebufferColor(g->gl.readFbo, -1, source)) return;
    const int src[4] = {x, y, x + width, y + height}, dst[4] = {xoffset, yoffset, xoffset + width, yoffset + height};
    BlitImages(*source.image, source.level, source.layer, *texture->image, std::uint32_t(level) + texture->viewBaseLevel,
               texture->viewBaseLayer, src, dst, depth ? VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT) : VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT),
               VK_FILTER_NEAREST);
}

// ---- samplers ----
void GLAD_API_PTR GenSamplers(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        names[i] = AllocateName(g->samplerObjects);
        // Sampler objects default to GL_NEAREST_MIPMAP_LINEAR / GL_LINEAR / GL_REPEAT.
    }
}
void GLAD_API_PTR DeleteSamplers(GLsizei count, const GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        if (!Lookup(g->samplerObjects, names[i])) continue;
        for (auto &bound : g->gl.samplers)
            if (bound == names[i]) bound = 0;
        g->samplerObjects[names[i]] = SamplerObject{};
    }
}
void GLAD_API_PTR BindSampler(GLuint unit, GLuint name) {
    if (unit >= kTextureUnits || g->gl.samplers[unit] == name) return;
    g->gl.samplers[unit] = name;
    MarkDirty(DirtyBindings);
}
GLboolean GLAD_API_PTR IsSampler(GLuint name) { return Lookup(g->samplerObjects, name) != nullptr; }
void GLAD_API_PTR SamplerParameteri(GLuint name, GLenum pname, GLint value) {
    if (SamplerObject *object = Lookup(g->samplerObjects, name)) SetSamplerParameter(object->state, pname, float(value));
}
void GLAD_API_PTR SamplerParameterf(GLuint name, GLenum pname, GLfloat value) {
    if (SamplerObject *object = Lookup(g->samplerObjects, name)) SetSamplerParameter(object->state, pname, value);
}
void GLAD_API_PTR SamplerParameterfv(GLuint name, GLenum pname, const GLfloat *values) {
    SamplerObject *object = Lookup(g->samplerObjects, name);
    if (!object || !values) return;
    if (pname == GL_TEXTURE_BORDER_COLOR) std::memcpy(object->state.border, values, sizeof(float) * 4);
    else SetSamplerParameter(object->state, pname, values[0]);
}

// ---- renderbuffers ----
void GLAD_API_PTR GenRenderbuffers(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) names[i] = AllocateName(g->renderbuffers);
}
void GLAD_API_PTR BindRenderbuffer(GLenum, GLuint name) {
    g->gl.renderbuffer = name;
    if (Renderbuffer *object = Lookup(g->renderbuffers, name)) object->bound = true;
}
void ReleaseRenderbuffer(Renderbuffer &object) {
    Retire(Retired::ViewKind, std::uint64_t(object.view), g->serial);
    object.view = VK_NULL_HANDLE;
    RetireImage(object.image);
    object.image.reset();
}
void GLAD_API_PTR RenderbufferStorage(GLenum, GLenum internal, GLsizei width, GLsizei height) {
    Renderbuffer *object = Lookup(g->renderbuffers, g->gl.renderbuffer);
    FormatInfo info;
    if (!object || !LookupFormat(internal, info)) return;
    EndPass();
    ReleaseRenderbuffer(*object);
    object->image = CreateImage(VK_IMAGE_TYPE_2D, info.vk, std::uint32_t(width), std::uint32_t(height), 1, 1, 1, true);
    object->view = MakeView(*object->image, VK_IMAGE_VIEW_TYPE_2D, object->image->aspect, 0, 1, 0, 1);
    object->internalFormat = internal;
    object->width = width;
    object->height = height;
    for (auto &framebuffer : g->framebuffers) ++framebuffer.revision;
}
void GLAD_API_PTR RenderbufferStorageMultisample(GLenum target, GLsizei, GLenum internal, GLsizei width, GLsizei height) {
    RenderbufferStorage(target, internal, width, height);
}
void GLAD_API_PTR DeleteRenderbuffers(GLsizei count, const GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        Renderbuffer *object = Lookup(g->renderbuffers, names[i]);
        if (!object) continue;
        EndPass();
        for (auto &framebuffer : g->framebuffers) {
            for (auto &attachment : framebuffer.color)
                if (attachment.kind == Attachment::Rb && attachment.name == names[i]) { attachment = {}; ++framebuffer.revision; }
            if (framebuffer.depth.kind == Attachment::Rb && framebuffer.depth.name == names[i]) { framebuffer.depth = {}; ++framebuffer.revision; }
        }
        ReleaseRenderbuffer(*object);
        if (g->gl.renderbuffer == names[i]) g->gl.renderbuffer = 0;
        *object = Renderbuffer{};
    }
}
GLboolean GLAD_API_PTR IsRenderbuffer(GLuint name) {
    Renderbuffer *object = Lookup(g->renderbuffers, name);
    return object && object->bound;
}

// ---- framebuffers ----
void GLAD_API_PTR GenFramebuffers(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) names[i] = AllocateName(g->framebuffers);
}
void GLAD_API_PTR CreateFramebuffers(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        names[i] = AllocateName(g->framebuffers);
        g->framebuffers[names[i]].bound = true;
    }
}
void GLAD_API_PTR BindFramebuffer(GLenum target, GLuint name) {
    if (Framebuffer *object = Lookup(g->framebuffers, name)) object->bound = true;
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) g->gl.drawFbo = name;
    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) g->gl.readFbo = name;
}
void GLAD_API_PTR DeleteFramebuffers(GLsizei count, const GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        if (!Lookup(g->framebuffers, names[i])) continue;
        if (g->pass.active && g->pass.fbo == names[i]) EndPass();
        if (g->gl.drawFbo == names[i]) g->gl.drawFbo = 0;
        if (g->gl.readFbo == names[i]) g->gl.readFbo = 0;
        g->framebuffers[names[i]] = Framebuffer{};
    }
}
GLboolean GLAD_API_PTR IsFramebuffer(GLuint name) {
    Framebuffer *object = Lookup(g->framebuffers, name);
    return object && object->bound;
}
Framebuffer *TargetFramebuffer(GLenum target) {
    return Lookup(g->framebuffers, target == GL_READ_FRAMEBUFFER ? g->gl.readFbo : g->gl.drawFbo);
}
void Attach(Framebuffer *framebuffer, GLenum point, Attachment attachment) {
    if (!framebuffer) return;
    if (point >= GL_COLOR_ATTACHMENT0 && point < GL_COLOR_ATTACHMENT0 + 8) framebuffer->color[point - GL_COLOR_ATTACHMENT0] = attachment;
    else if (point == GL_DEPTH_ATTACHMENT) framebuffer->depth = attachment;
    else if (point == GL_STENCIL_ATTACHMENT) framebuffer->stencil = attachment;
    else if (point == GL_DEPTH_STENCIL_ATTACHMENT) framebuffer->depth = framebuffer->stencil = attachment;
    ++framebuffer->revision;
}
void GLAD_API_PTR FramebufferTexture2D(GLenum target, GLenum point, GLenum textarget, GLuint texture, GLint level) {
    Attachment attachment;
    if (texture) {
        attachment.kind = Attachment::Tex;
        attachment.name = texture;
        attachment.level = level;
        if (textarget >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && textarget <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
            attachment.layer = int(textarget - GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    }
    Attach(TargetFramebuffer(target), point, attachment);
}
void GLAD_API_PTR FramebufferTextureLayer(GLenum target, GLenum point, GLuint texture, GLint level, GLint layer) {
    Attachment attachment;
    if (texture) {
        attachment.kind = Attachment::Tex;
        attachment.name = texture;
        attachment.level = level;
        attachment.layer = layer;
    }
    Attach(TargetFramebuffer(target), point, attachment);
}
void GLAD_API_PTR FramebufferTexture(GLenum target, GLenum point, GLuint texture, GLint level) {
    FramebufferTextureLayer(target, point, texture, level, 0);
}
void GLAD_API_PTR NamedFramebufferTexture(GLuint name, GLenum point, GLuint texture, GLint level) {
    Attachment attachment;
    if (texture) {
        attachment.kind = Attachment::Tex;
        attachment.name = texture;
        attachment.level = level;
    }
    Attach(Lookup(g->framebuffers, name), point, attachment);
}
void GLAD_API_PTR FramebufferRenderbuffer(GLenum target, GLenum point, GLenum, GLuint renderbuffer) {
    Attachment attachment;
    if (renderbuffer) {
        attachment.kind = Attachment::Rb;
        attachment.name = renderbuffer;
    }
    Attach(TargetFramebuffer(target), point, attachment);
}
GLenum Status(GLuint name) {
    if (!name) return GL_FRAMEBUFFER_COMPLETE;
    Framebuffer *framebuffer = Lookup(g->framebuffers, name);
    if (!framebuffer) return GL_FRAMEBUFFER_UNDEFINED;
    bool any = false;
    ResolvedAttachment resolved;
    for (const auto &attachment : framebuffer->color) {
        if (attachment.kind == Attachment::None) continue;
        if (!ResolveAttachment(attachment, resolved) || IsDepthFormat(resolved.image->format) ||
            !(resolved.image->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        any = true;
    }
    if (framebuffer->depth.kind != Attachment::None) {
        if (!ResolveAttachment(framebuffer->depth, resolved) || !IsDepthFormat(resolved.image->format))
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        any = true;
    }
    return any ? GL_FRAMEBUFFER_COMPLETE : GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
}
GLenum GLAD_API_PTR CheckFramebufferStatus(GLenum target) {
    return Status(target == GL_READ_FRAMEBUFFER ? g->gl.readFbo : g->gl.drawFbo);
}
GLenum GLAD_API_PTR CheckNamedFramebufferStatus(GLuint name, GLenum) { return Status(name); }
void GLAD_API_PTR DrawBuffers(GLsizei count, const GLenum *buffers) {
    Framebuffer *framebuffer = Lookup(g->framebuffers, g->gl.drawFbo);
    if (!framebuffer) return;
    framebuffer->drawBuffers.fill(GL_NONE);
    framebuffer->drawBufferCount = std::min<int>(count, 8);
    for (int i = 0; i < framebuffer->drawBufferCount; ++i) framebuffer->drawBuffers[std::size_t(i)] = buffers[i];
    ++framebuffer->revision;
}
void GLAD_API_PTR DrawBuffer(GLenum buffer) { DrawBuffers(1, &buffer); }
void GLAD_API_PTR ReadBuffer(GLenum buffer) {
    if (Framebuffer *framebuffer = Lookup(g->framebuffers, g->gl.readFbo)) framebuffer->readBuffer = buffer;
}
void GLAD_API_PTR InvalidateFramebuffer(GLenum, GLsizei, const GLenum *) {}

template <class F> void (*Erase(F function))(void) { return reinterpret_cast<void (*)(void)>(function); }
} // namespace

int SlotForTarget(GLenum target) {
    switch (target) {
    case GL_TEXTURE_2D: return Slot2D;
    case GL_TEXTURE_2D_ARRAY: return Slot2DArray;
    case GL_TEXTURE_CUBE_MAP: return SlotCube;
    case GL_TEXTURE_CUBE_MAP_ARRAY: return SlotCubeArray;
    case GL_TEXTURE_3D: return Slot3D;
    case GL_TEXTURE_BUFFER: return SlotBuffer;
    default: return -1;
    }
}

Texture *GetTexture(GLuint name) { return Lookup(g->textures, name); }

bool RealizeTexture(Texture &texture) {
    if (texture.image) return true;
    if (texture.specs.empty() || !texture.specs[0].specified || texture.target == GL_TEXTURE_BUFFER) return false;
    const LevelSpec &base = texture.specs[0];
    std::uint32_t specified = 0;
    while (specified < texture.specs.size() && texture.specs[specified].specified) ++specified;
    const bool layered = IsLayeredTarget(texture.target);
    const bool volume = texture.target == GL_TEXTURE_3D;
    std::uint32_t levels = specified;
    if (texture.wantMipmaps && !texture.immutable)
        levels = FullChain(std::uint32_t(base.width), std::uint32_t(base.height), volume ? std::uint32_t(base.depth) : 1);
    if (texture.maxLevel >= 0) levels = std::min(levels, std::uint32_t(texture.maxLevel) + 1);
    levels = std::max(levels, 1u);
    const FormatInfo info = InfoForVk(texture.format);
    texture.image = CreateImage(volume ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D, texture.format, std::uint32_t(base.width),
                                std::uint32_t(base.height), volume ? std::uint32_t(base.depth) : 1,
                                layered ? std::uint32_t(base.depth) : 1, levels, info.channel != Channel::BC);
    texture.isView = false;
    texture.viewBaseLayer = texture.viewBaseLevel = 0;
    texture.viewLayers = texture.viewLevels = 0;
    bool anyData = false;
    for (std::uint32_t level = 0; level < std::min(levels, specified); ++level) {
        const LevelSpec &spec = texture.specs[level];
        if (spec.data.empty()) continue;
        UploadRegion(*texture.image, level, 0, 0, 0, spec.width, spec.height, spec.depth, layered, spec.data.data(), spec.data.size());
        anyData = true;
    }
    if (!anyData) {
        // OpenGL leaves unspecified storage undefined; define it so sampling and
        // LOAD attachments are valid everywhere.
        EndPass();
        Transition(*texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkImageSubresourceRange range{texture.image->aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        if (info.channel == Channel::BC) {
            // Compressed storage cannot be cleared; it is written before use.
        } else if (IsDepthFormat(texture.format)) {
            VkClearDepthStencilValue clear{1.0f, 0};
            vkCmdClearDepthStencilImage(g->cmd, texture.image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        } else {
            VkClearColorValue clear{};
            vkCmdClearColorImage(g->cmd, texture.image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        }
    }
    if (texture.wantMipmaps && levels > 1 && specified <= 1) GenerateMips(*texture.image);
    texture.wantMipmaps = false;
    texture.specs.clear();
    texture.specs.shrink_to_fit();
    return true;
}

VkImageView SamplingView(Texture &texture) {
    if (!RealizeTexture(texture)) return VK_NULL_HANDLE;
    Image &image = *texture.image;
    if (texture.view && texture.viewMaxLevel == texture.maxLevel && texture.viewBaseLevelUsed == texture.baseLevel)
        return texture.view;
    if (texture.view) Retire(Retired::ViewKind, std::uint64_t(texture.view), g->serial);
    const std::uint32_t firstLevel = texture.viewBaseLevel + std::uint32_t(std::max(texture.baseLevel, 0));
    std::uint32_t available = texture.isView && texture.viewLevels ? texture.viewLevels : image.levels - texture.viewBaseLevel;
    std::uint32_t levels = available > std::uint32_t(std::max(texture.baseLevel, 0)) ? available - std::uint32_t(std::max(texture.baseLevel, 0)) : 1;
    if (texture.maxLevel >= texture.baseLevel) levels = std::min(levels, std::uint32_t(texture.maxLevel - texture.baseLevel) + 1);
    const std::uint32_t layerCount = texture.isView && texture.viewLayers ? texture.viewLayers : image.layers - texture.viewBaseLayer;
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
    std::uint32_t layers = 1;
    switch (texture.target) {
    case GL_TEXTURE_2D_ARRAY: type = VK_IMAGE_VIEW_TYPE_2D_ARRAY; layers = layerCount; break;
    case GL_TEXTURE_CUBE_MAP: type = VK_IMAGE_VIEW_TYPE_CUBE; layers = 6; break;
    case GL_TEXTURE_CUBE_MAP_ARRAY: type = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY; layers = layerCount / 6 * 6; break;
    case GL_TEXTURE_3D: type = VK_IMAGE_VIEW_TYPE_3D; break;
    default: break;
    }
    const VkImageAspectFlags aspect = IsDepthFormat(image.format) ? VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT) : image.aspect;
    texture.view = MakeView(image, type, aspect, firstLevel, levels, texture.viewBaseLayer, std::max(layers, 1u));
    texture.viewMaxLevel = texture.maxLevel;
    texture.viewBaseLevelUsed = texture.baseLevel;
    return texture.view;
}

VkSampler GetSampler(const SamplerState &state, bool mipmapped, bool shadow) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](std::uint64_t value) { hash = (hash ^ value) * 1099511628211ull; };
    mix(std::uint64_t(state.minFilter)); mix(std::uint64_t(state.magFilter));
    mix(std::uint64_t(state.wrapS)); mix(std::uint64_t(state.wrapT)); mix(std::uint64_t(state.wrapR));
    mix(std::uint64_t(state.compareMode)); mix(std::uint64_t(state.compareFunc));
    std::uint32_t bits[7];
    std::memcpy(&bits[0], &state.maxAnisotropy, 4); std::memcpy(&bits[1], &state.minLod, 4); std::memcpy(&bits[2], &state.maxLod, 4);
    std::memcpy(&bits[3], state.border, 16);
    for (auto value : bits) mix(value);
    mix(mipmapped); mix(shadow);
    const auto found = g->samplerCache.find(hash);
    if (found != g->samplerCache.end()) return found->second;
    const auto address = [](GLint wrap) {
        switch (wrap) {
        case GL_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case GL_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case GL_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    };
    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.magFilter = state.magFilter == GL_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    const bool minLinear = state.minFilter == GL_LINEAR || state.minFilter == GL_LINEAR_MIPMAP_NEAREST || state.minFilter == GL_LINEAR_MIPMAP_LINEAR;
    const bool usesMips = state.minFilter != GL_NEAREST && state.minFilter != GL_LINEAR;
    info.minFilter = minLinear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    info.mipmapMode = (state.minFilter == GL_NEAREST_MIPMAP_LINEAR || state.minFilter == GL_LINEAR_MIPMAP_LINEAR)
                          ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU = address(state.wrapS);
    info.addressModeV = address(state.wrapT);
    info.addressModeW = address(state.wrapR);
    info.minLod = usesMips ? std::max(state.minLod, 0.0f) : 0.0f;
    info.maxLod = usesMips && mipmapped ? std::min(state.maxLod, VK_LOD_CLAMP_NONE) : 0.25f;
    if (state.maxAnisotropy > 1.0f && g->anisotropy) {
        info.anisotropyEnable = VK_TRUE;
        info.maxAnisotropy = std::min(state.maxAnisotropy, g->properties.limits.maxSamplerAnisotropy);
    }
    if (shadow || state.compareMode == GL_COMPARE_REF_TO_TEXTURE) {
        info.compareEnable = VK_TRUE;
        switch (state.compareFunc) {
        case GL_LESS: info.compareOp = VK_COMPARE_OP_LESS; break;
        case GL_GEQUAL: info.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; break;
        case GL_GREATER: info.compareOp = VK_COMPARE_OP_GREATER; break;
        case GL_EQUAL: info.compareOp = VK_COMPARE_OP_EQUAL; break;
        case GL_NOTEQUAL: info.compareOp = VK_COMPARE_OP_NOT_EQUAL; break;
        case GL_ALWAYS: info.compareOp = VK_COMPARE_OP_ALWAYS; break;
        case GL_NEVER: info.compareOp = VK_COMPARE_OP_NEVER; break;
        default: info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; break;
        }
    }
    const bool opaque = state.border[3] >= 1.0f;
    const bool white = opaque && state.border[0] >= 1.0f;
    info.borderColor = white ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE : opaque ? VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK
                                                                           : VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(g->device, &info, nullptr, &sampler) != VK_SUCCESS) Fatal("vkCreateSampler failed");
    g->samplerCache[hash] = sampler;
    return sampler;
}

void CreateDummyTextures() {
    const auto make = [](GLenum target, int slot, int layers, bool volume) {
        const GLuint name = AllocateName(g->textures);
        Texture &texture = g->textures[name];
        texture.target = target;
        texture.format = VK_FORMAT_R8G8B8A8_UNORM;
        texture.internalFormat = GL_RGBA8;
        texture.sampler.minFilter = GL_NEAREST;
        texture.sampler.magFilter = GL_NEAREST;
        texture.image = CreateImage(volume ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, 1,
                                    std::uint32_t(layers), 1, false);
        // An incomplete OpenGL texture samples as opaque black.
        Transition(*texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue clear{};
        clear.float32[3] = 1.0f;
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS};
        vkCmdClearColorImage(g->cmd, texture.image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        g->dummyTextures[std::size_t(slot)] = name;
    };
    make(GL_TEXTURE_2D, Slot2D, 1, false);
    make(GL_TEXTURE_2D_ARRAY, Slot2DArray, 1, false);
    make(GL_TEXTURE_CUBE_MAP, SlotCube, 6, false);
    make(GL_TEXTURE_CUBE_MAP_ARRAY, SlotCubeArray, 6, false);
    make(GL_TEXTURE_3D, Slot3D, 1, true);
}

bool ResolveAttachment(const Attachment &attachment, ResolvedAttachment &out) {
    if (attachment.kind == Attachment::Rb) {
        Renderbuffer *renderbuffer = Lookup(g->renderbuffers, attachment.name);
        if (!renderbuffer || !renderbuffer->image) return false;
        out.image = renderbuffer->image;
        out.view = renderbuffer->view;
        out.width = renderbuffer->image->width;
        out.height = renderbuffer->image->height;
        out.layer = out.level = 0;
        return true;
    }
    if (attachment.kind != Attachment::Tex) return false;
    Texture *texture = Lookup(g->textures, attachment.name);
    if (!texture || !RealizeTexture(*texture)) return false;
    Image &image = *texture->image;
    const std::uint32_t level = texture->viewBaseLevel + std::uint32_t(attachment.level);
    const std::uint32_t layer = texture->viewBaseLayer + std::uint32_t(attachment.layer);
    if (level >= image.levels || layer >= image.layers || image.type == VK_IMAGE_TYPE_3D) return false;
    const std::uint64_t key = (std::uint64_t(level) << 32) | layer;
    auto found = texture->attachmentViews.find(key);
    if (found == texture->attachmentViews.end())
        found = texture->attachmentViews.emplace(key, MakeView(image, VK_IMAGE_VIEW_TYPE_2D, image.aspect, level, 1, layer, 1)).first;
    out.image = texture->image;
    out.view = found->second;
    out.width = LevelSize(image.width, level);
    out.height = LevelSize(image.height, level);
    out.layer = layer;
    out.level = level;
    return true;
}

bool ResolveFramebufferColor(GLuint fbo, int index, ResolvedAttachment &out) {
    if (!fbo) {
        if (index > 0) return false;
        out.image = g->defaultColor;
        out.view = g->defaultColorView;
        out.width = std::uint32_t(g->drawableWidth);
        out.height = std::uint32_t(g->drawableHeight);
        out.layer = out.level = 0;
        return true;
    }
    Framebuffer *framebuffer = Lookup(g->framebuffers, fbo);
    if (!framebuffer) return false;
    // index < 0 selects the read buffer; otherwise draw buffer `index`.
    const GLenum buffer = index < 0 ? framebuffer->readBuffer : framebuffer->drawBuffers[std::size_t(index)];
    if (buffer < GL_COLOR_ATTACHMENT0 || buffer >= GL_COLOR_ATTACHMENT0 + 8) return false;
    return ResolveAttachment(framebuffer->color[buffer - GL_COLOR_ATTACHMENT0], out);
}
bool ResolveFramebufferDepth(GLuint fbo, ResolvedAttachment &out) {
    if (!fbo) {
        out.image = g->defaultDepth;
        out.view = g->defaultDepthView;
        out.width = std::uint32_t(g->drawableWidth);
        out.height = std::uint32_t(g->drawableHeight);
        out.layer = out.level = 0;
        return true;
    }
    Framebuffer *framebuffer = Lookup(g->framebuffers, fbo);
    return framebuffer && ResolveAttachment(framebuffer->depth, out);
}

void DestroyTextureObject(Texture &texture) {
    if (texture.view) vkDestroyImageView(g->device, texture.view, nullptr);
    for (auto &entry : texture.attachmentViews) vkDestroyImageView(g->device, entry.second, nullptr);
    texture.view = VK_NULL_HANDLE;
    texture.attachmentViews.clear();
    texture.image.reset();
}

void InstallTextureEntries(std::unordered_map<std::string, void (*)(void)> &table) {
    table["glGenTextures"] = Erase(&Invalidating<&GenTextures>::Call);
    table["glCreateTextures"] = Erase(&Invalidating<&CreateTextures>::Call);
    table["glBindTexture"] = Erase(&BindTexture);
    table["glBindTextureUnit"] = Erase(&Invalidating<&BindTextureUnit>::Call);
    table["glActiveTexture"] = Erase(&ActiveTexture);
    table["glDeleteTextures"] = Erase(&Invalidating<&DeleteTextures>::Call);
    table["glIsTexture"] = Erase(&IsTexture);
    table["glTexParameteri"] = Erase(&Invalidating<&TexParameteri>::Call);
    table["glTexParameterf"] = Erase(&Invalidating<&TexParameterf>::Call);
    table["glTexParameteriv"] = Erase(&Invalidating<&TexParameteriv>::Call);
    table["glTexParameterfv"] = Erase(&Invalidating<&TexParameterfv>::Call);
    table["glTextureParameteri"] = Erase(&Invalidating<&TextureParameteri>::Call);
    table["glTexImage2D"] = Erase(&Invalidating<&TexImage2D>::Call);
    table["glTexImage3D"] = Erase(&Invalidating<&TexImage3D>::Call);
    table["glCompressedTexImage2D"] = Erase(&Invalidating<&CompressedTexImage2D>::Call);
    table["glTexStorage2D"] = Erase(&Invalidating<&TexStorage2D>::Call);
    table["glTexStorage3D"] = Erase(&Invalidating<&TexStorage3D>::Call);
    table["glTextureStorage2D"] = Erase(&Invalidating<&TextureStorage2D>::Call);
    table["glTexSubImage2D"] = Erase(&Invalidating<&TexSubImage2D>::Call);
    table["glTexSubImage3D"] = Erase(&Invalidating<&TexSubImage3D>::Call);
    table["glTextureSubImage2D"] = Erase(&Invalidating<&TextureSubImage2D>::Call);
    table["glGenerateMipmap"] = Erase(&Invalidating<&GenerateMipmap>::Call);
    table["glTextureView"] = Erase(&Invalidating<&TextureView>::Call);
    table["glTexBuffer"] = Erase(&TexBuffer);
    table["glReadPixels"] = Erase(&Invalidating<&ReadPixels>::Call);
    table["glGetTexImage"] = Erase(&Invalidating<&GetTexImage>::Call);
    table["glGetCompressedTexImage"] = Erase(&Invalidating<&GetCompressedTexImage>::Call);
    table["glGetTexLevelParameteriv"] = Erase(&GetTexLevelParameteriv);
    table["glBlitFramebuffer"] = Erase(&Invalidating<&BlitFramebuffer>::Call);
    table["glCopyTexSubImage2D"] = Erase(&Invalidating<&CopyTexSubImage2D>::Call);
    table["glGenSamplers"] = Erase(&Invalidating<&GenSamplers>::Call);
    table["glDeleteSamplers"] = Erase(&Invalidating<&DeleteSamplers>::Call);
    table["glBindSampler"] = Erase(&BindSampler);
    table["glIsSampler"] = Erase(&IsSampler);
    table["glSamplerParameteri"] = Erase(&Invalidating<&SamplerParameteri>::Call);
    table["glSamplerParameterf"] = Erase(&Invalidating<&SamplerParameterf>::Call);
    table["glSamplerParameterfv"] = Erase(&Invalidating<&SamplerParameterfv>::Call);
    table["glGenRenderbuffers"] = Erase(&Invalidating<&GenRenderbuffers>::Call);
    table["glBindRenderbuffer"] = Erase(&Invalidating<&BindRenderbuffer>::Call);
    table["glRenderbufferStorage"] = Erase(&Invalidating<&RenderbufferStorage>::Call);
    table["glRenderbufferStorageMultisample"] = Erase(&Invalidating<&RenderbufferStorageMultisample>::Call);
    table["glDeleteRenderbuffers"] = Erase(&Invalidating<&DeleteRenderbuffers>::Call);
    table["glIsRenderbuffer"] = Erase(&IsRenderbuffer);
    table["glGenFramebuffers"] = Erase(&Invalidating<&GenFramebuffers>::Call);
    table["glCreateFramebuffers"] = Erase(&Invalidating<&CreateFramebuffers>::Call);
    table["glBindFramebuffer"] = Erase(&Invalidating<&BindFramebuffer>::Call);
    table["glDeleteFramebuffers"] = Erase(&Invalidating<&DeleteFramebuffers>::Call);
    table["glIsFramebuffer"] = Erase(&IsFramebuffer);
    table["glFramebufferTexture2D"] = Erase(&Invalidating<&FramebufferTexture2D>::Call);
    table["glFramebufferTextureLayer"] = Erase(&Invalidating<&FramebufferTextureLayer>::Call);
    table["glFramebufferTexture"] = Erase(&Invalidating<&FramebufferTexture>::Call);
    table["glNamedFramebufferTexture"] = Erase(&Invalidating<&NamedFramebufferTexture>::Call);
    table["glFramebufferRenderbuffer"] = Erase(&Invalidating<&FramebufferRenderbuffer>::Call);
    table["glCheckFramebufferStatus"] = Erase(&CheckFramebufferStatus);
    table["glCheckNamedFramebufferStatus"] = Erase(&CheckNamedFramebufferStatus);
    table["glDrawBuffers"] = Erase(&Invalidating<&DrawBuffers>::Call);
    table["glDrawBuffer"] = Erase(&Invalidating<&DrawBuffer>::Call);
    table["glReadBuffer"] = Erase(&Invalidating<&ReadBuffer>::Call);
    table["glInvalidateFramebuffer"] = Erase(&Invalidating<&InvalidateFramebuffer>::Call);
}

} // namespace JudasVulkan::detail
