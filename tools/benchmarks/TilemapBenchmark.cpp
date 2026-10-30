#include "authoring/Tilemap.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <cmath>
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point at){return std::chrono::duration<double,std::milli>(Clock::now()-at).count();}
void Report(const char* label,unsigned count,std::vector<double> values){std::sort(values.begin(),values.end());std::printf("%s,%u,%zu,%.6f,%.6f,%.6f\n",label,count,values.size(),values[values.size()/2],values[size_t(std::ceil(values.size()*.95))-1],values.back());}
int main(int argc,char** argv){try{
 if(argc!=2)return 2;
 Project project;std::string error;if(!project.Load(argv[1],error))throw std::runtime_error(error);
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());AssetId sheet;for(const auto& [id,a]:assets.Records())if(a.type==AssetType::SpriteSheet){sheet=id;break;}
 for(unsigned count:{128u,512u,2048u}){
  Scene source;auto owner=source.CreateObject("Grid").id;TilemapRecipe r;r.root=owner;r.sheet=sheet;
  for(unsigned i=0;i<count;++i)r.cells.push_back({"ground","tile-0",int(i%64),int(i/64)});
  Scene compiled;TilemapRecipe baked;std::vector<double> fresh,edit;
  for(unsigned i=0;i<6;++i){auto started=Clock::now();if(!BakeTilemap(source,r,assets,compiled,baked,error))throw std::runtime_error(error);auto elapsed=Ms(started);if(i)fresh.push_back(elapsed);
   Scene rebuilt;TilemapRecipe updated;started=Clock::now();if(!BakeTilemap(compiled,baked,assets,rebuilt,updated,error))throw std::runtime_error(error);elapsed=Ms(started);if(i)edit.push_back(elapsed);}
  Report("fresh_bake_ms",count,fresh);Report("existing_rebuild_ms",count,edit);
  unsigned boxes=0;for(const auto& o:compiled.Objects())boxes+=bool(o.body);std::printf("geometry,%u,tiles=%zu,merged_static_boxes=%u\n",count,r.cells.size(),boxes);
  for(bool naive:{false,true}){
   Scene scene=compiled;
   if(naive){std::vector<SceneObjectId> remove;for(const auto& o:scene.Objects())if(o.body)remove.push_back(o.id);for(auto id:remove)scene.DestroyObject(id);
    for(auto& o:scene.Objects())if(o.render){o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarBox({.5,.5});o.body->friction=.7f;}}
   std::vector<SceneObjectId> ids;const float top=float(count/64);
   for(int i=0;i<8;++i){auto& o=scene.CreateObject("Dynamic probe");o.transform.position={1.5f+i*8,top+2,0};o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarCircle(.15);o.body->motion=SceneBodyMotion::Dynamic;ids.push_back(o.id);}
   ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.legacyGameplay=false;if(!world.Build(scene,&resources,error))throw std::runtime_error(error);
   std::vector<double> steps;for(int i=0;i<180;++i){for(auto id:ids)world.Physics().ApplyForce(world.RuntimeBody(id),{0,-9.81f,0});auto start=Clock::now();world.Physics().Step(1.f/60);if(i>=60)steps.push_back(Ms(start));}
   for(auto id:ids){const auto p=world.Physics().GetTransform(world.RuntimeBody(id)).position;if(std::abs(p.y-top-.15f)>.002f||std::abs(p.z)>.0001f)throw std::runtime_error("physical landing gate failed");}
   Report(naive?"per_tile_physics_ms":"merged_physics_ms",count,steps);
  }
 }
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
