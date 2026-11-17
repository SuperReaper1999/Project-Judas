#include "EngineHost.h"
#include "SkeletalAnimation.h"
#include "render/ShadowPrimitiveRange.h"
#include <SDL2/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
unsigned failures=0;
void Check(bool ok,const char* label){std::printf("%s %s\n",ok?"PASS":"FAIL",label);failures+=!ok;}
void RangeContracts(){
 using S=JudasRender::ShadowPrimitiveState;
 std::vector<S> parts{{3,3,true,true,false},{6,6,true,true,false}};
 auto resolve=[&]{return JudasRender::CoalesceOpaqueShadowRange(parts.size(),[&](size_t i){return parts[i];});};
 auto range=resolve();Check(range&&range->first==3&&range->count==9,"contiguous opaque triangles retain exact index coverage");
 parts[1].first=9;Check(!resolve(),"gaps are not filled with unintended triangles");parts[1].first=3;Check(!resolve(),"overlapping ranges are not reordered or duplicated");parts[1].first=6;
 parts[1].count=0;Check(!resolve(),"empty ranges retain their original draw semantics");parts[1].count=5;Check(!resolve(),"incomplete triangles cannot join across draw boundaries");parts[1].count=6;
 parts[1].opaque=false;Check(!resolve(),"masked or blended parts retain material-specific draws");parts[1].opaque=true;
 parts[1].cull=false;Check(!resolve(),"different face-culling policies retain separate draws");parts[1].cull=true;
 parts[1].reversed=true;Check(!resolve(),"mirrored joint winding retains separate draws");parts[1].reversed=false;
 parts={{0,1073741823u,true,true,false},{1073741823u,1073741826u,true,true,false}};Check(!resolve(),"merged count cannot exceed signed GL draw limits");
 parts[0].first=std::numeric_limits<unsigned>::max()-2;Check(!resolve(),"index arithmetic cannot wrap");
}
MeshData Mesh(bool indexed,bool skinned){
 MeshData data;data.vertices.resize(6);
 const glm::vec3 positions[]={{-1,-.7f,0},{-.1f,-.7f,0},{-.5f,.7f,0},{.1f,-.7f,0},{1,-.7f,0},{.5f,.7f,0}};
 for(unsigned i=0;i<6;++i){data.vertices[i].position=positions[i];data.vertices[i].normal={0,0,1};}
 if(indexed)data.indices={0,1,2,3,4,5};
 data.primitives={{0,3,0,"left"},{3,3,1,"right"}};
 if(skinned){
  auto asset=std::make_shared<SkeletalAsset>();auto& s=asset->skeleton;s.names={"left","right"};s.parents={-1,-1};s.order={0,1};s.rest.local.resize(2);s.skinNodes={0,1};s.inverseBind={glm::mat4(1),glm::mat4(1)};data.skeletal=asset;data.skinVertices.resize(6);
  for(unsigned i=3;i<6;++i){data.skinVertices[i].joints.x=1;data.skinVertices[i].joints1.x=1;}
 }
 return data;
}
void RenderContract(Renderer& r,bool indexed,bool skinned,int variant){
 std::printf("CASE indexed=%d skinned=%d variant=%d\n",indexed,skinned,variant);
 auto data=Mesh(indexed,skinned);auto mesh=r.CreateMesh(data);Check(mesh.IsValid(),"native test mesh uploads");
 if(variant==5||variant==6)Check(r.UpdateMeshStructuralSections(mesh,variant==5?std::vector<int>{0,0}:std::vector<int>{-1,0}),"authored structural sections publish");
 MaterialDefinition definition;definition.model=MaterialModel::Unlit;
 const auto first=r.CreateMaterial(definition);
 if(variant==1){definition.alpha=MaterialAlpha::Mask;definition.baseColor.a=.1f;definition.alphaCutoff=.5f;}
 if(variant==2)definition.doubleSided=true;
 if(variant==7)definition.alpha=MaterialAlpha::Blend;
 const auto second=r.CreateMaterial(definition);
 MaterialBinding left;left.handle=first;MaterialBinding right;right.handle=second;
 std::vector<glm::mat4> pose{glm::mat4(1),glm::mat4(1)};
 if(variant==4)pose[1]=glm::scale(glm::mat4(1),{-1,1,1});
 std::vector<std::string> hidden;if(variant==3)hidden={"right"};
 auto capture=[&](bool merge,unsigned& draws,unsigned& triangles){
  SDL_setenv("JUDAS_SHADOW_COALESCE",merge?"1":"0",1);
  r.BeginFrame(128,128);r.SetMaterialBindings({left,right});
  const auto before=r.Stats().drawCalls;const auto beforeTriangles=r.Stats().triangles;
  r.BeginShadowPass(0,glm::ortho(-2.f,2.f,-2.f,2.f,.1f,10.f)*glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0)));
  if(variant==8)r.BeginMeshBatch();
  r.DrawMesh(mesh,{0,0,0},{1,0,0,0},{1,1,1},{},{1,1,1},1,skinned?&pose:nullptr,&hidden);
  if(variant==8){r.DrawMesh(mesh,{.25f,0,0},{1,0,0,0},{1,1,1},{},{1,1,1},1);r.EndMeshBatch();}
  GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);std::vector<float> depth(size_t(viewport[2])*viewport[3]);glReadPixels(0,0,viewport[2],viewport[3],GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
  r.EndShadowPass();draws=r.Stats().drawCalls-before;triangles=r.Stats().triangles-beforeTriangles;Check(glGetError()==GL_NO_ERROR,"native shadow readback has no GL error");return depth;
 };
 unsigned originalDraws=0,mergedDraws=0,originalTriangles=0,mergedTriangles=0;const auto original=capture(false,originalDraws,originalTriangles);const auto merged=capture(true,mergedDraws,mergedTriangles);
 Check(originalTriangles==mergedTriangles,"actual submitted triangle count is unchanged");
 Check(original==merged,"actual shadow depth is identical to retained per-part submission");
 Check(std::any_of(merged.begin(),merged.end(),[](float value){return value<.99f;}),"comparison contains real rendered occluders");
 Check((variant==0||variant==8)?(originalDraws>0&&originalDraws==mergedDraws*2):originalDraws==mergedDraws,"only compatible opaque parts reduce draw work");
 r.DestroyMesh(mesh);r.DestroyMaterial(first);r.DestroyMaterial(second);
}
}
int main(){
 RangeContracts();EngineHost host;std::string error;if(!host.Init("Opaque shadow range fidelity",128,128,false,error)){std::puts(error.c_str());return 1;}
 for(bool indexed:{false,true})for(bool skinned:{false,true})for(int variant=0;variant<9;++variant){if((variant>=4&&variant<=6&&!skinned)||(variant==8&&skinned))continue;RenderContract(host.GetRenderer(),indexed,skinned,variant);}
 host.Shutdown();return failures?1:0;
}
