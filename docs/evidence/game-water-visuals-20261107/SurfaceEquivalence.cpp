#include "fluid/GameWaterSurface.h"
#include <iostream>
#include <chrono>
#include <algorithm>
namespace JudasWater {MeshData BuildReferenceSurface(const SurfaceField&,unsigned);}
int main(){
    std::size_t total=0;std::string error;
    for(int variant=0;variant<3;++variant){
        JudasWater::Settings s;s.cells={12,8,10};s.cellSize=.4f;
        if(variant==2)s.rotation=glm::angleAxis(.37f,glm::normalize(glm::vec3(1,0,1)));
        s.gravity=s.rotation*s.gravity;
        JudasWater::Grid g;if(!g.Initialize(s,error)){std::cerr<<error;return 2;}
        g.Fill([&](glm::vec3 p){return p.y<1.6f&&(variant==0||p.x<2.3f||p.z<1.8f);});
        auto f=JudasWater::BuildSurfaceField(g);
        for(unsigned subdivision=1;subdivision<=6;++subdivision){
            auto a=JudasWater::BuildReferenceSurface(f,subdivision),b=JudasWater::BuildPoolSurface(f,subdivision);
            if(a.vertices.size()!=b.vertices.size())return 3;
            for(std::size_t i=0;i<a.vertices.size();++i){
                const auto& x=a.vertices[i];const auto& y=b.vertices[i];
                if(x.position!=y.position||x.normal!=y.normal||x.uv!=y.uv||x.uv1!=y.uv1||x.tangent!=y.tangent)return 4;
            }
            total+=a.vertices.size();
        }
        for(int pair=0;pair<20;++pair){
            double before=0,after=0;std::size_t count=0;
            auto run=[&](bool reference){auto start=std::chrono::steady_clock::now();
                auto mesh=reference?JudasWater::BuildReferenceSurface(f,4):JudasWater::BuildPoolSurface(f,4);
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                count=mesh.vertices.size();(reference?before:after)=ms;};
            run(pair%2==0);run(pair%2!=0);
            std::cout<<"TIMING,"<<variant<<','<<pair<<','<<before<<','<<after<<','<<count<<'\n';
        }
    }
    std::cout<<"PASS 18 meshes; "<<total<<" ordered vertices exactly identical in position, normal, UVs and private layer payload\n";
}
