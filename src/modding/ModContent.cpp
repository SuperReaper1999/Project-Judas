#include "ModContent.h"
#include "ContentStorage.h"
#include "SceneFingerprint.h"
#include "SaveArchive.h"
#include "PlatformServices.h"
#include "TextLayout.h"
#include "AssetDependencies.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "CollisionAsset.h"
#include "RuntimeUI.h"
#include "ModelArchive.h"
#include "AsyncFile.h"
#include <unicode/normalizer2.h>
#include <unicode/unistr.h>
#include <algorithm>
#include <charconv>
#include <fstream>
#include <filesystem>
#include <functional>
#include <sstream>
namespace JudasMods {
namespace {
using AuthoringJSON::require;using AuthoringJSON::keys;using AuthoringJSON::typed;
JSON Parse(const std::string& text){
 require(text.size()<=Limits::Metadata,"mod metadata exceeds 1 MiB");size_t events=0;
 std::vector<std::set<std::string>> stack;
 return JSON::parse(text,[&](int depth,JSON::parse_event_t e,JSON& v){
  require(depth<=16&&++events<=65536,"mod metadata depth/event bound");
  if(e==JSON::parse_event_t::object_start)stack.emplace_back();
  if(e==JSON::parse_event_t::key)require(stack.back().insert(v.get<std::string>()).second,"duplicate mod metadata key");
  if(e==JSON::parse_event_t::object_end)stack.pop_back();
  return true;
 });
}
std::string String(const JSON& j,const char* key,size_t limit=256){auto s=j.at(key).get<std::string>();std::string error;require(s.size()<=limit&&s.find('\0')==std::string::npos&&ValidTextUTF8(s,error),std::string(key)+": invalid/bounded UTF-8 string");return s;}
void Identity(const std::string& s){require(ValidIdentity(s),"invalid mod/game/export identity: "+s);}
void Version(const std::string& s){std::array<unsigned,3> v{};require(ParseVersion(s,v),"version requires major.minor.patch: "+s);}
void Hash(const std::string& s){require(s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos,"invalid SHA-256");}
void Path(const std::string& s){std::string key,error;require(PortablePath(s,key,error),"package path "+s+": "+error);}
Reference Ref(const JSON& j){keys(j,{"kind","value"},"reference");Reference r{String(j,"kind"),String(j,"value",Limits::Path)};AssetType t{};require(r.kind=="scene"||AssetKind(r.kind,t),"unsupported reference kind: "+r.kind);if(r.kind=="scene")Path(r.value);else require(IsValidAssetId(r.value),"invalid reference AssetId");return r;}
JSON Ref(const Reference& r){return {{"kind",r.kind},{"value",r.value}};}
void Array(const JSON& a,size_t limit,const char* name){require(a.is_array()&&a.size()<=limit,std::string(name)+": array/count bound");}
std::set<std::string> Kinds(const JSON& a,bool scenes=true){Array(a,18,"kinds");std::set<std::string> out;for(auto& v:a){auto s=v.get<std::string>();AssetType t{};require((scenes&&s=="scene")||AssetKind(s,t),"unknown asset kind: "+s);require(out.insert(s).second,"duplicate asset kind");}return out;}
std::string RefKey(const Reference& r){return r.kind+":"+r.value;}
void BoundedString(SaveArchive& a,std::string& value,size_t limit){uint32_t n=0;a(n);require(n<=limit&&n<=a.bytes.size()-a.position,"bounded/truncated mod field");value.assign(a.bytes.data()+a.position,n);a.position+=n;}
void CheckManifest(const Manifest& m){Manifest checked;std::string e;require(ParseManifest(ToJSON(m).dump(),checked,e),e);}
}
bool ValidIdentity(const std::string& s){return !s.empty()&&s.size()<=64&&s.front()>='a'&&s.front()<='z'&&std::all_of(s.begin(),s.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';});}
bool ParseVersion(const std::string& s,std::array<unsigned,3>& out){
 if(s.empty()||s.size()>32)return false;
 size_t begin=0;std::array<unsigned,3> v{};
 for(unsigned i=0;i<3;++i){auto end=i==2?s.size():s.find('.',begin);if(end==std::string::npos||end==begin||(end-begin>1&&s[begin]=='0'))return false;auto p=std::from_chars(s.data()+begin,s.data()+end,v[i]);if(p.ec!=std::errc()||p.ptr!=s.data()+end||v[i]>65535)return false;begin=end+1;}
 if(begin!=s.size()+1)return false;
 out=v;return true;
}
bool VersionMatches(const std::string& actual,const std::string& required){std::array<unsigned,3> a{},lo{},hi{};if(!ParseVersion(actual,a))return false;if(ParseVersion(required,lo))return a==lo;
 // Exactly one bounded range syntax. No wildcard, caret, prerelease or resolver.
 auto split=required.find(" <");return required.rfind(">=",0)==0&&split!=std::string::npos&&ParseVersion(required.substr(2,split-2),lo)&&ParseVersion(required.substr(split+2),hi)&&lo<hi&&a>=lo&&a<hi;
}
bool AssetKind(const std::string& s,AssetType& out){for(int i=0;i<=int(AssetType::PhysicalMaterial);++i)if(s==AssetTypeName(AssetType(i))){out=AssetType(i);return true;}return false;}
bool PortablePath(const std::string& s,std::string& collisionKey,std::string& error){try{
 require(!s.empty()&&s.size()<=Limits::Path&&s.front()!='/'&&s.find_first_of("\\:\0",0,3)==std::string::npos,"absolute, Windows or NUL path");
 require(ValidTextUTF8(s,error),"invalid UTF-8");auto u=icu::UnicodeString::fromUTF8(s);UErrorCode status=U_ZERO_ERROR;auto* normal=icu::Normalizer2::getNFCInstance(status);require(U_SUCCESS(status),"Unicode normalization unavailable");icu::UnicodeString n;normal->normalize(u,n,status);require(U_SUCCESS(status),"Unicode normalization failed");n.foldCase();icu::UnicodeString folded;normal->normalize(n,folded,status);require(U_SUCCESS(status),"Unicode normalization failed");std::string key;folded.toUTF8String(key);
 size_t start=0;while(start<=s.size()){auto end=s.find('/',start);if(end==std::string::npos)end=s.size();auto part=s.substr(start,end-start);require(!part.empty()&&part!="."&&part!=".."&&part.back()!='.'&&part.back()!=' ',"empty/traversal/trailing-dot-space component");require(std::none_of(part.begin(),part.end(),[](unsigned char c){return c<32||c==127||c=='<'||c=='>'||c=='"'||c=='|'||c=='?'||c=='*';}),"nonportable filename character");auto stem=part.substr(0,part.find('.'));std::transform(stem.begin(),stem.end(),stem.begin(),[](unsigned char c){return char(c>='a'&&c<='z'?c-32:c);});require(stem!="CON"&&stem!="PRN"&&stem!="AUX"&&stem!="NUL"&&stem!="CONIN$"&&stem!="CONOUT$"&&!(stem.size()==4&&(stem.rfind("COM",0)==0||stem.rfind("LPT",0)==0)&&stem[3]>='1'&&stem[3]<='9'),"Windows reserved filename");require(!((stem.rfind("COM",0)==0||stem.rfind("LPT",0)==0)&&(stem.substr(3)=="¹"||stem.substr(3)=="²"||stem.substr(3)=="³")),"Windows reserved superscript filename");if(end==s.size())break;start=end+1;}
 collisionKey=key;error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
std::string QualifiedAsset(const std::string& game,const std::string& mod,const std::string& local){Identity(game);Identity(mod);require(IsValidAssetId(local),"source-local asset identity requires 32 hex digits");return SceneFingerprintSha256("Judas.ModAsset.1:"+game+":"+mod+":"+local).substr(0,32);}
JSON ToJSON(const Contract& c){JSON p=JSON::object(),r=JSON::object(),e=JSON::object();for(auto& [n,v]:c.published)p[n]=Ref(v);for(auto& [n,v]:c.replacements)r[n]={{"target",Ref(v.target)},{"revision",v.revision},{"interface",v.interface}};for(auto& [n,v]:c.extensions)e[n]={{"kinds",v.kinds},{"maximum",v.maximum}};return {{"format",c.format},{"enabled",c.enabled},{"executable",c.executable},{"game",c.game},{"gameVersion",c.gameVersion},{"version",c.version},{"registries",c.registries},{"additions",c.additions},{"published",p},{"behaviors",c.behaviors},{"replacements",r},{"extensions",e},{"editableSources",c.editableSources}};}
bool ParseContract(const std::string& text,Contract& out,std::string& error){try{
 auto j=Parse(text);keys(j,{"format","enabled","executable","game","gameVersion","version","registries","additions","published","behaviors","replacements","extensions","editableSources"},"mod contract");Contract c;c.format=typed<unsigned>(j.at("format"));require(c.format==1,"unsupported mod contract format");c.enabled=j.at("enabled").get<bool>();c.executable=j.at("executable").get<bool>();c.game=String(j,"game");c.gameVersion=String(j,"gameVersion");c.version=String(j,"version");c.registries=String(j,"registries");Identity(c.game);Version(c.gameVersion);Version(c.version);Hash(c.registries);c.additions=Kinds(j.at("additions"));
 for(const char* k:{"published","replacements","extensions"})require(j.at(k).is_object()&&j.at(k).size()<=256,std::string(k)+" bound");
 for(auto i=j.at("published").begin();i!=j.at("published").end();++i){Identity(i.key());c.published.emplace(i.key(),Ref(i.value()));}
 require(j.at("behaviors").is_object()&&j.at("behaviors").size()<=64,"published behavior bound");for(auto i=j.at("behaviors").begin();i!=j.at("behaviors").end();++i){require(IsValidAssetId(i.key())&&i.value().is_object(),"invalid published behavior identity/schema");std::vector<ScriptProperty> properties;require(ScriptSystem::ReadProperties(i.value().dump(),"{}",properties,error),"invalid published behavior schema: "+error);require(std::any_of(c.published.begin(),c.published.end(),[&](const auto& ref){return ref.second.kind=="script"&&ref.second.value==i.key();}),"behavior must be published");c.behaviors[i.key()]=i.value();}
 std::set<std::string> targets;for(auto i=j.at("replacements").begin();i!=j.at("replacements").end();++i){Identity(i.key());keys(i.value(),{"target","revision","interface"},"replacement policy");Replacement r{Ref(i.value().at("target")),String(i.value(),"revision"),i.value().at("interface")};Hash(r.revision);require(r.interface.is_object()&&r.interface.size()<=32,"replacement interface bound");require(targets.insert(RefKey(r.target)).second,"two contract groups grant the same target");c.replacements.emplace(i.key(),r);}
 for(auto i=j.at("extensions").begin();i!=j.at("extensions").end();++i){Identity(i.key());keys(i.value(),{"kinds","maximum"},"extension policy");Extension e{Kinds(i.value().at("kinds")),typed<size_t>(i.value().at("maximum"))};require(e.maximum>0&&e.maximum<=256,"extension count bound");c.extensions.emplace(i.key(),e);}
 Array(j.at("editableSources"),64,"editable sources");c.editableSources=j.at("editableSources").get<std::vector<std::string>>();for(auto& p:c.editableSources)Path(p);
 out=std::move(c);error.clear();return true;
 }catch(const std::exception& e){error="mod contract: "+std::string(e.what());return false;}}
JSON ToJSON(const Manifest& m){JSON files=JSON::array(),assets=JSON::array(),deps=JSON::array(),imports=JSON::array(),exports=JSON::object(),cs=JSON::array(),rs=JSON::array();for(auto& f:m.files)files.push_back({{"path",f.path},{"size",f.size},{"sha256",f.hash}});for(auto& a:m.assets)assets.push_back({{"local",a.local},{"id",a.id},{"type",a.type},{"path",a.path}});for(auto& d:m.dependencies)deps.push_back({{"id",d.id},{"version",d.version}});for(auto& i:m.imports)imports.push_back({{"owner",i.owner},{"name",i.name},{"reference",Ref(i.reference)}});for(auto& [n,r]:m.exports)exports[n]=Ref(r);for(auto& c:m.contributions)cs.push_back({{"point",c.point},{"name",c.name},{"reference",Ref(c.reference)},{"data",c.data}});for(auto& r:m.replacements)rs.push_back({{"target",r.target},{"revision",r.revision},{"provider",Ref(r.provider)}});return {{"format",m.format},{"id",m.id},{"name",m.name},{"version",m.version},{"game",m.game},{"gameVersion",m.gameVersion},{"contractVersion",m.contractVersion},{"author",m.author},{"license",m.license},{"executable",m.executable},{"notices",m.notices},{"files",files},{"assets",assets},{"scenes",m.scenes},{"dependencies",deps},{"imports",imports},{"exports",exports},{"contributions",cs},{"replacements",rs}};}
bool ParseManifest(const std::string& text,Manifest& out,std::string& error,bool authorDraft){try{
 auto j=Parse(text);keys(j,{"format","id","name","version","game","gameVersion","contractVersion","author","license","executable","notices","files","assets","scenes","dependencies","imports","exports","contributions","replacements"},"mod manifest");Manifest m;m.format=typed<unsigned>(j.at("format"));require(m.format==1,"unsupported mod payload format");
 for(auto item:{std::pair<const char*,std::string*>{"id",&m.id},{"name",&m.name},{"version",&m.version},{"game",&m.game},{"gameVersion",&m.gameVersion},{"contractVersion",&m.contractVersion},{"author",&m.author},{"license",&m.license}})*item.second=String(j,item.first);
 Identity(m.id);require(m.id!="base"&&m.id!="judas","reserved mod namespace");Identity(m.game);Version(m.version);std::array<unsigned,3> v{};require(ParseVersion(m.gameVersion,v)||VersionMatches("0.0.0",m.gameVersion)||m.gameVersion.rfind(">=",0)==0,"invalid game version requirement");
 // Validate range grammar independently of whether this particular version matches.
 auto validRange=[](const std::string& s){std::array<unsigned,3>a{},b{};if(ParseVersion(s,a))return true;auto p=s.find(" <");return s.rfind(">=",0)==0&&p!=std::string::npos&&ParseVersion(s.substr(2,p-2),a)&&ParseVersion(s.substr(p+2),b)&&a<b;};require(validRange(m.gameVersion)&&validRange(m.contractVersion),"invalid compatibility range");m.executable=j.at("executable").get<bool>();require(!m.name.empty()&&!m.author.empty()&&!m.license.empty(),"name/author/license required");
 for(auto [name,cap]:{std::pair<const char*,size_t>{"files",Limits::Files},{"assets",Limits::Files},{"scenes",256},{"dependencies",Limits::Mods},{"imports",256},{"contributions",256},{"replacements",64},{"notices",64}})Array(j.at(name),cap,name);
 std::set<std::string> paths,physical;uint64_t total=0;for(auto& f:j.at("files")){keys(f,{"path","size","sha256"},"file inventory");Inventory a{String(f,"path",Limits::Path),String(f,"sha256"),typed<uint64_t>(f.at("size"))};std::string key;require(PortablePath(a.path,key,error),error);require(paths.insert(key).second,"duplicate/case/Unicode-alias package path: "+a.path);Hash(a.hash);require(a.size<=Limits::File&&total<=Limits::Package-a.size,"file/decoded package byte bound");total+=a.size;physical.insert(a.path);m.files.push_back(a);}
 std::set<std::string> locals,ids,assetPaths;for(auto& a:j.at("assets")){keys(a,{"local","id","type","path"},"asset inventory");Asset r{String(a,"local"),String(a,"id"),String(a,"type"),String(a,"path",Limits::Path)};AssetType type{};require(AssetKind(r.type,type),"unknown asset type");require(IsValidAssetId(r.local)&&r.id==QualifiedAsset(m.game,m.id,r.local),"asset identity must be owner-qualified before cooking: "+r.id);require(locals.insert(r.local).second&&ids.insert(r.id).second&&assetPaths.insert(r.path).second,"duplicate owned asset identity/path");require(physical.count(r.path),"asset absent from inventory: "+r.path);AssetType extension{};require(AssetTypeForExtension(std::filesystem::u8path(r.path).extension().u8string(),extension)&&extension==type,"asset extension/type mismatch: "+r.path);m.assets.push_back(r);}
 std::set<std::string> scenes;for(auto& p:j.at("scenes")){auto s=p.get<std::string>();Path(s);require(std::filesystem::u8path(s).extension()==".judas"&&physical.count(s)&&!assetPaths.count(s)&&scenes.insert(s).second,"invalid/duplicate scene inventory");m.scenes.push_back(s);}
 auto owned=[&](const Reference& r){if(r.kind=="scene")return scenes.count(r.value)>0;auto a=std::find_if(m.assets.begin(),m.assets.end(),[&](const Asset& a){return a.id==r.value&&a.type==r.kind;});return a!=m.assets.end();};
 std::set<std::string> dependencies;for(auto& d:j.at("dependencies")){keys(d,{"id","version"},"dependency");Dependency v{String(d,"id"),String(d,"version")};Identity(v.id);require(v.id!=m.id&&dependencies.insert(v.id).second&&validRange(v.version),"invalid/duplicate dependency: "+v.id);m.dependencies.push_back(v);}
 std::set<std::string> imports;for(auto& i:j.at("imports")){keys(i,{"owner","name","reference"},"import");Import v{String(i,"owner"),String(i,"name"),Ref(i.at("reference"))};Identity(v.name);require(v.owner=="base"||dependencies.count(v.owner),"undeclared import owner: "+v.owner);require(imports.insert(v.owner+":"+v.name).second,"duplicate declared import");m.imports.push_back(v);}
 require(j.at("exports").is_object()&&j.at("exports").size()<=256,"exports bound");for(auto i=j.at("exports").begin();i!=j.at("exports").end();++i){Identity(i.key());auto r=Ref(i.value());require(authorDraft||owned(r),"export must be owned: "+i.key());m.exports.emplace(i.key(),r);}
 std::set<std::string> contributions;for(auto& i:j.at("contributions")){keys(i,{"point","name","reference","data"},"contribution");Contribution c{String(i,"point"),String(i,"name"),Ref(i.at("reference")),i.at("data")};Identity(c.point);Identity(c.name);require((authorDraft||owned(c.reference))&&c.data.is_object()&&c.data.dump().size()<=16384&&contributions.insert(c.point+":"+c.name).second,"invalid contribution descriptor");m.contributions.push_back(c);}
 std::set<std::string> replacements;for(auto& i:j.at("replacements")){keys(i,{"target","revision","provider"},"replacement request");RequestReplacement r{String(i,"target"),String(i,"revision"),Ref(i.at("provider"))};Identity(r.target);Hash(r.revision);require((authorDraft||owned(r.provider))&&replacements.insert(r.target).second,"invalid replacement provider");m.replacements.push_back(r);}
 m.notices=j.at("notices").get<std::vector<std::string>>();for(auto& n:m.notices){Path(n);require(authorDraft||physical.count(n),"missing license notice: "+n);}
 out=std::move(m);error.clear();return true;
 }catch(const std::exception& e){error="mod manifest: "+std::string(e.what());return false;}}
bool EncodePackage(const Package& p,std::string& out,std::string& error){try{
 CheckManifest(p.manifest);require(p.files.size()==p.manifest.files.size(),"payload differs from inventory");SaveArchive a;std::string magic="JudasMod";unsigned version=1;std::string metadata=ToJSON(p.manifest).dump();uint32_t count=uint32_t(p.files.size());a(magic,version,metadata,count);
 for(auto& f:p.manifest.files){auto i=p.files.find(f.path);require(i!=p.files.end()&&i->second.size()==f.size&&SceneFingerprintSha256(i->second)==f.hash,"payload integrity mismatch: "+f.path);auto path=f.path,bytes=i->second;a(path,bytes);}out=std::move(a.bytes);error.clear();return true;
 }catch(const std::exception& e){error="mod encode: "+std::string(e.what());return false;}}
bool DecodePackage(const std::string& bytes,Package& out,std::string& error){try{
 SaveArchive a(bytes);std::string magic,metadata;unsigned version=0;uint32_t count=0;BoundedString(a,magic,16);a(version);BoundedString(a,metadata,Limits::Metadata);a(count);require(magic=="JudasMod"&&version==1,"unsupported .judasmod container");require(count<=Limits::Files,"package file count bound");Package p;require(ParseManifest(metadata,p.manifest,error),error);require(count==p.manifest.files.size(),"archive count differs from inventory");
 for(uint32_t i=0;i<count;++i){std::string path,data;BoundedString(a,path,Limits::Path);BoundedString(a,data,Limits::File);const auto& f=p.manifest.files[i];require(path==f.path&&data.size()==f.size&&SceneFingerprintSha256(data)==f.hash,"package inventory/hash mismatch: "+path);require(p.files.emplace(path,std::move(data)).second,"duplicate archive entry");}a.Finish();p.hash=SceneFingerprintSha256(bytes);out=std::move(p);error.clear();return true;
 }catch(const std::exception& e){error="mod decode: "+std::string(e.what());return false;}}
bool ReadPackage(const std::string& path,Package& out,std::string& error){std::string bytes;if(!ReadRegularContentFile(path,bytes,Limits::Package,error))return false;return DecodePackage(bytes,out,error);}

bool WritePackage(const std::string& path,const Package& p,std::string& error){std::string bytes;if(!EncodePackage(p,bytes,error))return false;try{auto output=std::filesystem::absolute(std::filesystem::u8path(path));require(output.extension()==".judasmod","mod package needs .judasmod extension");ContentStorage storage(output.parent_path().u8string());return storage.Publish(output.filename().u8string(),bytes,error);}catch(const std::exception& e){error=e.what();return false;}}
bool Resolve(const Contract& c,const std::vector<Selection>& selected,Resolution& out,std::string& error){try{
 require(c.enabled,"game has not opted into mods");require(selected.size()<=Limits::Mods,"selected mod count exceeds 16");std::map<std::string,Selection> byId;size_t total=0,edges=0;for(auto& s:selected){require(bool(s.package),"null selected package");auto& p=*s.package;CheckManifest(p.manifest);const auto& m=p.manifest;require(byId.emplace(m.id,s).second,"duplicate selected mod ID: "+m.id);require(m.game==c.game&&VersionMatches(c.gameVersion,m.gameVersion)&&VersionMatches(c.version,m.contractVersion),"incompatible game/contract: "+m.id);require(!m.executable||(c.executable&&s.trustedHash==p.hash&&!p.hash.empty()),"explicit executable trust required for payload: "+m.id);for(auto& f:m.files){require(total<=Limits::Storage-f.size,"active loadout exceeds 512 MiB");total+=size_t(f.size);}edges+=m.dependencies.size();require(edges<=Limits::Edges,"dependency edge bound");for(auto& a:m.assets)require(c.additions.count(a.type),"addition type denied: "+m.id+"/"+a.type);if(!m.scenes.empty())require(c.additions.count("scene"),"scene additions denied: "+m.id);}
 std::map<std::string,unsigned> degree;std::map<std::string,std::set<std::string>> followers;for(auto& [id,s]:byId){degree[id]=unsigned(s.package->manifest.dependencies.size());for(auto& d:s.package->manifest.dependencies){auto p=byId.find(d.id);require(p!=byId.end(),"missing dependency "+d.id+" required by "+id);require(VersionMatches(p->second.package->manifest.version,d.version),"dependency version incompatible: "+id+" -> "+d.id);followers[d.id].insert(id);}}
 std::set<std::string> ready;for(auto& [id,n]:degree)if(!n)ready.insert(id);Resolution result;while(!ready.empty()){auto id=*ready.begin();ready.erase(ready.begin());result.ordered.push_back(byId.at(id));for(auto& f:followers[id])if(!--degree[f])ready.insert(f);}require(result.ordered.size()==byId.size(),"mod dependency cycle");
 std::map<std::string,std::string> replaced;std::map<std::string,size_t> counts;JSON active=JSON::array(),cs=JSON::array(),exports=JSON::object();std::string identity="Judas.ModView.1:"+ToJSON(c).dump();
 for(auto& s:result.ordered){const auto& p=*s.package;const auto& m=p.manifest;active.push_back({{"id",m.id},{"name",m.name},{"version",m.version},{"sha256",p.hash},{"executable",m.executable}});identity+=':'+m.id+":"+m.version+":"+p.hash;
 for(auto& i:m.imports){const auto& provider=i.owner=="base"?c.published:byId.at(i.owner).package->manifest.exports;auto ref=provider.find(i.name);require(ref!=provider.end()&&RefKey(ref->second)==RefKey(i.reference),"unpublished/changed import: "+m.id+" -> "+i.owner+"/"+i.name);}
 for(auto& r:m.replacements){auto target=c.replacements.find(r.target);require(target!=c.replacements.end(),"protected/unpublished replacement: "+m.id+" -> "+r.target);require(r.revision==target->second.revision&&r.provider.kind==target->second.target.kind,"replacement revision/type mismatch: "+m.id+" -> "+r.target);auto [prior,inserted]=replaced.emplace(RefKey(target->second.target),m.id);require(inserted,"replacement conflict: "+prior->second+" and "+m.id+" -> "+r.target);}
 for(auto& contribution:m.contributions){auto point=c.extensions.find(contribution.point);require(point!=c.extensions.end()&&point->second.kinds.count(contribution.reference.kind),"unpublished/incompatible extension: "+m.id+" -> "+contribution.point);require(++counts[contribution.point]<=point->second.maximum,"extension contribution bound: "+contribution.point);auto published=contribution.reference;if(published.kind=="scene")published.value="Mods/"+m.id+"/"+published.value;cs.push_back({{"owner",m.id},{"point",contribution.point},{"name",contribution.name},{"reference",Ref(published)},{"data",contribution.data}});}
 JSON e=JSON::object();for(auto& [name,ref]:m.exports){auto published=ref;if(published.kind=="scene")published.value="Mods/"+m.id+"/"+published.value;e[name]=Ref(published);}exports[m.id]=e;
 }
 JSON baseExports=JSON::object();for(const auto& [name,ref]:c.published)baseExports[name]=Ref(ref);exports["base"]=baseExports;
 result.signature=selected.empty()?std::string():SceneFingerprintSha256(identity);result.catalog={{"active",active},{"contributions",cs},{"exports",exports}};require(result.catalog.dump().size()<=Limits::Metadata,"detached mod catalog exceeds 1 MiB");out=std::move(result);error.clear();return true;
 }catch(const std::exception& e){error="mod preflight: "+std::string(e.what());return false;}}
}
namespace JudasMods {
bool ValidateReferences(const Contract& c,const Resolution& resolved,const AssetDatabase& base,const std::map<std::string,std::string>& physical,std::string& error){try{
 using AuthoringJSON::require;
 std::map<AssetId,std::string> owners;std::map<AssetId,AssetType> types;for(auto& [id,a]:base.Records())types[id]=a.type;
 for(auto& s:resolved.ordered)for(auto& a:s.package->manifest.assets){AssetType t{};require(AssetKind(a.type,t),"invalid asset type");require(!types.count(a.id)&&owners.emplace(a.id,s.package->manifest.id).second,"real runtime AssetId collision: "+a.id);types.emplace(a.id,t);}
 auto physicalPath=[&](const std::string& owner,const std::string& path){auto i=physical.find(owner+":"+path);require(i!=physical.end(),"missing prepared content: "+owner+"/"+path);return i->second;};
 for(auto& selection:resolved.ordered){const auto& m=selection.package->manifest;
  std::set<AssetId> allowed;for(auto& a:m.assets)allowed.insert(a.id);for(auto& i:m.imports)if(i.reference.kind!="scene")allowed.insert(i.reference.value);
  auto references=[&](const std::set<AssetId>& refs){for(auto& id:refs)require(allowed.count(id)&&types.count(id),"undeclared/private/missing asset reference: "+m.id+" -> "+id);};
  auto scriptPolicy=[&](const Scene& scene){for(auto& o:scene.Objects())for(auto& script:o.scripts){auto own=owners.find(script.asset);if(own!=owners.end())require(m.executable,"hidden executable script in data-only scene/prefab: "+m.id);else {
    // Base behaviour permissions are an explicit published interface, not merely
    // a published script file. Validate ordinary property JSON without evaluating JS.
    if(!m.executable){auto behavior=c.behaviors.find(script.asset);require(behavior!=c.behaviors.end(),"data content may only instantiate explicitly published base behaviours: "+m.id);std::vector<ScriptProperty> properties;require(ScriptSystem::ReadProperties(behavior->second.dump(),script.properties,properties,error),"published behavior properties: "+m.id+": "+error);}
   }} };
  std::set<std::string> regular;for(auto& n:m.notices)regular.insert(n);
  for(auto& a:m.assets){regular.insert(a.path);AssetType type{};AssetKind(a.type,type);require(type!=AssetType::World&&type!=AssetType::Liquid&&type!=AssetType::Deformable&&type!=AssetType::Environment&&type!=AssetType::AudioEffect&&type!=AssetType::Catalog,"mandatory mod type not supported in v1: "+m.id+"/"+a.type);
   if(type==AssetType::Script)require(m.executable,"script payload requires executable declaration: "+m.id);
   if(type==AssetType::Mesh)require(std::filesystem::u8path(a.path).extension()==".judasmodel","mod meshes must use normal cooked .judasmodel assets");
   AssetRecord record{a.id,type,physicalPath(m.id,a.path),"Mods/"+m.id+"/"+a.path,"",false};std::set<AssetId> refs;std::set<std::string> extra;require(CollectAssetDependencies(record,refs,extra,error),m.id+"/"+a.path+": "+error);require(extra.empty(),"external model files must be cooked before packaging");references(refs);
   if(type==AssetType::Prefab){Scene scene;require(LoadSceneFromFile(record.path,scene,error),error);scriptPolicy(scene);}
  }
  for(auto& path:m.scenes){regular.insert(path);Scene scene;require(LoadSceneFromFile(physicalPath(m.id,path),scene,error),m.id+"/"+path+": "+error);std::set<AssetId> refs;CollectSceneAssetReferences(scene,refs);references(refs);scriptPolicy(scene);}
  for(auto& replacement:m.replacements){const auto& shape=c.replacements.at(replacement.target).interface;keys(shape,{"requiredComponents","scriptProperties"},"replacement structural interface");if(shape.empty())continue;
   require(replacement.provider.kind=="scene"||replacement.provider.kind=="prefab","structural interface requires scene/prefab provider");auto path=replacement.provider.value;if(replacement.provider.kind=="prefab"){auto asset=std::find_if(m.assets.begin(),m.assets.end(),[&](const Asset& a){return a.id==path;});require(asset!=m.assets.end(),"missing replacement prefab");path=asset->path;}Scene scene;require(LoadSceneFromFile(physicalPath(m.id,path),scene,error),error);
   if(shape.contains("requiredComponents")){Array(shape.at("requiredComponents"),8,"required components");for(auto& value:shape.at("requiredComponents")){auto kind=value.get<std::string>();require(kind=="render"||kind=="body"||kind=="navigationSurface"||kind=="character"||kind=="ui"||kind=="audioEmitter","unsupported required component");bool found=false;for(auto& o:scene.Objects()){found|=(kind=="render"&&bool(o.render))||(kind=="body"&&bool(o.body))||(kind=="navigationSurface"&&bool(o.navigationSurface))||(kind=="character"&&bool(o.characterMotor))||(kind=="ui"&&bool(o.ui))||(kind=="audioEmitter"&&bool(o.audioEmitter));}require(found,"replacement missing required component: "+kind);}}
   if(shape.contains("scriptProperties")){auto& schemas=shape.at("scriptProperties");require(schemas.is_object()&&schemas.size()<=32,"replacement script schema bound");for(auto schema=schemas.begin();schema!=schemas.end();++schema){bool found=false;for(auto& o:scene.Objects())for(auto& slot:o.scripts)if(slot.asset==schema.key()){std::vector<ScriptProperty> values;require(ScriptSystem::ReadProperties(schema.value().dump(),slot.properties,values,error),"replacement script interface: "+error);found=true;}require(found,"replacement missing required script interface: "+schema.key());}}
  }
  for(auto& f:m.files)require(regular.count(f.path),"unregistered/hidden payload file: "+m.id+"/"+f.path);
  for(auto& path:m.notices){auto extension=std::filesystem::u8path(path).extension();require(extension==".txt"||extension==".md","license notices must be plain text");}
 }
 error.clear();return true;
 }catch(const std::exception& e){error="mod references: "+std::string(e.what());return false;}}
}
