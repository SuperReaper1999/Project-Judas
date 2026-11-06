#include "CutVelocityDual.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace judas::volume_grid {
namespace {
using Real = CutCompensated;
bool Finite(Real x) { return std::isfinite(x.hi) && std::isfinite(x.lo); }
bool Fail(std::string& error, const char* text) { error = text; return false; }
}

bool CutVelocityDual::Build(const CutSpatialField& field, std::string& error) {
    const auto n = field.parent.size(), nf = field.geometry.faces.size();
    if (n == 0 || n > 1'000'000 || field.chart.regions.size() != n ||
        field.geometry.startCapacity.size() != n || field.geometry.endCapacity != field.geometry.startCapacity ||
        (field.geometry.motion && field.geometry.motion->translation != GridVec{}))
        return Fail(error, "velocity dual requires a nonempty frozen cut chart");
    CutVelocityDual next;
    next.shares_.resize(3*n); next.faceNodes_.assign(nf, SIZE_MAX);
    next.portNodes_.assign(field.boundaries.size(), SIZE_MAX);
    std::vector<std::array<Real,6>> columnVolumes(n);
    auto append = [&](std::size_t cell, int axis, int side, std::size_t node, double volume) {
        if (volume > 0.) {
            next.shares_[3*cell+axis].push_back({node, Real(volume)});
            columnVolumes[cell][2*axis+side]=columnVolumes[cell][2*axis+side]+Real(volume);
        }
    };
    for (std::size_t f = 0; f < nf; ++f) {
        const auto& face = field.geometry.faces[f];
        if (face.left >= n || face.right >= n || face.axis < 0 || face.axis > 2 ||
            !(face.meanArea >= 0. && std::isfinite(face.meanArea)))
            return Fail(error, "invalid velocity-dual aperture");
        if (face.meanArea == 0.) continue;
        const auto node = next.nodes_.size(); next.faceNodes_[f] = node;
        next.nodes_.push_back({face.axis,1,f,SIZE_MAX,face.meanArea,false});
        for (bool left : {true,false}) {
            const auto cell = left ? face.left : face.right;
            const auto& region = field.chart.regions[cell];
            CutRegionWaterPlane full{{1,false},region.hi[1]};
            const auto volume = MeasureCutRegionFaceInertiaVolumes(face,field.geometry.motion,region,left,full);
            append(cell,face.axis,left?1:0,node,volume[0]+volume[1]);
        }
    }
    for (std::size_t j = 0; j < field.boundaries.size(); ++j) {
        const auto& b = field.boundaries[j];
        if (b.fragment >= n || b.axis < 0 || b.axis > 2 || (b.sign != 1 && b.sign != -1) || !b.gasOnly)
            return Fail(error, "velocity dual supports gas-only chart ports");
        const auto& region = field.chart.regions[b.fragment];
        const double edge = b.sign > 0 ? region.hi[b.axis] : region.lo[b.axis];
        const double exterior = b.sign > 0 ? field.chart.layout.cells[b.axis]*field.chart.layout.h : 0.;
        if (edge != exterior) return Fail(error, "velocity-dual port is not exterior");
        CutRegionFace face{}; face.axis=b.axis; face.lo=region.lo; face.hi=region.hi;
        face.lo[b.axis]=face.hi[b.axis]=edge;
        const double area=MeasureCutRegionAperture(face,field.geometry.motion,face.lo,face.hi).mean;
        if (!(area > 0.)) return Fail(error, "velocity-dual port has no open aperture");
        const auto node=next.nodes_.size(); next.portNodes_[j]=node;
        next.nodes_.push_back({b.axis,b.sign,SIZE_MAX,j,area,false});
        const auto volume=MeasureCutRegionFaceInertiaVolumes(face,field.geometry.motion,region,b.sign>0,{{1,false},region.hi[1]});
        append(b.fragment,b.axis,b.sign>0?1:0,node,volume[0]+volume[1]);
    }
    for (std::size_t cell=0;cell<n;++cell) for (int axis=0;axis<3;++axis) {
        const Real capacity=field.geometry.startCapacity[cell];
        if (!(capacity.Value()>0. && Finite(capacity))) return Fail(error,"invalid velocity-dual capacity");
        for (int side=0;side<2;++side) {
            auto region=field.chart.regions[cell];auto solid=field.geometry.motion;
            const double anchor=region.lo[axis],width=region.hi[axis]-anchor;
            region.lo[axis]=0.;region.hi[axis]=width;
            if (solid) {solid->lo[axis]-=anchor;solid->hi[axis]-=anchor;}
            auto lo=region.lo,hi=region.hi;(side?lo:hi)[axis]=.5*width;
            const double half=CutRegionClippedVolume(region,solid,lo,hi);
            if ((columnVolumes[cell][2*axis+side]-Real(half)).Value()>
                512.*std::numeric_limits<double>::epsilon()*half)
                return Fail(error,"velocity aperture columns overlap within one half owner");
        }
        auto& shares=next.shares_[3*cell+axis]; Real sum;
        for (const auto& entry:shares) sum=sum+entry.fraction;
        const double excess=(sum-capacity).Value();
        if (excess>512.*std::numeric_limits<double>::epsilon()*capacity.Value())
            return Fail(error,"velocity aperture columns overlap their material owner");
        // A geometric partition can differ by roundoff; normalize only after
        // independently bounding that defect. Physical mass is never altered.
        const Real divisor=excess>0. ? sum : capacity;
        next.partitionRoundoff_=std::max(next.partitionRoundoff_,std::max(0.,excess));
        for (auto& entry:shares) entry.fraction=entry.fraction/divisor;
        if (excess<0.) {
            const auto node=next.nodes_.size(); next.nodes_.push_back({axis,1,SIZE_MAX,SIZE_MAX,0.,true});
            shares.push_back({node,(capacity-sum)/capacity});
        }
    }
    std::vector<Real> dual(next.nodes_.size());
    for (std::size_t cell=0;cell<n;++cell) for (int axis=0;axis<3;++axis)
        for (const auto& entry:next.shares_[3*cell+axis])
            dual[entry.node]=dual[entry.node]+entry.fraction*Real(field.geometry.startCapacity[cell]);
    for (auto v:dual) {
        if (!(v.Value()>0. && Finite(v))) return Fail(error,"velocity dual has no geometric owner");
        next.dualCapacity_.push_back(v.Value());
    }
    next.faceScatter_.resize(nf);
    std::size_t entries=0;
    for (std::size_t f=0;f<nf;++f) {
        const auto& face=field.geometry.faces[f]; if (face.meanArea==0.) continue;
        for (int axis=0;axis<3;++axis)
            for (const auto& a:next.shares_[3*face.left+axis])
                for (const auto& b:next.shares_[3*face.right+axis]) {
                    if (a.node==b.node) continue;
                    if (++entries>8'000'000) return Fail(error,"velocity-dual carrier budget exceeded");
                    next.faceScatter_[f].push_back({a.node,b.node,a.fraction*b.fraction});
                }
    }
    next.portScatter_.resize(field.boundaries.size());
    for (std::size_t j=0;j<field.boundaries.size();++j) for (int axis=0;axis<3;++axis)
        for (const auto& a:next.shares_[3*field.boundaries[j].fragment+axis])
            next.portScatter_[j].push_back({a.node,SIZE_MAX,a.fraction});
    next.sourceField_=&field;*this=std::move(next); error.clear(); return true;
}

bool CutVelocityDual::Mass(const std::vector<Real>& cellMass, std::vector<Real>& output, std::string& error) const {
    for(auto m:cellMass)if(!(m.Value()>0.&&Finite(m)))return Fail(error,"dual mass requires positive material");
    return MassDerivative(cellMass,output,error);
}
bool CutVelocityDual::MassDerivative(const std::vector<Real>& cellMass, std::vector<Real>& output, std::string& error) const {
    if (cellMass.size()*3!=shares_.size() || nodes_.empty()) return Fail(error,"dual mass source dimensions mismatch");
    std::vector<Real> next(nodes_.size());
    for (std::size_t i=0;i<cellMass.size();++i) {
        if (!Finite(cellMass[i])) return Fail(error,"dual mass derivative must be finite");
        for (int a=0;a<3;++a) for (const auto& share:shares_[3*i+a])
            next[share.node]=next[share.node]+share.fraction*cellMass[i];
    }
    output=std::move(next); error.clear(); return true;
}

bool CutVelocityDual::Carriers(const std::vector<Real>& primal, const std::vector<Real>& ports,
    std::vector<CutTransportFace>& output, std::vector<CutTransportGasPort>& boundary, std::string& error) const {
    if (primal.size()!=faceScatter_.size() || ports.size()!=portScatter_.size())
        return Fail(error,"dual carrier dimensions mismatch");
    std::vector<CutTransportFace> next; std::vector<CutTransportGasPort> outside;
    for (std::size_t f=0;f<primal.size();++f) {
        if (!Finite(primal[f])) return Fail(error,"nonfinite primal mass carrier");
        if (primal[f].Value()==0.) continue;
        for (const auto& entry:faceScatter_[f]) next.push_back({entry.from,entry.to,primal[f]*entry.fraction});
    }
    for (std::size_t j=0;j<ports.size();++j) {
        if (!Finite(ports[j])) return Fail(error,"nonfinite port mass carrier");
        if (ports[j].Value()==0.) continue;
        for (const auto& entry:portScatter_[j]) outside.push_back({entry.from,ports[j]*entry.fraction});
    }
    output=std::move(next); boundary=std::move(outside); error.clear(); return true;
}
} // namespace judas::volume_grid
