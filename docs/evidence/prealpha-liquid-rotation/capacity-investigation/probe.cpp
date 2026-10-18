#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "LiquidSurface.cpp"
int main(int argc,char**argv){
 std::ifstream in(argv[1]);std::string magic,fp;int kind;GravityEquilibrium eq;double vt,ht,ce,cap;size_t nt,nk;in>>magic>>std::quoted(fp)>>kind>>eq.up.x>>eq.up.y>>eq.up.z>>eq.center.x>>eq.center.y>>eq.center.z>>eq.magnitude>>vt>>ht>>ce>>cap>>nt>>nk;eq.kind=GravityEquilibrium::Kind(kind);double discard;for(size_t i=0;i<12*nt+2*nk;++i)in>>discard;
 LiquidSurfaceSettings s;glm::dvec3 axis,x,y;glm::dvec2 lo,hi;size_t nc,nf;in>>magic>>s.columns>>s.rows>>s.friction>>s.tolerance>>s.iterations>>s.splashSpeed>>s.splashFraction>>s.parcelBudget>>axis.x>>axis.y>>axis.z>>x.x>>x.y>>x.z>>y.x>>y.y>>y.z>>lo.x>>lo.y>>hi.x>>hi.y>>nc>>nf;for(size_t i=0;i<s.columns*s.rows;++i)in>>discard;
 std::cout<<std::setprecision(24);int failures=0;
 for(size_t i=0;i<nc;++i){unsigned col,row;glm::dvec3 cen;glm::dvec2 chart;double ca;in>>col>>row>>cen.x>>cen.y>>cen.z>>chart.x>>chart.y>>ca>>vt>>ht>>nt>>nk;LiquidGeometry g;g.cells.resize(nt);for(auto&t:g.cells)for(auto&p:t)in>>p.x>>p.y>>p.z;for(size_t j=0;j<nk*2;++j)in>>discard;LiquidBasinData b;curve(g,eq,b);
  long double total=0;for(auto&t:g.cells)total+=std::abs(glm::dot(t[1]-t[0],glm::cross(t[2]-t[0],t[3]-t[0])))/6;
  std::cout<<"CELL "<<i<<" capacity="<<b.capacity<<" long_total="<<total<<" rounded_total="<<double(total)<<" nc="<<b.curve.size()<<"\n";
  for(size_t j=1;j<b.curve.size();++j)if(b.curve[j].volume<b.curve[j-1].volume){++failures;std::cout<<"DROP "<<j<<" qprev="<<b.curve[j-1].q<<" q="<<b.curve[j].q<<" vprev="<<b.curve[j-1].volume<<" v="<<b.curve[j].volume<<" poly_prev=";for(auto c:b.capacityPolynomials[j-1])std::cout<<c<<",";std::cout<<"\n";}
 }
 std::cout<<"FAILURES="<<failures<<"\n";
}
