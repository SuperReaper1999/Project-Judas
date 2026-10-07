#pragma once
#include "SaveStorage.h"
#include "JobSystem.h"
#include <memory>
class SceneSession;class RuntimeWorld;class ResourceManager;class InteractivePlay;
class Project;class AssetDatabase;
// Existing content identity, normalized for packaging-only settings. A package's
// exclusion report retains hashes, never loadable assets or simulation state.
bool ComputeSaveContentFingerprint(const Project&,const AssetDatabase&,
 const std::vector<std::string>& scenes,std::string& digest,std::string& error,
 const JobContext* cancel=nullptr);
struct SaveRequestStatus {uint64_t id=0;std::string operation,slot,state="queued",error;bool recovered=false;double captureMs=0,workerMs=0,restoreMs=0;size_t bytes=0;};
// Project/session owner-thread service. Worker products contain immutable data;
// they never retain world, VM, scene-session or native-resource pointers.
class SaveService {
public:
 SaveService(SceneSession&,ResourceManager&,bool editor=false);~SaveService();
 uint64_t Request(const std::string& operation,const std::string& slot,const std::string& name,const std::string& metadata,std::string& error);
 bool Cancel(uint64_t);
 const SaveRequestStatus* Status(uint64_t)const;
 const std::vector<SaveSlotInfo>& List()const{return m_slots;}
 bool Busy()const{return m_active!=0;}
 void Advance(std::unique_ptr<RuntimeWorld>&,InteractivePlay&,std::string&);
 const std::string& Root()const{return m_root;}
 const std::string& Identity()const{return m_identity;}
 const std::string& Content()const{return m_content;}
private:
 struct Product;SceneSession& m_session;ResourceManager& m_resources;
 std::string m_root,m_identity,m_content,m_identityError,m_name,m_metadata;
 std::map<uint64_t,SaveRequestStatus> m_requests;std::vector<SaveSlotInfo> m_slots;
 uint64_t m_active=0;JobHandle m_job,m_identityJob;std::shared_ptr<Product> m_product,m_identityProduct;
 std::unique_ptr<RuntimeWorld> m_staged;std::shared_ptr<SceneSession> m_stagedSession;
 std::chrono::steady_clock::time_point m_restoreStart;
 void Finish(const std::string&);
 void RefreshContent();
};
