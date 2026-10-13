#include "Deformable.h"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
 if(argc!=3)return 2;std::ifstream file(argv[1],std::ios::binary);
 std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file),{}};
 DeformableAsset base,merged;std::string error;
 if(!DecodeDeformableAsset(bytes,base,error)){std::cerr<<error<<'\n';return 1;}
 merged.kind=base.kind;
 const glm::dvec3 centers[]={{-.82,-.05,-1.35},{.82,-.05,-1.35},{-.82,-.05,1.05},{.82,-.05,1.05}};
 for(unsigned i=0;i<4;++i){const unsigned nodeBase=merged.nodes.size(),vertexBase=merged.render.vertices.size(),indexBase=merged.render.indices.size();
  for(auto node:base.nodes)merged.nodes.push_back(node+centers[i]);
  merged.massWeights.insert(merged.massWeights.end(),base.massWeights.begin(),base.massWeights.end());
  for(auto face:base.triangles)merged.triangles.push_back(face+glm::uvec3(nodeBase));
  for(auto tet:base.tetrahedra)merged.tetrahedra.push_back(tet+glm::uvec4(nodeBase));
  for(auto vertex:base.render.vertices){vertex.position+=glm::vec3(centers[i]);merged.render.vertices.push_back(vertex);}
  for(auto index:base.render.indices)merged.render.indices.push_back(index+vertexBase);
  for(auto binding:base.binding){binding.nodes+=glm::uvec4(nodeBase);merged.binding.push_back(binding);}
  for(auto primitive:base.render.primitives){primitive.first+=indexBase;merged.render.primitives.push_back(primitive);}
  for(auto node:base.groups.at(i%2?"left":"right"))merged.groups["root"+std::to_string(i)].push_back(node+nodeBase);
  for(auto node:base.groups.at(i%2?"right":"left"))merged.groups["tip"+std::to_string(i)].push_back(node+nodeBase);
 }
 if(!PrepareDeformableAsset(merged,error)){std::cerr<<error<<'\n';return 1;}
 std::ofstream output(argv[2],std::ios::binary);const auto encoded=EncodeDeformableAsset(merged);output.write(encoded.data(),encoded.size());
 std::cout<<merged.nodes.size()<<" nodes, "<<merged.tetrahedra.size()<<" tets, "<<merged.groups.size()<<" groups\n";return output?0:1;
}
