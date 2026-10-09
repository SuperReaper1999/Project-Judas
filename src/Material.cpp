#include "Material.h"
#include "AssetDatabase.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
namespace {template<class T>bool Range(T x,T a,T b){return std::isfinite(x)&&x>=a&&x<=b;}
void Vec(std::ostream& s,glm::vec4 v){s<<v.x<<' '<<v.y<<' '<<v.z<<' '<<v.w;}
void Vec(std::ostream& s,glm::vec3 v){s<<v.x<<' '<<v.y<<' '<<v.z;}}
bool ValidateMaterial(const MaterialDefinition& m,std::string& e){
 bool ok=int(m.model)>=0&&int(m.model)<=2&&int(m.alpha)>=0&&int(m.alpha)<=2&&Range(m.metallic,0.f,1.f)&&Range(m.roughness,0.f,1.f)&&Range(m.normalStrength,0.f,8.f)&&Range(m.occlusionStrength,0.f,1.f)&&Range(m.emissiveIntensity,0.f,100000.f)&&Range(m.alphaCutoff,0.f,1.f);
 for(int i=0;i<4;++i)ok&=Range(m.baseColor[i],0.f,1.f);
 for(int i=0;i<3;++i)ok&=Range(m.emissive[i],0.f,100000.f);
 for(int i=0;i<2;++i)ok&=std::isfinite(m.uvScale[i])&&std::isfinite(m.uvOffset[i]);
 for(const auto& map:m.maps){ok&=map.asset.empty()||IsValidAssetId(map.asset);ok&=map.uvSet>=0&&map.uvSet<=1&&std::isfinite(map.rotation);for(int i=0;i<2;++i)ok&=std::isfinite(map.scale[i])&&std::isfinite(map.offset[i]);auto& s=map.sampler;ok&=(s.wrapS==10497||s.wrapS==33071||s.wrapS==33648)&&(s.wrapT==10497||s.wrapT==33071||s.wrapT==33648)&&(s.magFilter==9728||s.magFilter==9729)&&(s.minFilter==9728||s.minFilter==9729||(s.minFilter>=9984&&s.minFilter<=9987));}
 if(!ok)e="invalid material factors, texture identity or sampler";else e.clear();return ok;
}
std::string SerializeMaterial(const MaterialDefinition& m){std::ostringstream s;s<<std::setprecision(9)<<"JudasMaterial 1\nmodel "<<int(m.model)<<"\nalpha "<<int(m.alpha)<<"\nbase ";Vec(s,m.baseColor);s<<"\nmetallic "<<m.metallic<<"\nroughness "<<m.roughness<<"\nemissive ";Vec(s,m.emissive);s<<"\nemission "<<m.emissiveIntensity<<"\nnormal "<<m.normalStrength<<"\nocclusion "<<m.occlusionStrength<<"\ncutoff "<<m.alphaCutoff<<"\ndoubleSided "<<m.doubleSided<<"\nflipV "<<m.flipV<<"\nuv "<<m.uvScale.x<<' '<<m.uvScale.y<<' '<<m.uvOffset.x<<' '<<m.uvOffset.y<<'\n';for(size_t i=0;i<m.maps.size();++i){auto& map=m.maps[i];s<<"map "<<i<<' '<<std::quoted(map.asset)<<' '<<map.sampler.wrapS<<' '<<map.sampler.wrapT<<' '<<map.sampler.minFilter<<' '<<map.sampler.magFilter<<'\n';if(map.uvSet||map.scale!=glm::vec2(1)||map.offset!=glm::vec2(0)||map.rotation!=0)s<<"mapUV "<<i<<' '<<map.uvSet<<' '<<map.scale.x<<' '<<map.scale.y<<' '<<map.offset.x<<' '<<map.offset.y<<' '<<map.rotation<<'\n';}return s.str();}
bool ParseMaterial(const std::string& text,MaterialDefinition& out,std::string& e){std::istringstream s(text);std::string k;int version;MaterialDefinition m;if(!(s>>k>>version)||k!="JudasMaterial"||version!=1){e="expected JudasMaterial 1";return false;}while(s>>k){if(k=="model"||k=="alpha"){int v=0;s>>v;if(k=="model")m.model=MaterialModel(v);else m.alpha=MaterialAlpha(v);}else if(k=="base")s>>m.baseColor.x>>m.baseColor.y>>m.baseColor.z>>m.baseColor.w;else if(k=="metallic")s>>m.metallic;else if(k=="roughness")s>>m.roughness;else if(k=="emissive")s>>m.emissive.x>>m.emissive.y>>m.emissive.z;else if(k=="emission")s>>m.emissiveIntensity;else if(k=="normal")s>>m.normalStrength;else if(k=="occlusion")s>>m.occlusionStrength;else if(k=="cutoff")s>>m.alphaCutoff;else if(k=="doubleSided")s>>m.doubleSided;else if(k=="flipV")s>>m.flipV;else if(k=="uv")s>>m.uvScale.x>>m.uvScale.y>>m.uvOffset.x>>m.uvOffset.y;else if(k=="mapUV"){size_t i;if(!(s>>i)||i>=5){e="invalid map UV index";return false;}auto& map=m.maps[i];s>>map.uvSet>>map.scale.x>>map.scale.y>>map.offset.x>>map.offset.y>>map.rotation;}else if(k=="map"){size_t i;if(!(s>>i)||i>=5){e="invalid material map index";return false;}auto& map=m.maps[i];s>>std::quoted(map.asset)>>map.sampler.wrapS>>map.sampler.wrapT>>map.sampler.minFilter>>map.sampler.magFilter;}else{e="unknown material property "+k;return false;}if(!s){e="malformed material property "+k;return false;}}if(!ValidateMaterial(m,e))return false;out=std::move(m);return true;}
bool LoadMaterial(const std::string& path,MaterialDefinition& m,std::string& e){std::ifstream f(path);if(!f){e="cannot read material "+path;return false;}std::ostringstream s;s<<f.rdbuf();return ParseMaterial(s.str(),m,e);}
bool SaveMaterial(const std::string& path,const MaterialDefinition& m,std::string& e){if(!ValidateMaterial(m,e))return false;std::ofstream f(path);f<<SerializeMaterial(m);if(!f){e="cannot write material "+path;return false;}return true;}
MaterialDefinition ApplyMaterialOverride(const MaterialDefinition& src,const MaterialOverride& o){auto m=src;if(o.baseColor)m.baseColor=*o.baseColor;if(o.metallic)m.metallic=*o.metallic;if(o.roughness)m.roughness=*o.roughness;if(o.emissive)m.emissive=*o.emissive;if(o.emissiveIntensity)m.emissiveIntensity=*o.emissiveIntensity;if(o.uvScale)m.uvScale=*o.uvScale;if(o.uvOffset)m.uvOffset=*o.uvOffset;if(o.alpha)m.alpha=*o.alpha;if(o.alphaCutoff)m.alphaCutoff=*o.alphaCutoff;if(o.normalStrength)m.normalStrength=*o.normalStrength;if(o.occlusionStrength)m.occlusionStrength=*o.occlusionStrength;if(o.doubleSided)m.doubleSided=*o.doubleSided;for(size_t i=0;i<5;++i)if(o.textures[i]){m.maps[i].asset=*o.textures[i];m.maps[i].embedded={};m.maps[i].encodedImage.clear();}return m;}
std::string EncodeMaterialSlots(const std::vector<MaterialSlot>& slots){std::ostringstream s;s<<std::setprecision(9)<<slots.size();for(auto& slot:slots){auto& o=slot.overrides;unsigned flags=(o.baseColor?1:0)|(o.metallic?2:0)|(o.roughness?4:0)|(o.emissive?8:0)|(o.emissiveIntensity?16:0)|(o.uvScale?32:0)|(o.uvOffset?64:0);s<<' '<<std::quoted(slot.asset)<<' '<<flags;if(o.baseColor){s<<' ';Vec(s,*o.baseColor);}if(o.metallic)s<<' '<<*o.metallic;if(o.roughness)s<<' '<<*o.roughness;if(o.emissive){s<<' ';Vec(s,*o.emissive);}if(o.emissiveIntensity)s<<' '<<*o.emissiveIntensity;if(o.uvScale)s<<' '<<o.uvScale->x<<' '<<o.uvScale->y;if(o.uvOffset)s<<' '<<o.uvOffset->x<<' '<<o.uvOffset->y;}return s.str();}
bool DecodeMaterialSlots(const std::string& text,std::vector<MaterialSlot>& out,std::string& e){std::istringstream s(text);size_t count;if(!(s>>count)||count>64){e="material slot limit is 64";return false;}std::vector<MaterialSlot> slots(count);for(auto& slot:slots){unsigned flags;if(!(s>>std::quoted(slot.asset)>>flags)||flags>127||(!slot.asset.empty()&&!IsValidAssetId(slot.asset))){e="invalid material slot";return false;}auto& o=slot.overrides;if(flags&1){glm::vec4 v(0);s>>v.x>>v.y>>v.z>>v.w;o.baseColor=v;}if(flags&2){float v=0;s>>v;o.metallic=v;}if(flags&4){float v=0;s>>v;o.roughness=v;}if(flags&8){glm::vec3 v(0);s>>v.x>>v.y>>v.z;o.emissive=v;}if(flags&16){float v=0;s>>v;o.emissiveIntensity=v;}if(flags&32){glm::vec2 v(0);s>>v.x>>v.y;o.uvScale=v;}if(flags&64){glm::vec2 v(0);s>>v.x>>v.y;o.uvOffset=v;}if(!s||!ValidateMaterial(ApplyMaterialOverride(MaterialDefinition{},o),e))return false;}s>>std::ws;if(!s.eof()){e="trailing material slot data";return false;}out=std::move(slots);return true;}

#include "PhysicalMaterial.h"
bool ValidPhysicalMaterial(const PhysicalMaterial& m,std::string& error){if(!std::isfinite(m.friction)||m.friction<0||!std::isfinite(m.restitution)||m.restitution<0||m.restitution>1){error="physical material requires friction >= 0 and restitution 0..1";return false;}return true;}
std::string SerializePhysicalMaterial(const PhysicalMaterial& m){std::ostringstream s;s<<std::setprecision(9)<<"JudasPhysicalMaterial 1\nfriction "<<m.friction<<"\nrestitution "<<m.restitution<<'\n';return s.str();}
bool ParsePhysicalMaterial(const std::string& text,PhysicalMaterial& out,std::string& error){std::istringstream s(text);std::string k;int v;PhysicalMaterial m;bool f=false,r=false;if(!(s>>k>>v)||k!="JudasPhysicalMaterial"||v!=1){error="expected JudasPhysicalMaterial 1";return false;}while(s>>k){if(k=="friction"&&!f){s>>m.friction;f=true;}else if(k=="restitution"&&!r){s>>m.restitution;r=true;}else{error="unknown/duplicate physical material field "+k;return false;}if(!s){error="invalid physical material "+k;return false;}}if(!f||!r){error="physical material requires friction and restitution";return false;}if(!ValidPhysicalMaterial(m,error))return false;out=m;return true;}
bool LoadPhysicalMaterial(const std::string& path,PhysicalMaterial& m,std::string& error){std::ifstream f(path);if(!f){error="cannot read physical material "+path;return false;}std::ostringstream text;text<<f.rdbuf();return ParsePhysicalMaterial(text.str(),m,error);}

MaterialDefinition MaterialSettings(const MaterialDefinition& d){
 MaterialDefinition m;m.model=d.model;m.alpha=d.alpha;m.baseColor=d.baseColor;m.metallic=d.metallic;m.roughness=d.roughness;m.emissive=d.emissive;m.emissiveIntensity=d.emissiveIntensity;m.normalStrength=d.normalStrength;m.occlusionStrength=d.occlusionStrength;m.alphaCutoff=d.alphaCutoff;m.doubleSided=d.doubleSided;m.flipV=d.flipV;m.uvScale=d.uvScale;m.uvOffset=d.uvOffset;
 for(size_t i=0;i<m.maps.size();++i){auto& a=m.maps[i];auto& b=d.maps[i];a.asset=b.asset;a.sampler=b.sampler;a.uvSet=b.uvSet;a.scale=b.scale;a.offset=b.offset;a.rotation=b.rotation;}
 return m;
}

#include "../third_party/nlohmann/json.hpp"
#include "Scene.h"
namespace {
const char* kMaterialMaps[]={"baseColor","metallicRoughness","normal","occlusion","emissive"};
using Json=nlohmann::json;
}
MaterialOverride ComposeMaterialOverrides(const MaterialOverride& a,const MaterialOverride& b){
 auto r=a;
#define OVERRIDE(name) if(b.name)r.name=b.name
 OVERRIDE(baseColor);OVERRIDE(metallic);OVERRIDE(roughness);OVERRIDE(emissiveIntensity);OVERRIDE(emissive);OVERRIDE(uvScale);OVERRIDE(uvOffset);OVERRIDE(alpha);OVERRIDE(alphaCutoff);OVERRIDE(normalStrength);OVERRIDE(occlusionStrength);OVERRIDE(doubleSided);
#undef OVERRIDE
 for(size_t i=0;i<5;++i)if(b.textures[i])r.textures[i]=b.textures[i];
 return r;
}
bool MaterialOverrideEmpty(const MaterialOverride& o){return !o.baseColor&&!o.metallic&&!o.roughness&&!o.emissiveIntensity&&!o.emissive&&!o.uvScale&&!o.uvOffset&&!o.alpha&&!o.alphaCutoff&&!o.normalStrength&&!o.occlusionStrength&&!o.doubleSided&&std::none_of(o.textures.begin(),o.textures.end(),[](auto& t){return t.has_value();});}
std::string EncodeMaterialOverrides(const MaterialOverride& o){
 Json j=Json::object();
 if(o.baseColor)j["baseColor"]={{"x",o.baseColor->x},{"y",o.baseColor->y},{"z",o.baseColor->z},{"a",o.baseColor->w}};
 if(o.emissive)j["emissive"]={{"x",o.emissive->x},{"y",o.emissive->y},{"z",o.emissive->z}};
 for(auto p:{std::pair<const char*,const std::optional<float>*>{"metallic",&o.metallic},{"roughness",&o.roughness},{"emissiveIntensity",&o.emissiveIntensity},{"alphaCutoff",&o.alphaCutoff},{"normalStrength",&o.normalStrength},{"occlusionStrength",&o.occlusionStrength}})if(*p.second)j[p.first]=**p.second;
 if(o.alpha)j["alphaMode"]=std::array<const char*,3>{"opaque","mask","blend"}.at(int(*o.alpha));
 if(o.doubleSided)j["doubleSided"]=*o.doubleSided;
 for(auto p:{std::pair<const char*,const std::optional<glm::vec2>*>{"uvScale",&o.uvScale},{"uvOffset",&o.uvOffset}})if(*p.second)j[p.first]={{"x",(**p.second).x},{"y",(**p.second).y}};
 for(size_t i=0;i<5;++i)if(o.textures[i])j["textures"][kMaterialMaps[i]]=*o.textures[i];
 return j.dump();
}
bool PatchMaterialOverrides(const std::string& text,MaterialOverride& out,std::string& error){
 try{auto j=Json::parse(text);
 if(!j.is_object())throw std::runtime_error("material parameters must be an object");
 auto o=out;
 for(auto it=j.begin();it!=j.end();++it){auto& v=it.value();
 auto& k=it.key();
  if(k=="baseColor")o.baseColor=glm::vec4(v.at("x").get<float>(),v.at("y").get<float>(),v.at("z").get<float>(),v.at("a").get<float>());
  else if(k=="emissive")o.emissive=glm::vec3(v.at("x").get<float>(),v.at("y").get<float>(),v.at("z").get<float>());
  else if(k=="uvScale"||k=="uvOffset"){glm::vec2 p(v.at("x").get<float>(),v.at("y").get<float>());
 if(k=="uvScale")o.uvScale=p;else o.uvOffset=p;}
  else if(k=="alphaMode"){auto a=v.get<std::string>();
 if(a=="opaque")o.alpha=MaterialAlpha::Opaque;else if(a=="mask")o.alpha=MaterialAlpha::Mask;else if(a=="blend")o.alpha=MaterialAlpha::Blend;else throw std::runtime_error("alphaMode must be opaque/mask/blend");}
  else if(k=="doubleSided")o.doubleSided=v.get<bool>();
  else if(k=="textures"){if(!v.is_object())throw std::runtime_error("textures must be named asset references");for(auto t=v.begin();t!=v.end();++t){int map=-1;for(int i=0;i<5;++i)if(t.key()==kMaterialMaps[i])map=i;
 if(map<0)throw std::runtime_error("unsupported texture slot: "+t.key());
 o.textures[map]=t.value().get<std::string>();}}
  else {std::optional<float>* field=nullptr;
 if(k=="metallic")field=&o.metallic;else if(k=="roughness")field=&o.roughness;else if(k=="emissiveIntensity")field=&o.emissiveIntensity;else if(k=="alphaCutoff")field=&o.alphaCutoff;else if(k=="normalStrength")field=&o.normalStrength;else if(k=="occlusionStrength")field=&o.occlusionStrength;
 if(!field)throw std::runtime_error("unknown material parameter: "+k);
 *field=v.get<float>();}
 }
 if(!ValidateMaterial(ApplyMaterialOverride(MaterialDefinition{},o),error))return false;
 out=std::move(o);
 error.clear();
 return true;
 }catch(const std::exception& e){error=e.what();
 return false;}
}
bool DecodeMaterialOverrides(const std::string& text,MaterialOverride& out,std::string& e){MaterialOverride o;
 if(!PatchMaterialOverrides(text,o,e))return false;
 out=std::move(o);
 return true;}
MaterialSlot ResolveRenderMaterial(const SceneRenderComponent& render,unsigned slot,const std::string& part){
 MaterialSlot result;result.overrides=render.instanceOverrides;
 auto apply=[&](const MaterialSlot& v){if(v.useSource||!v.asset.empty())result.asset=v.asset;result.overrides=ComposeMaterialOverrides(result.overrides,v.overrides);};
 if(slot<render.materials.size())apply(render.materials[slot]);
 if(auto it=render.partMaterials.find(part);it!=render.partMaterials.end())apply(it->second);
 for(const auto& key:{std::string("*"),std::string("#")+std::to_string(slot),part})if(!key.empty())if(auto it=render.runtimeMaterials.find(key);it!=render.runtimeMaterials.end())apply(it->second);
 return result;
}
bool ValidateAppearance(const SceneSettings& s,std::string& error){
 bool ok=Range(s.sunIntensity,0.f,10000.f)&&Range(s.exposure,.000001f,10000.f)&&Range(s.environmentIntensity,0.f,10000.f);
 for(int i=0;i<3;++i)ok&=Range(s.backgroundColor[i],0.f,10000.f)&&Range(s.sunColor[i],0.f,10000.f)&&Range(s.ambientColor[i],0.f,10000.f)&&std::isfinite(s.sunDirection[i]);
 float length=glm::dot(s.sunDirection,s.sunDirection);ok&=std::isfinite(length)&&length>1e-12f;
 float q=glm::dot(s.environmentRotation,s.environmentRotation);ok&=std::isfinite(q)&&q>1e-12f;
 if(!ok)error="invalid sun, ambient or environment settings (finite direction, colors/intensity 0..10000, positive exposure required)";else error.clear();
 return ok;
}
std::string EncodeAppearanceState(const SceneSettings& s){
 auto v=[](glm::vec3 a){return Json::array({a.x,a.y,a.z});};
 return Json{{"version",1},{"sunEnabled",s.sunEnabled},{"sunIntensity",s.sunIntensity},{"sunDirection",v(s.sunDirection)},{"sunColor",v(s.sunColor)},{"ambientColor",v(s.ambientColor)},{"backgroundColor",v(s.backgroundColor)},{"exposure",s.exposure},{"environmentIntensity",s.environmentIntensity},{"environmentAsset",s.environmentAsset},{"environmentBackground",s.environmentBackground},{"linearRendering",s.linearRendering},{"environmentRotation",Json::array({s.environmentRotation.w,s.environmentRotation.x,s.environmentRotation.y,s.environmentRotation.z})}}.dump();
}
bool DecodeAppearanceState(const std::string& text,SceneSettings& out,std::string& error){try{auto j=Json::parse(text);
 if(!j.is_object()||j.size()!=13||!j.at("version").is_number_integer()||j.at("version")!=1)throw std::runtime_error("invalid or unsupported appearance state record");
 auto s=out;
 auto v=[&](const char* name){auto a=j.at(name);
 if(!a.is_array()||a.size()!=3)throw std::runtime_error("appearance vector requires three components");
 return glm::vec3(a.at(0).get<float>(),a.at(1).get<float>(),a.at(2).get<float>());};
 s.sunEnabled=j.at("sunEnabled").get<bool>();s.sunIntensity=j.at("sunIntensity").get<float>();s.sunDirection=v("sunDirection");s.sunColor=v("sunColor");s.ambientColor=v("ambientColor");s.backgroundColor=v("backgroundColor");s.exposure=j.at("exposure").get<float>();s.environmentIntensity=j.at("environmentIntensity").get<float>();s.environmentAsset=j.at("environmentAsset").get<std::string>();s.environmentBackground=j.at("environmentBackground").get<bool>();s.linearRendering=j.at("linearRendering").get<bool>();
 auto q=j.at("environmentRotation");
 if(!q.is_array()||q.size()!=4)throw std::runtime_error("appearance quaternion requires four components");
 s.environmentRotation={q.at(0).get<float>(),q.at(1).get<float>(),q.at(2).get<float>(),q.at(3).get<float>()};
 if(!s.environmentAsset.empty()&&!IsValidAssetId(s.environmentAsset))throw std::runtime_error("invalid environment identity");
 if(!ValidateAppearance(s,error))return false;
 out=std::move(s);
 return true;
 }catch(const std::exception& e){error=e.what();
 return false;}}
