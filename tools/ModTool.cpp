#include "ModAuthoring.h"
#include "SceneFingerprint.h"
#include "EnginePaths.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace JudasMods;
namespace fs=std::filesystem;
int main(int argc,char** argv){try{std::string error;if(argc<3)throw std::runtime_error("judas_mod inspect PACKAGE | kit PROJECT DEST DOCS | new KIT DEST MOD_ID | package PROJECT OUTPUT | validate PROJECT | install GAME PACKAGE | list GAME | enable GAME MOD VERSION HASH [--trust-executable] | disable GAME MOD | remove GAME MOD VERSION HASH | resolve GAME");auto need=[&](bool ok){if(!ok)throw std::runtime_error(error);};std::string command=argv[1];
 if(command=="inspect"){Package p;need(ReadPackage(argv[2],p,error));std::cout<<ToJSON(p.manifest).dump(2)<<"\nsha256 "<<p.hash<<'\n';return 0;}
 if(command=="new"){if(argc!=5)throw std::runtime_error("new KIT DEST MOD_ID");Project p;need(NewModProject(argv[2],argv[3],argv[4],p,error));std::cout<<p.ProjectFile()<<'\n';return 0;}
 Project p;need(p.Load(argv[2],error));if(command=="kit"){if(argc!=5)throw std::runtime_error("kit PROJECT DEST DOCS");need(ExportKit(p,argv[3],argv[4],error));return 0;}
 if(command=="bind-dependency"){if(argc!=4)throw std::runtime_error("bind-dependency PROJECT PACKAGE");need(BindDependency(p,argv[3],error));return 0;}
 if(command=="identity"){if(argc!=4)throw std::runtime_error("identity PROJECT LOCAL_ASSET_ID");std::cout<<AuthorIdentity(p,argv[3])<<'\n';return 0;}
 if(command=="package"||command=="validate"){auto source=fs::u8path(p.RootDir())/"mod.json";if(fs::file_size(source)>Limits::Metadata)throw std::runtime_error("manifest exceeds 1 MiB");std::ifstream file(source);std::string text((std::istreambuf_iterator<char>(file)),{});Manifest m;need(ParseManifest(text,m,error,true));Package package;need(AuthorPackage(p,m,package,error));if(command=="package"){if(argc!=4)throw std::runtime_error("package PROJECT OUTPUT");need(WritePackage(argv[3],package,error));}std::cout<<package.manifest.id<<" "<<package.manifest.version<<" "<<package.hash<<'\n';return 0;}
 Contract c;need(ParseContract(p.Settings().modContract,c,error));std::string root;need(GameModDirectory(c,root,error));Store store(root);
 if(command=="install"){if(argc!=4)throw std::runtime_error("install GAME PACKAGE");Package package;need(ReadPackage(argv[3],package,error));need(InstallForGame(p,package,error));std::cout<<"Installed; not enabled. Changes apply next launch. "<<package.hash<<'\n';}
 else if(command=="list"){std::vector<Installed> installed;std::vector<Loadout> loadout;need(store.List(installed,loadout,error));for(auto& i:installed)std::cout<<i.id<<" "<<i.version<<" "<<i.hash<<'\n';for(auto& s:loadout)std::cout<<"SELECTED "<<s.id<<" "<<s.version<<" "<<s.hash<<" trusted="<<(!s.trustedHash.empty())<<'\n';}
 else if(command=="enable"){if(argc!=6&&argc!=7)throw std::runtime_error("enable GAME MOD VERSION HASH [--trust-executable]");bool trust=argc==7&&std::string(argv[6])=="--trust-executable";if(argc==7&&!trust)throw std::runtime_error("unknown trust option");need(store.Enable(c,argv[3],argv[4],argv[5],trust,error));std::cout<<"Enabled exact payload; restart the game.\n";}
 else if(command=="disable"){if(argc!=4)throw std::runtime_error("disable GAME MOD");need(store.Disable(argv[3],error));std::cout<<"Disabled for next launch; dependents may require correction.\n";}
 else if(command=="remove"){if(argc!=6)throw std::runtime_error("remove GAME MOD VERSION HASH");need(store.Remove(argv[3],argv[4],argv[5],error));}
 else if(command=="resolve"){Snapshot snapshot;need(store.Freeze(c,snapshot,error));std::cout<<snapshot.resolution.catalog.dump(2)<<"\nsignature "<<snapshot.resolution.signature<<'\n';}
 else {throw std::runtime_error("unknown mod command");}
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
