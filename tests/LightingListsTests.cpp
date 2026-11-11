#include "lighting/LightLists.h"
#include "EngineHost.h"
#include "RenderLightGrid.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstring>
namespace {unsigned checks=0,failures=0;
void Check(bool yes,const char* text){++checks;failures+=!yes;std::printf("%s %s\n",yes?"PASS":"FAIL",text);}
void Policy(const char* value){
#ifdef _WIN32
 _putenv_s("JUDAS_LIGHT_LISTS",value);
#else
 setenv("JUDAS_LIGHT_LISTS",value,1);
#endif
}
bool SameLists(const RenderLightGrid& cpu,const std::vector<glm::vec4>& actual){
 const unsigned tiles=cpu.columns*cpu.rows,prefix=1+tiles+cpu.visible*4;
 if(actual.size()<prefix||actual[0]!=cpu.texels[0])return false;
 for(unsigned i=1+tiles;i<prefix;++i)if(actual[i]!=cpu.texels[i])return false;
 for(unsigned t=0;t<tiles;++t){const auto a=actual[1+t],b=cpu.texels[1+t];if(a.y!=b.y)return false;
  for(unsigned i=0;i<unsigned(b.y);++i)if(actual.at(size_t(a.x)+i)!=cpu.texels.at(size_t(b.x)+i))return false;
 }return true;
}
}
int main(){std::string error;EngineHost host;Check(host.Init("Lighting lists correctness",320,240,false,error),error.c_str());if(failures)return 1;
 auto& renderer=host.GetRenderer();const auto& caps=renderer.Capabilities();
 const auto view=glm::lookAt(glm::vec3(0,4,12),glm::vec3(0),glm::vec3(0,1,0));
 const auto projection=glm::perspective(glm::radians(65.f),320.f/240,.1f,100.f);
 JudasLighting::LightLists transport;
 for(unsigned count:{0u,1u,32u,128u,1200u}){
  std::vector<DynamicLight> lights(count);for(unsigned i=0;i<count;++i){auto& l=lights[i];l.identity=i+1;l.position={float(int(i%16)-8),float(1+i%4),-float(i%8)};l.range=count>1024?100:6;l.color={4,2,1};}
  RenderLightGrid cpu,prepared;cpu.Build(lights,view,projection,320,240);prepared.Build(lights,view,projection,320,240,false);
  Check(cpu.visible==prepared.visible&&cpu.maxPerTile==prepared.maxPerTile&&cpu.overflow==prepared.overflow,"CPU and compute preparation preserve selection, coverage, cap and observable overflow");
  if(caps.modernTier){std::vector<glm::vec4> actual;
   Check(transport.Upload(prepared,true,caps,error)&&transport.ReadForDiagnostics(actual,error)&&SameLists(cpu,actual),"real GPU deterministic tile lists exactly match every CPU record and used index, including overload");
   Check(transport.Upload(cpu,false,caps,error)&&transport.ReadForDiagnostics(actual,error)&&actual==cpu.texels,"modern CPU-built SSBO transport preserves the full packet exactly");
  }else Check(!transport.Upload(prepared,true,caps,error),"portable profile declines compute without issuing modern GL work");
 }
 std::vector<DynamicLight> lights(32);for(unsigned i=0;i<lights.size();++i){auto& l=lights[i];l.identity=i+1;l.position={float(int(i%8)-4)*1.5f,2,-float(i/8)*2};l.range=7;l.color={.8f+float(i%3),.5f,1};}
 auto frame=[&](const char* mode){Policy(mode);Renderer renderer;Check(renderer.Init(),"specialized lighting renderer initializes on the current context");renderer.BeginFrame(320,240);renderer.SetCamera(view,projection);renderer.SetLighting({0,1,0},{0,0,0},{.02f,.02f,.02f});renderer.SetDynamicLights(lights);renderer.DrawBox({0,-.2f,0},{1,0,0,0},{7,.2f,7},{.6f,.6f,.6f},1);renderer.DrawBox({0,.75f,0},{1,0,0,0},{.5f,.75f,.5f},{.7f,.3f,.2f},1);renderer.EndFrame();std::vector<unsigned char> image;renderer.CaptureFrame(320,240,image);Check(glGetError()==GL_NO_ERROR,"specialized lighting rendering leaves no graphics errors");renderer.Shutdown();return image;};
 const auto texture=frame("texture"),storage=frame("cpu"),compute=frame("compute");
 Check(!texture.empty()&&texture==storage&&storage==compute,"real texture, CPU SSBO and compute SSBO paths render identical pixels");
 Check(std::any_of(texture.begin(),texture.end(),[](auto c){return c>32;}),"render comparison contains actual visible lighting rather than a blank frame");
 Check(transport.Shutdown(error),"lighting list buffers and kernels retire on the owning context");
 Check(glGetError()==GL_NO_ERROR,"all light-list paths leave no graphics errors");
 std::printf("Lighting lists %u checks %u failures\n",checks,failures);return failures?1:0;
}
