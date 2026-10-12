// Audit-only actual imported articulation on a real cooked triangle-mesh floor.
// Uses unmodified RuntimeWorld / PhysicsWorld; no pose repair or forced sleeping.
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "CollisionAsset.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
int main(int argc,char**argv){
 if(argc!=2)return 2;namespace fs=std::filesystem;fs::path out=argv[1];fs::create_directories(out);
 std::string error;Scene original;Project project;
 if(!LoadSceneFromFile("projects/post_m65_consumers/Scenes/skate/park.judas",original,error)||!project.Load("projects/post_m65_consumers/post_m65_consumers.judasproj",error))return 3;
 auto rig=*original.Find(12);rig.scripts.clear();EngineHost host;
 if(!host.Init("M78 isolated mesh contact",320,240,false,error))return 4;
 host.GetWindow().SetTestInputMode(true);host.OpenProjectAssets(project.RootDir(),project.AssetsDir());
 host.Resources().GetMesh(rig.render->meshAsset,error);host.Resources().WaitForAll();
 MeshData mesh;constexpr unsigned n=64;for(unsigned z=0;z<=n;++z)for(unsigned x=0;x<=n;++x){MeshVertex v;v.position={float(x)*60/n-30,0,float(z)*60/n-30};mesh.vertices.push_back(v);}
 for(unsigned z=0;z<n;++z)for(unsigned x=0;x<n;++x){unsigned a=z*(n+1)+x,b=a+1,c=a+n+1,d=c+1;mesh.indices.insert(mesh.indices.end(),{a,c,b,b,c,d});}
 CollisionAsset cooked;CollisionCookSettings settings;
 if(!CookCollision(mesh,settings,cooked,error)){std::cerr<<error<<'\n';return 5;}
 auto asset=std::make_shared<CollisionAsset>(std::move(cooked));bool sane=true;
 for(bool useMesh:{false,true})for(int count:{1,4}){
  Scene scene;scene.Settings()=original.Settings();auto& g=scene.CreateObject("generic gravity");g.gravity=SceneGravityComponent{};g.gravity->kind=SceneGravityKind::Uniform;g.gravity->regionShape=SceneRegionShape::Box;g.gravity->regionHalfExtents={1000,1000,1000};
  std::vector<EntityId> owners;for(int k=0;k<count;++k){auto r=rig;r.id=200+k;r.transform.position={85.f+k*3,2,0};scene.Objects().push_back(r);owners.push_back(r.id);}scene.SetNextId(500);
  RuntimeWorld world;world.legacyGameplay=false;if(!world.Build(scene,&host.Resources(),error,&project.Settings().classification)){std::cerr<<error;return 6;}
  host.Resources().WaitForAll();for(int k=0;k<60;++k)world.UpdateAnimations(1.f/60);
  BodyHandle floor=useMesh?world.Physics().CreateShape(Shape::Cooked(asset,"m78-owned-grid"),BodyTransform{{90,0,0},glm::quat(1,0,0,0)},false,0,.8f,0):world.Physics().CreateStaticBox({90,-.5f,0},{30,.5f,30},.8f,0);
  if(!floor.IsValid())return 7;for(auto id:owners)if(!world.EnterRagdoll(id,error)){std::cerr<<error;return 8;}
  GameSession game;if(!game.Begin(world,error))return 9;
  std::vector<double> times;unsigned contacts=0,limits=0;size_t searches=0;double lowest=1e9;unsigned sleeping=0;
  std::string name=std::string(useMesh?"mesh":"box")+"-"+std::to_string(count);std::ofstream trace(out/(name+".csv"));trace<<"step,wall_ms,contacts,ccd_queries,ccd_iterations,ccd_limit,sampling_fallback,geometry_predicates\n";
  for(int k=0;k<900;++k){auto begin=std::chrono::steady_clock::now();StepPlayedWorld(game,host.GetWindow(),1.f/60);double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();times.push_back(ms);auto& s=world.Physics().LastStepStats();contacts+=unsigned(s.contactPoints);limits+=unsigned(s.impactSearchLimit);searches+=s.impactSearchIterations;trace<<k<<','<<ms<<','<<s.contactPoints<<','<<s.impactQueries<<','<<s.impactSearchIterations<<','<<s.impactSearchLimit<<','<<s.impactSamplingFallbacks<<','<<s.geometryPredicates<<'\n';
   for(auto id:owners)for(auto& b:rig.ragdoll->bones){auto h=world.RuntimeBody(world.RagdollBody(id,b.joint));auto p=world.Physics().GetTransform(h).position;lowest=std::min(lowest,double(p.y));sane&=std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&p.y>-.5f;}
  }
  for(auto id:owners)for(auto& b:rig.ragdoll->bones)sleeping+=world.Physics().IsSleeping(world.RuntimeBody(world.RagdollBody(id,b.joint)));
  double first=*std::max_element(times.begin(),times.begin()+120);std::sort(times.begin(),times.end());
  sane&=contacts>0;std::cout<<std::setprecision(9)<<"{\"case\":\""<<name<<"\",\"steps\":900,\"hz\":60,\"rigBodies\":"<<count*rig.ragdoll->bones.size()<<",\"meshTriangles\":"<<(useMesh?asset->faces.size():0)<<",\"medianMs\":"<<times[450]<<",\"p95Ms\":"<<times[854]<<",\"maxMs\":"<<times.back()<<",\"first120MaxMs\":"<<first<<",\"contactRows\":"<<contacts<<",\"ccdIterations\":"<<searches<<",\"ccdLimits\":"<<limits<<",\"minimumBoneCenterY\":"<<lowest<<",\"sleeping\":"<<sleeping<<"}"<<std::endl;
  game.End();world.Destroy();
 }
 std::cout<<"observer_sanity="<<sane<<'\n';return sane?0:1;
}
