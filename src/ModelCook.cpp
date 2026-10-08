#include "ModelCook.h"
#include "PerformanceProfiler.h"
#include "ModelArchive.h"
#include "PoseComposition.h"
#include "SceneFingerprint.h"
#include "AssetDatabase.h"
#include "AsyncFile.h"
#include "GltfLoader.h"
#include "../third_party/nlohmann/json.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <set>
#include <algorithm>
#include "PlatformServices.h"
namespace fs=std::filesystem;using Json=nlohmann::json;
namespace {
constexpr const char* Revision="M66-ufbx-0.23.1-cgltf-1.15-cook-9-model-1";
void Require(bool b,const std::string& e){if(!b)throw std::runtime_error(e);}
Json ReadJson(const fs::path& path){JUDAS_PROFILE_SCOPE("Model recipe/receipt parsing");std::ifstream f(path);Require(bool(f),"cannot read recipe: "+path.string());Require(fs::file_size(path)<=4*1024*1024,"recipe/report exceeds 4 MiB");return Json::parse(f,[](int depth,Json::parse_event_t,const Json&){Require(depth<48,"recipe nesting bound");return true;},true,false);}
void Write(const fs::path& p,const void* data,size_t size){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write(static_cast<const char*>(data),size);Require(bool(f),"write failed: "+p.string());}
void WriteJson(const fs::path& p,const Json& j){auto text=j.dump(2)+'\n';Write(p,text.data(),text.size());}
fs::path Relative(const fs::path& root,const std::string& path){auto p=fs::weakly_canonical(root/path);auto relative=p.lexically_relative(root);Require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..","recipe path outside approved project: "+path);return p;}
std::string Hash(const fs::path& p){JUDAS_PROFILE_SCOPE("Model content hashing");std::string h,e;Require(SceneFingerprintSha256File(p.string(),h,e),e);return h;}
std::string Stamp(const fs::path& p){return ImportFileStamp(p);}

void VerifyInputs(ModelCookTask& t,const fs::path& root,const Json& record){
 for(auto it=record.at("inputs").begin();it!=record.at("inputs").end();++it){auto path=Relative(root,it.key());auto before=Stamp(path);Require(Hash(path)==it.value()&&Stamp(path)==before,"input changed during import; retry: "+it.key());t.verifiedInputStamps[it.key()]=before;}
}
std::string StagePath(const std::string& output){return CreateImportStagingFile(fs::u8path(output));}
std::string AtomicCache(const fs::path& p,const void* data,size_t size){auto staged=StagePath(p.string());try{Write(staged,data,size);auto hash=Hash(staged);ReplaceStagedFile(staged,p);return hash;}catch(...){fs::remove(staged);throw;}}
void AtomicCacheJson(const fs::path& p,const Json& j){auto bytes=j.dump(2)+'\n';AtomicCache(p,bytes.data(),bytes.size());}

std::string Key(const Skeleton& s,int i){return SkeletonJointKey(s,i);}
void CheckCancelled(ModelCookTask& t){Require(!(t.cancel.load()||(t.workerCancelled&&t.workerCancelled())),"import cancelled; last accepted generation retained");}
void ApplyAliases(MeshData& m,const Json& options){
 if(options.contains("materialAliases"))for(size_t i=0;i<m.materialKeys.size();++i){auto old=m.materialKeys[i];if(options["materialAliases"].contains(old)){auto replacement=options["materialAliases"].at(old).get<std::string>();Require(!replacement.empty(),"material alias cannot be empty");m.materialKeys[i]=replacement;for(auto& part:m.primitives)if(part.material==int(i)){auto suffix=part.part.rfind("/material/");if(suffix!=std::string::npos)part.part=part.part.substr(0,suffix+10)+replacement;}}}
 if(m.skeletal&&options.contains("nodeAliases")){auto asset=std::make_shared<SkeletalAsset>(*m.skeletal);auto& s=asset->skeleton;std::vector<std::string> original;for(size_t i=0;i<s.names.size();++i)original.push_back(Key(s,int(i)));for(int i:s.order)if(options["nodeAliases"].contains(original[i])){auto name=options["nodeAliases"].at(original[i]).get<std::string>();Require(!name.empty(),"node alias cannot be empty");s.names[i]=name;}std::set<std::string> keys;for(size_t i=0;i<s.names.size();++i)Require(keys.insert(Key(s,int(i))).second,"node aliases produce ambiguous hierarchy identity");for(auto& part:m.primitives)if(part.node>=0){auto suffix=part.part.rfind("/material/");if(suffix!=std::string::npos)part.part=Key(s,part.node)+part.part.substr(suffix);}m.skeletal=asset;}
}
void StableMaterials(MeshData& m,const Json& previous){
 if(m.materialKeys.empty())return;
 std::map<std::string,int> mapping;for(size_t i=0;i<m.materialKeys.size();++i)Require(mapping.emplace(m.materialKeys[i],int(i)).second,"ambiguous material name; author unique material identities or remap before import: "+m.materialKeys[i]);
 std::vector<std::string> keys;if(previous.contains("materialKeys"))keys=previous.at("materialKeys").get<std::vector<std::string>>();
 for(const auto& key:keys)Require(mapping.count(key),"material renamed/deleted: "+key+"; update recipe slot remap before accepting");
 for(auto& [key,index]:mapping){(void)index;if(std::find(keys.begin(),keys.end(),key)==keys.end())keys.push_back(key);}
 std::vector<MaterialDefinition> sorted;std::vector<int> remap(m.materials.size());for(size_t i=0;i<keys.size();++i){int old=mapping.at(keys[i]);remap[old]=int(i);sorted.push_back(std::move(m.materials[old]));}for(auto& part:m.primitives)if(part.material>=0)part.material=remap.at(part.material);m.materials=std::move(sorted);m.materialKeys=std::move(keys);
}
void SelectParts(MeshData& m,const Json& settings){
 if(!settings.contains("selectedParts")||settings.at("selectedParts").empty())return;
 auto selection=settings.at("selectedParts").get<std::vector<std::string>>();std::vector<MeshPrimitive> parts;for(auto& key:selection){auto i=std::find_if(m.primitives.begin(),m.primitives.end(),[&](auto& p){return p.part==key;});Require(i!=m.primitives.end(),"selected source part missing: "+key);parts.push_back(*i);}Require(!parts.empty(),"no selected parts");m.primitives=std::move(parts);
}
void SortParts(MeshData& m,const Json& previous){
 std::map<std::string,MeshPrimitive> parts;for(auto& p:m.primitives)Require(parts.emplace(p.part,p).second,"ambiguous part identity; select distinct source material/node identities: "+p.part);
 std::vector<MeshPrimitive> ordered;if(previous.contains("parts"))for(auto& old:previous.at("parts")){auto key=old.at("identity").get<std::string>();auto it=parts.find(key);Require(it!=parts.end(),"source part renamed/deleted: "+key+"; correct recipe correspondence before replacing authored references");ordered.push_back(it->second);parts.erase(it);}for(auto& [key,part]:parts){(void)key;ordered.push_back(part);}m.primitives=std::move(ordered);
}
AnimationClip TrimClip(const Skeleton& skeleton,const AnimationClip& original,float begin,float end,double rate){
 Require(std::isfinite(begin)&&std::isfinite(end)&&begin>=0&&end>begin&&end<=original.duration,"clip trim must lie within the selected take");
 AnimationClip clip;clip.name=original.name;clip.duration=end-begin;clip.loop=original.loop;
 for(const auto& source:original.tracks){AnimationTrack track;track.node=source.node;track.path=source.path;track.interpolation=source.interpolation==TrackInterpolation::Step?TrackInterpolation::Step:TrackInterpolation::Linear;
  std::set<float> times{begin,end};for(float time:source.times)if(time>begin&&time<end)times.insert(time);for(size_t i=1;i<size_t(std::ceil((end-begin)*rate));++i)times.insert(begin+float(i/rate));
  for(float time:times){auto pose=SampleClip(skeleton,original,time);auto& p=pose.local[track.node];track.times.push_back(time-begin);track.values.push_back(track.path==TrackPath::Translation?glm::vec4(p.translation,0):track.path==TrackPath::Scale?glm::vec4(p.scale,0):glm::vec4(p.rotation.x,p.rotation.y,p.rotation.z,p.rotation.w));}clip.tracks.push_back(std::move(track));
 }return clip;
}
void RootPolicy(SkeletalAsset& a,AnimationClip& clip,const Json& settings,double rate){
 auto policy=settings.value("policy",std::string("preserve"));if(policy=="preserve")return;Require(policy=="inPlace"||policy=="extract","root policy must be preserve/inPlace/extract");
 auto& s=a.skeleton;int root=FindSkeletonJoint(s,settings.at("node"));Require(root>=0,"root motion node missing or ambiguous");auto axes=settings.value("translation",std::vector<bool>{true,false,true});Require(axes.size()==3,"root translation selection requires three model axes");glm::vec3 axis(0);if(settings.contains("rotationAxis")){auto values=settings.at("rotationAxis").get<std::vector<float>>();Require(values.size()==3,"root rotation axis dimensions");axis={values[0],values[1],values[2]};Require(std::isfinite(glm::length(axis))&&glm::length(axis)>1e-6f,"root rotation axis");axis=glm::normalize(axis);}
 // One synthetic model root expresses the exact complementary rigid transform.
 int outer=FindSkeletonJoint(s,"__JudasMotionRoot");Require(outer>=0,"missing normalized motion root");
 auto rigidRotation=[](glm::mat4 m){glm::vec3 x(m[0]),y(m[1]),z(m[2]);Require(glm::length(x)>1e-8f&&glm::length(y)>1e-8f&&glm::length(z)>1e-8f,"singular root-motion orientation");x=glm::normalize(x);y=glm::normalize(y);z=glm::normalize(z);Require(std::abs(glm::dot(x,y))<1e-4f&&std::abs(glm::dot(y,z))<1e-4f&&std::abs(glm::dot(z,x))<1e-4f&&glm::dot(glm::cross(x,y),z)>0,"root-motion orientation requires an unreflected orthogonal basis; select a compatible ancestor");return glm::normalize(glm::quat_cast(glm::mat3(x,y,z)));};
 auto original=clip;auto initial=ResolveJointMatrices(s,SampleClip(s,original,0))[root];glm::quat initialRotation=glm::dot(axis,axis)>0?rigidRotation(initial):glm::quat(1,0,0,0);glm::vec3 initialPosition(initial[3]);
 std::set<float> times{0,clip.duration};for(size_t k=1;k<size_t(std::ceil(clip.duration*rate));++k)times.insert(float(k/rate));for(auto& t:original.tracks)for(auto time:t.times)times.insert(time);
 AnimationTrack translation,rotation;translation.node=rotation.node=outer;translation.path=TrackPath::Translation;rotation.path=TrackPath::Rotation;
 for(auto time:times){auto global=ResolveJointMatrices(s,SampleClip(s,original,time))[root];glm::vec3 delta=glm::vec3(global[3])-initialPosition;for(int k=0;k<3;++k)if(!axes[k])delta[k]=0;
  glm::quat twist(1,0,0,0);if(glm::dot(axis,axis)>0){auto q=rigidRotation(global)*glm::inverse(initialRotation);glm::vec3 projected=axis*glm::dot(glm::vec3(q.x,q.y,q.z),axis);twist=glm::quat(q.w,projected.x,projected.y,projected.z);if(glm::dot(twist,twist)>1e-12f)twist=glm::normalize(twist);else twist=glm::quat(1,0,0,0);}
  JointTransform motion;motion.translation=delta+initialPosition-twist*initialPosition;motion.rotation=twist;clip.motionTimes.push_back(time);clip.motion.push_back(motion);
  auto inverse=glm::inverse(twist);glm::vec3 complementary=-(inverse*motion.translation);translation.times.push_back(time);translation.values.push_back({complementary,0});rotation.times.push_back(time);rotation.values.push_back({inverse.x,inverse.y,inverse.z,inverse.w});
 }
 clip.tracks.push_back(std::move(translation));clip.tracks.push_back(std::move(rotation));if(policy=="inPlace"){clip.motion.clear();clip.motionTimes.clear();}
}
void AddMotionRoot(Skeleton& s){Require(FindSkeletonJoint(s,"__JudasMotionRoot")<0,"source reserves normalized motion root name");int i=int(s.names.size());Require(size_t(i)<kModelNodeLimit,"model hierarchy resource bound");s.motionRoot=i;s.names.push_back("__JudasMotionRoot");s.parents.push_back(-1);s.rest.local.emplace_back();if(!s.affine.empty())s.affine.push_back(glm::mat4(1));for(int k=0;k<i;++k)if(s.parents[k]<0)s.parents[k]=i;s.order.insert(s.order.begin(),i);}
Json Report(const ModelCookTask& t,const MeshData& m,const Json& inputs,const std::string& recipeDigest){Json j={{"revision",Revision},{"recipeDigest",recipeDigest},{"inputs",inputs},{"sourceUnitMeters",t.report.sourceUnitMeters},{"vertices",m.vertices.size()},{"assetId",t.assetId},{"materialKeys",m.materialKeys},{"sourceBones",t.report.sourceBones},{"hierarchyNodes",t.report.hierarchyNodes},{"skinPaletteEntries",t.report.skinJoints},{"parts",Json::array()},{"clips",Json::array()},{"diagnostics",Json::array()}};for(auto& p:m.primitives)j["parts"].push_back({{"identity",p.part},{"node",p.node},{"triangles",p.count/3}});if(m.skeletal){auto& s=m.skeletal->skeleton;j["joints"]=Json::array();for(size_t i=0;i<s.names.size();++i)j["joints"].push_back(Key(s,int(i)));for(auto& c:m.skeletal->clips)j["clips"].push_back({{"name",c.name},{"duration",c.duration},{"motionExtracted",!c.motion.empty()},{"loop",c.loop}});}for(auto& d:t.report.diagnostics)j["diagnostics"].push_back({{"severity",d.severity},{"code",d.code},{"source",fs::path(d.source).filename().string()},{"node",d.node},{"face",d.face},{"vertex",d.vertex},{"message",d.message},{"action",d.action}});return j;}
}
bool CookModelRecipe(const std::string& recipe,ModelCookTask& t){JUDAS_PROFILE_SCOPE("Model import recipe");try{
 t.recipe=fs::absolute(recipe).string();auto root=fs::weakly_canonical(fs::path(recipe).parent_path().parent_path());auto j=ReadJson(recipe);Require(j.at("format")=="JudasImport"&&j.at("version")==1,"import recipe header/version");t.assetId=j.at("assetId");Require(IsValidAssetId(t.assetId),"recipe asset identity");t.output=Relative(root,j.at("output")).string();auto source=Relative(root,j.at("source"));CheckCancelled(t);t.progress=5;
 std::string error;Json inputs=Json::object();inputs[j.at("source").get<std::string>()]=Hash(source);auto digest=SceneFingerprintSha256(j.dump()+Revision);
 Json previous=Json::object();MeshData accepted;bool decoded=false;
 if(fs::exists(t.output)){
  auto beforeOutput=Stamp(t.output);t.outputHash=Hash(t.output);Require(Stamp(t.output)==beforeOutput,"accepted model changed during verification; retry");t.verifiedOutputStamp=beforeOutput;
  auto receipt=fs::path(t.output+".import-report.json");
  if(fs::exists(receipt))try{auto r=ReadJson(receipt);if(r.value("outputHash",std::string())==t.outputHash&&r.contains("importRecord")&&r.value("recordHash",std::string())==SceneFingerprintSha256(r.at("importRecord").get<std::string>())){t.importRecord=r.at("importRecord");t.receiptHit=true;}}catch(const std::exception&){/* disposable receipt: fall back to validated payload */}
  if(!t.receiptHit){std::vector<uint8_t> bytes;Require(ReadWholeFile(t.output,bytes,error)&&DecodeModelArchive(bytes.data(),bytes.size(),accepted,error),error);++t.decodedProducts;decoded=true;t.importRecord=accepted.importRecord;}
  if(!t.importRecord.empty())previous=Json::parse(t.importRecord).value("manifest",Json::object());
 }
 if(!previous.empty()){
  bool same=previous.value("recipeDigest",std::string())==digest;
  for(auto it=previous.at("inputs").begin();it!=previous.at("inputs").end();++it){auto p=Relative(root,it.key());if(!fs::exists(p)||(inputs.contains(it.key())?inputs[it.key()]:Json(Hash(p)))!=it.value())same=false;}
  if(same){
   if(t.previewRequired&&!decoded){std::vector<uint8_t> bytes;Require(ReadWholeFile(t.output,bytes,error)&&DecodeModelArchive(bytes.data(),bytes.size(),accepted,error),error);++t.decodedProducts;decoded=true;}
   if(t.previewRequired)t.preview=std::make_shared<MeshData>(std::move(accepted));
   t.report.sourceBones=previous.at("sourceBones");t.report.hierarchyNodes=previous.at("hierarchyNodes");t.report.skinJoints=previous.at("skinPaletteEntries");t.report.parts=previous.at("parts").size();t.report.vertices=previous.value("vertices",size_t(0));t.report.sourceUnitMeters=previous.value("sourceUnitMeters",1.0);
   VerifyInputs(t,root,Json::parse(t.importRecord));CheckCancelled(t);t.unchanged=t.success=true;t.progress=100;return true;
  }
 }
 accepted=MeshData{};
 ModelImportSettings settings;auto options=j.value("settings",Json::object());settings.sourceUnitMeters=options.value("unitMeters",0.0);settings.sampleRate=options.value("sampleRate",60.0);if(options.contains("basisRotation")){auto q=options.at("basisRotation").get<std::vector<float>>();Require(q.size()==4,"basis rotation requires x/y/z/w");settings.basisRotation={q[3],q[0],q[1],q[2]};}settings.allowBaseMesh=options.value("allowBaseMesh",false);settings.cancelled=[&]{return t.cancel.load()||(t.workerCancelled&&t.workerCancelled());};
 if(options.contains("dependencyRemaps"))for(auto it=options["dependencyRemaps"].begin();it!=options["dependencyRemaps"].end();++it)settings.dependencyRemaps[it.key()]=Relative(root,it.value()).string();
 MeshData mesh;
 auto cache=root/".cache"/"model-import"/(inputs[j.at("source").get<std::string>()].get<std::string>()+"-"+SceneFingerprintSha256(options.dump()+Revision));
 bool cacheValid=fs::exists(cache.string()+".judasmodel")&&fs::exists(cache.string()+".json");
 Json cacheInfo;if(cacheValid)try{cacheInfo=ReadJson(cache.string()+".json");cacheValid=cacheInfo.value("payloadHash",std::string())==Hash(cache.string()+".judasmodel");for(auto it=cacheInfo.at("dependencies").begin();it!=cacheInfo.at("dependencies").end();++it){auto path=Relative(root,it.key());if(!fs::exists(path)||Hash(path)!=it.value())cacheValid=false;}}catch(const std::exception&){cacheValid=false;}
 if(cacheValid){std::vector<uint8_t> cached;Require(ReadWholeFile(cache.string()+".judasmodel",cached,error)&&DecodeModelArchive(cached.data(),cached.size(),mesh,error),error);t.report.sourceBones=cacheInfo.at("sourceBones");t.report.hierarchyNodes=cacheInfo.at("nodes");t.report.skinJoints=mesh.skeletal?mesh.skeletal->skeleton.skinNodes.size():0;t.report.parts=mesh.primitives.size();t.report.vertices=mesh.vertices.size();for(auto it=cacheInfo.at("dependencies").begin();it!=cacheInfo.at("dependencies").end();++it)t.report.dependencies.push_back(Relative(root,it.key()).string());}
 else {Require(ImportModelSource(source.string(),settings,mesh,t.report,error),error);std::vector<uint8_t> cached;Require(EncodeModelArchive(mesh,cached,error),error);Json deps=Json::object();for(auto& p:t.report.dependencies){auto rel=fs::weakly_canonical(p).lexically_relative(root);Require(!rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..","copy/remap texture into project Sources");deps[rel.generic_string()]=Hash(p);}auto payloadHash=AtomicCache(cache.string()+".judasmodel",cached.data(),cached.size());AtomicCacheJson(cache.string()+".json",{{"sourceBones",t.report.sourceBones},{"nodes",t.report.hierarchyNodes},{"dependencies",deps},{"payloadHash",payloadHash}});}
 t.progress=45;CheckCancelled(t);
 for(auto& path:t.report.dependencies){auto relative=fs::weakly_canonical(path).lexically_relative(root);Require(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..","dependency must be copied/remapped into project-owned Sources: "+path);inputs[relative.generic_string()]=Hash(path);}
 if(mesh.skeletal){auto asset=std::make_shared<SkeletalAsset>(*mesh.skeletal);if(j.contains("motions"))for(auto& motion:j["motions"]){CheckCancelled(t);auto path=Relative(root,motion.at("source"));inputs[motion.at("source").get<std::string>()]=Hash(path);std::vector<AnimationClip> clips;ModelImportReport motionReport;auto motionSettings=settings;if(motion.contains("jointRemaps"))motionSettings.jointRemaps=motion.at("jointRemaps").get<std::map<std::string,std::string>>();
 std::vector<std::string> motionDependencies;Require(GatherModelDependencies(path.string(),motionDependencies,error,&motionSettings),error);std::string motionInputs=Hash(path);for(auto& dependency:motionDependencies){auto rel=fs::weakly_canonical(dependency).lexically_relative(root);Require(!rel.empty()&&!rel.is_absolute()&&*rel.begin()!="..","motion dependency outside project");auto hash=Hash(dependency);inputs[rel.generic_string()]=hash;motionInputs+=rel.generic_string()+hash;}
 auto clipCache=root/".cache"/"model-import"/(SceneFingerprintSha256(motionInputs)+"-"+SceneFingerprintSha256(inputs[j.at("source").get<std::string>()].get<std::string>()+motion.value("jointRemaps",Json::object()).dump()+options.dump()+Revision)+".motion.judasmodel");
 bool motionValid=false;if(fs::exists(clipCache)&&fs::exists(clipCache.string()+".sha256"))try{std::ifstream receipt(clipCache.string()+".sha256");std::string hash;receipt>>hash;motionValid=hash==Hash(clipCache);}catch(const std::exception&){motionValid=false;}
 if(motionValid){std::vector<uint8_t> cached;MeshData holder;Require(ReadWholeFile(clipCache.string(),cached,error)&&DecodeModelArchive(cached.data(),cached.size(),holder,error),error);clips=holder.skeletal->clips;}
 else {Require(ImportCompatibleMotion(path.string(),motionSettings,asset->skeleton,clips,motionReport,error),error);MeshData holder;holder.vertices.resize(3);holder.indices={0,1,2};holder.skinVertices.resize(3);auto shared=std::make_shared<SkeletalAsset>();shared->skeleton=asset->skeleton;shared->clips=clips;holder.skeletal=shared;std::vector<uint8_t> cached;Require(EncodeModelArchive(holder,cached,error),error);auto hash=AtomicCache(clipCache,cached.data(),cached.size());AtomicCache(clipCache.string()+".sha256",hash.data(),hash.size());}auto take=motion.at("take").get<std::string>();auto it=std::find_if(clips.begin(),clips.end(),[&](auto& c){return c.name==take;});if(it==clips.end()){std::string choices;for(auto& candidate:clips)choices+=(choices.empty()?"":", ")+candidate.name;Require(false,"selected motion take missing: "+take+"; available: "+choices);}it->name=motion.at("name");asset->clips.push_back(*it);}
 mesh.skeletal=asset;ApplyAliases(mesh,options);asset=std::make_shared<SkeletalAsset>(*mesh.skeletal);AddMotionRoot(asset->skeleton);if(j.contains("clips")){std::vector<AnimationClip> selected;for(auto& entry:j["clips"]){auto name=entry.at("sourceClip").get<std::string>();auto it=std::find_if(asset->clips.begin(),asset->clips.end(),[&](auto& c){return c.name==name;});Require(it!=asset->clips.end(),"selected clip missing: "+name);auto clip=*it;if(entry.contains("trim")){auto trim=entry.at("trim");clip=TrimClip(asset->skeleton,clip,trim.at(0),trim.at(1),settings.sampleRate);}clip.name=entry.value("name",clip.name);clip.loop=entry.value("loop",false);RootPolicy(*asset,clip,entry.value("rootMotion",Json{{"policy","preserve"}}),settings.sampleRate);selected.push_back(std::move(clip));}asset->clips=std::move(selected);}mesh.skeletal=asset;}
 mesh.importRecord=Json{{"recipe",fs::path(t.recipe).lexically_relative(root).generic_string()},{"revision",Revision},{"digest",digest},{"inputs",inputs}}.dump();
 if(previous.contains("joints")&&mesh.skeletal){std::set<std::string> keys;for(size_t i=0;i<mesh.skeletal->skeleton.names.size();++i)keys.insert(Key(mesh.skeletal->skeleton,int(i)));for(auto& key:previous["joints"])Require(keys.count(key.get<std::string>()),"required joint renamed/deleted: "+key.get<std::string>()+"; remap authored consumers explicitly before accepting a replacement");}
 if(!mesh.skeletal)ApplyAliases(mesh,options);
 StableMaterials(mesh,previous);SelectParts(mesh,options);SortParts(mesh,previous);
 auto manifest=Report(t,mesh,inputs,digest);auto record=Json::parse(mesh.importRecord);record["manifest"]=manifest;mesh.importRecord=record.dump();
 t.importRecord=mesh.importRecord;VerifyInputs(t,root,Json::parse(t.importRecord));
 t.progress=75;CheckCancelled(t);std::vector<uint8_t> bytes;Require(EncodeModelArchive(mesh,bytes,error),error);t.temporary=StagePath(t.output);Write(t.temporary,bytes.data(),bytes.size());auto report=Report(t,mesh,inputs,digest);t.outputHash=Hash(t.temporary);report["outputHash"]=t.outputHash;report["importRecord"]=t.importRecord;report["recordHash"]=SceneFingerprintSha256(t.importRecord);WriteJson(t.temporary+".report",report);CheckCancelled(t);t.preview=std::make_shared<MeshData>(std::move(mesh));t.success=true;t.progress=95;return true;
 }catch(const std::exception& e){t.error=e.what();t.success=false;if(!t.temporary.empty()){std::error_code ec;fs::remove(t.temporary,ec);fs::remove(t.temporary+".report",ec);}return false;}}
bool PublishModelImport(ModelCookTask& t,std::string& error){JUDAS_PROFILE_SCOPE("Model generation publication");
 try {
  Require(t.success&&!t.cancel.load(),t.error.empty()?"import cancelled/not ready":t.error);
  auto record=Json::parse(t.importRecord);auto root=fs::weakly_canonical(fs::path(t.recipe).parent_path().parent_path());
  {
   Require(SceneFingerprintSha256(ReadJson(t.recipe).dump()+Revision)==record.at("digest"),"recipe changed after cook; retry before publication");
   // Contents were verified on the worker, with stable before/after ctime,
   // inode/size/mtime. Stamps are only a publication race guard, never a cache
   // key. A changed stamp requires content verification, including same-mtime edits.
   for(auto it=record.at("inputs").begin();it!=record.at("inputs").end();++it){auto path=Relative(root,it.key());if(!t.verifiedInputStamps.count(it.key())||Stamp(path)!=t.verifiedInputStamps.at(it.key()))Require(Hash(path)==it.value(),"input changed after cook; retry before publication: "+it.key());}
  }
  if(!t.unchanged){Require(t.preview!=nullptr,"missing staged generation");
   // The cooked file is the coherent generation and includes its provenance.
   // Validate/stage metadata before the single atomic generation replacement.
   auto meta=t.output+".judasmeta";bool newMeta=!fs::exists(meta);
   if(!newMeta){std::string id,source;AssetType type;Require(AssetDatabase::ReadMeta(meta,id,type,source,error)&&id==t.assetId&&type==AssetType::Mesh,"existing output identity differs from recipe");}
   else Require(AssetDatabase::WriteMeta(t.temporary+".meta",t.assetId,AssetType::Mesh,"import recipe",error),error);
   if(newMeta)ReplaceStagedFile(t.temporary+".meta",meta);
   try {ReplaceStagedFile(t.temporary,t.output);}catch(...){if(newMeta)fs::remove(meta);throw;}
   // A sidecar report is inspectable telemetry, never required for runtime validity.
   try {ReplaceStagedFile(t.temporary+".report",t.output+".import-report.json");} catch (const std::exception& e) {t.report.diagnostics.push_back({"warning","report-promotion",t.recipe,"","Retry report publication",e.what()});}
  }
  if(t.unchanged){Require(Stamp(t.output)==t.verifiedOutputStamp||Hash(t.output)==t.outputHash,"accepted model changed before publication; retry");auto receipt=Json::parse(t.importRecord).at("manifest");receipt["outputHash"]=t.outputHash;receipt["importRecord"]=t.importRecord;receipt["recordHash"]=SceneFingerprintSha256(t.importRecord);auto temp=StagePath(t.output);try{WriteJson(temp,receipt);ReplaceStagedFile(temp,t.output+".import-report.json");}catch(...){fs::remove(temp);throw;}}
  t.progress=100;error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
ModelCookTask::~ModelCookTask(){if(!temporary.empty()){std::error_code ec;fs::remove(temporary,ec);fs::remove(temporary+".report",ec);fs::remove(temporary+".meta",ec);}}
std::shared_ptr<ModelCookTask> QueueModelImport(JobSystem& jobs,const std::string& recipe){
 auto t=std::make_shared<ModelCookTask>();t->recipe=recipe;
 t->job=jobs.Submit([t](JobContext& context){t->workerCancelled=[&context]{return context.CancelRequested();};CookModelRecipe(t->recipe,*t);t->workerCancelled={};t->done.store(true,std::memory_order_release);if(context.CancelRequested())context.ReportCancelled();else if(!t->success)context.SetError(t->error);},JobPriority::Normal,"Model import");return t;
}
bool CreateModelRecipe(const std::string& projectRoot,const std::string& source,const std::string& output,std::string& recipe,std::string& error){try{auto root=fs::weakly_canonical(projectRoot);auto destination=Relative(root,output);Require(destination.extension()==".judasmodel","output must be .judasmodel");Require(!fs::exists(destination),"output already exists; use its recipe to reimport");auto id=MintAssetId();auto owned=root/"Sources"/id/fs::path(source).filename();fs::create_directories(owned.parent_path());fs::copy_file(source,owned);{std::vector<std::string> dependencies;std::string warning;if(!GatherModelDependencies(source,dependencies,warning))dependencies.clear();auto original=fs::weakly_canonical(fs::path(source).parent_path());for(auto& dependency:dependencies){auto relative=fs::weakly_canonical(dependency).lexically_relative(original);auto copied=owned.parent_path()/relative;fs::create_directories(copied.parent_path());fs::copy_file(dependency,copied,fs::copy_options::overwrite_existing);}}auto path=root/"Imports"/(fs::path(output).stem().string()+".judasimport");Require(!fs::exists(path),"recipe already exists");Json j={{"format","JudasImport"},{"version",1},{"assetId",id},{"source",owned.lexically_relative(root).generic_string()},{"output",destination.lexically_relative(root).generic_string()},{"settings",{{"unitMeters",0},{"sampleRate",60},{"allowBaseMesh",false},{"dependencyRemaps",Json::object()}}},{"motions",Json::array()}};WriteJson(path,j);recipe=path.string();error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
