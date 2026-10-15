// Isolated M87 experiment: exact frozen ImpactSolver inputs, no scene or physics
// budget edits. Iteration alternatives intentionally alter solve mathematics.
#include "FrozenImpact.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
extern "C" int JudasM87ConfigureImpactDiagnostic(int,const char*,unsigned);
extern "C" void JudasM87ImpactDiagnosticStep(unsigned);
extern "C" void JudasM87ConfigureImpactCapture(const char*,unsigned,unsigned);
struct Instance {
 std::vector<RigidBody> bodies;
 std::vector<ImpactContact> contacts;
 explicit Instance(const M87Frozen::Snapshot& s){
  bodies.reserve(s.before.size());for(const auto& b:s.before)bodies.push_back(M87Frozen::Unpack(b));
  contacts.reserve(s.contacts.size());for(const auto& c:s.contacts)contacts.push_back(M87Frozen::Unpack(c,bodies));
 }
};
struct Difference {
 std::size_t bodyWords=0,rowWords=0,rowCount=0;
 double maximumLinearDifference=0,maximumAngularDifference=0;
};
Difference Compare(const Instance& instance,const ImpactSolver& solver,const M87Frozen::Snapshot& s){
 Difference d;
 for(std::size_t i=0;i<instance.bodies.size();++i){
  const auto actual=M87Frozen::Pack(instance.bodies[i]);
  for(std::size_t j=0;j<actual.size();++j)d.bodyWords+=actual[j]!=s.after.at(i)[j];
  const auto golden=M87Frozen::Unpack(s.after.at(i));
  d.maximumLinearDifference=std::max(d.maximumLinearDifference,glm::length(glm::dvec3(instance.bodies[i].linearVelocity)-glm::dvec3(golden.linearVelocity)));
  d.maximumAngularDifference=std::max(d.maximumAngularDifference,glm::length(glm::dvec3(instance.bodies[i].angularVelocity)-glm::dvec3(golden.angularVelocity)));
 }
 d.rowCount=solver.Constraints().size();
 if(d.rowCount!=s.rows.size())d.rowWords=std::max(d.rowCount,s.rows.size())*6;
 else for(std::size_t i=0;i<d.rowCount;++i){const auto actual=M87Frozen::Pack(solver.Constraints()[i]);for(std::size_t j=0;j<actual.size();++j)d.rowWords+=actual[j]!=s.rows[i][j];}
 return d;
}
int main(int argc,char** argv){try{
 if(argc<3){std::cerr<<"usage: replay FROZEN_INPUT OUTPUT_DIR [repeats=100] [iterations=0:all] [csv=1]\n";return 2;}
 const auto frozen=M87Frozen::Read(argv[1]);const std::string output=argv[2];
 const unsigned repeats=argc>3?unsigned(std::stoul(argv[3])):100;const int only=argc>4?std::stoi(argv[4]):0;const bool csv=argc>5?std::stoi(argv[5])!=0:true;
 if(repeats<1||repeats>1000)throw std::runtime_error("repeats out of bounded range [1,1000]");
 std::filesystem::create_directories(output);JudasM87ConfigureImpactCapture(nullptr,0,0);JudasM87ImpactDiagnosticStep(frozen.step);
 std::ofstream table(output+"/replay.csv");table<<std::setprecision(17);
 table<<"iterations,captured_iterations,bodies,dynamic_bodies,contacts,rows,accepted,body_words_different,row_words_different,max_linear_difference_mps,max_angular_difference_radps,kinetic_before_j,kinetic_after_j,median_solve_ms,p95_solve_ms,max_solve_ms,repeats\n";
 bool matched=false;std::size_t dynamics=0;for(const auto& b:frozen.before)dynamics+=!M87Frozen::Unpack(b).IsStatic();
 for(const int it:{10,20,40,80}){if(only&&only!=it)continue;
  const std::string residual=output+"/residual-"+std::to_string(it)+".csv";
  if(!JudasM87ConfigureImpactDiagnostic(it,csv?residual.c_str():nullptr,0))throw std::runtime_error("diagnostic configuration failed");
  Instance actual(frozen);ImpactSolver solver;const auto result=solver.Solve(actual.contacts,M87Frozen::Float(frozen.dt),frozen.forceInelastic);const auto difference=Compare(actual,solver,frozen);
  if(it==frozen.iterations){matched=true;if(difference.bodyWords||difference.rowWords)throw std::runtime_error("captured-iteration replay differs from recorded physical endpoint or impulse rows");}
  JudasM87ConfigureImpactDiagnostic(it,nullptr,0);
  // Warm reusable solver storage once; resetting exact inputs is outside timer.
  {Instance warm(frozen);solver.Solve(warm.contacts,M87Frozen::Float(frozen.dt),frozen.forceInelastic);}
  std::vector<double> times;times.reserve(repeats);bool allAccepted=true;
  for(unsigned repeat=0;repeat<repeats;++repeat){
   Instance repeatInput(frozen);const auto begin=std::chrono::steady_clock::now();
   const auto answer=solver.Solve(repeatInput.contacts,M87Frozen::Float(frozen.dt),frozen.forceInelastic);
   const auto end=std::chrono::steady_clock::now();times.push_back(std::chrono::duration<double,std::milli>(end-begin).count());allAccepted=allAccepted&&answer.energyBudgetSatisfied;
   if(it==frozen.iterations){const auto equal=Compare(repeatInput,solver,frozen);if(equal.bodyWords||equal.rowWords)throw std::runtime_error("repeated captured-iteration replay differs");}
  }
  std::ofstream samples(output+"/times-"+std::to_string(it)+".csv");samples<<std::setprecision(17)<<"repeat,solve_ms\n";for(unsigned i=0;i<times.size();++i)samples<<i<<','<<times[i]<<'\n';
  std::sort(times.begin(),times.end());const auto percentile=[&](double p){return times[std::min(std::size_t(std::ceil(p*times.size()))-1,times.size()-1)];};
  table<<it<<','<<frozen.iterations<<','<<frozen.before.size()<<','<<dynamics<<','<<frozen.contacts.size()<<','<<solver.Constraints().size()<<','<<(result.energyBudgetSatisfied&&allAccepted)<<','<<difference.bodyWords<<','<<difference.rowWords<<','<<difference.maximumLinearDifference<<','<<difference.maximumAngularDifference<<','<<result.kineticBefore<<','<<result.kineticAfter<<','<<percentile(.5)<<','<<percentile(.95)<<','<<times.back()<<','<<repeats<<'\n';table.flush();
  std::cout<<"iterations="<<it<<" accepted="<<result.energyBudgetSatisfied<<" body_words_different="<<difference.bodyWords<<" row_words_different="<<difference.rowWords<<" median_ms="<<percentile(.5)<<" p95_ms="<<percentile(.95)<<" max_ms="<<times.back()<<'\n';
 }
 if(!only&&!matched)throw std::runtime_error("captured iteration count unsupported");
 std::cout<<"Frozen solve replay complete. Captured-iteration endpoint and row bits matched; alternatives intentionally change the numerical solve.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
