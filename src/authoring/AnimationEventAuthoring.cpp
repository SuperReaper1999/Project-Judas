#include "AnimationEventAuthoring.h"
#include "PlatformServices.h"
#include "../../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace fs=std::filesystem;using Json=nlohmann::json;
namespace {
std::string Read(const fs::path& p){if(!fs::is_regular_file(p)||fs::file_size(p)>4*1024*1024)throw std::runtime_error("Recipe is missing or exceeds 4 MiB");std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
Json Parse(const std::string& bytes){auto j=Json::parse(bytes,[](int depth,Json::parse_event_t,const Json&){if(depth>=48)throw std::runtime_error("Recipe nesting bound");return true;});if(j.at("format")!="JudasImport"||j.at("version")!=1)throw std::runtime_error("Expected current JudasImport recipe");return j;}
}
bool AnimationEventDraft::Load(const std::string& root,const std::string& recipe,const AnimationClip& selected,std::string& error){try{
 auto base=fs::weakly_canonical(root),p=fs::weakly_canonical(base/fs::u8path(recipe)),relative=p.lexically_relative(base);
 if(relative.empty()||relative.is_absolute()||*relative.begin()==".."||p.extension()!=".judasimport")throw std::runtime_error("Choose a .judasimport inside the current project");
 AnimationEventDraft candidate;candidate.path=p.string();candidate.baseline=Read(p);candidate.clip=selected.name;candidate.duration=selected.duration;candidate.events=selected.events;
 auto j=Parse(candidate.baseline);bool found=false;
 if(j.contains("clips"))for(const auto& entry:j["clips"])if(entry.value("name",entry.at("sourceClip").get<std::string>())==selected.name){found=true;if(entry.contains("events")){candidate.events.clear();if(!entry["events"].is_array()||entry["events"].size()>1024)throw std::runtime_error("Invalid event array");for(const auto& e:entry["events"])candidate.events.push_back({e.at("id"),e.at("name"),e.value("data",std::string()),e.at("time")});}}
 if(!found)throw std::runtime_error("Populate clip selections in Model import first, then reimport");
 AnimationClip validation=selected;validation.events=candidate.events;if(!ValidAnimationEvents(validation,error))return false;
 candidate.savedEvents=candidate.events;
 *this=std::move(candidate);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool AnimationEventDraft::Save(std::string& error){std::string staged;try{
 AnimationClip validation;validation.duration=duration;validation.events=events;if(!ValidAnimationEvents(validation,error))return false;
 if(Read(path)!=baseline)throw std::runtime_error("Recipe changed on disk; reload the draft before saving");
 auto j=Parse(baseline);bool found=false;for(auto& entry:j.at("clips"))if(entry.value("name",entry.at("sourceClip").get<std::string>())==clip){if(found)throw std::runtime_error("Ambiguous output clip name");found=true;entry["events"]=Json::array();for(const auto& e:events)entry["events"].push_back({{"id",e.id},{"name",e.name},{"time",e.time},{"data",e.data}});}
 if(!found)throw std::runtime_error("Clip selection disappeared");
 auto bytes=j.dump(2)+'\n';if(bytes.size()>4*1024*1024)throw std::runtime_error("Recipe exceeds 4 MiB");
 staged=CreateImportStagingFile(path);{std::ofstream f(staged,std::ios::binary);f.write(bytes.data(),bytes.size());if(!f)throw std::runtime_error("Cannot write event draft");}
 // Recheck immediately before publication; atomic rename retains last good bytes.
 if(Read(path)!=baseline)throw std::runtime_error("Recipe changed during save");
 ReplaceStagedFile(staged,path);baseline=bytes;savedEvents=events;error.clear();return true;
 }catch(const std::exception& e){if(!staged.empty()){std::error_code ec;fs::remove(staged,ec);}error=e.what();return false;}}

bool AnimationEventDraft::Dirty()const{
 if(events.size()!=savedEvents.size())return true;
 for(size_t i=0;i<events.size();++i){const auto& a=events[i];const auto& b=savedEvents[i];if(a.id!=b.id||a.name!=b.name||a.data!=b.data||a.time!=b.time)return true;}
 return false;
}
