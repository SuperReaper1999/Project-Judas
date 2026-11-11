#include "Deformable.h"
#include "deformables/PanelCook.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace {
using V=glm::dvec3;
int checks=0,failures=0;
void Check(bool ok,const char* message){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",message);}
// Independent retained original per-vertex mapping. No prepared groups are read.
void Original(const DeformableInstance& material,double alpha,MeshData& mesh){
 const auto& asset=*material.asset;if(mesh.vertices.size()!=asset.render.vertices.size())mesh=asset.render;
 alpha=std::clamp(alpha,0.,1.);
 for(unsigned i=0;i<mesh.vertices.size();++i){const auto& b=asset.binding[i];
  V p=glm::mix(material.previous[b.nodes.x],material.positions[b.nodes.x],alpha),q=glm::mix(material.previous[b.nodes.y],material.positions[b.nodes.y],alpha),r=glm::mix(material.previous[b.nodes.z],material.positions[b.nodes.z],alpha);
  V rp=asset.nodes[b.nodes.x],rq=asset.nodes[b.nodes.y],rr=asset.nodes[b.nodes.z];
  auto normal=glm::cross(q-p,r-p);double area=glm::length(normal);
  if(area<1e-14){mesh.vertices[i].position=glm::vec3(p*b.weights.x+q*b.weights.y+r*b.weights.z);continue;}
  normal/=area;V rn=glm::normalize(glm::cross(rq-rp,rr-rp)),restX=glm::normalize(rq-rp),nowX=glm::normalize(q-p),restY=glm::cross(rn,restX),nowY=glm::cross(normal,nowX);
  auto rotate=[&](V value){return nowX*glm::dot(value,restX)+nowY*glm::dot(value,restY)+normal*glm::dot(value,rn);};
  const auto& source=asset.render.vertices[i];V residual=V(source.position)-(rp*b.weights.x+rq*b.weights.y+rr*b.weights.z);
  auto& mapped=mesh.vertices[i];mapped.position=glm::vec3(p*b.weights.x+q*b.weights.y+r*b.weights.z+rotate(residual));
  mapped.normal=glm::vec3(rotate(V(source.normal)));mapped.tangent=glm::vec4(glm::vec3(rotate(V(glm::vec3(source.tangent)))),source.tangent.w);
 }
}
bool Same(const MeshData& a,const MeshData& b){
 if(a.vertices.size()!=b.vertices.size()||a.indices!=b.indices)return false;
 for(size_t i=0;i<a.vertices.size();++i){const auto& x=a.vertices[i];const auto& y=b.vertices[i];
  if(std::memcmp(&x.position,&y.position,3*sizeof(float))||std::memcmp(&x.normal,&y.normal,3*sizeof(float))||
     std::memcmp(&x.tangent,&y.tangent,4*sizeof(float))||std::memcmp(&x.uv,&y.uv,2*sizeof(float))||
     std::memcmp(&x.uv1,&y.uv1,2*sizeof(float)))return false;
 }return true;
}
}
int main(){
 auto source=MakeDeformableSheet(24,24,2,1,2).render;
 for(size_t i=0;i<source.vertices.size();++i){auto& v=source.vertices[i];v.position.z=.15f*v.position.x*v.position.x+.05f*v.position.y*v.position.y;
  v.normal=glm::normalize(glm::vec3(-.3f*v.position.x,-.1f*v.position.y,1));v.tangent=glm::vec4(glm::normalize(glm::vec3(1,0,.3f*v.position.x)),i%2?1.f:-1.f);
 }
 DeformableAsset cooked;std::string error;PanelCookSettings recipe;recipe.normalAxis=2;
 Check(CookDeformablePanel(source,recipe,cooked,error),"original curved detailed panel cooks through the normal engine workflow");
 if(cooked.panelRenderGroups.empty()){std::puts(error.c_str());return 1;}
 std::vector<unsigned> visits(cooked.binding.size());size_t bytes=cooked.panelRenderGroups.capacity()*sizeof(DeformableAsset::PanelRenderGroup);
 for(const auto& group:cooked.panelRenderGroups){bytes+=group.vertices.capacity()*sizeof(unsigned);for(auto index:group.vertices)++visits.at(index);}
 Check(std::all_of(visits.begin(),visits.end(),[](unsigned n){return n==1;}),"each original detailed vertex belongs to exactly one prepared physical frame");
 auto encoded=EncodeDeformableAsset(cooked);DeformableAsset decoded;
 Check(DecodeDeformableAsset({encoded.begin(),encoded.end()},decoded,error)&&decoded.panelRenderGroups.size()==cooked.panelRenderGroups.size(),"normal asset decode rebuilds derived groups without a format change");
 auto without=cooked;without.panelRenderGroups.clear();
 Check(EncodeDeformableAsset(without)==encoded,"prepared groups add no serialized asset bytes or fingerprint changes");
 auto asset=std::make_shared<DeformableAsset>(std::move(decoded));DeformableInstance material;DeformableSettings settings;settings.panel.enabled=true;
 material.Initialize(asset,settings,glm::dmat4(1),1);MeshData expected=asset->render,actual=asset->render;
 for(unsigned state=0;state<32;++state){
  const V offset=state%3==0?V(100000,-200000,50000):V(.1*state,-.2*state,.3*state);
  for(unsigned i=0;i<asset->nodes.size();++i){const auto rest=asset->nodes[i];
   material.previous[i]=rest+offset+V(0,.02*std::sin(double(i)+state),.03*std::cos(double(i)*.3));
   material.positions[i]=rest+offset+V(.03*std::sin(double(i)),.04*std::cos(double(i)*.4+state),.02*std::sin(double(i)*.7+state));
  }
  if(state%4==0){const auto n=asset->panelRenderGroups.front().nodes;for(auto index:{n.x,n.y,n.z})material.previous[index]=material.positions[index]=offset;}
  bool exact=true;for(double alpha:{-.5,0.,.001,.3,.5,.999,1.,2.}){Original(material,alpha,expected);MapDeformablePanel(material,alpha,actual);exact&=Same(expected,actual);}
  Check(exact,"prepared mapping preserves every position/normal/tangent/UV bit across motion, shifts, interpolation and degeneracy");
 }
 auto reordered=*asset;std::reverse(reordered.binding.begin(),reordered.binding.end());std::reverse(reordered.render.vertices.begin(),reordered.render.vertices.end());
 Check(PrepareDeformableAsset(reordered,error),"re-preparing a changed source rebuilds its derived ownership");
 material.asset=std::make_shared<DeformableAsset>(reordered);expected=reordered.render;actual=reordered.render;
 Original(material,.37,expected);MapDeformablePanel(material,.37,actual);Check(Same(expected,actual),"changed bindings do not reuse stale prepared membership");
 std::printf("PANEL_RENDER_PREPARATION vertices=%zu groups=%zu shared_prepared_bytes=%zu cooked_bytes=%zu\n",cooked.render.vertices.size(),cooked.panelRenderGroups.size(),bytes,encoded.size());
 std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
