#include "ContentStorage.h"
#ifdef __ANDROID__
#include "../PlatformServices.h"
#endif
#include "ModContent.h"
#include <filesystem>
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#endif
namespace fs=std::filesystem;
namespace {
void Need(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
void Name(const std::string& name){std::string key,error;Need(name.size()<=240&&name.find('/')==std::string::npos&&JudasMods::PortablePath(name,key,error),"invalid flat content-storage filename: "+error);}
std::atomic<uint64_t> serial{1};
#ifndef _WIN32
struct FD {int value=-1;explicit FD(int n):value(n){}~FD(){if(value>=0)::close(value);}};
void Regular(int fd,size_t limit){struct stat st{};Need(fd>=0&&fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1&&st.st_size>=0&&uint64_t(st.st_size)<=limit,"nonregular/linked/oversized content file");}
struct FileLease {FD fd;explicit FileLease(int n,bool exclusive=false):fd(n){Regular(n,size_t(-1));Need(!flock(n,(exclusive?LOCK_EX:LOCK_SH)|LOCK_NB),"content version busy");}};
#else
struct Handle {HANDLE value=INVALID_HANDLE_VALUE;explicit Handle(HANDLE h):value(h){}~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
void Regular(HANDLE h,size_t limit){BY_HANDLE_FILE_INFORMATION i{};Need(h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(h,&i)&&!(i.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&i.nNumberOfLinks==1&&((uint64_t(i.nFileSizeHigh)<<32)|i.nFileSizeLow)<=limit,"nonregular/linked/oversized content file");}
#endif
}
struct ContentStorage::Impl {std::string root;
#ifndef _WIN32
 int directory=-1;~Impl(){if(directory>=0)close(directory);}
#else
 std::vector<std::shared_ptr<Handle>> directories;
#endif
};
ContentStorage::ContentStorage(std::string root):m(new Impl){m->root=fs::absolute(fs::u8path(root)).lexically_normal().u8string();}
ContentStorage::~ContentStorage()=default;
const std::string& ContentStorage::Root()const{return m->root;}
std::string ContentStorage::Path(const std::string& name)const{Name(name);return (fs::u8path(m->root)/name).u8string();}
bool ContentStorage::Open(std::string& error){try{
#ifndef _WIN32
 if(m->directory>=0)return true;
 fs::path anchor="/",relative=fs::u8path(m->root).relative_path();
#ifdef __ANDROID__
 Need(AndroidStorageAnchor(fs::u8path(m->root),anchor,relative,error),error);
#endif
 FD parent(open(anchor.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));Need(parent.value>=0,"cannot open filesystem root");
 for(const auto& part:relative){auto name=part.u8string();Need(name!=".."&&!name.empty(),"invalid storage root component");Need(!mkdirat(parent.value,name.c_str(),0700)||errno==EEXIST,"cannot create content directory");int child=openat(parent.value,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);Need(child>=0,"storage root contains link/inaccessible component");close(parent.value);parent.value=child;}
 m->directory=parent.value;parent.value=-1;
#else
 if(!m->directories.empty())return true;
 auto path=fs::u8path(m->root);Need(!path.root_name().empty()&&path.root_name().u8string().rfind("\\\\",0)!=0,"local Windows content root required");auto current=path.root_path();std::vector<std::shared_ptr<Handle>> pinned;
 auto pin=[&]{auto h=std::make_shared<Handle>(CreateFileW(current.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));FILE_ATTRIBUTE_TAG_INFO info{};Need(h->value!=INVALID_HANDLE_VALUE&&GetFileInformationByHandleEx(h->value,FileAttributeTagInfo,&info,sizeof(info))&&(info.FileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(info.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),"storage root contains Windows reparse/inaccessible component");pinned.push_back(h);};pin();
 for(const auto& part:path.relative_path()){Need(part!=L"..","invalid storage root");current/=part;Need(CreateDirectoryW(current.c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS,"cannot create Windows content directory");pin();}m->directories=std::move(pinned);
#endif
 error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
bool ContentStorage::Read(const std::string& name,std::string& out,size_t limit,std::string& error){try{Name(name);Need(Open(error),error);
#ifndef _WIN32
 FD file(openat(m->directory,name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));Regular(file.value,limit);struct stat st{};Need(!fstat(file.value,&st),"stat content failed");std::string bytes(size_t(st.st_size),'\0');size_t at=0;while(at<bytes.size()){auto n=read(file.value,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;Need(n>0,"truncated content read");at+=size_t(n);}char extra;Need(read(file.value,&extra,1)==0,"content changed during read");
#else
 Handle file(CreateFileW(fs::u8path(Path(name)).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));Regular(file.value,limit);LARGE_INTEGER size{};Need(GetFileSizeEx(file.value,&size),"stat content failed");std::string bytes(size_t(size.QuadPart),'\0');size_t at=0;while(at<bytes.size()){DWORD n=0;Need(ReadFile(file.value,bytes.data()+at,DWORD(std::min<size_t>(bytes.size()-at,1024*1024)),&n,nullptr)&&n>0,"truncated content read");at+=n;}
#endif
 out=std::move(bytes);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
std::shared_ptr<void> ContentStorage::Lease(const std::string& name,std::string& error){try{Name(name);Need(Open(error),error);
#ifndef _WIN32
 auto lease=std::make_shared<FileLease>(openat(m->directory,name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
#else
 auto lease=std::make_shared<Handle>(CreateFileW(fs::u8path(Path(name)).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));Regular(lease->value,size_t(-1));
#endif
 error.clear();return lease;}catch(const std::exception& e){error=e.what();return {};}}
std::shared_ptr<void> ContentStorage::Exclusive(const std::string& name,std::string& error){try{
 Name(name);Need(Open(error),error);
#ifndef _WIN32
 auto lease=std::make_shared<FileLease>(openat(m->directory,name.c_str(),O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600),true);Regular(lease->fd.value,0);
#else
 auto lease=std::make_shared<Handle>(CreateFileW(fs::u8path(Path(name)).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));Regular(lease->value,0);
#endif
 error.clear();return lease;
 }catch(const std::exception& e){error="content transaction busy/invalid: "+std::string(e.what());return {};}}
bool ContentStorage::Publish(const std::string& name,const std::string& bytes,std::string& error,bool immutable){std::string temp;bool published=false;try{Name(name);Need(Open(error),error);temp=".tmp-"+std::to_string(serial.fetch_add(1));
#ifndef _WIN32
 temp+='-'+std::to_string(getpid());auto guard=Exclusive(".publish-lock",error);Need(bool(guard),error);std::shared_ptr<FileLease> existing;struct stat st{};if(!fstatat(m->directory,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)){Need(S_ISREG(st.st_mode)&&st.st_nlink==1,"refusing linked/nonregular destination");Need(!immutable,"immutable content version already exists");existing=std::make_shared<FileLease>(openat(m->directory,name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC),true);}
 FD file(openat(m->directory,temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));Need(file.value>=0,"cannot create content staging file");if(boundary)boundary("temporary");size_t at=0;while(at<bytes.size()){auto n=write(file.value,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;Need(n>0,"cannot write content staging");at+=size_t(n);}Need(!fsync(file.value),"content sync failed");if(boundary)boundary("file-synced");Need(!renameat(m->directory,temp.c_str(),m->directory,name.c_str()),"content publication failed");published=true;Need(!fsync(m->directory),"content directory sync failed");
#else
 temp+='-'+std::to_string(GetCurrentProcessId());Handle guard(CreateFileW(fs::u8path(Path(".publish-lock")).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));Regular(guard.value,0);
 auto destination=fs::u8path(Path(name));auto attributes=GetFileAttributesW(destination.c_str());if(attributes!=INVALID_FILE_ATTRIBUTES){Need(!immutable,"immutable content version already exists");Handle existing(CreateFileW(destination.c_str(),GENERIC_READ|DELETE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));Regular(existing.value,size_t(-1));}
 Handle file(CreateFileW(fs::u8path(Path(temp)).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));Need(file.value!=INVALID_HANDLE_VALUE,"cannot create content staging");if(boundary)boundary("temporary");size_t at=0;while(at<bytes.size()){DWORD n=0;Need(WriteFile(file.value,bytes.data()+at,DWORD(std::min<size_t>(bytes.size()-at,1024*1024)),&n,nullptr)&&n>0,"cannot write content staging");at+=n;}Need(FlushFileBuffers(file.value),"content sync failed");if(boundary)boundary("file-synced");CloseHandle(file.value);file.value=INVALID_HANDLE_VALUE;Need(MoveFileExW(fs::u8path(Path(temp)).c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"content version busy or publication failed");published=true;
#endif
 if(boundary)boundary("published");
 error.clear();return true;}catch(const std::exception& e){error=(published?"published; durability uncertain: ":"")+std::string(e.what());if(!temp.empty()&&!published){
#ifndef _WIN32
 if(m->directory>=0)unlinkat(m->directory,temp.c_str(),0);
#else
 DeleteFileW(fs::u8path(Path(temp)).c_str());
#endif
 }return false;}}
bool ContentStorage::Remove(const std::string& name,std::string& error){try{Name(name);Need(Open(error),error);auto guard=Exclusive(".publish-lock",error);Need(bool(guard),error);
#ifndef _WIN32
 FD file(openat(m->directory,name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));Regular(file.value,size_t(-1));Need(!flock(file.value,LOCK_EX|LOCK_NB),"content version is in use");Need(!unlinkat(m->directory,name.c_str(),0)&&!fsync(m->directory),"delete content failed/durability uncertain");
#else
 auto p=fs::u8path(Path(name));Handle file(CreateFileW(p.c_str(),GENERIC_READ|DELETE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));Regular(file.value,size_t(-1));FILE_DISPOSITION_INFO dispose{TRUE};Need(SetFileInformationByHandle(file.value,FileDispositionInfo,&dispose,sizeof(dispose)),"content version is in use/delete failed");
#endif
 error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
bool ContentStorage::List(std::vector<std::string>& out,size_t limit,std::string& error){try{Need(Open(error),error);std::vector<std::string> result;
#ifndef _WIN32
 FD scan(openat(m->directory,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));DIR* dir=fdopendir(dup(scan.value));Need(dir!=nullptr,"cannot list content directory");try{while(auto* entry=readdir(dir)){std::string name=entry->d_name;if(name=="."||name=="..")continue;Need(result.size()<limit,"content directory entry bound");Name(name);struct stat st{};Need(!fstatat(m->directory,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)&&S_ISREG(st.st_mode)&&st.st_nlink==1,"nonregular/linked storage entry");result.push_back(name);}}catch(...){closedir(dir);throw;}closedir(dir);
#else
 for(auto& entry:fs::directory_iterator(fs::u8path(m->root))){Need(result.size()<limit,"content directory entry bound");auto name=entry.path().filename().u8string();Name(name);Handle file(CreateFileW(entry.path().c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));Regular(file.value,size_t(-1));result.push_back(name);}
#endif
 std::sort(result.begin(),result.end());out=std::move(result);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
bool ReadRegularContentFile(const std::string& source,std::string& out,size_t limit,std::string& error){try{
 auto path=fs::absolute(fs::u8path(source)).lexically_normal();
#ifndef _WIN32
 fs::path anchor="/",relative=path.relative_path();
#ifdef __ANDROID__
 std::string anchorError;Need(AndroidStorageAnchor(path,anchor,relative,anchorError),anchorError);
#endif
 FD parent(open(anchor.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));Need(parent.value>=0,"cannot open input root");for(const auto& part:relative.parent_path()){int child=openat(parent.value,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);Need(child>=0,"input parent contains link/inaccessible component");close(parent.value);parent.value=child;}FD file(openat(parent.value,path.filename().c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));Regular(file.value,limit);struct stat st{};Need(!fstat(file.value,&st),"input stat failed");std::string bytes(size_t(st.st_size),'\0');size_t at=0;while(at<bytes.size()){auto n=read(file.value,bytes.data()+at,bytes.size()-at);if(n<0&&errno==EINTR)continue;Need(n>0,"truncated input");at+=size_t(n);}char extra;Need(read(file.value,&extra,1)==0,"input grew during read");
#else
 std::vector<std::shared_ptr<Handle>> parents;auto current=path.root_path();auto pin=[&]{auto handle=std::make_shared<Handle>(CreateFileW(current.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));FILE_ATTRIBUTE_TAG_INFO info{};Need(handle->value!=INVALID_HANDLE_VALUE&&GetFileInformationByHandleEx(handle->value,FileAttributeTagInfo,&info,sizeof(info))&&(info.FileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(info.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT),"input parent contains reparse/inaccessible component");parents.push_back(handle);};pin();for(const auto& part:path.parent_path().relative_path()){current/=part;pin();}Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));Regular(file.value,limit);LARGE_INTEGER size{};Need(GetFileSizeEx(file.value,&size),"input stat failed");std::string bytes(size_t(size.QuadPart),'\0');size_t at=0;while(at<bytes.size()){DWORD n=0;Need(ReadFile(file.value,bytes.data()+at,DWORD(std::min<size_t>(bytes.size()-at,1024*1024)),&n,nullptr)&&n>0,"truncated input");at+=n;}
#endif
 out=std::move(bytes);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
