#include "PowertrainAuthoring.h"
#include "Scene.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <stdexcept>
#include <cmath>

namespace {
using J=nlohmann::ordered_json;
void Require(bool condition,const char* error){if(!condition)throw std::runtime_error(error);}
void Keys(const J& value,std::initializer_list<const char*> names){Require(value.is_object()&&value.size()==names.size(),"powertrain record has missing/unknown fields");for(auto name:names)Require(value.contains(name),"powertrain record has missing field");}
double Number(const J& value){Require(value.is_number(),"powertrain expects a number");const auto result=value.get<double>();Require(std::isfinite(result),"nonfinite powertrain number");return result;}
unsigned Index(const J& value){Require(value.is_number_integer()||value.is_number_unsigned(),"powertrain index must be an integer");const auto n=Number(value);Require(n>=0&&n<=64,"powertrain index outside bounded graph");return unsigned(n);}
uint64_t Id(const J& value){Require(value.is_string(),"powertrain entity identities must be decimal strings");const auto text=value.get<std::string>();uint64_t result=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);Require(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&text==std::to_string(result),"invalid powertrain entity identity");return result;}
template<class E> E Kind(const J& value,std::initializer_list<std::pair<const char*,E>> names){Require(value.is_string(),"powertrain kind must be a string");const auto name=value.get<std::string>();for(auto [text,kind]:names)if(name==text)return kind;throw std::runtime_error("unknown powertrain kind");}
void Array(const J& value,size_t limit){Require(value.is_array()&&value.size()<=limit,"powertrain graph exceeds array bound");}
const char* ShaftName(PowertrainShaftKind kind){return kind==PowertrainShaftKind::Wheel?"wheel":kind==PowertrainShaftKind::Body?"body":"internal";}
const char* SourceName(PowertrainSourceKind kind){return kind==PowertrainSourceKind::Combustion?"combustion":kind==PowertrainSourceKind::Electric?"electric":"custom";}
const char* LinkName(PowertrainLinkKind kind){return kind==PowertrainLinkKind::Clutch?"clutch":kind==PowertrainLinkKind::Differential?"differential":"gear";}
}
std::string EncodePowertrainSettings(const PowertrainSettings& settings){
 J j={{"version",1},{"enabled",settings.enabled},{"shafts",J::array()},{"sources",J::array()},{"links",J::array()},{"brakes",J::array()}};
 for(const auto& v:settings.shafts)j["shafts"].push_back({{"name",v.name},{"kind",ShaftName(v.kind)},{"inertia",v.inertia},{"initialAngularVelocity",v.initialAngularVelocity},{"viscousDrag",v.viscousDrag},{"target",std::to_string(v.target)},{"bearing",std::to_string(v.bearing)},{"axis",{v.axis.x,v.axis.y,v.axis.z}}});
 for(const auto& v:settings.sources){J curve=J::array();for(const auto& p:v.torqueCurve)curve.push_back({p.rpm,p.torque});j["sources"].push_back({{"name",v.name},{"shaft",v.shaft},{"kind",SourceName(v.kind)},{"torqueCurve",curve},{"maxTorque",v.maxTorque},{"maxPower",v.maxPower},{"starterTorque",v.starterTorque}});}
 for(const auto& v:settings.links)j["links"].push_back({{"name",v.name},{"kind",LinkName(v.kind)},{"input",v.input},{"output",v.output},{"outputB",v.outputB},{"ratio",v.ratio},{"maxTorque",v.maxTorque}});
 for(const auto& v:settings.brakes)j["brakes"].push_back({{"name",v.name},{"shaft",v.shaft},{"maxTorque",v.maxTorque}});
 return j.dump();
}
bool DecodePowertrainSettings(const std::string& text,PowertrainSettings& out,std::string& error){try{
 Require(text.size()<=128*1024,"powertrain graph exceeds 128 KiB");auto j=J::parse(text);Keys(j,{"version","enabled","shafts","sources","links","brakes"});Require(j.at("version")==1&&j.at("enabled").is_boolean(),"unsupported powertrain graph version/enabled");
 PowertrainSettings settings;settings.enabled=j.at("enabled").get<bool>();Array(j.at("shafts"),kPowertrainMaxShafts);Array(j.at("sources"),kPowertrainMaxSources);Array(j.at("links"),kPowertrainMaxRows);Array(j.at("brakes"),kPowertrainMaxBrakes);
 for(const auto& a:j.at("shafts")){Keys(a,{"name","kind","inertia","initialAngularVelocity","viscousDrag","target","bearing","axis"});PowertrainShaft v;v.name=a.at("name").get<std::string>();v.kind=Kind<PowertrainShaftKind>(a.at("kind"),{{"internal",PowertrainShaftKind::Internal},{"wheel",PowertrainShaftKind::Wheel},{"body",PowertrainShaftKind::Body}});v.inertia=Number(a.at("inertia"));v.initialAngularVelocity=Number(a.at("initialAngularVelocity"));v.viscousDrag=Number(a.at("viscousDrag"));v.target=Id(a.at("target"));v.bearing=Id(a.at("bearing"));Array(a.at("axis"),3);Require(a.at("axis").size()==3,"powertrain axis needs three components");for(unsigned i=0;i<3;++i)v.axis[i]=float(Number(a.at("axis").at(i)));settings.shafts.push_back(v);}
 for(const auto& a:j.at("sources")){Keys(a,{"name","shaft","kind","torqueCurve","maxTorque","maxPower","starterTorque"});PowertrainSource v;v.name=a.at("name").get<std::string>();v.shaft=Index(a.at("shaft"));v.kind=Kind<PowertrainSourceKind>(a.at("kind"),{{"combustion",PowertrainSourceKind::Combustion},{"electric",PowertrainSourceKind::Electric},{"custom",PowertrainSourceKind::Custom}});Array(a.at("torqueCurve"),64);for(const auto& point:a.at("torqueCurve")){Array(point,2);Require(point.size()==2,"torque point needs RPM and torque");v.torqueCurve.push_back({Number(point[0]),Number(point[1])});}v.maxTorque=Number(a.at("maxTorque"));v.maxPower=Number(a.at("maxPower"));v.starterTorque=Number(a.at("starterTorque"));settings.sources.push_back(v);}
 for(const auto& a:j.at("links")){Keys(a,{"name","kind","input","output","outputB","ratio","maxTorque"});PowertrainLink v;v.name=a.at("name").get<std::string>();v.kind=Kind<PowertrainLinkKind>(a.at("kind"),{{"gear",PowertrainLinkKind::Gear},{"clutch",PowertrainLinkKind::Clutch},{"differential",PowertrainLinkKind::Differential}});v.input=Index(a.at("input"));v.output=Index(a.at("output"));v.outputB=Index(a.at("outputB"));v.ratio=Number(a.at("ratio"));v.maxTorque=Number(a.at("maxTorque"));settings.links.push_back(v);}
 for(const auto& a:j.at("brakes")){Keys(a,{"name","shaft","maxTorque"});PowertrainBrake v;v.name=a.at("name").get<std::string>();v.shaft=Index(a.at("shaft"));v.maxTorque=Number(a.at("maxTorque"));settings.brakes.push_back(v);}
 if(!ValidatePowertrainSettings(settings,error))return false;
 out=std::move(settings);return true;
 }catch(const std::exception& e){error=e.what();return false;}}
std::vector<uint64_t> PowertrainReferences(const PowertrainSettings& settings){std::set<uint64_t> ids;for(const auto& shaft:settings.shafts){if(shaft.target)ids.insert(shaft.target);if(shaft.bearing)ids.insert(shaft.bearing);}return {ids.begin(),ids.end()};}
void RemapPowertrainReferences(PowertrainSettings& settings,const std::function<uint64_t(uint64_t)>& remap){for(auto& shaft:settings.shafts){if(shaft.target)shaft.target=remap(shaft.target);if(shaft.bearing)shaft.bearing=remap(shaft.bearing);}}
bool ValidateScenePowertrains(const Scene& scene,std::string& error){
 std::set<std::pair<PowertrainShaftKind,uint64_t>> terminals;
 for(const auto& o:scene.Objects())if(o.powertrain){if(!ValidatePowertrainSettings(*o.powertrain,error))return false;
  for(const auto& shaft:o.powertrain->shafts){const auto* target=scene.Find(shaft.target);const auto* bearing=scene.Find(shaft.bearing);
   if(shaft.bearing&&(!bearing||!bearing->body||bearing->characterMotor||bearing->deformable)){error="powertrain bearing must reference an ordinary rigid body";return false;}
   if(shaft.kind==PowertrainShaftKind::Wheel&&(shaft.bearing||!target||!target->wheel)){error="powertrain wheel terminal must reference a wheel component";return false;}
   if(shaft.kind==PowertrainShaftKind::Body&&(!target||!target->body||target->body->motion!=SceneBodyMotion::Dynamic||target->characterMotor||target->deformable)){error="powertrain body terminal must reference an ordinary dynamic body";return false;}
   if(shaft.kind!=PowertrainShaftKind::Internal&&!terminals.emplace(shaft.kind,shaft.target).second){error="powertrain terminal has competing assembly ownership";return false;}
  }
 }
 return true;
}
