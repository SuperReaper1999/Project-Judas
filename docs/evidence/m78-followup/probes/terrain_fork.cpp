#include "TerrainAuthoring.h"
#include "ModAuthoring.h"
#include "Project.h"
#include <fstream>
#include <filesystem>
#include <iostream>
int main(int argc,char** argv){if(argc!=4)return 2;std::string error;Project p;AssetDatabase db;TerrainSource original;if(!p.Load(argv[1],error)||!JudasMods::OpenAuthorAssets(p,db,error)||!LoadTerrain(p.Resolve(argv[2]),original,error)){std::cerr<<error;return 3;}const auto before=SerializeTerrain(original);auto fork=ForkTerrain(original);if(fork.identity==original.identity||fork.meshId==original.meshId||fork.collisionId==original.collisionId||fork.textureId==original.textureId)return 4;fork.heights[fork.heights.size()/2]+=.75f;std::filesystem::path root=argv[3];std::filesystem::create_directories(root/"Imports");auto path=root/"Imports/fork.judasterrain";std::ofstream(path)<<SerializeTerrain(fork);TerrainCookResult result;if(!CookTerrain(fork,path.string(),root.string(),db,result,error)){std::cerr<<error;return 5;}TerrainSource reloaded;if(!LoadTerrain(p.Resolve(argv[2]),reloaded,error)||SerializeTerrain(reloaded)!=before)return 6;std::cout<<"PASS generic fork allocates four independent identities\nPASS sculpt/cook original production pipeline\nPASS original source remains byte-equivalent in serialization\n";return 0;}
