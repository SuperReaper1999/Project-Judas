#pragma once

#include <string>

#include "TextureData.h"

// Milestone 13: CPU-side TrueType font loading, the text-rendering sibling of
// ModelLoader/TextureLoader (src/ModelLoader.h, src/TextureLoader.h) — same
// split: this file produces plain CPU data, Renderer.cpp is the only place
// that uploads it to the GPU and draws with it (see docs/ARCHITECTURE.md,
// "Milestone 13, Text rendering").
//
// Uses stb_truetype.h (public domain, vendored at third_party/stb_truetype.h
// — the same vendoring reasoning already applied to stb_image/
// stb_image_write/tiny_obj_loader: font rasterization is a solved commodity
// problem, not something Judas needs to own). The demo font itself is
// DejaVu Sans (assets/fonts/DejaVuSans.ttf, Bitstream Vera License — a
// permissive, redistribution-friendly license; see
// assets/fonts/DejaVuSans-LICENSE.txt for the full text), chosen because it
// was already installed on the development machine under that license and
// needed no separate download/attribution search.
//
// One glyph, baked once at a fixed pixel size into a single-channel-turned-
// RGBA bitmap atlas (see FontAtlasData::atlasTexture) covering the ordinary
// printable ASCII range (32..126) — sufficient for HUD telemetry and menu
// labels, this milestone's entire text need. No Unicode, no fallback glyph,
// no multi-font/style support: not required by M13, not built speculatively.

// One baked glyph's placement, in PIXELS at the atlas's own baked size, plus
// its atlas UV rect — mirrors stb_truetype's own stbtt_bakedchar fields
// (copied out into a plain struct here so nothing outside FontLoader.cpp
// needs to know stb_truetype's types exist, matching the
// MeshData/TextureData "plain CPU data only" convention).
struct FontGlyph {
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;  // atlas UV rect
    float width = 0.0f, height = 0.0f;                  // pixels, at bake size
    float offsetX = 0.0f, offsetY = 0.0f;  // pixels from the pen position (baseline) to the glyph's own top-left
    float advanceX = 0.0f;                 // pixels to move the pen after drawing this glyph
};

constexpr int kFontFirstChar = 32;   // ' '
constexpr int kFontGlyphCount = 95;  // through '~' (126), inclusive

struct FontAtlasData {
    TextureData atlasTexture;  // RGBA (see LoadFontAtlas: r=g=b=255, a=coverage)
    FontGlyph glyphs[kFontGlyphCount];
    float pixelHeight = 0.0f;  // the size this atlas was baked at; text must be scaled relative to it
    float ascent = 0.0f;       // pixels from the baseline up to the font's own top, at pixelHeight
    float lineHeight = 0.0f;   // recommended baseline-to-baseline distance, at pixelHeight
};

// Reads `path` (a .ttf file) and bakes glyphs 32..126 at `pixelHeight` into
// a square RGBA atlas. Returns false (with `outError` set) if the file
// can't be read/parsed, or if all 95 glyphs don't fit the atlas at the
// requested size — no CPU-only test exercises the second case since the
// fixed atlas size chosen in the .cpp comfortably fits DejaVu Sans at every
// pixelHeight this engine actually requests, but the check exists so a
// future change can't silently truncate glyphs.
bool LoadFontAtlas(const char* path, float pixelHeight, FontAtlasData& outAtlas,
                    std::string& outError);
