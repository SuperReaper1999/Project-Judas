#include "BasinFixture.h"
#include "CutRegionGeometry.h"
#include <chrono>
#include <iomanip>
#include <iostream>
using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;
namespace {
double Distance(const GridVec& p,const std::vector<BoxMotion>& boxes){
 double distance=1e300;
 for(const auto& b:boxes){double squared=0.,inside=-1e300;
  for(int a=0;a<3;++a){double q=std::max(b.lo[a]-p[a],p[a]-b.hi[a]);inside=std::max(inside,q);squared+=std::max(0.,q)*std::max(0.,q);}
  distance=std::min(distance,std::sqrt(squared)+std::min(0.,inside));
 }return distance;
}
struct SampledDistance {
 GridLayout layout;std::vector<double> nodes;
 std::size_t Index(int x,int y,int z)const{return (std::size_t(z)*(layout.cells[1]+1)+y)*(layout.cells[0]+1)+x;}
 void Build(const GridLayout& l,const std::vector<BoxMotion>& boxes){
  layout=l;nodes.resize(std::size_t(l.cells[0]+1)*(l.cells[1]+1)*(l.cells[2]+1));
  for(int z=0;z<=l.cells[2];++z)for(int y=0;y<=l.cells[1];++y)for(int x=0;x<=l.cells[0];++x)
   nodes[Index(x,y,z)]=Distance({x*l.h,y*l.h,z*l.h},boxes);
 }
 double At(const GridVec& p)const{
  std::array<int,3> base;GridVec f;
  for(int a=0;a<3;++a){base[a]=std::clamp(int(std::floor(p[a]/layout.h)),0,layout.cells[a]-1);f[a]=p[a]/layout.h-base[a];}
  double s=0.;for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x)
   s+=nodes[Index(base[0]+x,base[1]+y,base[2]+z)]*(x?f[0]:1.-f[0])*(y?f[1]:1.-f[1])*(z?f[2]:1.-f[2]);
  return s;
 }
};
}
int main(){
 std::cout<<std::setprecision(17);int failures=0;
 SampledDistance plane;plane.layout={{2,2,2},.5};plane.nodes.resize(27);
 for(int z=0;z<=2;++z)for(int y=0;y<=2;++y)for(int x=0;x<=2;++x)
  plane.nodes[plane.Index(x,y,z)]=x*.5-.6;
 const double planeError=std::abs(plane.At({.234,.567,.789})-(.234-.6));
 if(planeError>1e-15)++failures;
 std::cout<<"AFFINE_DISTANCE error="<<planeError<<std::endl;
 // A uniform sample of a distance function need not preserve thin positive
 // components. This exact signed-distance control has a positive gap but
 // negative samples on BOTH sides, so linear reconstruction closes it.
 const double gap=1e-3,lo=.5-gap*.5,hi=.5+gap*.5;
 auto thin=[&](double x){return std::min(x-lo,hi-x);};
 double approximate=.5*(thin(.45)+thin(.55)),actual=thin(.5);
 if(!(actual>0.&&approximate<0.))++failures;
 std::cout<<"THIN_GAP actual="<<actual<<" sampled="<<approximate<<" closed="<<(approximate<0.)<<std::endl;
 for(bool rotated:{false,true}){
  auto fixture=Fixture::Authored(rotated);auto boxes=fixture.Walls();boxes.push_back(fixture.InitialBox());
  auto started=std::chrono::steady_clock::now();SampledDistance sdf;sdf.Build(fixture.layout,boxes);
  double build=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
  CutRegionChart chart;CutRegionGeometry geometry;std::string error;
  if(!PrepareCutRegionChart(fixture.layout,fixture.Walls(),fixture.InitialBox(),{-.005,-.015,-.005},{.005,.025,.005},chart,error)||!BuildCutRegionGeometry(chart,{},geometry,error)){std::cerr<<error;return 2;}
  std::vector<GridVec> points;for(auto face:geometry.faces)if(face.meanArea>0.){GridVec p;for(int a=0;a<3;++a)p[a]=face.lo[a]+.5*(face.hi[a]-face.lo[a]);points.push_back(p);}
  std::vector<double> exact(points.size()),sampled(points.size());started=std::chrono::steady_clock::now();
  for(std::size_t i=0;i<points.size();++i)exact[i]=Distance(points[i],boxes);
  double exactMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();started=std::chrono::steady_clock::now();
  for(std::size_t i=0;i<points.size();++i)sampled[i]=sdf.At(points[i]);
  double queryMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();std::size_t signErrors=0,free=0;double errorMax=0.,minFree=1e300;
  for(std::size_t i=0;i<points.size();++i){errorMax=std::max(errorMax,std::abs(exact[i]-sampled[i]));if(exact[i]>0.){++free;minFree=std::min(minFree,exact[i]);if(sampled[i]<=0.)++signErrors;}}
  std::cout<<"SDF rotated="<<rotated<<" samples="<<sdf.nodes.size()<<" bytes="<<sdf.nodes.size()*sizeof(double)<<" points="<<points.size()<<" build_ms="<<build<<" analytic_query_ms="<<exactMs<<" sampled_query_ms="<<queryMs<<" maxDistanceError="<<errorMax<<" freePoints="<<free<<" freeClassifiedSolid="<<signErrors<<" minimumPositiveDistance="<<minFree<<std::endl;
 }
 return failures?1:0;
}
