#pragma once

#include <string>

#include "TextureData.h"

// Loads a single 2D image from disk into a Judas-owned TextureData — see
// docs/ARCHITECTURE.md, "Milestone 9," for why stb_image was chosen. Pure
// CPU-side, no OpenGL context needed (see tests/AssetTests.cpp).
//
// Always decodes to 4 channels (RGBA) regardless of the source file's own
// channel count — see TextureData.h. Loaded with vertical flip-on-load
// enabled, so pixel row 0 in the returned data is the BOTTOM of the image
// as authored, matching OpenGL's own texture-coordinate convention
// (v=0 at the bottom) — see docs/ARCHITECTURE.md's UV-convention note for
// the full reasoning and how it pairs with beacon.obj's own UV authoring.
//
// Returns false and fills `outError` with a human-readable message
// (including the offending path) on any failure — missing file or
// undecodable image — never returns a partially-filled or garbage texture.
bool LoadTextureFromFile(const std::string& path, TextureData& outTexture, std::string& outError);
