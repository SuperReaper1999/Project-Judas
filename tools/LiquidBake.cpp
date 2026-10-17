#include "BuildIdentity.h"
#include "LiquidTypes.h"
#include "AssetDatabase.h"
#include "Project.h"
#include "Prefab.h"
#include "SceneSerialization.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
int main(int argc,char** argv){
    if (HandleBuildIdentityArguments(argc, argv, "liquid_bake")) return 0;
if(argc!=3){std::puts("usage: judas_liquid_bake project.judasproj scene-relative-path");return 2;}Project project;Scene scene,resolved,flat;AssetDatabase assets;std::string error;auto fail=[&](){std::fprintf(stderr,"%s\n",error.c_str());return 1;};if(!project.Load(argv[1],error))return fail();assets.Scan(project.RootDir(),project.AssetsDir());auto file=std::filesystem::path(project.RootDir())/argv[2];if(!LoadSceneFromFile(file.string(),scene,error)||!ResolvePrefabs(scene,&assets,resolved,error)||!FlattenHierarchy(resolved,flat,error))return fail();for(auto& o:scene.Objects())if(o.liquidBasin){auto* source=flat.Find(o.id);LiquidGeometry geometry;GravityEquilibrium eq;std::string hash;LiquidBasinData data;auto start=std::chrono::steady_clock::now();if(!source||!PrepareLiquidBake(flat,*source,assets,geometry,eq,hash,error)||!BakeLiquidBasin(geometry,eq,o.liquidBasin->volumeTolerance,o.liquidBasin->heightTolerance,hash,data,error))return fail();if(!BakeLiquidSurface(data,o.liquidBasin->surface,data.dynamicSurface,error,&geometry))return fail();auto path=std::filesystem::path(project.AssetsDir())/"liquid"/("basin-"+std::to_string(o.id)+".judasbasin");std::filesystem::create_directories(path.parent_path());if(!SaveLiquidBasin(path.string(),data,error))return fail();AssetRecord record;auto* existing=assets.FindByRelativePath(std::filesystem::relative(path,project.RootDir()).generic_string());if(existing)record=*existing;else if(!assets.Track(path.string(),record,error))return fail();o.liquidBasin->asset=record.id;std::printf("basin %llu: %.6f L capacity, %zu curve samples, %zu integration cells, interpolation <=%.6g L, curved height bound %.6g m; %.3f ms; %s\n",static_cast<unsigned long long>(o.id),data.capacity*1000,data.curve.size(),data.geometry.cells.size(),data.volumeTolerance*1000,data.coordinateError,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),hash.c_str());}if(!SaveSceneToFile(scene,file.string(),error))return fail();return 0;}
