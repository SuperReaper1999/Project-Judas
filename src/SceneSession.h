#pragma once
#include "Project.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
class RuntimeWorld;
class WorldStreaming;
class ResourceManager;
class InteractivePlay;
class SaveService;
class InputSystem;
// Project/session lifetime, independent of scene-owned JS heaps and entities.
class SceneSession {
public:
    SceneSession(const Project& project, const std::string& current);
    ~SceneSession();
    WorldStreaming* Streaming(RuntimeWorld&,std::string& error);
    void AdvanceStreaming(RuntimeWorld&,bool paused);
    bool networkActive=false; // transient world-owned session gate; never persisted
    const Project& ProjectConfig()const{return m_project;}
    bool Accepting()const{return m_accepting;}
    WorldStreaming* StreamingIfLoaded()const{return m_streaming.get();}
    bool ComposedProject()const{return !m_project.Settings().worldManifest.empty();}
    bool Request(const std::string& scene, std::string& error);
    bool Reload(std::string& error) { return Request(m_current,error); }
    LocalizationSession& Localization(ResourceManager* r){m_localization.Bind(r);return m_localization;}
    const std::string& Current() const { return m_current; }
    const std::vector<std::string>& Scenes() const { return m_scenes; }
    bool Set(const std::string& key,const std::string& json,std::string& error);
    std::string Get(const std::string& key) const;
    void Erase(const std::string& key) { m_values.erase(key); }
    // Called only after Frame/callbacks have returned. Invalid candidates never
    // replace the live world. First accepted request wins until this boundary.
    bool Apply(std::unique_ptr<RuntimeWorld>& world,InteractivePlay& play,
               ResourceManager& resources,std::string& error);
    SaveService* Saves(ResourceManager&);
    void AdvanceSaves(std::unique_ptr<RuntimeWorld>&,InteractivePlay&,ResourceManager&,std::string&);
    // Shared shipping/harness outer boundary, after all frame callbacks return.
    bool AdvanceOuter(std::unique_ptr<RuntimeWorld>&,InteractivePlay&,ResourceManager&,InputSystem&,std::string&);
    void SetEditorSaveIsolation(bool editor){m_editorSaves=editor;}
    bool Pending() const { return !m_pending.empty(); }
    bool OperationInProgress() const; // includes queued scene replacement and active save work
    void EnableSaves(bool enabled) { m_saves=enabled; }
private:
    friend class SaveService;
    std::unique_ptr<SaveService> m_saveService;
    bool m_editorSaves=false;
    std::unique_ptr<WorldStreaming> m_streaming;
    RuntimeWorld* m_streamWorld=nullptr;
    std::string m_streamError;
    Project m_project;
    LocalizationSession m_localization;
    std::string m_current,m_pending;
    std::vector<std::string> m_scenes;
    std::map<std::string,std::string> m_values;
    bool m_saves=true,m_accepting=true;
};
