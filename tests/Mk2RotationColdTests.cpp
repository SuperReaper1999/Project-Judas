// Real process-separated WorldPersistence/SaveStorage rotation continuation.
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SaveStorage.h"
#include "ToolProcess.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using Json=nlohmann::json;
constexpr float dt=1.f/60;
unsigned checks=0;
void Require(bool okay,const std::string& why) {
    ++checks;
    if (!okay) throw std::runtime_error(why);
}
Json State(RuntimeWorld& world,EntityId id) {
    const auto h=world.RuntimeBody(id);
    const auto t=world.Physics().GetTransform(h);
    const auto v=world.Physics().GetLinearVelocity(h),w=world.Physics().GetAngularVelocity(h);
    return Json::array({t.position.x,t.position.y,t.position.z,t.rotation.w,t.rotation.x,
        t.rotation.y,t.rotation.z,v.x,v.y,v.z,w.x,w.y,w.z});
}
Scene Fixture(EntityId& player) {
    Scene scene;
    for (float x:{-.6f,.6f}) {
        auto& wall=scene.CreateObject("Physical wall");
        wall.transform.position={x,0,0};wall.body=SceneBodyComponent{};
        wall.body->halfExtents={.05f,1,1};wall.body->friction=0;wall.body->restitution=1;
    }
    auto& body=scene.CreateObject("Spinning elastic body");player=body.id;
    body.body=SceneBodyComponent{};body.body->motion=SceneBodyMotion::Dynamic;
    body.body->halfExtents={.08f,.2f,.05f};body.body->friction=0;body.body->restitution=1;
    body.body->initialLinearVelocity={5,0,0};body.body->initialAngularVelocity={3,7,20};
    return scene;
}
}
int main(int argc,char** argv) {
    if (argc==2) {
        std::string output,error;
        const auto executable=std::filesystem::absolute(argv[0]);
        for (const std::string mode:{"write","read"}) {
            if (!CaptureToolOutput(executable,{mode,argv[1]},output,error,
                                  std::chrono::seconds(60),65536)) {
                std::cerr<<output<<"\nFAIL child "<<mode<<": "<<error<<'\n';return 1;
            }
            std::cout<<output;
        }
        return 0;
    }
    if (argc!=3) { std::cerr<<"usage: mk2_rotation_cold_tests write|read <owned-output>\n";return 2; }
    try {
        const auto output=std::filesystem::absolute(argv[2]);
        const bool write=std::string(argv[1])=="write";
        Require(write||std::string(argv[1])=="read","known mode");
        if (write) { Require(!std::filesystem::exists(output),"write output must be new");std::filesystem::create_directories(output); }
        SaveStorage storage((output/"slots").string());
        RuntimeWorld world;Scene scene;std::string error;EntityId player=0;
        GameSnapshot saved;Json expected;
        if (write) {
            scene=Fixture(player);
            Require(world.Build(scene,nullptr,error),"build authored fixture: "+error);
            for (int step=0;step<17;++step) world.Physics().Step(dt);
            Require(WorldPersistence::Capture(world,saved.participants,error),"capture actual physical state: "+error);
            saved.project="mk2-rotation-cold";saved.scene="elastic";saved.content=std::string(64,'0');
            saved.metadata=Json{{"body",player}}.dump();
            Require(storage.Write("spin",saved,error),"atomic native save: "+error);
            expected["boundary"]=State(world,player);
            for (int step=0;step<240;++step) {
                world.Physics().Step(dt);
                expected["steps"].push_back(State(world,player));
            }
            std::ofstream(output/"uninterrupted.json")<<expected.dump(2)<<'\n';
        } else {
            bool recovered=false;
            Require(storage.Read("spin",saved,recovered,error)&&!recovered,"fresh-process save read: "+error);
            player=Json::parse(saved.metadata).at("body");
            Require(WorldPersistence::SceneFrom(saved.participants,scene,error),"saved content reconstruction: "+error);
            Require(world.Build(scene,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error),"fresh runtime reconstruction: "+error);
            Require(WorldPersistence::Restore(world,saved.participants,error),"restore real physical participants: "+error);
            std::ifstream file(output/"uninterrupted.json");file>>expected;
            const auto compare=[&](const Json& reference) {
                const auto actual=State(world,player);
                for (std::size_t k=0;k<actual.size();++k)
                    Require(actual[k]==reference[k],"cold continuation differs in physical scalar "+std::to_string(k));
            };
            compare(expected.at("boundary"));
            for (const auto& reference:expected.at("steps")) {world.Physics().Step(dt);compare(reference);}
            // A negative control proves the comparison observes physical
            // continuation rather than a saved status or echoed metadata.
            const auto h=world.RuntimeBody(player);
            world.Physics().ApplyImpulseAtPoint(h,{1,0,0},world.Physics().GetCenterOfMass(h));
            Require(State(world,player)!=expected.at("steps").back(),"negative impulse changes the compared physical endpoint");
        }
        std::cout<<"MK2 rotation cold "<<(write?"write":"read")<<": "<<checks<<" checks PASS\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
