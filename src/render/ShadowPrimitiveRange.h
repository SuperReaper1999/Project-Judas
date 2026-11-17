#pragma once
#include <cstddef>
#include <limits>
#include <optional>

namespace JudasRender {
struct ShadowPrimitiveState {
    unsigned first=0,count=0;
    bool opaque=false,cull=false,reversed=false;
};
struct ShadowPrimitiveRange { unsigned first=0,count=0; };
// Only contiguous complete triangles with identical depth raster state combine.
// Material factors/UVs do not affect the production opaque shadow shader.
template<class StateAt>
std::optional<ShadowPrimitiveRange> CoalesceOpaqueShadowRange(std::size_t parts,StateAt stateAt) {
    if(parts<2)return {};
    const auto first=stateAt(0);
    ShadowPrimitiveRange range{first.first,0};
    for(std::size_t i=0;i<parts;++i) {
        const auto part=i?stateAt(i):first;
        if(!part.opaque||!part.count||part.count%3||part.cull!=first.cull||
           part.reversed!=first.reversed||part.first!=range.first+range.count||
           part.count>std::numeric_limits<unsigned>::max()-range.first-range.count||
           part.count>unsigned(std::numeric_limits<int>::max())-range.count)return {};
        range.count+=part.count;
    }
    return range;
}
}
