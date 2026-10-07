#include "InputSystem.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "PerformanceProfiler.h"
#include "SceneSession.h"
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "SceneSerialization.h"
#include "ResourceManager.h"
#include <algorithm>
#include <filesystem>

SceneSession::SceneSession(const Project& project,const std::string& current):m_project(project),m_localization(project.Settings().localization) {
    namespace fs=std::filesystem;
    m_current=project.MakeRelative(current);
    if(!project.IsLoaded())return;
    std::error_code ec;
    const auto root=fs::weakly_canonical(project.RootDir(),ec);
    const auto add=[&](const fs::path& p){
        std::error_code detail;auto real=fs::canonical(p,detail);if(detail)return;
        auto relative=real.lexically_relative(root);
        if(relative.empty()||relative.is_absolute()||*relative.begin()==".."||real.extension()!=".judas")return;
        m_scenes.push_back(relative.generic_string());
    };
    add(project.StartupScenePath());
    if(fs::is_directory(project.ScenesDir(),ec))
        for(fs::recursive_directory_iterator it(project.ScenesDir(),ec),end;it!=end&&!ec;it.increment(ec))
            if(it->is_regular_file(ec))add(it->path());
    std::sort(m_scenes.begin(),m_scenes.end());
    m_scenes.erase(std::unique(m_scenes.begin(),m_scenes.end()),m_scenes.end());
}
SceneSession::~SceneSession()=default;
WorldStreaming* SceneSession::Streaming(RuntimeWorld& world,std::string& error){
    if(!m_accepting&&(!m_streaming||m_streamWorld!=&world)){error="Scene lifecycle ending";return nullptr;}
    if(!ComposedProject()){error="Project has no world manifest";return nullptr;}
    if(m_streamWorld!=&world){m_streaming.reset();m_streamWorld=&world;m_streamError.clear();}
    if(!m_streaming&&m_streamError.empty()){
        auto* resources=world.Resources();auto* db=resources?resources->Assets():nullptr;
        auto* asset=db?db->Find(m_project.Settings().worldManifest):nullptr;WorldManifest manifest;
        if(!resources||!resources->Jobs()){m_streamError="Streaming requires the normal asynchronous JobSystem";}
        else if(!asset||asset->type!=AssetType::World||!LoadWorldManifest(asset->path,manifest,m_streamError)||!ValidateWorldManifest(manifest,m_project,m_streamError)){if(m_streamError.empty())m_streamError="Missing world manifest asset";}
        else m_streaming=std::make_unique<WorldStreaming>(world,*resources,m_project,std::move(manifest));
    }
    error=m_streamError;return m_streaming.get();
}
void SceneSession::AdvanceStreaming(RuntimeWorld& world,bool paused){if(!ComposedProject())return;std::string error;if(auto* stream=Streaming(world,error))stream->Advance(paused);}
bool SceneSession::Request(const std::string& scene,std::string& error){
    if(!m_accepting||(m_saveService&&m_saveService->Busy())){error="Scene/save lifecycle is busy";return false;}
    if(std::find(m_scenes.begin(),m_scenes.end(),scene)==m_scenes.end()){
        error="Scene is not registered in this project: "+scene;return false;
    }
    if(m_pending.empty())m_pending=scene;
    return true;
}
bool SceneSession::Set(const std::string& key,const std::string& json,std::string& error){
    if(key.empty()||key.size()>128||!ScriptSystem::ValidateJson(json,error,false)){if(error.empty())error="Invalid session key";return false;}
    auto next=m_values;next[key]=json;size_t bytes=0;for(auto& e:next)bytes+=e.first.size()+e.second.size();
    if(next.size()>256||bytes>65536){error="Session exceeds 256 keys or 64KiB";return false;}
    m_values=std::move(next);return true;
}
std::string SceneSession::Get(const std::string& key) const {auto it=m_values.find(key);return it==m_values.end()?"null":it->second;}
bool SceneSession::Apply(std::unique_ptr<RuntimeWorld>& world,InteractivePlay& play,ResourceManager& resources,std::string& error){
    if(m_pending.empty())return true;
    JUDAS_PROFILE_SCOPE("Scene replacement");
    PerformanceProfiler::Get().Boundary("Scene replacement");
    const auto requested=std::move(m_pending);m_pending.clear();
    Scene scene;
    if(!LoadSceneFromFile(m_project.Resolve(requested),scene,error))return false;
    auto next=std::make_unique<RuntimeWorld>();
    next->audioGroups=m_project.Settings().audio;
    next->legacyGameplay=m_project.Settings().legacyGameplay;
    next->SetSceneControl(world->SceneControl());
    if(!next->Build(scene,&resources,error,&m_project.Settings().classification, &m_project.Settings().navigation))return false;
    // Validate ordinary player/session construction before ending the old scene.
    GameSession validation;
    if(!validation.Begin(*next,error))return false;
    validation.End();
    auto state=world->SceneControl();
    m_accepting=false;
    play.End(); // stop callbacks, UI and audio before releasing scene resources
    m_streaming.reset();m_streamWorld=nullptr;
    world=std::move(next);
    m_current=requested;
    m_accepting=true;
    world->SetSceneControl(std::move(state));
    if(!play.Begin(*world,WorldCoordinates(scene.Settings().worldOrigin),error))return false;
    play.SetWorldStatePath(m_saves&&!world->IsComposed()?m_project.WorldStatePathForScene(m_project.Resolve(requested)):"",false);
    // No automatic save overlay: reload means fresh authored scene. Explicit
    // save/load compatibility remains tied to this new world's baseline.
    return true;
}

SaveService* SceneSession::Saves(ResourceManager& resources){if(!m_saveService)m_saveService=std::make_unique<SaveService>(*this,resources,m_editorSaves);return m_saveService.get();}
void SceneSession::AdvanceSaves(std::unique_ptr<RuntimeWorld>& world,InteractivePlay& play,ResourceManager& resources,std::string& error){if(m_saveService)m_saveService->Advance(world,play,error);(void)resources;}

bool SceneSession::AdvanceOuter(std::unique_ptr<RuntimeWorld>& owner,InteractivePlay& play,ResourceManager& resources,InputSystem& input,std::string& error){
    AdvanceStreaming(*owner,play.IsPaused());
    auto* previous=owner.get();AdvanceSaves(owner,play,resources,error);
    if(previous!=owner.get())input.DiscardPending();
    // A save error must remain visible rather than being overwritten by Apply.
    std::string transitionError;bool ok=Apply(owner,play,resources,transitionError);
    if(previous!=owner.get())input.DiscardStickHistory();
    if(!transitionError.empty()){if(!error.empty())error+="; ";error+=transitionError;}
    return ok;
}
