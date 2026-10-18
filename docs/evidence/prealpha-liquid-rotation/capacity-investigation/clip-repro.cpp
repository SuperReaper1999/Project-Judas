#include <iostream>
#include <iomanip>
#include <fstream>
#include "LiquidSurface.cpp"
int main(){
 auto g=LiquidBox({-1,0,-1},{1,1,1});auto rotation=glm::angleAxis(1e-10,glm::normalize(V(1,3,2)));for(auto&t:g.cells)for(auto&p:t)p=rotation*p;
 GravityEquilibrium e;e.magnitude=9.81;e.up=glm::normalize(rotation*V(0,1,0));LiquidBasinData basin;std::string error;BakeLiquidBasin(g,e,1e-6,.02,"clip-regression",basin,error);LiquidSurfaceSettings s;s.enabled=true;s.columns=s.rows=4;std::shared_ptr<const LiquidSurfaceData> surface;if(!BakeLiquidSurface(basin,s,surface,error,&g)){std::cerr<<error;return 1;}
 double maxError=0,head=0,stored=0,direct=0;size_t ci=0,ki=0;for(size_t i=0;i<surface->cells.size();++i){const auto& b=surface->cells[i].storage;for(size_t j=1;j<b.curve.size();++j){double a=b.curve[j-1].q,z=b.curve[j].q;for(double q:{a,std::nextafter(a,z),a+(z-a)/2,std::nextafter(z,a),z}){double cv=capacity(b,q),cl=LiquidCapacity(b.geometry,e,q),er=std::abs(cv-cl);if(er>maxError){maxError=er;head=q;ci=i;ki=j;stored=cv;direct=cl;}}}}
 std::cout<<std::setprecision(24)<<"MAX cell="<<ci<<" knot="<<ki<<" q="<<head<<" stored="<<stored<<" direct="<<direct<<" error="<<maxError<<" up="<<e.up.x<<","<<e.up.y<<","<<e.up.z<<"\n";
 auto& cell=surface->cells[ci];auto& b=cell.storage;
 std::ofstream geom(".cache/prealpha-liquid-rotation/capacity-investigation/clip-failing-cell.judascavity");geom<<"JudasCavity1 "<<b.geometry.cells.size()<<"\n"<<std::setprecision(17);
 for(size_t i=0;i<b.geometry.cells.size();++i){const auto&t=b.geometry.cells[i];LiquidGeometry one;one.cells.push_back(t);LiquidBasinData exact;curve(one,e,exact);std::array<double,4> q;for(unsigned j=0;j<4;++j)q[j]=e.Coordinate(t[j]);double expected=capacity(exact,head);auto actual=LiquidClip(t,q,head);std::cout<<"TET "<<i<<" expected="<<expected<<" actual="<<actual.volume<<" delta="<<actual.volume-expected<<"\n";for(unsigned j=0;j<4;++j){std::cout<<" vertex "<<j<<"="<<t[j].x<<","<<t[j].y<<","<<t[j].z<<" q="<<q[j]<<" signed="<<q[j]-head<<"\n";geom<<t[j].x<<" "<<t[j].y<<" "<<t[j].z<<" ";}geom<<"\n";}
}
