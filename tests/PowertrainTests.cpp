#include "Powertrain.h"
#include "SaveArchive.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace {
int checks=0,failures=0;
void Check(bool pass,const char* label){++checks;failures+=!pass;std::printf("%s %s\n",pass?"PASS":"FAIL",label);}
bool Near(double a,double b,double tolerance=1e-9){return std::abs(a-b)<=tolerance*std::max({1.0,std::abs(a),std::abs(b)});}
PowertrainSettings Shafts(unsigned count=2,double inertia=1){PowertrainSettings settings;for(unsigned i=0;i<count;++i){PowertrainShaft shaft;shaft.name="shaft"+std::to_string(i);shaft.inertia=inertia;settings.shafts.push_back(shaft);}return settings;}
struct Rig {
    PowertrainInstance instance;
    std::vector<double> velocity,w;
    std::vector<bool> available;
    explicit Rig(PowertrainSettings settings):velocity(settings.shafts.size()),w(velocity.size()*velocity.size(),0),available(velocity.size(),true){instance.Reset(settings);for(unsigned i=0;i<velocity.size();++i){velocity[i]=settings.shafts[i].initialAngularVelocity;w[i*velocity.size()+i]=1/settings.shafts[i].inertia;}}
    std::vector<double> Step(double dt=.1,bool sources=true){auto impulse=instance.Step(dt,velocity,w,available,sources);for(unsigned i=0;i<velocity.size();++i)for(unsigned j=0;j<velocity.size();++j)velocity[i]+=w[i*velocity.size()+j]*impulse[j];return impulse;}
};
PowertrainSource Custom(unsigned shaft=0,double capacity=100){PowertrainSource source;source.name="custom";source.shaft=shaft;source.maxTorque=capacity;return source;}
PowertrainLink Gear(double ratio=1){PowertrainLink link;link.name="gear";link.ratio=ratio;return link;}
}
int main(){
    std::string error;auto settings=Shafts();
    Check(ValidatePowertrainSettings(settings,error),"valid independent scalar mechanical assembly");
    auto invalid=settings;invalid.shafts[1].name=invalid.shafts[0].name;
    Check(!ValidatePowertrainSettings(invalid,error),"duplicate shaft identity rejected");
    invalid=settings;invalid.shafts[0].inertia=0;Check(!ValidatePowertrainSettings(invalid,error),"zero rotor inertia rejected");
    invalid=settings;invalid.links={Gear(0)};Check(!ValidatePowertrainSettings(invalid,error),"zero signed gear ratio rejected");
    invalid=settings;invalid.links={Gear()};invalid.links[0].output=5;Check(!ValidatePowertrainSettings(invalid,error),"missing shaft reference rejected");
    invalid=settings;invalid.shafts[0].kind=PowertrainShaftKind::Wheel;invalid.shafts[1].kind=PowertrainShaftKind::Wheel;invalid.shafts[0].target=invalid.shafts[1].target=42;
    Check(!ValidatePowertrainSettings(invalid,error),"duplicate external wheel terminal rejected");
    invalid=settings;invalid.shafts[0].kind=PowertrainShaftKind::Wheel;invalid.shafts[0].target=42;invalid.shafts[0].bearing=43;
    Check(!ValidatePowertrainSettings(invalid,error),"wheel terminal rejects an ignored explicit bearing instead of implying an effect");
    invalid=settings;invalid.shafts[0].axis={0,0,0};Check(!ValidatePowertrainSettings(invalid,error),"degenerate physical axis rejected");
    settings=Shafts(1,2);settings.sources={Custom()};Rig free(settings);free.instance.command.sources[0].customTorque=12;free.Step();
    Check(Near(free.velocity[0],.6)&&Near(free.instance.state.sourceTorque[0],12),"free rotor obeys I domega = torque dt");
    free.instance.AdvanceAngles(.1);Check(Near(free.instance.state.shafts[0].angle,.06),"angle advances from resolved physical spin only when requested");
    auto heavier=settings;heavier.shafts[0].inertia=4;Rig heavy(heavier);heavy.instance.command.sources[0].customTorque=12;heavy.Step();
    Check(Near(heavy.velocity[0],.3),"rotational inertia changes actual acceleration");
    const auto firstSpin=free.velocity[0];free.Step(.1,false);
    Check(Near(free.velocity[0],firstSpin)&&Near(free.instance.state.sourceTorque[0],12)&&Near(free.instance.state.shafts[0].torque,12),"post-load projection retains source telemetry without integrating source twice");
    free.instance.command.sources[0].customTorque=101;const auto beforeRejected=free.velocity[0];const auto rejected=free.Step();
    Check(Near(rejected[0],0)&&Near(free.velocity[0],beforeRejected)&&!free.instance.state.converged&&Near(free.instance.state.sourceTorque[0],0),"over-cap custom command is transactional and reports no applied source work");
    free.instance.command.sources[0].customTorque=12;free.Step(.1,false);
    Check(!free.instance.state.converged&&Near(free.velocity[0],beforeRejected),"failed pre-step remains failed through the same interval post projection");
    free.Step();Check(free.instance.state.converged&&free.velocity[0]>beforeRejected,"new interval can recover after invalid command is corrected");
    settings=Shafts(1,2);PowertrainSource engine;engine.name="combustion";engine.kind=PowertrainSourceKind::Combustion;engine.maxTorque=200;engine.starterTorque=36;
    engine.torqueCurve={{0,0},{1200,100},{2400,140},{6000,0}};settings.sources={engine};
    settings.shafts[0].initialAngularVelocity=1800*(2*3.14159265358979323846)/60;Rig combustion(settings);combustion.instance.command.sources[0].input=.5;combustion.Step();
    Check(Near(combustion.instance.state.sourceTorque[0],60)&&Near(combustion.velocity[0],settings.shafts[0].initialAngularVelocity+3),"combustion torque interpolates authored RPM curve at actual shaft speed");
    settings.shafts[0].initialAngularVelocity=0;Rig stalled(settings);stalled.instance.command.sources[0].input=1;stalled.Step();
    Check(stalled.velocity[0]==0,"zero-speed combustion source does not invent idle rotation");
    stalled.instance.command.sources[0].starter=1;stalled.Step();Check(Near(stalled.velocity[0],1.8)&&Near(stalled.instance.state.sourceTorque[0],36),"starter is authored physical torque on rotor inertia");
    auto malformed=settings;malformed.sources[0].torqueCurve[2].rpm=1000;
    Check(!ValidatePowertrainSettings(malformed,error),"unordered engine torque samples rejected");
    settings=Shafts(1,2);PowertrainSource electric;electric.name="electric";electric.kind=PowertrainSourceKind::Electric;electric.maxTorque=100;electric.maxPower=10000;settings.sources={electric};
    Rig electricLow(settings);electricLow.instance.command.sources[0].input=1;electricLow.Step();
    settings.shafts[0].initialAngularVelocity=200;Rig electricHigh(settings);electricHigh.instance.command.sources[0].input=1;electricHigh.Step();
    Check(Near(electricLow.velocity[0],5)&&Near(electricLow.instance.state.sourceTorque[0],100)&&Near(electricHigh.instance.state.sourceTorque[0],50),"electric source has real stall torque and high-speed power-limited torque");
    electricLow.instance.command.sources[0].input=-1;electricLow.Step();Check(Near(electricLow.velocity[0],0),"signed electric control can apply opposing physical torque");
    settings=Shafts(1,2);settings.shafts[0].initialAngularVelocity=10;settings.shafts[0].viscousDrag=2;Rig drag(settings);drag.Step(1);
    Check(Near(drag.velocity[0],5)&&Near(drag.instance.state.kineticEnergy,25),"implicit bearing drag removes energy without crossing zero");
    drag.instance.settings.shafts[0].viscousDrag=100000;drag.Step(1);Check(drag.velocity[0]>0&&drag.velocity[0]<.001,"large physical bearing drag remains passive on a light rotor");
    settings=Shafts();settings.shafts[0].inertia=2;settings.shafts[1].inertia=8;settings.shafts[0].initialAngularVelocity=10;settings.links={Gear(.5)};Rig reduction(settings);const auto reductionImpulse=reduction.Step();
    Check(Near(reduction.velocity[0],5)&&Near(reduction.velocity[1],2.5)&&reduction.instance.state.residual<1e-9,"ideal reduction solves reflected load inertia exactly");
    Check(Near(reductionImpulse[0],-10)&&Near(reductionImpulse[1],20)&&Near(reductionImpulse[0]*reduction.velocity[0]+reductionImpulse[1]*reduction.velocity[1],0),"gear impulses respect ratio and zero virtual power");
    Check(Near(reduction.instance.state.constraintEnergyChange,-50)&&Near(reduction.instance.state.kineticEnergy,50),"initial gear engagement cannot create kinetic energy");
    settings.shafts[0].initialAngularVelocity=0;settings.sources={Custom()};Rig load(settings);load.instance.command.sources[0].customTorque=12;load.Step();
    Check(Near(load.velocity[0],.3)&&Near(load.velocity[1],.15),"connected load slows source through reflected inertia rather than scripted RPM");
    settings.shafts[0].initialAngularVelocity=10;settings.sources.clear();settings.links[0].ratio=-.5;Rig reverse(settings);reverse.Step();
    Check(Near(reverse.velocity[0],5)&&Near(reverse.velocity[1],-2.5),"signed gear reverses output rotation with physical reaction torque");
    settings=Shafts(3,2);PowertrainLink diff;diff.name="open";diff.kind=PowertrainLinkKind::Differential;diff.input=0;diff.output=1;diff.outputB=2;settings.links={diff};settings.sources={Custom()};settings.brakes={{"left brake",1,100}};
    Rig differential(settings);differential.instance.command.sources[0].customTorque=20;differential.instance.command.brakes[0].engagement=1;differential.Step();
    Check(Near(differential.velocity[0],.2)&&Near(differential.velocity[1],0)&&Near(differential.velocity[2],.4),"open differential permits unequal actual output speeds under unequal load");
    Check(Near(differential.instance.state.shafts[2].torque,8)&&Near(differential.instance.state.links[0].torque,8)&&Near(differential.instance.state.brakeTorque[0],-8),"open differential supplies equal output impulses and loaded side receives real brake reaction");
    settings=Shafts();settings.shafts[0].initialAngularVelocity=10;auto clutch=Gear();clutch.kind=PowertrainLinkKind::Clutch;clutch.maxTorque=2;settings.links={clutch};Rig slipping(settings);slipping.Step(.5);
    Check(Near(slipping.velocity[0],9)&&Near(slipping.velocity[1],1)&&Near(slipping.instance.state.links[0].torque,2)&&slipping.instance.state.links[0].saturated,"clutch capacity bounds physical transfer while retaining real slip");
    const auto beforePost=slipping.velocity;slipping.velocity[1]=0;slipping.Step(.5,false);
    Check(Near(slipping.velocity[0],beforePost[0])&&Near(slipping.velocity[1],0)&&Near(slipping.instance.state.links[0].torque,2)&&Near(slipping.instance.state.links[0].slip,-9)&&slipping.instance.state.links[0].active,"post-contact coupling cannot spend the clutch torque budget twice");
    Rig halfClutch(settings);halfClutch.instance.command.links[0].engagement=.5;halfClutch.Step(.5);
    Check(Near(halfClutch.velocity[0],9.5)&&Near(halfClutch.velocity[1],.5),"engagement fraction scales coupling capacity without faking shaft speed");
    Rig openClutch(settings);openClutch.instance.command.links[0].engagement=0;openClutch.Step(.5);
    Check(Near(openClutch.velocity[0],10)&&Near(openClutch.velocity[1],0)&&!openClutch.instance.state.links[0].active,"disengaged clutch disconnects drive mechanically");
    settings.links[0].maxTorque=100;Rig lockClutch(settings);lockClutch.Step(.5);
    Check(Near(lockClutch.velocity[0],5)&&Near(lockClutch.velocity[1],5)&&!lockClutch.instance.state.links[0].saturated,"sufficient clutch capacity synchronizes shafts using real inertia");
    settings=Shafts(1,2);settings.shafts[0].initialAngularVelocity=2;settings.brakes={{"disc",0,5}};Rig brake(settings);brake.instance.command.brakes[0].engagement=1;brake.Step();
    Check(Near(brake.velocity[0],1.75)&&Near(brake.instance.state.brakeTorque[0],-5),"disc brake torque obeys finite angular impulse bound");
    const double brakeSpin=brake.velocity[0];brake.Step(.1,false);Check(Near(brake.velocity[0],brakeSpin)&&Near(brake.instance.state.brakeTorque[0],-5),"brake budget is shared across pre/post phases");
    settings.brakes[0].maxTorque=100;Rig stop(settings);stop.instance.command.brakes[0].engagement=1;stop.Step();Check(Near(stop.velocity[0],0),"sufficient mechanical braking stops without artificial reversal");
    settings=Shafts(1);settings.shafts[0].initialAngularVelocity=10;settings.brakes={{"a",0,20},{"b",0,20}};Rig redundantBrake(settings);for(auto& control:redundantBrake.instance.command.brakes)control.engagement=1;redundantBrake.Step();
    Check(redundantBrake.instance.state.converged&&Near(redundantBrake.velocity[0],6)&&Near(redundantBrake.instance.state.brakeTorque[0],-20)&&Near(redundantBrake.instance.state.brakeTorque[1],-20),"rank-dependent physical brakes combine capacities without a numerical spring");
    settings=Shafts();settings.shafts[0].initialAngularVelocity=10;settings.links={Gear(),Gear()};settings.links[1].name="redundant gear";Rig redundantGear(settings);redundantGear.Step();
    Check(redundantGear.instance.state.converged&&Near(redundantGear.velocity[0],5)&&Near(redundantGear.velocity[1],5),"consistent redundant ideal links converge without budget inflation");
    settings=Shafts(20);settings.shafts[0].initialAngularVelocity=10;double reflected=0;
    for(unsigned i=0;i<20;++i){reflected+=std::pow(.8,2*int(i));if(i){auto link=Gear(.8);link.name="chain"+std::to_string(i);link.input=i-1;link.output=i;settings.links.push_back(link);}}
    Rig chain(settings);chain.Step();bool chainCorrect=chain.instance.state.converged;for(unsigned i=0;i<20;++i)chainCorrect&=Near(chain.velocity[i],10*std::pow(.8,int(i))/reflected,1e-8);
    Check(chainCorrect&&chain.instance.state.residual<1e-8,"long gear train uses exact dense resolution rather than sequential gear drift");
    settings=Shafts();settings.sources={Custom()};Rig shared(settings);shared.w={1.5,.5,.5,1.5};shared.instance.command.sources[0].customTorque=4;shared.Step();
    Check(Near(shared.velocity[0],.6)&&Near(shared.velocity[1],.2),"shared bearing inverse-mass cross terms transmit housing reaction");
    shared.w={1,2,2,1};const auto sharedBefore=shared.velocity;const auto invalidMass=shared.Step();
    Check(!shared.instance.state.converged&&invalidMass==std::vector<double>({0,0})&&shared.velocity==sharedBefore&&shared.instance.state.sourceTorque[0]==0,"indefinite physical inverse mass fails transactionally before source application");
    shared.w={1,0,0,1};shared.Step(.1,false);Check(!shared.instance.state.converged&&shared.velocity==sharedBefore,"corrected matrix cannot conceal the already-failed source interval");
    shared.Step();Check(shared.instance.state.converged,"next source interval retries a repaired physical configuration normally");
    settings=Shafts();settings.links={Gear()};settings.sources={Custom()};Rig lost(settings);lost.available[1]=false;lost.w={1,0,0,0};lost.instance.command.sources[0].customTorque=4;lost.Step();
    Check(Near(lost.velocity[0],.4)&&!lost.instance.state.links[0].active&&!lost.instance.state.shafts[1].available,"missing external DOF disconnects incident links without freezing unrelated source");
    settings=Shafts(1);settings.enabled=false;settings.sources={Custom()};Rig disabled(settings);disabled.instance.command.sources[0].customTorque=40;disabled.Step();disabled.instance.AdvanceAngles(.1);
    Check(disabled.velocity[0]==0&&disabled.instance.state.shafts[0].angle==0&&disabled.instance.state.sourceTorque[0]==0,"disabled assembly applies no torque and advances no physical angle");
    settings=Shafts();settings.sources={Custom()};settings.links={Gear(.5)};settings.brakes={{"brake",1,15}};Rig saved(settings);saved.instance.command.sources[0].customTorque=20;saved.Step();saved.instance.AdvanceAngles(.1);saved.instance.absoluteSpin={2.5,-1.25};saved.instance.internalInitialized={true,true};
    SaveArchive archive;saved.instance.Persist(archive);
    auto staleObservation=saved.instance;staleObservation.state.shafts[0].angularVelocity=987654;SaveArchive cacheArchive;staleObservation.Persist(cacheArchive);
    Check(cacheArchive.bytes==archive.bytes,"stale published relative velocity cannot change the durable mechanical payload");
    staleObservation.absoluteSpin[0]+=.25;SaveArchive momentumArchive;staleObservation.Persist(momentumArchive);
    Check(momentumArchive.bytes!=archive.bytes,"actual rotor momentum changes the durable payload independently of observation caches");
    PowertrainInstance restored;restored.Reset(settings);restored.state.shafts[0].angularVelocity=-123456;SaveArchive reader(archive.bytes);restored.Persist(reader);reader.Finish();
    Check(restored.absoluteSpin==saved.instance.absoluteSpin&&restored.internalInitialized==saved.instance.internalInitialized&&restored.command.sources[0].customTorque==20&&Near(restored.state.shafts[0].angle,saved.instance.state.shafts[0].angle)&&!restored.state.available&&restored.state.shafts[0].angularVelocity==0,"save stores durable actual rotor state and commands and invalidates derived observations");
    const auto nextA=saved.instance.Step(.1,saved.velocity,saved.w,saved.available),nextB=restored.Step(.1,saved.velocity,saved.w,saved.available);
    Check(nextA==nextB&&saved.instance.state.sourceTorque==restored.state.sourceTorque,"new-format continuation reproduces exact cold constraint projection");
    bool truncated=false;try{PowertrainInstance broken;broken.Reset(settings);SaveArchive shortArchive(archive.bytes.substr(0,archive.bytes.size()-1));broken.Persist(shortArchive);}catch(const std::runtime_error&){truncated=true;}
    Check(truncated,"truncated mechanical save fails cleanly");
    settings=Shafts(1);settings.sources={Custom(0,1e200)};Rig overflow(settings);overflow.instance.command.sources[0].customTorque=1e200;const auto overflowImpulse=overflow.Step();
    Check(!overflow.instance.state.converged&&overflowImpulse[0]==0&&overflow.velocity[0]==0&&overflow.instance.state.sourceTorque[0]==0,"finite but unrepresentable energy fails before publishing any physical work");
    // A representative 7-DOF source/clutch/transmission/open-diff assembly.
    // These are core solve timings; runtime contact/render cost is measured
    // separately by the ordinary lab and wheel regressions.
    settings=Shafts(7);settings.sources={Custom(0,200)};auto friction=Gear();friction.name="clutch";friction.kind=PowertrainLinkKind::Clutch;friction.maxTorque=150;settings.links={friction};
    for(unsigned i=2;i<5;++i){auto link=Gear(i==2?.25:1);link.name="gear"+std::to_string(i);link.input=i-1;link.output=i;settings.links.push_back(link);}
    diff.input=4;diff.output=5;diff.outputB=6;settings.links.push_back(diff);settings.brakes={{"left",5,100},{"right",6,100}};
    for(unsigned count:{1u,10u,100u}){std::vector<Rig> many;for(unsigned i=0;i<count;++i){many.emplace_back(settings);many.back().instance.command.sources[0].customTorque=40;}
        std::vector<double> times;
        for(unsigned step=0;step<240;++step){const auto start=std::chrono::steady_clock::now();for(auto& assembly:many){assembly.Step(1.0/60);assembly.Step(1.0/60,false);assembly.instance.AdvanceAngles(1.0/60);}const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(step>=40)times.push_back(elapsed);}
        std::sort(times.begin(),times.end());std::printf("PERF %u 7-DOF assemblies pre+post p50 %.6f ms p95 %.6f ms max %.6f ms\n",count,times[times.size()/2],times[size_t(.95*times.size())],times.back());
        Check(std::all_of(many.begin(),many.end(),[](const Rig& r){return r.instance.state.converged&&r.instance.state.residual<1e-7;}),"representative repeated bounded mechanical workload converges");
    }
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
