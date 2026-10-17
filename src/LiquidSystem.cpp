#include "PerformanceProfiler.h"
#include "LiquidSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <set>
#include <chrono>
#include <numeric>
#include <functional>
namespace {
using V=glm::dvec3;
std::atomic<std::uint64_t> generations{1};
V local(const SceneTransform& p,V v){return glm::inverse(glm::dquat(p.rotation))*(v-V(p.position));}
V global(const SceneTransform& p,V v){return V(p.position)+glm::dquat(p.rotation)*v;}
GravityEquilibrium localized(GravityEquilibrium e,const SceneTransform& p){e.up=glm::inverse(glm::dquat(p.rotation))*e.up;e.center=local(p,e.center);return e;}
std::array<double,4> coordinates(const LiquidTet& t,const GravityEquilibrium& e){return {e.Coordinate(t[0]),e.Coordinate(t[1]),e.Coordinate(t[2]),e.Coordinate(t[3])};}
bool aabb(const LiquidTet& t,V lo,V hi){V a(1e30),b(-1e30);for(V p:t){a=glm::min(a,p);b=glm::max(b,p);}return a.x<hi.x&&b.x>lo.x&&a.y<hi.y&&b.y>lo.y&&a.z<hi.z&&b.z>lo.z;}
// Openings are authored convex polygons on an exterior cavity face. Orient the
// plane away from the cavity interior; winding does not carry game semantics.
V openingNormal(const LiquidState& s){const auto& o=s.containerSettings.opening;V n=glm::normalize(glm::cross(o[1]-o[0],o[2]-o[0])),center(0);size_t count=0;for(auto& t:s.geometry.cells)for(V v:t){center+=v;++count;}center/=double(count);if(glm::dot(n,center-o[0])>0)n=-n;return n;}
bool throughOpening(const LiquidState& s,V start,V end){start=local(s.pose,start);end=local(s.pose,end);const auto& o=s.containerSettings.opening;V n=openingNormal(s);double a=glm::dot(n,start-o[0]),b=glm::dot(n,end-o[0]);if(a<0||b>0||a-b<1e-12)return false;V p=start+(end-start)*(a/(a-b));double sign=0;for(size_t i=0;i<o.size();++i){double side=glm::dot(glm::cross(o[(i+1)%o.size()]-o[i],p-o[i]),n);if(std::abs(side)<1e-8)continue;if(sign&&side*sign<0)return false;sign=side;}return true;}
// Return physical supporting planes only when the tetrahedral union is convex.
// This is a geometry certificate, not a cavity bounding-box approximation.
std::vector<glm::dvec4> convexPlanes(const LiquidGeometry& geometry){
 if(!LiquidConvexCell(geometry))return {};
 std::vector<glm::dvec4> result;
 for(auto& t:geometry.cells)for(auto plane:LiquidTetPlanes(t)){
  bool supports=true;for(auto& other:geometry.cells)for(V v:other)supports&=glm::dot(V(plane),v)<=plane.w+1e-9;
  if(!supports)continue;
  bool duplicate=false;for(auto existing:result)duplicate|=glm::length(V(existing)-V(plane))<1e-8&&std::abs(existing.w-plane.w)<1e-8;
  if(!duplicate)result.push_back(plane);
 }
 return result;
}
double surfaceQ(const LiquidState& s){return s.equilibrium.kind==GravityEquilibrium::Kind::Plane?s.q+glm::dot(glm::dquat(s.pose.rotation)*s.equilibrium.up,V(s.pose.position)):s.q;}
}
LiquidSystem::LiquidSystem():m_generation(generations.fetch_add(1)){}
bool LiquidSystem::CanShiftOrigin(glm::dvec3 delta,std::string& error)const {
 if(!std::isfinite(glm::dot(delta,delta))){error="nonfinite liquid origin translation";return false;}
 auto inside=[&](V position){position-=delta;return std::isfinite(glm::dot(position,position))&&glm::length(position)<=8192.;};
 for(const auto& [id,state]:m_states){
  for(unsigned corner=0;corner<8;++corner){
   V point(corner&1?state.maximum.x:state.minimum.x,
           corner&2?state.maximum.y:state.minimum.y,
           corner&4?state.maximum.z:state.minimum.z);
   if(!inside(global(state.pose,point))){error="liquid owner "+std::to_string(state.entity)+" must remain inside the 8 km physics chart";return false;}
  }
 }
 for(const auto& [id,parcel]:m_parcels)if(!inside(V(parcel.position))){error="detached liquid parcel "+std::to_string(id)+" must remain inside the 8 km physics chart";return false;}
 error.clear();return true;
}
void LiquidSystem::ShiftOrigin(glm::dvec3 delta) {
 for(auto& [id,state]:m_states)state.pose.position=glm::vec3(V(state.pose.position)-delta);
 for(auto& [id,parcel]:m_parcels)parcel.position=glm::vec3(V(parcel.position)-delta);
 // Optical cells contain derived world-space planes. Rebuild them lazily;
 // immutable capacity geometry and all authoritative surface history stay local.
 m_opticalCache.clear();
}
LiquidHandle LiquidSystem::Insert(LiquidState state,std::string& error){if(m_entities.count(state.entity)){error="duplicate liquid owner";return {};}if(!std::isfinite(state.volume+state.capacity+state.material.density)||state.material.id.empty()||state.material.density<=0||state.capacity<=0||state.volume<0||state.volume>state.capacity+1e-12){error="initial liquid exceeds capacity";return {};}if(m_density.count(state.material.id)&&m_density[state.material.id]!=state.material.density){error="material identity has contradictory density";return {};}state.handle={m_next++,m_generation};m_density[state.material.id]=state.material.density;m_expected[state.material.id]+=state.volume;auto h=state.handle;Resolve(state);m_entities[state.entity]=h;m_states[h.id]=std::move(state);return h;}
LiquidHandle LiquidSystem::AddBasin(SceneObjectId entity,const LiquidBasinSettings& a,const SceneTransform& pose,std::shared_ptr<const LiquidBasinData> data,std::string& error){if(!data||data->curve.empty()){error="missing capacity data";return {};}LiquidState s;s.entity=entity;s.enabled=a.enabled;s.material=a.material;s.volume=a.initialVolume;s.capacity=data->capacity;s.geometry=data->geometry;s.equilibrium=data->equilibrium;s.basin=data;s.pose=pose;if(data->dynamicSurface)s.dynamicSurface=std::make_shared<LiquidSurface>(data->dynamicSurface,a.initialVolume);return Insert(std::move(s),error);}
LiquidHandle LiquidSystem::AddContainer(SceneObjectId entity,const LiquidContainerSettings& a,const SceneTransform& p,const LiquidGeometry& g,const GravityEquilibrium& e,std::string& error){if(!ValidateLiquidGeometry(g,error))return {};if(!std::isfinite(e.magnitude)||e.magnitude<=0){error="container requires nonzero gravity";return {};}if(a.opening.size()<3){error="opening needs a polygon";return {};}V n=glm::cross(a.opening[1]-a.opening[0],a.opening[2]-a.opening[0]);if(glm::length(n)<1e-10){error="degenerate container opening";return {};}n=glm::normalize(n);double sign=0;for(auto& t:g.cells)for(V p:t){double d=glm::dot(n,p-a.opening[0]);if(std::abs(d)<1e-8)continue;if(sign&&d*sign<0){error="opening must be an exterior cavity face";return {};}sign=d;}for(V p:a.opening)if(std::abs(glm::dot(n,p-a.opening[0]))>1e-7){error="opening must be planar";return {};}double turn=0;for(size_t i=0;i<a.opening.size();++i){V u=a.opening[(i+1)%a.opening.size()]-a.opening[i],v=a.opening[(i+2)%a.opening.size()]-a.opening[(i+1)%a.opening.size()];double d=glm::dot(glm::cross(u,v),n);if(std::abs(d)<1e-10)continue;if(turn&&d*turn<0){error="opening must be convex and ordered";return {};}turn=d;}LiquidState s;s.entity=entity;s.container=true;s.enabled=a.enabled;s.material=a.material;s.volume=a.initialVolume;s.pose=p;s.equilibrium=localized(e,p);s.equilibrium.kind=GravityEquilibrium::Kind::Plane;s.equilibrium.up=glm::inverse(glm::dquat(p.rotation))*e.Up(V(p.position));s.geometry=g;s.containerSettings=a;for(auto& t:g.cells){auto q=coordinates(t,s.equilibrium);s.capacity+=LiquidClip(t,q,*std::max_element(q.begin(),q.end())).volume;}return Insert(std::move(s),error);}
LiquidState* LiquidSystem::Get(LiquidHandle h){auto it=m_states.find(h.id);return h.generation==m_generation&&it!=m_states.end()?&it->second:nullptr;}
const LiquidState* LiquidSystem::Get(LiquidHandle h)const{auto it=m_states.find(h.id);return h.generation==m_generation&&it!=m_states.end()?&it->second:nullptr;}
LiquidHandle LiquidSystem::Handle(SceneObjectId id)const{auto it=m_entities.find(id);return it==m_entities.end()?LiquidHandle{}:it->second;}
bool LiquidSystem::Remove(LiquidHandle h){auto* s=Get(h);if(!s)return false;if(s->volume>0){auto id=m_nextParcel++;m_parcels[id]={id,s->material,s->volume,s->pose.position,{0,0,0},true};}m_entities.erase(s->entity);m_states.erase(h.id);for(auto it=m_opticalCache.begin();it!=m_opticalCache.end();)if(it->first.first==h.id)it=m_opticalCache.erase(it);else ++it;return true;}
bool LiquidSystem::SetEnabled(LiquidHandle h,bool enabled){auto* s=Get(h);if(!s)return false;s->enabled=enabled;return true;}
void LiquidSystem::Resolve(LiquidState& s){
 if(s.dynamicSurface&&s.capacityResolved){s.q=LiquidInverse(*s.basin,s.volume);s.capacity=s.dynamicSurface->Capacity();s.stableCapacity=s.capacity;s.surface.vertices.clear();return;}
 double lo=1e30,hi=-1e30;s.minimum=V(1e30);s.maximum=V(-1e30);for(auto& t:s.geometry.cells)for(V p:t){lo=std::min(lo,s.equilibrium.Coordinate(p));hi=std::max(hi,s.equilibrium.Coordinate(p));s.minimum=glm::min(s.minimum,p);s.maximum=glm::max(s.maximum,p);}s.capacityResolved=true;s.stableCapacity=s.capacity;if(s.container){double lip=hi;for(V p:s.containerSettings.opening)lip=std::min(lip,s.equilibrium.Coordinate(p));s.stableCapacity=LiquidCapacity(s.geometry,s.equilibrium,lip);double a=lo,b=hi;for(int i=0;i<40;++i){double q=(a+b)*.5;if(LiquidCapacity(s.geometry,s.equilibrium,q)<s.volume)a=q;else b=q;}s.q=(a+b)*.5;}else s.q=LiquidInverse(*s.basin,s.volume);
 if(s.dynamicSurface){s.capacity=s.dynamicSurface->Capacity();s.stableCapacity=s.capacity;s.surface.vertices.clear();return;}
 s.surface.vertices.clear();if(s.volume>1e-14)for(auto& t:s.geometry.cells)LiquidClip(t,coordinates(t,s.equilibrium),s.q,&s.surface);}
double LiquidSystem::Transfer(LiquidHandle a,LiquidHandle b,double request,std::optional<glm::vec3> sourcePoint,std::optional<glm::vec3> targetPoint){
 JUDAS_PROFILE_SCOPE("Conserved liquid transfer");
 auto* source=Get(a);auto* target=Get(b);
 if(!source||!target||a==b||!source->enabled||!target->enabled||!source->equilibriumValid||!target->equilibriumValid||source->material.id!=target->material.id||!std::isfinite(request)||request<0)return 0;
 double delta=std::min({request,source->volume,std::max(0.,target->capacity-target->volume)});
 std::optional<V> sp=sourcePoint?std::optional<V>(local(source->pose,V(*sourcePoint))):std::nullopt;
 std::optional<V> tp=targetPoint?std::optional<V>(local(target->pose,V(*targetPoint))):std::nullopt;
 if(source->dynamicSurface)delta=std::min(delta,source->dynamicSurface->Plan(-delta,sp).amount);
 if(target->dynamicSurface)delta=std::min(delta,target->dynamicSurface->Plan(delta,tp).amount);
 if(delta<=0)return 0;
 if(source->dynamicSurface)source->dynamicSurface->Apply(source->dynamicSurface->Plan(-delta,sp),false);
 if(target->dynamicSurface)target->dynamicSurface->Apply(target->dynamicSurface->Plan(delta,tp),true);
 source->volume-=delta;target->volume+=delta;Resolve(*source);Resolve(*target);return delta;
}
double LiquidSystem::Emit(LiquidHandle h,double request,glm::vec3 p,glm::vec3 v){
 JUDAS_PROFILE_SCOPE("Conserved parcel emission");
 auto* s=Get(h);if(!s||!s->enabled||!s->equilibriumValid||!std::isfinite(request)||request<0||!std::isfinite(glm::dot(p,p)+glm::dot(v,v)))return 0;
 double amount=std::min(request,s->volume);std::optional<V> point=local(s->pose,V(p));
 if(s->dynamicSurface)amount=std::min(amount,s->dynamicSurface->Plan(-amount,point).amount);
 if(amount<=0)return 0;
 if(s->dynamicSurface)s->dynamicSurface->Apply(s->dynamicSurface->Plan(-amount,point),false);
 auto id=m_nextParcel++;m_parcels[id]={id,s->material,amount,p,v,false,s->entity};s->volume-=amount;Resolve(*s);return amount;
}
double LiquidSystem::Receive(std::uint64_t id,LiquidHandle h,double maximum,std::optional<glm::vec3> point){
 JUDAS_PROFILE_SCOPE("Conserved parcel reception");
 auto it=m_parcels.find(id);auto* s=Get(h);if(it==m_parcels.end()||!s||!s->enabled||!s->equilibriumValid||s->material.id!=it->second.material.id||!std::isfinite(maximum)||maximum<0)return 0;
 double amount=std::min({maximum,it->second.volume,std::max(0.,s->capacity-s->volume)});
 std::optional<V> location=point?std::optional<V>(local(s->pose,V(*point))):std::nullopt;
 if(s->dynamicSurface)amount=std::min(amount,s->dynamicSurface->Plan(amount,location).amount);
 if(amount<=0)return 0;
 if(s->dynamicSurface)s->dynamicSurface->Apply(s->dynamicSurface->Plan(amount,location),true);
 s->volume+=amount;it->second.volume-=amount;if(it->second.volume==0)m_parcels.erase(it);Resolve(*s);return amount;
}
bool LiquidSystem::SurfaceImpulse(LiquidHandle h,glm::vec3 point,glm::vec3 impulse){
 auto* s=Get(h);if(!s||!s->enabled||!s->dynamicSurface||!s->dynamicSurface->enabled||!std::isfinite(glm::dot(point,point)+glm::dot(impulse,impulse)))return false;
 s->dynamicSurface->Impulse(local(s->pose,V(point)),glm::inverse(glm::dquat(s->pose.rotation))*V(impulse),s->material.density);Resolve(*s);return true;
}
bool LiquidSystem::SurfaceEnabled(LiquidHandle h,bool enabled){auto* s=Get(h);if(!s||!s->dynamicSurface)return false;s->dynamicSurface->enabled=enabled;return true;}
LiquidAccounting LiquidSystem::Accounting(const std::string& name)const{LiquidAccounting a;for(const auto& [id,s]:m_states)if(s.material.id==name){if(s.container)a.containers+=s.volume;else a.reservoirs+=s.volume;}for(const auto& [id,p]:m_parcels)if(p.material.id==name)a.detached+=p.volume;a.total=a.reservoirs+a.containers+a.detached;auto it=m_expected.find(name);if(it!=m_expected.end())a.expected=it->second;a.error=a.total-a.expected;a.tolerance=std::max(1e-12,64*std::numeric_limits<double>::epsilon()*std::max(1.,a.expected)*(m_states.size()+m_parcels.size()+1));return a;}
std::optional<LiquidSample> LiquidSystem::Sample(glm::vec3 p,LiquidHandle excluded,float alpha)const{
 for(auto& [id,s]:m_states){
  if(!s.enabled||!s.equilibriumValid||s.volume<=0||s.handle==excluded)continue;
  V v=local(s.pose,V(p));if(v.x<s.minimum.x||v.y<s.minimum.y||v.z<s.minimum.z||v.x>s.maximum.x||v.y>s.maximum.y||v.z>s.maximum.z)continue;
  int cell=s.dynamicSurface?s.dynamicSurface->Cell(v):-1;
  if(s.dynamicSurface&&cell<0)continue;
  double level=s.dynamicSurface?s.dynamicSurface->Coordinate(unsigned(cell),alpha):s.q;
  const auto& geometry=s.dynamicSurface?s.dynamicSurface->cells[cell].storage.geometry:s.geometry;
  for(auto& t:geometry.cells){bool inside=false;double q=LiquidCoordinate(t,s.equilibrium,v,inside);
   if(inside&&q<=level+1e-9){LiquidSample sample;sample.handle=s.handle;sample.entity=s.entity;sample.material=s.material;
    sample.depth=std::max(0.,level-s.equilibrium.Coordinate(v));sample.coordinate=level;V up=s.equilibrium.Up(v);
    sample.up=glm::vec3(glm::dquat(s.pose.rotation)*up);
    V normal=up;V surface=v+up*sample.depth;
    if(s.dynamicSurface){auto planes=LiquidOccupiedPlanes(t,s.equilibrium,level);if(planes.size()==5){auto cap=planes.back();normal=glm::normalize(V(cap));double projection=glm::dot(V(cap),up);if(projection>1e-12)surface=v+up*((cap.w-glm::dot(V(cap),v))/projection);}}
    sample.normal=glm::vec3(glm::dquat(s.pose.rotation)*normal);
    sample.surfacePoint=glm::vec3(global(s.pose,surface));
    if(s.dynamicSurface)sample.velocity=glm::vec3(glm::dquat(s.pose.rotation)*s.dynamicSurface->cells[cell].velocity);
    return sample;
   }
  }
 }return {};
}
bool LiquidSystem::SetContainerPose(LiquidHandle h,const SceneTransform& pose,const GravityEquilibrium& e,std::string& error){auto* s=Get(h);if(!s||!s->container){error="invalid container handle";return false;}if(!std::isfinite(e.magnitude)||e.magnitude<=0){s->equilibriumValid=false;s->pose=pose;s->surface.vertices.clear();error="container requires nonzero gravity";return false;}s->equilibriumValid=true;s->pose=pose;s->equilibrium=localized(e,pose);s->equilibrium.kind=GravityEquilibrium::Kind::Plane;s->equilibrium.up=glm::inverse(glm::dquat(pose.rotation))*e.Up(V(pose.position));Resolve(*s);return true;}
void LiquidSystem::Synchronize(RuntimeWorld& world){auto objects=world.ScriptObjects();for(auto& o:objects)if(o.liquidBasin||o.liquidContainer){if(Handle(o.id).id)continue;auto* assets=world.Resources();if(!assets||!assets->Assets())continue;std::string error;auto resource=assets->GetLiquid(o.liquidBasin?o.liquidBasin->asset:o.liquidContainer->geometry,error);if(!resource){if(error!="loading")m_errors[o.id]=error;continue;}auto pose=world.PresentedTransform(o.id,o.transform,1);if(o.liquidBasin){if(!resource->basin){m_errors[o.id]="basin asset is a cavity, not a baked curve";continue;}AddBasin(o.id,*o.liquidBasin,pose,resource->basin,error);}else{GravityEquilibrium eq;if(!world.Gravity().Equilibrium(pose.position,eq)){m_errors[o.id]="unsupported container gravity equilibrium";continue;}AddContainer(o.id,*o.liquidContainer,pose,resource->geometry,eq,error);}if(!error.empty())m_errors[o.id]=error;else m_errors.erase(o.id);}
 std::vector<LiquidHandle> dead;for(const auto& [id,s]:m_states){auto* e=world.FindEntity(s.entity);if(!e||e->lifecycle==EntityLifecycle::Destroyed)dead.push_back(s.handle);}for(auto h:dead)Remove(h);}
LiquidSubmersion LiquidSystem::Submerged(const LiquidGeometry& geometry)const{
 JUDAS_PROFILE_SCOPE("Liquid submersion integration");
 LiquidSubmersion out;V first(0),force(0);
 for(auto& [id,s]:m_states)if(s.enabled&&!s.container&&s.volume>0)for(auto worldTet:geometry.cells){
  LiquidTet body;for(int i=0;i<4;++i)body[i]=local(s.pose,worldTet[i]);if(!aabb(body,s.minimum,s.maximum))continue;
  // Loading measures displaced volume in the occupied envelope, before this
  // body's own excluded solid space. Otherwise buoyancy would always be zero.
  V bodyMinimum(1e30),bodyMaximum(-1e30);for(V p:body){bodyMinimum=glm::min(bodyMinimum,p);bodyMaximum=glm::max(bodyMaximum,p);}
  auto accumulate=[&](LiquidMoment m,glm::vec3 flow){if(m.volume>0){V c=global(s.pose,m.first/m.volume);
   out.velocity+=flow*float(m.volume);out.volume+=m.volume;out.displacedMass+=s.material.density*m.volume;first+=c*m.volume;
   force+=glm::dquat(s.pose.rotation)*s.equilibrium.Up(m.first/m.volume)*(s.material.density*s.equilibrium.magnitude*m.volume);
  }};
  auto add=[&](const LiquidGeometry& domain,double level,glm::vec3 flow){for(auto& basin:domain.cells){
   V lo(1e30),hi(-1e30);for(V v:basin){lo=glm::min(lo,v);hi=glm::max(hi,v);}if(!aabb(body,lo,hi))continue;
   accumulate(LiquidIntersection(body,basin,s.equilibrium,level),flow);
  }};
  auto addCached=[&](const LiquidSurfaceCellData& cell,double level,glm::vec3 flow){for(auto& tet:cell.loadingGeometry){
   if(glm::any(glm::lessThanEqual(bodyMaximum,tet.minimum))||glm::any(glm::greaterThanEqual(bodyMinimum,tet.maximum)))continue;
   accumulate(LiquidIntersection(body,tet,level),flow);
  }};
  if(s.dynamicSurface){auto& data=*s.dynamicSurface->data;glm::dvec2 lo(1e30),hi(-1e30);bool cap=true;
   for(V p:body){if(data.equilibrium.kind==GravityEquilibrium::Kind::Radius&&glm::dot(p-data.equilibrium.center,data.axis)<=0){cap=false;break;}auto chart=data.Chart(p);lo=glm::min(lo,chart);hi=glm::max(hi,chart);}
   unsigned firstX=0,firstY=0,lastX=data.settings.columns-1,lastY=data.settings.rows-1;
   if(cap){if(glm::any(glm::lessThan(hi,data.minimum))||glm::any(glm::greaterThan(lo,data.maximum)))continue;
    auto lower=(lo-data.minimum)/(data.maximum-data.minimum),upper=(hi-data.minimum)/(data.maximum-data.minimum);
    firstX=unsigned(std::clamp(int(std::floor(lower.x*data.settings.columns)),0,int(lastX)));lastX=unsigned(std::clamp(int(std::floor(upper.x*data.settings.columns)),0,int(lastX)));
    firstY=unsigned(std::clamp(int(std::floor(lower.y*data.settings.rows)),0,int(lastY)));lastY=unsigned(std::clamp(int(std::floor(upper.y*data.settings.rows)),0,int(lastY)));
   }
   for(unsigned y=firstY;y<=lastY;++y)for(unsigned x=firstX;x<=lastX;++x){int i=data.grid[y*data.settings.columns+x];if(i<0||s.dynamicSurface->cells[i].volume<=0)continue;addCached(data.cells[i],s.dynamicSurface->cells[i].q,glm::vec3(glm::dquat(s.pose.rotation)*s.dynamicSurface->cells[i].velocity));}
  }
  else add(s.geometry,s.q,{0,0,0});
 }
 if(out.volume>0){out.center=glm::vec3(first/out.volume);out.velocity/=float(out.volume);}
 out.buoyancy=glm::vec3(force);return out;
}
void LiquidSystem::BeginStep(){for(auto& [id,s]:m_states)if(s.dynamicSurface)s.dynamicSurface->BeginStep();}
void LiquidSystem::Update(RuntimeWorld& world,double dt){
    JUDAS_PROFILE_SCOPE("Liquid owners update");if(dt<=0||!std::isfinite(dt))return;m_connections.clear();auto begin=std::chrono::steady_clock::now(),phase=begin;m_stepTimes={};auto elapsed=[&](){auto now=std::chrono::steady_clock::now();double seconds=std::chrono::duration<double>(now-phase).count();phase=now;return seconds;};Synchronize(world);std::set<std::uint64_t> failedGeometry;
 static ProfileLabel containerLabel("Liquid containers"), connectionLabel("Liquid paired connections"), parcelLabel("Liquid parcels"), solidLabel("Liquid solid geometry batch"), surfaceLabel("Liquid surface solve batch"), loadLabel("Liquid body loading");
 ProfileScope containerScope(containerLabel);
 JUDAS_PROFILE_COUNTER("Liquid container batches",1,ProfileCounterMode::Sum);
 JUDAS_PROFILE_COUNTER("Liquid owners",double(m_states.size()),ProfileCounterMode::Latest);
 JUDAS_PROFILE_COUNTER("Liquid parcels",double(m_parcels.size()),ProfileCounterMode::Latest);


 for(auto& [id,s]:m_states)if(s.enabled&&s.container){auto* o=world.RuntimeDefinition(s.entity);if(!o)continue;auto pose=world.PresentedTransform(s.entity,o->transform,1);GravityEquilibrium eq;std::string error;if(!world.Gravity().Equilibrium(pose.position,eq)||!SetContainerPose(s.handle,pose,eq,error)){s.equilibriumValid=false;s.pose=pose;s.surface.vertices.clear();m_errors[s.entity]="unsupported container gravity; retained volume";continue;}m_errors.erase(s.entity);
  // Vented cavity: only opening connectivity permits equalisation. Its external
  // head determines a TARGET volume; transfer the delta, never copy overlap.
  // Rim vertices lie on solid wall boundaries. Sample strictly inside the
  // actual open polygon, rather than treating a wall corner as an aperture.
  V openingCentre(0);for(V p:s.containerSettings.opening)openingCentre+=p;openingCentre/=double(s.containerSettings.opening.size());
  std::vector<V> probes{openingCentre};for(V p:s.containerSettings.opening)probes.push_back((p+openingCentre)*.5);
  std::optional<LiquidSample> external;for(V aperture:probes){auto q=Sample(glm::vec3(global(s.pose,aperture)),s.handle);if(q&&q->handle.id!=s.handle.id){external=q;break;}}
  if(external){auto* source=Get(external->handle);if(source&&source->material.id==s.material.id){V point=local(s.pose,V(external->surfacePoint));double target=LiquidCapacity(s.geometry,s.equilibrium,s.equilibrium.Coordinate(point));double head=std::max(0.,target-s.volume);double flow=s.containerSettings.discharge*s.containerSettings.openingArea*std::sqrt(2*eq.magnitude*std::max(.001,external->depth))*dt;Transfer(source->handle,s.handle,std::min(head,flow),external->surfacePoint);}}
  double excess=std::max(0.,s.volume-s.stableCapacity);if(excess>1e-12){V lip=s.containerSettings.opening.front();for(V v:s.containerSettings.opening)if(s.equilibrium.Coordinate(v)<s.equilibrium.Coordinate(lip))lip=v;double head=std::max(0.,s.q-s.equilibrium.Coordinate(lip));double amount=std::min(excess,s.containerSettings.discharge*s.containerSettings.openingArea*std::sqrt(2*eq.magnitude*head)*dt);auto body=world.RuntimeBody(s.entity);glm::vec3 velocity(0);if(world.Physics().IsDynamicBody(body))velocity=world.Physics().GetLinearVelocity(body)+glm::cross(world.Physics().GetAngularVelocity(body),glm::vec3(global(s.pose,lip))-pose.position);const V outward=glm::dquat(s.pose.rotation)*openingNormal(s);velocity+=glm::vec3(outward)*float(std::sqrt(2*eq.magnitude*head));Emit(s.handle,amount,glm::vec3(global(s.pose,lip)+outward*.002),velocity);}}
 containerScope.End(); ProfileScope connectionScope(connectionLabel);
 for(auto& o:world.ScriptObjects())if(o.liquidConnection){auto c=*o.liquidConnection;m_connections[o.id]=false;if(!c.enabled)continue;auto* a=Get(Handle(c.source));auto* b=Get(Handle(c.destination));if(!a||!b||!a->basin||!b->basin||!a->enabled||!b->enabled||a->material.id!=b->material.id)continue;GravityEquilibrium eq;if(!world.Gravity().Equilibrium(o.transform.position,eq))continue;if(!localized(eq,a->pose).Equivalent(a->equilibrium)||!localized(eq,b->pose).Equivalent(b->equilibrium)){m_errors[o.id]="spill basins must share one supported equilibrium field";continue;}double threshold=eq.Coordinate(V(o.transform.position));auto endpoint=[&](const LiquidState& owner){if(!owner.dynamicSurface)return o.transform.position;
   V position=local(owner.pose,V(o.transform.position));int cell=owner.dynamicSurface->Cell(position);
   if(cell<0){auto chart=owner.dynamicSurface->data->Chart(position);double nearest=1e30;for(unsigned i=0;i<owner.dynamicSurface->cells.size();++i){auto delta=owner.dynamicSurface->data->cells[i].chart-chart;double distance=glm::dot(delta,delta);if(distance<nearest){nearest=distance;cell=int(i);}}}
   return glm::vec3(global(owner.pose,owner.dynamicSurface->data->Point(owner.dynamicSurface->data->cells[cell].chart,owner.dynamicSurface->cells[cell].q)));
  };
  auto pa=endpoint(*a),pb=endpoint(*b);
  auto headAt=[&](const LiquidState& owner,glm::vec3 point){if(!owner.dynamicSurface)return surfaceQ(owner);int cell=owner.dynamicSurface->Cell(local(owner.pose,V(point)));return owner.dynamicSurface->cells[cell].q+surfaceQ(owner)-owner.q;};double qa=headAt(*a,pa),qb=headAt(*b,pb);if(std::max(qa,qb)<=threshold)continue;m_connections[o.id]=true;if(qb>qa){if(!c.bidirectional)continue;std::swap(a,b);std::swap(qa,qb);std::swap(pa,pb);}double flow=c.discharge*c.openingArea*std::sqrt(2*eq.magnitude*std::max(0.,qa-std::max(qb,threshold)))*dt;double low=0,high=std::min({a->volume,b->capacity-b->volume,flow});
  // Bound by equalisation, so large timesteps cannot ping-pong reservoirs.
  if(!a->dynamicSurface&&!b->dynamicSurface)for(int i=0;i<30;++i){double delta=(low+high)*.5;double aq=LiquidInverse(*a->basin,a->volume-delta),bq=LiquidInverse(*b->basin,b->volume+delta);double ao=surfaceQ(*a)-a->q,bo=surfaceQ(*b)-b->q;if(aq+ao>bq+bo&&aq+ao>threshold)low=delta;else high=delta;}
  Transfer(a->handle,b->handle,(a->dynamicSurface||b->dynamicSurface)?high:low,pa,pb);}
 // Test the existing authoritative solid query FIRST. Receiving volume cannot
 // pass through a wall merely because another cavity lies behind it.
 connectionScope.End(); ProfileScope parcelScope(parcelLabel);
 std::vector<std::uint64_t> parcelIds;for(const auto& [id,p]:m_parcels)parcelIds.push_back(id);
 for(auto id:parcelIds){auto it=m_parcels.find(id);if(it==m_parcels.end()||it->second.parked)continue;auto& p=it->second;const auto start=p.position;p.velocity+=world.Gravity().Sample(start)*float(dt);const auto end=start+p.velocity*float(dt);const double distance=glm::length(end-start);PhysicsQueryFilter filter;auto sourceBody=world.RuntimeBody(p.source);if(sourceBody.id!=BodyHandle::kInvalidId)filter.ignoredBodies.push_back(sourceBody);auto hit=distance>1e-8?world.Physics().Raycast(start,(end-start)/float(distance),float(distance),filter):PhysicsCastHit{};const auto limit=hit.hit?hit.point:end;double travel=glm::length(limit-start);bool received=false;int samples=std::min(256,std::max(1,int(std::ceil(travel/.03))));
  for(int i=1;i<=samples&&!received;++i){auto point=glm::mix(start,limit,float(i)/samples);for(auto& [sid,s]:m_states){if(!s.enabled||!s.equilibriumValid||s.material.id!=p.material.id||s.material.density!=p.material.density||(s.entity==p.source&&s.container)||s.capacity<=s.volume)continue;V v=local(s.pose,V(point));if(v.x<s.minimum.x||v.y<s.minimum.y||v.z<s.minimum.z||v.x>s.maximum.x||v.y>s.maximum.y||v.z>s.maximum.z)continue;if(s.container&&!throughOpening(s,V(start),V(point)))continue;int cell=s.dynamicSurface?s.dynamicSurface->Cell(v):-1;
   if(s.dynamicSurface&&cell<0)continue;
   const auto& geometry=s.dynamicSurface?s.dynamicSurface->cells[cell].storage.geometry:s.geometry;
   for(auto& t:geometry.cells){bool inside;double coordinate=LiquidCoordinate(t,s.equilibrium,v,inside);
    bool reached=!s.dynamicSurface||coordinate<=s.dynamicSurface->cells[cell].q+1e-8;
    if(s.dynamicSurface&&s.dynamicSurface->cells[cell].volume==0)reached=hit.hit&&i==samples&&glm::dot(hit.normal,glm::vec3(glm::dquat(s.pose.rotation)*s.equilibrium.Up(v)))>.5f;
    if(inside&&reached){const double volume=p.volume;auto velocity=p.velocity;double accepted=Receive(id,s.handle,volume,point);if(accepted>0){SurfaceImpulse(s.handle,point,velocity*float(accepted*s.material.density));received=true;}break;}
   }if(received)break;}}
  it=m_parcels.find(id);if(it==m_parcels.end())continue;auto& remainder=it->second;if(hit.hit){remainder.position=hit.point;remainder.velocity={0,0,0};remainder.parked=true;}else remainder.position=end;
 }

 parcelScope.End(); ProfileScope solidScope(solidLabel);
 m_stepTimes.containers=elapsed();
 for(auto& [id,s]:m_states)if(s.enabled&&s.dynamicSurface){
  std::vector<std::vector<glm::dvec4>> solids,dynamicSolids;
  for(auto& o:world.ScriptObjects())if(o.body){
   auto body=world.RuntimeBody(o.id);if(!body.IsValid()||!world.Physics().IsBodyEnabled(body)||world.Physics().IsBodySensor(body))continue;
   auto pose=world.Physics().GetTransform(body);
   auto box=[&](glm::vec3 center,glm::vec3 half,glm::quat child=glm::quat(1,0,0,0)){
    V lower(1e30),upper(-1e30);
    for(unsigned corner=0;corner<8;++corner){V p=V(center)+glm::dquat(child)*V(corner&1?half.x:-half.x,corner&2?half.y:-half.y,corner&4?half.z:-half.z);p=local(s.pose,V(pose.position)+glm::dquat(pose.rotation)*p);lower=glm::min(lower,p);upper=glm::max(upper,p);}
    if(glm::any(glm::lessThanEqual(upper,s.minimum))||glm::any(glm::greaterThanEqual(lower,s.maximum)))return;
    std::vector<glm::dvec4> planes;
    for(unsigned axis=0;axis<3;++axis)for(double sign:{-1.,1.}){V normal(0);normal[axis]=sign;normal=glm::inverse(glm::dquat(s.pose.rotation))*glm::dquat(pose.rotation)*glm::dquat(child)*normal;
     V middle=local(s.pose,V(pose.position)+glm::dquat(pose.rotation)*V(center));planes.push_back({normal,glm::dot(normal,middle)+half[axis]});
    }(world.Physics().IsDynamicBody(body)?dynamicSolids:solids).push_back(std::move(planes));
   };
   if(o.body->shape==SceneShape::Box)box({},o.body->halfExtents);
   else if(o.body->shape==SceneShape::Compound)for(auto& b:o.body->compoundBoxes)if(b.type==ShapeType::Box)box(b.localCenter,b.halfExtents,b.rotation);
  }
  const unsigned staticCount=unsigned(solids.size());solids.insert(solids.end(),dynamicSolids.begin(),dynamicSolids.end());
  // Exclude the actual owned wet cavity, not a hollow bucket's bounding hull.
  // Opening samples still see the surrounding reservoir for paired scooping.
  for(auto& [otherId,c]:m_states)if(c.container&&c.enabled&&c.equilibriumValid&&c.volume>0){
   auto addWet=[&](std::vector<glm::dvec4> planes){
    for(auto& plane:planes){V normal=glm::inverse(glm::dquat(s.pose.rotation))*glm::dquat(c.pose.rotation)*V(plane);
     V origin=local(s.pose,V(c.pose.position));plane={normal,plane.w+glm::dot(normal,origin)};
    }solids.push_back(std::move(planes));
   };
   V lo(1e30),hi(-1e30);for(auto& tet:c.geometry.cells)for(V p:tet){V v=local(s.pose,global(c.pose,p));lo=glm::min(lo,v);hi=glm::max(hi,v);}
   if(glm::any(glm::lessThanEqual(hi,s.minimum))||glm::any(glm::greaterThanEqual(lo,s.maximum)))continue;
   auto cavity=convexPlanes(c.geometry);
   if(!cavity.empty()){cavity.push_back({c.equilibrium.up,c.q});addWet(std::move(cavity));}
   else for(auto& tet:c.geometry.cells){auto planes=LiquidOccupiedPlanes(tet,c.equilibrium,c.q);if(!planes.empty())addWet(std::move(planes));}
  }
  std::string error;double excess=s.dynamicSurface->SetSolids(solids,error,staticCount);
  if(excess<0){m_errors[s.entity]=error;failedGeometry.insert(id);continue;}
  // If the wet connected component has no spare space, park conserved overflow
  // at that cell's surface. It remains in M54 accounting; never discard it.
  if(excess>0)for(unsigned i=0;i<s.dynamicSurface->cells.size();++i){auto& c=s.dynamicSurface->cells[i];double amount=std::max(0.,c.volume-c.storage.capacity);
   if(amount<=0)continue;
   V point=s.dynamicSurface->data->Point(s.dynamicSurface->data->cells[i].chart,c.q);
   double emitted=Emit(s.handle,amount,glm::vec3(global(s.pose,point)),{0,0,0});
   if(emitted>0)m_parcels.rbegin()->second.parked=true;
  }
  s.capacity=s.dynamicSurface->Capacity();s.stableCapacity=s.capacity;
 }


 solidScope.End(); ProfileScope surfaceScope(surfaceLabel);
 m_stepTimes.geometry=elapsed();
 for(auto& [id,s]:m_states)if(s.enabled&&s.dynamicSurface&&s.dynamicSurface->enabled){std::string error;
  if(failedGeometry.count(id))continue;
  bool solved=s.dynamicSurface->Step(dt,error);
  if(!solved)m_errors[s.entity]=error;
  else m_errors.erase(s.entity);
  if(solved&&s.dynamicSurface->stats.residual<=s.dynamicSurface->data->settings.tolerance*std::max(1.,s.volume)&&m_parcels.size()<s.dynamicSurface->data->settings.parcelBudget){
   auto& surface=*s.dynamicSurface;auto settings=surface.data->settings;
   for(unsigned i=0;i<surface.cells.size()&&m_parcels.size()<settings.parcelBudget;++i){auto& c=surface.cells[i];double speed=glm::length(c.velocity);
    if(speed<=settings.splashSpeed||c.volume<=0)continue;
    V point=surface.data->Point(surface.data->cells[i].chart,c.q),up=s.equilibrium.Up(point),velocity=c.velocity+up*(speed*.3);
    double energy=surface.KineticEnergy(i)*.1;
    // Include a conservative lift allowance (the full represented column
    // depth plus emergence/curvature error), not only parcel kinetic energy.
    double perVolume=.5*glm::dot(velocity,velocity)+s.equilibrium.magnitude*(std::max(0.,c.q-c.storage.curve.front().q)+.002+s.basin->coordinateError);
    double amount=std::min(c.volume*settings.splashFraction,energy/perVolume);
    if(amount<=1e-12)continue;
    if(surface.TakeKineticEnergy(i,amount*perVolume))Emit(s.handle,amount,glm::vec3(global(s.pose,point+up*.002)),glm::vec3(glm::dquat(s.pose.rotation)*velocity));
   }
  }
  Resolve(s);
 }

 surfaceScope.End(); ProfileScope loadScope(loadLabel);
 m_stepTimes.surface=elapsed();
 for(auto& o:world.ScriptObjects())if(o.liquidInteraction&&o.liquidInteraction->enabled){auto h=world.RuntimeBody(o.id);if(!world.Physics().IsDynamicBody(h)||!world.Physics().IsBodyEnabled(h)||world.Physics().IsBodySensor(h)||world.Physics().GetMass(h)<=0)continue;auto pose=world.Physics().GetTransform(h);LiquidGeometry body;if(o.body->shape==SceneShape::Box)body=LiquidBox(-V(o.body->halfExtents),V(o.body->halfExtents));else if(o.body->shape==SceneShape::Compound)for(auto b:o.body->compoundBoxes){auto g=LiquidBox(-V(b.halfExtents),V(b.halfExtents));for(auto& cell:g.cells)for(auto& p:cell)p=V(b.localCenter)+glm::dquat(b.rotation)*p;body.cells.insert(body.cells.end(),g.cells.begin(),g.cells.end());}else continue;for(auto& t:body.cells)for(V& p:t)p=V(pose.position)+glm::dquat(pose.rotation)*p;auto submerged=Submerged(body);if(submerged.volume>0){world.Physics().ApplyForce(h,submerged.buoyancy);world.Physics().ApplyTorque(h,glm::cross(submerged.center-pose.position,submerged.buoyancy));auto v=world.Physics().GetLinearVelocity(h)-submerged.velocity;double coefficient=std::min(o.liquidInteraction->drag*submerged.displacedMass,world.Physics().GetMass(h)/dt*.5);world.Physics().ApplyForce(h,-v*float(coefficient));}}

 m_stepTimes.loading=elapsed();m_stepTimes.total=std::chrono::duration<double>(phase-begin).count();
}

std::vector<glm::vec2> LiquidSystem::OpticalPaths(const glm::mat4& view,const glm::mat4& projection,unsigned columns,unsigned rows,float alpha)const{
    JUDAS_PROFILE_SCOPE("Water optical paths");
 struct Region {const OpticalCell* geometry;double level;};std::vector<Region> regions;
 for(auto& [id,s]:m_states)if(s.enabled&&s.equilibriumValid&&s.volume>0){
  auto add=[&](const LiquidGeometry& geometry,double level,unsigned index,std::uint64_t revision){
   auto& cache=m_opticalCache[{id,index}];
   if(cache.revision!=revision||cache.pose.position!=s.pose.position||cache.pose.rotation!=s.pose.rotation||!cache.equilibrium.Equivalent(s.equilibrium)){
    cache.cells.clear();cache.nodes.clear();cache.order.clear();cache.revision=revision;cache.pose=s.pose;cache.equilibrium=s.equilibrium;cache.minimum=V(1e30);cache.maximum=V(-1e30);
    cache.cells.reserve(geometry.cells.size());
    for(auto& tet:geometry.cells){OpticalTet item;auto levels=coordinates(tet,s.equilibrium);item.minimumQ=*std::min_element(levels.begin(),levels.end());item.maximumQ=*std::max_element(levels.begin(),levels.end());
     item.minimum=V(1e30);item.maximum=V(-1e30);for(V p:tet){p=global(s.pose,p);item.minimum=glm::min(item.minimum,p);item.maximum=glm::max(item.maximum,p);}
     cache.minimum=glm::min(cache.minimum,item.minimum);cache.maximum=glm::max(cache.maximum,item.maximum);
     auto planes=LiquidOccupiedPlanes(tet,s.equilibrium,(item.minimumQ+item.maximumQ)*.5);
     if(planes.size()<4)continue;
     for(unsigned i=0;i<4;++i){V normal=glm::dquat(s.pose.rotation)*V(planes[i]);item.planes[i]={normal,planes[i].w+glm::dot(normal,V(s.pose.position))};}
     if(planes.size()==5){item.capNormal=glm::dquat(s.pose.rotation)*V(planes[4]);item.capOffset=planes[4].w-(item.minimumQ+item.maximumQ)*.5+glm::dot(item.capNormal,V(s.pose.position));}
     cache.cells.push_back(std::move(item));
    }
    cache.order.resize(cache.cells.size());std::iota(cache.order.begin(),cache.order.end(),0);
    std::function<unsigned(unsigned,unsigned)> build=[&](unsigned begin,unsigned end){unsigned index=unsigned(cache.nodes.size());cache.nodes.push_back({});OpticalNode node;node.minimum=V(1e30);node.maximum=V(-1e30);node.begin=begin;node.end=end;
     for(unsigned i=begin;i<end;++i){auto& tet=cache.cells[cache.order[i]];node.minimum=glm::min(node.minimum,tet.minimum);node.maximum=glm::max(node.maximum,tet.maximum);}
     if(end-begin>8){V extent=node.maximum-node.minimum;unsigned axis=extent.y>extent.x?1:0;if(extent.z>extent[axis])axis=2;unsigned middle=(begin+end)/2;
      std::nth_element(cache.order.begin()+begin,cache.order.begin()+middle,cache.order.begin()+end,[&](unsigned a,unsigned b){auto& x=cache.cells[a];auto& y=cache.cells[b];double ca=x.minimum[axis]+x.maximum[axis],cb=y.minimum[axis]+y.maximum[axis];return ca==cb?a<b:ca<cb;});node.left=build(begin,middle);node.right=build(middle,end);}
     cache.nodes[index]=node;return index;};if(!cache.cells.empty())build(0,unsigned(cache.cells.size()));
   }
   if(!cache.cells.empty())regions.push_back({&cache,level});
  };
  if(s.dynamicSurface){for(unsigned i=0;i<s.dynamicSurface->cells.size();++i){auto& cell=s.dynamicSurface->cells[i];add(cell.storage.geometry,s.dynamicSurface->Coordinate(i,alpha),i,cell.geometryRevision);}}
  else add(s.geometry,s.q,0,1);
 }
 // A camera ray visits only intersected surface-cell bounds. The per-cell
 // tetrahedron BVHs remain cached; this small top-level tree tracks moving owners.
 struct RegionNode {V minimum,maximum;unsigned begin,end,left=0,right=0;};
 std::vector<unsigned> regionOrder(regions.size());std::iota(regionOrder.begin(),regionOrder.end(),0);
 std::vector<RegionNode> regionNodes;
 std::function<unsigned(unsigned,unsigned)> buildRegions=[&](unsigned begin,unsigned end){
  unsigned index=unsigned(regionNodes.size());regionNodes.push_back({});RegionNode node;node.minimum=V(1e30);node.maximum=V(-1e30);node.begin=begin;node.end=end;
  for(unsigned i=begin;i<end;++i){auto& g=*regions[regionOrder[i]].geometry;node.minimum=glm::min(node.minimum,g.minimum);node.maximum=glm::max(node.maximum,g.maximum);}
  if(end-begin>4){V extent=node.maximum-node.minimum;unsigned axis=extent.y>extent.x?1:0;if(extent.z>extent[axis])axis=2;unsigned middle=(begin+end)/2;
   std::nth_element(regionOrder.begin()+begin,regionOrder.begin()+middle,regionOrder.begin()+end,[&](unsigned a,unsigned b){auto& x=*regions[a].geometry;auto& y=*regions[b].geometry;double ca=x.minimum[axis]+x.maximum[axis],cb=y.minimum[axis]+y.maximum[axis];return ca==cb?a<b:ca<cb;});node.left=buildRegions(begin,middle);node.right=buildRegions(middle,end);
  }regionNodes[index]=node;return index;
 };if(!regions.empty())buildRegions(0,unsigned(regions.size()));
 std::vector<glm::vec2> paths(columns*rows,{0,0});auto inverse=glm::inverse(glm::dmat4(projection*view));
 for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<columns;++x){glm::dvec2 uv((x+.5)/columns,(y+.5)/rows);
  glm::dvec4 near=inverse*glm::dvec4(uv*2.-1.,-1,1),far=inverse*glm::dvec4(uv*2.-1.,1,1);
  V origin=V(near)/near.w,end=V(far)/far.w,ray=glm::normalize(end-origin);double maximum=glm::length(end-origin);
  std::vector<std::pair<double,double>> intervals;
  auto bounds=[&](V lo,V hi,double& a,double& b){for(unsigned axis=0;axis<3;++axis){if(std::abs(ray[axis])<1e-12){if(origin[axis]<lo[axis]||origin[axis]>hi[axis])return false;}else{double low=(lo[axis]-origin[axis])/ray[axis],high=(hi[axis]-origin[axis])/ray[axis];a=std::max(a,std::min(low,high));b=std::min(b,std::max(low,high));}}return b>a;};
  std::array<unsigned,64> regionTodo{};unsigned regionPending=regionNodes.empty()?0:1;
  while(regionPending){auto& regionNode=regionNodes[regionTodo[--regionPending]];double ra=0,rb=maximum;if(!bounds(regionNode.minimum,regionNode.maximum,ra,rb))continue;
   if(regionNode.end-regionNode.begin>4){regionTodo[regionPending++]=regionNode.right;regionTodo[regionPending++]=regionNode.left;continue;}
   for(unsigned regionIndex=regionNode.begin;regionIndex<regionNode.end;++regionIndex){auto& region=regions[regionOrder[regionIndex]];auto& geometry=*region.geometry;double enter=ra,exit=rb;if(!bounds(geometry.minimum,geometry.maximum,enter,exit))continue;
   std::array<unsigned,64> todo{};unsigned pending=1;while(pending){auto& node=geometry.nodes[todo[--pending]];double na=enter,nb=exit;if(!bounds(node.minimum,node.maximum,na,nb))continue;
    if(node.end-node.begin>8){todo[pending++]=node.right;todo[pending++]=node.left;continue;}
    for(unsigned index=node.begin;index<node.end;++index){auto& tet=geometry.cells[geometry.order[index]];if(region.level<=tet.minimumQ)continue;double a=na,b=nb;if(!bounds(tet.minimum,tet.maximum,a,b))continue;
     for(auto plane:tet.planes){double divisor=glm::dot(V(plane),ray),numerator=plane.w-glm::dot(V(plane),origin);if(std::abs(divisor)<1e-12){if(numerator<0){a=1;b=0;break;}}else if(divisor>0)b=std::min(b,numerator/divisor);else a=std::max(a,numerator/divisor);}
     if(b>a&&region.level<tet.maximumQ){double divisor=glm::dot(tet.capNormal,ray),numerator=region.level+tet.capOffset-glm::dot(tet.capNormal,origin);if(std::abs(divisor)<1e-12){if(numerator<0){a=1;b=0;}}else if(divisor>0)b=std::min(b,numerator/divisor);else a=std::max(a,numerator/divisor);}
     if(b>a)intervals.push_back({a,b});
    }
   }
  }
  }
  if(intervals.empty())continue;
  std::sort(intervals.begin(),intervals.end());double start=intervals.front().first,finish=intervals.front().second;
  for(size_t i=1;i<intervals.size();++i){if(intervals[i].first>finish+1e-6)break;finish=std::max(finish,intervals[i].second);}
  paths[y*columns+x]={float(start),float(finish)};
 }
 return paths;
}
