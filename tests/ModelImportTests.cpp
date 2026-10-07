#include "ModelImport.h"
#include "ModelArchive.h"
#include "ModelCook.h"
#include "SkeletalAnimation.h"
#include "PoseComposition.h"
#include "SceneFingerprint.h"
#include "AsyncFile.h"
#include "Tangents.h"
#include "../third_party/ufbx/ufbx.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <memory>
#include <set>
#include <glm/gtc/matrix_transform.hpp>
namespace fs=std::filesystem;using Json=nlohmann::json;
unsigned checks=0,failures=0;
void Check(bool value,const char* text){++checks;if(!value)++failures;std::cout<<(value?"PASS ":"FAIL ")<<text<<'\n';}
glm::mat4 Matrix(ufbx_matrix m){return {{m.m00,m.m10,m.m20,0},{m.m01,m.m11,m.m21,0},{m.m02,m.m12,m.m22,0},{m.m03,m.m13,m.m23,1}};}
glm::vec3 Deformed(const MeshData& m,size_t i,const std::vector<glm::mat4>& palette){auto& w=m.skinVertices[i];glm::mat4 blend(0);for(int k=0;k<4;++k)blend+=palette[w.joints[k]]*w.weights[k]+palette[w.joints1[k]]*w.weights1[k];return glm::vec3(blend*glm::vec4(m.vertices[i].position,1));}
int main(int argc,char** argv){
 fs::path source=argc>1?argv[1]:".cache/m66/originals/Skateboarder.fbx";
 fs::path output=argc>2?argv[2]:"docs/evidence/m66/development/fidelity";fs::create_directories(output);
 MeshData model;ModelImportSettings settings;ModelImportReport report;std::string error;
 MeshData tangentFixture;tangentFixture.vertices.resize(3);tangentFixture.vertices[0].position={0,0,0};tangentFixture.vertices[1].position={0,1,0};tangentFixture.vertices[2].position={0,0,1};for(auto& v:tangentFixture.vertices)v.normal={-.99999994f,0,0};Check(GenerateMeshTangents(tangentFixture)&&std::isfinite(tangentFixture.vertices[0].tangent.x)&&std::isfinite(tangentFixture.vertices[0].tangent.y)&&std::abs(glm::dot(glm::vec3(tangentFixture.vertices[0].tangent),tangentFixture.vertices[0].normal))<1e-5,"near-axis degenerate UV fallback yields finite orthogonal tangent");
 auto begin=std::chrono::steady_clock::now();bool imported=ImportModelSource(source.string(),settings,model,report,error);Check(imported,"original FBX imports without preprocessing");if(!imported){std::cerr<<error<<'\n';return 1;}
 double importMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
 Check(report.sourceBones==65&&report.hierarchyNodes==72&&report.parts==6,"65 source bones / 72 hierarchy nodes / six retained parts");
 Check(model.vertexLocations.size()==model.vertices.size()&&model.faceLocations.size()==model.indices.size()/3,"original vertex/face provenance survives triangulation and tangent seams");
 ufbx_load_opts options{};options.target_axes=ufbx_axes_right_handed_y_up;options.target_unit_meters=1;options.space_conversion=UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;options.geometry_transform_handling=UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;options.inherit_mode_handling=UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
 ufbx_error diagnostic{};std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(ufbx_load_file(source.c_str(),&options,&diagnostic),ufbx_free_scene);Check(bool(scene),"independent source evaluator loads original");if(!scene)return 1;
 auto& rig=model.skeletal->skeleton;auto rest=ResolveJointMatrices(rig,rig.rest);double restError=0;
 for(size_t i=0;i<rest.size();++i){auto reference=Matrix(scene->nodes.data[i]->node_to_world);for(int c=0;c<4;++c)for(int r=0;r<4;++r)restError=std::max(restError,double(std::abs(rest[i][c][r]-reference[c][r])));}
 Check(restError<3e-5,"normalized default hierarchy equals source-evaluated transforms");
 Json samples=Json::array();double maxPositionError=0,maxJointError=0;
 for(size_t clipIndex=0;clipIndex<model.skeletal->clips.size();++clipIndex){auto& clip=model.skeletal->clips[clipIndex];auto* stack=scene->anim_stacks.data[clipIndex];for(float t:{0.f,.123f,.713f}){
  t=std::min(t,clip.duration);ufbx_evaluate_opts evaluate{};std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> evaluated(ufbx_evaluate_scene(scene.get(),stack->anim,t+stack->time_begin,&evaluate,&diagnostic),ufbx_free_scene);if(!evaluated)return 1;
  auto pose=SampleClip(rig,clip,t);auto global=ResolveJointMatrices(rig,pose);auto palette=ResolveSkinMatrices(rig,pose);double jointError=0,positionError=0;
  for(size_t i=0;i<global.size();++i){auto reference=Matrix(evaluated->nodes.data[i]->node_to_world);for(int c=0;c<4;++c)for(int r=0;r<4;++r)jointError=std::max(jointError,double(std::abs(global[i][c][r]-reference[c][r])));}
  for(size_t i=0;i<model.vertices.size();i+=173){auto location=model.vertexLocations[i];auto* node=evaluated->nodes.data[location.node];auto* mesh=node->mesh;auto local=mesh->vertices.data[location.element];auto transform=mesh->skin_deformers.count?ufbx_get_skin_vertex_matrix(mesh->skin_deformers.data[0],location.element,&node->geometry_to_world):node->geometry_to_world;auto v=ufbx_transform_position(&transform,local);positionError=std::max(positionError,double(glm::length(Deformed(model,i,palette)-glm::vec3(v.x,v.y,v.z))));}
  maxPositionError=std::max(maxPositionError,positionError);maxJointError=std::max(maxJointError,jointError);samples.push_back({{"clip",clip.name},{"time",t},{"jointMatrixMaxError",jointError},{"vertexPositionMaxErrorMetres",positionError}});
 }}
 Check(maxJointError<.002,"baked named takes agree with source evaluation at independent off-grid times");Check(maxPositionError<.002,"per-mesh skin bindings reproduce original deformed vertices within 2 mm");
 auto palette=ResolveSkinMatrices(rig,rig.rest);glm::vec3 minimum(INFINITY),maximum(-INFINITY);for(size_t i=0;i<model.vertices.size();++i){auto p=Deformed(model,i,palette);minimum=glm::min(minimum,p);maximum=glm::max(maximum,p);}auto dimensions=maximum-minimum;
 Check(dimensions.y>1.3f&&dimensions.y<2.3f,"FBX centimetres normalized to metre-scale character without height guessing");size_t embedded=0;for(auto& m:model.materials)for(auto& map:m.maps)embedded+=!map.embedded.pixels.empty();Check(embedded>0,"source embedded material textures retained");
 std::vector<uint8_t> archive,again;MeshData round;Check(EncodeModelArchive(model,archive,error)&&DecodeModelArchive(archive.data(),archive.size(),round,error),"self-contained runtime archive validates and round-trips");Check(EncodeModelArchive(round,again,error)&&archive==again,"normalized model bytes deterministic on round-trip");MeshData previous=round;uint8_t malformed[]={0x9f,0x9f,0xff};Check(!DecodeModelArchive(malformed,sizeof(malformed),round,error)&&round.skeletal==previous.skeletal,"malformed runtime payload fails transactionally");
 // Analytic rigid root displacement crosses a loop and supports reverse intervals.
 AnimationClip motion;motion.duration=1;motion.motionTimes={0,1};JointTransform endpoint;endpoint.translation={2,0,0};motion.motion={{},endpoint};auto crossing=RootMotionInterval(motion,.75,1.25,true);auto reverse=RootMotionInterval(motion,1.25,.75,true);Check(glm::length(crossing.translation-glm::vec3(1,0,0))<1e-6f&&glm::length(reverse.translation+crossing.translation)<1e-6f,"root interval loop and reverse sampling do not teleport");

 // Independent glTF fixtures exercise the hardware-facing representation.
 MeshData capacity,multipart,external;
 ModelImportReport fixtureReport;
 Check(ImportModelSource("tests/fixtures/m66/capacity256.gltf",settings,capacity,fixtureReport,error),"256 weighted-joint fixture imports");
 if(capacity.skeletal){auto p=ResolveSkinMatrices(capacity.skeletal->skeleton,SampleClip(capacity.skeletal->skeleton,capacity.skeletal->clips[0],1));std::set<uint32_t> used;for(auto& w:capacity.skinVertices)for(int k=0;k<4;++k){if(w.weights[k]>0)used.insert(w.joints[k]);if(w.weights1[k]>0)used.insert(w.joints1[k]);}Check(used.size()==256&&glm::length(Deformed(capacity,capacity.vertices.size()-1,p)-capacity.vertices.back().position-glm::vec3(.5,0,0))<1e-5f,"all 256 joints and eight meaningful high-index influences deform vertices");}
 Check(ImportModelSource("tests/fixtures/m66/multipart.gltf",settings,multipart,fixtureReport,error),"multiple skins, independent rig and affine attachment import");
 if(multipart.skeletal){auto p=ResolveSkinMatrices(multipart.skeletal->skeleton,multipart.skeletal->skeleton.rest);Check(multipart.primitives.size()==4&&p.size()==6,"per-skin palettes and static attachment retained");auto& first=multipart.primitives[0];auto& second=multipart.primitives[1];auto a=Deformed(multipart,multipart.indices[first.first],p),b=Deformed(multipart,multipart.indices[second.first],p);Check(glm::length(a)<1e-6f&&glm::length(b-glm::vec3(2,0,0))<1e-6f,"distinct inverse binds preserved and skinned mesh-node transform not double-applied");}
 bool loadedExternal=ImportModelSource("tests/fixtures/m66/external.gltf",settings,external,fixtureReport,error);Check(loadedExternal,"external Unicode/encoded-space texture and buffer resolve in approved root");if(!loadedExternal)std::cerr<<error<<'\n';if(loadedExternal)Check(external.materials[0].maps[0].uvSet==1&&std::abs(external.materials[0].maps[0].rotation-.2f)<1e-6f,"required KHR texture transform / UV1 / sampler preserved");
 MeshData glb;Check(ImportModelSource("tests/fixtures/m66/multipart.glb",settings,glb,fixtureReport,error)&&glb.primitives.size()==4,"binary GLB multi-part front end retains both skins and affine instances");
 MeshData analytic;ModelImportReport analyticReport;bool analyticOk=ImportModelSource("tests/fixtures/m66/pivot-centimetres.fbx",settings,analytic,analyticReport,error);Check(analyticOk,"independently authored centimetre/pivot FBX imports");if(!analyticOk)std::cerr<<error<<'\n';if(analyticOk){auto transforms=ResolveSkinMatrices(analytic.skeletal->skeleton,analytic.skeletal->skeleton.rest);double worst=0;glm::vec3 expected[3]={{2.5f,-.5f,0},{2.5f,.5f,0},{1.5f,-.5f,0}};for(size_t i=0;i<analytic.vertices.size();++i){auto actual=Deformed(analytic,i,transforms);double nearest=INFINITY;for(auto p:expected)nearest=std::min(nearest,double(glm::length(p-actual)));worst=std::max(worst,nearest);}Check(worst<1e-5,"source pivot evaluation agrees with independent analytic metre-space coordinates");}
 // Motion files preserve the original target hierarchy, not source array indices.
 for(const char* motionName:{"SkateboardingPush.fbx","SkateboardingCruise.fbx","Walking.fbx"}){auto path=source.parent_path()/motionName;std::vector<AnimationClip> clips;ModelImportReport motionReport;bool imported=ImportCompatibleMotion(path.string(),settings,rig,clips,motionReport,error);Check(imported,"original animation-only file maps onto complete rig with verified proportions");if(!imported){std::cerr<<error<<'\n';continue;}std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> motionScene(ufbx_load_file(path.c_str(),&options,&diagnostic),ufbx_free_scene);if(!motionScene)return 1;double worst=0,translationMetres=0,basisRadians=0;
  for(size_t c=0;c<clips.size();++c)for(float time:{.123f,.517f}){time=std::min(time,clips[c].duration);auto global=ResolveJointMatrices(rig,SampleClip(rig,clips[c],time));std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> evaluation(ufbx_evaluate_scene(motionScene.get(),motionScene->anim_stacks.data[c]->anim,time+motionScene->anim_stacks.data[c]->time_begin,nullptr,&diagnostic),ufbx_free_scene);if(!evaluation)return 1;for(auto* node:evaluation->nodes){if(!node->bone)continue;std::vector<std::string> segments;for(auto* n=node;n;n=n->parent)segments.push_back(n->is_root?"__root":std::string(n->name.data,n->name.length));std::string key;for(auto it=segments.rbegin();it!=segments.rend();++it){if(!key.empty())key+='/';key+=*it;}int joint=FindSkeletonJoint(rig,key);if(joint<0)continue;auto expected=Matrix(node->node_to_world);translationMetres=std::max(translationMetres,double(glm::length(glm::vec3(global[joint][3]-expected[3]))));
   // Direction error for each transformed joint axis is radians, including affine
   // transforms; the separate matrix check still detects scale/shear differences.
   for(int axis=0;axis<3;++axis){auto a=glm::normalize(glm::vec3(global[joint][axis])),b=glm::normalize(glm::vec3(expected[axis]));basisRadians=std::max(basisRadians,double(std::atan2(glm::length(glm::cross(a,b)),glm::dot(a,b))));}
   for(int x=0;x<4;++x)for(int y=0;y<4;++y)worst=std::max(worst,double(std::abs(global[joint][x][y]-expected[x][y])));}}
  std::cout<<"MOTION "<<motionName<<" off_grid_joint_error="<<worst<<" translation_error_metres="<<translationMetres<<" basis_angle_error_radians="<<basisRadians<<'\n';Check(worst<.005&&translationMetres<.005&&basisRadians<.01,"assembled motion agrees with independent source within 5 mm / 0.01 radian bake tolerances");
 }
 Json result={{"checks",checks},{"failures",failures},{"sourceBones",report.sourceBones},{"hierarchyNodes",report.hierarchyNodes},{"skinPaletteEntries",report.skinJoints},{"parts",report.parts},{"vertices",report.vertices},{"dimensionsMetres",{dimensions.x,dimensions.y,dimensions.z}},{"embeddedMaps",embedded},{"importMs",importMs},{"archiveBytes",archive.size()},{"restMatrixMaxError",restError},{"samples",samples}};std::ofstream(output/"measurements.json")<<result.dump(2)<<'\n';std::cout<<result.dump(2)<<'\n';return failures?1:0;
}
