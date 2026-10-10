#pragma once
#include "MeshData.h"
#include "TextureData.h"
#include "AssetDatabase.h"
#include <array>
#include <functional>

// Authoring only. Runtime consumes ordinary immutable mesh/texture/collision assets.
struct TerrainLayer {std::string key,texture;float tileMetres=4;};
struct TerrainSource {
 unsigned version=1,nx=65,nz=65,textureSize=512;
 float width=64,depth=64;
 std::string identity,meshId,textureId,collisionId,productDirectory;
 std::vector<float> heights;
 std::vector<std::array<float,4>> weights;
 std::vector<TerrainLayer> layers;
};
bool ValidateTerrain(const TerrainSource&,std::string&);
TerrainSource CreateTerrain(unsigned nx,unsigned nz,float width,float depth);
TerrainSource ForkTerrain(const TerrainSource&);
std::string SerializeTerrain(const TerrainSource&);
bool ParseTerrain(const std::string&,TerrainSource&,std::string&);
bool LoadTerrain(const std::string&,TerrainSource&,std::string&);
MeshData BuildTerrainMesh(const TerrainSource&);
bool PaintTerrainTexture(const TerrainSource&,const AssetDatabase&,TextureData&,std::string&);
bool TerrainPoint(const TerrainSource&,float x,float z,glm::vec3& point);
bool PickTerrain(const MeshData&,glm::vec3 origin,glm::vec3 direction,glm::vec3& hit);
enum class TerrainBrushKind {Raise,Lower,Flatten,Smooth,Paint};
struct TerrainBrush {TerrainBrushKind kind=TerrainBrushKind::Raise;float radius=3,strength=.5f,falloff=1;unsigned layer=0;};
// A gesture has a frozen flatten height and arc-length dab scheduler, independent
// of event/frame rate. Stationary hold does not repeatedly apply a dab.
class TerrainDraft {
public:
 TerrainSource source;
 bool Open(const std::string&,std::string&);
 bool Save(std::string&);
 bool ExternalChanged()const;
 bool Dirty()const;
 void MarkChanged(bool geometry=true){m_dirty=true;++m_revision;if(geometry)++m_geometryRevision;}
 uint64_t GeometryRevision()const{return m_geometryRevision;}
 bool Begin(glm::vec3,const TerrainBrush&,std::string&);
 bool Continue(glm::vec3,std::string&);
 void End();void Cancel();bool Undo();bool Redo();
 bool Active()const{return m_active;}
 uint64_t Revision()const{return m_revision;}
 const std::string& Path()const{return m_path;}
 void SetPath(std::string path){m_path=std::move(path);}
private:
 void Dab(glm::vec3);
 std::string m_path,m_disk,m_saved;
 TerrainBrush m_brush;glm::vec3 m_last{0};float m_remainder=0,m_flatten=0;
 bool m_active=false,m_dirty=true,m_dirtyBefore=true;uint64_t m_revision=0,m_geometryRevision=0;
 struct Snapshot {std::vector<float> heights;std::vector<std::array<float,4>> weights;};
 Snapshot Capture()const;void Restore(const Snapshot&);
 Snapshot m_before;std::vector<Snapshot> m_undo,m_redo;
 void Push(std::vector<Snapshot>&,Snapshot);
};
struct TerrainCookResult {bool geometryChanged=false,appearanceChanged=false;size_t bytes=0;double milliseconds=0;};
// Captures expected files before work, guards dependencies and source again at
// publication, then uses the existing rollback-capable authoring transaction.
// Cancellation/stale input cannot replace the last good products.
bool CookTerrain(const TerrainSource&,const std::string& sourcePath,const std::string& projectRoot,
                 const AssetDatabase&,TerrainCookResult&,std::string&,const std::function<bool()>& cancelled={});
