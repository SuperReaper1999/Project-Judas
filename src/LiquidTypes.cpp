#include "LiquidTypes.h"
#include "Scene.h"
#include "AssetDatabase.h"
#include "SceneFingerprint.h"
#include "UniformGravity.h"
#include "RadicalGravity.h"
#include "GravityContextMap.h"
#include "GravityVolume.h"
#include "OrientedGravityVolume.h"
#include "SphericalVolume.h"
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <type_traits>
namespace {
template<class T>std::string text(T v){std::ostringstream s;s<<std::setprecision(17)<<v;return s.str();}
bool material(const LiquidMaterial& m){return !m.id.empty()&&m.id.size()<=64&&std::isfinite(m.density)&&m.density>0;}
}
std::map<std::string,std::string> LiquidProperties(const SceneObject& o){std::map<std::string,std::string> p;
#define F(prefix,obj,field) p["liquid." prefix "." #field]=text(obj.field)
 if(o.liquidBasin){auto& a=*o.liquidBasin;F("basin",a,enabled);F("basin",a,geometry);F("basin",a,asset);F("basin",a,initialVolume);F("basin",a,volumeTolerance);F("basin",a,heightTolerance);F("basin",a,material.id);F("basin",a,material.density);if(a.surface.enabled){F("basin",a,surface.enabled);F("basin",a,surface.columns);F("basin",a,surface.rows);F("basin",a,surface.friction);F("basin",a,surface.tolerance);F("basin",a,surface.iterations);F("basin",a,surface.splashSpeed);F("basin",a,surface.splashFraction);F("basin",a,surface.parcelBudget);}}
 if(o.liquidContainer){auto& a=*o.liquidContainer;F("container",a,enabled);F("container",a,geometry);F("container",a,initialVolume);F("container",a,material.id);F("container",a,material.density);F("container",a,openingArea);F("container",a,discharge);std::ostringstream s;s<<std::setprecision(17)<<a.opening.size();for(auto v:a.opening)s<<' '<<v.x<<' '<<v.y<<' '<<v.z;p["liquid.container.opening"]=s.str();}
 if(o.liquidConnection){auto& a=*o.liquidConnection;F("connection",a,enabled);F("connection",a,bidirectional);F("connection",a,source);F("connection",a,destination);F("connection",a,openingArea);F("connection",a,discharge);}
 if(o.liquidInteraction){auto& a=*o.liquidInteraction;F("interaction",a,enabled);F("interaction",a,drag);}
#undef F
 return p;}
bool ApplyLiquidProperties(const std::map<std::string,std::string>& p,SceneObject& o,std::string& error){auto read=[&](std::string key,auto& v){auto it=p.find(key);if(it==p.end()){error="missing "+key;return false;}if constexpr(std::is_same_v<std::decay_t<decltype(v)>,std::string>)v=it->second;else{std::istringstream s(it->second);if(!(s>>v)){error="invalid "+key;return false;}s>>std::ws;if(!s.eof()){error="invalid "+key;return false;}}return true;};
#define R(prefix,obj,field) if(!read("liquid." prefix "." #field,obj.field))return false
 if(p.count("liquid.basin.enabled")){LiquidBasinSettings a;R("basin",a,enabled);R("basin",a,geometry);R("basin",a,asset);R("basin",a,initialVolume);R("basin",a,volumeTolerance);R("basin",a,heightTolerance);R("basin",a,material.id);R("basin",a,material.density);if(p.count("liquid.basin.surface.enabled")){R("basin",a,surface.enabled);R("basin",a,surface.columns);R("basin",a,surface.rows);R("basin",a,surface.friction);R("basin",a,surface.tolerance);R("basin",a,surface.iterations);R("basin",a,surface.splashSpeed);R("basin",a,surface.splashFraction);R("basin",a,surface.parcelBudget);}o.liquidBasin=a;}
 if(p.count("liquid.container.enabled")){LiquidContainerSettings a;R("container",a,enabled);R("container",a,geometry);R("container",a,initialVolume);R("container",a,material.id);R("container",a,material.density);R("container",a,openingArea);R("container",a,discharge);auto it=p.find("liquid.container.opening");if(it==p.end()){error="missing opening";return false;}std::istringstream s(it->second);size_t n;if(!(s>>n)||n<3||n>32){error="opening requires 3..32 polygon vertices";return false;}a.opening.resize(n);for(auto& v:a.opening)if(!(s>>v.x>>v.y>>v.z)){error="invalid opening";return false;}s>>std::ws;if(!s.eof())return false;o.liquidContainer=a;}
 if(p.count("liquid.connection.enabled")){LiquidConnectionSettings a;R("connection",a,enabled);R("connection",a,bidirectional);R("connection",a,source);R("connection",a,destination);R("connection",a,openingArea);R("connection",a,discharge);o.liquidConnection=a;}
 if(p.count("liquid.interaction.enabled")){LiquidInteractionSettings a;R("interaction",a,enabled);R("interaction",a,drag);o.liquidInteraction=a;}
#undef R
 if(LiquidProperties(o).size()!=p.size()){error="unknown liquid property";return false;}return ValidateLiquidComponents(o,error);}
bool ValidateLiquidComponents(const SceneObject& o,std::string& error){auto volume=[](double x){return std::isfinite(x)&&x>=0;};if(o.fluidVolume&&(o.liquidBasin||o.liquidContainer)){error="one water body cannot use particle and conserved ownership together";return false;}if(o.liquidBasin&&o.liquidContainer){error="one entity cannot own both basin and container";return false;}if(o.liquidBasin){auto& a=*o.liquidBasin;if(!material(a.material)||!volume(a.initialVolume)||a.volumeTolerance<=0||a.heightTolerance<=0||!std::isfinite(a.volumeTolerance+a.heightTolerance)){error="invalid liquid basin settings";return false;}if(a.surface.enabled&&(a.surface.columns<2||a.surface.rows<2||a.surface.columns>32||a.surface.rows>32||!std::isfinite(a.surface.friction+a.surface.tolerance+a.surface.splashSpeed+a.surface.splashFraction)||a.surface.friction<0||a.surface.tolerance<=0||a.surface.iterations<4||a.surface.iterations>512||a.surface.splashSpeed<=0||a.surface.splashFraction<0||a.surface.splashFraction>1||a.surface.parcelBudget>1024)){error="invalid dynamic surface settings";return false;}if(o.body&&o.body->motion==SceneBodyMotion::Dynamic){error="baked basins must be static";return false;}}
 if(o.liquidContainer){auto& a=*o.liquidContainer;if(!material(a.material)||!volume(a.initialVolume)||a.opening.size()<3||a.opening.size()>32||a.openingArea<=0||a.discharge<=0||!std::isfinite(a.openingArea+a.discharge)){error="invalid liquid container settings";return false;}for(auto p:a.opening)if(!std::isfinite(glm::dot(p,p))){error="invalid opening vertex";return false;}if(o.body&&!o.body->fluidCavities.empty()){error="a cavity cannot use particle and conserved liquid ownership together";return false;}}
 if(o.liquidConnection){auto a=*o.liquidConnection;if(!a.source||!a.destination||a.source==a.destination||a.openingArea<=0||a.discharge<=0||!std::isfinite(a.openingArea+a.discharge)){error="invalid spill connection";return false;}}
 if(o.body&&o.body->shape==SceneShape::Compound&&(o.liquidInteraction||o.liquidContainer||!o.body->fluidCavities.empty()))for(auto& child:o.body->compoundBoxes)if(child.type!=ShapeType::Box){error="liquid volume/cavity ownership requires supported box children, not hull proxies";return false;}
 if(o.liquidInteraction&&o.body&&!o.body->fluidCavities.empty()){error="conserved-liquid interaction cannot share a particle-fluid cavity";return false;}if(o.liquidInteraction&&(!volume(o.liquidInteraction->drag)||!o.body||(o.body->shape!=SceneShape::Box&&o.body->shape!=SceneShape::Compound))){error="liquid interaction requires a box/compound collider and finite drag";return false;}
 if((o.liquidBasin||o.liquidContainer)&&o.transform.scale!=glm::vec3(1)){error="liquid geometry is in simulation metres; entity scale must be one";return false;}return true;}
bool PrepareLiquidBake(const Scene& scene,const SceneObject& o,const AssetDatabase& assets,LiquidGeometry& g,GravityEquilibrium& local,std::string& hash,std::string& error){if(!o.liquidBasin){error="not a liquid basin";return false;}auto* a=assets.Find(o.liquidBasin->geometry);if(!a||a->missing||!LoadLiquidGeometry(a->path,g,error)){error="basin cavity: "+error;return false;}
 // Bake constructs ordinary authored gravity regions in the SAME registration
 // order as RuntimeWorld. No liquid consumer branches on gravity implementation.
 std::vector<std::unique_ptr<GravityField>> fields;std::vector<std::unique_ptr<GravityVolume>> volumes;GravityContextMap resolver;
 for (const auto& x : scene.Objects()) if (x.gravity) {
     const auto& settings = *x.gravity;
     // RuntimeWorld normalizes authored orientations before constructing fields.
     // Using the raw float quaternion here gives a different physical field.
     const auto rotation = glm::normalize(x.transform.rotation);
     std::unique_ptr<GravityField> field;
     if (settings.kind == SceneGravityKind::Radial)
         field = std::make_unique<RadicalGravity>(x.transform.position, settings.magnitude);
     else
         field = std::make_unique<UniformGravity>(rotation * glm::vec3(0, -settings.magnitude, 0));
     std::unique_ptr<GravityVolume> volume;
     if (settings.regionShape == SceneRegionShape::Box)
         volume = std::make_unique<OrientedGravityVolume>(x.transform.position, rotation, settings.regionHalfExtents);
     else
         volume = std::make_unique<SphericalVolume>(x.transform.position, settings.regionRadius);
     resolver.AddRegion(*field, *volume);
     fields.push_back(std::move(field));
     volumes.push_back(std::move(volume));
 }
 const auto ownerRotation = glm::normalize(o.transform.rotation);
 GravityEquilibrium eq;
 if (!resolver.Equilibrium(o.transform.position, eq)) {
     error = "basin has no supported conservative gravity equilibrium";
     return false;
 }
 for (const auto& tet : g.cells) for (auto vertex : tet) {
     GravityEquilibrium sampled;
     const auto point = o.transform.position + ownerRotation * glm::vec3(vertex);
     if (!resolver.Equilibrium(point, sampled) || !eq.Equivalent(sampled)) {
         error = "basin spans incompatible gravity regions";
         return false;
     }
 }
 local = eq;
 const auto inverse = glm::inverse(glm::dquat(ownerRotation));
 local.up = inverse * eq.up;
 local.center = inverse * (eq.center - glm::dvec3(o.transform.position));
 std::ifstream f(a->path,std::ios::binary);std::ostringstream bytes;bytes<<f.rdbuf();std::ostringstream s;s<<std::setprecision(17)<<"Judas.LiquidBake.1 "<<a->id<<' '<<bytes.str()<<' '<<o.liquidBasin->volumeTolerance<<' '<<o.liquidBasin->heightTolerance<<' '<<int(local.kind)<<' '<<local.up.x<<' '<<local.up.y<<' '<<local.up.z<<' '<<local.center.x<<' '<<local.center.y<<' '<<local.center.z<<' '<<local.magnitude;if(o.liquidBasin->surface.enabled){auto& d=o.liquidBasin->surface;s<<" Surface1 "<<d.columns<<' '<<d.rows<<' '<<d.friction<<' '<<d.tolerance<<' '<<d.iterations<<' '<<d.splashSpeed<<' '<<d.splashFraction<<' '<<d.parcelBudget;}hash=SceneFingerprintSha256(s.str());return true;}
std::string LiquidSourceFingerprint(const Scene& s,const SceneObject& o,const AssetDatabase& a,std::string& e){LiquidGeometry g;GravityEquilibrium eq;std::string h;if(!PrepareLiquidBake(s,o,a,g,eq,h,e))return {};return h;}
bool DecodeLiquidResource(const std::vector<unsigned char>& bytes,LiquidResource& out,std::string& error){std::string data(bytes.begin(),bytes.end());if((data.rfind("JudasBasin1 ",0)==0||data.rfind("JudasBasin2 ",0)==0)){auto b=std::make_shared<LiquidBasinData>();if(!DecodeLiquidBasin(bytes,*b,error))return false;out.basin=b;return true;}std::istringstream s(data);std::string magic;size_t n;if(!(s>>magic>>n)||magic!="JudasCavity1"||n>16384){error="invalid liquid geometry asset";return false;}out.geometry.cells.resize(n);for(auto& t:out.geometry.cells)for(auto& p:t)if(!(s>>p.x>>p.y>>p.z)){error="truncated cavity";return false;}s>>std::ws;return s.eof()&&ValidateLiquidGeometry(out.geometry,error);}
