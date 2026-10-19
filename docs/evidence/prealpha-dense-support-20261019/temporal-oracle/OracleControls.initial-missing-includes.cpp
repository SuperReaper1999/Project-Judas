#include "FloorOracle.h"
#include <cstdio>

int main(){
    unsigned checks=0,failures=0;
    auto check=[&](bool value,const char* text){++checks;if(!value){++failures;std::printf("FAIL %s\n",text);}};
    for(const auto rotation:{glm::quat(1,0,0,0),glm::angleAxis(1.13f,glm::normalize(glm::vec3(.31f,.77f,-.41f)))}){
        PhysicsWorld physics;check(physics.Init(),"native physics initialization");
        const glm::vec3 centre(4,-2,7);const glm::vec3 up=rotation*glm::vec3(0,1,0);
        auto floor=physics.CreateStaticBox(centre,rotation,{4,.25f,5},.65f,.025f);
        auto frame=temporal_quality::ReadFloor(physics,floor);
        check(frame.valid,"actual authored rotated box floor exists");
        auto sphere=physics.CreateDynamicSphere(centre+up*.65f,.3f,2,.65f,.025f);
        Shape shape;BodyTransform pose;physics.GetBodyShape(sphere,shape,pose);
        auto above=temporal_quality::ObserveFloor(frame,shape,pose);
        check(above.valid&&above.fullyInside&&std::abs(above.clearance-.1)<1e-5,"sphere clearance from true finite floor top");
        auto box=physics.CreateDynamicBox(centre+up*.85f,{.2f,.4f,.3f},2,.65f,.025f);
        physics.ResetBody(box,centre+up*.85f,rotation);physics.GetBodyShape(box,shape,pose);
        auto cube=temporal_quality::ObserveFloor(frame,shape,pose);
        check(cube.valid&&cube.fullyInside&&std::abs(cube.clearance-.2)<1e-5,"box support extent in actual arbitrary floor frame");
        physics.ResetBody(sphere,centre-up*1.2f,rotation);physics.GetBodyShape(sphere,shape,pose);
        auto below=temporal_quality::ObserveFloor(frame,shape,pose);
        check(below.valid&&below.fullyInside&&below.clearance< -1.7,"already crossed nonoverlapping sphere detected below floor");
        RigidBody a,b;a.position=pose.position;a.orientation=pose.rotation;b.position=centre;b.orientation=rotation;
        check(ComputeContacts(shape,a,Shape::Box({4,.25f,5}),b,0).count==0,"negative crossing is not merely an overlapping-contact oracle");
        physics.ResetBody(sphere,centre+rotation*glm::vec3(6,.65f,0),rotation);physics.GetBodyShape(sphere,shape,pose);
        auto outside=temporal_quality::ObserveFloor(frame,shape,pose);
        check(outside.valid&&!outside.centreInside&&!outside.fullyInside,"finite floor footprint excluded outside true box");
        physics.DestroyBody(floor);auto missing=temporal_quality::ReadFloor(physics,floor);
        check(!missing.valid&&!temporal_quality::ObserveFloor(missing,shape,pose).valid,"absent floor reported unavailable rather than zero penetration");
        shape.pivotOffset={.1f,0,0};
        check(!temporal_quality::ObserveFloor(frame,shape,pose).valid,"unsupported displaced descriptor explicitly rejected");
    }
    std::printf("floor_oracle checks=%u failures=%u\n",checks,failures);return failures?1:0;
}
