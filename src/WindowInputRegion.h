#pragma once
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

// Optional embedding boundary. Native OS dimensions remain unchanged; game
// input and presentation use the content rectangle and its render resolution.
struct WindowInputRegion {
    Uint32 windowId = 0;
    SDL_Rect content{0, 0, 1, 1};
    int renderWidth = 1, renderHeight = 1;
    bool Valid() const {
        return content.w > 0 && content.h > 0 && renderWidth > 0 && renderHeight > 0 &&
               renderWidth <= 32768 && renderHeight <= 32768;
    }
    SDL_FPoint Normalize(int x, int y) const {
        return {float(x - content.x) / content.w, float(y - content.y) / content.h};
    }
    void Map(int& x, int& y) const {
        const auto point = Normalize(x, y);
        x = int(std::lround(point.x * renderWidth));
        y = int(std::lround(point.y * renderHeight));
    }
    SDL_Rect ToNative(SDL_Rect rect) const {
        return {content.x + int(std::lround(double(rect.x) * content.w / renderWidth)),
                content.y + int(std::lround(double(rect.y) * content.h / renderHeight)),
                std::max(1,int(std::lround(double(rect.w) * content.w / renderWidth))),
                std::max(1,int(std::lround(double(rect.h) * content.h / renderHeight)))};
    }
    bool Contains(int x, int y) const {
        const auto p = Normalize(x, y);
        return p.x >= 0 && p.y >= 0 && p.x < 1 && p.y < 1;
    }
};
