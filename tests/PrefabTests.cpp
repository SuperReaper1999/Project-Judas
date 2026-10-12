#include "Prefab.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "WorldState.h"
#include "editor/EditorDocument.h"
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <cstdio>

namespace {int checks=0,failures=0;void Check(bool b,const char* s){++checks;if(!b)++failures;std::printf("%s %s\n",b?"PASS":"FAIL",s);}}
int main(){
    namespace fs=std::filesystem;
    const fs::path directory=fs::temp_directory_path()/"judas-m36-prefab-tests";
    fs::remove_all(directory);fs::create_directories(directory/"Assets");
    std::string error;Scene prefab;prefab.Settings().name="Reusable assembly";
    auto& root=prefab.CreateObject("Root");root.body=SceneBodyComponent{};root.body->motion=SceneBodyMotion::Dynamic;root.render=SceneRenderComponent{};
    const auto rid=root.id;
    auto& child=prefab.CreateObject("Child light");const auto cid=child.id;child.parent=rid;child.transform.position={0,2,0};child.light=SceneLightComponent{};child.light->color={3,3,3};child.render=SceneRenderComponent{};
    Check(ValidatePrefab(prefab,error),"source hierarchy valid");
    std::string text;SaveSceneToString(prefab,text);Scene round;
    Check(LoadSceneFromString(text,round,error)&&ScenesEqual(prefab,round),"prefab scene-component roundtrip");
    const auto path=(directory/"Assets/assembly.judasprefab").string();Check(SaveSceneToFile(prefab,path,error),"save source");
    AssetDatabase assets;assets.Scan(directory.string(),(directory/"Assets").string());AssetRecord asset;
    Check(assets.Track(path,asset,error),"normal prefab asset metadata");
    if(failures){std::printf("ERROR %s\n",error.c_str());return 1;}
    Scene scene;SceneTransform placement;placement.position={4,0,0};SceneObjectId a=0,b=0;
    Check(InstantiatePrefab(scene,prefab,asset.id,placement,a,error),"first linked instance");placement.position={-4,0,0};
    Check(InstantiatePrefab(scene,prefab,asset.id,placement,b,error),"second linked instance");
    const auto childA=scene.Find(a)->prefabIds.at(cid),childB=scene.Find(b)->prefabIds.at(cid);
    Check(a!=b&&childA!=childB&&scene.Find(childA)->parent==a,"independent stable hierarchy identity");
    Scene before=scene;scene.Find(childA)->light->color={5,5,5};CapturePrefabEdits(before,scene);
    Check(scene.Find(childA)->prefabOverrides.count("light.color"),"generic serialized-property override captured");
    prefab.Find(cid)->light->color={4,4,4};auto& extra=prefab.CreateObject("Added source child");extra.parent=rid;
    Check(SaveSceneToFile(prefab,path,error),"intentional source edit");
    Scene resolved;Check(ResolvePrefabs(scene,&assets,resolved,error),"resolve source edit and added child");
    if(!resolved.Find(childA)){std::printf("ERROR %s\n",error.c_str());return 1;}
    Check(resolved.Find(childA)->light->color.x==5&&resolved.Find(childB)->light->color.x==4,"override wins and unmodified source propagates");
    Check(resolved.Find(childA)->prefabSource==cid,"child identity survives source addition");
    bool authoredOrder=resolved.Objects().size()>=scene.Objects().size();
    for(size_t i=0;i<scene.Objects().size()&&authoredOrder;++i)authoredOrder=resolved.Objects()[i].id==scene.Objects()[i].id;
    Check(authoredOrder,"resolver retains exact authored order while new children append stably");
    Check(resolved.Objects()[scene.Objects().size()].prefabRoot==a&&resolved.Objects()[scene.Objects().size()+1].prefabRoot==b,"new children tied at final rank retain instance expansion order");
    Scene reordered=scene;std::reverse(reordered.Objects().begin(),reordered.Objects().end());Scene reorderedResolved;
    bool reversedOrder=ResolvePrefabs(reordered,&assets,reorderedResolved,error);
    for(size_t i=0;i<reordered.Objects().size()&&reversedOrder;++i)reversedOrder=reorderedResolved.Objects()[i].id==reordered.Objects()[i].id;
    Check(reversedOrder,"source order rather than numeric identity controls mixed hierarchy resolution");
    Check(RevertPrefabProperty(resolved,childA,"light.color",assets,error)&&resolved.Find(childA)->light->color.x==4,"revert property to current source");
    SaveSceneToString(resolved,text);Scene reloaded;Check(LoadSceneFromString(text,reloaded,error)&&ScenesEqual(resolved,reloaded),"linked instance mapping and override roundtrip");
    Scene flat;Check(FlattenHierarchy(resolved,flat,error)&&flat.Find(childA)->transform.position==glm::vec3(4,2,0),"parent-local transform resolution");
    ResourceManager resources(nullptr,&assets);
    RuntimeWorld world;Check(world.Build(resolved,&resources,error),"ordinary world construction resolves prefab");
    const auto fingerprint=world.BaselineFingerprint();const auto saved=CaptureWorldState(world);
    prefab.Find(cid)->light->range=12;SaveSceneToFile(prefab,path,error);RuntimeWorld changed;
    Check(changed.Build(resolved,&resources,error)&&changed.BaselineFingerprint()!=fingerprint,"effective authored fingerprint notices source edit");
    Check(!ApplyWorldState(changed,saved,error),"old save rejected against changed prefab baseline");
    Check(world.Build(resolved,&resources,error),"refresh current authored baseline before spawning");
    auto start=std::chrono::steady_clock::now();auto spawned=world.SpawnPrefab(asset.id,placement,error);
    const auto spawnUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
    Check(spawned!=0,"runtime spawn actual body and child light");
    Check(world.FindEntity(spawned)&&!world.FindEntity(spawned)->authored,"spawn root ordinary independent runtime identity");
    auto delta=CaptureWorldState(world);RuntimeWorld restored;
    Check(restored.Build(resolved,&resources,error),"fresh current baseline");
    auto malformed=delta;
    for(auto& change:malformed.entities)if(change.created&&change.definition.parent){change.definition.parent=987654321;break;}
    const auto beforeCount=restored.Entities().size();
    Check(!ApplyWorldState(restored,malformed,error)&&restored.Entities().size()==beforeCount&&restored.AdditionalEntities().empty(),"invalid created parent rejected before world mutation");
    Check(ApplyWorldState(restored,delta,error),"created runtime hierarchy state reconstruction");
    Check(world.DestroyHierarchy(spawned,error)&&world.FindEntity(spawned)->lifecycle==EntityLifecycle::Destroyed,"destroy spawned hierarchy normally");
    Check(world.FindEntity(a)->lifecycle!=EntityLifecycle::Destroyed,"other instance unaffected");
    RuntimeWorld play;const auto authoredBefore=resolved;
    Check(play.Build(resolved,&resources,error),"editor authored prefab constructs Play world");play.Destroy();
    Check(ScenesEqual(authoredBefore,resolved),"Stop discards runtime without mutating authored prefab instances");
    const auto oldSize=world.Entities().size();Check(world.SpawnPrefab(std::string(32,'0'),placement,error)==0&&world.Entities().size()==oldSize,"missing prefab fails without half-instance");
    std::printf("PERFORMANCE one_spawn_us %.3f\n",spawnUs);
    EditorDocument doc;doc.GetScene()=resolved;doc.BeginEdit();doc.GetScene().Find(childA)->light->range=42;doc.CommitEdit();
    Check(doc.GetScene().Find(childA)->prefabOverrides.count("light.range"),"ordinary editor property edit creates override");doc.Undo();
    Check(doc.GetScene().Find(childA)->light->range!=42,"prefab edit undo restores authored state");
    // Representation changes still use ordinary component headers/fields.
    before=resolved;resolved.Find(childA)->audioEmitter=SceneAudioEmitterComponent{};
    CapturePrefabEdits(before,resolved);Scene withAudio;
    Check(ResolvePrefabs(resolved,&assets,withAudio,error)&&withAudio.Find(childA)->audioEmitter.has_value(),"generic component addition override");
    before=withAudio;withAudio.Find(childA)->light.reset();CapturePrefabEdits(before,withAudio);Scene withoutLight;
    Check(ResolvePrefabs(withAudio,&assets,withoutLight,error)&&!withoutLight.Find(childA)->light,"generic component removal override");
    Check(RevertPrefabProperty(withoutLight,childA,"light",assets,error)&&withoutLight.Find(childA)->light,"revert removed component block");
    for(int count:{1,10,100}){
        Scene batch;const auto begin=std::chrono::steady_clock::now();
        for(int i=0;i<count;++i){SceneObjectId id;InstantiatePrefab(batch,prefab,asset.id,placement,id,error);}
        std::printf("PERFORMANCE instantiate_count %d total_us %.3f\n",count,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count());
    }
    resources.Shutdown();fs::remove_all(directory);
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
