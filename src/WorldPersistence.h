#pragma once
#include "SaveStorage.h"
#include <string>
class RuntimeWorld;
class Scene;
// Small participant registry, not reflection. Each subsystem owns its record
// version and native fixup. Capture/restoration are main-thread operations.
class WorldPersistence {
public:
 static bool Capture(RuntimeWorld&,std::map<std::string,SaveChunk>&,std::string&);
 static bool SceneFrom(const std::map<std::string,SaveChunk>&,Scene&,std::string&);
 static bool Prepare(RuntimeWorld&,std::string&);
 static bool PrepareAudio(RuntimeWorld&,std::string&);
 static bool Restore(RuntimeWorld&,const std::map<std::string,SaveChunk>&,std::string&);
 static void Published(RuntimeWorld&);
 // Owner-thread instance records; no source parsing or whole-world capture.
 static bool CanSuspendAnimation(RuntimeWorld&,uint64_t);
 static bool CaptureAnimation(RuntimeWorld&,uint64_t,std::string&,std::string&);
 static bool RestoreAnimation(RuntimeWorld&,uint64_t,const std::string&,std::string&);
 static bool CaptureKinematic(RuntimeWorld&,uint64_t,std::string&,std::string&);
 static bool RestoreKinematic(RuntimeWorld&,uint64_t,const std::string&,std::string&);
private:
 static void Entities(RuntimeWorld&,SaveArchive&);
 static void Motors(RuntimeWorld&,SaveArchive&);
 static void Animation(RuntimeWorld&,SaveArchive&);
 static void InstanceAnimation(RuntimeWorld&,uint64_t,SaveArchive&);
 static void Articulation(RuntimeWorld&,SaveArchive&);
 static void ArticulationExtended(RuntimeWorld&,SaveArchive&,unsigned);
 static void PhysicalAnimation(RuntimeWorld&,SaveArchive&);
 static void Navigation(RuntimeWorld&,SaveArchive&);
 static void Deformables(RuntimeWorld&,SaveArchive&);
 static void Audio(RuntimeWorld&,SaveArchive&);
};
