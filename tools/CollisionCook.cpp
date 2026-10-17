#include "BuildIdentity.h"
#include "CollisionAsset.h"
#include <cstdio>
#include <stdexcept>
int main(int argc,char** argv){
    if (HandleBuildIdentityArguments(argc, argv, "collision_cook")) return 0;
try{
 bool twoSided=argc>6&&std::string(argv[argc-1])=="--two-sided";if(twoSided)--argc;
 if(argc<6||argc>9)throw std::runtime_error("usage: judas_collision_cook mesh|hull source output sourceAssetId primitive [scaleX scaleY scaleZ] [--two-sided]; one-sided mesh by default");
 CollisionCookSettings settings;settings.twoSided=twoSided;std::string kind=argv[1];if(kind!="mesh"&&kind!="hull")throw std::runtime_error("kind must be mesh or hull");settings.convex=kind=="hull";settings.primitive=std::stoul(argv[5]);
 if(argc!=6&&argc!=9)throw std::runtime_error("supply all three positive scale values");
 if(argc==9)for(int i=0;i<3;++i)settings.transform[i][i]=std::stod(argv[6+i]);
 CollisionAsset asset;std::string error;if(!CookCollisionFile(argv[2],argv[4],settings,argv[3],asset,error))throw std::runtime_error(error);
 std::printf("%s: %zu vertices %zu triangles %zu BVH nodes, volume %.9g m3\n",argv[3],asset.vertices.size(),asset.faces.size(),asset.nodes.size(),asset.volume);for(auto& warning:asset.warnings)std::printf("warning: %s\n",warning.c_str());return 0;
 }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
