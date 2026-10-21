#include "CaseIO.h"
#include <iostream>
#include <cstring>
#include <algorithm>
struct State {
 std::vector<RigidBody> bodies;std::vector<ImpactContact> rows;
 State(const DiagnosisJSON& j,bool continuing){
  for(auto& b:j["bodies"])bodies.push_back(RBody(b));
  for(unsigned i=0;i<j["rows"].size();++i){auto& r=j["rows"][i];
   rows.push_back({&bodies[r["a"].get<unsigned>()],&bodies[r["b"].get<unsigned>()],RContact(r["g"]),r["friction"].get<float>(),r["restitution"].get<float>(),r["persistent"].get<bool>()});
   const auto key=j["row_keys"][i];
   rows.back().continuing=continuing&&key!=j["seed"]&&std::find(j["support_pairs"].begin(),j["support_pairs"].end(),key)!=j["support_pairs"].end();
   rows.back().maxDefectDisplacement=j["row_allowance"][i];
  }
 }
};
bool same(glm::vec3 a,glm::vec3 b){return !std::memcmp(&a,&b,sizeof(a));}
int main(int argc,char**argv){DiagnosisJSON out=DiagnosisJSON::array();unsigned failures=0;
 for(int i=1;i<argc;++i){DiagnosisJSON j;std::ifstream(argv[i])>>j;State legacy(j,false),current(j,true),negative(j,false);ImpactSolver a,b,c;ImpactResult result,control;
  const auto old=a.Solve(legacy.rows,j["dt"],j["capture"]);const auto nop=[](const auto&){return true;};
  const bool accepted=b.SolveGrowing(current.rows,j["dt"],nop,result),rejectedControl=c.SolveGrowing(negative.rows,j["dt"],nop,control);
  unsigned mismatch=0,oldChanged=0,newChanged=0,controlChanged=0;
  for(unsigned k=0;k<legacy.bodies.size();++k){const auto before=RBody(j["bodies"][k]),expected=RBody(j["actual_after"][k]);
   auto equal=[](const RigidBody&l,const RigidBody&r){return same(l.linearVelocity,r.linearVelocity)&&same(l.angularVelocity,r.angularVelocity);};
   mismatch+=!equal(legacy.bodies[k],expected);oldChanged+=!equal(legacy.bodies[k],before);newChanged+=!equal(current.bodies[k],before);controlChanged+=!equal(negative.bodies[k],before);
  }
  failures += mismatch;
  if(!old.energyBudgetSatisfied) ++failures;
  if(!accepted || !result.energyBudgetSatisfied) ++failures;
  if(j["event"]==28 && newChanged!=0) ++failures;
  if(j["event"]==18 && newChanged!=3) ++failures;
  if(controlChanged!=oldChanged) ++failures;
  out.push_back({{"event",j["event"]},{"legacy_actual_mismatches",mismatch},{"old_changed",oldChanged},{"new_changed",newChanged},{"new_accepted",accepted},{"new_energy_ok",result.energyBudgetSatisfied},{"negative_no_support_changed",controlChanged},{"negative_accepted",rejectedControl}});
 }
 std::cout<<out.dump(2)<<'\n';return failures?1:0;}
