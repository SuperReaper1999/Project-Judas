#include "EnginePaths.h"
#include "PlatformServices.h"
#include "GamePackage.h"
#include "SaveStorage.h"
#include "SceneFingerprint.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif
int main(int argc,char** argv){
 namespace fs=std::filesystem;
 if(argc==3&&std::string(argv[1])=="--quoted-child"){std::ofstream(fs::u8path(argv[2]))<<"child";return 0;}
 if(argc==2&&std::string(argv[1])=="--build-info"){std::printf("Judas runtime %s Release\n",RuntimePlatformName());return 0;}
 int checks=0,failures=0;auto check=[&](bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);};
 const auto root=fs::temp_directory_path()/("judas-port-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root);
 std::string error;check(!EngineExecutableDir().empty()&&fs::is_directory(fs::u8path(EngineExecutableDir())),"executable root independent of cwd");
 fs::current_path(root);check(fs::is_directory(fs::u8path(EngineExecutableDir())),"executable root after unrelated cwd");
 GamePackage package{"",std::string(64,'a')};std::string saves;check(PackageSaveDirectory(package,saves,error)&&fs::u8path(saves).is_absolute(),"absolute user save location");
 const auto file=root/"model with spaces.bin";{std::ofstream(file)<<"first";}
 auto stamp=ImportFileStamp(file);auto staging=CreateImportStagingFile(file);{std::ofstream(fs::u8path(staging))<<"second";}
 ReplaceStagedFile(fs::u8path(staging),file);check(ImportFileStamp(file)!=stamp,"import replacement changes race stamp");
 {std::ifstream f(file);std::string text;f>>text;check(text=="second","replace existing staged destination");}
 auto exportStage=fs::u8path(CreateExportStagingDirectory(root));
 check(fs::is_directory(exportStage),"exclusive export staging directory");
 {
  SaveStorage store((root/"save storage").u8string());GameSnapshot s;s.project="port";s.scene="Scenes/main.judas";s.content=SceneFingerprintSha256("port");s.displayName="Unicode résumé 日本語";s.timestamp=1;s.participants["fixture"]={1,"payload"};
  GameSnapshot out;bool recovered=false;check(store.Write("slot",s,error),"save first generation");s.timestamp=2;check(store.Write("slot",s,error),"save replacement and backup");
  check(store.Read("slot",out,recovered,error)&&out.timestamp==2&&out.displayName==s.displayName,"save metadata/payload round trip");
  {std::ofstream(root/"save storage/slot.save",std::ios::trunc)<<"corrupt";}
  check(store.Read("slot",out,recovered,error)&&recovered&&out.timestamp==1,"previous generation recovery");
  check(!store.Write("../escape",s,error),"path traversal rejected");
  store.boundary=[](const char* p){if(std::string(p)=="file-synced")throw std::runtime_error("controlled publication failure");};
  check(!store.Write("failed",s,error)&&!fs::exists(root/"save storage/failed.save"),"failed publication not advertised");store.boundary={};
  check(store.List(s.project,s.content,error).size()==1,"slot listing excludes temporary files");
#ifdef _WIN32
  HANDLE lock=CreateFileW((root/L"save storage/.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
  check(lock!=INVALID_HANDLE_VALUE&&!store.Write("busy",s,error),"exclusive save lock rejects concurrent writer");
  if(lock!=INVALID_HANDLE_VALUE)CloseHandle(lock);
#endif
  check(store.Delete("slot",error)&&!store.Read("slot",out,recovered,error),"delete current and previous generation");
 }
#ifdef _WIN32
 std::string info;auto executable=fs::u8path(EngineExecutableDir())/"judas_windows_readiness_tests.exe";
 check(WindowsCaptureBuildInfo(executable,info)&&info=="Judas runtime Windows Release\n","bounded native build-info capture");
 auto childFile=root/"child quoted path.txt";check(WindowsLaunchProcess(executable,{"--quoted-child",childFile.u8string()},error),"native process with spaced argument");
 for(int n=0;n<100&&!fs::exists(childFile);++n)std::this_thread::sleep_for(std::chrono::milliseconds(20));
 check(fs::exists(childFile),"child receives exact quoted path");
#endif
 fs::current_path(fs::temp_directory_path());fs::remove_all(root);
 std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
