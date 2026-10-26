#include "SaveEvolution.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "NamedAuthoring.h"
#include "ScriptSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
using J=nlohmann::json;
namespace {
void need(bool b,const std::string& e){if(!b)throw std::runtime_error(e);}
void keys(const J& j,std::initializer_list<const char*> allowed){need(j.is_object(),"object required");for(auto it=j.begin();it!=j.end();++it)need(std::any_of(allowed.begin(),allowed.end(),[&](auto k){return it.key()==k;}),"unknown migration field: "+it.key());}
uint32_t version(const J& j){need(j.is_number_unsigned()||(j.is_number_integer()&&j.get<int64_t>()>=0),"unsigned version required");auto n=j.get<uint64_t>();need(n<=1000000,"version bound");return uint32_t(n);}
bool hash(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
J parse(const std::string& text){need(text.size()<=1024*1024,"migration policy exceeds 1 MiB");return J::parse(text,[](int depth,J::parse_event_t,const J&){need(depth<=48,"migration nesting bound");return true;});}
J policy(const std::string& text){auto p=parse(text);keys(p,{"format","compatibility","version","sources","steps","modTransitions"});need(p.at("format")==1,"unsupported migration policy format");auto id=p.at("compatibility").get<std::string>();need(!id.empty()&&id.size()<=128,"compatibility identity requires 1..128 bytes");need(version(p.at("version"))>0,"current version must be positive");auto& sources=p.at("sources");need(sources.is_array()&&sources.size()<=128,"source allowlist bound");std::set<std::string> hashes;for(auto& s:sources){keys(s,{"content","compatibility","version"});auto h=s.at("content").get<std::string>();need(hash(h)&&hashes.insert(h).second,"invalid/duplicate source fingerprint");need(s.at("compatibility").is_string(),"source compatibility required");need(version(s.at("version"))<version(p.at("version")),"source must precede current version");}
 auto& steps=p.at("steps");need(steps.is_array()&&steps.size()<=64,"step bound");std::set<uint32_t> froms;for(auto& s:steps){keys(s,{"from","to","operations"});auto from=version(s.at("from")),to=version(s.at("to"));need(to==from+1&&to<=version(p.at("version"))&&froms.insert(from).second,"steps must be unique consecutive forward edges");need(s.at("operations").is_array()&&s.at("operations").size()<=128,"operation bound");}
 if(p.contains("modTransitions")){need(p["modTransitions"].is_array()&&p["modTransitions"].size()<=32,"mod transition bound");for(auto& m:p["modTransitions"]){keys(m,{"from","to"});need((m.at("from").is_object()||m.at("from").is_null())&&(m.at("to").is_object()||m.at("to").is_null()),"exact mod contexts required");}}
 return p;
}
J mods(const std::string& s){return s.empty()?J(nullptr):parse(s);}
std::string modDifference(const std::string& old,const std::string& now){
 std::string message="mod loadout/contract mismatch; saved requirements: "+(old.empty()?"unmodded":old)+"; current: "+(now.empty()?"unmodded":now);return message;
}
void patch(J& value,const J& ops,bool projectJson=true){need(ops.is_array()&&ops.size()<=128,"JSON patch bound");for(auto& op:ops){keys(op,{"op","path","from","value"});auto path=op.at("path").get<std::string>();need(path.size()<=1024&&std::count(path.begin(),path.end(),'/')<=32,"JSON pointer bound");if(op.contains("from")){auto from=op.at("from").get<std::string>();need(from.size()<=1024&&std::count(from.begin(),from.end(),'/')<=32,"JSON source pointer bound");}auto kind=op.at("op").get<std::string>();need(kind=="test"||kind=="add"||kind=="replace"||kind=="remove"||kind=="move"||kind=="copy","unsupported JSON patch operation");}value=value.patch(ops);if(projectJson){std::string e;need(ScriptSystem::ValidateJson(value.dump(),e,false),"patched JSON: "+e);}else parse(value.dump());}
SaveChunk& chunk(GameSnapshot& s,const std::string& name){auto i=s.participants.find(name);need(i!=s.participants.end()&&(i->second.version==1||(name=="entities"&&i->second.version==2)),"required supported participant: "+name);return i->second;}
void operation(GameSnapshot& s,const J& op){
 auto kind=op.at("kind").get<std::string>();
 if(kind=="script"){
  keys(op,{"kind","entity","asset","slot","patch"});need(op.contains("entity")!=op.contains("asset"),"script selector requires exactly one of entity or asset");auto slot=op.at("slot").get<uint64_t>();std::set<uint64_t> selected;
  if(op.contains("entity"))selected.insert(op.at("entity").get<uint64_t>());else{
   auto& entities=chunk(s,"entities");SaveArchive sceneData(entities.data);std::string text,error;sceneData(text);Scene scene;need(LoadSceneFromString(text,scene,error),error);
   const auto asset=op.at("asset").get<std::string>();for(const auto& object:scene.Objects())for(const auto& script:object.scripts)if(script.enabled&&script.asset==asset&&script.id==slot)selected.insert(object.id);
  }
  need(!selected.empty(),"script selector matched no active snapshot instance");auto& c=chunk(s,"scripts");SaveArchive in(c.data);uint32_t n=0;in(n);need(n<=4096,"script count bound");SaveArchive out;out(n);std::set<uint64_t> found;
  for(uint32_t i=0;i<n;++i){uint64_t e=0,k=0;std::string json;in(e,k,json);if(selected.count(e)&&k==slot){need(found.insert(e).second,"duplicate script identity");auto value=parse(json);patch(value,op.at("patch"));json=value.dump();}out(e,k,json);}in.Finish();need(found==selected,"migration script identity/state missing");c.data=std::move(out.bytes);
 }else if(kind=="session"){
  keys(op,{"kind","key","patch"});auto& c=chunk(s,"session");SaveArchive in(c.data);std::map<std::string,std::string> values;std::string locale;in(values,locale);in.Finish();auto key=op.at("key").get<std::string>();auto it=values.find(key);need(it!=values.end(),"migration session key not found");auto value=parse(it->second);patch(value,op.at("patch"));it->second=value.dump();SaveArchive out;out(values,locale);c.data=std::move(out.bytes);
 }else if(kind=="scenePath"){
  keys(op,{"kind","from","to"});need(s.scene==op.at("from").get<std::string>(),"scene path precondition failed");s.scene=op.at("to").get<std::string>();need(!s.scene.empty()&&s.scene.size()<=1024,"scene path bound");
 }else if(kind=="scene"||kind=="entity"||kind=="prefab"){
  if(kind=="scene")keys(op,{"kind","patch"});else if(kind=="entity")keys(op,{"kind","entity","patch"});else keys(op,{"kind","asset","patch"});auto& c=chunk(s,"entities");SaveArchive in(c.data);std::string text,error,named;in(text);auto nativeTail=in.bytes.substr(in.position);Scene before;need(LoadSceneFromString(text,before,error),error);need(LegacyToNamed(text,"scene",named,error),error);auto j=parse(named);
  if(kind=="scene")patch(j,op.at("patch"),false);else{
   size_t matched=0;for(size_t i=0;i<before.Objects().size();++i){const auto& object=before.Objects()[i];const bool match=kind=="entity"?object.id==op.at("entity").get<uint64_t>():object.prefabAsset==op.at("asset").get<std::string>();
    if(match){patch(j["data"]["objects"][i],op.at("patch"),false);++matched;}
   }need(matched>0,"migration entity/prefab selector matched no active snapshot object");
  }
  Scene after;const auto document=j.dump();parse(document);need(LoadSceneFromString(document,after,error),error);
  // Preserve geometry, IDs, topology and every physical/continuation setting.
  // Only names, colour/visibility/texture, script bindings/properties and prefab
  // provenance may evolve here. A full physical conversion needs a real codec.
  need(before.Objects().size()==after.Objects().size(),"scene migration cannot add/remove physical identities");
  Scene normalized=after;
  for(size_t i=0;i<before.Objects().size();++i){auto& a=before.Objects()[i];auto& b=normalized.Objects()[i];need(a.id==b.id,"scene identity/order migration unsupported");b.name=a.name;b.scripts=a.scripts;b.prefabAsset=a.prefabAsset;b.prefabOverrides=a.prefabOverrides;
   if(a.render&&b.render){b.render->color=a.render->color;b.render->visible=a.render->visible;b.render->textureAsset=a.render->textureAsset;}
  }
  std::string a,b;SaveSceneToString(before,a);SaveSceneToString(normalized,b);need(a==b,"migration changed physical geometry/topology or unsupported component state");SaveSceneToString(after,text);SaveArchive out;out(text);out.bytes+=nativeTail;c.data=std::move(out.bytes);
 }else throw std::runtime_error("unknown migration operation: "+kind);
}
}
bool ValidateSaveEvolution(const std::string& p,std::string& e){try{if(!p.empty())policy(p);e.clear();return true;}catch(const std::exception& x){e=x.what();return false;}}
bool SaveEvolutionCurrent(const std::string& p,SaveEvolutionIdentity& id,std::string& e){try{id={};if(!p.empty()){auto j=policy(p);id={j.at("compatibility").get<std::string>(),version(j.at("version"))};}e.clear();return true;}catch(const std::exception& x){e=x.what();return false;}}
std::string DescribeSaveCompatibility(const GameSnapshot& saved,
 const std::string& content,const SaveEvolutionIdentity& current,const std::string& currentMods) {
    std::string result;
    auto add = [&](const std::string& reason) {
        if (!result.empty()) result += "\n";
        result += reason;
    };
    if (saved.content != content)
        add("Content fingerprint changed: saved " + saved.content + "; installed " + content + ".");
    if (saved.compatibility != current.compatibility)
        add("Compatibility identity changed: saved '" + saved.compatibility + "'; installed '" + current.compatibility + "'.");
    if (saved.contentVersion != current.version)
        add("Content version changed: saved " + std::to_string(saved.contentVersion) + "; installed " + std::to_string(current.version) + ".");
    try {
        if (mods(saved.modContext) != mods(currentMods)) add(modDifference(saved.modContext, currentMods));
    } catch (const std::exception& exception) {
        add("Cannot compare saved/installed mod metadata: " + std::string(exception.what()));
    }
    if (!result.empty())
        result += "\nLoad refused before world replacement. Keep the original save and a matching game build, or use an explicit project-approved migration. A scene save is not an expedition migration.";
    return result;
}
bool EvolveSave(const std::string& text,const std::string& content,const std::string& currentMods,const GameSnapshot& input,bool allow,GameSnapshot& output,std::string& report,std::string& error){
 try{
  SaveEvolutionIdentity current;need(SaveEvolutionCurrent(text,current,error),error);need(input.gameVersion==1,"unsupported engine snapshot schema (not project data)");
  const bool sameMods=mods(input.modContext)==mods(currentMods);
  const bool sameIdentity=input.compatibility==current.compatibility&&input.contentVersion==current.version;
  if(input.content==content&&sameIdentity&&sameMods){output=input;report="exact";error.clear();return true;}
  need(allow,DescribeSaveCompatibility(input,content,current,currentMods));need(!text.empty(),"project has no save evolution policy");auto p=policy(text);
  need(std::any_of(p["sources"].begin(),p["sources"].end(),[&](const auto& s){return s["content"]==input.content&&s["compatibility"]==input.compatibility&&version(s["version"])==input.contentVersion;}),"source content/compatibility/version not explicitly approved");
  if(!sameMods){bool approved=false;if(p.contains("modTransitions"))for(auto& m:p["modTransitions"])approved|=m["from"]==mods(input.modContext)&&m["to"]==mods(currentMods);need(approved,modDifference(input.modContext,currentMods));}
  auto candidate=input;auto v=input.contentVersion;J applied=J::array();unsigned count=0;
  while(v<current.version){auto it=std::find_if(p["steps"].begin(),p["steps"].end(),[&](const auto& step){return version(step["from"])==v;});need(it!=p["steps"].end(),"missing ordered migration edge from "+std::to_string(v));for(auto& op:(*it)["operations"]){need(++count<=512,"total migration operation bound");operation(candidate,op);}applied.push_back({{"from",v},{"to",v+1}});++v;}
  need(!applied.empty(),"no forward migration path");candidate.content=content;candidate.compatibility=current.compatibility;candidate.contentVersion=current.version;candidate.modContext=currentMods;
  // Exercise the same archive bounds and checksum codec before any publication.
  EncodeGameSnapshot(candidate);report=J({{"steps",applied},{"operations",count},{"sourceContent",input.content},{"targetContent",content}}).dump();output=std::move(candidate);error.clear();return true;
 }catch(const std::exception& x){error="save evolution: "+std::string(x.what());return false;}
}
