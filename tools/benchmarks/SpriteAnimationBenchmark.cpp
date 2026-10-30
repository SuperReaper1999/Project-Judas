#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "Project.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    Project project;
    std::string error;
    if(!project.Load(argv[1],error)){
        std::cerr<<error;
        return 1;
    }
    AssetDatabase db;
    db.Scan(project.RootDir(),project.AssetsDir());
    AssetId sheet;
    for(const auto& [id,r]:db.Records())if(r.type==AssetType::SpriteSheet){
        sheet=id;
        break;
    }
    if(sheet.empty())return 1;
    ResourceManager resources(nullptr,&db);
    resources.SetBlockingMode(true);
    resources.SetHeadlessResidency(true);
    std::cout<<"sprites,enabled,median_ms,p95_ms,worst_ms,phase\n";
    for(int n:{
        128,1024
    })for(bool enabled:{
        false,true
    }){
        Scene scene;
        for(int i=0;i<n;++i){
            auto& o=scene.CreateObject("Animated prop");
            o.render=SceneRenderComponent{
            };
            o.render->shape=SceneShape::Sprite;
            if(enabled){
                o.render->spritePlayback=SpritePlayback{
                };
                o.render->spritePlayback->sheet=sheet;
                o.render->spritePlayback->clip="run";
            }
        }
        RuntimeWorld world;
        world.legacyGameplay=false;
        if(!world.Build(scene,&resources,error)){
            std::cerr<<error;
            return 1;
        }
        std::vector<double> times;
        for(int i=0;i<270;++i){
            auto start=std::chrono::steady_clock::now();
            world.UpdateSpriteAnimations(1.0/60);
            auto end=std::chrono::steady_clock::now();
            if(i>=30)times.push_back(std::chrono::duration<double,std::milli>(end-start).count());
        }
        std::sort(times.begin(),times.end());
        const auto phase=enabled?world.RuntimeDefinition(1)->render->spritePlayback->time:0;
        if(enabled&&phase<4){
            std::cerr<<"clock did not advance";
            return 1;
        }
        std::cout<<n<<','<<enabled<<','<<times[times.size()/2]<<','<<times[size_t(std::ceil(.95*times.size()))-1]<<','<<times.back()<<','<<phase<<'\n';
    }
}
