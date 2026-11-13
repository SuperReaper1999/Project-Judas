#pragma once

#include <string>
struct SDL_Window;
struct TextureData;

// Empty path selects the embedded Judas artwork. Invalid custom images leave
// the window's existing fallback intact. Image pixels are never GL resources.
bool ApplyAppIcon(SDL_Window* window, const std::string& path, std::string& error);
// Decode the same embedded artwork for engine-owned UI. GL bottom-up RGBA;
// the caller owns pixels and any GPU texture it creates. No file dependency.
bool LoadDefaultAppIcon(TextureData&, std::string& error);
bool ValidateAppIcon(const std::string& path, std::string& error);
bool WriteDefaultAppIcon(const std::string& path, std::string& error);
