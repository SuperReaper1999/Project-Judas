#include "RenderLightGrid.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
void RenderLightGrid::Build(const std::vector<DynamicLight>& lights,const glm::mat4& view,const glm::mat4& projection,unsigned width,unsigned height){
 if(lights.empty()){columns=rows=1;visible=rejected=overflow=maxPerTile=0;texels={{1,1,2,0},{2,0,0,0}};return;}
 columns=std::max(1u,(std::min(width,7680u)+TileSize-1)/TileSize);rows=std::max(1u,(std::min(height,7680u)+TileSize-1)/TileSize);
 visible=rejected=overflow=maxPerTile=0;texels.clear();texels.resize(1+columns*rows);std::vector<std::vector<unsigned>> bins(columns*rows);
 struct Candidate{unsigned index;float score;};std::vector<Candidate> candidates;const auto eye=glm::vec3(glm::inverse(view)[3]);
 for(unsigned i=0;i<lights.size();++i){const auto& l=lights[i];float energy=std::max({l.color.x,l.color.y,l.color.z});if(!(std::isfinite(l.range)&&l.range>0&&std::isfinite(energy)&&std::isfinite(glm::length(l.color))&&energy>0&&std::isfinite(glm::length(l.position)))){++rejected;continue;}candidates.push_back({i,energy/(1+glm::dot(l.position-eye,l.position-eye))});}
 // Stable deterministic relevance selection, never registration-order truncation.
 std::stable_sort(candidates.begin(),candidates.end(),[&](auto a,auto b){return a.score!=b.score?a.score>b.score:lights[a.index].identity<lights[b.index].identity;});
 const auto vp=projection*view;
 for(auto candidate:candidates){const auto& l=lights[candidate.index];glm::vec2 lo(1),hi(-1);bool crosses=false;unsigned outside[6]={};
  // Project an enclosing AABB. A near-plane crossing uses the whole viewport,
  // so no sphere influence can disappear because its centre is behind the eye.
  for(unsigned c=0;c<8;++c){auto p=vp*glm::vec4(l.position+glm::vec3(c&1?l.range:-l.range,c&2?l.range:-l.range,c&4?l.range:-l.range),1);crosses|=p.w<=0;outside[0]+=p.x < -p.w;outside[1]+=p.x>p.w;outside[2]+=p.y < -p.w;outside[3]+=p.y>p.w;outside[4]+=p.z < -p.w;outside[5]+=p.z>p.w;if(p.w>0){auto xy=glm::vec2(p)/p.w;lo=glm::min(lo,xy);hi=glm::max(hi,xy);}}
  bool culled=false;for(auto n:outside)culled|=n==8;if(culled){++rejected;continue;}
  if(visible==MaxLights){++overflow;continue;}if(crosses){lo={-1,-1};hi={1,1};}
  auto cell=[](float x,unsigned n){return std::min(n-1,unsigned(std::max(0.f,std::floor((x*.5f+.5f)*n))));};auto x0=cell(lo.x,columns),x1=cell(hi.x,columns),y0=cell(lo.y,rows),y1=cell(hi.y,rows);
  unsigned index=visible++;const auto direction=glm::length(l.direction)>1e-6f?glm::normalize(l.direction):glm::vec3(0,0,-1);
  texels.emplace_back(l.position,l.range);texels.emplace_back(l.color,float(l.kind==LightKind::Spot));texels.emplace_back(direction,std::cos(glm::radians(std::min(l.innerConeDegrees,l.outerConeDegrees))));texels.emplace_back(std::cos(glm::radians(l.outerConeDegrees)),float(l.shadowMapIndex),0,0);
  for(unsigned y=y0;y<=y1;++y)for(unsigned x=x0;x<=x1;++x){auto& bin=bins[y*columns+x];if(bin.size()<MaxPerTile)bin.push_back(index);else ++overflow;}
 }
 texels[0]={float(columns),float(rows),float(1+columns*rows),0};
 for(unsigned t=0;t<bins.size();++t){auto& bin=bins[t];maxPerTile=std::max(maxPerTile,unsigned(bin.size()));texels[1+t]={float(texels.size()),float(bin.size()),0,0};for(auto i:bin)texels.emplace_back(float(i),0,0,0);}
}
std::vector<DynamicLight> SelectShadowLights(const std::vector<DynamicLight>& input,const glm::mat4& view,const glm::mat4& projection,unsigned budget){
 auto result=input;const glm::vec3 eye=glm::vec3(glm::inverse(view)[3]);std::vector<std::pair<float,unsigned>> requests;
 for(unsigned i=0;i<result.size();++i){auto& l=result[i];bool request=l.castsShadow||l.shadowMapIndex>=1;l.shadowMapIndex=-1;if(!request||l.kind!=LightKind::Spot)continue;
  RenderLightGrid test;test.Build({l},view,projection,64,64);if(!test.visible)continue;
  float energy=std::max({l.color.x,l.color.y,l.color.z});float score=l.shadowPriority+energy/(1+glm::dot(eye-l.position,eye-l.position));requests.emplace_back(score,i);
 }
 std::stable_sort(requests.begin(),requests.end(),[&](auto a,auto b){return a.first!=b.first?a.first>b.first:result[a.second].identity<result[b.second].identity;});
 for(unsigned n=0;n<std::min({budget,8u,unsigned(requests.size())});++n)result[requests[n].second].shadowMapIndex=int(n+1);
 return result;
}
