#include "ModelCollision.h"
#include <map>
#include <algorithm>
#include <set>
#include <cmath>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include "SceneFingerprint.h"
#include "../third_party/nlohmann/json.hpp"
bool CleanModelCollision(const MeshData& source,const ModelCollisionCleanup& settings,MeshData& output,ModelCollisionCleanupReport& report,std::string& error,CollisionDiagnostic* diagnostic){
 report={};if(diagnostic)*diagnostic={};
 try{
  if(source.vertices.size()>65536||source.indices.size()>196608)throw std::runtime_error("collision cleanup input bound");
  if(!std::isfinite(settings.weldTolerance)||settings.weldTolerance<0||settings.weldTolerance>.001)throw std::runtime_error("collision weld tolerance must be 0..0.001 metres");
  auto fail=[&](const char* code,const std::string& message,unsigned face,unsigned first,unsigned second){
   if(diagnostic){auto& d=*diagnostic;d.code=code;d.action="Select source location; choose explicit safe cleanup or a separate collision proxy.";d.sourceFace=face;if(face<source.faceLocations.size()){auto loc=source.faceLocations[face];d.sourceFace=loc.element;d.node=source.sourceNodes.at(loc.node);}auto vertex=[&](unsigned i){unsigned index=source.indices.empty()?i:source.indices.at(i);d.sourceVertices.push_back(index<source.vertexLocations.size()?source.vertexLocations[index].element:index);return glm::dvec3(source.vertices.at(index).position);};auto a=vertex(first),b=vertex(second);d.point=(a+b)*.5;d.minimum=glm::min(a,b);d.maximum=glm::max(a,b);}
   throw std::runtime_error(message);
  };
  MeshData mesh=source;mesh.primitives.clear();std::vector<unsigned> remap(mesh.vertices.size());
  // Grid-neighbour lookup, bounded source count; only explicit user tolerance.
  std::map<std::array<long long,3>,std::vector<unsigned>> buckets;
  std::vector<glm::dvec3> positions;std::map<uint32_t,unsigned> identities;std::map<std::array<double,3>,unsigned> exact;
  for(unsigned i=0;i<mesh.vertices.size();++i){glm::dvec3 p(mesh.vertices[i].position);unsigned chosen=unsigned(positions.size());
   if(settings.weldTolerance>0){double scale=1/settings.weldTolerance;std::array<long long,3> key{};for(int k=0;k<3;++k){double cell=std::floor(p[k]*scale);if(!std::isfinite(cell)||std::abs(cell)>1e12)throw std::runtime_error("collision weld grid coordinate bound");key[k]=static_cast<long long>(cell);}
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){auto it=buckets.find({key[0]+x,key[1]+y,key[2]+z});if(it!=buckets.end())for(unsigned v:it->second)if(glm::length(positions[v]-p)<=settings.weldTolerance)chosen=std::min(chosen,v);}
    if(chosen==positions.size()){positions.push_back(p);buckets[key].push_back(chosen);}else ++report.welded;
   }else {
    // Match strict cook topology: visual UV/normal seams share original
    // position identity; distinct authored coincident sheets stay distinct.
    bool fresh;if(!source.sourceVertexIds.empty()){auto entry=identities.emplace(source.sourceVertexIds.at(i),chosen);chosen=entry.first->second;fresh=entry.second;}else{auto entry=exact.emplace(std::array<double,3>{p.x,p.y,p.z},chosen);chosen=entry.first->second;fresh=entry.second;}if(fresh)positions.push_back(p);else if(positions.at(chosen)!=p)throw std::runtime_error("source identity position disagreement");
   }remap[i]=chosen;
  }
  std::vector<uint32_t> indices;std::vector<ModelSourceLocation> faces;std::vector<unsigned> originalFaces;
  unsigned count=source.indices.empty()?source.vertices.size():source.indices.size();if(count%3)throw std::runtime_error("collision cleanup triangle count");
  for(unsigned i=0;i<count;i+=3){unsigned a=source.indices.empty()?i:source.indices[i],b=source.indices.empty()?i+1:source.indices[i+1],c=source.indices.empty()?i+2:source.indices[i+2];if(a>=remap.size()||b>=remap.size()||c>=remap.size())throw std::runtime_error("collision cleanup vertex range");bool degenerate=glm::length(glm::cross(positions[remap[b]]-positions[remap[a]],positions[remap[c]]-positions[remap[a]]))<=1e-12;
   if(degenerate){if(!settings.removeDegenerates)fail("degenerate-face","degenerate collision face requires explicit removal",i/3,i,i+1);++report.removed;continue;}originalFaces.push_back(i/3);indices.insert(indices.end(),{a,b,c});if(!source.faceLocations.empty())faces.push_back(source.faceLocations.at(i/3));
  }
  if(settings.orientPatches){std::map<std::pair<unsigned,unsigned>,std::vector<std::pair<unsigned,bool>>> edges;for(unsigned i=0;i<indices.size();i+=3)for(unsigned k=0;k<3;++k){unsigned a=remap[indices[i+k]],b=remap[indices[i+(k+1)%3]];edges[std::minmax(a,b)].push_back({i/3,a<b});}
   std::vector<std::vector<std::pair<unsigned,bool>>> graph(indices.size()/3);for(auto& [edge,uses]:edges){(void)edge;if(uses.size()>2){unsigned face=uses.front().first,first=0,second=0;for(unsigned k=0;k<3;++k){auto v=remap[indices[face*3+k]];if(v==edge.first)first=k;if(v==edge.second)second=k;}auto original=originalFaces.at(face);fail("nonmanifold-edge","nonmanifold patches cannot be safely oriented; choose a separate collision source",original,original*3+first,original*3+second);}if(uses.size()==2){auto a=uses[0],b=uses[1];bool opposite=a.second==b.second;graph[a.first].push_back({b.first,opposite});graph[b.first].push_back({a.first,opposite});}}
   std::vector<int> flip(graph.size(),-1);for(unsigned start=0;start<flip.size();++start)if(flip[start]<0){std::vector<unsigned> queue{start};flip[start]=0;for(size_t i=0;i<queue.size();++i)for(auto [next,invert]:graph[queue[i]]){int expected=flip[queue[i]]^int(invert);if(flip[next]>=0&&flip[next]!=expected)throw std::runtime_error("non-orientable collision patch");if(flip[next]<0){flip[next]=expected;queue.push_back(next);}}}for(unsigned f=0;f<flip.size();++f)if(flip[f]){std::swap(indices[f*3+1],indices[f*3+2]);++report.flipped;}
  }
  mesh.indices=std::move(indices);mesh.faceLocations=std::move(faces);if(settings.weldTolerance>0){mesh.sourceVertexIds=remap;for(unsigned i=0;i<mesh.vertices.size();++i)mesh.vertices[i].position=glm::vec3(positions[remap[i]]);}output=std::move(mesh);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}

bool ReadModelCollisionCleanup(const std::string& destination,ModelCollisionCleanup& settings,std::string& error){try{auto path=destination+".cleanup.json";if(!std::filesystem::exists(path))return true;if(std::filesystem::file_size(path)>4096)throw std::runtime_error("collision cleanup record bound");std::ifstream file(path);auto j=nlohmann::json::parse(file);settings.removeDegenerates=j.at("removeDegenerates");settings.orientPatches=j.at("orientPatches");settings.weldTolerance=j.at("weldTolerance");return true;}catch(const std::exception& e){error=e.what();return false;}}
bool CookImportedCollisionFile(const std::string& source,const std::string& sourceId,const CollisionCookSettings& settings,const ModelCollisionCleanup& cleanup,const std::string& destination,CollisionAsset& result,ModelCollisionCleanupReport& report,CollisionDiagnostic& diagnostic,std::string& error){
 MeshData raw,derived;CollisionAsset asset;if(!LoadCollisionSource(source,settings.primitive,raw,error)||!CleanModelCollision(raw,cleanup,derived,report,error,&diagnostic)||!CookCollision(derived,settings,asset,error,&diagnostic))return false;
 asset.sourceAsset=sourceId;asset.selectedPrimitive=settings.primitive;asset.settingsFingerprint=CollisionSettingsFingerprint(settings);if(!SceneFingerprintSha256File(source,asset.sourceFingerprint,error))return false;
 // Repairs are author choices persisted alongside the coherent derived asset.
 auto temporary=destination+".cleanup-staging";std::ofstream file(temporary);file<<nlohmann::json{{"removeDegenerates",cleanup.removeDegenerates},{"orientPatches",cleanup.orientPatches},{"weldTolerance",cleanup.weldTolerance},{"removed",report.removed},{"welded",report.welded},{"flipped",report.flipped}}.dump(2)<<'\n';file.close();if(!file){error="cannot stage cleanup settings";return false;}if(!SaveCollisionAsset(destination,asset,error)){std::filesystem::remove(temporary);return false;}std::error_code ec;std::filesystem::rename(temporary,destination+".cleanup.json",ec);if(ec){error=ec.message();return false;}result=std::move(asset);return true;
}
