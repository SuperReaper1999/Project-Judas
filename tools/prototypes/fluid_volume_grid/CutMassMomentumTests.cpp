#include "CutMassMomentum.h"
#include <cmath>
#include <iostream>
#include <limits>

using namespace judas::volume_grid;
namespace {
using Real = CutCompensated;
int checks = 0, failures = 0;
void Check(bool ok, const char* name) { ++checks; if (!ok) { ++failures; std::cerr << name << '\n'; } }
bool Near(double a, double b) { return std::abs(a-b) <= 1e-11 * std::max(1., std::abs(b)); }
CutMomentumVector V(double x, double y = 0., double z = 0.) { return {Real(x),Real(y),Real(z)}; }
}
int main(int argc, char** argv) {
    const auto backend = argc > 1 && std::string(argv[1]) == "--reference"
        ? CutMomentumBackend::Reference : CutMomentumBackend::Fast;
    std::string error;
    CutMassMomentumResult result;
    const std::vector<double> capacity{1.,1.};
    auto Run = [&](const std::vector<Real>& start, const std::vector<Real>& end,
        const std::vector<CutTransportFace>& flux, const std::vector<CutMomentumBoundary>& ports,
        const std::vector<CutMomentumVector>& v, const std::vector<CutMomentumVector>& impulse) {
        return TransportCutMassMomentum(capacity,start,end,flux,ports,v,impulse,1e-18,1e-12,result,error,backend);
    };
    Check(Run({2.,1.},{1.,2.},{{0,1,1.}},{},{V(3.,-2.,7.),V(3.,-2.,7.)},{V(0),V(0)}), "uniform transport admitted");
    for (auto v : result.velocity) for (int k=0;k<3;++k)
        Check(Near(v[k].Value(), V(3.,-2.,7.)[k].Value()), "uniform velocity preserved while mass moves");
    Check(Near(result.beforeEnergy,result.afterEnergy), "uniform kinetic energy conserved");
    Check(Near(result.mixingLoss,0.) && Near(result.unsteadyLoss,0.), "uniform flow has no numerical mixing loss");
    Check(Run({2.,1.},{1.,2.},{{0,1,1.}},{},{V(4.),V(-2.)},{V(0),V(0)}), "two-cell mixing admitted");
    Check(Near(result.velocity[0][0].Value(),4.) && Near(result.velocity[1][0].Value(),1.), "independent two-cell momentum solution");
    Check(Near(result.beforeEnergy,18.) && Near(result.afterEnergy,9.), "independent two-cell kinetic energy");
    Check(Near(result.unsteadyLoss,4.5) && Near(result.mixingLoss,4.5), "loss is exactly accounted transport mixing");
    Check(Near(result.energyResidual,0.), "discrete work identity closes");
    Check(Run({1.,2.},{2.,1.},{{0,1,-1.}},{},{V(-2.),V(4.)},{V(0),V(0)}), "reversed carrier admitted");
    Check(Near(result.velocity[0][0].Value(),1.) && Near(result.velocity[1][0].Value(),4.), "negative flux changes donor");
    Check(Run({1.,1.},{1.,1.},{},{},{V(0),V(0)},{V(2.,3.,4.),V(-2.,-3.,-4.)}), "impulse admitted");
    Check(Near(result.afterEnergy,29.) && Near(result.impulseWork,58.) && Near(result.unsteadyLoss,29.), "backward-Euler impulse work accounted independently");
    Check(Run({1.,1.},{1.,1.},{},{{0,-1.,V(5.)},{0,1.,V(0)}},{V(1.),V(0)},{V(0),V(0)}), "open-flow momentum admitted");
    Check(Near(result.velocity[0][0].Value(),3.), "inflow and outflow use their actual donor velocities");
    Check(Near(result.externalMomentum[0].Value(),-2.) && Near(result.externalEnergy,-8.), "boundary momentum and energy are extensive");
    Check(Near(result.momentumResidual[0].Value(),0.) && Near(result.energyResidual,0.), "open work/impulse ledgers close");
    for (double mass : {1e-12,1e-6,1.,1e6}) {
        std::vector<Real> start{Real(mass)*Real(2.),Real(mass)}, end{Real(mass),Real(mass)*Real(2.)};
        Check(Run(start,end,{{0,1,mass}},{},{V(3.),V(3.)},{V(0),V(0)}), "mass-scale transport admitted");
        Check(Near(result.velocity[0][0].Value(),3.) && Near(result.velocity[1][0].Value(),3.), "mass-scale uniform momentum survives");
    }
    auto saved=result.velocity;
    Check(!Run({1.,1.},{2.,1.},{},{},{V(0),V(0)},{V(0),V(0)}), "mass without a carrier rejects");
    Check(result.velocity[0][0].Value()==saved[0][0].Value(), "mass rejection preserves publication");
    Check(!Run({1.,1.},{0.,2.},{{0,1,1.}},{},{V(0),V(0)},{V(0),V(0)}), "empty velocity owner explicitly unsupported");
    Check(!Run({1.,1.},{1.,1.},{},{},{V(std::numeric_limits<double>::quiet_NaN()),V(0)},{V(0),V(0)}), "NaN velocity rejects");
    Check(!Run({1.,1.},{1.,1.},{{0,2,0.}},{},{V(0),V(0)},{V(0),V(0)}), "invalid carrier ownership rejects");

    CutSelectiveTransport phases; std::vector<Real> massFlux;
    Check(phases.Build({1.,1.},{{0,1,.5}}, {}, {.75,.25},{.3,.9},1.2,1e-14,error), "real two-material transport prepared");
    Check(phases.InteriorMassFlux(1000.,massFlux,error), "accepted material mass flux exposed");
    std::vector<Real> startMass{Real(750.)+Real(.3),Real(250.)+Real(.9)}, endMass;
    for (int i=0;i<2;++i) endMass.push_back(Real(1000.)*phases.Water()[i]+phases.Mass()[i]);
    // The phase solve's 1e-14 m^3 budget implies 1e-11 kg at 1000 kg/m^3;
    // unlike the exact controls, this caller must declare that input error.
    const bool materialOk=TransportCutMassMomentum(capacity,startMass,endMass,
        {{0,1,massFlux[0]}},{},{V(2.),V(2.)},{V(0),V(0)},1e-11,1e-12,result,error,backend);
    if(!materialOk) std::cerr<<error<<"\n";
    Check(materialOk, "actual material flux drives compatible momentum");
    Check(Near(result.velocity[0][0].Value(),2.) && Near(result.velocity[1][0].Value(),2.), "material and momentum uniform flow agree");
    auto savedFlux=massFlux;
    Check(!phases.InteriorMassFlux(-1.,massFlux,error), "invalid material density rejects");
    Check(massFlux[0].Value()==savedFlux[0].Value(), "failed mass-flux query preserves output");

    // Negative control: retaining velocity during unowned mass replacement
    // creates energy even with zero impulse. The carrier gate must reject it.
    const double oldMass=1.2e-10,newMass=1e-7,u=3305.;
    Check(.5*newMass*u*u > 800.*(.5*oldMass*u*u), "legacy fixed-speed mass replacement adds unaccounted kinetic energy");
    Check(!Run({oldMass,1.},{newMass,1.},{},{},{V(u),V(0)},{V(0),V(0)}), "unowned half-dual mass replacement is not accepted as momentum transport");
    Check(TransportCutMassMomentum({1.,1.,1.},{1.,1.,1.},{1.,1.,1.},
        {{0,1,1.},{1,2,1.},{2,0,1.}}, {}, {V(1.),V(0),V(0)}, {V(0),V(0),V(0)},
        1e-18,1e-12,result,error,backend), "cyclic carrier admitted");
    Check(Near(result.velocity[0][0].Value(),4./7.) &&
        Near(result.velocity[1][0].Value(),2./7.) && Near(result.velocity[2][0].Value(),1./7.),
        "independent cyclic matrix inverse");
    Check(Near(result.afterEnergy,3./14.) &&
        Near(result.unsteadyLoss+result.mixingLoss,2./7.), "cyclic transport work identity");

    // An independently soluble arrival control using the observed first-face
    // mass ratio and impulse. This is NOT a coupled basin trajectory: its
    // incoming carrier is prescribed, and the pressure flux must be re-solved
    // before this velocity could be used in the actual pressure integrator.
    const Real gasMass=1.3411045074462915e-10, wetMass=1.1175865106418074e-7;
    const Real arriving=wetMass-gasMass, push=gasMass*Real(3299.2337958479102);
    Check(Run({gasMass,1.},{wetMass,Real(1.)-arriving},{{1,0,arriving}},{},
        {V(0),V(0)},{{push,Real(),Real()},V(0)}), "gas-to-water arrival carries momentum without velocity cloning");
    const double expected=(push/wetMass).Value();
    Check(Near(result.velocity[0][0].Value(),expected) && expected>3.9 && expected<4.,
        "arrival velocity follows actual ending mass and known impulse");
    Check(Near((result.momentum[0][0]+result.momentum[1][0]-push).Value(),0.) &&
        std::abs(result.energyResidual)<1e-16, "arrival preserves impulse and closes independent work identity");
    Check(.5*wetMass.Value()*3299.2337958479102*3299.2337958479102 > 600000.*result.afterEnergy,
        "unchanged-velocity arrival negative control exposes the energy amplification");
    std::cout << "CutMassMomentumTests: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
