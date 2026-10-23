#include "LiquidSystem.h"
#include "SaveArchive.h"
#include "RuntimeWorld.h"

void LiquidSurface::Persist(SaveArchive& a){
 auto solids=m_solids;a(solids);
 if(a.reading){std::string error;SetSolids(solids,error);a.Require(error.empty(),"cannot reconstruct saved liquid exclusion geometry");}
 uint32_t count=uint32_t(cells.size()),faces=uint32_t(discharge.size());a(count,faces);
 a.Require(count==cells.size()&&faces==discharge.size(),"saved surface topology mismatch");
 a(enabled);
 for(auto& cell:cells){a(cell.volume,cell.q,cell.previousQ,cell.velocity);a.Require(cell.volume>=0,"negative saved liquid cell volume");}
 for(auto& flow:discharge)a(flow);
 // Derived aperture/optical/mesh caches are rebuilt from immutable geometry.
}
bool LiquidSystem::PreparePersistence(RuntimeWorld& world,std::string& error){
 if(!world.AdvancedFluidsEnabled()){if(!m_states.empty()||!m_parcels.empty()){error="advanced fluid state requires Project Settings > Experimental > Advanced Fluid Dynamics";return false;}return true;}
 Synchronize(world);
 if(!m_errors.empty()){error="liquid participant: "+m_errors.begin()->second;return false;}
 for(const auto& o:world.ScriptObjects())if((o.liquidBasin||o.liquidContainer)&&!Handle(o.id).id){error="loading";return false;}
 return true;
}
void LiquidSystem::Persist(SaveArchive& a){
 uint32_t count=uint32_t(m_states.size());a(count);a.Require(m_simulationAllowed||count==0,"saved advanced liquid state requires Project Settings > Experimental > Advanced Fluid Dynamics");a.Require(count==m_states.size(),"saved liquid owner count mismatch");
 std::set<SceneObjectId> seen;
 for(uint32_t i=0;i<count;++i){SceneObjectId entity=0;LiquidState* state=nullptr;
  if(!a.reading){auto it=m_states.begin();std::advance(it,i);state=&it->second;entity=state->entity;}
  a(entity);if(a.reading)state=Get(Handle(entity));a.Require(state&&seen.insert(entity).second,"unknown/duplicate liquid owner");auto& s=*state;
  std::string material=s.material.id;double density=s.material.density;a(material,density);a.Require(material==s.material.id&&density==s.material.density,"liquid material mismatch");
  a(s.enabled,s.equilibriumValid,s.volume);a.Require(s.volume>=0,"negative saved owner volume");
  bool surface=bool(s.dynamicSurface);a(surface);a.Require(surface==bool(s.dynamicSurface),"liquid surface participation mismatch");
  if(surface){s.dynamicSurface->Persist(a);a.Require(std::abs(s.dynamicSurface->Volume()-s.volume)<=1e-8*std::max(1.,s.volume),"surface allocation disagrees with owner ledger");}
  if(a.reading)Resolve(s);
 }
 a(m_expected,m_density,m_nextParcel,m_connections);
 uint32_t parcels=uint32_t(m_parcels.size());a(parcels);a.Require(parcels<=4096,"saved parcel limit");
 if(a.reading)m_parcels.clear();
 for(uint32_t i=0;i<parcels;++i){DetachedLiquid p;if(!a.reading){auto it=m_parcels.begin();std::advance(it,i);p=it->second;}
  a(p.id,p.material.id,p.material.density,p.volume,p.position,p.velocity,p.parked,p.source);
  a.Require(p.id&&p.id<m_nextParcel&&p.volume>=0&&p.material.density>0,"invalid saved parcel");
  if(a.reading)a.Require(m_parcels.emplace(p.id,p).second,"duplicate saved parcel");
 }
 if(a.reading){for(auto& [material,_]:m_expected){auto ledger=Accounting(material);a.Require(std::abs(ledger.error)<=ledger.tolerance,"restored liquid accounting mismatch");}m_opticalCache.clear();}
}
