#include "PlanarMovingVolume.h"
#include "CoupledVolumeBox.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <utility>

namespace {

using namespace judas::volume_grid;

constexpr GridLayout kLayout{{12,12,12},.2};
constexpr double kDt = 1. / 120.;
constexpr double kGravity = -9.81;
constexpr double kHeight = 1.1;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double measurement = 0) {
        ++total;
        if (!pass) ++failed;
        std::printf("%s %s %.17g\n",pass ? "PASS" : "FAIL",label,measurement);
    }
};

template <typename Height>
MovingVolumeState Seed(Height waterHeight, const std::vector<BoxMotion>& boxes = {}) {
    MovingVolumeState result;
    result.water.resize(kLayout.CellCount());
    for (int axis = 0; axis < 3; ++axis)
        result.velocity[axis].resize(kLayout.FaceCount(axis));
    for (int k = 0; k < kLayout.cells[2]; ++k)
        for (int j = 0; j < kLayout.cells[1]; ++j)
            for (int i = 0; i < kLayout.cells[0]; ++i) {
                const GridVec lo{i*kLayout.h,j*kLayout.h,k*kLayout.h};
                const GridVec hi{(i+1)*kLayout.h,std::min((j+1)*kLayout.h,waterHeight(i,k)),
                                 (k+1)*kLayout.h};
                double water = (hi[0]-lo[0])*std::max(0.,hi[1]-lo[1])*(hi[2]-lo[2]);
                for (const auto& box : boxes) {
                    double occupied = 1;
                    for (int a = 0; a < 3; ++a)
                        occupied *= std::max(0.,std::min(hi[a],box.hi[a])-std::max(lo[a],box.lo[a]));
                    water -= occupied;
                }
                result.water[kLayout.Cell(i,j,k)] = water;
            }
    return result;
}

double Volume(const MovingVolumeState& state) {
    return std::accumulate(state.water.begin(),state.water.end(),0.);
}

double MaxSpeed(const MovingVolumeState& state) {
    double maximum = 0;
    for (const auto& axis : state.velocity)
        for (double value : axis)
            maximum = std::max(maximum,std::abs(value));
    return maximum;
}

double Dot(const GridVec& a, const GridVec& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

double ReplayedWaterError(const MovingVolumeState& before, const MovingVolumeStep& after) {
    auto water = before.water;
    for (int axis = 0; axis < 3; ++axis) {
        auto limits = kLayout.cells;
        ++limits[axis];
        for (int k = 0; k < limits[2]; ++k)
            for (int j = 0; j < limits[1]; ++j)
                for (int i = 0; i < limits[0]; ++i) {
                    const int right[]{i,j,k};
                    if (right[axis] == 0 || right[axis] == kLayout.cells[axis])
                        continue;
                    int left[]{i,j,k};
                    --left[axis];
                    const auto face = kLayout.Face(axis,i,j,k);
                    const double transferred = (after.materialMassFlux[axis][face]-
                                                after.flux[axis][face])/999;
                    water[kLayout.Cell(left[0],left[1],left[2])] -= transferred;
                    water[kLayout.Cell(i,j,k)] += transferred;
                }
    }
    double sum = 0;
    for (std::size_t i = 0; i < water.size(); ++i)
        sum += std::abs(water[i]-after.state.water[i]);
    return sum;
}

void NonplanarContinuation(Checks& checks) {
    const auto initial = Seed([](int i,int k) {
        return kHeight+.025*std::sin(.51*i)+.018*std::cos(.73*k);
    });
    auto state = initial;
    PlanarMovingStep result;
    std::string error;
    double maxVolumeError = 0, maxFluxReplayError = 0, peak = 0;
    int advanced = 0;
    for (int step = 0; step < 24; ++step) {
        if (!AdvanceEvolvingVolume(kLayout,{},state,kDt,1,kGravity,result,error)) {
            std::printf("nonplanar continuation failed at step %d: %s\n",step,error.c_str());
            break;
        }
        ++advanced;
        maxVolumeError = std::max(maxVolumeError,std::abs(Volume(result.fluid.state)-Volume(initial)));
        maxFluxReplayError = std::max(maxFluxReplayError,
            ReplayedWaterError(state,result.fluid)-result.fluid.waterRoundoffCorrection);
        peak = std::max(peak,MaxSpeed(result.fluid.state));
        state = std::move(result.fluid.state);
    }
    checks.Expect(advanced == 24,"nonplanar free surface continues through 24 evolving steps",advanced);
    checks.Expect(maxVolumeError < 2e-10,"evolving free surface conserves global water volume",maxVolumeError);
    checks.Expect(maxFluxReplayError < 2e-11,"published phase flux reproduces each evolving cell water update",
                  maxFluxReplayError);
    double changed = 0;
    for (std::size_t i = 0; i < state.water.size(); ++i)
        changed = std::max(changed,std::abs(state.water[i]-initial.water[i]));
    checks.Expect(peak > .01 && std::isfinite(peak),"gravity causes resolved flow on sloped free surface",peak);
    checks.Expect(changed > 1e-5,"nonplanar continuation actually transports material",changed);
}

void DistinctWetAndDryCirculation(Checks& checks) {
    const BoxMotion dryBody{41,{1.18,.16,.62},{1.63,.49,1.27},{}};
    auto state = Seed([](int i,int) { return i < 3 ? 1.2 : 0.; },{dryBody});
    constexpr double speed = .18;
    const auto addLoop = [&](int x,int y) {
        state.velocity[0][kLayout.Face(0,x+1,y,0)] = speed;
        state.velocity[1][kLayout.Face(1,x+1,y+1,0)] = speed;
        state.velocity[0][kLayout.Face(0,x+1,y+1,0)] = -speed;
        state.velocity[1][kLayout.Face(1,x,y+1,0)] = -speed;
    };
    addLoop(0,3); // above the flat reference height, actually fully wet
    addLoop(8,0); // below the flat reference height, actually fully dry

    PlanarMovingStep result;
    std::string error;
    const bool ok = AdvanceEvolvingVolume(kLayout,{dryBody},state,kDt,1,0,result,error);
    if (!ok) std::printf("phase separation failed: %s\n",error.c_str());
    checks.Expect(ok,"independent local wet and dry loops advance");
    if (!ok) return;

    const auto wetFace = kLayout.Face(0,1,3,0);
    const auto dryFace = kLayout.Face(0,9,0,0);
    const double expectedFlux = kDt*kLayout.h*kLayout.h*speed;
    const double wetMassError = std::abs(result.fluid.materialMassFlux[0][wetFace]-1000*expectedFlux);
    const double dryMassError = std::abs(result.fluid.materialMassFlux[0][dryFace]-expectedFlux);
    checks.Expect(wetMassError < 2e-12,"high local water transports water even above global reference",
                  wetMassError);
    checks.Expect(dryMassError < 2e-14,"dry local air transports only air even below global reference",
                  dryMassError);
    const auto& loads = result.loads;
    const double force = loads.size() == 1 ? std::sqrt(Dot(loads.front().force,loads.front().force)) : INFINITY;
    checks.Expect(loads.size() == 1 && loads.front().owner == dryBody.owner && force < 1e-10,
                  "disjoint wet circulation creates no phantom pressure on dry body",force);
    double dryWater = 0;
    for (int k = 0; k < kLayout.cells[2]; ++k)
        for (int j = 0; j < kLayout.cells[1]; ++j)
            for (int i = 3; i < kLayout.cells[0]; ++i)
                dryWater += result.fluid.state.water[kLayout.Cell(i,j,k)];
    checks.Expect(dryWater < 1e-12,"no water is invented across the untouched dry gap",dryWater);

    auto badState = state;
    badState.water[kLayout.Cell(0,0,0)] += .0001;
    const auto saved = result;
    const bool accepted = AdvanceEvolvingVolume(kLayout,{dryBody},badState,kDt,1,0,result,error);
    checks.Expect(!accepted && !error.empty(),"nonplanar overcapacity state is rejected");
    checks.Expect(result.fluid.state.water == saved.fluid.state.water &&
                  result.fluid.pressure == saved.fluid.pressure &&
                  !saved.loads.empty() && result.loads.size() == saved.loads.size() &&
                  result.loads.front().force == saved.loads.front().force,
                  "overcapacity rejection leaves the last fluid and body result intact");
}

void ContinuingBoxCrossing(Checks& checks) {
    BoxMotion box{49,{.43,kHeight+.004,.61},{.91,1.32,1.47},{.0005,-.0015,.0003}};
    auto state = Seed([](int,int) { return kHeight; },{box});
    const double volume = Volume(state);
    PlanarMovingStep result;
    std::string error;
    double maxVolumeError = 0,maxReplayError = 0,absoluteWork = 0;
    int steps = 0;
    for (int step = 0; step < 10; ++step) {
        if (!AdvanceEvolvingVolume(kLayout,{box},state,kDt,1,kGravity,result,error)) {
            std::printf("moving box continuation failed at step %d: %s\n",step,error.c_str());
            break;
        }
        ++steps;
        maxVolumeError = std::max(maxVolumeError,std::abs(Volume(result.fluid.state)-volume));
        maxReplayError = std::max(maxReplayError,
            ReplayedWaterError(state,result.fluid)-result.fluid.waterRoundoffCorrection);
        if (result.loads.size() != 1 || result.loads.front().owner != box.owner) {
            checks.Expect(false,"moving box retains pressure owner during continuation");
            return;
        }
        absoluteWork += std::abs(Dot(result.loads.front().force,box.translation));
        for (int a = 0; a < 3; ++a) {
            box.lo[a] += box.translation[a];
            box.hi[a] += box.translation[a];
        }
        state = std::move(result.fluid.state);
    }
    checks.Expect(steps == 10 && box.lo[1] < kHeight,
                  "evolving solver continues after a dry box crosses into water",steps);
    checks.Expect(maxVolumeError < 2e-10,"repeated moving box sweep conserves all water",maxVolumeError);
    checks.Expect(maxReplayError < 2e-11,"moving box phase flux reproduces each cell update",maxReplayError);
    checks.Expect(absoluteWork > 1e-3 && std::isfinite(absoluteWork),
                  "moving box has nonzero finite work through actual pressure evolution",absoluteWork);
}

void AdaptiveCutVolumeCrossing(Checks& checks) {
    const BoxMotion box{37,{.43,kHeight+.004,.61},{.91,1.58,1.47},{.003,-.012,.002}};
    const auto initial = Seed([](int,int) { return kHeight; },{box});
    constexpr double dt = 1. / 60.;
    EvolvingVolumeInterval result;
    std::string error;
    const bool accepted = AdvanceEvolvingVolumeInterval(kLayout,{box},initial,dt,1,
                                                        kGravity,1,result,error);
    if (!accepted) std::printf("adaptive crossing failed: %s\n",error.c_str());
    checks.Expect(accepted,"original 12 mm crossing succeeds with exact path subdivision");
    if (!accepted) return;
    checks.Expect(result.substeps == 2 && result.rejectedAttempts == 1 && result.maximumCfl < 1,
                  "subdivision addresses cut-cell capacity with two admitted half steps",result.maximumCfl);
    const double waterError = std::abs(Volume(initial)-Volume(result.final.fluid.state));
    checks.Expect(waterError < 2e-10,"subdivided original crossing conserves global water",waterError);

    auto state = initial;
    GridPressureLoad mean;
    for (int i = 0; i < 2; ++i) {
        BoxMotion half = box;
        const double begin = .5*i;
        for (int a = 0; a < 3; ++a) {
            half.lo[a] = std::fma(begin,box.translation[a],box.lo[a]);
            half.hi[a] = std::fma(begin,box.translation[a],box.hi[a]);
            half.translation[a] *= .5;
        }
        PlanarMovingStep step;
        if (!AdvanceEvolvingVolume(kLayout,{half},state,dt/2,1,kGravity,step,error) ||
            step.loads.size() != 1 || step.loads.front().owner != box.owner) {
            checks.Expect(false,"independent prescribed half-step replay resolves");
            return;
        }
        for (int a = 0; a < 3; ++a) {
            mean.force[a] += step.loads.front().force[a]/2;
            mean.torque[a] += step.loads.front().torque[a]/2;
        }
        state = std::move(step.fluid.state);
    }
    checks.Expect(state.water == result.final.fluid.state.water &&
                  state.velocity == result.final.fluid.state.velocity,
                  "adaptive endpoint matches two independently submitted half intervals");
    checks.Expect(result.meanLoads.size() == 1 && result.meanLoads.front().owner == box.owner &&
                  result.meanLoads.front().force == mean.force && result.meanLoads.front().torque == mean.torque,
                  "subdivision publishes time-weighted force and torque once");

    const auto saved = result;
    const bool acceptedWithoutSplit = AdvanceEvolvingVolumeInterval(kLayout,{box},initial,dt,1,
                                                                    kGravity,0,result,error);
    checks.Expect(!acceptedWithoutSplit && error.find("donor capacity") != std::string::npos,
                  "original faster crossing still rejects with subdivision disabled");
    checks.Expect(!saved.meanLoads.empty() && result.meanLoads.size() == saved.meanLoads.size() &&
                  result.meanLoads.front().force == saved.meanLoads.front().force &&
                  result.final.fluid.state.water == saved.final.fluid.state.water &&
                  result.final.fluid.state.velocity == saved.final.fluid.state.velocity,
                  "unsplit capacity failure does not replace accepted endpoint or reaction");
}

void NeutralCoupledRest(Checks& checks) {
    for (const bool halfWet : {false,true}) {
        const double height = halfWet ? kHeight : 0;
        const BoxMotion shape{54,{.43,halfWet ? kHeight-.138 : 1.106,.61},
                              {.91,halfWet ? kHeight+.138 : 1.382,1.47},{}};
        auto state = Seed([&](int,int) { return height; },{shape});
        const double volume = Volume(state);
        const double area = (shape.hi[0]-shape.lo[0])*(shape.hi[2]-shape.lo[2]);
        const double depth = shape.hi[1]-shape.lo[1];
        const double waterDepth = std::clamp(height-shape.lo[1],0.,depth);
        const double mass = area*(depth+999*waterDepth);
        CoupledBoxState body{shape.owner,shape.lo,shape.hi,{},mass};
        double maxForceError = 0,maxSpeed = 0;
        int advanced = 0;
        for (int i = 0; i < 4; ++i) {
            CoupledBoxStep step;
            std::string error;
            if (!AdvanceEvolvingCoupledVolumeBox(kLayout,body,state,kDt,1,kGravity,step,error)) {
                std::printf("neutral %s rest failed at step %d: %s\n",
                            halfWet ? "half-wet" : "all-air",i,error.c_str());
                break;
            }
            ++advanced;
            auto errorForce = step.pressureForce;
            errorForce[1] += mass*kGravity;
            maxForceError = std::max(maxForceError,std::sqrt(Dot(errorForce,errorForce)));
            maxSpeed = std::max(maxSpeed,std::sqrt(Dot(step.body.velocity,step.body.velocity)));
            body = step.body;
            state = std::move(step.fluid.state);
        }
        checks.Expect(advanced == 4 && maxSpeed < 1e-8,
                      halfWet ? "neutral half-submerged body stays at hydrostatic rest"
                              : "neutral dry body has only physical air buoyancy",maxSpeed);
        checks.Expect(advanced == 4 && maxForceError < 2e-6,
                      "finite-body pressure force independently balances neutral weight",maxForceError);
        checks.Expect(std::abs(Volume(state)-volume) < 2e-10,
                      "neutral finite-body rest preserves water quantity",std::abs(Volume(state)-volume));
    }
}

void CoupledWaterlineTrajectory(Checks& checks, bool ownsMomentum) {
    const BoxMotion shape{58,{.43,1.106,.61},{.91,1.382,1.47},{}};
    auto state = Seed([](int,int) { return kHeight; },{shape});
    const double area = (shape.hi[0]-shape.lo[0])*(shape.hi[2]-shape.lo[2]);
    const double solidVolume = area*(shape.hi[1]-shape.lo[1]);
    const double tankArea = kLayout.cells[0]*kLayout.h*kLayout.cells[2]*kLayout.h;
    const double mass = 650*solidVolume;
    CoupledBoxState body{shape.owner,shape.lo,shape.hi,{0,-.1,0},mass};
    const double water = Volume(state);
    HalfDualVolumeState halfVelocity;
    for (auto& axis : halfVelocity.velocity)
        axis.resize(2*kLayout.CellCount());
    double maxImpulseError = 0,maxTravelError = 0,maxEnergyError = 0;
    double maxVolumeError = 0,peakUpwardForce = 0;
    double transportMomentumError = 0, mergeLoss = 0;
    int advanced = 0;
    for (int i = 0; i < 16; ++i) {
        CoupledBoxStep step;
        std::string error;
        OwnedCoupledBoxStep owned;
        const bool accepted = ownsMomentum ?
            AdvanceOwnedEvolvingCoupledVolumeBox(kLayout,body,state,halfVelocity,kDt,1,kGravity,owned,error) :
            AdvanceEvolvingCoupledVolumeBox(kLayout,body,state,kDt,1,kGravity,step,error);
        if (!accepted) {
            std::printf("coupled waterline failed at step %d: %s\n",i,error.c_str());
            break;
        }
        if (ownsMomentum) {
            step = std::move(owned.box);
            halfVelocity = std::move(owned.halfVelocity);
            mergeLoss += owned.mergeKineticLoss;
            for (double residual : owned.transportMomentumError)
                transportMomentumError = std::max(transportMomentumError,std::abs(residual));
        }
        ++advanced;
        auto free = body.velocity;
        free[1] += kDt*kGravity;
        GridVec change{};
        for (int a = 0; a < 3; ++a) {
            change[a] = step.body.velocity[a]-free[a];
            const double errorImpulse = mass*change[a]-kDt*step.pressureForce[a];
            const double errorTravel = step.body.lo[a]-body.lo[a]-kDt*step.body.velocity[a];
            maxImpulseError = std::max(maxImpulseError,std::abs(errorImpulse));
            maxTravelError = std::max(maxTravelError,std::abs(errorTravel));
        }

        const double quantity = Volume(state);
        double height = quantity/tankArea;
        if (height > body.lo[1]) {
            height = (quantity-area*body.lo[1])/(tankArea-area);
            if (height > body.hi[1])
                height = (quantity+solidVolume)/tankArea;
        }
        // Exact pressure integral over the translated solid, independently
        // referenced to the equal-volume planar gravitational background.
        const auto integral = [&](const CoupledBoxState& state) {
            const double lo = state.lo[1]-height, hi = state.hi[1]-height;
            const double wetLo = std::min(lo,0.), wetHi = std::min(hi,0.);
            return .5*area*kGravity*(hi*hi-lo*lo+999*(wetHi*wetHi-wetLo*wetLo));
        };
        const double rigidKinetic = .5*mass*(Dot(step.body.velocity,step.body.velocity)-Dot(free,free));
        const double hydrostaticPotential = integral(step.body)-integral(body);
        const double energyResidual = rigidKinetic+hydrostaticPotential+.5*mass*Dot(change,change)+
            step.fluid.pressureKineticWork+step.fluid.pressureCorrectionEnergy;
        maxEnergyError = std::max({maxEnergyError,std::abs(energyResidual),
                                    std::abs(energyResidual-step.pressureEnergyResidual)});
        maxVolumeError = std::max(maxVolumeError,std::abs(Volume(step.fluid.state)-water));
        peakUpwardForce = std::max(peakUpwardForce,step.pressureForce[1]);
        body = step.body;
        state = std::move(step.fluid.state);
    }
    checks.Expect(advanced == 16 && body.lo[1] < kHeight && body.velocity[1] < 0,
                  "unprescribed finite-mass body crosses into evolving water",body.lo[1]);
    const double freefallSpeed = -.1+advanced*kDt*kGravity;
    checks.Expect(advanced == 16 && body.velocity[1] > freefallSpeed+.1 && peakUpwardForce > 10,
                  "water pressure measurably slows the entering body",body.velocity[1]-freefallSpeed);
    checks.Expect(maxImpulseError < 2e-6,"reported reaction closes all components of rigid impulse",maxImpulseError);
    checks.Expect(maxTravelError < 2e-14,"rigid position and accepted velocity use the same motion",maxTravelError);
    checks.Expect(maxEnergyError < 1e-7,"coupled pressure work and swept hydrostatic potential close",maxEnergyError);
    checks.Expect(maxVolumeError < 2e-10,"unprescribed crossing retains all water",maxVolumeError);
    if (ownsMomentum) {
        checks.Expect(transportMomentumError < 2e-10,
                      "coupled waterline transport closes the material momentum ledger",
                      transportMomentumError);
        checks.Expect(mergeLoss > 0 && std::isfinite(mergeLoss),
                      "coupled waterline actually advects and merges unequal half velocities",mergeLoss);
    }
}

void RotateEvolvingChart(Checks& checks) {
    const BoxMotion box{61,{.43,.952,.61},{.91,1.328,1.47},{.0005,-.0015,.0003}};
    const auto source = Seed([](int i,int k) {
        return kHeight+.025*std::sin(.51*i)+.018*std::cos(.73*k);
    },{box});
    const auto rotate = [](const GridVec& a) { return GridVec{a[2],a[0],a[1]}; };
    const BoxMotion turned{box.owner,rotate(box.lo),rotate(box.hi),rotate(box.translation)};
    auto rotated = source; // zero velocity stays zero under the exact permutation.
    for (int k = 0; k < kLayout.cells[2]; ++k)
        for (int j = 0; j < kLayout.cells[1]; ++j)
            for (int i = 0; i < kLayout.cells[0]; ++i)
                rotated.water[kLayout.Cell(k,i,j)] = source.water[kLayout.Cell(i,j,k)];

    PlanarMovingStep a,b;
    std::string error;
    const bool solved = AdvanceEvolvingVolume(kLayout,{box},source,kDt,1,kGravity,a,error) &&
        AdvanceEvolvingVolume(kLayout,{turned},rotated,kDt,2,kGravity,b,error);
    if (!solved) std::printf("dynamic chart rotation failed: %s\n",error.c_str());
    checks.Expect(solved,"nonplanar moving fixture solves after cyclic chart rotation");
    if (!solved) return;
    double waterError = 0,pressureError = 0,velocityError = 0;
    for (int k = 0; k < kLayout.cells[2]; ++k)
        for (int j = 0; j < kLayout.cells[1]; ++j)
            for (int i = 0; i < kLayout.cells[0]; ++i) {
                const auto x = kLayout.Cell(i,j,k), y = kLayout.Cell(k,i,j);
                waterError = std::max(waterError,std::abs(a.fluid.state.water[x]-b.fluid.state.water[y]));
                pressureError = std::max(pressureError,std::abs(a.fluid.pressure[x]-b.fluid.pressure[y]));
            }
    for (int axis = 0; axis < 3; ++axis) {
        auto limits = kLayout.cells;
        ++limits[axis];
        const int turnedAxis = (axis+1)%3;
        for (int k = 0; k < limits[2]; ++k)
            for (int j = 0; j < limits[1]; ++j)
                for (int i = 0; i < limits[0]; ++i) {
                    const auto x = kLayout.Face(axis,i,j,k),y = kLayout.Face(turnedAxis,k,i,j);
                    velocityError = std::max(velocityError,
                        std::abs(a.fluid.state.velocity[axis][x]-b.fluid.state.velocity[turnedAxis][y]));
                }
    }
    checks.Expect(waterError < 2e-11,"transported phase water rotates with its chart",waterError);
    checks.Expect(pressureError < 2e-6 && velocityError < 2e-8 &&
                  MaxSpeed(a.fluid.state) > 1e-3,
                  "actual evolving pressure and flow rotate with the chart",velocityError);
    double loadError = 0;
    if (a.loads.size() == 1 && b.loads.size() == 1) {
        const auto expectedForce = rotate(a.loads[0].force),expectedTorque = rotate(a.loads[0].torque);
        for (int axis = 0; axis < 3; ++axis)
            loadError = std::max({loadError,std::abs(b.loads[0].force[axis]-expectedForce[axis]),
                                  std::abs(b.loads[0].torque[axis]-expectedTorque[axis])});
    } else {
        loadError = INFINITY;
    }
    checks.Expect(loadError < 2e-6,"coupled moving pressure force and torque rotate",loadError);
}

} // namespace

int main() {
    Checks checks;
    NonplanarContinuation(checks);
    DistinctWetAndDryCirculation(checks);
    ContinuingBoxCrossing(checks);
    AdaptiveCutVolumeCrossing(checks);
    NeutralCoupledRest(checks);
    CoupledWaterlineTrajectory(checks,false);
    CoupledWaterlineTrajectory(checks,true);
    RotateEvolvingChart(checks);
    std::printf("SUMMARY evolving volume %d checks %d failures\n",checks.total,checks.failed);
    return checks.failed ? 1 : 0;
}
