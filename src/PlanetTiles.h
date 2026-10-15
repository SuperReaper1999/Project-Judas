#pragma once
#include "PlanetTerrain.h"
#include "Renderer.h"
#include "PhysicsWorld.h"
#include <set>
#include <array>
#include <optional>
#include <chrono>
// Runtime residency is owned by the world (or the editor preview). No global
// cache, renderer-owned simulation state, or second terrain contact solver.
class PlanetTiles {
public:
 explicit PlanetTiles(PlanetSettings,PhysicsWorld* physics=nullptr,unsigned layer=0,CategoryMask mask=kAllCategories,CategoryMask tags=0);
 ~PlanetTiles();
 PlanetTiles(const PlanetTiles&)=delete;
 const PlanetSurface surface;
 struct Stats {unsigned leaves=6,resident=0,triangles=0,colliders=0;double maximumPixelError=0,prepareMs=0,collisionMs=0,selectionMs=0,buildMs=0,uploadMs=0;bool budgetLimited=false,collisionReady=false;std::string error;};
 const Stats& State()const{return m_stats;}
 void Prepare(Renderer&,glm::dvec3 eye,glm::dvec3 origin,double fov,unsigned pixels,double dt);
 void Draw(Renderer&,glm::dvec3 origin)const;
 // Explicit physical-interest set. Query readiness before creating/moving a
 // body into a new chart. Visual selection never removes physical support.
 bool Collision(glm::dvec3 absoluteFocus,glm::dvec3 origin,unsigned buildBudget=UINT32_MAX);
 bool Owns(BodyHandle h)const{for(const auto& entry:m_contacts)if(entry.second.body.id==h.id)return true;return false;}
 void ClearCollision();
 void SetTags(CategoryMask);
 std::optional<glm::dvec3> CollisionFocus()const{return m_focus;}
 std::vector<PlanetTileKey> Leaves()const;
 glm::dvec3 RenderPoint(PlanetTileKey,unsigned x,unsigned y)const;
private:
 struct Tile {PlanetTileData data;MeshHandle mesh;double morph=1,uploaded=-1;uint64_t revision=0;std::vector<glm::dvec3> resolved;std::vector<std::pair<PlanetTileKey,double>> dependencies;};
 struct Contact {BodyHandle body;glm::dvec3 anchor;};
 Renderer* m_renderer=nullptr;PhysicsWorld* m_physics=nullptr;
 unsigned m_layer;CategoryMask m_mask,m_tags;
 std::map<PlanetTileKey,Tile> m_tiles;
 std::set<PlanetTileKey> m_leaves;
 std::array<std::vector<std::pair<uint64_t,PlanetTileKey>>,6> m_ranges;
 void IndexLeaves();
 std::map<PlanetTileKey,Contact> m_contacts;
 std::map<PlanetTileKey,std::pair<glm::dvec3,std::shared_ptr<CollisionAsset>>> m_pendingCooks;
 Stats m_stats;
 std::optional<glm::dvec3> m_focus;
 Tile& Ensure(PlanetTileKey,double morph);
 PlanetTileKey LeafAt(glm::dvec3 direction)const;
 std::vector<std::pair<PlanetTileKey,double>> Dependencies(PlanetTileKey)const;
 glm::dvec3 ResolvePoint(PlanetTileKey,unsigned,unsigned)const;
 void Upload(Tile&);
};
