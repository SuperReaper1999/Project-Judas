#include "WorldPersistence.h"
#include "Prefab.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include <filesystem>
#include <cstdio>
int checks=0,failures=0;void Check(bool v,const char* s){++checks;failures+=!v;std::printf("%s %s\n",v?"PASS":"FAIL",s);}
int main(){
 namespace fs=std::filesystem;auto dir=fs::temp_directory_path()/"judas-m45-authoring";fs::remove_all(dir);fs::create_directories(dir/"Assets");std::string error,text;
 Scene source;auto& a=source.CreateObject("Root");auto root=a.id;a.body=SceneBodyComponent{};a.body->motion=SceneBodyMotion::Dynamic;
 auto& b=source.CreateObject("Child");auto child=b.id;b.parent=root;b.transform.position={2,0,0};b.body=SceneBodyComponent{};b.body->motion=SceneBodyMotion::Dynamic;
 b.joint=SceneJointComponent{};b.joint->bodyA=root;b.joint->bodyB=child;b.joint->settings.anchorA={1,0,0};b.joint->settings.anchorB={-1,0,0};
 SaveSceneToString(source,text);Scene round;Check(LoadSceneFromString(text,round,error)&&ScenesEqual(source,round),"joint authored roundtrip");
 std::string hash,changed;ComputeSceneFingerprint(source,hash,error);round.Find(child)->joint->settings.enabled=false;ComputeSceneFingerprint(round,changed,error);Check(hash!=changed,"joint settings affect canonical authored fingerprint");
 auto path=(dir/"Assets/assembly.judasprefab").string();SaveSceneToFile(source,path,error);AssetDatabase assets;assets.Scan(dir.string(),(dir/"Assets").string());AssetRecord asset;Check(assets.Track(path,asset,error),"joint prefab normal asset");
 Scene scene;SceneTransform place;SceneObjectId first,second;Check(InstantiatePrefab(scene,source,asset.id,place,first,error),"first joint prefab");place.position={5,0,0};Check(InstantiatePrefab(scene,source,asset.id,place,second,error),"second joint prefab");auto c=scene.Find(first)->prefabIds.at(child);Check(scene.Find(c)->joint->bodyA==first&&scene.Find(c)->joint->bodyB==c,"stable participant remapping");
 Scene before=scene;scene.Find(c)->joint->settings.enabled=false;CapturePrefabEdits(before,scene);Scene resolved;Check(ResolvePrefabs(scene,&assets,resolved,error)&&!resolved.Find(c)->joint->settings.enabled,"generic joint override");
 ResourceManager resources(nullptr,&assets);RuntimeWorld world;Check(world.Build(scene,&resources,error),"joint prefab actual world build");auto h=world.RuntimeJoint(c);JointState state;Check(h.IsValid()&&world.Physics().GetJoint(h,state)&&!state.active,"runtime joint respects authored override");
 auto spawned=world.SpawnPrefab(asset.id,place,error);EntityId jointOwner=0;for(const auto& o:world.ScriptObjects())if(o.parent==spawned&&o.joint)jointOwner=o.id;
 Check(spawned&&jointOwner&&world.RuntimeJoint(jointOwner).IsValid(),"runtime spawned participant hierarchy creates joint");
 std::map<std::string,SaveChunk> saved;Scene savedScene;RuntimeWorld restored;Check(WorldPersistence::Capture(world,saved,error)&&WorldPersistence::SceneFrom(saved,savedScene,error)&&restored.Build(savedScene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,saved,error)&&restored.RuntimeJoint(jointOwner).IsValid(),"created joint hierarchy current snapshot persistence");
 auto retained=world.RuntimeJoint(jointOwner);world.DestroyHierarchy(spawned,error);Check(!world.Physics().GetJoint(retained,state),"hierarchy destruction invalidates joint");
 Scene invalid=source;invalid.Find(child)->joint->bodyA=999;SaveSceneToString(invalid,text);Check(!LoadSceneFromString(text,round,error),"missing participant rejected atomically");
 source.DestroyObject(root);Check(source.Objects().empty(),"hierarchy deletion clears joint references");
 std::printf("SUMMARY %d checks %d failures ERROR %s\n",checks,failures,error.c_str());return failures?1:0;
}
