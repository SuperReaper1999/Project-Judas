#include "ScreenshotWriter.h"

// stb_image_write triggers -Wmissing-field-initializers under this
// project's own warning flags in a few of its internal functions we don't
// even call (BMP/TGA/HDR/JPG writers) — a lint characteristic of a
// third-party header we don't control, not of this project's code, so it's
// suppressed only for this one include rather than loosening -Wall/-Wextra
// project-wide.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

bool WriteRgbPng(const std::string& path, int width, int height, const std::vector<unsigned char>& rgbPixels) {
    if (width <= 0 || height <= 0 || rgbPixels.size() < static_cast<std::size_t>(width) * height * 3) return false;
    return stbi_write_png(path.c_str(), width, height, 3, rgbPixels.data(), width * 3) != 0;
}
