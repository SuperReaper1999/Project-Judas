#include "CutOwnedProjection.h"
#include "CutHydroacousticGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>

namespace judas::volume_grid {
namespace {
bool Fail(std::string& error, const std::string& why) { error=why; return false; }
struct Sum {
    double v=0.,c=0.;
    void Add(double x) { const double n=v+x; c+=std::abs(v)>=std::abs(x)?(v-n)+x:(x-n)+v; v=n; }
    double Get() const { return v+c; }
};
struct Port { std::size_t face,left,right; double g,predictor; };
struct Vent { std::size_t owner,fragment; double g,predictor; };
}

bool ProjectCutOwnedMaterial(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                              const CutConnectedOwners& owners, const CutDynamicState& source,
                              const CutDynamicSettings& settings, const std::vector<CutRegionGasVent>& vents,
                              CutOwnedProjection& output, std::string& error,
                              CutHydroacousticGraphFailure* pressureFailure,
                              CutOwnedProjection* diagnosticTrial) {
    if (diagnosticTrial == &output) return Fail(error,"diagnostic pressure trial must not alias physical output");
    const auto fragments=chart.regions.size(), count=owners.members.size();
    if (count==0 || owners.owner.size()!=fragments || owners.capacity.size()!=count ||
        owners.lo.size()!=count || owners.hi.size()!=count ||
        geometry.startCapacity.size()!=fragments || geometry.endCapacity.size()!=fragments ||
        !source.ventSpeed.empty() ||
        source.water.size()!=fragments || source.gasMass.size()!=fragments ||
        (!source.pressure.empty() && source.pressure.size()!=fragments) ||
        (!source.faceSpeed.empty() && source.faceSpeed.size()!=geometry.faces.size()) ||
        !(settings.dt>0. && std::isfinite(settings.dt) && settings.soundSpeed>0. &&
          std::isfinite(settings.soundSpeed*settings.soundSpeed) && settings.gasDensity>0. &&
          std::isfinite(settings.gasDensity) && settings.waterDensity>0. && std::isfinite(settings.waterDensity) &&
          settings.volumeTolerance>0. && std::isfinite(settings.volumeTolerance) &&
          settings.relativeVolumeTolerance>0. && settings.relativeVolumeTolerance<1. &&
          std::isfinite(settings.relativeVolumeTolerance)) || settings.iterationLimit<1 ||
        (geometry.motion && geometry.motion->translation!=GridVec{}))
        return Fail(error,"invalid static connected-owner projection input");
    for (double g:settings.gravity)
        if (!std::isfinite(g)) return Fail(error,"owned gravity is nonfinite");
    if (!ValidateCutRegionGasVents(chart,vents,error)) return false;
    const double c2=settings.soundSpeed*settings.soundSpeed;
    std::vector<double> water(count),mass(count),gas(count),pressure(count),fragmentGas(fragments);
    std::vector<bool> seen(fragments);
    for (std::size_t o=0;o<count;++o) {
        if (owners.members[o].empty()) return Fail(error,"material owner has no geometry");
        if (!(owners.capacity[o]>=0. && std::isfinite(owners.capacity[o])) ||
            owners.members[o][0]>=fragments)
            return Fail(error,"invalid connected material owner");
        Sum w,m,g;
        pressure[o]=source.pressure.empty()?0.:source.pressure[owners.members[o][0]];
        const double density=settings.gasDensity+pressure[o]/c2;
        if (!(density>0. && std::isfinite(density))) return Fail(error,"owner EOS is outside its domain");
        for (auto f:owners.members[o]) {
            if (f>=fragments || seen[f] || owners.owner[f]!=o ||
                (!source.pressure.empty() && source.pressure[f]!=pressure[o]) ||
                !(source.water[f]>=0. && std::isfinite(source.water[f]) &&
                  source.gasMass[f]>=0. && std::isfinite(source.gasMass[f])))
                return Fail(error,"owner import requires a consistent physical and pressure state");
            seen[f]=true;
            fragmentGas[f]=source.gasMass[f]/density;
            w.Add(source.water[f]); m.Add(source.gasMass[f]); g.Add(fragmentGas[f]);
        }
        water[o]=w.Get();mass[o]=m.Get();gas[o]=g.Get();
        const double tol=256.*std::numeric_limits<double>::epsilon()*owners.capacity[o];
        if (std::abs((owners.capacity[o]-water[o])-gas[o])>tol)
            return Fail(error,"owner import changes occupied capacity");
    }
    if (std::find(seen.begin(),seen.end(),false)!=seen.end())
        return Fail(error,"connected material ownership omits a geometric fragment");
    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionPhasePlanes(chart,geometry,1,source.water,fragmentGas,
                                         source.interfaces,planes,error)) return false;
    std::vector<Port> ports;
    std::vector<CutHydroacousticGraphLink> links;
    for (std::size_t f=0;f<geometry.faces.size();++f) {
        const auto& face=geometry.faces[f];
        if (face.left>=fragments || face.right>=fragments || face.axis<0 || face.axis>2 ||
            !(face.meanArea>=0. && std::isfinite(face.meanArea)))
            return Fail(error,"invalid connected boundary face");
        const auto a=owners.owner[face.left],b=owners.owner[face.right];
        if (a==b) {
            if (!source.faceSpeed.empty() && source.faceSpeed[f]!=0.)
                return Fail(error,"retained internal momentum requires a local velocity reconstruction");
            continue;
        }
        if (face.meanArea==0.) continue;
        const auto l=MeasureCutRegionFaceInertiaVolumes(face,geometry.motion,chart.regions[face.left],true,planes[face.left]);
        const auto r=MeasureCutRegionFaceInertiaVolumes(face,geometry.motion,chart.regions[face.right],false,planes[face.right]);
        const double inertia=settings.waterDensity*(l[0]+r[0])+settings.gasDensity*(l[1]+r[1]);
        const double u=(source.faceSpeed.empty()?0.:source.faceSpeed[f])+settings.dt*settings.gravity[face.axis];
        const double areaImpulse=settings.dt*face.meanArea;
        const double g=areaImpulse*areaImpulse/inertia;
        if (!(inertia>0. && g>0. && std::isfinite(g) && std::isfinite(u)))
            return Fail(error,"invalid retained boundary inertia");
        ports.push_back({f,a,b,g,areaImpulse*u});links.push_back({a,b,g});
    }
    std::vector<Vent> boundary;
    for (std::size_t f=0;f<fragments;++f)
        for (const auto& vent:vents) {
            const auto& region=chart.regions[f];
            const double area=MeasureCutRegionGasVent(region,geometry.motion,vent,planes[f]).mean;
            if (area==0.) continue;
            const double length=.5*(region.hi[vent.axis]-region.lo[vent.axis]);
            const double g=settings.dt*settings.dt*area/(settings.gasDensity*length);
            boundary.push_back({owners.owner[f],f,g,settings.dt*settings.dt*area*settings.gravity[vent.axis]*vent.sign});
        }
    CutOwnedProjection next;
    next.faceVolume.resize(geometry.faces.size());next.waterFlux.resize(geometry.faces.size());
    std::vector<double> density(count),storage(count),rhs(count),residual(count),linearTolerance(count);
    std::vector<Sum> outWater(count),outMass(count),incomingWater(count),incomingMass(count);
    std::vector<double> outgoingWater(count),outgoingMass(count),unsupported(count);
    for (int iteration=0;iteration<settings.iterationLimit;++iteration) {
        for (std::size_t o=0;o<count;++o) {
            density[o]=settings.gasDensity+pressure[o]/c2;
            if (!(density[o]>0. && std::isfinite(density[o]))) return Fail(error,"owned pressure leaves gas EOS domain");
        }
        std::fill(outWater.begin(),outWater.end(),Sum{});std::fill(outMass.begin(),outMass.end(),Sum{});
        std::fill(incomingWater.begin(),incomingWater.end(),Sum{});std::fill(incomingMass.begin(),incomingMass.end(),Sum{});
        std::fill(outgoingWater.begin(),outgoingWater.end(),0.);std::fill(outgoingMass.begin(),outgoingMass.end(),0.);
        std::fill(unsupported.begin(),unsupported.end(),0.);
        for (const auto& port:ports) {
            const double q=port.predictor+port.g*(pressure[port.left]-pressure[port.right]);
            const auto donor=q>=0.?port.left:port.right, receiver=q>=0.?port.right:port.left;
            CutOwnedDeparture departure;
            if (!MeasureCutOwnedDeparture(chart,geometry,owners,planes,port.face,q,departure,error)) return false;
            double wet=std::copysign(departure.water,q);
            const double missing=std::abs(q)-(departure.water+departure.gas);
            const double roundoff=256.*std::numeric_limits<double>::epsilon()*std::abs(q);
            const double unowned=std::max(departure.unsupportedVolume,std::abs(missing)>roundoff?std::abs(missing):0.);
            unsupported[donor]+=unowned;
            if (unowned>0.) {
                // Trial residual continuation only. A missing volume is
                // not an accepted material transfer; final audit rejects.
                const auto& face=geometry.faces[port.face];
                const auto trace=MeasureCutRegionFacePhase(face,geometry.motion,planes[face.left],planes[face.right]);
                wet=q*trace.wet[q>=0.?0:1].mean/face.meanArea;
            }
            const double gm=(q-wet)*density[donor];
            outWater[port.left].Add(wet);outWater[port.right].Add(-wet);
            outMass[port.left].Add(gm);outMass[port.right].Add(-gm);
            incomingWater[receiver].Add(std::abs(wet));incomingMass[receiver].Add(std::abs(gm));
            outgoingWater[donor]+=std::abs(wet);outgoingMass[donor]+=std::abs(gm);
            next.faceVolume[port.face]=q;next.waterFlux[port.face]=wet;
        }
        next.externalGasMass=0.;
        next.ventVolume.clear();
        for (const auto& vent:boundary) {
            const double q=vent.predictor+vent.g*pressure[vent.owner];
            next.ventVolume.push_back({vent.fragment,q});
            const double gm=q*(q>=0.?density[vent.owner]:settings.gasDensity);
            outMass[vent.owner].Add(gm);next.externalGasMass+=gm;
            if (gm>=0.) outgoingMass[vent.owner]+=gm;else incomingMass[vent.owner].Add(-gm);
        }
        next.water.resize(count);next.gasMass.resize(count);next.residual=0.;
        double required=settings.volumeTolerance;
        bool localClosed=true;
        for (std::size_t o=0;o<count;++o) {
            next.water[o]=water[o]-outWater[o].Get();next.gasMass[o]=mass[o]-outMass[o].Get();
            residual[o]=gas[o]+outWater[o].Get()-next.gasMass[o]/density[o];
            next.residual=std::max(next.residual,std::abs(residual[o]));
            const double tolerance=settings.relativeVolumeTolerance*owners.capacity[o];
            linearTolerance[o] = owners.capacity[o] > 0. ?
                std::min(settings.volumeTolerance,tolerance) : settings.volumeTolerance;
            if (std::abs(residual[o])>tolerance) {
                localClosed=false;
                required=std::min(required,tolerance);
            }
            storage[o]=std::max(0.,next.gasMass[o])/(density[o]*density[o]*c2);
            rhs[o]=-residual[o];
        }
        for (const auto& vent:boundary)storage[vent.owner]+=vent.g;
        next.iterations=iteration+1;
        if (localClosed && next.residual<=settings.volumeTolerance) {
            if (diagnosticTrial) { *diagnosticTrial=next; diagnosticTrial->pressure=pressure; }
            for (std::size_t o=0;o<count;++o) {
                const bool pureWater=mass[o]==0. && incomingMass[o].Get()==0.;
                const bool pureGas=water[o]==0. && incomingWater[o].Get()==0.;
                const double tol=256.*std::numeric_limits<double>::epsilon()*owners.capacity[o];
                if (unsupported[o]>0. && !(pureWater||pureGas)) {
                    std::ostringstream detail;detail.precision(17);
                    detail<<"connected mixed-owner continuation lacks a geometric path: owner="<<o
                          <<" members="<<owners.members[o].size()<<" capacity="<<owners.capacity[o]
                          <<" unsupported_m3="<<unsupported[o]<<" outgoingWater="<<outgoingWater[o]
                          <<" initialWater="<<water[o]<<" incomingWater="<<incomingWater[o].Get()
                          <<" initialGasMass="<<mass[o]<<" outgoingGasMass="<<outgoingMass[o]
                          <<" incomingGasMass="<<incomingMass[o].Get()
                          <<" trialWater="<<next.water[o]<<" trialGasMass="<<next.gasMass[o]
                          <<" iteration="<<iteration+1<<" globalResidual="<<next.residual
                          <<" bounds=["<<owners.lo[o][0]<<","<<owners.hi[o][0]<<";"
                          <<owners.lo[o][1]<<","<<owners.hi[o][1]<<";"
                          <<owners.lo[o][2]<<","<<owners.hi[o][2]<<"]";
                    return Fail(error,detail.str());
                }
                if (!(next.water[o]>=0. && next.water[o]<=owners.capacity[o]+tol &&next.gasMass[o]>=0. &&
                      outgoingWater[o]<=water[o]+(pureWater?incomingWater[o].Get():0.)+tol &&
                      outgoingMass[o]<=(mass[o]+(pureGas?incomingMass[o].Get():0.))*(1.+256.*std::numeric_limits<double>::epsilon())))
                    return Fail(error,"connected material transport violates owned phase inventory at owner "+std::to_string(o));
            }
            next.pressure=std::move(pressure);output=std::move(next);error.clear();return true;
        }
        CutHydroacousticGraphResult correction;
        if (!SolveCutHydroacousticGraphLocal(storage,links,rhs,linearTolerance,correction,error,pressureFailure)) {
            std::ostringstream detail;detail.precision(17);
            detail<<"owned pressure closure failed: "<<error<<" requested="<<required
                  <<" globalResidual="<<next.residual<<" iteration="<<iteration+1;
            return Fail(error,detail.str());
        }
        for (std::size_t o=0;o<count;++o)pressure[o]+=correction.pressure[o];
    }
    std::ostringstream detail;detail.precision(17);detail<<"owned pressure/transport did not close: residual="<<next.residual;
    return Fail(error,detail.str());
}
} // namespace judas::volume_grid
