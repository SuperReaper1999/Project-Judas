#include "GltfLoader.h"
#include "ModelLoader.h"
#include "SkeletalAnimation.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "ScreenshotWriter.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <set>
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
bool Near(float a,float b){return std::abs(a-b)<2e-5f;}
int main(int argc,char** argv){
 namespace fs=std::filesystem;fs::path out=argc==3?argv[2]:"build/m46-focused";fs::create_directories(out);
 MeshData mesh,json;std::string error;
 Check(LoadModelMesh("tests/fixtures/animation/Assets/bend_bar.glb",mesh,error),"cgltf GLB skin import");
 Check(LoadModelMesh("tests/fixtures/m46/bend_bar.gltf",json,error),"cgltf embedded glTF import");
 if(!mesh.skeletal){std::puts(error.c_str());return 1;}
 const auto& asset=*mesh.skeletal;const auto& s=asset.skeleton;
 Check(s.parents.size()==4&&s.parents[1]==0&&s.parents[2]==1&&s.skinNodes.size()==3,"bone hierarchy includes mesh and ancestors");
 Check(mesh.vertices.size()==json.vertices.size()&&mesh.skinVertices.size()==mesh.vertices.size()&&asset.clips.size()==2,"vertex influences and two named clips");
 auto rest=ResolveSkinMatrices(s,s.rest);bool identity=true;for(auto m:rest)for(int x=0;x<4;++x)for(int y=0;y<4;++y)identity&=Near(m[x][y],x==y?1:0);
 Check(identity,"inverse bind transforms resolve rest skin to identity");
 auto sample=SampleClip(s,asset.clips[0],.5f);Check(Near(sample.local[1].rotation.z,std::sin(.35f))&&Near(sample.local[2].rotation.z,std::sin(-.25f)),"known Wave sample uses independent trigonometric reference");
 auto skin=ResolveSkinMatrices(s,sample);auto point=skin[1]*glm::vec4(0,2,0,1);
 Check(Near(point.x,-std::sin(.7f))&&Near(point.y,1+std::cos(.7f)),"joint hierarchy deforms known vertex to analytic point");
 sample=SampleClip(s,asset.clips[0],.25f);Check(Near(glm::length(sample.local[1].rotation),1)&&Near(sample.local[1].rotation.z,std::sin(.175f)),"linear quaternion interpolation stays normalized");
 AnimationClip special;special.duration=1;AnimationTrack track;track.node=0;track.path=TrackPath::Translation;track.times={0,1};track.values={{0,0,0,0},{2,0,0,0}};track.interpolation=TrackInterpolation::Step;special.tracks={track};Check(Near(SampleClip(s,special,.5f).local[0].translation.x,0),"STEP interpolation");
 track.interpolation=TrackInterpolation::CubicSpline;track.values={{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{2,0,0,0},{0,0,0,0}};special.tracks={track};Check(Near(SampleClip(s,special,.5f).local[0].translation.x,1),"CUBICSPLINE Hermite translation");
 AnimationPlayback a,b;a.clip="Wave";b.clip="Stretch";a.Evaluate(asset,.25);b.Evaluate(asset,.75);Check(Near(a.time,.25)&&Near(b.time,.75),"shared immutable asset independent playback");a.speed=2;a.Evaluate(asset,1);Check(Near(a.time,.25),"loop and speed wrap");a.loop=false;a.Seek(asset,1.5);a.Evaluate(asset,1);Check(!a.playing&&Near(a.time,2),"non-loop terminates at end");a.Stop(asset);Check(a.stopped&&!a.playing&&Near(a.time,0),"stop resets playback to rest");
 MeshData untouched=mesh;const char bad[]="{bad";Check(!ParseGltfMesh(bad,sizeof(bad),untouched,error)&&untouched.skeletal==mesh.skeletal,"invalid glTF fails transactionally");
 std::ifstream input("tests/fixtures/m46/bend_bar.gltf");std::string content((std::istreambuf_iterator<char>(input)),{});auto uri=content.find("data:application/octet-stream;base64,");content.replace(uri,35,"external.bin");Check(!ParseGltfMesh(content.data(),content.size(),untouched,error),"unregistered external buffers rejected clearly");
 Scene scene,round;Check(LoadSceneFromFile("tests/fixtures/animation/scene.judas",scene,error),"ordinary authored animation scene loads");if(failures){std::puts(error.c_str());return 1;}std::string serialized;SaveSceneToString(scene,serialized);Check(LoadSceneFromString(serialized,round,error)&&ScenesEqual(scene,round),"animation configuration serialization round-trip");std::string before,after;ComputeSceneFingerprint(scene,before,error);round.Find(10)->animation->speed=2;ComputeSceneFingerprint(round,after,error);Check(before!=after,"authored playback contributes to strict content fingerprint");
 std::mutex mutex;std::set<std::thread::id> decode,upload;std::set<unsigned> live;auto owner=std::this_thread::get_id();EngineHost host;
 Check(host.Init("M46 skinning",320,240,false,error,[&](const ResourceTraceEvent& event){std::lock_guard<std::mutex> guard(mutex);if(event.point==ResourceTracePoint::DecodeBegin)decode.insert(event.thread);if(event.point==ResourceTracePoint::MeshCreated){upload.insert(event.thread);live.insert(event.handle);}if(event.point==ResourceTracePoint::MeshDestroyed)live.erase(event.handle);}),"real GL EngineHost initializes");if(failures)return 1;
 host.OpenProjectAssets(fs::absolute("tests/fixtures/animation").string(),fs::absolute("tests/fixtures/animation/Assets").string());auto& resources=host.Resources();Check(!resources.BlockingMode(),"normal resource pipeline remains asynchronous");auto handle=resources.GetMesh("46464646464646464646464646464601",error);resources.WaitForAll();handle=resources.GetMesh("46464646464646464646464646464601",error);
 Check(handle.IsValid()&&resources.TryGetSkeletal("46464646464646464646464646464601"),"normal async decode and GPU handoff retain immutable skeleton");if(!handle.IsValid()){std::puts(error.c_str());return 1;}Check(!decode.empty()&&!decode.count(owner)&&upload==std::set<std::thread::id>{owner},"decode occurs on worker and GPU creation on context owner");
 auto& renderer=host.GetRenderer();auto frame=[&](const std::vector<glm::mat4>* pose,glm::quat rotation){renderer.BeginFrame(320,240);renderer.SetCamera(glm::lookAt(glm::vec3(0,1.5,6),glm::vec3(0,1.5,0),glm::vec3(0,1,0)),glm::ortho(-2.f,2.f,-1.7f,1.7f,.1f,20.f));renderer.SetLighting({0,1,0},{0,0,0},{1,1,1});renderer.DrawMesh(handle,{0,0,0},rotation,{1,1,1},{},{1,.4f,.1f},1,pose);renderer.EndFrame();std::vector<unsigned char> pixels;renderer.CaptureFrame(320,240,pixels);return pixels;};
 auto restPixels=frame(nullptr,{1,0,0,0});auto wave=ResolveSkinMatrices(s,SampleClip(s,asset.clips[0],.5));auto wavePixels=frame(&wave,{1,0,0,0});Check(restPixels!=wavePixels,"actual GPU palette changes rendered skin pixels");WriteRgbPng((out/"rest.png").string(),320,240,restPixels);WriteRgbPng((out/"wave.png").string(),320,240,wavePixels);auto rotated=frame(&wave,glm::angleAxis(.5f,glm::normalize(glm::vec3(1,2,3))));Check(rotated!=wavePixels,"arbitrary authored world rotation reaches normal skinned renderer");
 RuntimeWorld world;Check(world.Build(scene,&resources,error),"ordinary RuntimeWorld builds animation components");if(failures){std::puts(error.c_str());return 1;}world.UpdateAnimations(.25);auto* first=world.RuntimeAnimation(10);auto* second=world.RuntimeAnimation(11);Check(first&&second&&first->asset==second->asset&&first->playback.clip!=second->playback.clip,"runtime instances share asset but own final poses and players");Check(world.SetFinalPose(10,s.rest,error)&&world.AnimationSkin(10)->at(1)==rest[1],"external pose producer can resolve final pose independently of clip player");
 SceneTransform placement;placement.position={8,0,0};auto spawned=world.SpawnPrefab("46464646464646464646464646464603",placement,error);world.UpdateAnimations(.1);Check(spawned&&world.RuntimeAnimation(spawned)&&world.AnimationSkin(spawned),"runtime prefab creates ordinary animated instance");Check(world.DestroyHierarchy(spawned,error)&&!world.RuntimeAnimation(spawned),"destroyed animated prefab handle rejects access");world.Destroy();Check(!world.AnimationSkin(10),"world teardown releases pose state");Check(world.Build(scene,&resources,error),"world reconstruction after Stop");world.UpdateAnimations(0);Check(Near(world.RuntimeAnimation(10)->playback.time,0),"fresh Play has authored playback state");world.Destroy();
 auto start=std::chrono::steady_clock::now();for(int i=0;i<10000;++i){auto p=SampleClip(s,asset.clips[0],float(i%120)/60);auto m=ResolveSkinMatrices(s,p);if(m.empty())return 1;}auto cost=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/10000;
 renderer.FinishForDiagnostics();start=std::chrono::steady_clock::now();for(int i=0;i<20;++i){renderer.BeginFrame(320,240);renderer.SetCamera(glm::lookAt(glm::vec3(0,2,9),glm::vec3(0,1,0),glm::vec3(0,1,0)),glm::perspective(glm::radians(60.f),320.f/240,.1f,30.f));for(int n=0;n<8;++n)renderer.DrawMesh(handle,{float(n%4)-1.5f,0,float(n/4)*2},glm::quat(1,0,0,0),{.5,.5,.5},{},{1,.4,.1},1,&wave);renderer.EndFrame();}renderer.FinishForDiagnostics();double render=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20;
 std::printf("PERFORMANCE pose_sample_resolve_us=%.6f eight_instances_gl_frame_ms=%.6f frames=20\n",cost,render);
 host.Shutdown();Check(live.empty(),"normal shutdown balances skinned and static GPU mesh resources");std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
