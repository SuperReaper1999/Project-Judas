#include "WaterCellAudit.h"
#include <cmath>
#include <iostream>
using namespace judas::volume_grid;
namespace {
using Real=CutCompensated;int checks=0,failures=0;
void Check(bool ok,const char* why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}}
std::vector<WaterCellPatch> Cube(){std::vector<WaterCellPatch> faces;for(int a=0;a<3;++a)for(int s:{-1,1}){GridVec r{};r[a]=s*.5;faces.push_back({a,s,1.,r,false});}return faces;}
}
int main(){
 const double rho=1000.,dt=1./60.;std::string error;GridVec gravity{1.2,-9.81,.7},source{.3,-.2,.1};auto faces=Cube();WaterCellProjection op;
 Check(PrepareWaterMimeticCell(1.,rho,dt,gravity,faces,op,error,source),"prepare source momentum in the compatible mass metric");
 std::vector<Real> q(op.predictor.begin(),op.predictor.end()),pressure(op.free.size());
 for(std::size_t i=0;i<op.free.size();++i){auto f=faces[op.free[i]];Check(std::abs(q[i].Value()-f.sign*source[f.axis])<1e-14,"unconstrained constant source velocity is reproduced");}
 // All exterior pressures zero: a free liquid patch accelerates exactly by g.
 for(std::size_t i=0;i<op.free.size();++i)for(std::size_t j=0;j<op.free.size();++j)q[i]=q[i]+Real(dt*op.mobility[i*op.free.size()+j]*op.gravityHead[j]);
 WaterCellAudit audit;Check(AuditWaterCell(1.,rho,dt,gravity,faces,op,q,pressure,audit,error),"independent momentum and work audit accepts free acceleration");
 for(int a=0;a<3;++a)Check(std::abs(audit.meanVelocity[a]-source[a]-dt*gravity[a])<1e-13,"mean velocity advances by physical gravity without gas inertia");
 Check(std::abs(audit.workResidual)<1e-10&&audit.implicitLoss>0.,"backward-Euler work includes its measured nonnegative temporal loss");
 auto saved=audit;q[0]=q[0]+Real(.01);
 Check(!AuditWaterCell(1.,rho,dt,gravity,faces,op,q,pressure,audit,error)&&audit.afterEnergy==saved.afterEnergy,"a corrupted flux fails physical audit transactionally");
 faces=Cube();for(auto& f:faces)f.fixed=!(f.axis==1&&f.sign==1);
 Check(PrepareWaterMimeticCell(1.,rho,dt,{0.,-9.81,0.},faces,op,error),"contained liquid column prepares");
 Check(AuditWaterCell(1.,rho,dt,{0.,-9.81,0.},faces,op,{Real(0.)},{Real(0.)},audit,error),"static column wall-pressure audit passes");
 Check(std::abs(audit.wallReaction[1]+9810.)<1e-9&&std::abs(audit.meanVelocity[1])<1e-14,"hydrostatic wall reaction equals liquid weight");
 // A real boundary sliver must remain represented, even far below one ULP.
 faces=Cube();auto f=faces[0];faces[0].area=1.-1e-24;f.area=1e-24;f.offset[1]=.5;faces.push_back(f);
 Check(PrepareWaterMimeticCell(1.,rho,dt,{0.,0.,0.},faces,op,error),"independent microscopic pressure patch keeps a positive mass metric");
 std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
