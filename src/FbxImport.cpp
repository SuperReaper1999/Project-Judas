#include "PerformanceProfiler.h"
#include "ModelImport.h"
#include "TextureLoader.h"
#include "Tangents.h"
#include "ModelLoader.h"
#include "GltfLoader.h"
#include "AsyncFile.h"
#include "PoseComposition.h"
#include "../third_party/ufbx/ufbx.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <map>
#include <set>
#include <cmath>
#include <stdexcept>
#include <algorithm>
bool ImportObjSource(const std::string&,const ModelImportSettings&,MeshData&,ModelImportReport&,std::string&);
bool GatherObjDependencies(const std::string&,std::vector<std::string>&,std::string&);
namespace {
using ScenePtr=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
glm::vec3 V(ufbx_vec3 v){return {v.x,v.y,v.z};}
glm::quat Q(ufbx_quat q){return glm::normalize(glm::quat(q.w,q.x,q.y,q.z));}
glm::mat4 M(ufbx_matrix m){return {{m.m00,m.m10,m.m20,0},{m.m01,m.m11,m.m21,0},{m.m02,m.m12,m.m22,0},{m.m03,m.m13,m.m23,1}};}
std::string S(ufbx_string s){return std::string(s.data,s.length);}
ScenePtr Load(const std::string& path,const ModelImportSettings& settings){
 ufbx_load_opts opts{};opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1;opts.space_conversion=UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;opts.generate_missing_normals=true;opts.load_external_files=false;
 if(!std::isfinite(glm::dot(settings.basisRotation,settings.basisRotation))||std::abs(glm::length(settings.basisRotation)-1)>1e-4f)throw std::runtime_error("basis override requires a normalized quaternion");
 opts.use_root_transform=settings.basisRotation!=glm::quat(1,0,0,0);opts.root_transform=ufbx_identity_transform;auto q=settings.basisRotation;opts.root_transform.rotation={q.x,q.y,q.z,q.w};
 opts.geometry_transform_handling=UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
 opts.inherit_mode_handling=UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
 opts.node_depth_limit=256;opts.temp_allocator.memory_limit=512*1024*1024;opts.result_allocator.memory_limit=512*1024*1024;
 opts.progress_cb.fn=[](void* user,const ufbx_progress*){auto* s=static_cast<const ModelImportSettings*>(user);return s->cancelled&&s->cancelled()?UFBX_PROGRESS_CANCEL:UFBX_PROGRESS_CONTINUE;};opts.progress_cb.user=const_cast<ModelImportSettings*>(&settings);
 ufbx_error error{};ScenePtr scene(ufbx_load_file(path.c_str(),&opts,&error),ufbx_free_scene);
 if(!scene){char message[1024];ufbx_format_error(message,sizeof(message),&error);throw std::runtime_error(message);}
 if(scene->nodes.count>kModelNodeLimit)throw std::runtime_error("FBX hierarchy exceeds 4096 nodes");
 if(settings.sourceUnitMeters>0){if(!std::isfinite(settings.sourceUnitMeters)||settings.sourceUnitMeters<1e-6||settings.sourceUnitMeters>1000)throw std::runtime_error("source unit override must be 1e-6..1000 metres");opts.target_unit_meters=scene->settings.unit_meters/settings.sourceUnitMeters;scene.reset();scene.reset(ufbx_load_file(path.c_str(),&opts,&error));if(!scene)throw std::runtime_error("FBX source unit override failed");}
 return scene;
}
Skeleton Hierarchy(const ufbx_scene& scene){Skeleton s;std::map<uint32_t,int> ids;for(size_t i=0;i<scene.nodes.count;++i)ids[scene.nodes.data[i]->element_id]=int(i);
 std::set<int> done;std::function<void(int)> visit=[&](int i){if(done.count(i))return;auto* n=scene.nodes.data[i];if(n->parent)visit(ids.at(n->parent->element_id));done.insert(i);s.order.push_back(i);};
 for(size_t i=0;i<scene.nodes.count;++i){auto* n=scene.nodes.data[i];s.names.push_back(n->is_root?"__root":S(n->name));s.parents.push_back(n->parent?ids.at(n->parent->element_id):-1);auto& t=n->local_transform;s.rest.local.push_back({V(t.translation),V(t.scale),Q(t.rotation)});visit(int(i));}
 return s;
}
std::string Key(const Skeleton& s,int i){return SkeletonJointKey(s,i);}
std::vector<AnimationClip> Clips(const ufbx_scene& scene,const ModelImportSettings& settings,const std::map<uint32_t,int>& mapping){
 if(!std::isfinite(settings.sampleRate)||settings.sampleRate<1||settings.sampleRate>240)throw std::runtime_error("FBX sample rate must be 1..240 Hz");
 std::vector<AnimationClip> clips;for(size_t a=0;a<scene.anim_stacks.count;++a){auto* stack=scene.anim_stacks.data[a];ufbx_bake_opts opts{};opts.resample_rate=settings.sampleRate;opts.minimum_sample_rate=settings.sampleRate;opts.trim_start_time=true;opts.key_reduction_enabled=true;opts.key_reduction_rotation=true;opts.key_reduction_threshold=1e-6;opts.step_handling=UFBX_BAKE_STEP_HANDLING_IDENTICAL_TIME;
 opts.temp_allocator.memory_limit=256*1024*1024;opts.result_allocator.memory_limit=256*1024*1024;
 ufbx_error error{};std::unique_ptr<ufbx_baked_anim,decltype(&ufbx_free_baked_anim)> baked(ufbx_bake_anim(&scene,stack->anim,&opts,&error),ufbx_free_baked_anim);if(!baked)throw std::runtime_error("FBX animation bake failed: "+S(error.description));
 auto keyTime=[](double seconds,ufbx_baked_key_flags flags){float time=float(seconds);if(flags&UFBX_BAKED_KEY_STEP_LEFT)time=std::nextafter(time,-INFINITY);if(flags&UFBX_BAKED_KEY_STEP_RIGHT)time=std::nextafter(time,INFINITY);return time;};
 AnimationClip clip;clip.name=S(stack->name);if(clip.name.empty())clip.name="Take "+std::to_string(a);clip.duration=float(baked->playback_time_end-baked->playback_time_begin);
 for(auto& node:baked->nodes){auto it=mapping.find(node.element_id);if(it==mapping.end())throw std::runtime_error("animated node has no verified target mapping: "+std::to_string(node.element_id));
  auto vec=[&](ufbx_baked_vec3_list keys,TrackPath path){if(!keys.count)return;AnimationTrack t;t.node=it->second;t.path=path;for(auto& k:keys){auto time=keyTime(k.time,k.flags);if(time<0||time>clip.duration)continue;if(!t.times.empty()&&time<=t.times.back())throw std::runtime_error("animation discontinuities exceed float time resolution");t.times.push_back(time);t.values.push_back({V(k.value),0});}clip.tracks.push_back(std::move(t));};vec(node.translation_keys,TrackPath::Translation);vec(node.scale_keys,TrackPath::Scale);
  if(node.rotation_keys.count){AnimationTrack t;t.node=it->second;t.path=TrackPath::Rotation;for(auto& k:node.rotation_keys){auto q=Q(k.value);auto time=keyTime(k.time,k.flags);if(time<0||time>clip.duration)continue;if(!t.times.empty()&&time<=t.times.back())throw std::runtime_error("animation discontinuities exceed float time resolution");t.times.push_back(time);t.values.push_back({q.x,q.y,q.z,q.w});}clip.tracks.push_back(std::move(t));}
 }
 clips.push_back(std::move(clip));}
 return clips;
}
void Image(const ufbx_material_map& src,MaterialMap& target,const std::string& source,const ModelImportSettings& settings,ModelImportReport& report){
 if(!src.texture||!src.texture_enabled)return;
 auto* t=src.texture;auto uvName=S(t->uv_set);if(!uvName.empty()){auto it=std::find(report.uvSets.begin(),report.uvSets.end(),uvName);if(it==report.uvSets.end())throw std::runtime_error("unknown/ambiguous material UV set: "+uvName);target.uvSet=int(it-report.uvSets.begin());if(target.uvSet>1)throw std::runtime_error("material UV set beyond UV1");}
 target.sampler.wrapS=t->wrap_u==UFBX_WRAP_CLAMP?33071:10497;target.sampler.wrapT=t->wrap_v==UFBX_WRAP_CLAMP?33071:10497;
 if(t->has_uv_transform){auto transform=t->uv_to_texture;glm::vec2 x(transform.m00,transform.m10),y(transform.m01,transform.m11);if(glm::length(x)<1e-8f||glm::length(y)<1e-8f||std::abs(glm::dot(glm::normalize(x),glm::normalize(y)))>1e-5f)throw std::runtime_error("UV shear/singular texture transform requires material remap");target.rotation=std::atan2(x.y,x.x);target.scale={glm::length(x),glm::length(y)*(glm::determinant(glm::mat2(x,y))<0?-1.f:1.f)};target.offset={transform.m03,transform.m13};}
 if(t->type!=UFBX_TEXTURE_FILE)throw std::runtime_error("FBX layered/procedural texture requires explicit material remap: "+S(t->name));
 std::string error;if(t->content.size){target.encodedImage.assign(static_cast<const uint8_t*>(t->content.data),static_cast<const uint8_t*>(t->content.data)+t->content.size);if(!DecodeTextureFromMemory(static_cast<const uint8_t*>(t->content.data),t->content.size,S(t->name),target.embedded,error))throw std::runtime_error(error);return;}
 std::string name=S(t->relative_filename);if(name.empty())name=S(t->filename);std::replace(name.begin(),name.end(),'\\','/');auto root=std::filesystem::weakly_canonical(std::filesystem::path(source).parent_path());std::filesystem::path path;
 auto remap=settings.dependencyRemaps.find(name);if(remap!=settings.dependencyRemaps.end())path=std::filesystem::weakly_canonical(remap->second);else {path=std::filesystem::weakly_canonical(root/name);auto relative=path.lexically_relative(root);if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("texture outside approved source root; remap required: "+name);}
 if(std::filesystem::file_size(path)>64*1024*1024)throw std::runtime_error("texture exceeds 64 MiB");
 std::vector<uint8_t> bytes;if(!ReadWholeFile(path.string(),bytes,error)||!DecodeTextureFromMemory(bytes.data(),bytes.size(),name,target.embedded,error))throw std::runtime_error("texture "+name+": "+error);target.encodedImage=std::move(bytes);report.dependencies.push_back(path.string());
}
}
bool ImportModelSource(const std::string& path,const ModelImportSettings& settings,MeshData& result,ModelImportReport& report,std::string& error){JUDAS_PROFILE_SCOPE("Model source normalization");try{
 if(std::filesystem::file_size(path)>256*1024*1024)throw std::runtime_error("source file exceeds 256 MiB");
 auto ext=std::filesystem::path(path).extension().string();if(ext!=".fbx"&&ext!=".FBX"){
  MeshData mesh;std::vector<uint8_t> bytes;if(!ReadWholeFile(path,bytes,error))return false;
  bool ok=(ext==".glb"||ext==".gltf")?ParseGltfMeshSource(bytes.data(),bytes.size(),path,mesh,error,&report.dependencies,false,&settings.dependencyRemaps):(ext==".obj"||ext==".OBJ")?ImportObjSource(path,settings,mesh,report,error):LoadModelMesh(path,mesh,error);if(!ok)return false;
  if(!std::isfinite(glm::length(settings.basisRotation))||std::abs(glm::length(settings.basisRotation)-1)>1e-4f)throw std::runtime_error("basis requires normalized quaternion");
  auto basis=glm::mat4_cast(settings.basisRotation);float units=settings.sourceUnitMeters>0?float(settings.sourceUnitMeters):1.f;if(!std::isfinite(units)||units<1e-6f||units>1000)throw std::runtime_error("unit override bound");basis*=glm::scale(glm::mat4(1),glm::vec3(units));
  if(mesh.skeletal){auto shared=std::make_shared<SkeletalAsset>(*mesh.skeletal);auto& s=shared->skeleton;if(s.affine.empty())s.affine.assign(s.names.size(),glm::mat4(1));for(size_t i=0;i<s.parents.size();++i)if(s.parents[i]<0)s.affine[i]=basis*s.affine[i];mesh.skeletal=shared;}
  else for(auto& v:mesh.vertices){v.position=glm::vec3(basis*glm::vec4(v.position,1));v.normal=glm::normalize(glm::inverseTranspose(glm::mat3(basis))*v.normal);v.tangent=glm::vec4(glm::normalize(glm::mat3(basis)*glm::vec3(v.tangent)),v.tangent.w);}
  result=std::move(mesh);report.hierarchyNodes=result.skeletal?result.skeletal->skeleton.names.size():0;report.skinJoints=result.skeletal?result.skeletal->skeleton.skinNodes.size():0;report.parts=result.primitives.size();report.vertices=result.vertices.size();return true;
 }
 auto source=Load(path,settings);auto& scene=*source;MeshData mesh;auto asset=std::make_shared<SkeletalAsset>();asset->skeleton=Hierarchy(scene);auto& s=asset->skeleton;report.sourceUnitMeters=scene.settings.unit_meters;report.sourceBones=scene.bones.count;report.hierarchyNodes=scene.nodes.count;
 std::map<uint32_t,int> ids;std::set<std::string> keys;for(size_t i=0;i<scene.nodes.count;++i){ids[scene.nodes.data[i]->element_id]=int(i);auto key=Key(s,int(i));if(!keys.insert(key).second)throw std::runtime_error("ambiguous hierarchy path: "+key);mesh.sourceNodes.push_back(key);}asset->clips=Clips(scene,settings,ids);
 std::map<std::pair<uint32_t,uint32_t>,uint32_t> positionIds;
 for(auto* node:scene.nodes)if(node->mesh)for(size_t i=0;i<node->mesh->uv_sets.count;++i){if(i>1)throw std::runtime_error("FBX UV sets beyond UV1 require material remap");auto name=S(node->mesh->uv_sets.data[i].name);if(report.uvSets.size()<=i)report.uvSets.resize(i+1);if(!report.uvSets[i].empty()&&report.uvSets[i]!=name)throw std::runtime_error("inconsistent named UV channels across mesh parts require source/material remap");report.uvSets[i]=name;}
 for(auto* material:scene.materials){MaterialDefinition d;d.flipV=false;auto& p=material->pbr;auto& f=material->fbx;auto color=p.base_color.has_value?p.base_color.value_vec3:f.diffuse_color.value_vec3;d.baseColor={V(color),1};if(p.base_factor.has_value)d.baseColor*=float(p.base_factor.value_real);d.roughness=p.roughness.has_value?float(p.roughness.value_real):.65f;d.metallic=p.metalness.has_value?float(p.metalness.value_real):p.metalness.texture?1:0;d.emissive=V(p.emission_color.value_vec3);d.emissiveIntensity=p.emission_factor.has_value?float(p.emission_factor.value_real):1;d.doubleSided=material->features.double_sided.enabled;d.baseColor.a=p.opacity.has_value?float(p.opacity.value_real):float(1-(f.transparency_factor.has_value?f.transparency_factor.value_real:1)*std::max({f.transparency_color.value_vec3.x,f.transparency_color.value_vec3.y,f.transparency_color.value_vec3.z}));if(d.baseColor.a<1)d.alpha=MaterialAlpha::Blend;Image(p.base_color.texture?p.base_color:f.diffuse_color,d.maps[0],path,settings,report);Image(p.normal_map.texture?p.normal_map:f.normal_map,d.maps[2],path,settings,report);Image(p.emission_color,d.maps[4],path,settings,report);Image(p.ambient_occlusion,d.maps[3],path,settings,report);
 MaterialMap opacity;bool inverseOpacity=!p.opacity.texture&&f.transparency_color.texture;Image(p.opacity.texture?p.opacity:f.transparency_color,opacity,path,settings,report);
 if(!opacity.embedded.pixels.empty()){auto& base=d.maps[0];if(!base.embedded.pixels.empty()&&(base.uvSet!=opacity.uvSet||base.scale!=opacity.scale||base.offset!=opacity.offset||base.rotation!=opacity.rotation))throw std::runtime_error("opacity and colour use different UV transforms; remap to packed base colour");if(base.embedded.pixels.empty()){base=opacity;base.embedded.pixels.assign(opacity.embedded.pixels.size(),255);}base.encodedImage.clear();auto& image=base.embedded;for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){int u=x*opacity.embedded.width/image.width,v=y*opacity.embedded.height/image.height;auto alpha=opacity.embedded.pixels[(size_t(v)*opacity.embedded.width+u)*4];image.pixels[(size_t(y)*image.width+x)*4+3]=inverseOpacity?255-alpha:alpha;}d.alpha=MaterialAlpha::Blend;report.diagnostics.push_back({"warning","opacity-conversion",path,S(material->name),"Choose a project mask material if authored cutout is intended","Source opacity is packed into base colour alpha; blend is the explicit approximation"});}
 else for(size_t i=3;i<d.maps[0].embedded.pixels.size();i+=4)if(d.maps[0].embedded.pixels[i]<255){d.alpha=MaterialAlpha::Blend;break;}
 MaterialMap rough,metal;Image(p.roughness,rough,path,settings,report);Image(p.metalness,metal,path,settings,report);
 if(!rough.embedded.pixels.empty()||!metal.embedded.pixels.empty()){auto& reference=!rough.embedded.pixels.empty()?rough:metal;d.maps[1]=reference;auto& image=d.maps[1].embedded;int width=image.width,height=image.height;d.maps[1].encodedImage.clear();image.pixels.assign(size_t(width)*height*4,255);auto compatible=[&](const MaterialMap& m){return m.embedded.pixels.empty()||(m.uvSet==reference.uvSet&&m.scale==reference.scale&&m.offset==reference.offset&&m.rotation==reference.rotation);};if(!compatible(rough)||!compatible(metal))throw std::runtime_error("metal/roughness maps use incompatible UV transforms; remap to a packed Judas material");auto value=[](const TextureData& image,int x,int y,int width,int height){if(image.pixels.empty())return uint8_t(255);int u=x*image.width/width,v=y*image.height/height;return image.pixels[(size_t(v)*image.width+u)*4];};for(int y=0;y<height;++y)for(int x=0;x<width;++x){size_t i=(size_t(y)*width+x)*4;image.pixels[i+1]=value(rough.embedded,x,y,width,height);image.pixels[i+2]=value(metal.embedded,x,y,width,height);}}
 std::string materialError;if(!ValidateMaterial(d,materialError))throw std::runtime_error(materialError);mesh.materialKeys.push_back(S(material->name));mesh.materials.push_back(std::move(d));}
 if(mesh.materials.empty()){mesh.materials.emplace_back();mesh.materialKeys.push_back("default");}
 std::map<uint32_t,int> materials;for(size_t i=0;i<scene.materials.count;++i)materials[scene.materials.data[i]->element_id]=int(i);
 for(size_t ni=0;ni<scene.nodes.count;++ni){auto* node=scene.nodes.data[ni];auto* src=node->mesh;if(!src)continue;
  if(src->num_indices>6000000||mesh.vertices.size()+src->num_indices>2000000)throw std::runtime_error("FBX geometry allocation bound");
  if((src->blend_deformers.count||src->cache_deformers.count)&&!settings.allowBaseMesh)throw std::runtime_error("unsupported morph/cache deformation; explicitly select base mesh to import with loss");
  if(src->uv_sets.count>2)throw std::runtime_error("FBX UV sets beyond UV1 require explicit material remap");
  if(src->skin_deformers.count>1)throw std::runtime_error("multiple deformers on one mesh unsupported");
  auto* skin=src->skin_deformers.count?src->skin_deformers.data[0]:nullptr;
  auto paletteBase=s.skinNodes.size();if(skin){if(skin->skinning_method!=UFBX_SKINNING_METHOD_LINEAR)throw std::runtime_error("dual-quaternion skinning is unsupported; select linear source export");for(auto* c:skin->clusters){s.skinNodes.push_back(ids.at(c->bone_node->element_id));s.inverseBind.push_back(M(c->geometry_to_bone));}}
  else{s.skinNodes.push_back(int(ni));s.inverseBind.push_back(M(node->geometry_to_node));}
  if(s.skinNodes.size()>kModelPaletteLimit)throw std::runtime_error("FBX palette allocation bound");
  for(size_t mi=0;mi<std::max(size_t(1),node->materials.count);++mi){unsigned first=mesh.indices.size();std::map<uint32_t,uint32_t> corners;
   for(size_t fi=0;fi<src->faces.count;++fi){if(src->face_material.count&&src->face_material.data[fi]!=mi)continue;auto face=src->faces.data[fi];if(face.num_indices<3)continue;std::vector<uint32_t> triangle((face.num_indices-2)*3);auto count=ufbx_triangulate_face(triangle.data(),triangle.size(),src,face);
    for(size_t k=0;k<size_t(count)*3;++k){if(k%3==0)mesh.faceLocations.push_back({uint32_t(ni),uint32_t(fi)});uint32_t corner=triangle[k];auto found=corners.find(corner);if(found!=corners.end()){mesh.indices.push_back(found->second);continue;}
     MeshVertex v;v.position=V(ufbx_get_vertex_vec3(&src->vertex_position,corner));v.normal=V(ufbx_get_vertex_vec3(&src->vertex_normal,corner));if(src->vertex_uv.exists){auto uv=ufbx_get_vertex_vec2(&src->vertex_uv,corner);v.uv={uv.x,uv.y};}v.uv1=v.uv;if(src->uv_sets.count>1){auto uv=ufbx_get_vertex_vec2(&src->uv_sets.data[1].vertex_uv,corner);v.uv1={uv.x,uv.y};}
     MeshSkinVertex w;w.joints.x=uint32_t(paletteBase);if(skin){auto vertex=src->vertex_indices.data[corner];auto weights=skin->vertices.data[vertex];if(weights.num_weights>8)throw std::runtime_error("FBX vertex exceeds eight influences; no silent reduction");w.weights=glm::vec4(0);double sum=0;for(size_t wi=0;wi<weights.num_weights;++wi){auto weight=skin->weights.data[weights.weight_begin+wi];if(weight.weight<0||!std::isfinite(weight.weight)||weight.cluster_index>=skin->clusters.count)throw std::runtime_error("invalid FBX weight");if(wi<4){w.joints[wi]=uint32_t(paletteBase+weight.cluster_index);w.weights[wi]=float(weight.weight);}else{w.joints1[wi-4]=uint32_t(paletteBase+weight.cluster_index);w.weights1[wi-4]=float(weight.weight);}sum+=weight.weight;}if(sum<=1e-8)throw std::runtime_error("unweighted FBX skin vertex");w.weights/=float(sum);w.weights1/=float(sum);}
     auto index=uint32_t(mesh.vertices.size());corners[corner]=index;mesh.indices.push_back(index);mesh.vertices.push_back(v);mesh.skinVertices.push_back(w);auto original=src->vertex_indices.data[corner];auto identity=positionIds.emplace(std::make_pair(uint32_t(ni),original),uint32_t(positionIds.size()));mesh.sourceVertexIds.push_back(identity.first->second);mesh.vertexLocations.push_back({uint32_t(ni),original});
    }
   }
   if(mesh.indices.size()>first)mesh.primitives.push_back({first,unsigned(mesh.indices.size()-first),node->materials.count?materials.at(node->materials.data[mi]->element_id):0,Key(s,int(ni))+"/material/"+(node->materials.count?S(node->materials.data[mi]->name):"default"),int(ni)});
  }
 }
 if(mesh.vertices.empty())throw std::runtime_error("FBX contains no selected visual geometry");
 if(!GenerateMeshTangents(mesh))throw std::runtime_error("FBX tangent generation failed");
 mesh.skeletal=asset;report.skinJoints=s.skinNodes.size();report.parts=mesh.primitives.size();report.vertices=mesh.vertices.size();report.diagnostics.push_back({"warning","fbx-material-approximation",path,"","Remap material slots for unsupported appearance","FBX factors/maps converted to Judas metallic/roughness; source shader graphs are not reproduced"});result=std::move(mesh);error.clear();return true;
 }catch(const std::exception& e){error=std::string("Model import: ")+e.what();report.diagnostics.push_back({"error","import-failed",path,"","Correct source/settings or remap dependency; last generation remains intact",error});return false;}}
bool ImportMotionSource(const std::string& path,const ModelImportSettings& settings,MeshData& result,ModelImportReport& report,std::string& error){JUDAS_PROFILE_SCOPE("Motion source normalization");try{
 if(std::filesystem::file_size(path)>256*1024*1024)throw std::runtime_error("motion source exceeds 256 MiB");
 auto extension=std::filesystem::path(path).extension().string();MeshData holder;
 if(extension==".fbx"||extension==".FBX"){
  auto source=Load(path,settings);auto asset=std::make_shared<SkeletalAsset>();asset->skeleton=Hierarchy(*source);std::map<uint32_t,int> mapping;std::set<std::string> keys;
  for(size_t i=0;i<source->nodes.count;++i){auto key=Key(asset->skeleton,int(i));if(!keys.insert(key).second)throw std::runtime_error("ambiguous motion hierarchy path: "+key);mapping[source->nodes.data[i]->element_id]=int(i);holder.sourceNodes.push_back(key);}
  asset->clips=Clips(*source,settings,mapping);holder.skeletal=asset;report.sourceBones=source->bones.count;report.hierarchyNodes=source->nodes.count;report.sourceUnitMeters=source->settings.unit_meters;
 }else if(extension==".gltf"||extension==".glb"){
  std::vector<uint8_t> bytes;if(!ReadWholeFile(path,bytes,error)||!ParseGltfMeshSource(bytes.data(),bytes.size(),path,holder,error,&report.dependencies,true,&settings.dependencyRemaps))return false;
  if(!holder.skeletal)throw std::runtime_error("motion file has no animation hierarchy");
  if(!std::isfinite(glm::length(settings.basisRotation))||std::abs(glm::length(settings.basisRotation)-1)>1e-4f)throw std::runtime_error("motion basis requires normalized quaternion");
  float units=settings.sourceUnitMeters>0?float(settings.sourceUnitMeters):1.f;if(!std::isfinite(units)||units<1e-6f||units>1000)throw std::runtime_error("motion unit override bound");
  auto normalized=std::make_shared<SkeletalAsset>(*holder.skeletal);auto& s=normalized->skeleton;if(s.affine.empty())s.affine.assign(s.names.size(),glm::mat4(1));auto basis=glm::mat4_cast(settings.basisRotation)*glm::scale(glm::mat4(1),glm::vec3(units));for(size_t i=0;i<s.parents.size();++i)if(s.parents[i]<0)s.affine[i]=basis*s.affine[i];holder.skeletal=normalized;
  report.hierarchyNodes=s.names.size();report.skinJoints=s.skinNodes.size();report.parts=holder.primitives.size();report.vertices=holder.vertices.size();
 }else if(extension==".judasmodel"){
  // Already-normalized cooked content is a valid source. Extracted root tracks
  // are diagnosed by the retarget bake instead of being silently discarded.
  if(settings.sourceUnitMeters!=0||settings.basisRotation!=glm::quat(1,0,0,0))throw std::runtime_error("cooked motion source is already normalized; remove unit/basis override");
  if(!LoadModelMesh(path,holder,error))return false;
  if(!holder.skeletal)throw std::runtime_error("cooked motion source has no skeleton");
  report.hierarchyNodes=holder.skeletal->skeleton.names.size();report.skinJoints=holder.skeletal->skeleton.skinNodes.size();report.parts=holder.primitives.size();report.vertices=holder.vertices.size();
 }else throw std::runtime_error("motion source must be FBX, glTF, GLB or judasmodel");
 if(settings.cancelled&&settings.cancelled())throw std::runtime_error("motion import cancelled");
 result=std::move(holder);error.clear();return true;
 }catch(const std::exception& e){error=std::string("Motion import: ")+e.what();return false;}}
bool ImportCompatibleMotion(const std::string& path,const ModelImportSettings& settings,const Skeleton& target,std::vector<AnimationClip>& result,ModelImportReport& report,std::string& error){JUDAS_PROFILE_SCOPE("Model source normalization");try{
 auto extension=std::filesystem::path(path).extension().string();if(extension==".gltf"||extension==".glb"){
  std::vector<uint8_t> bytes;MeshData holder;if(!ReadWholeFile(path,bytes,error)||!ParseGltfMeshSource(bytes.data(),bytes.size(),path,holder,error,&report.dependencies,true,&settings.dependencyRemaps))return false;
  if(!holder.skeletal)throw std::runtime_error("motion file has no animation hierarchy");
  auto normalized=std::make_shared<SkeletalAsset>(*holder.skeletal);auto& source=normalized->skeleton;
  if(!std::isfinite(glm::length(settings.basisRotation))||std::abs(glm::length(settings.basisRotation)-1)>1e-4f)throw std::runtime_error("motion basis requires normalized quaternion");
  float units=settings.sourceUnitMeters>0?float(settings.sourceUnitMeters):1.f;if(!std::isfinite(units)||units<1e-6f||units>1000)throw std::runtime_error("motion unit override bound");auto basis=glm::mat4_cast(settings.basisRotation)*glm::scale(glm::mat4(1),glm::vec3(units));if(source.affine.empty())source.affine.assign(source.names.size(),glm::mat4(1));for(size_t i=0;i<source.parents.size();++i)if(source.parents[i]<0)source.affine[i]=basis*source.affine[i];holder.skeletal=normalized;auto globals=ResolveJointMatrices(source,source.rest),targetGlobals=ResolveJointMatrices(target,target.rest);std::map<int,int> remap;std::set<int> used;
  for(size_t i=0;i<source.names.size();++i){auto key=Key(source,int(i));auto explicitKey=settings.jointRemaps.find(key);int index=FindSkeletonJoint(target,explicitKey==settings.jointRemaps.end()?key:explicitKey->second);if(index>=0){if(!used.insert(index).second)throw std::runtime_error("ambiguous glTF motion mapping: "+key);remap[int(i)]=index;}}
  for(auto& [node,index]:remap){int parent=source.parents[node];if(parent<0)continue;auto matched=remap.find(parent);if(matched==remap.end()||target.parents[index]!=matched->second)throw std::runtime_error("incompatible glTF motion hierarchy: "+Key(source,node));double a=glm::length(glm::vec3(globals[node][3]-globals[parent][3])),b=glm::length(glm::vec3(targetGlobals[index][3]-targetGlobals[matched->second][3]));if(std::abs(a-b)>std::max(.002,a*.002))throw std::runtime_error("incompatible glTF motion rest proportions: "+Key(source,node));}
  for(size_t i=0;i<source.skinNodes.size();++i){auto node=remap.find(source.skinNodes[i]);if(node==remap.end())throw std::runtime_error("missing skin joint in motion mapping");bool compatible=false;for(size_t k=0;k<target.skinNodes.size();++k)if(target.skinNodes[k]==node->second){float worst=0;for(int c=0;c<4;++c)for(int r=0;r<4;++r)worst=std::max(worst,std::abs(source.inverseBind[i][c][r]-target.inverseBind[k][c][r]));compatible|=worst<1e-4f;}if(!compatible)throw std::runtime_error("incompatible glTF inverse binding: "+Key(source,source.skinNodes[i]));}
  auto clips=holder.skeletal->clips;for(auto& clip:clips)for(auto& track:clip.tracks){auto i=remap.find(track.node);if(i==remap.end())throw std::runtime_error("missing animated glTF joint: "+Key(source,track.node));track.node=i->second;}result=std::move(clips);return true;
 }
 auto source=Load(path,settings);auto s=Hierarchy(*source);std::map<std::string,int> targets;for(size_t i=0;i<target.names.size();++i)if(!targets.emplace(Key(target,int(i)),int(i)).second)throw std::runtime_error("ambiguous target hierarchy path");std::map<uint32_t,int> mapping;std::set<int> matched;for(size_t i=0;i<s.names.size();++i){auto key=Key(s,int(i));auto remap=settings.jointRemaps.find(key);auto it=targets.find(remap==settings.jointRemaps.end()?key:remap->second);if(it!=targets.end()){if(!matched.insert(it->second).second)throw std::runtime_error("ambiguous multiple source joints map to "+it->first);mapping[source->nodes.data[i]->element_id]=it->second;}}
 // Verify parent correspondence and segment lengths in normalized model space.
 // Default pose rotations may differ between motion exports; bone proportions may not.
 auto sourceGlobal=ResolveJointMatrices(s,s.rest),targetGlobal=ResolveJointMatrices(target,target.rest);
 for(size_t i=0;i<s.names.size();++i){auto* node=source->nodes.data[i];if(!node->bone)continue;auto found=mapping.find(node->element_id);if(found==mapping.end())continue;int parent=s.parents[i];if(parent<0||!source->nodes.data[parent]->bone)continue;auto mappedParent=mapping.find(source->nodes.data[parent]->element_id);if(mappedParent==mapping.end()||target.parents[found->second]!=mappedParent->second)throw std::runtime_error("incompatible bone parent: "+Key(s,int(i)));
  double sourceLength=glm::length(glm::vec3(sourceGlobal[i][3]-sourceGlobal[parent][3]));double targetLength=glm::length(glm::vec3(targetGlobal[found->second][3]-targetGlobal[mappedParent->second][3]));if(std::abs(sourceLength-targetLength)>std::max(.002,sourceLength*.002))throw std::runtime_error("incompatible normalized bone length: "+Key(s,int(i))+"; same-rig mapping is not proportion retargeting");
 }
 auto clips=Clips(*source,settings,mapping);report.sourceBones=source->bones.count;report.hierarchyNodes=source->nodes.count;result=std::move(clips);error.clear();return true;
 }catch(const std::exception& e){error=std::string("Compatible motion: ")+e.what();return false;}}

bool GatherModelDependencies(const std::string& source,std::vector<std::string>& paths,std::string& error,const ModelImportSettings* settings){
 try {auto extension=std::filesystem::path(source).extension().string();if(extension==".gltf"||extension==".glb")return GatherGltfDependencies(source,paths,error,settings?&settings->dependencyRemaps:nullptr);if(extension==".obj"||extension==".OBJ")return GatherObjDependencies(source,paths,error);if(extension!=".fbx"&&extension!=".FBX")return true;
  ModelImportSettings settings;auto scene=Load(source,settings);auto root=std::filesystem::weakly_canonical(std::filesystem::path(source).parent_path());std::set<std::string> distinct;
  for(auto* texture:scene->textures){if(texture->type!=UFBX_TEXTURE_FILE||texture->content.size)continue;auto name=S(texture->relative_filename);if(name.empty())name=S(texture->filename);std::replace(name.begin(),name.end(),'\\','/');auto path=std::filesystem::weakly_canonical(root/name);auto relative=path.lexically_relative(root);if(relative.empty()||relative.is_absolute()||*relative.begin()==".."||!std::filesystem::is_regular_file(path)){error="Unresolved source texture requires an explicit project-owned dependency remap: "+name;continue;}distinct.insert(path.string());}paths.assign(distinct.begin(),distinct.end());return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
