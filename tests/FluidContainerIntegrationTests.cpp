#include "Project.h"
// FTFT9: actual authored-container, GameSession and StepPlayedWorld coupling.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <glm/gtc/quaternion.hpp>
#include "GameSession.h"
#include "ProductionFluidCoupling.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "Window.h"
namespace {
namespace fs = std::filesystem;
constexpr float dt = SimulationTiming::kFixedTimestep;
int checks = 0, failures = 0, totalSteps = 0;
void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
bool Finite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
struct Budget {
    double liquidMass = 0, liquidEnergy = 0, bodyEnergy = 0, retainedMass = 0;
    glm::dvec3 liquidMomentum{0}, bodyMomentum{0}, liquidCenter{0};
    glm::vec3 bodyPosition{0}, bodyVelocity{0};
    bool finite = true;
};
Budget Measure(RuntimeWorld& world, BodyHandle body) {
    Budget b;
    const auto pose = world.Physics().GetTransform(body);
    b.bodyPosition = pose.position;
    b.bodyVelocity = world.Physics().GetLinearVelocity(body);
    const auto spin = world.Physics().GetAngularVelocity(body);
    b.bodyMomentum = double(world.Physics().GetMass(body)) * glm::dvec3(b.bodyVelocity);
    b.bodyEnergy = .5 * double(world.Physics().GetMass(body)) * glm::dot(glm::dvec3(b.bodyVelocity), glm::dvec3(b.bodyVelocity)) +
                   .5 * glm::dot(glm::dvec3(spin), glm::dmat3(world.Physics().GetInertiaWorld(body)) * glm::dvec3(spin));
    for (const auto& p : world.Fluid().Particles()) {
        b.finite &= Finite(p.position) && Finite(p.velocity);
        b.liquidMass += p.mass;
        b.liquidMomentum += double(p.mass) * glm::dvec3(p.velocity);
        b.liquidEnergy += .5 * double(p.mass) * glm::dot(glm::dvec3(p.velocity), glm::dvec3(p.velocity));
        b.liquidCenter += double(p.mass) * glm::dvec3(p.position);
        const glm::vec3 local = glm::inverse(pose.rotation) * (p.position - pose.position);
        // Independent exact bounds from the authored tank construction.
        if (std::abs(local.x) <= .60001f && std::abs(local.z) <= .60001f && local.y >= -.00001f && local.y <= 1.20001f)
            b.retainedMass += p.mass;
    }
    if (b.liquidMass > 0) b.liquidCenter /= b.liquidMass;
    b.finite &= Finite(pose.position) && Finite(b.bodyVelocity) && Finite(spin) &&
                std::isfinite(b.bodyEnergy) && std::isfinite(b.liquidEnergy);
    return b;
}
Scene Fixture(float spacing, float mass, bool supported, bool gravity, bool sealed) {
    Scene s;
    s.Settings().name = "Ordinary compound container";
    s.Settings().fluidScale = spacing / .05f;
    auto& tank = s.CreateObject("Open compound vessel");
    tank.body = SceneBodyComponent{};
    auto& b = *tank.body;
    b.motion = SceneBodyMotion::Dynamic;
    b.shape = SceneShape::Compound;
    b.mass = mass; b.friction = .6f; b.restitution = 0;
    b.compoundBoxes = {{{0,-.05f,0},{.7f,.05f,.7f}}, {{-.65f,.6f,0},{.05f,.6f,.7f}},
                       {{.65f,.6f,0},{.05f,.6f,.7f}}, {{0,.6f,-.65f},{.6f,.6f,.05f}},
                       {{0,.6f,.65f},{.6f,.6f,.05f}}};
    if (sealed) b.compoundBoxes.push_back({{0,1.25f,0},{.7f,.05f,.7f}});
    b.fluidCavities = {{{0,.6f,0},{.6f,.6f,.6f}}};
    if (supported) {
        auto& floor = s.CreateObject("Support");
        floor.transform.position = {0,-.35f,0};
        floor.body = SceneBodyComponent{}; floor.body->halfExtents = {2,.25f,2};
        floor.body->restitution = 0;
    }
    auto& volume = s.CreateObject("Finite liquid lattice");
    volume.transform.position = {0,.5f*spacing,0};
    volume.fluidVolume = SceneFluidVolumeComponent{};
    volume.fluidVolume->spacing = spacing;
    volume.fluidVolume->countX = spacing < .18f ? 7 : 5;
    volume.fluidVolume->countY = 3;
    volume.fluidVolume->countZ = volume.fluidVolume->countX;
    // Fixture counts discretize a ~one-metre-wide block at the two stated resolutions.
    auto& field = s.CreateObject("Uniform acceleration");
    field.gravity = SceneGravityComponent{}; field.gravity->kind = SceneGravityKind::Uniform;
    field.gravity->magnitude = gravity ? 9.81f : 0;
    field.gravity->regionShape = SceneRegionShape::Box; field.gravity->regionHalfExtents = glm::vec3(1000);
    auto& start = s.CreateObject("Observer outside liquid");
    start.transform.position = {10,20,10}; start.playerStart = ScenePlayerStartComponent{};
    return s;
}
struct Result {
    std::string name;
    int steps = 0, failed = 0;
    double mass = 0, tankMass = 0, load = 0, expectedLoad = 0, loadRelativeError = 0;
    double retained = 0, maxEnergy = 0, maxMomentumResidual = 0, relativeDrift = 0;
    double synchronizedDrift = 0, synchronizedRetained = 0, bodyDriftError = 0;
    double preparationEnergyMax = 0, preparationFinalEnergy = 0, preparationRelativeRmsSpeed = 0;
    int preparationSteps = 0;
    double particleMedianMs = 0, couplingMedianMs = 0, runtime = 0;
    glm::dvec3 momentumResidual{0};
};
double Median(std::vector<double> values) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end()); return values[values.size()/2];
}
Result Run(const fs::path& output, const std::string& kind, float spacing, float mass, bool rawInitial, bool openZeroG) {
    Result result; result.name = kind + "-s" + std::to_string(spacing) + "-m" + std::to_string(int(mass));
    const auto start = std::chrono::steady_clock::now(); const int beforeFailures = failures;
    const bool supported = kind == "supported", gravity = kind != "zero-g-impulse";
    const bool hasSupport = supported || (!rawInitial && gravity);
    const bool sealed = !gravity && !rawInitial && !openZeroG;
    Scene authored = Fixture(spacing,mass,hasSupport,gravity,sealed), loaded;
    std::string text,error; SaveSceneToString(authored,text); std::ofstream(output/(result.name+".judas"))<<text;
    Check(LoadSceneFromString(text,loaded,error),result.name+" serialized scene load: "+error);
    RuntimeWorld world;ProjectSettings experimental;experimental.advancedFluidDynamics=true;world.ConfigureProject(experimental); GameSession session; Window window; window.SetTestInputMode(true);
    if (!world.Build(loaded,nullptr,error) || !session.Begin(world,error)) {
        Check(false,result.name+" ordinary runtime/session: "+error); result.failed=failures-beforeFailures; return result;
    }
    const auto* entity=world.FindEntity(1);
    const BodyHandle body=world.DynamicBodies()[entity->slot].Handle();
    const Budget unrelaxed=Measure(world,body);
    if (!rawInitial && !supported) {
        const int preparationMinimumSteps = gravity ? 360 : 240;
        std::ofstream preparation(output/(result.name+"-preparation.csv"));
        preparation << std::setprecision(17) << "step,body_kinetic_J,liquid_kinetic_J,body_y,liquid_y,retained_mass,body_vy,liquid_mean_vy,batch_rigid_rows,batch_iterations,batch_closing_speed,batch_cap,batch_support_Iy\n";
        for (int i=0; i<preparationMinimumSteps || !world.FluidCoupling().Measurements().executed; ++i) {
            StepPlayedWorld(session,window,dt); ++totalSteps; ++result.preparationSteps;
            const auto state=Measure(world,body);
            Check(state.finite && state.liquidMass==unrelaxed.liquidMass,result.name+" preparation finite/mass-preserving");
            result.preparationEnergyMax=std::max(result.preparationEnergyMax,state.bodyEnergy+state.liquidEnergy);
            preparation << i << ',' << state.bodyEnergy << ',' << state.liquidEnergy << ','
                        << state.bodyPosition.y << ',' << state.liquidCenter.y << ',' << state.retainedMass << ','
                        << state.bodyVelocity.y << ',' << state.liquidMomentum.y/state.liquidMass << ','
                        << world.Physics().GetParticleContactStats().rigidContactRows << ','
                        << world.Physics().GetParticleContactStats().iterations << ','
                        << world.Physics().GetParticleContactStats().maximumClosingSpeed << ','
                        << world.Physics().GetParticleContactStats().iterationCapReached << ','
                        << world.FluidCoupling().Measurements().containedStaticSupportImpulse.y << '\n';
        }
        if (gravity) {
            // Static authored supports are deliberately not lifecycle entities.
            // Remove their real collision body via the ordinary physics API;
            // no liquid/body state is reset or overwritten for the release.
            Check(world.StaticBodies().size()==1,result.name+" exactly one removable physical support");
            if (world.StaticBodies().size()==1) world.Physics().DestroyBody(world.StaticBodies()[0].handle);
        }
    }
    const Budget initial=Measure(world,body);
    result.preparationFinalEnergy=initial.bodyEnergy+initial.liquidEnergy;
    double relativeEnergy=0;
    for (const auto& particle:world.Fluid().Particles()) {
        const auto relative=glm::dvec3(particle.velocity-initial.bodyVelocity);
        relativeEnergy+=double(particle.mass)*glm::dot(relative,relative);
    }
    result.preparationRelativeRmsSpeed=std::sqrt(relativeEnergy/initial.liquidMass);
    result.mass=initial.liquidMass; result.tankMass=mass;
    const auto volumeObject=std::find_if(loaded.Objects().begin(),loaded.Objects().end(),[](const auto& o){return bool(o.fluidVolume);});
    const auto& volume=*volumeObject->fluidVolume;
    const double expectedMass=double(volume.countX*volume.countY*volume.countZ)*double(world.FluidSettingsUsed().restDensity)*
                              double(spacing)*double(spacing)*double(spacing);
    Check(std::abs(result.mass-expectedMass)<1e-5*expectedMass,result.name+" finite authored lattice independent mass");
    const glm::dvec3 externalImpulse=gravity?glm::dvec3(0):glm::dvec3(mass*.1,0,0);
    if (!gravity) {
        world.Physics().ApplyLinearImpulse(body,glm::vec3(externalImpulse));
        const double actualWork=Measure(world,body).bodyEnergy-initial.bodyEnergy;
        const double expectedWork=glm::dot(externalImpulse,glm::dvec3(initial.bodyVelocity))+
                                  glm::dot(externalImpulse,externalImpulse)/(2*mass);
        Check(std::abs(actualWork-expectedWork)<=1e-5,result.name+" external impulse work explicitly accounted");
        Check(std::abs(actualWork-glm::dot(externalImpulse,glm::dvec3(initial.bodyVelocity))-1.0)<=1e-4,
              result.name+" prescribed horizontal impulse supplies1J in pre-impulse co-moving frame");
        std::cout << "IMPULSE_WORK " << result.name << " measured_lab_J=" << actualWork
                  << " expected_lab_J=" << expectedWork << " co_moving_J="
                  << actualWork-glm::dot(externalImpulse,glm::dvec3(initial.bodyVelocity)) << '\n';
    }
    const int count=supported?360:120;
    const glm::dvec3 g=gravity?glm::dvec3(0,-double(9.81f),0):glm::dvec3(0);
    const double totalMass=mass+initial.liquidMass;
    double elapsedLiquid=0, loadImpulse=0, loadTime=0;
    glm::dvec3 sumContained{0}, sumExteriorNotApplied{0}, sumAnalytic{0}, sumCouplingSupport{0};
    std::vector<double> particleMs,couplingMs;
    std::ofstream csv(output/(result.name+".csv"));
    csv<<std::setprecision(17)<<"step,time,executed,fluid_dt,contained_mass,liquid_mass,retained_mass,tank_y,tank_vy,liquid_y,contained_Ix,contained_Iy,contained_Iz,body_Px,body_Py,body_Pz,liquid_Px,liquid_Py,liquid_Pz,gravity_Iy,residual_x,residual_y,residual_z,kinetic_J,hydro_Fy,drag_Fy,particle_ms,coupling_ms,batch_support_Ix,batch_support_Iy,batch_support_Iz,batch_iterations,batch_closing_speed,batch_speed_scale,batch_cap,batch_rigid_rows,pre_step_body_y,pre_step_tank_vy,liquid_mean_vy,batch_gap_rows,batch_accelerations,batch_matrix_products,batch_breakdowns\n";
    for(int i=0;i<count;++i) {
        const auto preStep=Measure(world,body);
        StepPlayedWorld(session,window,dt); ++totalSteps;
        const auto state=Measure(world,body); const auto& m=world.FluidCoupling().Measurements();
        const auto found=std::find_if(m.bodies.begin(),m.bodies.end(),[&](const auto& x){return x.body.id==body.id;});
        Check(found!=m.bodies.end(),result.name+" actual coupling reports dynamic container");
        if(found==m.bodies.end()) break;
        if(m.executed) {elapsedLiquid+=m.lastFluidDeltaTime;particleMs.push_back(m.particleMilliseconds);}
        couplingMs.push_back(m.totalMilliseconds);
        sumContained+=glm::dvec3(found->containedImpulse); sumExteriorNotApplied+=glm::dvec3(m.exteriorReactionNotApplied);
        sumAnalytic+=double(dt)*glm::dvec3(found->buoyancyForce+found->dragForce);
        sumCouplingSupport+=glm::dvec3(m.containedStaticSupportImpulse);
        const auto batch=world.Physics().GetParticleContactStats();
        const glm::dvec3 gravityImpulse=g*(double(mass)*double(dt)*double(i+1)+initial.liquidMass*elapsedLiquid);
        const glm::dvec3 residual=state.bodyMomentum+state.liquidMomentum-initial.bodyMomentum-initial.liquidMomentum-externalImpulse-gravityImpulse;
        result.momentumResidual=residual;
        result.maxMomentumResidual=std::max(result.maxMomentumResidual,glm::length(residual));
        const double energy=state.bodyEnergy+state.liquidEnergy;result.maxEnergy=std::max(result.maxEnergy,energy);
        Check(state.finite,result.name+" finite mechanical state");
        Check(state.liquidMass==initial.liquidMass,result.name+" particle mass unchanged");
        if(i>=count-120 && m.executed) {loadImpulse-=found->containedImpulse.y;loadTime+=m.lastFluidDeltaTime;}
        result.retained=state.retainedMass/initial.liquidMass;
        result.relativeDrift=glm::length((state.liquidCenter-glm::dvec3(state.bodyPosition))-(initial.liquidCenter-glm::dvec3(initial.bodyPosition)));
        if(m.executed) {
            result.synchronizedDrift=result.relativeDrift;
            result.synchronizedRetained=result.retained;
            // Judas gravity uses kick-then-drift. This oracle does not call any
            // production integrator: n equal gravity kicks give n(n+1)/2.
            const double n=i+1;
            const glm::dvec3 expected=glm::dvec3(initial.bodyPosition)+
                glm::dvec3(initial.bodyVelocity)*(double(dt)*n)+g*(double(dt)*double(dt)*n*(n+1)/2);
            result.bodyDriftError=glm::length(glm::dvec3(state.bodyPosition)-expected);
        }
        csv<<i<<','<<double(i+1)*dt<<','<<m.executed<<','<<(m.executed?m.lastFluidDeltaTime:0)<<','<<found->containedMass<<','<<state.liquidMass<<','<<state.retainedMass<<','<<state.bodyPosition.y<<','<<state.bodyVelocity.y<<','<<state.liquidCenter.y<<','<<found->containedImpulse.x<<','<<found->containedImpulse.y<<','<<found->containedImpulse.z<<','<<state.bodyMomentum.x<<','<<state.bodyMomentum.y<<','<<state.bodyMomentum.z<<','<<state.liquidMomentum.x<<','<<state.liquidMomentum.y<<','<<state.liquidMomentum.z<<','<<gravityImpulse.y<<','<<residual.x<<','<<residual.y<<','<<residual.z<<','<<energy<<','<<found->buoyancyForce.y<<','<<found->dragForce.y<<','<<m.particleMilliseconds<<','<<m.totalMilliseconds<<','<<m.containedStaticSupportImpulse.x<<','<<m.containedStaticSupportImpulse.y<<','<<m.containedStaticSupportImpulse.z<<','<<batch.iterations<<','<<batch.maximumClosingSpeed<<','<<batch.speedScale<<','<<batch.iterationCapReached<<','<<batch.rigidContactRows<<','<<preStep.bodyPosition.y<<','<<preStep.bodyVelocity.y<<','<<state.liquidMomentum.y/state.liquidMass<<','<<batch.persistentGapRows<<','<<batch.normalAccelerations<<','<<batch.normalMatrixProducts<<','<<batch.normalAccelerationBreakdowns<<'\n';
    }
    result.steps=count;
    result.load=loadTime>0?loadImpulse/loadTime:0;
    result.expectedLoad=gravity?initial.liquidMass*double(9.81f):0;
    result.loadRelativeError=gravity?std::abs(result.load-result.expectedLoad)/result.expectedLoad:0;
    Check((rawInitial?result.retained:result.synchronizedRetained)>=.99,
          result.name+" at least99percent finite liquid remains within resolved vessel at matching physical time");
    if(supported) {
        Check(result.loadRelativeError<=.10,result.name+" last2s contained contact load within10percent analytical liquid weight");
        const double initialPotential=initial.liquidMass*double(9.81f)*initial.liquidCenter.y+mass*double(9.81f)*.1;
        Check(result.maxEnergy<=2*initialPotential+10,result.name+" no explosive kinetic gain beyond generous available-settling-work budget");
    } else if(gravity) {
        Check((rawInitial?result.relativeDrift:result.synchronizedDrift)<=.20,
              result.name+" common-freefall liquid/container relative drift <=.20m in2s at matching physical time");
        Check(result.bodyDriftError<=.20,result.name+" container freefall endpoint within.20m independent kick-drift oracle");
        Check(result.maxMomentumResidual<=.03*totalMass*double(9.81f)*double(dt)*count,
              result.name+" common-freefall total momentum residual <=3percent gravity impulse");
        const double idealEnergy=.5*totalMass*std::pow(double(9.81f)*double(dt)*count,2);
        Check(result.maxEnergy<=2*idealEnergy+10,result.name+" common-freefall energy has no explosive growth");
    } else {
        Check(result.maxEnergy<=2*(.5*mass*.1*.1)+10,result.name+" zero-g impulse has no explosive energy creation");
        Check(glm::length(result.momentumResidual)<=.03*glm::length(externalImpulse)+.01,
              result.name+" zero-g total momentum residual <=3percent prescribed impulse");
    }
    result.particleMedianMs=Median(particleMs);result.couplingMedianMs=Median(couplingMs);
    result.runtime=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();result.failed=failures-beforeFailures;
    std::cout<<std::setprecision(17)<<"CASE "<<result.name<<" sealed="<<sealed<<" steps="<<count<<" failures="<<result.failed<<" liquid_mass="<<result.mass<<" load="<<result.load<<" expected_load="<<result.expectedLoad<<" load_error="<<result.loadRelativeError<<" retained="<<result.retained<<" relative_drift="<<result.relativeDrift<<" synchronized_drift="<<result.synchronizedDrift<<" body_drift_error="<<result.bodyDriftError<<" preparation_steps="<<result.preparationSteps<<" preparation_max_energy="<<result.preparationEnergyMax<<" preparation_final_energy="<<result.preparationFinalEnergy<<" preparation_relative_rms="<<result.preparationRelativeRmsSpeed<<" momentum_residual="<<glm::length(result.momentumResidual)<<" max_kinetic="<<result.maxEnergy<<" liquid_steps="<<world.FluidCoupling().Measurements().executedSteps<<" particle_ms="<<result.particleMedianMs<<" coupling_ms="<<result.couplingMedianMs<<" seconds="<<result.runtime<<" sum_contained_Iy="<<sumContained.y<<" omitted_exterior_Iy="<<sumExteriorNotApplied.y<<" analytic_Iy="<<sumAnalytic.y<<" batch_static_support_Iy="<<sumCouplingSupport.y<<" momentum_scope="<<(supported?"includes-unmeasured-ordinary-static-support":"external-gravity-and-prescribed-impulse")<<'\n';
    return result;
}
}
int main(int argc,char**argv) {
    fs::path output; bool supportedOnly=false, rawInitial=false, openZeroG=false;
    for(int i=1;i<argc;++i) {if(std::string(argv[i])=="--output"&&i+1<argc)output=argv[++i];else if(std::string(argv[i])=="--supported-only")supportedOnly=true;else if(std::string(argv[i])=="--raw-initial")rawInitial=true;else if(std::string(argv[i])=="--open-zero-g")openZeroG=true;else return 2;}
    if(output.empty()) {std::cerr<<"--output fresh persistent directory is required\n";return 2;}
    if(fs::exists(output)&&!fs::is_empty(output)) {std::cerr<<"refusing to overwrite evidence\n";return 2;}fs::create_directories(output);
    std::vector<Result> results;
    for(float spacing:{.15f,.20f}) {
        for(float mass:{200.f,2000.f})results.push_back(Run(output,"supported",spacing,mass,rawInitial,openZeroG));
        if(!supportedOnly) {results.push_back(Run(output,"freefall",spacing,200,rawInitial,openZeroG));results.push_back(Run(output,"zero-g-impulse",spacing,200,rawInitial,openZeroG));}
    }
    std::ofstream summary(output/"summary.csv");summary<<std::setprecision(17)<<"case,steps,failures,liquid_mass,tank_mass,measured_load,expected_load,load_error,retained_fraction,relative_drift,synchronized_drift,synchronized_retained,body_drift_error,preparation_steps,preparation_max_energy,preparation_final_energy,preparation_relative_rms,final_momentum_residual,max_momentum_residual,max_kinetic_J,particle_median_ms,coupling_median_ms,seconds\n";
    for(const auto& r:results)summary<<r.name<<','<<r.steps<<','<<r.failed<<','<<r.mass<<','<<r.tankMass<<','<<r.load<<','<<r.expectedLoad<<','<<r.loadRelativeError<<','<<r.retained<<','<<r.relativeDrift<<','<<r.synchronizedDrift<<','<<r.synchronizedRetained<<','<<r.bodyDriftError<<','<<r.preparationSteps<<','<<r.preparationEnergyMax<<','<<r.preparationFinalEnergy<<','<<r.preparationRelativeRmsSpeed<<','<<glm::length(r.momentumResidual)<<','<<r.maxMomentumResidual<<','<<r.maxEnergy<<','<<r.particleMedianMs<<','<<r.couplingMedianMs<<','<<r.runtime<<'\n';
    std::cout<<"FTFT9 CONTAINERS cases="<<results.size()<<" steps="<<totalSteps<<" checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
