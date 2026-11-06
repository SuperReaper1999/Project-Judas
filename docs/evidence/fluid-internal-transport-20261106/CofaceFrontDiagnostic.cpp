#include "CutPhaseFronts.cpp"
#include <iostream>
#include <iomanip>
using namespace judas::volume_grid;
int main(){
 CutRegionChart chart;chart.layout={{2,1,2},1.};const double thin=1e-6;
 chart.regions={{{0.,0.,0.},{1.,1.,thin},0},{{0.,0.,thin},{.5,1.,1.+thin},1},{{.5,0.,thin},{1.,1.,1.+thin},2}};
 chart.faces={{0,1,2,{0.,0.,thin},{.5,1.,thin},.5,.5,.5,0.,.5},{0,2,2,{.5,0.,thin},{1.,1.,thin},.5,.5,.5,0.,.5}};
 CutRegionGeometry geometry;std::string error;
 if(!BuildCutRegionGeometry(chart,{},geometry,error)){std::cerr<<error;return 2;}
 CutConnectedOwners owners;owners.owner={0,0,0};owners.members={{0,1,2}};owners.capacity={geometry.startCapacity[0]+geometry.startCapacity[1]+geometry.startCapacity[2]};
 CutDynamicState source;source.water={0.,geometry.startCapacity[1],0.};source.gasMass={1.2*geometry.startCapacity[0],0.,1.2*geometry.startCapacity[2]};
 std::vector<CutPhaseBoundary> ports={{1,2,1,-1.,1.,false},{2,2,1,1.,0.,false}};

 State initial;initial.w=source.water;initial.g={geometry.startCapacity[0],0.,geometry.startCapacity[2]};initial.orientation.resize(3);
 std::vector<double> density={1.2},external={-1.,1.},predictor={0.,0.},mobility={1.,1.},sources={0.};
 Context context{chart,geometry,owners,density,external,ports,predictor,mobility,1.2,sources};
 for(double h:{1e-8,1e-10,1e-12,1e-14}){
  State full,half,end;Rate f,a,b;
  if(!Heun(context,initial,h,full,f,error)||!Heun(context,initial,h/2.,half,a,error)||!Heun(context,half,h/2.,end,b,error)){std::cerr<<error;return 2;}
  double defect=0.;for(std::size_t i=0;i<3;++i)defect+=std::abs(full.w[i]-end.w[i]);
  std::cout<<std::setprecision(17)<<"COFACE_ERROR h="<<h<<" full="<<full.w[0]<<" halves="<<end.w[0]<<" centralErrorOverH="<<std::abs(full.w[0]-end.w[0])/h<<" ownerErrorOverH="<<defect/h<<'\n';
 }
 CutPhaseFrontSettings settings;settings.maximumSteps=64;CutPhaseFrontResult out;out.water={-7.};
 bool ok=AdvanceCutPhaseFronts(chart,geometry,owners,source,{1.2},{0.,0.},ports,{0.,0.},{1.,1.},settings,out,error);
 std::cout<<std::setprecision(17)<<"COFACE admitted="<<ok<<" unchanged="<<(out.water==std::vector<double>{-7.})<<" reason="<<error<<'\n';
 std::cout<<"The first Euler stage adds h water to the initially dry thin region.\n"
          <<"Its single upper-facing plane then wets both disjoint top-face patches.\n"
          <<"The endpoint rate is zero; Heun gives h/2 but two half steps give h/4.\n"
          <<"The central defect/h is 1/4; the owner L1 defect/h is 1/2, independent of h.\n";
 return ok?0:1;
}
