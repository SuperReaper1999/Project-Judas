#include "authoring/Tilemap.h"
#include "SpriteAnimation.h"
#include "SceneSerialization.h"
#include "Prefab.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "ResourceManager.h"
#include "SceneFingerprint.h"
#include "ScreenshotWriter.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "Project.h"
#include <algorithm>
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
bool SolidCell(RuntimeWorld& world,int x,int y){
 const auto hit=world.Physics().CastPlanar(Shape::Planar(PlanarBox({.1,.1})),
     {{x+.5f,y+.5f,0},{1,0,0,0}},{1,0,0},0);
 return hit.hit&&hit.initialOverlap;
}
void CheckIncrementalEdits(const AssetDatabase& assets,const AssetId& palette){
 std::string error;Scene source;const auto owner=source.CreateObject("Incremental grid").id;
 TilemapRecipe recipe;recipe.root=owner;recipe.sheet=palette;
 recipe.layers.push_back({"decor","Decoration",1,0,false});
 for(int y=-1;y<=1;++y)for(int x=-18;x<=18;++x)recipe.cells.push_back({"ground","red",x,y});
 recipe.cells.push_back({"decor","red",1,3});
 Scene authored;TilemapRecipe baked;
 Check(BakeTilemap(source,recipe,assets,authored,baked,error),"compile cross-chunk incremental fixture");
 ResourceManager resources(nullptr,&assets);RuntimeWorld world;
 Check(world.Build(authored,&resources,error),"build script-free incremental world");
 auto read=[&](){TilemapRecipe r;Check(ParseTilemap(world.Settings().authoringRecipes.at(TilemapKey(owner)),r,error),"read incremental recipe");return r;};
 auto entry=[&](const TilemapRecipe& r,const std::string& key){
  auto it=std::find_if(r.generated.begin(),r.generated.end(),[&](const auto& g){return g.key==key;});
  if(it==r.generated.end())throw std::runtime_error("missing generated key: "+key);
  return *it;
 };
 const auto untouched=entry(baked,"collision/ground/16/-1/3/1");
 const auto untouchedBody=world.RuntimeBody(untouched.entity);
 const auto copies=world.MetadataStats().definitionsCopied;
 const auto atlas=world.RuntimeDefinition(entry(baked,"tile/decor/1/3").entity)->render->textureAsset;
 const auto atlasRefs=resources.RefCount(atlas);
 Check(world.EditTilemapCell(owner,"decor",{1,3},"green",true,true,error),"decorative edit succeeds without scripts");
 auto current=read();
 bool retained=true;
 for(const auto& old:baked.generated)if(old.key!="tile/decor/1/3"){
  auto now=entry(current,old.key);retained&=now.entity==old.entity&&now.hash==old.hash;
 }
 Check(retained&&world.RuntimeBody(untouched.entity).id==untouchedBody.id,"decorative edit retains all other sprites and actual collider handles");
 Check(world.MetadataStats().definitionsCopied==copies,"decorative edit performs zero whole-world definition copies");
 Check(resources.RefCount(atlas)==atlasRefs,"replacement transfers rather than accumulates atlas references");
 const auto afterDecor=EncodeTilemap(current);const auto nextId=world.NextRuntimeEntityId();
 Check(world.EditTilemapCell(owner,"decor",{1,3},"green",true,true,error)&&
       world.NextRuntimeEntityId()==nextId&&world.Settings().authoringRecipes.at(TilemapKey(owner))==afterDecor,
       "identical edit is identity/allocation stable");
 bool repeated=true;
 for(int i=0;i<40;++i)repeated&=world.EditTilemapCell(owner,"decor",{1,3},i%2?"green":"red",true,true,error);
 Check(repeated&&resources.RefCount(atlas)==atlasRefs,"repeated tile replacement keeps resource demand bounded");
 // Topology changes on both sides of positive and negative chunk boundaries.
 for(const auto cell:std::vector<glm::ivec2>{{-17,-1},{-16,-1},{-1,0},{0,0},{15,1},{16,1}}){
  for(bool erase:{true,false}){
   current=read();auto expected=current;
   Check(PaintTilemap(expected,"ground",cell,cell,erase?"":"green",erase,false,true,error),"prepare independent authoring edit");
   Scene before;
   before.Settings()=world.Settings();
   for(const auto& object:world.ScriptObjects())before.InsertObject(object);
   Scene oracle;TilemapRecipe rebuilt;
   Check(BakeTilemap(before,expected,assets,oracle,rebuilt,error,true),"whole-map authoring oracle accepts same edit: "+error);
   if(!world.EditTilemapCell(owner,"ground",cell,erase?"":"green",false,true,error))throw std::runtime_error(error);
   const auto actual=read();bool equivalent=actual.cells.size()==rebuilt.cells.size()&&actual.generated.size()==rebuilt.generated.size();
   for(const auto& generated:rebuilt.generated){
    const auto live=entry(actual,generated.key);
    auto expectedObject=*oracle.Find(generated.entity);expectedObject.id=live.entity;
    equivalent&=live.hash==TilemapObjectFingerprint(expectedObject);
   }
   Check(equivalent,"incremental sprites and merged collision equal whole-map authoring output");
   bool occupancy=true;
   for(int y=-1;y<=1;++y)for(int x=-18;x<=18;++x)
    occupancy&=SolidCell(world,x,y)==!(erase&&glm::ivec2(x,y)==cell);
   Check(occupancy,"real MK2 collision matches every occupied and erased cell across chunk boundaries");
  }
 }
 // Invalid requests preserve the exact durable recipe, handles and allocator.
 const auto stable=world.Settings().authoringRecipes.at(TilemapKey(owner));
 const auto allocation=world.NextRuntimeEntityId();
 Check(!world.EditTilemapCell(owner,"ground",{513,0},"green",false,false,error)&&
       !world.EditTilemapCell(owner,"missing",{0,0},"green",false,false,error)&&
       !world.EditTilemapCell(owner,"ground",{0,0},"missing",false,false,error)&&
       world.Settings().authoringRecipes.at(TilemapKey(owner))==stable&&
       world.NextRuntimeEntityId()==allocation&&world.RuntimeBody(untouched.entity).id==untouchedBody.id,
       "invalid coordinate/layer/frame leave recipe, allocator and unrelated collision untouched");
 // A manually edited destination must not be silently overwritten.
 const auto target=entry(read(),"tile/decor/1/3").entity;
 auto sprite=world.RuntimeDefinition(target)->render->sprite;sprite.size.x*=2;
 Check(world.SetSprite(target,sprite)&&!world.EditTilemapCell(owner,"decor",{1,3},"red",false,false,error)&&
       world.Settings().authoringRecipes.at(TilemapKey(owner))==stable,"manual target changes reject atomically");
 // Checking the ownership guard requires a fresh world with the original hash.
 RuntimeWorld guarded;Check(guarded.Build(authored,&resources,error),"build ownership negative control");
 SceneObject child;child.name="User child";child.parent=entry(baked,"tile/decor/1/3").entity;
 Check(guarded.CreateEntity(child,nullptr,&error)!=0,"attach a real child to a managed sprite");
 Check(!guarded.EditTilemapCell(owner,"decor",{1,3},"",false,false,error)&&
       guarded.Settings().authoringRecipes.at(TilemapKey(owner))==EncodeTilemap(baked),"erasure cannot delete a user's child");
 auto jointScene=authored;auto& moving=jointScene.CreateObject("Jointed planar actor");
 moving.transform.position={20,5,0};moving.body=SceneBodyComponent{};
 moving.body->shape=SceneShape::Planar;moving.body->motion=SceneBodyMotion::Dynamic;
 moving.body->planar=PlanarCircle(.25);const auto movingId=moving.id;
 auto& joint=jointScene.CreateObject("Tile anchor");joint.joint=SceneJointComponent{};
 joint.joint->bodyA=movingId;joint.joint->bodyB=untouched.entity;joint.joint->settings.type=JointType::PlanarPin;
 RuntimeWorld jointed;Check(jointed.Build(jointScene,&resources,error),"build real joint anchored to a tile collider: "+error);
 Check(!jointed.EditTilemapCell(owner,"ground",{16,-1},"",false,false,error)&&
       error.find("joint anchor")!=std::string::npos&&
       jointed.Settings().authoringRecipes.at(TilemapKey(owner))==EncodeTilemap(baked)&&SolidCell(jointed,16,-1),
       "collision edit rejects removing a live joint anchor without changing water-tight collision");
 auto translated=authored;translated.Find(owner)->transform.position={4,8,0};
 RuntimeWorld offset;Check(offset.Build(translated,&resources,error),"build translated grid frame");
 Check(offset.EditTilemapCell(owner,"ground",{-17,-1},"",false,false,error)&&
       !SolidCell(offset,-13,7)&&SolidCell(offset,-12,7),"incremental collision respects the grid's world translation");
}
int main(int argc,char** argv){try{
 const auto root=fs::absolute(argc>1?argv[1]:".cache/tilemap-tests");fs::create_directories(root/"Assets");std::string error;
 // Independently authored two-colour atlas: known frame identity and real pixels.
 std::vector<unsigned char> rgb(8*4*3);for(int y=0;y<4;++y)for(int x=0;x<8;++x){auto i=(y*8+x)*3;rgb[i]=x<4?255:0;rgb[i+1]=x<4?0:255;}
 Check(WriteRgbPng((root/"Assets/tiles.png").string(),8,4,rgb),"author independent atlas");AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord image,palette;
 auto track=[&](const fs::path& path,AssetRecord& out){auto relative=path.lexically_relative(root).generic_string();if(auto* a=assets.FindByRelativePath(relative)){out=*a;return true;}return assets.Track(path.string(),out,error);};
 Check(track(root/"Assets/tiles.png",image),"track atlas");SpriteSheet sheet;sheet.texture=image.id;sheet.width=8;sheet.height=4;sheet.pixelsPerUnit=4;sheet.frames={{"red","Red",{0,0,4,4},{.5,.5}},{"green","Green",{4,0,4,4},{.5,.5}}};
 Check(SaveSpriteSheet((root/"Assets/tiles.judassprite").string(),sheet,error)&&track(root/"Assets/tiles.judassprite",palette),"ordinary tracked sprite-sheet palette");
 if(argc>2&&(std::string(argv[2])=="--benchmark"||std::string(argv[2])=="--benchmark-topology")){
  // Identical single-cell edits with and without unrelated world content.
  // Include the complete native call, not just the compiler's inner loop.
  for(int unrelated:{0,2000})for(bool collision:{false,true}){
   Scene source;auto owner=source.CreateObject("Benchmark map").id;
   TilemapRecipe recipe;recipe.root=owner;recipe.sheet=palette.id;
   recipe.layers={{"ground","Ground",0,0,true},{"soil","Soil",1,0,false},
                  {"crops","Crops",2,0,false},{"detail","Detail",3,0,false},
                  {"overlay","Overlay",4,0,false}};
   for(int y=0;y<14;++y)for(int x=0;x<20;++x){
    recipe.cells.push_back({"ground","red",x,y});
    if(y*20+x<190)recipe.cells.push_back({recipe.layers[1+(y*20+x)%4].id,"red",x,y});
   }
   for(int i=0;i<unrelated;++i)source.CreateObject("Unrelated "+std::to_string(i));
   Scene compiled;TilemapRecipe baked;
   Check(BakeTilemap(source,recipe,assets,compiled,baked,error),"compile 470-cell benchmark: "+error);
   ResourceManager resources(nullptr,&assets);RuntimeWorld world;
   Check(world.Build(compiled,&resources,error),"build benchmark world: "+error);
   std::vector<double> samples;
   const auto copied=world.MetadataStats().definitionsCopied;
   const bool topology=std::string(argv[2])=="--benchmark-topology";
   for(int i=0;i<44;++i){
    const auto start=std::chrono::steady_clock::now();
    const bool ok=world.EditTilemapCell(owner,collision?"ground":"soil",{10,7},
          i%2?"red":topology?"":"green",false,false,error);
    Check(ok,"benchmark edit: "+error);
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    if(i>=4)samples.push_back(ms);
   }
   double sum=0;for(auto ms:samples)sum+=ms;std::sort(samples.begin(),samples.end());
   std::printf("TILE_EDIT_BENCH unrelated=%d collision=%d topology=%d samples=%zu mean_ms=%.6f median_ms=%.6f p95_ms=%.6f worst_ms=%.6f definitions_copied=%llu\n",
       unrelated,int(collision),int(topology),samples.size(),sum/samples.size(),samples[samples.size()/2],
       samples[size_t(std::ceil(samples.size()*.95))-1],samples.back(),
       (unsigned long long)(world.MetadataStats().definitionsCopied-copied));
  }
  return 0;
 }
 if(argc>2&&std::string(argv[2])=="--cold"){
  SaveStorage storage((root/"ColdSlots").string());GameSnapshot snapshot;bool recovered=false;
  Check(storage.Read("tilemap",snapshot,recovered,error)&&!recovered,"new process reads durable modern tilemap save");
  Scene restored;Check(WorldPersistence::SceneFrom(snapshot.participants,restored,error),"new process reconstructs actual generated scene");
  ResourceManager resources(nullptr,&assets);RuntimeWorld world;
  Check(world.Build(restored,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"native participants restore before continuation: "+error);
  SceneObjectId id=0;for(const auto& o:restored.Objects())if(o.name=="Native drop")id=o.id;Check(id!=0,"saved moving object retained stable identity");
  std::ifstream file(root/"endpoint.bin",std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(file),{}};SaveArchive expected(bytes);glm::vec3 position,velocity;expected(position,velocity);expected.Finish();
  const auto body=world.RuntimeBody(id);Check(world.Physics().GetTransform(body).position==position&&world.Physics().GetLinearVelocity(body)==velocity,"cold actual physical endpoint matches saved boundary exactly");
  world.Physics().ApplyLinearImpulse(body,{1,0,0});for(int i=0;i<30;++i){world.Physics().ApplyForce(body,{0,-9.81f,0});world.Physics().Step(1.f/60);}
  Check(world.Physics().GetTransform(body).position.x>position.x&&world.Physics().GetTransform(body).position.y>1.2f,"cold restored object really moves and collides with painted floor");
  Check(!SolidCell(world,-1,0)&&SolidCell(world,10,0),"cold save restores erased and added collision");
  TilemapRecipe restoredRecipe;
  Check(ParseTilemap(world.Settings().authoringRecipes.at(TilemapKey(1)),restoredRecipe,error)&&
        std::any_of(restoredRecipe.cells.begin(),restoredRecipe.cells.end(),[](const auto& cell){
            return cell.x==10&&cell.y==0&&cell.frame=="green";
        }),"cold save restores edited tile recipe and palette frame");
  Check(world.EditTilemapCell(1,"ground",{10,0},"",false,false,error)&&!SolidCell(world,10,0),
        "cold-restored grid remains editable: "+error);
  auto decoration=std::find_if(restoredRecipe.generated.begin(),restoredRecipe.generated.end(),[](const auto& g){return g.key=="tile/decor/5/2";});
  Check(decoration!=restoredRecipe.generated.end()&&world.RuntimeDefinition(decoration->entity)->render->sprite.flipY,
        "cold process retains decorative tile frame/flip and generated identity");
  Check(world.EditTilemapCell(1,"decor",{5,2},"",false,false,error)&&!world.RuntimeDefinition(decoration->entity),
        "cold-restored decoration can be removed through incremental editing");
  std::printf("TILEMAP_COLD checks=%u failures=0\n",checks);return 0;
 }
 // Shipped editable examples must not retain fingerprints from retired scene
 // fields. This checks real repository content, not freshly generated fixtures.
 for(const char* relative:{"beacon_grove/Scenes/main.judas","tilemap_workshop/Scenes/main.judas",
                           "tilemap_workshop/Scenes/village.judas"}){
  Scene example;const auto path=fs::path(__FILE__).parent_path().parent_path()/"projects"/relative;
  Check(LoadSceneFromFile(path.string(),example,error),std::string("load retained tilemap example: ")+relative);
  bool current=true;
  for(const auto& [key,text]:example.Settings().authoringRecipes)if(key.rfind("tilemap.",0)==0){
   TilemapRecipe map;current&=ParseTilemap(text,map,error);
   for(const auto& child:map.generated){const auto* object=example.Find(child.entity);
    current&=object&&TilemapObjectFingerprint(*object)==child.hash;
   }
  }
  Check(current,std::string("retained example generated fingerprints match current authored objects: ")+relative);
 }
 CheckIncrementalEdits(assets,palette.id);
 Scene source;auto owner=source.CreateObject("Map").id;TilemapRecipe r;r.root=owner;r.sheet=palette.id;
 r.layers.push_back({"decor","Decoration",1,0,false});
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
 ResourceManager resources(nullptr,&assets);RuntimeWorld world;Scene physical=scene;
 auto& drop=physical.CreateObject("Native drop");drop.transform.position={2,4,0};drop.body=SceneBodyComponent{};drop.body->shape=SceneShape::Planar;drop.body->motion=SceneBodyMotion::Dynamic;drop.body->planar=PlanarBox({.3,.3});const auto dropId=drop.id;
 const auto editsFile=root/"Assets/runtime-edits.js";
 std::ofstream(editsFile)<<R"JS(export default class {
  constructor({entity}) {this.entity=entity;this.state={};}
  fixedUpdate() {
   if(this.state.done)return;
   this.state.erased=this.entity.setTile('ground',-1,0,null);
   this.state.placed=this.entity.setTile('ground',10,0,'green',{flipX:true});
   this.state.decorated=this.entity.setTile('decor',5,2,'green',{flipY:true});
   try {this.entity.setTile('ground',10,0,'not-in-the-palette');}
   catch {this.state.rejected=true;}
   this.state.done=true;
  }
 })JS";
 AssetRecord editsScript;
 Check(track(editsFile,editsScript),"register runtime tile edits script");
 physical.Find(owner)->scripts.push_back({1,editsScript.id,true,"{}"});
 Check(world.Build(physical,&resources,error),"compiled map builds in ordinary runtime: "+error);auto body=world.RuntimeBody(dropId);
 for(int i=0;i<180;++i){world.Physics().ApplyForce(body,{0,-9.81f,0});world.Physics().Step(1.f/60);}
 const auto pose=world.Physics().GetTransform(body);Check(pose.position.y>1.2f&&pose.position.y<1.4f,"falling body rests on actual painted floor");
 Scene hole=preserved;auto& falling=hole.CreateObject("Hole negative control");falling.transform.position={-.5f,4,0};falling.body=physical.Find(dropId)->body;const auto holeId=falling.id;
 RuntimeWorld holeWorld;Check(holeWorld.Build(hole,&resources,error),"erased opening builds independent physical negative control");
 for(int i=0;i<180;++i){holeWorld.Physics().ApplyForce(holeWorld.RuntimeBody(holeId),{0,-9.81f,0});holeWorld.Physics().Step(1.f/60);}
 Check(holeWorld.Physics().GetTransform(holeWorld.RuntimeBody(holeId)).position.y < -5,"erasing tiles removes real collision; body falls through actual hole");
 auto tilted=physical;const auto rotation=glm::angleAxis(.63f,glm::normalize(glm::vec3(1,2,3)));tilted.Find(owner)->transform.rotation=rotation;
 tilted.Find(dropId)->transform.position=rotation*tilted.Find(dropId)->transform.position;tilted.Find(dropId)->transform.rotation=rotation;
 RuntimeWorld rotated;Check(rotated.Build(tilted,&resources,error),"rotated grid and body share native plane");
 for(int i=0;i<180;++i){rotated.Physics().ApplyForce(rotated.RuntimeBody(dropId),rotation*glm::vec3(0,-9.81f,0));rotated.Physics().Step(1.f/60);}
 const auto local=glm::inverse(rotation)*rotated.Physics().GetTransform(rotated.RuntimeBody(dropId)).position;
 Check(std::abs(local.y-pose.position.y)<.001f&&std::abs(local.z)<.0001f,"rotated gravity preserves local landing and actual planar mobility");
 Check(SolidCell(world,-1,0)&&!SolidCell(world,10,0),"pre-edit native geometry has the opposite occupancy");
 world.FixedScripts(nullptr,1.f/60);
 const auto editStates=world.Scripts()->Capture();
 for(const auto& fault:world.Scripts()->Diagnostics())std::fprintf(stderr,"%s\n",fault.message.c_str());
 Check(editStates.size()==1&&editStates[0].json.find("\"erased\":true")!=std::string::npos&&
       editStates[0].json.find("\"placed\":true")!=std::string::npos&&
       editStates[0].json.find("\"rejected\":true")!=std::string::npos,
       "JudasJS erases and places tiles and rejects unknown frames");
 Check(!SolidCell(world,-1,0)&&SolidCell(world,10,0),"runtime edits change actual planar collision immediately");
 TilemapRecipe runtimeRecipe;
 Check(ParseTilemap(world.Settings().authoringRecipes.at(TilemapKey(owner)),runtimeRecipe,error),
       "updated live recipe is valid");
 const auto created=std::find_if(runtimeRecipe.generated.begin(),runtimeRecipe.generated.end(),[](const auto& entry){
     return entry.key=="tile/ground/10/0";
 });
 Check(created!=runtimeRecipe.generated.end()&&world.RuntimeDefinition(created->entity)->render->sprite.flipX&&
       world.RuntimeDefinition(created->entity)->render->sprite.rect.x==.5f,
       "runtime sprite publishes the selected frame and flip");
 SceneObject liveDrop;
 liveDrop.name="Drop above the runtime tile";
 liveDrop.transform.position={10.5f,4,0};
 liveDrop.body=physical.Find(dropId)->body;
 const auto liveDropId=world.CreateEntity(liveDrop,nullptr,&error);
 const auto liveDropBody=world.RuntimeBody(liveDropId);
 Check(liveDropBody.IsValid(),"create real planar drop above the new tile");
 for(int i=0;i<180;++i){
    world.Physics().ApplyForce(liveDropBody,{0,-9.81f,0});
    world.Physics().Step(1.f/60);
 }
 Check(std::abs(world.Physics().GetTransform(liveDropBody).position.y-1.3f)<.1f,
       "a falling rigid body lands on the runtime-painted tile");
 GameSnapshot snapshot;snapshot.project="tilemap-proof";snapshot.scene="main";snapshot.content=std::string(64,'a');
 Check(WorldPersistence::Capture(world,snapshot.participants,error),"capture real tilemap physics participants");SaveStorage storage((root/"ColdSlots").string());Check(storage.Write("tilemap",snapshot,error),"publish durable cold-process fixture");
 SaveArchive endpoint;auto expectedPosition=world.Physics().GetTransform(body).position,expectedVelocity=world.Physics().GetLinearVelocity(body);endpoint(expectedPosition,expectedVelocity);std::ofstream(root/"endpoint.bin",std::ios::binary)<<endpoint.bytes;
 Project project;const auto projectFile=root/"Tilemap_Test.judasproj";
 if(fs::exists(projectFile))Check(project.Load(projectFile.string(),error),"reopen stream fixture project");else Check(Project::CreateNew(root.string(),"Tilemap Test",project,error),"create stream fixture project");
 // This original streaming control owns a precompiled, non-scripted grid.
 Scene streamFixture=physical;
 streamFixture.Find(owner)->scripts.clear();
 fs::create_directories(root/"Scenes");Check(SaveSceneToFile(streamFixture,(root/"Scenes/tiles.judas").string(),error),"write ordinary region with generated tile geometry");
 JobSystem jobs(2);ResourceManager streamedResources(nullptr,&assets,&jobs);RuntimeWorld streamWorld;Scene empty;Check(streamWorld.Build(empty,&streamedResources,error),"streaming root has no hidden tile system");
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
