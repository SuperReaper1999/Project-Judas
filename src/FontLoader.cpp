#include "FontLoader.h"

#include <cstdio>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

namespace {
// Comfortably fits DejaVu Sans's 95 printable-ASCII glyphs at any pixel
// height this engine actually bakes at (HUD/menu text tops out well under
// 64px) — verified directly via LoadFontAtlas's own return-value check
// below, not just assumed.
constexpr int kAtlasWidth = 512;
constexpr int kAtlasHeight = 512;
}  // namespace

bool LoadFontAtlas(const char* path, float pixelHeight, FontAtlasData& outAtlas,
                    std::string& outError) {
    std::FILE* file = std::fopen(path, "rb");
    if (!file) {
        outError = std::string("Failed to open font file: ") + path;
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (fileSize <= 0) {
        std::fclose(file);
        outError = std::string("Font file is empty or unreadable: ") + path;
        return false;
    }
    std::vector<unsigned char> fontBuffer(static_cast<size_t>(fileSize));
    const size_t readBytes = std::fread(fontBuffer.data(), 1, fontBuffer.size(), file);
    std::fclose(file);
    if (readBytes != fontBuffer.size()) {
        outError = std::string("Failed to read font file: ") + path;
        return false;
    }

    // Single-channel coverage bitmap — stb_truetype's own bake output —
    // converted to RGBA (r=g=b=255, a=coverage) below so it can go through
    // exactly the same TextureData/Renderer::CreateTexture path every other
    // texture in this engine already uses, rather than teaching Renderer a
    // second, single-channel upload format for this one caller.
    std::vector<unsigned char> coverage(static_cast<size_t>(kAtlasWidth) * kAtlasHeight);
    std::vector<stbtt_bakedchar> bakedChars(kFontGlyphCount);

    const int result = stbtt_BakeFontBitmap(fontBuffer.data(), 0, pixelHeight, coverage.data(),
                                             kAtlasWidth, kAtlasHeight, kFontFirstChar,
                                             kFontGlyphCount, bakedChars.data());
    if (result <= 0) {
        outError = std::string("Font atlas too small to fit all glyphs for: ") + path;
        return false;
    }

    outAtlas.atlasTexture.width = kAtlasWidth;
    outAtlas.atlasTexture.height = kAtlasHeight;
    outAtlas.atlasTexture.pixels.resize(static_cast<size_t>(kAtlasWidth) * kAtlasHeight * 4);
    for (size_t i = 0; i < coverage.size(); ++i) {
        outAtlas.atlasTexture.pixels[i * 4 + 0] = 255;
        outAtlas.atlasTexture.pixels[i * 4 + 1] = 255;
        outAtlas.atlasTexture.pixels[i * 4 + 2] = 255;
        outAtlas.atlasTexture.pixels[i * 4 + 3] = coverage[i];
    }

    for (int i = 0; i < kFontGlyphCount; ++i) {
        const stbtt_bakedchar& baked = bakedChars[static_cast<size_t>(i)];
        FontGlyph& glyph = outAtlas.glyphs[i];
        glyph.u0 = static_cast<float>(baked.x0) / static_cast<float>(kAtlasWidth);
        glyph.v0 = static_cast<float>(baked.y0) / static_cast<float>(kAtlasHeight);
        glyph.u1 = static_cast<float>(baked.x1) / static_cast<float>(kAtlasWidth);
        glyph.v1 = static_cast<float>(baked.y1) / static_cast<float>(kAtlasHeight);
        glyph.width = static_cast<float>(baked.x1 - baked.x0);
        glyph.height = static_cast<float>(baked.y1 - baked.y0);
        glyph.offsetX = baked.xoff;
        glyph.offsetY = baked.yoff;
        glyph.advanceX = baked.xadvance;
    }

    // stb_truetype's own font-metrics query, at the same pixelHeight the
    // glyphs were baked at, so the ascent/lineHeight this atlas reports
    // stay consistent with the glyph quads themselves.
    stbtt_fontinfo fontInfo;
    stbtt_InitFont(&fontInfo, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0));
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);
    outAtlas.pixelHeight = pixelHeight;
    outAtlas.ascent = static_cast<float>(ascent) * scale;
    outAtlas.lineHeight = static_cast<float>(ascent - descent + lineGap) * scale;

    return true;
}
