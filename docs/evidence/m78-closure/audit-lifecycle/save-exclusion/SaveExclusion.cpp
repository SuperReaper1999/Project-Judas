#include "../FixtureSupport.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
using namespace LifecycleAudit;
namespace {
void RunCase(Receipt& receipt,const fs::path& output,bool exclude) {
    const std::string label=exclude?"excluded dynamic":"ordinary dynamic";
    std::string error;Scene scene;
    const auto kept=scene.CreateObject("persisted body").id;
    auto* definition=scene.Find(kept);definition->body=SceneBodyComponent{};
    definition->body->motion=SceneBodyMotion::Dynamic;definition->body->managed=true;
    definition->transform.position={0,3,0};
    RuntimeWorld source,restored;
    receipt.Require(source.Build(scene,nullptr,error),label+" normal source world",error);
    SceneObject actor;actor.name="simple runtime body";actor.body=SceneBodyComponent{};
    actor.body->motion=SceneBodyMotion::Dynamic;actor.body->managed=true;actor.transform.position={5,3,0};
    const auto runtime=source.CreateEntity(actor,nullptr,&error);
    receipt.Require(runtime!=0&&source.Physics().IsDynamicBody(source.RuntimeBody(runtime)),label+" normal runtime actor ownership",error);
    source.Physics().SetLinearVelocity(source.RuntimeBody(kept),{1,2,3});
    if(exclude)receipt.Require(source.SetTransientEntity(runtime,true,error),label+" public exclusion accepted",error);
    else receipt.Check(!source.IsTransientEntity(runtime),label+" actor retained by default");
    std::map<std::string,SaveChunk> chunks;
    const bool captured=WorldPersistence::Capture(source,chunks,error);
    receipt.Require(captured,label+" full ordinary participant capture",error);
    Scene saved;receipt.Require(WorldPersistence::SceneFrom(chunks,saved,error),label+" normal saved scene decode",error);
    receipt.Check(saved.Find(kept)!=nullptr&&(bool(saved.Find(runtime))==!exclude),label+" saved scene ownership matches public exclusion");
    fs::create_directories(output);receipt.Require(SaveSceneToFile(saved,(output/"captured-scene.judas").string(),error),label+" preserved captured scene",error);
    nlohmann::json participantHashes=nlohmann::json::object();
    for(const auto& [name,chunk]:chunks){std::ofstream file(output/(name+".participant"),std::ios::binary);file.write(chunk.data.data(),std::streamsize(chunk.data.size()));participantHashes[name]={{"version",chunk.version},{"bytes",chunk.data.size()},{"sha256",SceneFingerprintSha256(chunk.data)}};}
    std::ofstream(output/"PARTICIPANTS.json")<<participantHashes.dump(2)<<'\n';
    receipt.Require(restored.Build(saved,nullptr,error,nullptr,nullptr,true),label+" normal private saved-world construction",error);
    receipt.Require(WorldPersistence::Prepare(restored,error),label+" normal private participant preparation",error);
    const bool loaded=WorldPersistence::Restore(restored,chunks,error);
    // Both legal cases must restore. The excluded actor is absent by design;
    // no assertion treats a save rejection as successful exclusion behaviour.
    receipt.Check(loaded,label+" full private restore succeeds",{{"loaded",loaded},{"error",error}});
    if(loaded){receipt.Check(restored.Physics().GetLinearVelocity(restored.RuntimeBody(kept))==glm::vec3(1,2,3),label+" retained body's real velocity survives");
        receipt.Check(bool(restored.RuntimeDefinition(runtime))==!exclude,label+" restored ownership matches authored exclusion");}
}
}
int main(int argc,char** argv){Receipt receipt;fs::path output;try{
    output=FreshOutput(argc,argv);RunCase(receipt,output/"ordinary",false);RunCase(receipt,output/"excluded",true);
    return receipt.Finish(output,"Public saves.exclude may omit a simple dynamic actor without leaving required orphaned participants; no integration or fixture retuning.");
}catch(const std::exception& e){receipt.Check(false,"unexpected fixture failure",{{"error",e.what()}});if(!output.empty())return receipt.Finish(output,"save exclusion");std::cerr<<e.what()<<'\n';return 2;}}
