#ifdef _WIN32
#include "SaveStorage.h"
#include "PerformanceProfiler.h"
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <set>
#include <stdexcept>
namespace fs = std::filesystem;
namespace {
struct Handle {
 HANDLE value;
 explicit Handle(HANDLE h):value(h){}
 ~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
 Handle(const Handle&)=delete;
};
void Fail(const char* what){throw std::runtime_error(std::string(what)+": Windows error "+std::to_string(GetLastError()));}
void CheckFile(HANDLE handle){
 BY_HANDLE_FILE_INFORMATION info{};
 if(!GetFileInformationByHandle(handle,&info))Fail("inspect save file");
 if(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("refusing nonregular/reparse save file");
}
void CheckDestination(const fs::path& path){
 Handle file(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
 if(file.value==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_NOT_FOUND)return;Fail("inspect slot");}
 CheckFile(file.value);
}
struct Lock {
 Handle file;
 explicit Lock(const fs::path& root):file(CreateFileW((root/L".lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr)) {
  if(file.value==INVALID_HANDLE_VALUE)Fail("save storage busy or lock unavailable");
  CheckFile(file.value);
 }
};
std::set<std::string> SlotNames(const fs::path& root){
 std::set<std::string> names;unsigned count=0;
 for(const auto& entry:fs::directory_iterator(root)){
  if(++count>4096)throw std::runtime_error("save directory entry bound exceeded");
  const auto path=entry.path();auto ext=path.extension().string();auto id=path.stem().string();
  if((ext==".save"||ext==".previous")&&SaveStorage::ValidSlot(id))names.insert(id);
  if(names.size()>128)throw std::runtime_error("save slot listing exceeds 128");
 }
 return names;
}
}
SaveStorage::SaveStorage(std::string root):m_root(std::move(root)){}
SaveStorage::~SaveStorage(){for(auto h:m_directoryHandles)CloseHandle(static_cast<HANDLE>(h));}
bool SaveStorage::ValidSlot(const std::string& s){return !s.empty()&&s.size()<=64&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
bool SaveStorage::Open(std::string& error){
 if(!m_directoryHandles.empty())return true;
 try {
  const auto root=fs::absolute(fs::u8path(m_root)).lexically_normal();
  if(root.root_name().empty()||root.root_directory().empty())throw std::runtime_error("invalid Windows save root");
  fs::path path=root.root_path();
  // Pin each ancestor before creating/opening its child. Reject junctions as well as symlinks.
  auto pin=[&](const fs::path& directory){
   Handle h(CreateFileW(directory.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
   if(h.value==INVALID_HANDLE_VALUE)Fail("open save directory");
   BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(h.value,&info))Fail("inspect save directory");
   if(!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("save root contains non-directory/reparse component");
   m_directoryHandles.push_back(h.value);h.value=INVALID_HANDLE_VALUE;
  };
  pin(path);
  for(const auto& part:root.relative_path()){
   if(part==L".."||part.empty())throw std::runtime_error("invalid save root component");
   path/=part;
   if(!CreateDirectoryW(path.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)Fail("create save directory");
   pin(path);
  }
  m_root=root.u8string();return true;
 }catch(const std::exception& e){for(auto h:m_directoryHandles)CloseHandle(static_cast<HANDLE>(h));m_directoryHandles.clear();error=e.what();return false;}
}
bool SaveStorage::ReadFile(const std::string& file,GameSnapshot& value,std::string& error,bool metadataOnly){
 JUDAS_PROFILE_SCOPE("Save read");
 try {
  Handle handle(CreateFileW((fs::u8path(m_root)/fs::u8path(file)).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
  if(handle.value==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_NOT_FOUND){error="missing slot";return false;}Fail("read slot");}
  CheckFile(handle.value);LARGE_INTEGER size{};if(!GetFileSizeEx(handle.value,&size))Fail("stat slot");
  if(size.QuadPart<0||uint64_t(size.QuadPart)>SaveArchive::Limit)throw std::runtime_error("invalid/oversized save file");
  std::string bytes(size_t(size.QuadPart),'\0');size_t at=0;
  while(at<bytes.size()){DWORD n=0;DWORD count=DWORD(std::min<size_t>(bytes.size()-at,1<<20));if(!::ReadFile(handle.value,bytes.data()+at,count,&n,nullptr)||!n)throw std::runtime_error("truncated/failed slot read");at+=n;}
  return DecodeGameSnapshot(bytes,value,error,metadataOnly);
 }catch(const std::exception& e){error=e.what();return false;}
}
bool SaveStorage::Publish(const std::string& file,const std::string& bytes,std::string& error){
 JUDAS_PROFILE_SCOPE("Save write and synchronize");static std::atomic<uint64_t> next{1};
 fs::path tmp=fs::u8path(m_root)/(".tmp-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(next.fetch_add(1)));bool published=false, ownsTemporary=false;
 try {
  Handle handle(CreateFileW(tmp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
  if(handle.value==INVALID_HANDLE_VALUE)Fail("create temporary");
  ownsTemporary=true;
  if(boundary)boundary("temporary");size_t at=0;
  while(at<bytes.size()){DWORD n=0;DWORD count=DWORD(std::min<size_t>(bytes.size()-at,1<<20));if(!::WriteFile(handle.value,bytes.data()+at,count,&n,nullptr)||!n)Fail("write save");at+=n;}
  if(!FlushFileBuffers(handle.value))Fail("sync save");if(boundary)boundary("file-synced");
  CloseHandle(handle.value);handle.value=INVALID_HANDLE_VALUE;
  auto destination=fs::u8path(m_root)/fs::u8path(file);CheckDestination(destination);
  if(!MoveFileExW(tmp.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))Fail("publish save");
  published=true;if(boundary)boundary("published");
  // MOVEFILE_WRITE_THROUGH is Windows' publication barrier, not POSIX directory fsync.
  if(boundary)boundary("directory-synced");return true;
 }catch(const std::exception& e){error=published?"published; durability uncertain: "+std::string(e.what()):e.what();if(!published&&ownsTemporary)DeleteFileW(tmp.c_str());return false;}
}
bool SaveStorage::Write(const std::string& id,const GameSnapshot& value,std::string& error){
 error.clear();if(!ValidSlot(id)){error="invalid slot ID";return false;}if(!Open(error))return false;
 try {
  const auto root=fs::u8path(m_root);Lock lock(root);CheckDestination(root/(id+".save"));CheckDestination(root/(id+".previous"));
  const auto names=SlotNames(root);if(!names.count(id)&&names.size()>=128)throw std::runtime_error("save slot limit (128); delete an existing slot first");
  auto bytes=EncodeGameSnapshot(value);GameSnapshot previous;std::string oldError;
  bool good=ReadFile(id+".save",previous,oldError);
  if(!good&&(oldError.rfind("read slot:",0)==0||oldError.rfind("stat slot:",0)==0||oldError=="truncated/failed slot read")){error=oldError;return false;}
  if(good&&!Publish(id+".previous",EncodeGameSnapshot(previous),error))return false;
  if(boundary)boundary("previous-committed");return Publish(id+".save",bytes,error);
 }catch(const std::exception& e){error=e.what();return false;}
}
bool SaveStorage::Read(const std::string& id,GameSnapshot& value,bool& recovered,std::string& error){
 error.clear();recovered=false;if(!ValidSlot(id)){error="invalid slot ID";return false;}if(!Open(error))return false;
 try {Lock lock(fs::u8path(m_root));if(ReadFile(id+".save",value,error))return true;auto original=error;
  if(ReadFile(id+".previous",value,error)){recovered=true;error.clear();return true;}error=original;return false;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool SaveStorage::Delete(const std::string& id,std::string& error){
 error.clear();if(!ValidSlot(id)){error="invalid slot ID";return false;}if(!Open(error))return false;
 try {auto root=fs::u8path(m_root);Lock lock(root);for(auto ext:{".save",".previous"})CheckDestination(root/(id+ext));
  for(auto ext:{".save",".previous"})if(!DeleteFileW((root/(id+ext)).c_str())&&GetLastError()!=ERROR_FILE_NOT_FOUND)Fail("delete slot");return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
std::vector<SaveSlotInfo> SaveStorage::List(const std::string& project,const std::string& content,std::string& error){
 error.clear();std::vector<SaveSlotInfo> result;if(!Open(error))return result;
 try {auto root=fs::u8path(m_root);Lock lock(root);for(const auto& id:SlotNames(root)){SaveSlotInfo info;info.id=id;GameSnapshot value;
  if(!ReadFile(id+".save",value,info.error,true)){if(ReadFile(id+".previous",value,info.error,true))info.recovered=true;else {info.status=info.error.find("unsupported")!=std::string::npos?"incompatible":"corrupt";result.push_back(info);continue;}}
  info.error.clear();info.name=value.displayName;info.scene=value.scene;info.metadata=value.metadata;info.timestamp=value.timestamp;
  info.status=value.gameVersion!=1||value.project!=project||(!content.empty()&&value.content!=content)?"incompatible":"compatible";result.push_back(info);
 }}catch(const std::exception& e){error=e.what();}return result;
}
#endif
