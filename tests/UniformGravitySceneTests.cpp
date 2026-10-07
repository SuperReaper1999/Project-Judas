// FTFT3: authored files -> actual scene parser/RuntimeWorld -> ordinary fixed step.
// Oracles are generated separately with Decimal(80) scalar matrix arithmetic.
// No expected value calls GLM rotation, the factory, or GravityField::Sample.
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "GameSession.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "Window.h"
#include "WorldCoordinates.h"

namespace fs = std::filesystem;
namespace {
using Vec = std::array<long double, 3>;
int checks = 0, failures = 0;
void Check(bool ok, const std::string& label) {
    ++checks;
    if (!ok) { ++failures; std::cout << "FAIL " << label << '\n'; }
}
long double Gamma(int operations) {
    const long double u = std::numeric_limits<float>::epsilon() / 2.0L;
    return operations * u / (1 - operations * u);
}
long double Length(const Vec& v) { return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); }
Vec Convert(const glm::vec3& v) { return {v.x, v.y, v.z}; }
bool Equal(const Vec& a, const Vec& b) { return a == b; }
bool Within(const Vec& a, const Vec& b, const Vec& tolerance) {
    for (int k = 0; k < 3; ++k)
        if (!std::isfinite(a[k]) || std::abs(a[k]-b[k]) > tolerance[k]) return false;
    return true;
}
void WriteVec(std::ostream& out, const Vec& v) { for (auto x : v) out << '\t' << x; }
struct Fixture {
    std::string id, scene, group, sign, placement;
    float magnitude = 0;
    std::array<float,4> q{};
    Vec expected{}, scale{}, intended{};
};
std::vector<std::string> Split(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream input(line);
    std::string field;
    while (std::getline(input, field, '\t')) fields.push_back(field);
    return fields;
}
std::vector<Fixture> ReadFixtures(const fs::path& directory) {
    std::ifstream input(directory / "manifest.tsv");
    if (!input) throw std::runtime_error("missing independent fixture manifest");
    std::string line;
    std::getline(input, line);
    const auto header = Split(line);
    std::map<std::string,std::size_t> column;
    for (std::size_t i=0; i<header.size(); ++i) column[header[i]]=i;
    std::vector<Fixture> result;
    while (std::getline(input,line)) {
        if (line.empty()) continue;
        const auto fields = Split(line);
        const auto get = [&](const std::string& name) -> const std::string& { return fields.at(column.at(name)); };
        Fixture f;
        f.id=get("id"); f.scene=get("scene"); f.group=get("origin_group");
        f.sign=get("sign"); f.placement=get("placement"); f.magnitude=std::stof(get("magnitude"));
        for (int k=0; k<4; ++k) f.q[k]=std::stof(get(std::array<std::string,4>{"w","x","y","z"}[k]));
        for (int k=0; k<3; ++k) {
            const auto axis=std::array<std::string,3>{"x","y","z"}[k];
            f.expected[k]=std::stold(get("expected_"+axis));
            f.scale[k]=std::stold(get("scale_"+axis));
            f.intended[k]=std::stold(get("intended_"+axis));
        }
        result.push_back(f);
    }
    return result;
}
struct Outcome { Vec acceleration{}, velocity{}; };
Outcome Exercise(const Fixture& f, const Scene& scene, const std::string& path,
                 std::ofstream& samples, std::ofstream& velocities) {
    const int initialFailures=failures;
    const SceneObject* region=nullptr;
    for (const auto& object:scene.Objects()) if (object.gravity) region=&object;
    Check(region && region->gravity->kind==SceneGravityKind::Uniform, f.id+" uniform region loaded");
    if (!region) throw std::runtime_error("fixture lacks gravity region");
    const auto& q=region->transform.rotation;
    Check(region->gravity->magnitude==f.magnitude && q.w==f.q[0] && q.x==f.q[1] && q.y==f.q[2] && q.z==f.q[3],
          f.id+" parsed float inputs exactly match independent manifest");
    const glm::dvec3 origin=f.placement=="near" ? glm::dvec3(0) : glm::dvec3(1e9,-2e9,3e9);
    Check(scene.Settings().worldOrigin==origin, f.id+" authored double origin loaded");
    Vec tolerance;
    // One quaternion normalization and its rotation/product arithmetic have
    // fewer than 64 elementary rounding/error propagation terms for these
    // finite, well-scaled inputs. Condition each component on its contributing
    // products; a global |g| allowance would conceal ignored tiny rotations.
    // This is a conservative engineering bound, not a general GLM error proof.
    const long double underflow=8.0L*std::numeric_limits<float>::denorm_min();
    for(int k=0;k<3;++k) tolerance[k]=Gamma(64)*f.scale[k]+underflow;
    RuntimeWorld world;
    std::string error;
    if(!world.Build(scene,nullptr,error)) throw std::runtime_error("Build: "+error);
    Check(world.DynamicBodies().size()==1 && world.StaticBodies().empty() && !world.HasFluid() &&
          world.CelestialParticipants().empty() && !world.GetVehicle(), f.id+" ordinary isolated rigid scene");
    const WorldCoordinates coordinates(scene.Settings().worldOrigin);
    const std::array<glm::vec3,6> positions={glm::vec3(0),glm::vec3(3,4,5),glm::vec3(-40,6,17),
        glm::vec3(120,-80,10),glm::vec3(-100,100,-99),glm::vec3(255,-0.125f,1)};
    Outcome result;
    for(std::size_t i=0;i<positions.size();++i) {
        const auto local=coordinates.ToLocal(coordinates.ToGlobal(positions[i]));
        Check(local==positions[i],f.id+" fixed-origin boundary preserves represented sample point");
        const Vec observed=Convert(world.Gravity().Sample(local));
        Check(Within(observed,f.expected,tolerance),f.id+" "+path+" sample vector "+std::to_string(i));
        const long double magnitude=std::abs(static_cast<long double>(f.magnitude));
        const long double normTolerance=Length(tolerance);
        Check(std::abs(Length(observed)-magnitude)<=normTolerance,f.id+" magnitude "+std::to_string(i));
        if (magnitude>normTolerance) {
            Vec unit, expectedUnit, unitTolerance;
            for(int k=0;k<3;++k) {
                unit[k]=observed[k]/Length(observed); expectedUnit[k]=f.expected[k]/magnitude;
                unitTolerance[k]=tolerance[k]/(magnitude-normTolerance)+
                    std::abs(f.expected[k])*normTolerance/(magnitude*(magnitude-normTolerance));
            }
            Check(Within(unit,expectedUnit,unitTolerance),f.id+" direction "+std::to_string(i));
        } else Check(observed==Vec{},f.id+" zero gravity is exact");
        if(f.q[1]==0 && f.q[2]==0 && f.q[3]==0)
            Check(observed==Vec{0,-static_cast<long double>(f.magnitude),0},f.id+" identity magnitude is not quantized (exact represented value)");
        if(f.magnitude==0) Check(observed==Vec{},f.id+" zero field remains exact at every position");
        samples<<f.id<<'\t'<<path<<'\t'<<i;WriteVec(samples,Convert(local));WriteVec(samples,f.intended);
        WriteVec(samples,f.expected);WriteVec(samples,observed);WriteVec(samples,tolerance);
        samples<<'\t'<<magnitude<<'\t'<<Length(observed)<<'\n';
        if(i==0) result.acceleration=observed;
        else Check(Equal(observed,result.acceleration),f.id+" uniform field is position-independent");
    }
    Check(world.Gravity().Sample(glm::vec3(600,600,600))==glm::vec3(0),f.id+" unclaimed space remains zero");
    GameSession session;
    if(!session.Begin(world,error)) throw std::runtime_error("Begin: "+error);
    Window window;
    window.SetTestInputMode(true); // only held-input acquisition; same StepPlayedWorld.
    const auto handle=world.DynamicBodies().front().Handle();
    Check(world.Physics().GetMass(handle)==37.0f,f.id+" authored body mass retained");
    Check(world.Physics().GetLinearVelocity(handle)==glm::vec3(0),f.id+" body starts at rest");
    std::size_t contacts=0;
    for(int n=1;n<=120;++n) {
        StepPlayedWorld(session,window,SimulationTiming::kFixedTimestep);
        contacts+=world.Physics().LastStepContactCount();
        if(n!=1 && n!=10 && n!=60 && n!=120) continue;
        const long double time=n*static_cast<long double>(SimulationTiming::kFixedTimestep);
        Vec expected, velocityTolerance;
        for(int k=0;k<3;++k) {
            expected[k]=time*f.expected[k];
            velocityTolerance[k]=time*tolerance[k]+Gamma(n+4)*time*(std::abs(f.expected[k])+tolerance[k])+underflow;
        }
        const Vec observed=Convert(world.Physics().GetLinearVelocity(handle));
        Check(Within(observed,expected,velocityTolerance),f.id+" "+path+" free-body velocity step "+std::to_string(n));
        velocities<<f.id<<'\t'<<path<<'\t'<<n<<'\t'<<time;WriteVec(velocities,expected);
        WriteVec(velocities,observed);WriteVec(velocities,velocityTolerance);velocities<<'\n';
        result.velocity=observed;
    }
    Check(contacts==0,f.id+" no contacts contaminated free-flight result");
    Check(world.Physics().GetAngularVelocity(handle)==glm::vec3(0),f.id+" no unrelated angular force");
    std::cout<<"CASE "<<f.id<<' '<<path<<' '<<(failures==initialFailures?"PASS":"FAIL")<<" expected=";
    WriteVec(std::cout,f.expected);std::cout<<" observed=";WriteVec(std::cout,result.acceleration);
    std::cout<<" velocity120=";WriteVec(std::cout,result.velocity);std::cout<<'\n';
    return result;
}
}
int main(int argc,char** argv) {
    fs::path directory="docs/evidence/ftft3/fixtures", output="build/ftft3-construction";
    std::string prefix;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(i+1>=argc) return 2;
        if(arg=="--fixtures")directory=argv[++i];
        else if(arg=="--output")output=argv[++i];
        else if(arg=="--case")prefix=argv[++i];
        else return 2;
    }
    fs::create_directories(output/"roundtrip");
    std::ofstream samples(output/"samples.tsv"),velocities(output/"velocities.tsv");
    samples<<std::setprecision(21);velocities<<std::setprecision(21);std::cout<<std::setprecision(15);
    samples<<"id\tpath\tsample\tpx\tpy\tpz\tintended_x\tintended_y\tintended_z\texpected_x\texpected_y\texpected_z\tobserved_x\tobserved_y\tobserved_z\ttolerance_x\ttolerance_y\ttolerance_z\tmagnitude_expected\tmagnitude_observed\n";
    velocities<<"id\tpath\tstep\ttime\texpected_x\texpected_y\texpected_z\tobserved_x\tobserved_y\tobserved_z\ttolerance_x\ttolerance_y\ttolerance_z\n";
    int scenes=0;
    std::map<std::string,Outcome> equivalent;
    try {
        for(const auto& f:ReadFixtures(directory)) {
            if(!prefix.empty() && f.id.rfind(prefix,0)!=0) continue;
            Scene scene;
            std::string error;
            if(!LoadSceneFromFile((directory/f.scene).string(),scene,error)) throw std::runtime_error(error);
            ++scenes;
            const auto first=Exercise(f,scene,"original",samples,velocities);
            const auto file=output/"roundtrip"/f.scene;
            if(!SaveSceneToFile(scene,file.string(),error)) throw std::runtime_error(error);
            Scene reloaded;
            if(!LoadSceneFromFile(file.string(),reloaded,error)) throw std::runtime_error(error);
            const auto second=Exercise(f,reloaded,"roundtrip",samples,velocities);
            Check(Equal(first.acceleration,second.acceleration)&&Equal(first.velocity,second.velocity),f.id+" save/load preserves vector and trajectory exactly");
            auto [it,inserted]=equivalent.emplace(f.group,first);
            if(!inserted) Check(Equal(first.acceleration,it->second.acceleration)&&Equal(first.velocity,it->second.velocity),
                               f.id+" q/-q and near/far produce exactly equivalent local physics");
        }
    } catch(const std::exception& e) { Check(false,std::string("fixture exception: ")+e.what()); }
    Check(scenes>0,"at least one scene was exercised");
    std::ofstream result(output/"results.json");
    result<<"{\n  \"scenes\": "<<scenes<<",\n  \"runtime_constructions\": "<<scenes*2
          <<",\n  \"fixed_steps\": "<<scenes*240<<",\n  \"checks\": "<<checks<<",\n  \"failures\": "<<failures<<"\n}\n";
    std::cout<<"UniformGravityScene: "<<(failures?"FAIL":"PASS")<<" scenes="<<scenes<<" runtime_constructions="<<scenes*2
             <<" steps="<<scenes*240<<" checks="<<checks<<" failures="<<failures<<'\n';
    return failures?1:0;
}
