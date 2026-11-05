#include "CutConnectedOwners.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <map>
#include <tuple>
#include <utility>

namespace judas::volume_grid {
namespace {
bool Fail(std::string& error, const char* why) { error = why; return false; }
double LargestFace(const CutRegion& r) {
    double largest = 0.;
    for (int axis = 0; axis < 3; ++axis)
        largest = std::max(largest, (r.hi[(axis+1)%3]-r.lo[(axis+1)%3]) *
                                   (r.hi[(axis+2)%3]-r.lo[(axis+2)%3]));
    return largest;
}
struct Sum {
    double value = 0., tail = 0.;
    void Add(double x) {
        const double y = value + x;
        tail += std::abs(value) >= std::abs(x) ? (value-y)+x : (x-y)+value;
        value = y;
    }
    double Get() const { return value + tail; }
};
} // namespace

bool BuildCutConnectedOwners(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                             double minimumFraction, CutConnectedOwners& output, std::string& error) {
    const auto n = chart.regions.size();
    if (n == 0 || geometry.startCapacity.size() != n || geometry.endCapacity.size() != n ||
        chart.layout.CheckedCellCount() == 0 ||
        !(minimumFraction > 0. && minimumFraction <= 1.) ||
        (geometry.motion && geometry.motion->translation != GridVec{}))
        return Fail(error, "connected owners require finite static cut geometry");
    std::vector<std::size_t> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    auto volume = geometry.startCapacity;
    std::vector<GridVec> lo(n), hi(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (!(volume[i] >= 0. && std::isfinite(volume[i])) || volume[i] != geometry.endCapacity[i])
            return Fail(error, "connected owner capacity is invalid or moving");
        lo[i] = chart.regions[i].lo;
        hi[i] = chart.regions[i].hi;
    }
    const auto root = [&](std::size_t i) {
        while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; }
        return i;
    };
    for (const auto& f : geometry.faces)
        if (f.left >= n || f.right >= n || f.axis < 0 || f.axis > 2)
            return Fail(error, "invalid connected-owner face");
    const double h = chart.layout.h, target = minimumFraction*h*h*h;
    bool changed = true;
    while (changed) {
        changed = false;
        // Test physical seams, not individual quadrature rectangles. A
        // wide open seam can be subdivided into many tiny integration faces.
        using Key = std::tuple<std::size_t,std::size_t,int,double>;
        std::map<Key,Sum> seams;
        for (const auto& f : geometry.faces) {
            const auto i=root(f.left),j=root(f.right);
            if (i==j || volume[i]==0. || volume[j]==0.) continue;
            const int a=(f.axis+1)%3,b=(f.axis+2)%3;
            const double full=(f.hi[a]-f.lo[a])*(f.hi[b]-f.lo[b]);
            if (full>0. && f.startArea==full && f.endArea==full && f.meanArea==full)
                seams[{i,j,f.axis,f.lo[f.axis]}].Add(full);
        }
        for (const auto& [key,measure] : seams) {
            const auto [left,right,axis,coordinate]=key;
            (void)axis;(void)coordinate;
            const auto i=root(left),j=root(right);
            if (i!=left || j!=right || i==j || (volume[i]>=target && volume[j]>=target)) continue;
            GridVec nextLo,nextHi;
            bool local=true;
            for (int d=0;d<3;++d) {
                nextLo[d]=std::min(lo[i][d],lo[j][d]);
                nextHi[d]=std::max(hi[i][d],hi[j][d]);
                local=local && nextHi[d]-nextLo[d]<=2.*h;
            }
            const CutRegion a{lo[i],hi[i],0},b{lo[j],hi[j],0};
            if (!local || measure.Get()<.5*std::min(LargestFace(a),LargestFace(b))) continue;
            parent[i]=j;volume[j]+=volume[i];lo[j]=nextLo;hi[j]=nextHi;
            changed=true;
        }
    }
    CutConnectedOwners next;
    next.owner.resize(n);
    std::vector<std::size_t> compact(n,n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto r = root(i);
        if (compact[r] == n) {
            compact[r] = next.members.size();
            next.members.emplace_back();
            next.capacity.push_back(0.);
            next.lo.push_back(lo[r]); next.hi.push_back(hi[r]);
        }
        const auto o = next.owner[i] = compact[r];
        next.members[o].push_back(i);
        next.capacity[o] += geometry.startCapacity[i];
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool MeasureCutOwnedDeparture(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                              const CutConnectedOwners& owners,
                              const std::vector<CutRegionWaterPlane>& planes,
                              std::size_t faceIndex, double signedVolume,
                              CutOwnedDeparture& output, std::string& error) {
    if (faceIndex >= geometry.faces.size() || planes.size() != chart.regions.size() ||
        owners.owner.size() != chart.regions.size() || !std::isfinite(signedVolume))
        return Fail(error, "invalid owned departure input");
    CutOwnedDeparture next;
    if (signedVolume == 0.) { output = next; error.clear(); return true; }
    const auto& face = geometry.faces[faceIndex];
    if (face.axis<0 || face.axis>2 || face.left>=chart.regions.size() || face.right>=chart.regions.size())
        return Fail(error,"invalid owned departure face");
    if (!(face.meanArea > 0.)) return Fail(error, "owned departure has no open face");
    const bool left = signedVolume > 0.;
    const auto donor = left ? face.left : face.right;
    const auto o = owners.owner[donor];
    if (o >= owners.members.size()) return Fail(error, "owned departure has invalid material owner");
    const int normal = face.axis, a = (normal+1)%3, b = (normal+2)%3;
    const auto origin = face.lo;
    const double depth = std::abs(signedVolume)/face.meanArea;
    std::array<std::vector<double>,2> cuts{{{0.,face.hi[a]-origin[a]}, {0.,face.hi[b]-origin[b]}}};
    const auto add = [&](int tangential, double x) {
        const int axis = tangential == 0 ? a : b;
        x -= origin[axis];
        if (x > 0. && x < face.hi[axis]-origin[axis]) cuts[tangential].push_back(x);
    };
    for (const auto m : owners.members[o]) {
        if (m>=chart.regions.size() || owners.owner[m]!=o)
            return Fail(error,"invalid owned departure member");
        for (int t = 0; t < 2; ++t) {
            const int axis = t == 0 ? a : b;
            add(t,chart.regions[m].lo[axis]); add(t,chart.regions[m].hi[axis]);
        }
    }
    if (geometry.motion)
        for (int t = 0; t < 2; ++t) {
            const int axis = t == 0 ? a : b;
            add(t,geometry.motion->lo[axis]); add(t,geometry.motion->hi[axis]);
        }
    for (auto& c : cuts) { std::sort(c.begin(),c.end()); c.erase(std::unique(c.begin(),c.end()),c.end()); }
    if (cuts[0].size()*cuts[1].size() > 16384)
        return Fail(error, "owned departure exceeds explicit footprint partition budget");
    struct Segment { double start, stop; std::size_t member; };
    Sum wet, dry, unsupported;
    for (std::size_t ia=1; ia<cuts[0].size(); ++ia)
        for (std::size_t ib=1; ib<cuts[1].size(); ++ib) {
            const double area=(cuts[0][ia]-cuts[0][ia-1])*(cuts[1][ib]-cuts[1][ib-1]);
            // The flux aperture excludes a solid covering the face itself.
            // Such a sub-column carries no share of q and is not a donor.
            if (geometry.motion &&
                cuts[0][ia-1] >= geometry.motion->lo[a]-origin[a] && cuts[0][ia] <= geometry.motion->hi[a]-origin[a] &&
                cuts[1][ib-1] >= geometry.motion->lo[b]-origin[b] && cuts[1][ib] <= geometry.motion->hi[b]-origin[b] &&
                geometry.motion->lo[normal] <= origin[normal] && geometry.motion->hi[normal] >= origin[normal])
                continue;
            std::vector<Segment> segments;
            for (const auto m : owners.members[o]) {
                const auto& r=chart.regions[m];
                if (!(cuts[0][ia-1] >= r.lo[a]-origin[a] && cuts[0][ia] <= r.hi[a]-origin[a] &&
                      cuts[1][ib-1] >= r.lo[b]-origin[b] && cuts[1][ib] <= r.hi[b]-origin[b])) continue;
                double start=left ? origin[normal]-r.hi[normal] : r.lo[normal]-origin[normal];
                double stop=left ? origin[normal]-r.lo[normal] : r.hi[normal]-origin[normal];
                start=std::max(0.,start); stop=std::min(depth,stop);
                if (stop > start) segments.push_back({start,stop,m});
            }
            std::sort(segments.begin(),segments.end(),[](auto x,auto y){return x.start<y.start;});
            double reached=0.;
            for (const auto& segment : segments) {
                if (segment.start > reached) break; // real gap; never resume behind it
                if (segment.start < reached) return Fail(error,"owned fragment interiors overlap");
                auto region=chart.regions[segment.member];
                auto motion=geometry.motion;
                auto plane=planes[segment.member];
                for (int axis=0;axis<3;++axis) {
                    region.lo[axis]-=origin[axis]; region.hi[axis]-=origin[axis];
                    if (motion) { motion->lo[axis]-=origin[axis]; motion->hi[axis]-=origin[axis]; }
                }
                plane.coordinate-=origin[plane.orientation.axis];
                GridVec lo{},hi{};
                lo[a]=cuts[0][ia-1]; hi[a]=cuts[0][ia];
                lo[b]=cuts[1][ib-1]; hi[b]=cuts[1][ib];
                double stop=segment.stop;
                bool hitsSolid=false;
                if (motion && cuts[0][ia-1] >= motion->lo[a] && cuts[0][ia] <= motion->hi[a] &&
                    cuts[1][ib-1] >= motion->lo[b] && cuts[1][ib] <= motion->hi[b]) {
                    const double hit=left ? -motion->hi[normal] : motion->lo[normal];
                    const double far=left ? -motion->lo[normal] : motion->hi[normal];
                    if (far > reached && hit < stop) {
                        stop=std::max(reached,hit);
                        hitsSolid=true;
                    }
                }
                lo[normal]=left ? -stop : reached;
                hi[normal]=left ? -reached : stop;
                const auto phases=MeasureCutRegionPhaseVolumes(region,motion,lo,hi,plane);
                wet.Add(phases[0]); dry.Add(phases[1]);
                ++next.visitedFragments;
                reached=stop;
                if (hitsSolid || reached == depth) break;
            }
            unsupported.Add(area*(depth-reached));
        }
    next.water=wet.Get(); next.gas=dry.Get(); next.unsupportedVolume=unsupported.Get();
    if (!(next.water>=0. && next.gas>=0. && next.unsupportedVolume>=0. &&
          std::isfinite(next.water+next.gas+next.unsupportedVolume)))
        return Fail(error,"owned departure measures are not finite");
    output=next; error.clear(); return true;
}
} // namespace judas::volume_grid
