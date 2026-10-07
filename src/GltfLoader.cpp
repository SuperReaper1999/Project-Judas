#include <glm/gtc/matrix_inverse.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "GltfLoader.h"
#include "SkeletalAnimation.h"
#include "PoseComposition.h"
#include "TextureLoader.h"
#include "Tangents.h"
#include <cstring>
#include <glm/gtx/matrix_decompose.hpp>
#include <memory>
#include <functional>
#include <set>
#include <cmath>
#include <stdexcept>
#include <filesystem>
#include <algorithm>
#include <fstream>
namespace {
struct AllocationBudget {size_t used=0;};
void ConfigureMemory(cgltf_options& options,AllocationBudget& budget){options.memory.user_data=&budget;options.memory.alloc_func=[](void* user,cgltf_size size)->void*{auto& b=*static_cast<AllocationBudget*>(user);if(size>512*1024*1024||b.used>512*1024*1024-size)return nullptr;b.used+=size;return std::malloc(size);};options.memory.free_func=[](void*,void* p){std::free(p);};}
void DecodeStrings(cgltf_data& data){
 auto decode=[](char* value){if(value)cgltf_decode_string(value);};
 for(size_t i=0;i<data.nodes_count;++i)decode(data.nodes[i].name);
 for(size_t i=0;i<data.materials_count;++i)decode(data.materials[i].name);
 for(size_t i=0;i<data.animations_count;++i)decode(data.animations[i].name);
 for(size_t i=0;i<data.images_count;++i)decode(data.images[i].uri);
 for(size_t i=0;i<data.buffers_count;++i)decode(data.buffers[i].uri);
}
// Explicit remaps are author choices, not a search through the user's disk.
std::filesystem::path Dependency(const std::filesystem::path& root,const std::string& raw,const std::map<std::string,std::string>* remaps){
 std::string name=raw;std::replace(name.begin(),name.end(),'\\','/');std::vector<char> decoded(name.begin(),name.end());decoded.push_back(0);cgltf_decode_uri(decoded.data());std::string key=decoded.data();
 if(remaps){auto it=remaps->find(name);if(it==remaps->end())it=remaps->find(key);if(it!=remaps->end()){auto path=std::filesystem::weakly_canonical(it->second);if(!std::filesystem::is_regular_file(path))throw std::runtime_error("missing explicit glTF remap: "+name);return path;}}
 auto path=std::filesystem::weakly_canonical(root/key);auto relative=path.lexically_relative(root);if(root.empty()||relative.empty()||relative.is_absolute()||*relative.begin()==".."||!std::filesystem::is_regular_file(path))throw std::runtime_error("missing/unapproved glTF dependency: "+name);return path;
}
bool Finite(glm::vec4 v){for(int i=0;i<4;++i)if(!std::isfinite(v[i]))return false;return true;}
glm::vec4 Read(cgltf_accessor* a,size_t i){cgltf_float v[4]={0,0,0,0};if(!a||!cgltf_accessor_read_float(a,i,v,4))throw std::runtime_error("invalid attribute/animation accessor");glm::vec4 result(v[0],v[1],v[2],v[3]);if(!Finite(result))throw std::runtime_error("non-finite attribute/animation");return result;}
}
bool ParseGltfMesh(const void* bytes,size_t size,MeshData& result,std::string& error){return ParseGltfMeshSource(bytes,size,"",result,error);}
bool ParseGltfMeshSource(const void* bytes,size_t size,const std::string& sourcePath,MeshData& result,std::string& error,std::vector<std::string>* dependencies,bool animationOnly,const std::map<std::string,std::string>* remaps){
 if(size>256*1024*1024){error="glTF source exceeds 256 MiB";return false;}
 AllocationBudget budget;cgltf_options options{};ConfigureMemory(options,budget);
 auto approved=sourcePath.empty()?std::filesystem::path():std::filesystem::weakly_canonical(std::filesystem::path(sourcePath).parent_path());
 struct Context{std::filesystem::path root;std::vector<std::string>* dependencies;const std::map<std::string,std::string>* remaps;size_t bytes=0;std::string error;};Context fileContext{approved,dependencies,remaps,0,{}};
 options.file.user_data=&fileContext;
 options.file.read=[](const cgltf_memory_options*,const cgltf_file_options* opts,const char* path,cgltf_size* size,void** data){
  auto& context=*static_cast<Context*>(opts->user_data);auto& root=context.root;
  std::error_code ec;std::string name=path;std::replace(name.begin(),name.end(),'\\','/');std::filesystem::path absolute;
  try{auto relative=std::filesystem::weakly_canonical(name).lexically_relative(root);absolute=Dependency(root,relative.generic_string(),context.remaps);}catch(const std::exception& e){context.error=e.what();return cgltf_result_io_error;}
  auto bytes=std::filesystem::file_size(absolute,ec);if(ec||bytes>256*1024*1024||context.bytes>512*1024*1024-bytes)return cgltf_result_io_error;context.bytes+=bytes;
  std::ifstream file(absolute,std::ios::binary);auto* buffer=std::malloc(bytes);if(!buffer)return cgltf_result_out_of_memory;
  if(!file.read(static_cast<char*>(buffer),bytes)){std::free(buffer);return cgltf_result_io_error;}*size=bytes;*data=buffer;if(context.dependencies)context.dependencies->push_back(absolute.string());return cgltf_result_success;
 };
 options.file.release=[](const cgltf_memory_options*,const cgltf_file_options*,void* data){std::free(data);};cgltf_data* raw=nullptr;auto status=cgltf_parse(&options,bytes,size,&raw);if(status!=cgltf_result_success){error="glTF parse failed ("+std::to_string(status)+")";return false;}
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
 try{
  DecodeStrings(*data);
  // Self-contained files keep normal asset identity, async cancellation and export
  // independent of unregistered auxiliary files or arbitrary external paths.
  for(size_t i=0;i<data->buffers_count;++i)if(sourcePath.empty()&&data->buffers[i].uri&&std::string(data->buffers[i].uri).rfind("data:",0)!=0)throw std::runtime_error("external glTF buffers unsupported; embed buffers or use GLB");
  if(cgltf_load_buffers(&options,data.get(),sourcePath.empty()?nullptr:sourcePath.c_str())!=cgltf_result_success||cgltf_validate(data.get())!=cgltf_result_success)throw std::runtime_error(fileContext.error.empty()?"invalid glTF buffers/accessors":fileContext.error);
  if(data->nodes_count>kModelNodeLimit)throw std::runtime_error("model hierarchy exceeds 4096 nodes");
  auto asset=std::make_shared<SkeletalAsset>();auto& skeleton=asset->skeleton;auto nodeIndex=[&](const cgltf_node* n){return int(n-data->nodes);};
  std::vector<unsigned char> visited(data->nodes_count,0);
  std::function<void(int,unsigned)> visitDepth=[&](int i,unsigned depth){if(depth>256)throw std::runtime_error("hierarchy depth exceeds 256");if(i<0||size_t(i)>=data->nodes_count)throw std::runtime_error("invalid hierarchy reference");if(visited[i]==1)throw std::runtime_error("cyclic skeleton");if(visited[i]==2)return;visited[i]=1;auto& n=data->nodes[i];if(n.parent)visitDepth(nodeIndex(n.parent),depth+1);visited[i]=2;skeleton.order.push_back(i);};
  for(size_t i=0;i<data->nodes_count;++i){const auto& n=data->nodes[i];JointTransform local;glm::mat4 fixed(1);
   if(n.has_matrix){cgltf_node_transform_local(&n,&fixed[0][0]);for(int c=0;c<4;++c)if(!Finite(fixed[c]))throw std::runtime_error("nonfinite affine node");}

   else {if(n.has_translation)local.translation={n.translation[0],n.translation[1],n.translation[2]};if(n.has_scale)local.scale={n.scale[0],n.scale[1],n.scale[2]};if(n.has_rotation)local.rotation={n.rotation[3],n.rotation[0],n.rotation[1],n.rotation[2]};}
   if(!Finite({local.translation,0})||!Finite({local.scale,0})||!Finite({local.rotation.x,local.rotation.y,local.rotation.z,local.rotation.w})||glm::dot(local.rotation,local.rotation)<1e-12f)throw std::runtime_error("invalid rest transform");
   local.rotation=glm::normalize(local.rotation);skeleton.names.push_back(n.name?n.name:std::to_string(i));skeleton.parents.push_back(n.parent?nodeIndex(n.parent):-1);skeleton.rest.local.push_back(local);skeleton.affine.push_back(fixed);visitDepth(int(i),0);
  }
  // Draw palettes retain per-skin binds, independent of the shared pose hierarchy.
  std::vector<size_t> skinOffsets(data->skins_count);
  for(size_t k=0;k<data->skins_count;++k){const auto* skin=&data->skins[k];skinOffsets[k]=skeleton.skinNodes.size();
   if(!skin->joints_count||skeleton.skinNodes.size()+skin->joints_count>kModelPaletteLimit)throw std::runtime_error("skin palette exceeds model resource bound");
   if(skin->inverse_bind_matrices&&skin->inverse_bind_matrices->count!=skin->joints_count)throw std::runtime_error("inverse bind count mismatch");
   for(size_t i=0;i<skin->joints_count;++i){glm::mat4 inverse(1);if(skin->inverse_bind_matrices&&!cgltf_accessor_read_float(skin->inverse_bind_matrices,i,&inverse[0][0],16))throw std::runtime_error("invalid inverse bind matrix");for(int c=0;c<4;++c)if(!Finite(inverse[c]))throw std::runtime_error("nonfinite inverse bind matrix");skeleton.skinNodes.push_back(nodeIndex(skin->joints[i]));skeleton.inverseBind.push_back(inverse);}
  }
  bool animated=data->skins_count||data->animations_count;
  MeshData mesh;for(size_t i=0;i<skeleton.names.size();++i)mesh.sourceNodes.push_back(SkeletonJointKey(skeleton,int(i)));
  for(size_t i=0;i<data->extensions_required_count;++i)if(std::string(data->extensions_required[i])!="KHR_materials_unlit"&&std::string(data->extensions_required[i])!="KHR_texture_transform")throw std::runtime_error("required glTF extension unsupported: "+std::string(data->extensions_required[i]));
  for(size_t i=0;i<data->extensions_used_count;++i)if(std::string(data->extensions_used[i])!="KHR_materials_unlit"&&std::string(data->extensions_used[i])!="KHR_texture_transform")mesh.importWarnings.push_back("Optional extension ignored: "+std::string(data->extensions_used[i]));
  auto map=[&](const cgltf_texture_view& view,MaterialMap& out){
   if(!view.texture)return;
   if(view.texcoord>1||(view.has_transform&&view.transform.texcoord>1))throw std::runtime_error("material UV channel must be UV0 or UV1");
   out.uvSet=view.has_transform&&view.transform.has_texcoord?view.transform.texcoord:view.texcoord;
   if(view.has_transform){out.scale={view.transform.scale[0],view.transform.scale[1]};out.offset={view.transform.offset[0],view.transform.offset[1]};out.rotation=view.transform.rotation;}
   const auto* texture=view.texture;const auto* image=texture->image;if(!image)throw std::runtime_error("missing core texture image");
   if(texture->sampler){auto& t=*texture->sampler;out.sampler={int(t.wrap_s),int(t.wrap_t),t.min_filter?int(t.min_filter):9987,t.mag_filter?int(t.mag_filter):9729};}
   const uint8_t* bytes=nullptr;size_t size=0;void* owned=nullptr;
   if(image->buffer_view){auto* buffer=image->buffer_view;bytes=static_cast<const uint8_t*>(buffer->buffer->data)+buffer->offset;size=buffer->size;}
   else if(image->uri&&std::strncmp(image->uri,"data:",5)==0){std::string uri=image->uri;auto comma=uri.find(',');if(comma==std::string::npos||uri.substr(0,comma).find(";base64")==std::string::npos)throw std::runtime_error("image data URI must use base64");size=(uri.size()-comma-1)*3/4;while(size&&uri.back()=='='){--size;uri.pop_back();}if(cgltf_load_buffer_base64(&options,size,image->uri+comma+1,&owned)!=cgltf_result_success)throw std::runtime_error("invalid embedded image");bytes=static_cast<uint8_t*>(owned);}
   else if(image->uri&&!approved.empty()){
    std::string uri=image->uri;auto path=Dependency(approved,uri,remaps);
    if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>64*1024*1024)throw std::runtime_error("missing/oversized external image: "+uri);
    std::ifstream file(path,std::ios::binary);std::vector<uint8_t> contents{std::istreambuf_iterator<char>(file),{}};
    if(!file||contents.empty()||contents.size()>64*1024*1024)throw std::runtime_error("missing/oversized external image: "+uri);
    out.encodedImage=contents;if(dependencies)dependencies->push_back(path.string());
    std::string decodeError;if(!DecodeTextureFromMemory(contents.data(),contents.size(),uri,out.embedded,decodeError))throw std::runtime_error(decodeError);return;
   }else throw std::runtime_error("external glTF image requires approved import source root");
   out.encodedImage.assign(bytes,bytes+size);std::string error;bool ok=DecodeTextureFromMemory(bytes,size,"embedded glTF image",out.embedded,error);free(owned);if(!ok)throw std::runtime_error(error);
  };
  for(size_t i=0;i<data->materials_count;++i){const auto& src=data->materials[i];MaterialDefinition m;m.flipV=true;m.model=src.unlit?MaterialModel::Unlit:MaterialModel::PBR;auto& pbr=src.pbr_metallic_roughness;m.baseColor={pbr.base_color_factor[0],pbr.base_color_factor[1],pbr.base_color_factor[2],pbr.base_color_factor[3]};m.metallic=pbr.metallic_factor;m.roughness=pbr.roughness_factor;m.emissive={src.emissive_factor[0],src.emissive_factor[1],src.emissive_factor[2]};m.alpha=src.alpha_mode==cgltf_alpha_mode_mask?MaterialAlpha::Mask:src.alpha_mode==cgltf_alpha_mode_blend?MaterialAlpha::Blend:MaterialAlpha::Opaque;m.alphaCutoff=src.alpha_cutoff;m.doubleSided=src.double_sided;m.normalStrength=src.normal_texture.scale;m.occlusionStrength=src.occlusion_texture.scale;map(pbr.base_color_texture,m.maps[0]);map(pbr.metallic_roughness_texture,m.maps[1]);map(src.normal_texture,m.maps[2]);map(src.occlusion_texture,m.maps[3]);map(src.emissive_texture,m.maps[4]);std::string error;if(!ValidateMaterial(m,error))throw std::runtime_error(error);mesh.materialKeys.push_back(src.name?src.name:"unnamed-material");mesh.materials.push_back(std::move(m));}
  bool missingTangents=false;
  size_t meshNodes=0;
  for(size_t ni=0;ni<data->nodes_count;++ni){const auto* meshNode=&data->nodes[ni];auto* source=meshNode->mesh;if(!source)continue;++meshNodes;
   const cgltf_skin* skin=meshNode->skin;size_t paletteBase=skin?skinOffsets[size_t(skin-data->skins)]:skeleton.skinNodes.size();
   if(animated&&!skin){skeleton.skinNodes.push_back(int(ni));skeleton.inverseBind.push_back(glm::mat4(1));}
   glm::mat4 nodeTransform;cgltf_node_transform_world(meshNode,&nodeTransform[0][0]);
   if(std::abs(glm::determinant(glm::mat3(nodeTransform)))<1e-8f)throw std::runtime_error("singular mesh-node transform");
  for(size_t p=0;p<source->primitives_count;++p){const auto& primitive=source->primitives[p];if(primitive.type!=cgltf_primitive_type_triangles||primitive.targets_count||primitive.has_draco_mesh_compression)throw std::runtime_error("requires uncompressed triangles without morph targets");
   cgltf_accessor *positions=nullptr,*normals=nullptr,*uv=nullptr,*joints=nullptr,*weights=nullptr,*joints1=nullptr,*weights1=nullptr,*uv1=nullptr,*tangents=nullptr;
   for(size_t a=0;a<primitive.attributes_count;++a){const auto& attr=primitive.attributes[a];if(attr.type==cgltf_attribute_type_tangent)tangents=attr.data;else if(attr.type==cgltf_attribute_type_position)positions=attr.data;else if(attr.type==cgltf_attribute_type_normal)normals=attr.data;else if(attr.type==cgltf_attribute_type_texcoord){if(attr.index==0)uv=attr.data;else if(attr.index==1)uv1=attr.data;else throw std::runtime_error("mesh UV channels beyond UV1 unsupported");}else if(attr.type==cgltf_attribute_type_joints){if(attr.index==0)joints=attr.data;else if(attr.index==1)joints1=attr.data;else throw std::runtime_error("more than eight skin influences unsupported");}else if(attr.type==cgltf_attribute_type_weights){if(attr.index==0)weights=attr.data;else if(attr.index==1)weights1=attr.data;else throw std::runtime_error("more than eight skin influences unsupported");}}
   if(!positions||positions->type!=cgltf_type_vec3||(skin&&(!joints||!weights||joints->type!=cgltf_type_vec4||weights->type!=cgltf_type_vec4||joints->count!=positions->count||weights->count!=positions->count))||(normals&&(normals->count!=positions->count||normals->type!=cgltf_type_vec3))||(uv&&(uv->count!=positions->count||uv->type!=cgltf_type_vec2)))throw std::runtime_error("incompatible skin vertex attributes");
   if(mesh.vertices.size()+positions->count>2000000)throw std::runtime_error("model vertex resource bound");
   if(primitive.material){auto material=primitive.material-data->materials;for(auto& map:mesh.materials[material].maps)if((!map.embedded.pixels.empty()||!map.asset.empty())&&map.uvSet==1&&!uv1)throw std::runtime_error("material requires missing UV1");}
   auto base=uint32_t(mesh.vertices.size());for(size_t i=0;i<positions->count;++i){MeshVertex v;MeshSkinVertex influence;v.position=glm::vec3(Read(positions,i));v.normal=normals?glm::vec3(Read(normals,i)):glm::vec3(0);v.uv=uv?glm::vec2(Read(uv,i)):glm::vec2(0);v.uv1=uv1?glm::vec2(Read(uv1,i)):v.uv;if(tangents){if(tangents->count!=positions->count||tangents->type!=cgltf_type_vec4)throw std::runtime_error("invalid tangent accessor");v.tangent=Read(tangents,i);if(glm::length(glm::vec3(v.tangent))<1e-6f||abs(v.tangent.w)!=1)throw std::runtime_error("invalid tangent direction/handedness");}else missingTangents=true;if(skin){auto ji=Read(joints,i);influence.weights=Read(weights,i);float sum=0;for(int k=0;k<4;++k){if(ji[k]<0||ji[k]>=skin->joints_count||std::floor(ji[k])!=ji[k]||influence.weights[k]<0)throw std::runtime_error("invalid skin index/weight");influence.joints[k]=uint32_t(ji[k]);influence.joints[k]+=uint32_t(paletteBase);sum+=influence.weights[k];}if(joints1||weights1){if(!joints1||!weights1||joints1->count!=positions->count||weights1->count!=positions->count)throw std::runtime_error("invalid second influence set");auto ji1=Read(joints1,i);influence.weights1=Read(weights1,i);for(int k=0;k<4;++k){if(ji1[k]<0||ji1[k]>=skin->joints_count||std::floor(ji1[k])!=ji1[k]||influence.weights1[k]<0)throw std::runtime_error("invalid second skin index/weight");influence.joints1[k]=uint32_t(ji1[k])+uint32_t(paletteBase);sum+=influence.weights1[k];}}
if(!std::isfinite(sum)||sum<1e-8f)throw std::runtime_error("zero skin weights");
 influence.weights/=sum;influence.weights1/=sum;
}else if(animated)influence.joints.x=uint32_t(paletteBase);
if(!animated){auto linear=glm::mat3(nodeTransform);v.position=glm::vec3(nodeTransform*glm::vec4(v.position,1));v.normal=glm::inverseTranspose(linear)*v.normal;v.tangent=glm::vec4(glm::normalize(linear*glm::vec3(v.tangent)),v.tangent.w*(glm::determinant(linear)<0?-1.f:1.f));}
mesh.vertexLocations.push_back({uint32_t(ni),uint32_t(i)});mesh.sourceVertexIds.push_back(uint32_t(base+i));mesh.vertices.push_back(v);if(animated)mesh.skinVertices.push_back(influence);}
   unsigned first=unsigned(mesh.indices.size());size_t count=primitive.indices?primitive.indices->count:positions->count;if(mesh.indices.size()+count>6000000)throw std::runtime_error("model index resource bound");if(count%3)throw std::runtime_error("invalid triangle count");for(size_t i=0;i<count;++i){size_t index=primitive.indices?cgltf_accessor_read_index(primitive.indices,i):i;if(index>=positions->count)throw std::runtime_error("mesh index out of range");if(i%3==0)mesh.faceLocations.push_back({uint32_t(ni),uint32_t(p*2000000+i/3)});mesh.indices.push_back(base+uint32_t(index));}
   if(!primitive.material&&mesh.materials.size()==data->materials_count){MaterialDefinition fallback;fallback.flipV=true;fallback.metallic=1;fallback.roughness=1;mesh.materials.push_back(fallback);mesh.materialKeys.push_back("default");}
   std::string key=SkeletonJointKey(skeleton,int(ni));
   mesh.primitives.push_back({first,unsigned(count),primitive.material?int(primitive.material-data->materials):int(data->materials_count),key+"/material/"+mesh.materialKeys.at(primitive.material?size_t(primitive.material-data->materials):data->materials_count),int(ni)});
   if(!animated&&glm::determinant(glm::mat3(nodeTransform))<0)for(size_t i=first;i<first+count;i+=3)std::swap(mesh.indices[i+1],mesh.indices[i+2]);
   if(!normals)for(size_t i=mesh.indices.size()-count;i<mesh.indices.size();i+=3){auto& a=mesh.vertices[mesh.indices[i]];auto& b=mesh.vertices[mesh.indices[i+1]];auto& c=mesh.vertices[mesh.indices[i+2]];auto n=glm::cross(b.position-a.position,c.position-a.position);a.normal+=n;b.normal+=n;c.normal+=n;}
  }
  }
  if(!meshNodes&&!animationOnly)throw std::runtime_error("missing mesh");
  for(auto& v:mesh.vertices)v.normal=glm::length(v.normal)>1e-8f?glm::normalize(v.normal):glm::vec3(0,1,0);
  std::set<std::string> clipNames;
  for(size_t i=0;i<data->animations_count;++i){const auto& animation=data->animations[i];AnimationClip clip;clip.name=animation.name?animation.name:"Clip "+std::to_string(i);if(!clipNames.insert(clip.name).second)throw std::runtime_error("duplicate animation names");std::set<std::pair<int,int>> channels;
   for(size_t c=0;c<animation.channels_count;++c){const auto& channel=animation.channels[c];if(!channel.target_node||channel.target_node->has_matrix)throw std::runtime_error("invalid animation target; matrix nodes cannot receive TRS tracks");AnimationTrack t;t.node=nodeIndex(channel.target_node);t.path=channel.target_path==cgltf_animation_path_type_translation?TrackPath::Translation:channel.target_path==cgltf_animation_path_type_rotation?TrackPath::Rotation:TrackPath::Scale;if(channel.target_path!=cgltf_animation_path_type_translation&&channel.target_path!=cgltf_animation_path_type_rotation&&channel.target_path!=cgltf_animation_path_type_scale)throw std::runtime_error("morph animation unsupported");if(!channels.insert({t.node,int(t.path)}).second)throw std::runtime_error("duplicate animation channel");
    const auto& sampler=*channel.sampler;t.interpolation=sampler.interpolation==cgltf_interpolation_type_step?TrackInterpolation::Step:sampler.interpolation==cgltf_interpolation_type_cubic_spline?TrackInterpolation::CubicSpline:TrackInterpolation::Linear;
    if(sampler.input->type!=cgltf_type_scalar||sampler.output->type!=(t.path==TrackPath::Rotation?cgltf_type_vec4:cgltf_type_vec3)||sampler.output->count!=sampler.input->count*(t.interpolation==TrackInterpolation::CubicSpline?3:1)||!sampler.input->count)throw std::runtime_error("invalid animation sample dimensions");
    for(size_t k=0;k<sampler.input->count;++k){float time=Read(sampler.input,k).x;if(time<0||(!t.times.empty()&&time<=t.times.back()))throw std::runtime_error("animation times must strictly increase");t.times.push_back(time);clip.duration=std::max(clip.duration,time);}
    for(size_t k=0;k<sampler.output->count;++k){auto v=Read(sampler.output,k);if(t.path==TrackPath::Rotation&&(t.interpolation!=TrackInterpolation::CubicSpline||k%3==1)&&glm::dot(v,v)<1e-12f)throw std::runtime_error("zero rotation key");if(t.path==TrackPath::Rotation&&t.interpolation!=TrackInterpolation::CubicSpline)v=glm::normalize(v);t.values.push_back(v);}clip.tracks.push_back(std::move(t));
   }asset->clips.push_back(std::move(clip));
  }
  if(mesh.vertices.empty()&&!animationOnly)throw std::runtime_error("empty skinned mesh");
  // Primitives sharing a node/material/skin are one stable draw part. This
  // retains every triangle without inventing source-array-index identities.
  std::map<std::string,std::vector<MeshPrimitive>> groups;std::vector<std::string> groupOrder;for(auto& part:mesh.primitives){if(!groups.count(part.part))groupOrder.push_back(part.part);groups[part.part].push_back(part);}std::vector<uint32_t> regrouped;std::vector<ModelSourceLocation> provenance;std::vector<MeshPrimitive> merged;for(auto& key:groupOrder){auto parts=groups.at(key);auto part=parts.front();part.first=unsigned(regrouped.size());part.count=0;for(auto& p:parts){regrouped.insert(regrouped.end(),mesh.indices.begin()+p.first,mesh.indices.begin()+p.first+p.count);if(!mesh.faceLocations.empty())provenance.insert(provenance.end(),mesh.faceLocations.begin()+p.first/3,mesh.faceLocations.begin()+(p.first+p.count)/3);part.count+=p.count;}merged.push_back(part);}mesh.indices=std::move(regrouped);mesh.faceLocations=std::move(provenance);mesh.primitives=std::move(merged);
  if(missingTangents&&!GenerateMeshTangents(mesh))throw std::runtime_error("tangent generation failed");
  if(animated)mesh.skeletal=std::move(asset);
  result=std::move(mesh);error.clear();return true;
 }catch(const std::exception& e){error=std::string("glTF: ")+e.what();return false;}
}

bool GatherGltfDependencies(const std::string& source,std::vector<std::string>& result,std::string& error,const std::map<std::string,std::string>* remaps){
 AllocationBudget budget;cgltf_options options{};ConfigureMemory(options,budget);cgltf_data* data=nullptr;if(std::filesystem::file_size(source)>256*1024*1024){error="glTF source bound";return false;}auto status=cgltf_parse_file(&options,source.c_str(),&data);if(status!=cgltf_result_success){error="cannot parse source glTF dependencies";return false;}std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(data,cgltf_free);try{DecodeStrings(*data);
  auto root=std::filesystem::weakly_canonical(std::filesystem::path(source).parent_path());std::vector<std::string> paths;auto add=[&](const char* uri){if(!uri||std::strncmp(uri,"data:",5)==0)return;auto path=Dependency(root,uri,remaps);paths.push_back(path.string());};
  for(size_t i=0;i<data->buffers_count;++i)add(data->buffers[i].uri);
  for(size_t i=0;i<data->images_count;++i)add(data->images[i].uri);
  result=std::move(paths);return true;
 }catch(const std::exception& e){error=e.what();return false;}}
