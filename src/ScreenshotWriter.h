#pragma once

#include <string>
#include <vector>

// Milestone 28: writes tightly packed 8-bit RGB rows (top row first, as
// Renderer::CaptureFrame produces) to a PNG. The one place the vendored
// stb_image_write implementation is compiled; the runtime harness, the
// runtime's screenshot option and the editor all share it.
bool WriteRgbPng(const std::string& path, int width, int height, const std::vector<unsigned char>& rgbPixels);
