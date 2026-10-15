#include "RuntimeWorld.h"
#include "PlanetTiles.h"
#include "ResourceManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
void RuntimeWorld::PreparePlanets(Renderer& renderer,glm::dvec3 eye,double fov,unsigned pixels,double dt)const{
 for(auto& [id,p]:m_planets)if(RenderVisible(id))p->Prepare(renderer,eye,m_settings.worldOrigin,fov,pixels,dt);
}
void RuntimeWorld::DrawPlanets(Renderer& renderer)const{for(auto& [id,p]:m_planets)if(RenderVisible(id)){renderer.SetRenderLayer(RenderLayerOf(id));p->Draw(renderer,m_settings.worldOrigin);}}
const PlanetTiles* RuntimeWorld::Planet(EntityId id)const{auto it=m_planets.find(id);return RuntimeDefinition(id)&&it!=m_planets.end()?it->second.get():nullptr;}
bool RuntimeWorld::PlanetCollision(EntityId id,glm::dvec3 focus,std::string& error){auto it=m_planets.find(id);if(it==m_planets.end()||!RuntimeDefinition(id)){error="stale or non-planet entity";return false;}bool ready=it->second->Collision(focus,m_settings.worldOrigin,2);error=it->second->State().error;return ready;}
void RuntimeWorld::ClearPlanetCollision(EntityId id){auto it=m_planets.find(id);if(it!=m_planets.end())it->second->ClearCollision();}
