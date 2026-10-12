#include <vector>
#include <array>
#include <algorithm>
#include <limits>
#include <iostream>
#include <iomanip>
#include "RigidBody.h"
#include "JointTypes.h"
#define private public
#include "JointSolver.h"
#undef private
int main(){
 for(bool warm:{false,true}){
 RigidBody a,b;a.inverseMass=1;a.inverseInertiaLocal=glm::mat3(1);a.orientation=glm::angleAxis(.9f,glm::vec3(1,0,0));a.angularVelocity={10,0,0};
 JointState state;auto& s=state.settings;s.type=JointType::Hinge;s.limits=true;s.lower=-1;s.upper=1;s.anchorA={0,0,.5f};s.anchorB=a.orientation*s.anchorA;
 std::array<float,12> history{};history[1]=-20;
 JointSolver solver;solver.Prepare({{&a,&b,&state,&history}},1.f/60,warm);
 std::cout<<std::setprecision(9)<<"warm="<<warm<<" rows="<<solver.m_rows.size()<<" coordinate="<<state.coordinate<<" angular_x="<<a.angularVelocity.x<<'\n';
 for(size_t i=0;i<solver.m_rows.size();++i){auto& row=solver.m_rows[i];std::cout<<"row "<<i<<" bounds="<<row.low<<","<<row.high<<" bias="<<row.bias<<" mass="<<row.mass<<" lambda="<<row.impulse<<'\n';}
 }
}
