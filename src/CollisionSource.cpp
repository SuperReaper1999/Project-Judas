#include "CollisionAsset.h"
#include "SceneFingerprint.h"
#include "ModelLoader.h"
#include "SkeletalAnimation.h"
#include <sstream>
#include <iomanip>
#include "cgltf.h"
#include "../third_party/tiny_obj_loader.h"
#include <filesystem>
#include <memory>
#include <map>
#include <stdexcept>
#include <cmath>

bool LoadCollisionSource(const std::string& path,unsigned selected,MeshData& output,std::string& error) {
 try {
  MeshData mesh;
  const auto extension=std::filesystem::path(path).extension().string();
  if(extension==".judasmodel") {
   MeshData model;if(!LoadModelMesh(path,model,error))throw std::runtime_error(error);if(selected>=model.primitives.size())throw std::runtime_error("selected normalized model part missing");auto& part=model.primitives[selected];auto palette=model.skeletal?ResolveSkinMatrices(model.skeletal->skeleton,model.skeletal->skeleton.rest):std::vector<glm::mat4>{};
   mesh.sourceNodes=model.sourceNodes;std::map<uint32_t,uint32_t> remap;
   for(unsigned i=part.first;i<part.first+part.count;++i){unsigned original=model.indices.at(i);auto [entry,fresh]=remap.emplace(original,uint32_t(mesh.vertices.size()));if(fresh){auto vertex=model.vertices.at(original);if(!palette.empty()){auto& weights=model.skinVertices.at(original);glm::mat4 matrix(0);for(int k=0;k<4;++k)matrix+=palette.at(weights.joints[k])*weights.weights[k]+palette.at(weights.joints1[k])*weights.weights1[k];vertex.position=glm::vec3(matrix*glm::vec4(vertex.position,1));}mesh.vertices.push_back(vertex);if(!model.sourceVertexIds.empty())mesh.sourceVertexIds.push_back(model.sourceVertexIds.at(original));if(!model.vertexLocations.empty())mesh.vertexLocations.push_back(model.vertexLocations.at(original));}mesh.indices.push_back(entry->second);if(i%3==0&&!model.faceLocations.empty())mesh.faceLocations.push_back(model.faceLocations.at(i/3));}
   // The normalized rest binding may reflect a static attachment. Baking it
   // into collision vertices also changes oriented winding, once per triangle.
   if(!palette.empty())for(unsigned i=0;i<mesh.indices.size();i+=3){auto original=model.indices.at(part.first+i);auto& w=model.skinVertices.at(original);glm::mat4 m(0);for(int k=0;k<4;++k)m+=palette.at(w.joints[k])*w.weights[k]+palette.at(w.joints1[k])*w.weights1[k];if(glm::determinant(glm::mat3(m))<0)std::swap(mesh.indices[i+1],mesh.indices[i+2]);}
  } else if(extension==".obj") {
   tinyobj::attrib_t attributes;std::vector<tinyobj::shape_t> shapes;std::vector<tinyobj::material_t> materials;std::string warning,diagnostic;
   if(!tinyobj::LoadObj(&attributes,&shapes,&materials,&warning,&diagnostic,path.c_str()))throw std::runtime_error(diagnostic);
   if(selected>=shapes.size())throw std::runtime_error("selected OBJ object/group missing");
   const auto& shape=shapes[selected];mesh.sourceNodes.push_back(shape.name);unsigned corner=0;
   for(auto index:shape.mesh.indices) {
    if(index.vertex_index<0||size_t(index.vertex_index)*3+2>=attributes.vertices.size())throw std::runtime_error("OBJ collision position index out of range");
    MeshVertex vertex;vertex.position={attributes.vertices[3*index.vertex_index],attributes.vertices[3*index.vertex_index+1],attributes.vertices[3*index.vertex_index+2]};
    mesh.indices.push_back(mesh.vertices.size());mesh.vertices.push_back(vertex);mesh.sourceVertexIds.push_back(index.vertex_index);mesh.vertexLocations.push_back({0,uint32_t(index.vertex_index)});if(corner++%3==0)mesh.faceLocations.push_back({0,corner/3});
   }
  } else if(extension==".gltf"||extension==".glb") {
   cgltf_options options{};cgltf_data* raw=nullptr;
   if(cgltf_parse_file(&options,path.c_str(),&raw)!=cgltf_result_success)throw std::runtime_error("glTF collision parse failed");
   std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
   // Match Judas's normal registered-model policy: no unregistered side buffers.
   for(size_t i=0;i<data->buffers_count;++i)if(data->buffers[i].uri&&std::string(data->buffers[i].uri).rfind("data:",0)!=0)throw std::runtime_error("external glTF buffers unsupported; embed or use GLB");
   if(cgltf_load_buffers(&options,data.get(),path.c_str())!=cgltf_result_success||cgltf_validate(data.get())!=cgltf_result_success)throw std::runtime_error("invalid glTF collision buffers/accessors");
   const cgltf_node* node=nullptr;const cgltf_primitive* primitive=nullptr;unsigned ordinal=0;
   // Selection enumerates node/primitive pairs in file order, not all geometry.
   for(size_t n=0;n<data->nodes_count;++n)if(data->nodes[n].mesh)for(size_t p=0;p<data->nodes[n].mesh->primitives_count;++p)if(ordinal++==selected){node=&data->nodes[n];primitive=&node->mesh->primitives[p];}
   if(!primitive)throw std::runtime_error("selected glTF node/primitive missing");
   if(node->skin||primitive->targets_count||primitive->has_draco_mesh_compression||primitive->type!=cgltf_primitive_type_triangles)throw std::runtime_error("collision requires static uncompressed triangle primitive");
   cgltf_accessor* positions=nullptr;for(size_t i=0;i<primitive->attributes_count;++i)if(primitive->attributes[i].type==cgltf_attribute_type_position)positions=primitive->attributes[i].data;
   if(!positions||positions->type!=cgltf_type_vec3||positions->count>65536)throw std::runtime_error("invalid collision POSITION accessor");
   glm::mat4 transform;cgltf_node_transform_world(node,&transform[0][0]);
   glm::dmat3 linear(transform);if(!(glm::determinant(linear)>0))throw std::runtime_error("collision source mirror/singular transform unsupported");
   for(int a=0;a<3;++a)for(int b=a+1;b<3;++b)if(std::abs(glm::dot(glm::normalize(linear[a]),glm::normalize(linear[b])))>1e-7)throw std::runtime_error("collision source shear unsupported");
   for(size_t i=0;i<positions->count;++i){cgltf_float values[3];if(!cgltf_accessor_read_float(positions,i,values,3))throw std::runtime_error("collision position decode failed");MeshVertex vertex;vertex.position=glm::vec3(transform*glm::vec4(values[0],values[1],values[2],1));mesh.vertices.push_back(vertex);mesh.sourceVertexIds.push_back(i);}
   size_t count=primitive->indices?primitive->indices->count:positions->count;
   if(count>196608||count%3)throw std::runtime_error("collision triangle index limit/range");
   for(size_t i=0;i<count;++i){size_t v=primitive->indices?cgltf_accessor_read_index(primitive->indices,i):i;if(v>=positions->count)throw std::runtime_error("collision vertex index out of range");mesh.indices.push_back(v);}
  } else throw std::runtime_error("collision source must be OBJ/glTF/GLB or an imported .judasmodel");
  if(mesh.vertices.empty())throw std::runtime_error("selected source contains no collision geometry");
  // The returned data is already the selected geometry; do not select a second time.
  output=std::move(mesh);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}

std::string CollisionSettingsFingerprint(const CollisionCookSettings& settings) {
 std::ostringstream text;text<<std::setprecision(17)<<"JudasCollisionCook 1 "<<settings.convex<<' '<<settings.twoSided<<' '<<settings.primitive;
 for(int c=0;c<4;++c)for(int r=0;r<4;++r)text<<' '<<settings.transform[c][r];
 return SceneFingerprintSha256(text.str());
}
bool CollisionAssetStale(const CollisionAsset& asset,const std::string& source,std::string& reason) {
 std::string digest;if(!SceneFingerprintSha256File(source,digest,reason))return true;
 CollisionCookSettings settings;settings.convex=asset.convex;settings.twoSided=asset.twoSided;settings.primitive=asset.selectedPrimitive;settings.transform=asset.sourceTransform;
 if(digest!=asset.sourceFingerprint||CollisionSettingsFingerprint(settings)!=asset.settingsFingerprint){reason="stale collision cook: source/settings changed; rebake required";return true;}
 reason.clear();return false;
}
bool CookCollisionFile(const std::string& source,const std::string& sourceId,const CollisionCookSettings& settings,const std::string& destination,CollisionAsset& result,std::string& error) {
 MeshData mesh;CollisionAsset asset;
 if(!LoadCollisionSource(source,settings.primitive,mesh,error)||!CookCollision(mesh,settings,asset,error))return false;
 asset.sourceAsset=sourceId;asset.settingsFingerprint=CollisionSettingsFingerprint(settings);
 if(!SceneFingerprintSha256File(source,asset.sourceFingerprint,error)||!SaveCollisionAsset(destination,asset,error))return false;
 result=std::move(asset);return true;
}
