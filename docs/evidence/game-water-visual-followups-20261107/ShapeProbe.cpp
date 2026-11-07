#include "fluid/GameWaterSurface.h"
#include <iostream>
int main(){
 JudasWater::Settings s;s.cells={8,8,8};s.cellSize=.4f;s.origin={-1.6f,0,-1.6f};
 JudasWater::Grid g;std::string e;g.Initialize(s,e);g.FillBox({-2,0,-2},{2,1.99f,2});
 JudasWater::SolidBox b;b.kind=JudasWater::SolidBox::Kind::Capsule;b.center={0,.8f,.6f};b.radius=.3f;b.halfHeight=.6f;b.half={.3f,.9f,.3f};g.SetSolids({b});g.Step(1.f/60,e);
 auto f=JudasWater::BuildSurfaceField(g);auto m=JudasWater::BuildPoolSurface(f);unsigned sides=0,inside=0;
 for(auto& v:m.vertices)if(v.tangent.z>.5){++sides;if(v.uv.x>.001&&v.uv.y>.001&&v.uv.x<3.199&&v.uv.y<3.199){if(inside<12)std::cout<<"INNER "<<v.position.x<<','<<v.position.y<<','<<v.position.z<<" uv "<<v.uv.x<<','<<v.uv.y<<" ref "<<v.tangent.w<<'\n';++inside;}}
 std::cout<<"sides "<<sides<<" inside "<<inside<<'\n';
 for(int z=3;z<=6;++z)for(int x=2;x<=5;++x){std::cout<<x<<','<<z<<":";for(int y=0;y<6;++y){auto v=f.intervals[f.Index(x,y,z)];std::cout<<' '<<v.x<<'/'<<v.y<<'/'<<v.z;}std::cout<<'\n';}
}
