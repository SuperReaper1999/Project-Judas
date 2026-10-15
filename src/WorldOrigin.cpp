#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "ResourceManager.h"
#include "UniformGravity.h"
#include "RadicalGravity.h"
#include "SphericalVolume.h"
#include "OrientedGravityVolume.h"
#include <cmath>
namespace {
bool finite(glm::dvec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
}
bool RuntimeWorld::CanChangeOrigin(glm::dvec3 origin,std::string& error)const {
 if(!finite(origin)||glm::any(glm::greaterThan(glm::abs(origin),glm::dvec3(1e12)))){error="origin outside finite absolute precision range";return false;}
 if(legacyGameplay){error="live origins require script-owned execution";return false;}
 const auto delta=origin-m_settings.worldOrigin;
 for(auto body:m_physics.AliveBodies())if(glm::length(glm::dvec3(m_physics.GetTransform(body).position)-delta)>8192){error="live collision body must be retired before leaving the 8 km physics chart";return false;}
 if(m_hasLiquid||!m_fluidVolumes.empty()||m_emittedParticles||m_atmosphere||!m_combustibles.empty()) {error="active liquid/fluid/atmosphere/combustion has no live-origin contract";return false;}
 for(const auto& [id,o]:m_scriptDefinitions){
  if(!RuntimeDefinition(id))continue;
  if(o.deformable||o.ragdoll||o.wheel||o.powertrain||o.navigationSurface||o.navigationAgent||o.navigationObstacle||o.navigationLink||o.navigationModifier||o.audioEmitter||o.audioListener||o.audioZone||o.particleEmitter||o.socket||(o.animation&&(!o.animation->limbs.empty()||o.animation->fullBodyIK))){error="component has no live-origin contract on entity "+std::to_string(id);return false;}
  if(o.body||o.characterMotor||o.render||o.light||o.renderCamera){
   auto p=glm::dvec3(o.transform.position);
   const auto body=RuntimeBody(id);if(body.IsValid())p=glm::dvec3(m_physics.GetTransform(body).position);
   if(glm::length(p-delta)>8192){error="spatial entity must be streamed out before leaving the 8 km physics chart: "+std::to_string(id);return false;}
   for(auto parent=o.parent;parent;){auto* ancestor=RuntimeDefinition(parent);if(!ancestor)break;if(glm::length(glm::dvec3(ancestor->transform.position)-delta)>8192){error="active transform ancestor exceeds local precision chart";return false;}parent=ancestor->parent;}
  }
 }
 if(m_sceneControl&&m_assets)if(auto* stream=m_sceneControl->StreamingIfLoaded())if(!stream->CanChangeOrigin(error))return false;
 error.clear();return true;
}
bool RuntimeWorld::RequestOrigin(glm::dvec3 absolute,std::string& error){
 // Integral power-of-two anchors avoid accumulating sub-ULP shifts. Subtraction
 // is still binary64; a large absolute anchor is never cast to float first.
 auto target=glm::round(absolute/512.)*512.;
 if(!CanChangeOrigin(target,error)){m_originError=error;return false;}
 m_pendingOrigin=target;m_originError.clear();return true;
}
bool RuntimeWorld::CommitOrigin(){
 if(!m_pendingOrigin)return true;
 const auto target=*m_pendingOrigin;
 if(!CanChangeOrigin(target,m_originError))return false;
 const auto delta=target-m_settings.worldOrigin;if(delta==glm::dvec3(0)){m_pendingOrigin.reset();return true;}
 const auto shift=[&](glm::vec3& p){p=glm::vec3(glm::dvec3(p)-delta);};
 // Prepare every allocation before mutating the active chart. The following
 // translation phase only updates existing storage and publishes owned lists.
 auto regions=m_gravityRegions;decltype(m_gravityFields) fields;decltype(m_gravityVolumes) volumes;GravityContextMap map;
 for(auto& region:regions){shift(region.position);const auto& g=region.component;std::unique_ptr<GravityField> field;
  if(g.kind==SceneGravityKind::Radial)field=std::make_unique<RadicalGravity>(region.position,g.magnitude);else field=std::make_unique<UniformGravity>(region.rotation*glm::vec3(0,-g.magnitude,0));
  std::unique_ptr<GravityVolume> volume;if(g.regionShape==SceneRegionShape::Sphere)volume=std::make_unique<SphericalVolume>(region.position,g.regionRadius);else volume=std::make_unique<OrientedGravityVolume>(region.position,region.rotation,g.regionHalfExtents);
  map.AddRegion(*field,*volume);fields.push_back(std::move(field));volumes.push_back(std::move(volume));
 }
 m_pendingOrigin.reset();
 m_physics.ShiftOrigin(delta);
 for(auto& [_,o]:m_scriptDefinitions)if(!o.planet)shift(o.transform.position);
 for(auto& o:m_hierarchy.Objects())if(!o.parent&&!o.planet)shift(o.transform.position);
 for(auto* collection:{&m_entities,&m_extraEntities})for(auto& e:*collection){if(!e.definition.planet){shift(e.definition.transform.position);shift(e.state.position);}}
 for(auto& body:m_dynamicBodies)body.ShiftOrigin(delta);
 for(auto& [_,c]:m_characters){c.motor.ShiftOrigin(delta);shift(c.previous.position);}
 for(auto& [_,a]:m_animationInstances)for(auto* history:{&a.previousWorld,&a.recentWorld})for(auto& matrix:*history){auto p=glm::vec3(matrix[3]);shift(p);matrix[3]=glm::vec4(p,matrix[3].w);}
 for(auto& b:m_staticBodies)shift(b.position);
 for(auto& r:m_staticRenderables)shift(r.position);
 for(auto& t:m_terrains)shift(t.position);
 for(auto& l:m_staticLights)shift(l.position);
 for(auto& c:m_renderCameras){shift(c.transform.position);if(c.hasClipPlane)shift(c.clipPoint);}
 if(view)shift(view->pose.position);
 for(auto& source:m_pointMassSources)shift(source.position);
 // Publish translated fields in exactly their previous precedence order.
 m_gravityMap=std::move(map);m_gravityFields=std::move(fields);m_gravityVolumes=std::move(volumes);m_gravityRegions=std::move(regions);
 if(m_sceneControl&&m_assets)if(auto* stream=m_sceneControl->StreamingIfLoaded())stream->ShiftOrigin(delta);
 m_settings.worldOrigin=target;m_hierarchy.Settings().worldOrigin=target;++m_originRevision;m_originError.clear();return true;
}
