#pragma once
#include "CollisionAsset.h"
struct ModelCollisionCleanup {
 bool removeDegenerates=false,orientPatches=false;
 double weldTolerance=0; // explicit collision-only duplicate-position tolerance
};
struct ModelCollisionCleanupReport {unsigned removed=0,welded=0,flipped=0;};
// Derived geometry only. Never edits the visual mesh, UVs or source bytes.
bool CleanModelCollision(const MeshData&,const ModelCollisionCleanup&,MeshData&,ModelCollisionCleanupReport&,std::string&,CollisionDiagnostic* diagnostic=nullptr);

// Same explicit derived cleanup and strict cook used by editor and CLI.
bool CookImportedCollisionFile(const std::string& source,const std::string& sourceId,const CollisionCookSettings&,const ModelCollisionCleanup&,const std::string& destination,CollisionAsset&,ModelCollisionCleanupReport&,CollisionDiagnostic&,std::string&);
bool ReadModelCollisionCleanup(const std::string& destination,ModelCollisionCleanup&,std::string&);
