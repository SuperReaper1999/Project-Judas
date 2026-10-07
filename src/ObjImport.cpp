#include "ModelImport.h"
#include "TextureLoader.h"
#include "AsyncFile.h"
#include "Tangents.h"
#include "../third_party/tiny_obj_loader.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace fs=std::filesystem;
namespace {
// MaterialReader keeps dependency lookup inside the selected source tree. A
// recipe remap is the only way a downloaded OBJ can access a different tree.
struct Reader final:tinyobj::MaterialReader {
 fs::path root;const ModelImportSettings& settings;ModelImportReport& report;
 std::map<std::string,fs::path> textureRoots;
 Reader(const fs::path& r,const ModelImportSettings& s,ModelImportReport& p):root(r),settings(s),report(p){}
 fs::path Resolve(std::string name,fs::path base){std::replace(name.begin(),name.end(),'\\','/');auto remap=settings.dependencyRemaps.find(name);auto path=fs::weakly_canonical(remap==settings.dependencyRemaps.end()?base/name:fs::path(remap->second));auto relative=path.lexically_relative(root);if(remap==settings.dependencyRemaps.end()&&(relative.empty()||relative.is_absolute()||*relative.begin()==".."))throw std::runtime_error("OBJ dependency outside approved root: "+name+"; explicit remap required");if(!fs::is_regular_file(path)||fs::file_size(path)>64*1024*1024)throw std::runtime_error("missing/oversized OBJ dependency: "+name);report.dependencies.push_back(path.string());return path;}
 bool operator()(const std::string& name,std::vector<tinyobj::material_t>* materials,std::map<std::string,int>* mapping,std::string* warn,std::string* error) override {auto path=Resolve(name,root);std::ifstream in(path);auto first=materials->size();tinyobj::LoadMtl(mapping,materials,&in,warn,error);for(size_t i=first;i<materials->size();++i)textureRoots[materials->at(i).name]=path.parent_path();return error->empty();}
 void Map(const std::string& name,const tinyobj::texture_option_t& options,const fs::path& base,MaterialMap& map){if(name.empty())return;auto path=Resolve(name,base);std::string error;if(!ReadWholeFile(path.string(),map.encodedImage,error)||!DecodeTextureFromMemory(map.encodedImage.data(),map.encodedImage.size(),name,map.embedded,error))throw std::runtime_error(error);map.scale={options.scale[0],options.scale[1]};map.offset={options.origin_offset[0],options.origin_offset[1]};map.sampler.wrapS=map.sampler.wrapT=options.clamp?33071:10497;}
};
}
bool ImportObjSource(const std::string& path,const ModelImportSettings& settings,MeshData& out,ModelImportReport& report,std::string& error){try{
 if(fs::file_size(path)>64*1024*1024)throw std::runtime_error("OBJ exceeds 64 MiB");
 std::ifstream file(path);std::string text((std::istreambuf_iterator<char>(file)),{});std::istringstream stream(text);Reader reader(fs::weakly_canonical(fs::path(path).parent_path()),settings,report);
 tinyobj::attrib_t attributes;std::vector<tinyobj::shape_t> shapes;std::vector<tinyobj::material_t> materials;std::string warning;
 if(!tinyobj::LoadObj(&attributes,&shapes,&materials,&warning,&error,&stream,&reader,true)||!error.empty())throw std::runtime_error(error);
 if(attributes.vertices.size()>6000000)throw std::runtime_error("OBJ position resource bound");
 // Keep exact original face IDs for already-triangulated OBJ sources. For
 // general polygons tinyobjloader supplies triangulation but no source-face
 // map: vertex/node locations remain exact; an unknown face is honest.
 std::istringstream sourceStream(text);tinyobj::attrib_t sourceAttributes;
 std::vector<tinyobj::shape_t> sourceShapes;std::vector<tinyobj::material_t> sourceMaterials;
 std::string sourceWarning,sourceError;
 if(!tinyobj::LoadObj(&sourceAttributes,&sourceShapes,&sourceMaterials,&sourceWarning,&sourceError,&sourceStream,&reader,false))throw std::runtime_error(sourceError);
 MeshData mesh;
 for(auto& src:materials){MaterialDefinition material;material.baseColor={src.diffuse[0],src.diffuse[1],src.diffuse[2],src.dissolve};material.emissive={src.emission[0],src.emission[1],src.emission[2]};material.metallic=src.metallic;material.roughness=src.roughness>0?src.roughness:std::clamp(float(std::sqrt(2/(src.shininess+2))),.04f,1.f);material.flipV=false;if(src.dissolve<1)material.alpha=MaterialAlpha::Blend;auto base=reader.textureRoots.at(src.name);reader.Map(src.diffuse_texname,src.diffuse_texopt,base,material.maps[0]);reader.Map(src.normal_texname,src.normal_texopt,base,material.maps[2]);reader.Map(src.emissive_texname,src.emissive_texopt,base,material.maps[4]);
  if(!src.bump_texname.empty()||!src.specular_texname.empty()||!src.displacement_texname.empty()||!src.alpha_texname.empty()||!src.roughness_texname.empty()||!src.metallic_texname.empty())report.diagnostics.push_back({"warning","obj-material-approximation",path,src.name,"Remap to a Judas material for bump/displacement/independent alpha or PBR scalar maps","MTL diffuse/normal/emissive and scalar factors retained; unsupported maps are not rendered"});
  std::string e;if(!ValidateMaterial(material,e))throw std::runtime_error(e);mesh.materials.push_back(std::move(material));mesh.materialKeys.push_back(src.name);
 }
 int fallback=int(mesh.materials.size());mesh.materials.emplace_back();mesh.materialKeys.push_back("__default");
 for(size_t si=0;si<shapes.size();++si){auto& shape=shapes[si];if(shape.mesh.indices.size()>6000000||mesh.vertices.size()+shape.mesh.indices.size()>2000000)throw std::runtime_error("OBJ triangle resource bound");std::string key=shape.name.empty()?"unnamed":shape.name;mesh.sourceNodes.push_back(key);size_t corner=0;
  for(size_t face=0;face<shape.mesh.num_face_vertices.size();++face){if(shape.mesh.num_face_vertices[face]!=3)throw std::runtime_error("OBJ triangulation failed");int slot=shape.mesh.material_ids[face];if(slot<0)slot=fallback;if(size_t(slot)>=mesh.materials.size())throw std::runtime_error("invalid OBJ material slot");unsigned first=unsigned(mesh.indices.size());glm::vec3 positions[3];for(int k=0;k<3;++k){auto idx=shape.mesh.indices.at(corner+k);if(idx.vertex_index<0||size_t(idx.vertex_index)*3+2>=attributes.vertices.size())throw std::runtime_error("OBJ position index");positions[k]={attributes.vertices[idx.vertex_index*3],attributes.vertices[idx.vertex_index*3+1],attributes.vertices[idx.vertex_index*3+2]};}auto n=glm::cross(positions[1]-positions[0],positions[2]-positions[0]);n=glm::length(n)>1e-8f?glm::normalize(n):glm::vec3(0,1,0);
   for(int k=0;k<3;++k){auto idx=shape.mesh.indices.at(corner++);MeshVertex v;v.position=positions[k];v.normal=n;if(idx.normal_index>=0){if(size_t(idx.normal_index)*3+2>=attributes.normals.size())throw std::runtime_error("OBJ normal index");v.normal={attributes.normals[idx.normal_index*3],attributes.normals[idx.normal_index*3+1],attributes.normals[idx.normal_index*3+2]};}if(idx.texcoord_index>=0){if(size_t(idx.texcoord_index)*2+1>=attributes.texcoords.size())throw std::runtime_error("OBJ UV index");v.uv={attributes.texcoords[idx.texcoord_index*2],attributes.texcoords[idx.texcoord_index*2+1]};}v.uv1=v.uv;mesh.indices.push_back(unsigned(mesh.vertices.size()));mesh.vertices.push_back(v);mesh.sourceVertexIds.push_back(idx.vertex_index);mesh.vertexLocations.push_back({uint32_t(si),uint32_t(idx.vertex_index)});}
   bool exactFaces=si<sourceShapes.size()&&sourceShapes[si].mesh.num_face_vertices.size()==shape.mesh.num_face_vertices.size()&&std::all_of(sourceShapes[si].mesh.num_face_vertices.begin(),sourceShapes[si].mesh.num_face_vertices.end(),[](unsigned n){return n==3;});
   mesh.faceLocations.push_back({uint32_t(si),exactFaces?uint32_t(face):UINT32_MAX});auto part=key+"/material/"+mesh.materialKeys[slot];if(!mesh.primitives.empty()&&mesh.primitives.back().part==part)mesh.primitives.back().count+=3;else mesh.primitives.push_back({first,3,slot,part,int(si)});
  }
 }
 // Contiguous runs of one part can be separated by another material. Combine
 // index runs by stable shape/material identity without discarding seams.
 struct Part {int material=-1,node=-1;std::vector<uint32_t> indices;std::vector<ModelSourceLocation> faces;};
 std::map<std::string,Part> parts;for(auto& p:mesh.primitives){auto& item=parts[p.part];item.material=p.material;item.node=p.node;item.indices.insert(item.indices.end(),mesh.indices.begin()+p.first,mesh.indices.begin()+p.first+p.count);item.faces.insert(item.faces.end(),mesh.faceLocations.begin()+p.first/3,mesh.faceLocations.begin()+(p.first+p.count)/3);}mesh.indices.clear();mesh.primitives.clear();mesh.faceLocations.clear();for(auto& [key,item]:parts){auto first=unsigned(mesh.indices.size());mesh.indices.insert(mesh.indices.end(),item.indices.begin(),item.indices.end());mesh.faceLocations.insert(mesh.faceLocations.end(),item.faces.begin(),item.faces.end());mesh.primitives.push_back({first,unsigned(item.indices.size()),item.material,key,item.node});}
 if(mesh.vertices.empty())throw std::runtime_error("OBJ contains no triangles");
 if(!GenerateMeshTangents(mesh))throw std::runtime_error("OBJ tangents failed");
 std::sort(report.dependencies.begin(),report.dependencies.end());report.dependencies.erase(std::unique(report.dependencies.begin(),report.dependencies.end()),report.dependencies.end());if(!warning.empty())report.diagnostics.push_back({"warning","obj-source",path,"","Inspect source material declarations",warning});out=std::move(mesh);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool GatherObjDependencies(const std::string& path,std::vector<std::string>& paths,std::string& error){MeshData mesh;ModelImportSettings settings;ModelImportReport report;if(!ImportObjSource(path,settings,mesh,report,error))return false;paths=report.dependencies;return true;}
