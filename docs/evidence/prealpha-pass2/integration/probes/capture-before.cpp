#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include <cstdio>
int main(){Scene s;auto& o=s.CreateObject("atmosphere owner");o.body=SceneBodyComponent{};o.celestial=SceneCelestialComponent{};o.celestial->gravitationalParameter=62784;o.atmosphere=SceneAtmosphereComponent{};RuntimeWorld w;w.legacyGameplay=false;std::string e;if(!w.Build(s,nullptr,e))return 2;std::map<std::string,SaveChunk> old{{"sentinel",{1,"original"}}};bool saved=WorldPersistence::Capture(w,old,e);bool retained=old.size()==1&&old.count("sentinel")&&old.at("sentinel").data=="original";std::printf("saved=%d previous_retained=%d chunks=%zu diagnostic=%s\n",saved,retained,old.size(),e.c_str());return !saved&&retained?0:1;}
