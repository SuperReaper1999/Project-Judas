// Authored RuntimeWorld registration and public MK2/combustion integration.
// The fixture owns an explicit external heater; analytical material budgets
// independently check the real thermal solver, without built-in gameplay.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <glm/gtc/quaternion.hpp>
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "Window.h"
namespace {
namespace fs=std::filesystem;
constexpr float dt=SimulationTiming::kFixedTimestep;
constexpr double sigma=5.670374419e-8,pi=3.14159265358979323846,heatRelease=1.6e7;
int checks=0,failures=0,steps=0;
struct Frame {std::string name;glm::quat rotation{1,0,0,0};glm::vec3 shift{0};glm::dvec3 origin{0};float yaw=0;glm::vec3 Point(glm::vec3 x)const{return shift+rotation*x;}};
struct Result {std::string name;int count=0,failed=0,ignition=-1,extinction=-1;double initialT=0,finalT=0,initialFuel=0,finalFuel=0,chemical=0,heater=0,environment=0,gas=0,storageResidual=0,chemicalResidual=0,initialHeater=0,firstT=0,oxygen=0;};
std::vector<Result> results;
void Check(bool ok,const std::string& label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
struct Played {RuntimeWorld world;Window window;Scene scene;RadiantHeater heater;bool powered=false;
    bool Begin(const Scene& authored,const fs::path& output,const std::string& name){std::string text,error;bool ok=SaveSceneToString(authored,text);Check(ok,name+" serialization");if(!ok)return false;
        std::ofstream(output/(name+".judas"))<<text;ok=LoadSceneFromString(text,scene,error);Check(ok,name+" load: "+error);if(!ok)return false;
        window.SetTestInputMode(true);ok=world.Build(scene,nullptr,error)&&world.IsBuilt();Check(ok,name+" runtime/session: "+error);return ok;}
    void Heater(bool on){powered=on;heater.powerWatts=on?18000:0;}
    void Step(){world.Physics().Step(dt);world.Combustion().Step(dt,world.Physics(),world.GetAtmosphere()?&world.GetAtmosphere()->field:nullptr,world.GetAtmosphere()?world.GetAtmosphere()->frame:ReferenceFrame{},powered?&heater:nullptr);++steps;}
};
Scene Make(const Frame& frame,bool atmosphere,float temperature,float oxygen,const SceneCombustibleComponent& material,glm::vec3 velocity,bool nearHeater){
    Scene scene;scene.Settings().name="FTFT8 thermal "+frame.name;scene.Settings().worldOrigin=frame.origin;
    const glm::vec3 center=atmosphere?glm::vec3(80,0,0):glm::vec3(0);
    auto& body=scene.CreateObject("Finite coating");body.transform.position=frame.Point(center);body.transform.rotation=frame.rotation;
    body.body=SceneBodyComponent{};body.body->motion=SceneBodyMotion::Dynamic;body.body->shape=SceneShape::Sphere;body.body->radius=.2f;body.body->mass=25;body.body->friction=0;body.body->restitution=0;body.body->initialLinearVelocity=frame.rotation*velocity;body.combustible=material;
    if(atmosphere){auto& gas=scene.CreateObject("Prescribed reservoir");gas.transform.position=frame.Point({0,0,0});gas.transform.rotation=frame.rotation;gas.body=SceneBodyComponent{};gas.body->shape=SceneShape::Sphere;gas.body->radius=1;gas.celestial=SceneCelestialComponent{};gas.celestial->gravitationalParameter=62784;gas.atmosphere=SceneAtmosphereComponent{};gas.atmosphere->referenceDensity=.2f;gas.atmosphere->referenceTemperatureKelvin=temperature;gas.atmosphere->oxidizerMassFraction=oxygen;}
    return scene;
}
Result Run(const Frame& frame,const std::string& family,const fs::path& output,bool atmosphere,float referenceTemperature,float oxygen,SceneCombustibleComponent material,int heaterSteps,int count,glm::vec3 velocity={0,0,0},bool nearHeater=false){
    Result r;r.name=family+"-"+frame.name;const int failedBefore=failures;Played p;
    if(!p.Begin(Make(frame,atmosphere,referenceTemperature,oxygen,material,velocity,nearHeater),output,r.name))return r;
    p.heater.worldPosition=frame.Point((atmosphere?glm::vec3(80,0,0):glm::vec3(0))+glm::vec3(0,0,nearHeater?.25f:1.f));
    const auto* entity=p.world.FindEntity(p.scene.Objects()[0].id);const BodyHandle h=p.world.DynamicBodies()[entity->slot].Handle();auto& physics=p.world.Physics();
    const glm::vec3 v=physics.GetLinearVelocity(h),spin=frame.rotation*glm::vec3(.1f,-.2f,.3f);physics.SetAngularVelocity(h,spin);
    const auto* initial=p.world.Combustion().State(h);Check(initial!=nullptr,r.name+" authored thermal registration");if(!initial)return r;
    r.initialT=initial->temperatureKelvin;r.initialFuel=initial->remainingFuelMassKg;
    Check(p.world.Combustibles().size()==1&&p.world.Combustion().States().size()==1,r.name+" one ordinary thermal participant");
    const double initialExpected=atmosphere?referenceTemperature:300.;Check(std::abs(r.initialT-initialExpected)<.001,r.name+" authored reservoir initializes temperature");
    Check(r.initialFuel==double(material.initialFuelMassKg),r.name+" serialized finite fuel maps exactly");
    std::ofstream csv(output/(r.name+".csv"));csv<<std::setprecision(17)<<"step,x,y,z,temperature_before,temperature_after,fuel_before,fuel_after,burn_rate,chemical_W,gas_W,heater_W,pair_W,environment_W,oxygen_supply,oxidizer_density,storage_residual_J,chemical_residual_J\n";
    for(int i=0;i<count;++i){const auto before=*p.world.Combustion().State(h);p.Heater(i<heaterSteps);Check(p.powered==(i<heaterSteps),r.name+" fixture explicitly owns external heater power");p.Step();const auto after=*p.world.Combustion().State(h);const auto position=physics.GetTransform(h).position;
        Check(std::isfinite(after.temperatureKelvin)&&after.temperatureKelvin>0&&after.remainingFuelMassKg>=0&&after.remainingFuelMassKg<=before.remainingFuelMassKg,r.name+" finite state/fuel monotonically consumed");
        Check(physics.GetLinearVelocity(h)==v&&physics.GetAngularVelocity(h)==spin,r.name+" thermal path cannot add mechanical impulse");
        Check(physics.LastStepContactCount()==0,r.name+" collision-free carrier");
        const double burnt=before.remainingFuelMassKg-after.remainingFuelMassKg;
        const double chemical=burnt*heatRelease,reported=double(after.heatOutputWatts)*double(dt);
        const double chemicalResidual=reported-chemical;const double chemicalTolerance=4.*std::numeric_limits<float>::epsilon()*std::max(1.,chemical);
        Check(std::abs(chemicalResidual)<=chemicalTolerance,r.name+" each reported chemical joule has actual fuel decrement");
        const double input=chemical+(double(after.heaterWattsReceived)+double(after.pairwiseHeatWattsReceived)+double(after.environmentalHeatWattsReceived)-double(after.heatToGasWatts))*double(dt);
        const double stored=double(material.heatCapacityJPerK)*(double(after.temperatureKelvin)-double(before.temperatureKelvin));
        const double residual=stored-input;
        const double tolerance=4.*std::numeric_limits<float>::epsilon()*double(material.heatCapacityJPerK)*std::max(double(before.temperatureKelvin),double(after.temperatureKelvin))+8.*std::numeric_limits<float>::epsilon()*(std::abs(input)+1.);
        Check(std::abs(residual)<=tolerance,r.name+" material storage closes after reservoir/heater/product exchange");
        Check(3.*burnt<=double(after.oxygenSupplyKgPerSecond)*double(dt)+1e-10,r.name+" fuel requires supplied oxidizer");
        Check(after.burnRateKgPerSecond<=material.maximumFuelRateKgPerSecond+5e-9f,r.name+" authored maximum reaction rate retained");
        Check(after.pairwiseHeatWattsReceived==0,r.name+" one-body fixture has no invented pair heat");
        Check(after.heaterWattsReceived>=0&&after.heaterWattsReceived<=18000,r.name+" absorbed heater power bounded by actual input");
        if(i>=heaterSteps)Check(after.heaterWattsReceived==0,r.name+" releasing input removes external heater immediately");
        if(after.burnRateKgPerSecond>0&&r.ignition<0)r.ignition=i;
        if(r.ignition>=0&&after.burnRateKgPerSecond==0&&r.extinction<0)r.extinction=i;
        if(i==0){r.initialHeater=after.heaterWattsReceived;r.firstT=after.temperatureKelvin;r.oxygen=after.localOxidizerMassDensity;}
        r.chemical+=chemical;r.heater+=double(after.heaterWattsReceived)*double(dt);r.environment+=double(after.environmentalHeatWattsReceived)*double(dt);r.gas+=double(after.heatToGasWatts)*double(dt);
        r.storageResidual=std::max(r.storageResidual,std::abs(residual));r.chemicalResidual=std::max(r.chemicalResidual,std::abs(chemicalResidual));
        csv<<i<<','<<position.x<<','<<position.y<<','<<position.z<<','<<before.temperatureKelvin<<','<<after.temperatureKelvin<<','<<before.remainingFuelMassKg<<','<<after.remainingFuelMassKg<<','<<after.burnRateKgPerSecond<<','<<after.heatOutputWatts<<','<<after.heatToGasWatts<<','<<after.heaterWattsReceived<<','<<after.pairwiseHeatWattsReceived<<','<<after.environmentalHeatWattsReceived<<','<<after.oxygenSupplyKgPerSecond<<','<<after.localOxidizerMassDensity<<','<<residual<<','<<chemicalResidual<<'\n';
    }
    const auto final=*p.world.Combustion().State(h);r.count=count;r.finalT=final.temperatureKelvin;r.finalFuel=final.remainingFuelMassKg;
    if(family=="vacuum-cooling"||family=="vacuum-heater"){
        // Independent one-step Stefan-Boltzmann plus isotropic interception.
        // The heater is exactly one metre away for the reference fixture.
        const double heater=family=="vacuum-heater"?18000.*double(material.radiativeAreaSquareMeters)/(4.*pi):0.;
        const double radiation=-double(.8f)*sigma*double(material.radiativeAreaSquareMeters)*std::pow(300.,4);
        const double expected=300.+(heater+radiation)*double(dt)/double(material.heatCapacityJPerK);
        Check(std::abs(r.firstT-expected)<5e-5,r.name+" independent vacuum/heater first-step temperature");
        Check(std::abs(r.initialHeater-heater)<.002,r.name+" independent isotropic heater interception");
        Check(r.finalFuel==r.initialFuel&&r.chemical==0&&r.oxygen==0,r.name+" vacuum supplies no oxidizer even when hot/heated");
    }
    if(family=="finite-fuel"){
        Check(r.ignition==0&&r.extinction>=0&&r.finalFuel==0,r.name+" warm finite coating burns out and remains exhausted");
        Check(std::abs(r.chemical-r.initialFuel*heatRelease)<1e-8,r.name+" total chemical energy equals complete fuel inventory");
        Check(std::abs(r.gas-r.chemical)<.003,r.name+" zero retention routes released heat to prescribed reservoir");
        Check(std::abs(r.finalT-r.initialT)<.002,r.name+" fixed warm reservoir plus zero retention leaves thermal equilibrium");
        const double expectedOxygen=double(.2f)*double(oxygen);Check(std::abs(r.oxygen-expectedOxygen)<1e-6,r.name+" reservoir composition maps to sampled oxygen");
    }
    if(family=="no-oxygen"||family=="cold")Check(r.finalFuel==r.initialFuel&&r.chemical==0&&r.ignition<0,r.name+" absent oxygen/temperature prevents reaction");
    if(family=="heater-extinction"){
        Check(r.ignition>0&&r.ignition<heaterSteps,r.name+" real operator heat crosses ignition and starts reaction");
        Check(r.extinction>=heaterSteps&&r.finalT<material.ignitionTemperatureK&&r.finalFuel>0&&final.burnRateKgPerSecond==0,r.name+" heater release cools below ignition and extinguishes with fuel remaining");
    }
    if(family=="travel-vacuum"){
        Check(r.ignition==0&&r.finalFuel>0&&r.finalFuel<r.initialFuel,r.name+" freely moving carrier begins burning with finite fuel");
        Check(final.localOxidizerMassDensity==0&&final.oxygenSupplyKgPerSecond==0&&final.burnRateKgPerSecond==0,r.name+" ordinary travel exits atmospheric reservoir and extinguishes");
        Check(r.extinction>=0,r.name+" reaction eventually stops during outward travel");
    }
    r.failed=failures-failedBefore;std::cout<<"CASE "<<r.name<<" steps="<<r.count<<" failures="<<r.failed<<" T="<<r.initialT<<"->"<<r.finalT<<" fuel="<<r.initialFuel<<"->"<<r.finalFuel<<" ignition="<<r.ignition<<" extinction="<<r.extinction<<'\n';return r;
}
}
int main(int argc,char**argv){const auto start=std::chrono::steady_clock::now();fs::path output;bool vacuumOnly=false;
    for(int i=1;i<argc;++i){if(std::string(argv[i])=="--output"&&i+1<argc)output=argv[++i];else if(std::string(argv[i])=="--vacuum-only")vacuumOnly=true;else{std::cerr<<"usage: judas_combustion_integration_tests [--output fresh-directory] [--vacuum-only]\n";return 2;}}
    if(output.empty())output=fs::temp_directory_path()/("judas-ftft8-integration-"+std::to_string(start.time_since_epoch().count()));
    if(fs::exists(output)&&!fs::is_empty(output)){std::cerr<<"Refusing to overwrite thermal evidence\n";return 2;}fs::create_directories(output);
    const Frame identity{"identity",glm::quat(1,0,0,0),glm::vec3(0),glm::dvec3(0),0};
    SceneCombustibleComponent base;base.heatCapacityJPerK=150;base.initialFuelMassKg=.01f;base.ignitionTemperatureK=550;base.maximumFuelRateKgPerSecond=.003f;base.radiativeAreaSquareMeters=.5f;base.retainedCombustionHeatFraction=0;
    for(const std::string family:{"vacuum-cooling","vacuum-heater"})results.push_back(Run(identity,family,output,false,300,0,base,family=="vacuum-heater"?1:0,1));
    if(!vacuumOnly){const std::vector<Frame> frames{identity,{"rotated",glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3))),glm::vec3(4,-3,7),glm::dvec3(0),0},{"fixed-far",glm::quat(1,0,0,0),glm::vec3(0),glm::dvec3(1e12,-2e12,3e12),0}};
        for(const auto& frame:frames){auto fuel=base;fuel.initialFuelMassKg=.001f;fuel.radiativeAreaSquareMeters=1.5f;
            results.push_back(Run(frame,"finite-fuel",output,true,650,.21f,fuel,0,90));
            results.push_back(Run(frame,"no-oxygen",output,true,650,0,base,0,90));
            results.push_back(Run(frame,"cold",output,true,300,.21f,base,0,90));
            auto travel=base;travel.initialFuelMassKg=.1f;travel.radiativeAreaSquareMeters=1.5f;
            results.push_back(Run(frame,"travel-vacuum",output,true,650,.21f,travel,0,360,{6,0,0}));
        }
        auto heater=base;heater.heatCapacityJPerK=20;heater.initialFuelMassKg=.02f;
        for(const auto& frame:std::vector<Frame>{identity,{"yaw-translated",glm::angleAxis(.7f,glm::vec3(0,1,0)),glm::vec3(4,-3,7),glm::dvec3(0),glm::degrees(.7f)}})
            results.push_back(Run(frame,"heater-extinction",output,true,300,.21f,heater,80,400,{0,0,0},true));
        // Transform comparisons use independently generated scenes, not shared
        // state. Thermal laws depend only on scalar radius/distance/relativeflow.
        for(const std::string family:{"finite-fuel","no-oxygen","cold","travel-vacuum"}){const Result* reference=nullptr;for(const auto& r:results)if(r.name.rfind(family+"-",0)==0){if(!reference)reference=&r;else{Check(std::abs(r.finalT-reference->finalT)<.02,family+" rotated/fixed-origin temperature equivalence");Check(std::abs(r.finalFuel-reference->finalFuel)<2e-7,family+" rotated/fixed-origin finite fuel equivalence");}}}
    }
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::ostringstream json;json<<std::setprecision(17)<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"fixed_steps\":"<<steps<<",\"seconds\":"<<seconds<<",\"pass\":"<<(failures?"false":"true")<<",\"fixtures\":[";
    for(std::size_t i=0;i<results.size();++i){if(i)json<<',';const auto&r=results[i];json<<"{\"name\":\""<<r.name<<"\",\"steps\":"<<r.count<<",\"failures\":"<<r.failed<<",\"initial_temperature_K\":"<<r.initialT<<",\"final_temperature_K\":"<<r.finalT<<",\"initial_fuel_kg\":"<<r.initialFuel<<",\"final_fuel_kg\":"<<r.finalFuel<<",\"ignition_step\":"<<r.ignition<<",\"extinction_step\":"<<r.extinction<<",\"chemical_J\":"<<r.chemical<<",\"heater_J\":"<<r.heater<<",\"environment_J\":"<<r.environment<<",\"gas_J\":"<<r.gas<<",\"max_storage_residual_J\":"<<r.storageResidual<<",\"max_chemical_residual_J\":"<<r.chemicalResidual<<'}';}
    json<<"]}\n";std::ofstream(output/"results.json")<<json.str();std::cout<<"SUMMARY fixtures="<<results.size()<<" checks="<<checks<<" failures="<<failures<<" fixed_steps="<<steps<<" seconds="<<seconds<<'\n';return failures?1:0;
}
