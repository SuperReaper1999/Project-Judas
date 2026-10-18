#include "LiquidSurface.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <chrono>
using V=glm::dvec3;
using Face=LiquidPolygon;
double area(const Face& p){double a=0;for(size_t i=1;i+1<p.size();++i)a+=glm::length(glm::cross(p[i]-p[0],p[i+1]-p[0]))*.5;return a;}
Face clip(Face p,glm::dvec4 plane,bool inside){Face out;for(size_t i=0;i<p.size();++i){V a=p[i],b=p[(i+1)%p.size()];long double x=static_cast<long double>(plane.x)*a.x+static_cast<long double>(plane.y)*a.y+static_cast<long double>(plane.z)*a.z-plane.w,y=static_cast<long double>(plane.x)*b.x+static_cast<long double>(plane.y)*b.y+static_cast<long double>(plane.z)*b.z-plane.w;bool ia=inside?x<=0:x>0,ib=inside?y<=0:y>0;if(ia)out.push_back(a);if(ia!=ib)out.push_back(a+(b-a)*double(x/(x-y)));}return out;}
double exactOpen(const std::vector<Face>& original,const std::vector<glm::dvec4>& planes,const GravityEquilibrium& eq,double q){double result=0;for(auto polygon:original){for(auto plane:planes){auto outside=clip(polygon,plane,false);result+=LiquidFaceArea({outside},eq,q);polygon=clip(polygon,plane,true);if(polygon.empty())break;}}return result;}
std::shared_ptr<const LiquidSurfaceData> rotated(const LiquidSurfaceData& a,glm::dquat r){auto d=std::make_shared<LiquidSurfaceData>(a);d->axis=r*d->axis;d->x=r*d->x;d->y=r*d->y;d->equilibrium.up=r*d->equilibrium.up;d->equilibrium.center=r*d->equilibrium.center;for(auto& c:d->cells){c.centre=r*c.centre;c.storage.equilibrium=d->equilibrium;c.loadingGeometry.clear();for(auto& t:c.storage.geometry.cells){for(auto& v:t)v=r*v;c.loadingGeometry.push_back(CompileLiquidTetQuery(t,d->equilibrium));}}for(auto& f:d->faces){f.normal=r*f.normal;for(auto& p:f.polygons)for(auto& v:p)v=r*v;}return d;}
int main(){std::cout<<std::setprecision(17);GravityEquilibrium e;e.magnitude=9.81;auto g=LiquidBox({-1,0,-1},{1,1,1});LiquidBasinData b;std::string error;if(!BakeLiquidBasin(g,e,1e-6,.02,"face",b,error)){std::cerr<<error;return 2;}LiquidSurfaceSettings settings;settings.enabled=true;settings.columns=settings.rows=2;std::shared_ptr<const LiquidSurfaceData> data;if(!BakeLiquidSurface(b,settings,data,error,&g)){std::cerr<<error;return 2;}size_t index=0;for(;index<data->faces.size();++index)if(std::abs(data->faces[index].normal.x)>.9)break;auto f=data->faces.at(index);V tangent=glm::normalize(glm::cross(f.normal,e.up));double lower=1e30,upper=-1e30;for(auto& p:f.polygons)for(auto v:p){lower=std::min(lower,glm::dot(tangent,v));upper=std::max(upper,glm::dot(tangent,v));}double delta=2e-11/(upper-lower),offset=f.offset+delta*lower-1e-16;std::vector<glm::dvec4> solid={{f.normal+delta*tangent,offset},{-f.normal,-f.offset+.01},{e.up,10},{-e.up,10},{tangent,10},{-tangent,10}};
 double checksum=0;
 for(unsigned trial=0;trial<7;++trial){auto start=std::chrono::steady_clock::now();for(unsigned repeat=0;repeat<300;++repeat){unsigned run=repeat%3;glm::dquat r=run==0?glm::dquat(1,0,0,0):glm::angleAxis(run==1?.8:1.37,glm::normalize(V(1,2,3)));auto d=rotated(*data,r);auto s=solid;for(auto& p:s)p=glm::dvec4(r*V(p),p.w);LiquidSurface surface(d,2);double overflow=surface.SetSolids({s},error);std::fill(surface.discharge.begin(),surface.discharge.end(),0);surface.discharge[index]=1;checksum+=surface.KineticEnergy(d->faces[index].a)+overflow;}auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();std::cout<<"BENCH trial="<<trial<<" operations=300 milliseconds="<<ms<<" checksum="<<checksum<<'\n';}
}
