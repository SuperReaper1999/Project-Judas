#pragma once
#include "TerrainAuthoring.h"
// Bounded author-owned implicit solid: negative inside, positive in air.
// Runtime consumes normal immutable model/collision assets, not this grid.
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
class VolumeTerrainDraft {
public:
 VolumeTerrainSource source;
 bool Open(const std::string&,std::string&);bool Save(std::string&);
 void SetPath(std::string p){m_path=std::move(p);}const std::string& Path()const{return m_path;}
 bool Dirty()const{return m_dirty;}bool Active()const{return m_active;}uint64_t Revision()const{return m_revision;}
 bool ExternalChanged()const;
 bool Begin(glm::vec3,const VolumeBrush&,std::string&);bool Continue(glm::vec3,std::string&);
 void End();void Cancel();bool Undo();bool Redo();
private:
 void Push(std::vector<std::vector<float>>&,std::vector<float>);
 std::string m_path,m_disk,m_saved;bool m_dirty=true,m_active=false;uint64_t m_revision=0;
 VolumeBrush m_brush;glm::vec3 m_last{};float m_remainder=0;
 std::vector<float> m_before;std::vector<std::vector<float>> m_undo,m_redo;
};
bool CookVolumeTerrain(const VolumeTerrainSource&,const std::string& sourcePath,const std::string& projectRoot,
 const AssetDatabase&,TerrainCookResult&,std::string&,const std::function<bool()>& cancelled={});
