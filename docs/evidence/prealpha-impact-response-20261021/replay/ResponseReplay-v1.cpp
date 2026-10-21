#include "CaseIO.h"
#include <iostream>
#include <set>
#include <cstring>
#include <cmath>
#include <memory>
struct Replay {
 std::vector<RigidBody> bodies;
 std::vector<ImpactContact> rows;
 Replay(const DiagnosisJSON& j, int mode) {
  for(auto& b:j["bodies"])bodies.push_back(RBody(b));
  std::set<unsigned> seed;for(auto& i:j["seed_rows"])seed.insert(i.get<unsigned>());
  for(unsigned i=0;i<j["rows"].size();++i){if((mode==1&&seed.count(i))||(mode==2&&!seed.count(i)))continue;auto& r=j["rows"][i];rows.push_back({&bodies[r["a"].get<unsigned>()],&bodies[r["b"].get<unsigned>()],RContact(r["g"]),r["friction"].get<float>(),r["restitution"].get<float>(),r["persistent"].get<bool>()});}
 }
};
bool same(glm::vec3 a,glm::vec3 b){return std::memcmp(&a,&b,sizeof(a))==0;}
bool same(const RigidBody& a,const RigidBody& b){return same(a.linearVelocity,b.linearVelocity)&&same(a.angularVelocity,b.angularVelocity);}
double norm(const RigidBody& a,const RigidBody& b){auto v=glm::dvec3(a.linearVelocity)-glm::dvec3(b.linearVelocity),w=glm::dvec3(a.angularVelocity)-glm::dvec3(b.angularVelocity);return sqrt(glm::dot(v,v)+glm::dot(w,w));}
DiagnosisJSON response(const std::vector<RigidBody>& before,const std::vector<RigidBody>& after,const std::set<unsigned>& seed){unsigned changed=0,nonseed=0;double l1=0,max=0;for(unsigned i=0;i<before.size();++i){if(!same(before[i],after[i])){++changed;nonseed+=!seed.count(i);}double n=norm(before[i],after[i]);l1+=n;max=std::max(max,n);}return {{"changed",changed},{"nonseed_changed",nonseed},{"sum_mixed_velocity_norm",l1},{"max_mixed_velocity_norm",max}};}
int main(int argc,char**argv){DiagnosisJSON output=DiagnosisJSON::array();int failures=0;for(int f=1;f<argc;++f){DiagnosisJSON j;std::ifstream(argv[f])>>j;Replay full(j,0),without(j,1),only(j,2),zero(j,0);auto incoming=full.bodies;ImpactSolver solver;auto a=solver.Solve(full.rows,j["dt"],j["capture"]);auto b=solver.Solve(without.rows,j["dt"],j["capture"]);auto c=solver.Solve(only.rows,j["dt"],j["capture"]);for(auto& body:zero.bodies)body.linearVelocity=body.angularVelocity=glm::vec3(0);auto zeroBefore=zero.bodies;auto z=solver.Solve(zero.rows,j["dt"],j["capture"]);unsigned actualMismatch=0,sameWithout=0,changedSameWithout=0;std::set<unsigned> seedBodies;for(auto&i:j["seed_rows"]){auto&r=j["rows"][i.get<unsigned>()];seedBodies.insert(r["a"].get<unsigned>());seedBodies.insert(r["b"].get<unsigned>());}for(unsigned i=0;i<full.bodies.size();++i){actualMismatch+=!same(full.bodies[i],RBody(j["actual_after"][i]));if(same(full.bodies[i],without.bodies[i])){++sameWithout;changedSameWithout+=!same(incoming[i],full.bodies[i]);}}auto secondBefore=full.bodies;auto second=solver.Solve(full.rows,j["dt"],j["capture"]);DiagnosisJSON r={{"event",j["event"]},{"rows",j["rows"].size()},{"bodies",incoming.size()},{"seed_rows",j["seed_rows"]},{"actual_mismatch",actualMismatch},{"full",response(incoming,secondBefore,seedBodies)},{"without_seed",response(incoming,without.bodies,seedBodies)},{"only_seed",response(incoming,only.bodies,seedBodies)},{"zero_velocity_control",response(zeroBefore,zero.bodies,seedBodies)},{"second_projection_no_new_impact",response(secondBefore,full.bodies,seedBodies)},{"identical_without_seed",sameWithout},{"changed_identically_without_seed",changedSameWithout},{"energy_guards",{a.energyBudgetSatisfied,b.energyBudgetSatisfied,c.energyBudgetSatisfied,z.energyBudgetSatisfied,second.energyBudgetSatisfied}}};failures+=actualMismatch;output.push_back(r);}std::cout<<output.dump(2)<<'\n';return failures?1:0;}
