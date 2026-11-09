#include "CollisionAsset.h"
#include "Wheel.h"
#include "SceneSerialization.h"
#include "Scene.h"
#include "SceneFingerprint.h"
#include "vehicles/WheelContact.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) throw std::runtime_error(label);
    std::cout << "PASS " << label << '\n';
}
void ContactControls() {
    PhysicsWorld world;
    world.Init();
    const auto floor = world.CreateStaticBox({0,-.5f,0}, {5,.5f,5}, .8f, 0);
    const auto chassis = world.CreateDynamicBox({0,.8f,0}, {.4f,.1f,.7f}, 100, .5f, 0);
    WheelSettings wheel;
    wheel.chassis = 1;
    wheel.maxLength = .7f;
    std::vector<BodyHandle> scratch;
    auto support = [&] {
        return JudasVehicles::FindSupport(world,chassis,wheel,{.7f,.67f,0},
                                          {0,-1,0},{-1,0,0},scratch);
    };
    auto hit = support();
    Check(hit.hit && hit.body.id == floor.id && std::abs(hit.distance-.32f)<2e-4f,
          "finite tread supports independent radius and travel on flat ground");
    world.CreateStaticBox({1.125f,.5f,0},{.1f,.5f,5},.8f,0);
    hit = support();
    Check(hit.hit && hit.body.id == floor.id && hit.normal.y>.99f,
          "wall obstruction cannot hide supporting floor");
    world.SetBodyEnabled(floor,false);
    Check(!support().hit,"vertical wall cannot invent suspension support over empty space");

    // Width is genuine query geometry: a narrow rail outside the tread must
    // not support a narrow tyre merely because the old sphere reached it.
    world.CreateStaticBox({.92f,-.05f,0},{.015f,.05f,2},.8f,0);
    Check(!support().hit,"narrow tread does not contact an out-of-width rail");
    wheel.width=.6f;
    Check(support().hit,"wider authored tread reaches the real rail");
}
void SameMeshControls() {
    MeshData mesh;
    for (auto p : {glm::vec3(-3,0,-3),glm::vec3(.9f,0,-3),glm::vec3(.9f,0,3),
                   glm::vec3(-3,0,3),glm::vec3(.9f,0,-3),glm::vec3(.9f,2,-3),
                   glm::vec3(.9f,2,3),glm::vec3(.9f,0,3)}) {
        MeshVertex v; v.position=p; mesh.vertices.push_back(v);
    }
    mesh.indices={0,2,1,0,3,2,4,6,5,4,7,6};
    auto asset=std::make_shared<CollisionAsset>();
    CollisionCookSettings cook; cook.convex=false; cook.twoSided=true;
    std::string error;
    if(!CookCollision(mesh,cook,*asset,error)) throw std::runtime_error("cook: "+error);
    Check(true,"independent floor/wall triangle fixture cooks");
    PhysicsWorld world; world.Init();
    const auto body=world.CreateShape(Shape::Cooked(asset,"support-test"),{{0,0,0},{1,0,0,0}},false,0,.8f,0);
    PhysicsQueryFilter filter;
    const auto raw=world.SphereCast({.7f,.67f,0},.35f,{0,-1,0},.7f,filter);
    Check(raw.hit && raw.initialOverlap && std::abs(raw.normal.y)<.1f,
          "negative control really encounters the wall first in a shared mesh");
    filter.normalAxis={0,1,0}; filter.minimumNormalDot=1e-4f;
    const auto support=world.SphereCast({.7f,.67f,0},.35f,{0,-1,0},.7f,filter);
    Check(support.hit && support.body.id==body.id && support.normal.y>.99f &&
              std::abs(support.distance-.32f)<2e-4f,
          "support selection continues through triangles of the same collider");
}
void AuthoringControls() {
    Scene scene;
    auto& chassis=scene.CreateObject("Chassis");
    chassis.body=SceneBodyComponent{};chassis.body->motion=SceneBodyMotion::Dynamic;
    const auto chassisId=chassis.id;
    auto& tyre=scene.CreateObject("Wide tyre");
    tyre.wheel=WheelSettings{};tyre.wheel->chassis=chassisId;tyre.wheel->width=.48f;
    std::string text,error,before,after;
    Scene restored;
    Check(SaveSceneToString(scene,text)&&LoadSceneFromString(text,restored,error)&&
              ScenesEqual(scene,restored),"authored tread width roundtrips the actual scene codec");
    Check(ComputeSceneFingerprint(scene,before,error),"finite-width scene fingerprints");
    scene.Find(tyre.id)->wheel->width=.49f;
    Check(ComputeSceneFingerprint(scene,after,error)&&before!=after,
          "tread width participates in physical save compatibility");
}
}
int main() {
    try {ContactControls();SameMeshControls();AuthoringControls();std::cout<<"SUMMARY "<<checks<<" checks\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
