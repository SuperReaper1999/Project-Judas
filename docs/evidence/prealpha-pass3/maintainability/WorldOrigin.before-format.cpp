#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "ResourceManager.h"
#include "UniformGravity.h"
#include "RadicalGravity.h"
#include "SphericalVolume.h"
#include "OrientedGravityVolume.h"
#include "OriginTranslation.h"
#include "OriginLimits.h"
#include <cmath>
namespace {
bool finite(glm::dvec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
}
bool RuntimeWorld::CanChangeOrigin(glm::dvec3 origin,std::string& error)const {
 if(!finite(origin)||glm::any(glm::greaterThan(glm::abs(origin),glm::dvec3(OriginLimits::MaximumComponent)))){error="origin outside finite absolute precision range";return false;}
 if(legacyGameplay){error="live origins require script-owned execution";return false;}
 const auto delta=origin-m_settings.worldOrigin;
 if(!OriginLimits::ValidTranslation(delta)){error="origin translation outside finite native precision range; use intermediate chart anchors";return false;}
 for(auto body:m_physics.AliveBodies())if(glm::length(glm::dvec3(m_physics.GetTransform(body).position)-delta)>8192){error="live collision body must be retired before leaving the 8 km physics chart";return false;}
 // These are ownership restrictions, not a request to silently drop state.
 // Name the actual blocker so projects can retire it before retrying.
 if(m_liquid&&!m_liquid->CanShiftOrigin(delta,error))return false;
 if(!m_fluidVolumes.empty()){error="active legacy fluid has no live-origin contract; unload its fluid owners before changing origin";return false;}
 if(m_emittedParticles){error="active emitted liquid particles have no live-origin contract; retire them before changing origin";return false;}
 for(const auto& [id,o]:m_scriptDefinitions){
  if(!RuntimeDefinition(id))continue;
  // A liquid-only actor may have no rigid/render proxy yet. Until its actual
  // cavity is published we cannot certify the complete volume's chart bounds.
  if(delta!=glm::dvec3(0)&&(o.liquidBasin||o.liquidContainer)&&
     (!m_liquid||!m_liquid->Handle(id).id)){
   error="entity "+std::to_string(id)+" liquid geometry not published; prepare liquid resources before changing origin";
   return false;
  }
  if(o.body||o.characterMotor||o.render||o.light||o.renderCamera){
   auto p=glm::dvec3(o.transform.position);
   const auto body=RuntimeBody(id);if(body.IsValid())p=glm::dvec3(m_physics.GetTransform(body).position);
   if(glm::length(p-delta)>8192){error="spatial entity must be streamed out before leaving the 8 km physics chart: "+std::to_string(id);return false;}
   for(auto parent=o.parent;parent;){auto* ancestor=RuntimeDefinition(parent);if(!ancestor)break;if(glm::length(glm::dvec3(ancestor->transform.position)-delta)>8192){error="active transform ancestor exceeds local precision chart";return false;}parent=ancestor->parent;}
  }
 }
 for(const auto& [id,record]:m_deformables)for(auto point:record.simulation.positions)if(glm::length(point-delta)>8192){error="deformable must remain inside the 8 km physics chart: "+std::to_string(id);return false;}
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
 if(m_liquid)m_liquid->ShiftOrigin(delta);
 if(m_atmosphere)shift(m_atmosphere->frame.originPosition);
 // Combustion owns thermal scalars and generation-safe body handles, not a
 // second copy of body positions. Those handles survive the physics shift.
 for(auto& [_,o]:m_scriptDefinitions){if(!o.planet)shift(o.transform.position);TranslateOriginIntent(o,delta);}
 for(auto& o:m_hierarchy.Objects()){if(!o.parent&&!o.planet)shift(o.transform.position);TranslateOriginIntent(o,delta);}
 for(auto* collection:{&m_entities,&m_extraEntities})for(auto& e:*collection){if(!e.definition.planet){shift(e.definition.transform.position);shift(e.state.position);}TranslateOriginIntent(e.definition,delta);}
 for(auto& body:m_dynamicBodies)body.ShiftOrigin(delta);
 for(auto& [_,c]:m_characters){c.motor.ShiftOrigin(delta);shift(c.previous.position);}
 for(auto& [_,wheel]:m_wheels)for(auto* state:{&wheel.state,&wheel.previous}){shift(state->point);shift(state->center);}
 // Powertrain state is scalar rotational state: no translation or reset.
 m_navigation->ShiftOrigin(delta);
 for(auto& [_,a]:m_animationInstances){
  for(auto* history:{&a.previousWorld,&a.recentWorld})for(auto& matrix:*history)TranslateOriginMatrix(matrix,delta);
  if(a.fullBodyIK){auto& ik=*a.fullBodyIK;TranslateIKTargets(ik.settings.targets,delta);TranslateIKTargets(ik.mapping.settings.targets,delta);TranslateIKTargets(ik.sampledTargets,delta);}
 }
 for(auto& [_,r]:m_ragdolls){shift(r.reference.position);shift(r.previousPresentation.position);shift(r.currentPresentation.position);}
 for(auto& [_,s]:m_physicalAnimations){for(auto* history:{&s.previousReferenceWorld,&s.recentReferenceWorld})for(auto& matrix:*history)TranslateOriginMatrix(matrix,delta);if(s.pending&&s.pending->placement)shift(s.pending->placement->position);}
 for(auto& [_,balance]:m_balances)balance.controller.ShiftOrigin(delta);
 for(auto& [_,record]:m_deformables){record.simulation.ShiftOrigin(delta);for(auto& t:record.targets){TranslateOriginMatrix(t.previous,delta);TranslateOriginMatrix(t.current,delta);}++record.revision;}
 for(auto& emitter:m_particleEmitters){shift(emitter.transform.position);emitter.pool.ShiftOrigin(delta);}
 for(auto& emitter:m_audioEmitters){shift(emitter.transform.position);shift(emitter.lastPosition);}
 for(auto& zone:m_audioZones)shift(zone.transform.position);
 if(m_audioSystem){
  for(const auto& emitter:m_audioEmitters){AudioVoiceSnapshot voice;if(m_audioSystem->Snapshot(emitter.voice,voice)){shift(voice.position);m_audioSystem->SetPosition(emitter.voice,voice.position);}}
  for(auto& entry:m_audioOneShots){AudioVoiceSnapshot voice;if(m_audioSystem->Snapshot(entry.second,voice)){shift(voice.position);m_audioSystem->SetPosition(entry.second,voice.position);}}
 }
 for(auto& b:m_staticBodies)shift(b.position);
 for(auto& r:m_staticRenderables)shift(r.position);
 for(auto& t:m_terrains)shift(t.position);
 for(auto& l:m_staticLights)shift(l.position);
 for(auto& c:m_renderCameras){shift(c.transform.position);if(c.hasClipPlane)shift(c.clipPoint);}
 if(m_audioListener){shift(m_audioListener->transform.position);shift(m_lastListenerPosition);}
 if(view)shift(view->pose.position);
 if(m_players)for(auto& [_,player]:m_players->All())if(player.view)shift(player.view->position);
 for(auto& source:m_pointMassSources)shift(source.position);
 // Publish translated fields in exactly their previous precedence order.
 m_gravityMap=std::move(map);m_gravityFields=std::move(fields);m_gravityVolumes=std::move(volumes);m_gravityRegions=std::move(regions);
 if(m_sceneControl&&m_assets)if(auto* stream=m_sceneControl->StreamingIfLoaded())stream->ShiftOrigin(delta);
 m_settings.worldOrigin=target;m_hierarchy.Settings().worldOrigin=target;++m_originRevision;m_originError.clear();
 if(m_scripts)m_scripts->ShiftOrigin(delta);
 return true;
}
