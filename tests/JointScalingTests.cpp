// M87 ordered-membership and append semantics. The optional --benchmark DIR
// measures existing JointSolver preparation separately from ten unchanged solve
// rounds; no integration, gravity, contact or rendering claim is made here.
#include "JointSolver.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
constexpr float dt=1.f/60.f;
unsigned checks=0,failures=0;
void Check(bool value,const std::string& message){++checks;failures+=!value;std::cout<<(value?"PASS ":"FAIL ")<<message<<'\n';}
double Elapsed(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
double Quantile(std::vector<double> values,double q){std::sort(values.begin(),values.end());return values.empty()?0:values[std::size_t(q*double(values.size()-1))];}
Json Distribution(const std::vector<double>& v){return {{"median",Quantile(v,.5)},{"p95",Quantile(v,.95)},{"maximum",Quantile(v,1)}};}
void Mix(std::uint64_t& digest,float v){std::uint32_t bits;std::memcpy(&bits,&v,sizeof bits);for(unsigned shift=0;shift<32;shift+=8){digest^=(bits>>shift)&255u;digest*=1099511628211ull;}}
struct Fixture {
    std::vector<RigidBody> bodies;
    std::vector<JointState> states;
    std::vector<std::array<float,12>> warm;
    std::vector<JointInput> inputs;
    explicit Fixture(unsigned n):bodies(n+1),states(n),warm(n){
        for(unsigned i=0;i<n;++i){
            // Non-sorted address order must remain solver/input order. A common
            // immovable anchor also stresses duplicate body membership.
            const unsigned slot=n-i;
            auto& b=bodies[slot];b.inverseMass=1;b.inverseInertiaLocal=glm::mat3(1);
            b.position={float(i)*2,0,0};
            states[i].settings.type=JointType::Fixed;states[i].settings.anchorB=b.position;
            inputs.push_back({&b,&bodies[0],&states[i],&warm[i]});
        }
    }
};
struct Snapshot {
    std::vector<glm::vec3> linear,angular,reaction,torque;
    std::vector<std::array<float,12>> warm;
};
Snapshot Capture(const Fixture& f){Snapshot s;s.warm=f.warm;for(const auto& b:f.bodies){s.linear.push_back(b.linearVelocity);s.angular.push_back(b.angularVelocity);}for(const auto& j:f.states){s.reaction.push_back(j.reactionImpulse);s.torque.push_back(j.reactionAngularImpulse);}return s;}
bool Same(const Snapshot& a,const Snapshot& b){return a.linear==b.linear&&a.angular==b.angular&&a.reaction==b.reaction&&a.torque==b.torque&&a.warm==b.warm;}
void Membership(){
    Fixture f(5);JointSolver solver;
    const std::vector<JointInput> first{f.inputs[2],f.inputs[0],f.inputs[2]};
    solver.Prepare(first,dt,false);
    Check(solver.RowCount()==18,"non-append duplicate inputs retain their existing duplicate-row semantics");
    const auto& ordered=solver.Bodies();
    Check(ordered.size()==3&&ordered[0]==first[0].a&&ordered[1]==first[0].b&&ordered[2]==first[1].a,"body order follows first input encounter, not pointer/hash order");
    f.warm[4][0]=.05f;const auto snapshot=Capture(f);
    for(unsigned repeat=0;repeat<4;++repeat)solver.Prepare(first,dt,true,false,true);
    Check(solver.RowCount()==18&&Same(snapshot,Capture(f)),"repeated append duplicates preserve rows, velocities, reactions and warm histories exactly");
    solver.Prepare({f.inputs[4]},dt,true,false,true);
    Check(solver.RowCount()==24&&solver.Bodies().size()==4&&solver.Bodies().back()==f.inputs[4].a,"append adds only newly encountered participant and preserves existing body order");
    const auto appended=Capture(f);solver.Prepare({f.inputs[4],f.inputs[0]},dt,true,false,true);
    Check(solver.RowCount()==24&&Same(appended,Capture(f)),"append cannot apply a newly awakened joint warm start twice");
    solver.Prepare({},dt);
    Check(solver.Empty()&&solver.Bodies().empty(),"empty non-append boundary clears every membership domain");
    solver.Prepare(f.inputs,dt,false,false,true);
    Check(solver.RowCount()==30&&solver.Bodies().size()==6,"append after empty clear can prepare the same state/body pointer addresses again");
    solver.Prepare({f.inputs[3]},dt,false);
    Check(solver.RowCount()==6&&solver.Bodies().size()==2&&solver.Bodies()[0]==f.inputs[3].a,"ordinary next boundary discards preceding prepared/body membership");
    // A failed material still belonged to the original prepared-state set;
    // changing its status mid-boundary must not resurrect it through append.
    auto& material=f.states[1].constitutive;material.material.enabled=true;material.failed=true;
    solver.Prepare({f.inputs[1]},dt);
    Check(solver.Empty()&&solver.Bodies().size()==2,"failed material participant preserves body membership despite contributing no rows");
    material.failed=false;solver.Prepare({f.inputs[1]},dt,true,false,true);
    Check(solver.Empty(),"append respects a prepared no-row material participant");
    solver.Prepare({f.inputs[1]},dt,false);
    Check(solver.RowCount()==6,"new boundary can prepare that same material state after reset");
    material.material.enabled=false;
    for(auto& b:f.bodies){b.linearVelocity={};b.angularVelocity={};}
    f.warm[1][0]=.1f;const auto beforeImpact=Capture(f);
    solver.Prepare({f.inputs[1]},dt,false,true);
    Check(Same(beforeImpact,Capture(f)),"impact preparation retains no-warm-start and observation-retention semantics");
    solver.ResetImpulses();solver.SolveIteration();
    Check(std::isfinite(f.bodies[4].linearVelocity.x),"reset/impact solve remains finite after membership reuse");
}
void ScaleCorrectness(){
    for(unsigned count:{1u,31u,257u,2048u}){
        Fixture f(count);JointSolver solver;
        for(auto& b:f.bodies)if(!b.IsStatic()){b.linearVelocity={.5f,-.25f,.125f};b.angularVelocity={.1f,-.2f,.3f};}
        solver.Prepare(f.inputs,dt,false);
        bool order=solver.Bodies().size()==count+1&&solver.Bodies()[1]==&f.bodies[0];
        for(unsigned i=0;i<count&&order;++i)order=solver.Bodies()[i?i+1:0]==f.inputs[i].a;
        Check(order&&solver.RowCount()==6*count,"growing membership table preserves ordered independent rows at "+std::to_string(count));
        for(unsigned i=0;i<10;++i)solver.SolveIteration();
        double residual=0;for(const auto& b:f.bodies)residual=std::max(residual,double(glm::length(b.linearVelocity)+glm::length(b.angularVelocity)));
        Check(residual<1e-6,"all independent fixed participants solve, including hash-table growth boundary "+std::to_string(count));
        const auto after=Capture(f);solver.Prepare(f.inputs,dt,true,false,true);
        Check(solver.RowCount()==6*count&&Same(after,Capture(f)),"large repeated append neither adds nor warms duplicate rows "+std::to_string(count));
    }
}
int Benchmark(const std::filesystem::path& output){
    std::filesystem::create_directories(output);
    if(std::filesystem::exists(output/"summary.json"))throw std::runtime_error("existing joint benchmark summary is immutable");
    Json results=Json::array();std::ofstream csv(output/"samples.csv");csv<<std::setprecision(12)<<"joints,sample,measured,prepare_ms,solve10_ms,duplicate_append_ms\n";
    for(unsigned count:{128u,512u,2048u}){
        Fixture f(count);JointSolver solver;std::vector<double> prepare,solve,append;std::uint64_t digest=1469598103934665603ull;bool okay=true;
        for(unsigned sample=0;sample<200;++sample){
            for(auto& b:f.bodies)if(!b.IsStatic()){b.linearVelocity={.5f,-.25f,.125f};b.angularVelocity={.1f,-.2f,.3f};}
            auto start=Clock::now();solver.Prepare(f.inputs,dt);const auto prepareMs=Elapsed(start);
            start=Clock::now();for(unsigned i=0;i<10;++i)solver.SolveIteration();const auto solveMs=Elapsed(start);
            start=Clock::now();solver.Prepare(f.inputs,dt,true,false,true);const auto appendMs=Elapsed(start);
            if(sample>=100){prepare.push_back(prepareMs);solve.push_back(solveMs);append.push_back(appendMs);}
            csv<<count<<','<<sample<<','<<(sample>=100)<<','<<prepareMs<<','<<solveMs<<','<<appendMs<<'\n';
            okay&=solver.RowCount()==count*6&&solver.Bodies().size()==count+1;
            for(const auto& b:f.bodies)for(float value:{b.linearVelocity.x,b.linearVelocity.y,b.linearVelocity.z,b.angularVelocity.x,b.angularVelocity.y,b.angularVelocity.z}){okay&=std::isfinite(value)&&std::abs(value)<1e-6f;Mix(digest,value);}
            for(const auto& row:f.warm)for(float value:row)Mix(digest,value);
        }
        results.push_back({{"joints",count},{"bodies",count+1},{"warm_samples",100},{"measured_samples",100},{"prepare_ms",Distribution(prepare)},{"solve10_ms",Distribution(solve)},{"duplicate_append_ms",Distribution(append)},{"physical_pass",okay},{"physical_digest",digest}});
        std::cout<<results.back().dump()<<'\n';
    }
    std::ofstream(output/"summary.json")<<std::setw(2)<<results<<'\n';
    bool pass=true;for(const auto& result:results)pass&=result["physical_pass"].get<bool>();return pass?0:1;
}
}
int main(int argc,char** argv){try{if(argc==3&&std::string(argv[1])=="--benchmark")return Benchmark(argv[2]);if(argc!=1)throw std::invalid_argument("usage: judas_joint_scaling_tests [--benchmark OUTPUT]");Membership();ScaleCorrectness();std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}}
