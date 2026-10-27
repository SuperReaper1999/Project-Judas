#include "Project.h"
#include "SceneSerialization.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "GameSession.h"
#include "Simulation.h"
#include "FluidWorld.h"
#include "ProductionFluidCoupling.h"
#include "TerrainDemo.h"
#include "RadialTerrain.h"
#include <cstdio>
#include <stdexcept>
int main(int argc,char**argv){
 if(argc!=2)return 2;Project project;std::string error;if(!project.Load(argv[1],error)){std::puts(error.c_str());return 1;}
 EngineHost host;if(!host.Init("Legacy startup observer",640,360,false,error)){std::puts(error.c_str());return 1;}host.OpenProjectAssets(project.RootDir(),project.AssetsDir());host.Resources().SetBlockingMode(true);
 Scene scene;if(!LoadSceneFromFile(project.StartupScenePath(),scene,error)){std::puts(error.c_str());return 1;}
 for(int mode=0;mode<3;++mode){bool fluids=mode!=0;Scene played=scene;if(mode==2)for(auto& o:played.Objects())if(o.fluidVolume)o.transform.position.y+=10;RuntimeWorld world;auto settings=project.Settings();settings.advancedFluidDynamics=fluids;world.ConfigureProject(settings);if(!world.Build(played,&host.Resources(),error,&settings.classification,&settings.navigation)){std::puts(error.c_str());return 1;}host.Resources().WaitForAll();GameSession session;if(!session.Begin(world,error)){std::puts(error.c_str());return 1;}
  if(fluids){auto t=TerrainDemo::CreateSurface();float gap=1000;for(auto&p:world.Fluid().Particles())gap=std::min(gap,t->Sample(p.position-glm::vec3(300,0,0)).signedDistance);std::printf("INITIAL_PARTICLES count=%zu minTerrainGap=%.8g radius=%.8g\n",world.Fluid().Particles().size(),gap,world.FluidSettingsUsed().particleRadius);}
  std::printf("BEGIN case=%d fluids=%d bodies=%zu\n",mode,fluids,world.DynamicBodies().size());int firstStrong=-1;std::fflush(stdout);
  for(int s=0;s<120;++s){if(fluids&&s==1){auto h=world.DynamicBodies().back().Handle();auto pose=world.Physics().GetTransform(h);float gap=10000;for(auto&p:world.Fluid().Particles()){auto local=glm::conjugate(pose.rotation)*(p.position-pose.position);auto d=glm::max(glm::abs(local)-glm::vec3(2,.25,3),glm::vec3(0));gap=std::min(gap,glm::length(d)-world.FluidSettingsUsed().particleRadius);}std::printf("BEFORE_LOAD minimumParticleShipClearance=%.9g\n",gap);}StepPlayedWorld(session,host.GetWindow(),1.f/60);if(fluids&&firstStrong<0){for(auto&d:world.FluidCoupling().Measurements().bodies)if(glm::length(d.buoyancyForce)>8000){firstStrong=s;std::printf("FIRST_LARGE_LOAD case=%d step=%d time=%.9g handle=%u forceN=%.9g\n",mode,s,(s+1)/60.f,d.body.id,glm::length(d.buoyancyForce));break;}}if(s<3||s==9||s==29||s==119)for(size_t i=0;i<world.DynamicBodies().size();++i){auto h=world.DynamicBodies()[i].Handle();auto p=world.Physics().GetTransform(h).position;auto v=world.Physics().GetLinearVelocity(h);std::printf("FULL fluids=%d step=%d i=%zu speed=%.9g p=%.9g,%.9g,%.9g v=%.9g,%.9g,%.9g\n",fluids,s,i,glm::length(v),p.x,p.y,p.z,v.x,v.y,v.z);}if(fluids&&s<3){float speed=0;for(auto&p:world.Fluid().Particles())speed=std::max(speed,glm::length(p.velocity));std::printf("FLUID step=%d maxParticleSpeed=%.8g\n",s,speed);for(auto&d:world.FluidCoupling().Measurements().bodies)std::printf("LOAD step=%d handle=%u fraction=%.8g buoy=%.8g,%.8g,%.8g drag=%.8g,%.8g,%.8g\n",s,d.body.id,d.fraction,d.buoyancyForce.x,d.buoyancyForce.y,d.buoyancyForce.z,d.dragForce.x,d.dragForce.y,d.dragForce.z); }std::fflush(stdout);}
 }
}
