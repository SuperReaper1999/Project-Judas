#include "VolumeTerrain.h"
#include <chrono>
#include <cstdio>
int main(){for(unsigned n:{33u,65u}){VolumeTerrainDraft d;d.source=CreateVolumeTerrain({n,n,n},{32,32,32});VolumeBrush b;b.radius=.1f;std::string e;auto start=std::chrono::steady_clock::now();if(!d.Begin({-1,0,0},b,e)||!d.Continue({1,0,0},e)){std::fprintf(stderr,"%s\n",e.c_str());return 1;}auto moving=std::chrono::steady_clock::now();d.End();std::printf("PERF grid=%u samples=%zu radius=0.1 path=2m stroke_ms=%.6f end_ms=%.6f\n",n,d.source.density.size(),std::chrono::duration<double,std::milli>(moving-start).count(),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-moving).count());}return 0;}
