#include "RuntimeWorld.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
void Run(double from, double to) {
    Scene scene;
    scene.Settings().worldOrigin={from,0,0};
    RuntimeWorld world;
    world.legacyGameplay=false;
    std::string error;
    if(!world.Build(scene,nullptr,error))throw std::runtime_error(error);
    const auto before=world.Settings().worldOrigin;
    const bool admitted=world.RequestOrigin({to,0,0},error);
    bool committed=false,threw=false;
    std::string exception;
    try { if(admitted)committed=world.CommitOrigin(); }
    catch(const std::exception& e){threw=true;exception=e.what();}
    std::cout<<std::setprecision(17)<<"ORIGIN from="<<from<<" to="<<to
             <<" admitted="<<admitted<<" committed="<<committed<<" threw="<<threw
             <<" pending="<<world.OriginPending()<<" revision="<<world.OriginRevision()
             <<" after="<<world.Settings().worldOrigin.x<<" error="<<error
             <<" exception="<<exception<<" alive="<<world.Physics().AliveBodyCount()<<'\n';
}
int main(){Run(-1e12,1e12);Run(-1e12,0);Run(0,512);Run(1e12,1e12);Run(1e12,-1e12);}
