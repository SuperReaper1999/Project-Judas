#pragma once
#include <algorithm>
#include <cmath>

struct BrowserSurfaceSize { int width=0, height=0; };
// Optional browser presentation budget. Logical simulation and fixed cadence are
// independent of pixel density. Zero means no visible drawing area.
inline BrowserSurfaceSize ChooseBrowserSurface(double width,double height,double ratio) {
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return {};
    if(!std::isfinite(ratio)||ratio<=0)ratio=1;
    const double scale=std::min({ratio,2.0,4096.0/width,4096.0/height,
                                std::sqrt((2.0*1024*1024)/width/height)});
    return {std::clamp(int(std::floor(width*scale)),1,4096),
            std::clamp(int(std::floor(height*scale)),1,4096)};
}
