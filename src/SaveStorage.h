#pragma once
#include <vector>
#include "SaveArchive.h"
#include <functional>
#include <memory>

struct SaveChunk {uint32_t version=1;std::string data;void Save(SaveArchive& a){a(version,data);}};
struct GameSnapshot {
 std::string project,scene,content,displayName,metadata="{}",diagnosticBuild="Judas M61";
 uint64_t timestamp=0;uint32_t gameVersion=1;
 std::map<std::string,SaveChunk> participants;
 void Save(SaveArchive& a){a(project,scene,content,displayName,metadata,timestamp,gameVersion,diagnosticBuild,participants);}
};
struct SaveSlotInfo {std::string id,name,scene,metadata,status,error;uint64_t timestamp=0;bool recovered=false;};
// One scoped directory; POSIX descriptor-relative operations and Windows pinned
// directory handles reject symlinks/reparse points.
// A worker can own this storage independently of the requesting scene.
class SaveStorage {
public:
 explicit SaveStorage(std::string root);~SaveStorage();
 SaveStorage(const SaveStorage&)=delete;
 static bool ValidSlot(const std::string&);
 bool Write(const std::string&,const GameSnapshot&,std::string&);
 bool Read(const std::string&,GameSnapshot&,bool& recovered,std::string&);
 bool Delete(const std::string&,std::string&);
 std::vector<SaveSlotInfo> List(const std::string& project,const std::string& content,std::string&);
 const std::string& Root()const{return m_root;}
 // Isolated tests inject a failure/terminate a CONTROLLED child at exact storage
 // boundaries. Production never changes this function.
 std::function<void(const char*)> boundary;
private:
 std::string m_root;int m_directory=-1;
#ifdef _WIN32
 std::vector<void*> m_directoryHandles; // Held without delete-sharing to prevent root replacement.
#endif
 bool Open(std::string&);bool ReadFile(const std::string&,GameSnapshot&,std::string&,bool metadataOnly=false);
 bool Publish(const std::string&,const std::string&,std::string&);
};
std::string EncodeGameSnapshot(const GameSnapshot&);
bool DecodeGameSnapshot(const std::string&,GameSnapshot&,std::string&,bool metadataOnly=false);
