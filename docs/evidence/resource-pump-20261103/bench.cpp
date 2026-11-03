#include "ResourceManager.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
int main(int argc,char** argv) {
    if(argc!=3) return 1;
    namespace fs=std::filesystem;
    const auto root=fs::absolute(argv[1]);
    const int count=std::stoi(argv[2]);
    fs::create_directories(root/"Assets");
    AssetDatabase assets;
    assets.Scan(root.string(),(root/"Assets").string());
    std::vector<AssetId> ids;
    for(int i=0;i<count;++i) {
        const auto path=root/"Assets"/(std::to_string(i)+".tga");
        unsigned char pixels[22]={};
        pixels[2]=2; pixels[12]=pixels[14]=1; pixels[16]=32; pixels[21]=255;
        {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(pixels),sizeof(pixels));}
        AssetRecord record; std::string error;
        if(!assets.Track(path.string(),record,error)){std::cerr<<error<<'\n';return 2;}
        ids.push_back(record.id);
    }
    ResourceManager manager(nullptr,&assets);
    manager.SetHeadlessResidency(true);
    manager.SetBlockingMode(true);
    using Clock=std::chrono::steady_clock;
    const auto elapsed=[](auto start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();};
    for(int sample=0;sample<9;++sample) {
        for(const auto& id:ids) if(manager.RequestTexture(id)!=ResourceState::Ready)return 3;
        manager.AddRef(ids.back());
        const auto full=manager.Stats().bytesResident;
        if(!full || full%ids.size())return 4;
        auto start=Clock::now();
        for(int i=0;i<400;++i) manager.Pump();
        const auto idle=elapsed(start)/400;
        const auto unit=full/ids.size();
        start=Clock::now();
        const auto evicted=manager.EvictToFit(unit*2);
        const auto evict=elapsed(start);
        if(evicted!=ids.size()-2 || manager.Stats().bytesResident!=unit*2 ||
            manager.StateOf(ids.back())!=ResourceState::Ready ||
            manager.StateOf(ids[ids.size()-2])!=ResourceState::Ready)return 5;
        manager.ReleaseRef(ids.back());
        std::cout<<std::setprecision(9)<<count<<','<<sample<<','<<idle<<','<<evict<<','<<evicted<<'\n';
        manager.EvictToFit(0);
    }
}
