#include "NamedAuthoring.h"
#include "NamedInput.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "Prefab.h"
#include "RuntimeUI.h"
#include "WorldStreaming.h"
#include "../third_party/nlohmann/json.hpp"
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <limits>
using J=nlohmann::ordered_json;
namespace {
using AuthoringJSON::require;using AuthoringJSON::keys;using AuthoringJSON::typed;using AuthoringJSON::parse;
std::string quote(const std::string& s){require(s.find_first_of("\r\n\0",0,3)==std::string::npos,"legacy field cannot represent newline/NUL");std::ostringstream out;out<<std::quoted(s);return out.str();}
std::string atom(const J& v){if(v.is_object()&&v.contains("json")){keys(v,{"json"},"embedded JSON");return quote(v.at("json").dump());}if(v.is_boolean())return v.get<bool>()?"true":"false";if(v.is_number()){require(!v.is_number_float()||std::isfinite(v.get<double>()),"non-finite number");return v.dump();}if(v.is_string())return quote(v.get<std::string>());keys(v,{"symbol"},"symbol");auto s=v.at("symbol").get<std::string>();require(!s.empty()&&s.find_first_of(" \t\r\n\"#")==std::string::npos,"invalid symbol");return s;}
J tokens(std::istream& in){J a=J::array();in.imbue(std::locale::classic());while(in>>std::ws&&!in.eof()){if(in.peek()=='#')break;if(in.peek()=='"'){std::string s;require(bool(in>>std::quoted(s)),"unterminated string");a.push_back(s);continue;}std::string s;in>>s;if(s=="true"||s=="false"){a.push_back(s=="true");continue;}try{auto n=s=="-0"?J(-0.0):J::parse(s);if(n.is_number()){a.push_back(n);continue;}}catch(const std::exception&){}a.push_back(J{{"symbol",s}});}return a;}
std::string values(const J& j){if(!j.is_array())return atom(j);require(j.size()<=65536,"field tuple exceeds limit");std::string s;for(auto& v:j){if(!s.empty())s+=' ';s+=atom(v);}return s;}
J scalar(J a){return a.size()==1?a[0]:a;}
// Multi-value component records use named members. Vector/quaternion fields stay
// explicit fixed-size arrays; the canonical serializer remains the validator.
J namedField(const std::string& key,const J& value) {
    if(key=="body")return {{"motion",value.at(0)},{"shape",value.at(1)}};
    if(key=="gravity")return {{"kind",value.at(0)},{"magnitude",value.at(1)}};
    if(key=="gravity.region") {
        auto shape=value.at(0).at("symbol").get<std::string>();
        if(shape=="sphere")return {{"shape",value.at(0)},{"radius",value.at(1)}};
        return {{"shape",value.at(0)},{"halfExtents",{value.at(1),value.at(2),value.at(3)}}};
    }
    if(key=="light.cone")return {{"innerDegrees",value.at(0)},{"outerDegrees",value.at(1)}};
    if(key=="fidelity-policy") {
        if(value.is_object())return {{"mode",value}};
        return {{"mode",value.at(0)},{"fullRadius",value.at(1)},{"coarseRadius",value.at(2)}};
    }
    if(key=="body.compound-box"||key=="body.fluid-cavity") {
        J result={{"center",{value.at(0),value.at(1),value.at(2)}},
                  {"halfExtents",{value.at(3),value.at(4),value.at(5)}}};
        if(value.size()>6) {
            result["rotation"]={value.at(6),value.at(7),value.at(8),value.at(9)};
            result["shape"]=value.at(10);result["radius"]=value.at(11);
            result["asset"]=value.at(12);result["key"]=value.at(13);
        }
        return result;
    }
    return value;
}
J fieldTokens(const std::string& key,const J& value) {
    if(!value.is_object()||value.contains("json")||value.contains("symbol"))return value;
    J result=J::array();
    auto append=[&](const char* name,size_t size=0){const auto& member=value.at(name);
        if(size){require(member.is_array()&&member.size()==size,key+"/"+name+": wrong vector size");for(auto& v:member)result.push_back(v);}
        else result.push_back(member);
    };
    if(key=="body"){keys(value,{"motion","shape"},key);append("motion");append("shape");}
    else if(key=="gravity"){keys(value,{"kind","magnitude"},key);append("kind");append("magnitude");}
    else if(key=="gravity.region"){
        keys(value,{"shape","radius","halfExtents"},key);append("shape");
        if(value.at("shape").at("symbol")=="sphere")append("radius");else append("halfExtents",3);
        require(value.size()==2,key+": use radius OR halfExtents");
    }
    else if(key=="light.cone"){keys(value,{"innerDegrees","outerDegrees"},key);append("innerDegrees");append("outerDegrees");}
    else if(key=="fidelity-policy"){
        keys(value,{"mode","fullRadius","coarseRadius"},key);append("mode");
        if(value.at("mode").at("symbol")!="none"){append("fullRadius");append("coarseRadius");}
        else require(value.size()==1,key+": none has no radii");
    }
    else if(key=="body.compound-box"||key=="body.fluid-cavity"){
        keys(value,{"center","halfExtents","rotation","shape","radius","asset","key"},key);
        append("center",3);append("halfExtents",3);
        if(value.contains("rotation")){require(key=="body.compound-box",key+": unsupported rotation");append("rotation",4);append("shape");append("radius");append("asset");append("key");}
        else require(value.size()==2,key+": extended shape requires rotation/shape/radius/asset/key");
    }
    else throw std::runtime_error(key+": unsupported named field object (use scalar/vector or explicit json)");
    return result;
}
J fieldBlock(std::istream& in){J b=J::object();std::string line;while(std::getline(in,line)){std::istringstream row(line);std::string key;row>>key;if(key.empty()||key[0]=='#')continue;if(key=="end")return b;auto a=scalar(tokens(row));if(key=="authoring-recipes")a=J{{"json",parse(a.get<std::string>())}};if(key=="body.compound-box"||key=="body.fluid-cavity"){if(!b.contains(key))b[key]=J::array();b[key].push_back(namedField(key,a));}else{require(!b.contains(key),"duplicate field "+key);b[key]=namedField(key,a);}}throw std::runtime_error("missing end");}
std::string block(const J& b){require(b.is_object(),"fields: object required");std::string s;for(auto i=b.begin();i!=b.end();++i){require(i.key().find_first_of(" \t\r\n\"#")==std::string::npos,"invalid field key");if(i.key()=="body.compound-box"||i.key()=="body.fluid-cavity"){require(i.value().is_array(),i.key()+": repeated rows required");for(auto& v:i.value())s+=i.key()+" "+values(fieldTokens(i.key(),v))+"\n";}else s+=i.key()+" "+values(fieldTokens(i.key(),i.value()))+"\n";}return s+"end\n";}
J vec(glm::vec2 v){return {v.x,v.y};}J vec(glm::vec4 v){return {v.x,v.y,v.z,v.w};}
void v2(const J& j,glm::vec2& v){require(j.is_array()&&j.size()==2,"vec2 requires two numbers");v={j[0].get<float>(),j[1].get<float>()};}
void v4(const J& j,glm::vec4& v){require(j.is_array()&&j.size()==4,"vec4 requires four numbers");v={j[0].get<float>(),j[1].get<float>(),j[2].get<float>(),j[3].get<float>()};}
using AuthoringJSON::input;
J registry(const CategoryRegistry& r){J a=J::array();for(auto [id,n]:r.names)a.push_back({{"id",id},{"name",n}});return {{"nextId",r.nextId},{"entries",a}};}
CategoryRegistry registry(const J& j){keys(j,{"nextId","entries"},"registry");CategoryRegistry r;r.nextId=typed<unsigned>(j.at("nextId"));for(auto& e:j.at("entries")){keys(e,{"id","name"},"registry/entry");require(r.names.emplace(typed<unsigned>(e.at("id")),e.at("name").get<std::string>()).second,"duplicate registry ID");}std::string error;require(r.Validate(error),"registry: "+error);return r;}
J project(const ProjectSettings& p){J profiles=J::array(),locales=J::object(),audio=J::object();for(auto [id,a]:p.navigation.profiles)profiles.push_back({{"id",id},{"name",a.name},{"radius",a.radius},{"height",a.height},{"slope",a.slope},{"climb",a.climb}});for(auto [tag,e]:p.localization.locales)locales[tag]={{"catalog",e.catalog},{"fallback",e.fallback},{"fonts",e.fonts}};for(auto [n,g]:p.audio.groups)audio[n]={{"gain",g.gain},{"mute",g.mute},{"paused",g.paused}};
return {{"name",p.name},{"legacyGameplay",p.legacyGameplay},{"saveIdentity",p.saveIdentity},{"iconAsset",p.iconAsset},{"worldManifest",p.worldManifest},{"startupScene",p.startupScene},{"assetsDir",p.assetsDir},{"scenesDir",p.scenesDir},{"savesDir",p.savesDir},{"exportAssetPolicy",p.exportAssetPolicy},{"runtimeAssets",p.runtimeAssets},{"exportScenes",p.exportScenes},{"excludeScenes",p.excludeScenes},{"input",input(p.input)},{"classification",{{"tags",registry(p.classification.tags)},{"collision",registry(p.classification.collision)},{"render",registry(p.classification.render)}}},{"navigation",{{"areas",registry(p.navigation.areas)},{"nextProfile",p.navigation.nextProfile},{"profiles",profiles}}},{"localization",{{"defaultLocale",p.localization.defaultLocale},{"fonts",p.localization.fonts},{"locales",locales}}},{"audio",audio}};}
ProjectSettings project(const J& j){keys(j,{"name","legacyGameplay","saveIdentity","iconAsset","worldManifest","startupScene","assetsDir","scenesDir","savesDir","exportAssetPolicy","runtimeAssets","exportScenes","excludeScenes","input","classification","navigation","localization","audio"},"project");ProjectSettings p;
#define READ(K) p.K=j.at(#K).get<decltype(p.K)>();
READ(name) READ(legacyGameplay) READ(saveIdentity) READ(iconAsset) READ(worldManifest) READ(startupScene) READ(assetsDir) READ(scenesDir) READ(savesDir) READ(exportScenes) READ(excludeScenes)
#undef READ
p.exportAssetPolicy=j.value("exportAssetPolicy",std::string("all"));p.runtimeAssets=j.value("runtimeAssets",std::vector<std::string>{});
p.input=input(j.at("input"));auto& c=j.at("classification");keys(c,{"tags","collision","render"},"classification");p.classification={registry(c.at("tags")),registry(c.at("collision")),registry(c.at("render"))};auto& n=j.at("navigation");keys(n,{"areas","nextProfile","profiles"},"navigation");p.navigation.areas=registry(n.at("areas"));p.navigation.nextProfile=typed<unsigned>(n.at("nextProfile"));p.navigation.profiles.clear();for(auto& a:n.at("profiles")){keys(a,{"id","name","radius","height","slope","climb"},"navigation/profile");require(p.navigation.profiles.emplace(typed<unsigned>(a.at("id")),NavigationProfile{a.at("name").get<std::string>(),typed<float>(a.at("radius")),typed<float>(a.at("height")),typed<float>(a.at("slope")),typed<float>(a.at("climb"))}).second,"duplicate navigation profile");}auto& l=j.at("localization");keys(l,{"defaultLocale","fonts","locales"},"localization");p.localization.defaultLocale=l.at("defaultLocale").get<std::string>();p.localization.fonts=l.at("fonts").get<std::vector<std::string>>();for(auto e=l.at("locales").begin();e!=l.at("locales").end();++e){keys(e.value(),{"catalog","fallback","fonts"},"localization/locale");p.localization.locales[e.key()]={e.value().at("catalog").get<std::string>(),e.value().at("fallback").get<std::string>(),e.value().at("fonts").get<std::vector<std::string>>()};}for(auto e=j.at("audio").begin();e!=j.at("audio").end();++e){keys(e.value(),{"gain","mute","paused"},"audio/group");p.audio.groups[e.key()]={e.value().at("gain").get<float>(),e.value().at("mute").get<bool>(),e.value().at("paused").get<bool>()};}return p;}
J ui(const UIDocument& d){J a=J::array();for(auto& e:d.elements){J j;
#define PUT(K) j[#K]=e.K;
PUT(id) PUT(parent) PUT(text) PUT(texture) PUT(font) PUT(textKey) PUT(textLogicalAlign) PUT(mirrorRow) PUT(visible) PUT(enabled) PUT(clip) PUT(wrap) PUT(fit) PUT(spacing) PUT(fontSize) PUT(value) PUT(minimum) PUT(maximum)
#undef PUT
j["kind"]=int(e.kind);j["flow"]=int(e.flow);j["direction"]=int(e.direction);
#define VEC(K) j[#K]=vec(e.K);
VEC(anchorMin) VEC(anchorMax) VEC(offset) VEC(size) VEC(relativeSize) VEC(align) VEC(textAlign) VEC(margin) VEC(padding) VEC(background) VEC(color)
#undef VEC
a.push_back(j);}return {{"reference",vec(d.reference)},{"visible",d.visible},{"enabled",d.enabled},{"modal",d.modal},{"elements",a}};}
UIDocument ui(const J& j){keys(j,{"reference","visible","enabled","modal","elements"},"ui");UIDocument d;v2(j.at("reference"),d.reference);d.visible=j.at("visible").get<bool>();d.enabled=j.at("enabled").get<bool>();d.modal=j.at("modal").get<bool>();require(j.at("elements").is_array()&&j.at("elements").size()<=2048,"UI element count/type");for(auto& a:j.at("elements")){keys(a,{"id","parent","text","texture","font","textKey","textLogicalAlign","mirrorRow","visible","enabled","clip","wrap","fit","spacing","fontSize","value","minimum","maximum","kind","flow","direction","anchorMin","anchorMax","offset","size","relativeSize","align","textAlign","margin","padding","background","color"},"ui/element");UIElement e;
#define READ(K) e.K=typed<decltype(e.K)>(a.at(#K));
READ(id) READ(parent) READ(text) READ(texture) READ(font) READ(textKey) READ(textLogicalAlign) READ(mirrorRow) READ(visible) READ(enabled) READ(clip) READ(wrap) READ(fit) READ(spacing) READ(fontSize) READ(value) READ(minimum) READ(maximum)
#undef READ
e.kind=UIKind(typed<int>(a.at("kind")));e.flow=UIFlow(typed<int>(a.at("flow")));e.direction=TextDirection(typed<int>(a.at("direction")));
#define V2(K) v2(a.at(#K),e.K);
V2(anchorMin) V2(anchorMax) V2(offset) V2(size) V2(relativeSize) V2(align) V2(textAlign)
#undef V2
#define V4(K) v4(a.at(#K),e.K);
V4(margin) V4(padding) V4(background) V4(color)
#undef V4
d.elements.push_back(e);}std::string error;require(d.Validate(error),"ui: "+error);return d;}
}
bool NamedToLegacy(const std::string& text,const std::string& kind,std::string& out,std::string& error,bool validate){try{auto j=parse(text);keys(j,{"kind","schema","data"},"document");require(j.at("schema").is_number_integer()&&j.at("schema")==1,"unsupported authoring schema");auto k=j.at("kind").get<std::string>();require(k==kind||(kind=="scene"&&k=="prefab"),"document kind mismatch");auto& d=j.at("data");std::string s;
if(k=="scene"||k=="prefab"){keys(d,{"settings","objects"},"scene");s="JudasScene 3\nsettings\n"+block(d.at("settings"));require(d.at("objects").is_array()&&d.at("objects").size()<=65536,"scene object count/type");for(auto& o:d.at("objects")){keys(o,{"id","name","fields"},"scene/object");require(typed<uint64_t>(o.at("id"))>0,"object/id: positive stable integer required");s+="object "+o.at("id").dump()+" "+quote(o.at("name").get<std::string>())+"\n"+block(o.at("fields"));}}
else if(k=="project")s=Project::SerializeToString(project(d));else if(k=="ui")s=SerializeUIDocument(ui(d));else if(k=="input")s=input(d).Serialize();else if(k=="world"){keys(d,{"budget","regions","references"},"world");auto& budget=d.at("budget");keys(budget,{"installMilliseconds","unitsPerFrame","maxPreparing","pendingBytes","liveBytes","retainedBytes","resourceCacheBytes"},"world/budget");s="JudasWorld 1\nbudget ";for(auto key:{"installMilliseconds","unitsPerFrame","maxPreparing","pendingBytes","liveBytes","retainedBytes","resourceCacheBytes"}){s+=atom(budget.at(key))+" ";}s+="\n";for(auto& r:d.at("regions")){keys(r,{"id","scene","origin","rotation","halfExtents","priority","policy","estimatedBytes","dependencies"},"world/region");std::string deps;for(auto& v:r.at("dependencies")){if(!deps.empty())deps+=' ';deps+=v.get<std::string>();}s+="region "+quote(r.at("id").get<std::string>())+" "+quote(r.at("scene").get<std::string>())+" "+values(r.at("origin"))+" "+values(r.at("rotation"))+" "+values(r.at("halfExtents"))+" "+atom(r.at("priority"))+" "+quote(r.at("policy").get<std::string>())+" "+atom(r.at("estimatedBytes"))+" "+quote(deps)+"\n";}for(auto& r:d.at("references")){keys(r,{"region","local","field","target","targetLocal","mode"},"world/reference");s+="reference "+quote(r.at("region").get<std::string>())+" "+atom(r.at("local"))+" "+quote(r.at("field").get<std::string>())+" "+quote(r.at("target").get<std::string>())+" "+atom(r.at("targetLocal"))+" "+r.at("mode").get<std::string>()+"\n";}}else throw std::runtime_error("unsupported document kind "+k);
if((validate||k=="prefab")&&!ValidateAuthoredDocument(s,k,error))return false;
out=std::move(s);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
bool LegacyToNamed(const std::string& text,const std::string& kind,std::string& out,std::string& error){try{if(kind=="world"&&!ValidateAuthoredDocument(text,kind,error))return false;J d;if(kind=="scene"||kind=="prefab"){Scene normalized;require(LoadSceneFromString(text,normalized,error),error);if(kind=="prefab")require(ValidatePrefab(normalized,error),error);std::string canonical;SaveSceneToString(normalized,canonical);std::istringstream in(canonical);std::string line;std::getline(in,line);d={{"settings",J::object()},{"objects",J::array()}};while(std::getline(in,line)){std::istringstream row(line);std::string k;row>>k;if(k=="settings")d["settings"]=fieldBlock(in);if(k=="object"){auto a=tokens(row);require(a.size()==2,"object header");d["objects"].push_back({{"id",a[0]},{"name",a[1]},{"fields",fieldBlock(in)}});}}}else if(kind=="project"){ProjectSettings p;require(Project::ParseFromString(text,p,error),error);d=project(p);}else if(kind=="ui"){UIDocument u;require(ParseUIDocument(text,u,error),error);d=ui(u);}else if(kind=="input"){InputMap m;require(InputMap::Parse(text,m,error),error);d=input(m);}else if(kind=="world"){std::istringstream in(text);std::string line;d={{"budget",J::array()},{"regions",J::array()},{"references",J::array()}};while(std::getline(in,line)){std::istringstream row(line);std::string k;row>>k;auto a=tokens(row);if(k=="budget"){require(a.size()==7,"world/budget count");d["budget"]={{"installMilliseconds",a[0]},{"unitsPerFrame",a[1]},{"maxPreparing",a[2]},{"pendingBytes",a[3]},{"liveBytes",a[4]},{"retainedBytes",a[5]},{"resourceCacheBytes",a[6]}};}else if(k=="region"){require(a.size()==16,"world/region token count");std::vector<std::string> deps;std::istringstream ds(a[15].get<std::string>());std::string dep;while(ds>>dep)deps.push_back(dep);d["regions"].push_back({{"id",a[0]},{"scene",a[1]},{"origin",{a[2],a[3],a[4]}},{"rotation",{a[5],a[6],a[7],a[8]}},{"halfExtents",{a[9],a[10],a[11]}},{"priority",a[12]},{"policy",a[13]},{"estimatedBytes",a[14]},{"dependencies",deps}});}else if(k=="reference")d["references"].push_back({{"region",a[0]},{"local",a[1]},{"field",a[2]},{"target",a[3]},{"targetLocal",a[4]},{"mode",a[5].at("symbol")}});}}else throw std::runtime_error("unsupported document kind "+kind);
out=J{{"kind",kind},{"schema",1},{"data",d}}.dump(2)+"\n";std::string proof;require(NamedToLegacy(out,kind,proof,error),error);return true;}catch(const std::exception& e){error=e.what();return false;}}
bool ValidateAuthoredDocument(const std::string& t,const std::string& kind,std::string& error){if(IsNamedDocument(t)){std::string legacy;return NamedToLegacy(t,kind,legacy,error);}if(kind=="scene"||kind=="prefab"){Scene s;if(!LoadSceneFromString(t,s,error))return false;return kind=="prefab"?ValidatePrefab(s,error):true;}if(kind=="project"){ProjectSettings p;return Project::ParseFromString(t,p,error);}if(kind=="ui"){UIDocument u;return ParseUIDocument(t,u,error);}if(kind=="input"){InputMap m;return InputMap::Parse(t,m,error);}if(kind=="world"){WorldManifest w;return ParseWorldManifest(t,w,error);}error="unsupported document kind "+kind;return false;}
bool WriteAuthoredDocument(const std::string& path, const std::string& legacy,
                           const std::string& kind, std::string& error,
                           bool named, bool preserveExisting) {
    namespace fs = std::filesystem;
    std::string stage;
    auto read = [](const std::string& p) {
        std::ifstream f(p, std::ios::binary);
        std::ostringstream bytes;
        bytes << f.rdbuf();
        return bytes.str();
    };
    try {
        const bool existed = fs::exists(path);
        const auto expected = existed ? read(path) : std::string{};
        if (preserveExisting) named |= IsNamedDocument(expected);
        std::string text = legacy;
        if (named && !LegacyToNamed(legacy, kind, text, error)) return false;
        if (!ValidateAuthoredDocument(text, kind, error)) return false;
        stage = path + ".authoring-stage-" + MintAssetId();
        {
            std::ofstream f(stage, std::ios::binary);
            f.write(text.data(), std::streamsize(text.size()));
            require(bool(f), "cannot write staging file " + stage);
        }
        require(fs::exists(path) == existed && (!existed || read(path) == expected),
                "external edit detected before publication; last-good file retained");
        fs::rename(stage, path);
        return true;
    } catch (const std::exception& e) {
        std::error_code ignored;
        if (!stage.empty()) fs::remove(stage, ignored);
        error = path + ": " + e.what();
        return false;
    }
}
