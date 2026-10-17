#pragma once
#include "MeshData.h"
#include <functional>
#include <string>
#include <vector>
// Bounded author-owned implicit solid: negative inside, positive in air.
// M84 authoring and opt-in M97 instances share these exact grid operations.
struct VolumeTerrainSource {
 unsigned version=1;glm::uvec3 resolution{33};glm::vec3 extent{16};
 std::string identity,meshId,collisionId,productDirectory;
 std::vector<float> density;
 size_t Index(unsigned x,unsigned y,unsigned z)const{return (size_t(z)*resolution.y+y)*resolution.x+x;}
 glm::vec3 Point(unsigned x,unsigned y,unsigned z)const{return glm::vec3(x,y,z)*extent/glm::vec3(resolution-glm::uvec3(1))-extent*.5f;}
};
VolumeTerrainSource CreateVolumeTerrain(glm::uvec3,glm::vec3);
VolumeTerrainSource ForkVolumeTerrain(const VolumeTerrainSource&);
bool ValidateVolumeTerrain(const VolumeTerrainSource&,std::string&);
std::string SerializeVolumeTerrain(const VolumeTerrainSource&);
bool ParseVolumeTerrain(const std::string&,VolumeTerrainSource&,std::string&);
bool LoadVolumeTerrain(const std::string&,VolumeTerrainSource&,std::string&);
// Indexed, consistently triangulated tetrahedral extraction with outward winding.
bool BuildVolumeTerrainMesh(const VolumeTerrainSource&,MeshData&,std::string&,const std::function<bool()>& cancelled={});
enum class VolumeBrushKind {Remove,Add,Smooth};
struct VolumeBrush {VolumeBrushKind kind=VolumeBrushKind::Remove;float radius=1,strength=1;};
bool ApplyVolumeBrush(VolumeTerrainSource&,glm::vec3,const VolumeBrush&,std::string&);
