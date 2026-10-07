#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "MeshData.h"

// Immutable CPU cook, shared by body instances. All coordinates are asset local.
struct CollisionFace {
    std::array<uint32_t,3> vertices{};
    std::array<int32_t,3> adjacent{{-1,-1,-1}};
    std::array<bool,3> active{{true,true,true}};
    uint32_t source=0;
    glm::dvec3 normal{0};
};
struct CollisionPolygon {std::vector<uint32_t> vertices;glm::dvec3 normal{0};};
struct CollisionNode {glm::dvec3 minimum{0},maximum{0};uint32_t first=0,count=0,left=0,right=0;};
struct CollisionAsset {
    bool convex=false,twoSided=false;
    double weldTolerance=0; // Exact-position welding within source identity only.
    std::string sourceAsset,sourceFingerprint,settingsFingerprint;
    unsigned selectedPrimitive=0;
    glm::dmat4 sourceTransform{1};
    std::vector<glm::dvec3> vertices;
    std::vector<CollisionFace> faces;
    std::vector<CollisionPolygon> polygons;
    std::vector<std::array<uint32_t,2>> edges;
    std::vector<CollisionNode> nodes;
    std::vector<uint32_t> order;
    glm::dvec3 minimum{0},maximum{0},centerOfMass{0};
    double volume=0;
    glm::dmat3 unitInertia{0}; // about COM, density one
    std::vector<std::string> warnings;
    void Candidates(glm::dvec3 minimum,glm::dvec3 maximum,std::vector<uint32_t>&,uint64_t* tested=nullptr) const;
};
struct CollisionCookSettings {bool convex=false,twoSided=false;unsigned primitive=0;glm::dmat4 transform{1};};
struct CollisionDiagnostic {
    std::string code,node,action;
    uint32_t sourceFace=UINT32_MAX;
    std::vector<uint32_t> sourceVertices;
    glm::dvec3 point{0},minimum{0},maximum{0};
};
bool CookCollision(const MeshData&,const CollisionCookSettings&,CollisionAsset&,std::string&,CollisionDiagnostic* diagnostic=nullptr);
bool SaveCollisionAsset(const std::string&,const CollisionAsset&,std::string&);
bool DecodeCollisionAsset(const std::vector<uint8_t>&,CollisionAsset&,std::string&);
bool LoadCollisionAsset(const std::string&,CollisionAsset&,std::string&);
// Explicit source selection, separate from rendering import/animation policy.
bool LoadCollisionSource(const std::string&,unsigned primitive,MeshData&,std::string&);

// Shared editor/CLI transaction. Fingerprints bind the selected source and settings.
std::string CollisionSettingsFingerprint(const CollisionCookSettings&);
bool CookCollisionFile(const std::string& source,const std::string& sourceId,
                       const CollisionCookSettings&,const std::string& destination,
                       CollisionAsset&,std::string& error);
bool CollisionAssetStale(const CollisionAsset&,const std::string& source,std::string& reason);
