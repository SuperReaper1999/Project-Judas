#include "PhysicsWorld.h"
#include "PlanarGeometry.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
int main(){
    std::puts("bodies,median_ms,p95_ms,worst_ms,min_y,max_abs_z");
    for(int count:{16,64,128}) {
        PhysicsWorld world;world.Init();std::vector<BodyHandle> bodies;
        world.CreateShape(Shape::Planar(PlanarBox({20,.5})),{{0,-.5,0},{1,0,0,0}},false,1,.5,0);
        for(int i=0;i<count;++i)bodies.push_back(world.CreateShape(Shape::Planar(PlanarBox({.25,.25})),
            {{float(i%16)-8,.25f+float(i/16)*.505f,0},{1,0,0,0}},true,1,.5,0));
        std::vector<double> costs;double minY=1e9,maxZ=0;
        for(int step=0;step<120;++step){const auto start=std::chrono::steady_clock::now();
            for(auto body:bodies)world.ApplyLinearAcceleration(body,{0,-9.81f,0},1.f/60);
            world.Step(1.f/60);
            const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(step>=30)costs.push_back(elapsed);
            for(auto body:bodies){const auto p=world.GetTransform(body).position;minY=std::min(minY,double(p.y));maxZ=std::max(maxZ,std::abs(double(p.z)));}}
        std::sort(costs.begin(),costs.end());
        std::printf("%d,%.6f,%.6f,%.6f,%.9g,%.9g\n",count,costs[costs.size()/2],costs[(costs.size()*95+99)/100-1],costs.back(),minY,maxZ);
        if(minY<.24||maxZ!=0)return 1;
    }
}
