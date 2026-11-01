#pragma once

#include <algorithm>
#include <utility>
#include <iterator>
#include <vector>

namespace FluidHydrostaticDetail {

// Reconstruct each resolved vertical layer, without filling an arbitrarily
// tall void between its neighbours. Gaps no larger than the column resolution
// remain unresolved: literal particle cubes do not tile a disordered PBF pool.
// Closing a small internal gap never extends either external free surface.
inline void CloseColumnLayers(std::vector<std::pair<double, double>>& intervals,
                              double resolution, double rounding) {
    if(intervals.size()<2)return;
    std::sort(intervals.begin(),intervals.end());
    std::size_t count=1;
    for(std::size_t i=1;i<intervals.size();++i) {
        if(intervals[i].first-intervals[count-1].second<=resolution+rounding)
            intervals[count-1].second=std::max(intervals[count-1].second,intervals[i].second);
        else intervals[count++]=intervals[i];
    }
    intervals.resize(count);
}
inline double ColumnOccupiedHeight(std::vector<std::pair<double, double>>& intervals,
                                   double halfHeight, double resolution, double rounding) {
    if (intervals.empty()) return 0;
    const auto contribute = [&](const auto& span) {
        if (halfHeight == 0) return span.first <= 0 && span.second >= 0 ? 1.0 : 0.0;
        return std::max(0.0, std::min(span.second, halfHeight) - std::max(span.first, -halfHeight));
    };
    // A one-interval union needs no sort or second element. Besides making the
    // common case explicit, the union walk only needs subsequent intervals.
    if (intervals.size() == 1) return contribute(intervals.front());
    std::sort(intervals.begin(), intervals.end());
    double occupied = 0;
    auto layer = intervals.front();
    for (auto next = std::next(intervals.begin()); next != intervals.end(); ++next) {
        if (next->first - layer.second <= resolution + rounding) {
            layer.second = std::max(layer.second, next->second);
        } else {
            occupied += contribute(layer);
            layer = *next;
        }
    }
    return occupied + contribute(layer);
}
} // namespace FluidHydrostaticDetail
