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
 static bool CaptureWheel(RuntimeWorld&,uint64_t,std::string&,std::string&);
 static bool RestoreWheel(RuntimeWorld&,uint64_t,const std::string&,std::string&);
 static bool CapturePowertrain(RuntimeWorld&,uint64_t,std::string&,std::string&);
 static bool RestorePowertrain(RuntimeWorld&,uint64_t,const std::string&,std::string&);
 static bool CaptureAnimation(RuntimeWorld&,uint64_t,std::string&,std::string&);
 static bool RestoreAnimation(RuntimeWorld&,uint64_t,const std::string&,std::string&);
 static bool CaptureKinematic(RuntimeWorld&,uint64_t,std::string&,std::string&);
 static bool RestoreKinematic(RuntimeWorld&,uint64_t,const std::string&,std::string&,glm::dvec3 capturedOriginDelta={});
private:
 static bool IncludesEntity(const RuntimeWorld&,uint64_t);
 static bool CapturePrepared(RuntimeWorld&,std::map<std::string,SaveChunk>&,std::string&);
 static void Entities(RuntimeWorld&,SaveArchive&);
 static void Motors(RuntimeWorld&,SaveArchive&);
 static void Wheels(RuntimeWorld&,SaveArchive&);
 static void Powertrains(RuntimeWorld&,SaveArchive&);
 static void Animation(RuntimeWorld&,SaveArchive&);
 static void InstanceAnimation(RuntimeWorld&,uint64_t,SaveArchive&);
 static void AnimationObservation(RuntimeWorld&,uint64_t,SaveArchive&);
 static void Articulation(RuntimeWorld&,SaveArchive&);
 static void ArticulationExtended(RuntimeWorld&,SaveArchive&,unsigned,std::string* deferred=nullptr);
 static void JointContinuation(RuntimeWorld&,SaveArchive&);
 static void PhysicalAnimation(RuntimeWorld&,SaveArchive&);
 static void PhysicalBalance(RuntimeWorld&,SaveArchive&);
 static void Navigation(RuntimeWorld&,SaveArchive&);
 static void Deformables(RuntimeWorld&,SaveArchive&,bool materialFrame=true,bool attachmentLimits=true);
 static void Audio(RuntimeWorld&,SaveArchive&);
};
