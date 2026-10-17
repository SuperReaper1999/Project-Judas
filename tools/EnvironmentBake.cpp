#include "BuildIdentity.h"
#include "Environment.h"
#include <cstdio>
#include <chrono>
int main(int argc,char** argv){
    if (HandleBuildIdentityArguments(argc, argv, "environment_bake")) return 0;
if(argc<3){std::fprintf(stderr,"Usage: judas_environment_bake input.hdr output.judasenv [width=128] [samples=128]\n");return 2;}EnvironmentData data;std::string error;auto start=std::chrono::steady_clock::now();if(!BakeEnvironment(argv[1],argc>3?std::stoul(argv[3]):128,argc>4?std::stoul(argv[4]):128,data,error)||!SaveEnvironment(argv[2],data,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}std::printf("Baked %s content %s, %zu levels in %.3f s\n",argv[2],data.sourceHash.c_str(),data.specular.size(),std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());}
