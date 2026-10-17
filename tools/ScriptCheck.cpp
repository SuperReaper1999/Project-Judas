#include "Project.h"
#include "AssetDatabase.h"
#include "ScriptSystem.h"
#include <cstdio>
int main(int argc,char** argv){
    if(argc!=2){std::fprintf(stderr,"Usage: judas_script_check project.judasproj\nCompile/link registered modules without executing project code.\n");return 2;}
    Project project;std::string error;
    if(!project.Load(argv[1],error)){std::fprintf(stderr,"Project: %s\n",error.c_str());return 2;}
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
    for(const auto& text:assets.Problems())std::fprintf(stderr,"Asset: %s\n",(text.path+": "+text.message).c_str());
    auto errors=ScriptSystem::CheckProjectSources(assets);
    for(const auto& d:errors)std::fprintf(stderr,"%s\n",d.message.c_str());
    unsigned count=0;for(const auto& [id,a]:assets.Records()){(void)id;if(a.type==AssetType::Script)++count;}
    std::printf("%u registered script modules; %zu compile/link failures (maximum 128 reported). No project code executed.\n",count,errors.size());
    return errors.empty()&&assets.Problems().empty()?0:1;
}
