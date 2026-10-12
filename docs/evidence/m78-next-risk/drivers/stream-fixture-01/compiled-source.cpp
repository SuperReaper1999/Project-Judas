#include "Project.h"
#include "SceneSerialization.h"
#include "AssetDatabase.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char**argv){if(argc!=2)return 2;fs::path out=argv[1];if(fs::exists(out))return 3;fs::create_directories(out);fs::copy("projects/streamed_range",out/"project",fs::copy_options::recursive);auto root=out/"project";Project project;std::string error;unsigned tag;if(!project.Load((root/"streamed_range.judasproj").string(),error)||!project.Settings().classification.tags.Add("AuditMotor",tag)||!project.Save(error)){std::cerr<<error;return 1;}auto script=root/"Assets/audit_motor.js";std::ofstream(script)<<"export default class {constructor({entity}){this.entity=entity;this.state={starts:0};}start(){this.state.starts++;this.entity.addTag('AuditMotor');}}";AssetDatabase db;db.Scan(project.RootDir(),project.AssetsDir());AssetRecord asset;if(!db.Track(script.string(),asset,error)){std::cerr<<error;return 1;}unsigned patched=0;for(auto&p:fs::directory_iterator(root/"Scenes"))if(p.path().extension()==".judas"){Scene scene;if(!LoadSceneFromFile(p.path().string(),scene,error)){std::cerr<<error;return 1;}for(auto&obj:scene.Objects())if(obj.characterMotor){obj.scripts.push_back({123,asset.id,true,"{}"});++patched;}if(!SaveSceneToFile(scene,p.path().string(),error)){std::cerr<<error;return 1;}}std::cout<<"Prepared ordinary copied project: "<<patched<<" motors, AuditMotor tag="<<tag<<'\n';return patched?0:1;}
