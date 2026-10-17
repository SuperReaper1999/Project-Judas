#pragma once
#include "TerrainAuthoring.h"
#include "../VolumeTerrainGeometry.h"
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
 std::vector<float> m_before,m_savedDensity;std::vector<std::vector<float>> m_undo,m_redo;
};
bool CookVolumeTerrain(const VolumeTerrainSource&,const std::string& sourcePath,const std::string& projectRoot,
 const AssetDatabase&,TerrainCookResult&,std::string&,const std::function<bool()>& cancelled={});
