#include "authoring/Tilemap.h"
#include "SpriteAnimation.h"
#include "SceneSerialization.h"
#include "Prefab.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneFingerprint.h"
#include "ScreenshotWriter.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "Project.h"
#include <thread>
#include <chrono>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool good,const std::string& label){++checks;std::printf("%s %s\n",good?"PASS":"FAIL",label.c_str());if(!good)throw std::runtime_error(label);}
int main(int argc,char** argv){try{
 const auto root=fs::absolute(argc>1?argv[1]:".cache/tilemap-tests");fs::create_directories(root/"Assets");std::string error;
 // Independently authored two-colour atlas: known frame identity and real pixels.
 std::vector<unsigned char> rgb(8*4*3);for(int y=0;y<4;++y)for(int x=0;x<8;++x){auto i=(y*8+x)*3;rgb[i]=x<4?255:0;rgb[i+1]=x<4?0:255;}
 Check(WriteRgbPng((root/"Assets/tiles.png").string(),8,4,rgb),"author independent atlas");AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord image,palette;
 auto track=[&](const fs::path& path,AssetRecord& out){auto relative=path.lexically_relative(root).generic_string();if(auto* a=assets.FindByRelativePath(relative)){out=*a;return true;}return assets.Track(path.string(),out,error);};
 Check(track(root/"Assets/tiles.png",image),"track atlas");SpriteSheet sheet;sheet.texture=image.id;sheet.width=8;sheet.height=4;sheet.pixelsPerUnit=4;sheet.frames={{"red","Red",{0,0,4,4},{.5,.5}},{"green","Green",{4,0,4,4},{.5,.5}}};
 Check(SaveSpriteSheet((root/"Assets/tiles.judassprite").string(),sheet,error)&&track(root/"Assets/tiles.judassprite",palette),"ordinary tracked sprite-sheet palette");
 if(argc>2&&std::string(argv[2])=="--cold"){
  SaveStorage storage((root/"ColdSlots").string());GameSnapshot snapshot;bool recovered=false;
  Check(storage.Read("tilemap",snapshot,recovered,error)&&!recovered,"new process reads durable modern tilemap save");
  Scene restored;Check(WorldPersistence::SceneFrom(snapshot.participants,restored,error),"new process reconstructs actual generated scene");
  ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.legacyGameplay=false;
  Check(world.Build(restored,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"native participants restore before continuation: "+error);
  SceneObjectId id=0;for(const auto& o:restored.Objects())if(o.name=="Native drop")id=o.id;Check(id!=0,"saved moving object retained stable identity");
  std::ifstream file(root/"endpoint.bin",std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(file),{}};SaveArchive expected(bytes);glm::vec3 position,velocity;expected(position,velocity);expected.Finish();
  const auto body=world.RuntimeBody(id);Check(world.Physics().GetTransform(body).position==position&&world.Physics().GetLinearVelocity(body)==velocity,"cold actual physical endpoint matches saved boundary exactly");
  world.Physics().ApplyLinearImpulse(body,{1,0,0});for(int i=0;i<30;++i){world.Physics().ApplyForce(body,{0,-9.81f,0});world.Physics().Step(1.f/60);}
  Check(world.Physics().GetTransform(body).position.x>position.x&&world.Physics().GetTransform(body).position.y>1.2f,"cold restored object really moves and collides with painted floor");
  std::printf("TILEMAP_COLD checks=%u failures=0\n",checks);return 0;
 }
 Scene source;auto owner=source.CreateObject("Map").id;TilemapRecipe r;r.root=owner;r.sheet=palette.id;
 Check(PaintTilemap(r,"ground",{-3,-1},{4,0},"red",false,false,false,error),"paint inclusive negative-coordinate rectangle");
 Scene scene;TilemapRecipe baked;Check(BakeTilemap(source,r,assets,scene,baked,error),"compile ordinary scene objects: "+error);
 size_t visuals=0,bodies=0;for(const auto& o:scene.Objects()){visuals+=bool(o.render);bodies+=bool(o.body);}
 Check(visuals==16&&bodies==4,"16 tiles produce four exact chunk-local rectangles across negative chunk boundaries");
 std::string text;SaveSceneToString(scene,text);Scene round;Check(LoadSceneFromString(text,round,error)&&ScenesEqual(scene,round),"recipe and generated geometry survive scene serialization exactly");
 TilemapRecipe parsed;Check(ParseTilemap(scene.Settings().authoringRecipes.at(TilemapKey(owner)),parsed,error)&&EncodeTilemap(parsed)==EncodeTilemap(baked),"canonical recipe roundtrip");
 auto prior=text;Scene repeat;TilemapRecipe repeated;Check(BakeTilemap(scene,baked,assets,repeat,repeated,error)&&ScenesEqual(scene,repeat),"unchanged rebuild retains IDs and complete source");
 auto invalid=baked;const auto originalRecipe=EncodeTilemap(invalid);Check(!PaintTilemap(invalid,"ground",{-513,0},{0,0},"red",false,false,false,error)&&EncodeTilemap(invalid)==originalRecipe,"out-of-bounds paint rejects atomically");
 Check(!PaintTilemap(invalid,"ground",{0,0},{100,100},"red",false,false,false,error)&&EncodeTilemap(invalid)==originalRecipe,"oversized fill rejects before mutation");
 auto malformed=EncodeTilemap(baked);malformed.insert(malformed.size()-1,",\"unknown\":true");Check(!ParseTilemap(malformed,parsed,error),"unknown recipe fields reject");
 invalid=baked;invalid.cells.push_back(invalid.cells.front());Check(!ValidateTilemap(invalid,error),"duplicate cell negative control");
 invalid=baked;invalid.layers.push_back({"foreground","Foreground",0,2,true});invalid.cells.push_back({"foreground","green",0,0});Check(!ValidateTilemap(invalid,error),"overlapping solid layers cannot duplicate physical contacts");
 invalid=baked;invalid.cells[0].frame="removed-frame";Scene preserved=scene;TilemapRecipe unused;
 Check(!BakeTilemap(scene,invalid,assets,preserved,unused,error)&&ScenesEqual(scene,preserved),"missing frame fails without deleting the previous map");
 auto managed=baked.generated.front().entity;auto edited=scene;edited.Find(managed)->name="User edit";
 Check(!BakeTilemap(edited,baked,assets,preserved,unused,error),"manual generated-object changes are not overwritten");
 edited=scene;edited.CreateObject("Keep child").parent=managed;Check(!BakeTilemap(edited,baked,assets,preserved,unused,error),"user children cannot be removed by repainting");
 auto erased=baked;Check(PaintTilemap(erased,"ground",{-1,-1},{0,0},"",true,false,false,error)&&erased.cells.size()==12,"erase a real two-by-two opening");
 Check(BakeTilemap(scene,erased,assets,preserved,unused,error),"collision topology rebuild follows erasure");
 // Every output collider covers occupied cells, never the erased opening.
 bool exact=true;for(const auto& o:preserved.Objects())if(o.body){auto half=PlanarMassProperties(*o.body->planar).area;exact&=half>0;
  for(int y=-1;y<=0;++y)for(int x=-3;x<=4;++x){const auto p=glm::vec2(x+.5f,y+.5f)-glm::vec2(o.transform.position);auto box=o.body->planar->vertices;
   bool inside=p.x>box[0].x&&p.x<box[2].x&&p.y>box[0].y&&p.y<box[2].y;if(x>=-1&&x<=0)exact&=!inside;}}
 Check(exact,"merged collision rectangles do not bridge the excavated hole");
 // Real native bodies, impulses, gravity and contact. No screen boundary.
 ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.legacyGameplay=false;Scene physical=scene;
 auto& drop=physical.CreateObject("Native drop");drop.transform.position={2,4,0};drop.body=SceneBodyComponent{};drop.body->shape=SceneShape::Planar;drop.body->motion=SceneBodyMotion::Dynamic;drop.body->planar=PlanarBox({.3,.3});const auto dropId=drop.id;
 Check(world.Build(physical,&resources,error),"compiled map builds in ordinary runtime: "+error);auto body=world.RuntimeBody(dropId);
 for(int i=0;i<180;++i){world.Physics().ApplyForce(body,{0,-9.81f,0});world.Physics().Step(1.f/60);}
 const auto pose=world.Physics().GetTransform(body);Check(pose.position.y>1.2f&&pose.position.y<1.4f,"falling body rests on actual painted floor");
 Scene hole=preserved;auto& falling=hole.CreateObject("Hole negative control");falling.transform.position={-.5f,4,0};falling.body=physical.Find(dropId)->body;const auto holeId=falling.id;
 RuntimeWorld holeWorld;holeWorld.legacyGameplay=false;Check(holeWorld.Build(hole,&resources,error),"erased opening builds independent physical negative control");
 for(int i=0;i<180;++i){holeWorld.Physics().ApplyForce(holeWorld.RuntimeBody(holeId),{0,-9.81f,0});holeWorld.Physics().Step(1.f/60);}
 Check(holeWorld.Physics().GetTransform(holeWorld.RuntimeBody(holeId)).position.y < -5,"erasing tiles removes real collision; body falls through actual hole");
 auto tilted=physical;const auto rotation=glm::angleAxis(.63f,glm::normalize(glm::vec3(1,2,3)));tilted.Find(owner)->transform.rotation=rotation;
 tilted.Find(dropId)->transform.position=rotation*tilted.Find(dropId)->transform.position;tilted.Find(dropId)->transform.rotation=rotation;
 RuntimeWorld rotated;rotated.legacyGameplay=false;Check(rotated.Build(tilted,&resources,error),"rotated grid and body share native plane");
 for(int i=0;i<180;++i){rotated.Physics().ApplyForce(rotated.RuntimeBody(dropId),rotation*glm::vec3(0,-9.81f,0));rotated.Physics().Step(1.f/60);}
 const auto local=glm::inverse(rotation)*rotated.Physics().GetTransform(rotated.RuntimeBody(dropId)).position;
 Check(std::abs(local.y-pose.position.y)<.001f&&std::abs(local.z)<.0001f,"rotated gravity preserves local landing and actual planar mobility");
 GameSnapshot snapshot;snapshot.project="tilemap-proof";snapshot.scene="main";snapshot.content=std::string(64,'a');
 Check(WorldPersistence::Capture(world,snapshot.participants,error),"capture real tilemap physics participants");SaveStorage storage((root/"ColdSlots").string());Check(storage.Write("tilemap",snapshot,error),"publish durable cold-process fixture");
 SaveArchive endpoint;auto expectedPosition=world.Physics().GetTransform(body).position,expectedVelocity=world.Physics().GetLinearVelocity(body);endpoint(expectedPosition,expectedVelocity);std::ofstream(root/"endpoint.bin",std::ios::binary)<<endpoint.bytes;
 Project project;const auto projectFile=root/"Tilemap_Test.judasproj";
 if(fs::exists(projectFile))Check(project.Load(projectFile.string(),error),"reopen stream fixture project");else Check(Project::CreateNew(root.string(),"Tilemap Test",project,error),"create stream fixture project");
 fs::create_directories(root/"Scenes");Check(SaveSceneToFile(physical,(root/"Scenes/tiles.judas").string(),error),"write ordinary region with generated tile geometry");
 JobSystem jobs(2);ResourceManager streamedResources(nullptr,&assets,&jobs);RuntimeWorld streamWorld;streamWorld.legacyGameplay=false;Scene empty;Check(streamWorld.Build(empty,&streamedResources,error),"streaming root has no hidden tile system");
 WorldManifest manifest;WorldRegion region;region.id="tiles";region.scene="Scenes/tiles.judas";manifest.regions.emplace(region.id,region);WorldStreaming stream(streamWorld,streamedResources,project,manifest);
 auto pump=[&](const std::function<bool()>& done){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(std::chrono::steady_clock::now()<deadline){streamedResources.Pump();stream.Advance(false);if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;};
 auto request=stream.Request("tiles",false,error);Check(request&&pump([&]{return stream.Status(request)&&stream.Status(request)->state=="active";}),"actual streaming publishes tilemap region");
 const auto live=stream.Resolve("tiles",dropId);const auto native=streamWorld.RuntimeBody(live);Check(native.IsValid()&&streamWorld.Physics().AliveBodyCount()==bodies+1,"streamed map has real merged collision and dynamic object");
 streamWorld.Physics().SetLinearVelocity(native,{.5f,0,0});const auto savedPose=streamWorld.Physics().GetTransform(native);
 stream.Release(request);Check(pump([&]{return stream.Regions()[0].state=="unloaded";})&&streamWorld.Physics().AliveBodyCount()==0,"suspended map removes all actual region bodies");
 request=stream.Request("tiles",false,error);Check(request&&pump([&]{return stream.Status(request)&&stream.Status(request)->state=="active";}),"region revisit rebuilds real collision");
 const auto fresh=streamWorld.RuntimeBody(stream.Resolve("tiles",dropId));Check(fresh.IsValid()&&streamWorld.Physics().GetTransform(fresh).position==savedPose.position&&streamWorld.Physics().GetLinearVelocity(fresh).x==.5f,"regional suspension preserves physical endpoint and momentum");
 Scene prefab;Check(CreatePrefab(scene,owner,prefab,error)&&ValidatePrefab(prefab,error),"compiled tilemap is an ordinary valid prefab subtree");
 Scene instances;SceneObjectId instance=0;SceneTransform place;place.position={10,0,0};Check(InstantiatePrefab(instances,prefab,"12345678123456781234567812345678",place,instance,error),"compiled prefab instantiates through existing stable-ID path");
 auto scaled=scene;scaled.Find(owner)->transform.scale={2,1,1};Check(!BakeTilemap(scaled,baked,assets,preserved,unused,error),"physical grid resize requires cell dimensions, not silent scaled colliders");
 std::printf("TILEMAP checks=%u failures=0\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
