#include "PlanetTiles.h"
#include "CollisionAsset.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <glm/gtc/quaternion.hpp>
namespace {
using V=glm::dvec3;
uint64_t spread(uint64_t x){x=(x|(x<<16))&0x0000ffff0000ffffull;x=(x|(x<<8))&0x00ff00ff00ff00ffull;x=(x|(x<<4))&0x0f0f0f0f0f0f0f0full;x=(x|(x<<2))&0x3333333333333333ull;x=(x|(x<<1))&0x5555555555555555ull;return x;}
uint64_t morton(PlanetTileKey k){return (spread(k.x)|(spread(k.y)<<1))<<(2*(26-k.level));}
double ms(std::chrono::steady_clock::time_point start){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
std::vector<PlanetTileKey> children(PlanetTileKey k){std::vector<PlanetTileKey> result;for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)result.push_back({k.face,k.level+1,k.x*2+x,k.y*2+y});return result;}
V storedPoint(const PlanetTileData& data,const std::vector<V>& points,V direction,unsigned n){auto uv=PlanetSurface::FaceCoordinates(data.key.face,direction);const double count=double(1u<<data.key.level);const double sx=std::clamp(((uv.x+1)*.5*count-data.key.x)*n,0.,double(n)),sy=std::clamp(((uv.y+1)*.5*count-data.key.y)*n,0.,double(n));const unsigned x=std::min(unsigned(sx),n-1),y=std::min(unsigned(sy),n-1);const double u=sx-x,v=sy-y;const auto a=points[size_t(y)*(n+1)+x],c=points[size_t(y+1)*(n+1)+x+1];return v<=u?a*(1-u)+points[size_t(y)*(n+1)+x+1]*(u-v)+c*v:a*(1-v)+c*u+points[size_t(y+1)*(n+1)+x]*(v-u);}
bool descendant(PlanetTileKey k,PlanetTileKey p){return k.face==p.face&&k.level>=p.level&&(k.x>>(k.level-p.level))==p.x&&(k.y>>(k.level-p.level))==p.y;}
}
PlanetTiles::PlanetTiles(PlanetSettings settings,PhysicsWorld* physics,unsigned layer,CategoryMask mask,CategoryMask tags):surface(std::move(settings)),m_physics(physics),m_layer(layer),m_mask(mask),m_tags(tags){for(unsigned f=0;f<6;++f)m_leaves.insert({f,0,0,0});IndexLeaves();}
PlanetTiles::~PlanetTiles(){ClearCollision();if(m_renderer)for(auto& [_,t]:m_tiles)if(t.mesh.IsValid())m_renderer->DestroyMesh(t.mesh);}
void PlanetTiles::SetTags(CategoryMask tags){m_tags=tags;if(m_physics)for(const auto& [_,c]:m_contacts)m_physics->SetBodyTags(c.body,tags);}
void PlanetTiles::ClearCollision(){if(m_physics)for(auto& [_,c]:m_contacts)m_physics->DestroyBody(c.body);m_contacts.clear();m_pendingCooks.clear();m_stats.colliders=0;m_stats.collisionReady=false;m_focus.reset();}
PlanetTiles::Tile& PlanetTiles::Ensure(PlanetTileKey key,double morph){auto it=m_tiles.find(key);if(it!=m_tiles.end())return it->second;Tile t;t.data=BuildPlanetTile(surface,key,surface.settings.segments);t.morph=morph;return m_tiles.emplace(key,std::move(t)).first->second;}
std::vector<PlanetTileKey> PlanetTiles::Leaves()const{return {m_leaves.begin(),m_leaves.end()};}
void PlanetTiles::IndexLeaves(){for(auto& range:m_ranges)range.clear();for(auto k:m_leaves)m_ranges[k.face].emplace_back(morton(k),k);for(auto& range:m_ranges)std::sort(range.begin(),range.end(),[](const auto& a,const auto& b){return a.first<b.first;});}
PlanetTileKey PlanetTiles::LeafAt(V d)const{auto k=PlanetSurface::Locate(d,26);auto code=morton(k);const auto& ranges=m_ranges[k.face];auto it=std::upper_bound(ranges.begin(),ranges.end(),code,[](uint64_t a,const auto& b){return a<b.first;});return std::prev(it)->second;}
V PlanetTiles::RenderPoint(PlanetTileKey key,unsigned x,unsigned y)const{return m_tiles.at(key).resolved.at(size_t(y)*(surface.settings.segments+1)+x);}
V PlanetTiles::ResolvePoint(PlanetTileKey key,unsigned x,unsigned y)const{
 const unsigned n=surface.settings.segments;const auto& t=m_tiles.at(key);const auto i=size_t(y)*(n+1)+x;
 if(x&&y&&x<n&&y<n)return glm::mix(t.data.parentPoints[i],t.data.points[i],t.morph);
 // All owners of an edge select the same coarsest incident leaf. Fine edge
 // vertices lie on THAT leaf's actual triangle, including its morph state.
 // This also welds across cube faces; no face-specific neighbour table.
 const double count=double(1u<<key.level);auto model=PlanetSurface::Direction(key.face,2*(key.x+double(x)/n)/count-1,2*(key.y+double(y)/n)/count-1);
 auto uv=PlanetSurface::FaceCoordinates(key.face,model);const double epsilon=std::ldexp(1.,-int(key.level))*1e-5;
 PlanetTileKey common=key;double alpha=t.morph;
 for(int dy=(y==0?-1:0);dy<=(y==n?1:0);++dy)for(int dx=(x==0?-1:0);dx<=(x==n?1:0);++dx){auto other=LeafAt(PlanetSurface::Direction(key.face,uv.x+dx*epsilon,uv.y+dy*epsilon));if(other.level<common.level){common=other;alpha=m_tiles.at(other).morph;}else if(other.level==common.level){alpha=std::min(alpha,m_tiles.at(other).morph);if(other<common)common=other;}}
 const auto& owner=m_tiles.at(common);const auto& shared=owner.data;if(common.level<key.level)return storedPoint(shared,owner.resolved,model,n);auto p=storedPoint(shared,shared.points,model,n);if(common.level)p=glm::mix(storedPoint(shared,shared.parentPoints,model,n),p,alpha);return p;
}
std::vector<std::pair<PlanetTileKey,double>> PlanetTiles::Dependencies(PlanetTileKey key)const{
 std::set<PlanetTileKey> keys{key};const double count=1u<<key.level,epsilon=std::ldexp(1.,-int(key.level))*1e-5;
 for(unsigned y:{0u,1u,2u})for(unsigned x:{0u,1u,2u})if(x!=1||y!=1){const double u=2*(key.x+x*.5)/count-1,v=2*(key.y+y*.5)/count-1;for(int dy=(y==0?-1:0);dy<=(y==2?1:0);++dy)for(int dx=(x==0?-1:0);dx<=(x==2?1:0);++dx){auto other=LeafAt(PlanetSurface::Direction(key.face,u+dx*epsilon,v+dy*epsilon));if(other.level<=key.level)keys.insert(other);}}
 std::vector<std::pair<PlanetTileKey,double>> result;for(auto k:keys){result.emplace_back(k,m_tiles.at(k).morph);if(k.level<key.level)result.emplace_back(k,double(m_tiles.at(k).revision));}return result;
}
void PlanetTiles::Upload(Tile& t){auto data=t.data;data.points=t.resolved;data.parentPoints=data.points;auto mesh=data.Mesh(1,0);if(t.mesh.IsValid())m_renderer->UpdateMeshVertices(t.mesh,mesh.vertices);else t.mesh=m_renderer->CreateMesh(mesh);}
void PlanetTiles::Prepare(Renderer& renderer,V eye,V origin,double fov,unsigned pixels,double dt){Prepare(renderer,std::vector<PlanetView>{{eye,fov,pixels}},origin,dt);}
void PlanetTiles::Prepare(Renderer& renderer,const std::vector<PlanetView>& views,V origin,double dt){(void)origin;const auto start=std::chrono::steady_clock::now();if(m_renderer&&m_renderer!=&renderer)throw std::logic_error("planet renderer owner changed");m_renderer=&renderer;
 auto selection=SelectPlanetTiles(surface,views);m_stats.selectionMs=ms(start);const auto buildStart=std::chrono::steady_clock::now();std::set<PlanetTileKey> desired(selection.leaves.begin(),selection.leaves.end());if(m_tiles.empty()){m_leaves=desired;for(auto k:m_leaves)Ensure(k,1);}
 for(auto k:m_leaves)Ensure(k,1);
 // Only complete sibling groups replace their parent. A frame can never show
 // an unprepared hole; the six coverage roots exist for the entire lifetime.
 unsigned groups=std::max(1u,surface.settings.buildsPerFrame/4);
 std::vector<PlanetTileKey> split;
 for(auto k:m_leaves){bool wants=false;for(auto d:desired)if(d.level>k.level&&descendant(d,k)){wants=true;break;}if(wants&&m_tiles.at(k).morph>=1)split.push_back(k);}
 std::sort(split.begin(),split.end(),[&](auto a,auto b){return ([&](){double x=1e300,y=1e300;for(const auto& v:views){x=std::min(x,glm::length(surface.TileCenter(a)-v.eye));y=std::min(y,glm::length(surface.TileCenter(b)-v.eye));}return x<y;})();});
 for(auto k:split){if(!groups||m_leaves.size()+3>surface.settings.maxTiles||m_tiles.size()+4>surface.settings.maxResident)break;for(auto c:children(k))Ensure(c,0);m_leaves.erase(k);for(auto c:children(k))m_leaves.insert(c);--groups;}
 // Reverse morph before coarsening. Siblings share their boundary alpha.
 std::set<PlanetTileKey> parents;for(auto k:m_leaves)if(k.level)parents.insert(k.Parent());
 std::set<PlanetTileKey> merging;
 for(auto p:parents){bool all=true,wants=false;for(auto c:children(p))all&=m_leaves.count(c)!=0;for(auto d:desired)if(descendant(p,d)){wants=true;break;}if(all&&wants)for(auto c:children(p))merging.insert(c);}
 const double amount=surface.settings.morphSeconds>0?std::clamp(dt,0.,.1)/surface.settings.morphSeconds:1;
 for(auto k:m_leaves){auto& t=m_tiles.at(k);t.morph=std::clamp(t.morph+(merging.count(k)?-amount:amount),0.,1.);}
 for(auto p:parents){bool done=true;for(auto c:children(p))done&=merging.count(c)&&m_tiles.at(c).morph==0;if(done){for(auto c:children(p))m_leaves.erase(c);m_leaves.insert(p);Ensure(p,1).morph=1;}}
 m_stats.buildMs=ms(buildStart);const auto uploadStart=std::chrono::steady_clock::now();IndexLeaves();
 auto ordered=Leaves();std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){return std::tie(a.level,a.face,a.x,a.y)<std::tie(b.level,b.face,b.x,b.y);});
 for(auto k:ordered){auto& t=m_tiles.at(k);auto dependencies=Dependencies(k);if(!t.mesh.IsValid()||dependencies!=t.dependencies){const auto n=surface.settings.segments;t.resolved.resize(t.data.points.size());for(unsigned y=0;y<=n;++y)for(unsigned x=0;x<=n;++x)t.resolved[size_t(y)*(n+1)+x]=ResolvePoint(k,x,y);++t.revision;Upload(t);t.uploaded=t.morph;t.dependencies=std::move(dependencies);}}

 // Retain only direct parents needed for reverse transitions. CPU/GPU products
 // have a hard bound even after an arbitrarily long trip around the planet.
 std::set<PlanetTileKey> keep=m_leaves;for(auto k:m_leaves)if(k.level)keep.insert(k.Parent());
 for(auto it=m_tiles.begin();it!=m_tiles.end();)if(!keep.count(it->first)){if(it->second.mesh.IsValid())renderer.DestroyMesh(it->second.mesh);it=m_tiles.erase(it);}else ++it;
 m_stats.uploadMs=ms(uploadStart);m_stats.leaves=unsigned(m_leaves.size());m_stats.resident=unsigned(m_tiles.size());m_stats.triangles=m_stats.leaves*surface.settings.segments*surface.settings.segments*2;m_stats.maximumPixelError=selection.maximumPixelError;m_stats.budgetLimited=selection.budgetLimited;m_stats.prepareMs=ms(start);
}
void PlanetTiles::Draw(Renderer& renderer,V origin)const{for(auto k:m_leaves){auto it=m_tiles.find(k);if(it!=m_tiles.end()&&it->second.mesh.IsValid())renderer.DrawMesh(it->second.mesh,glm::vec3(it->second.data.origin-origin),{1,0,0,0},glm::vec3(1),{},glm::vec3(.24f,.43f,.31f));}}
bool PlanetTiles::Collision(V focus,V origin,unsigned buildBudget){auto start=std::chrono::steady_clock::now();m_stats.collisionReady=false;if(!m_physics){m_stats.error="no physics owner";return false;}
 try{if(!std::isfinite(glm::length(focus))||glm::length(focus-origin)>8192)throw std::runtime_error("planet collision focus outside local physics chart");auto model=glm::conjugate(glm::normalize(surface.settings.rotation))*glm::normalize(focus-surface.settings.center);const unsigned n=32;
 unsigned level=0;while(level<26&&surface.settings.radius*2/std::ldexp(double(n),int(level))>surface.settings.collisionSpacing)++level;
 if(surface.settings.radius*2/std::ldexp(double(n),int(level))>surface.settings.collisionSpacing*(1+1e-12))throw std::runtime_error("planet collision depth cannot meet authored spacing");
 auto central=PlanetSurface::Locate(model,level);const double tileWidth=2./std::ldexp(1.,int(level));auto uv=PlanetSurface::FaceCoordinates(central.face,model);
 // Neighbours are selected in cube coordinates, including outside a face.
 // Locate maps those rays back to their correct canonical adjacent face.
 int reach=int(std::ceil(surface.settings.collisionRadius/(surface.settings.radius*tileWidth*.33)))+1;
 const auto surfacePoint=surface.Sample(focus).point;
 if(reach>32)throw std::runtime_error("collision radius/spacing exceeds bounded neighbourhood");
 std::set<PlanetTileKey> wanted;
 for(int y=-reach;y<=reach;++y)for(int x=-reach;x<=reach;++x){auto d=PlanetSurface::Direction(central.face,uv.x+x*tileWidth*.5,uv.y+y*tileWidth*.5);auto k=PlanetSurface::Locate(d,level);if(glm::length(surface.TileCenter(k)-surfacePoint)<=surface.settings.collisionRadius+surface.TileRadius(k))wanted.insert(k);}
 if(wanted.size()>surface.settings.maxCollisionTiles)throw std::runtime_error("planet collision tile budget exceeded; reduce interest radius or author coarser collision spacing");
 // Prepare every missing immutable cook before retiring old support. Failed
 // preparation leaves all prior colliders intact, and reports unready.
 for(auto it=m_pendingCooks.begin();it!=m_pendingCooks.end();)if(!wanted.count(it->first))it=m_pendingCooks.erase(it);else ++it;
 bool preparing=false;for(auto k:wanted)if(!m_contacts.count(k)&&!m_pendingCooks.count(k)){if(!buildBudget){preparing=true;continue;}--buildBudget;auto tile=BuildPlanetTile(surface,k,n,false);auto cook=std::make_shared<CollisionAsset>();std::string error;if(!CookCollision(tile.Mesh(1,0),{},*cook,error))throw std::runtime_error(error);m_pendingCooks.emplace(k,std::make_pair(tile.origin,cook));}
 if(preparing){m_stats.error.clear();m_stats.collisionMs=ms(start);return false;}
 std::vector<BodyHandle> installed;installed.reserve(m_pendingCooks.size());try{for(auto& [k,p]:m_pendingCooks){auto body=m_physics->CreateShape(Shape::Cooked(p.second,""),{glm::vec3(p.first-origin),{1,0,0,0}},false,0,.7f,0);installed.push_back(body);m_physics->SetCollisionFilter(body,m_layer,m_mask);m_physics->SetBodyTags(body,m_tags);m_contacts.emplace(k,Contact{body,p.first});}}catch(...){for(auto h:installed)m_physics->DestroyBody(h);for(auto& [k,_]:m_pendingCooks)m_contacts.erase(k);throw;}
 m_pendingCooks.clear();
 for(auto it=m_contacts.begin();it!=m_contacts.end();)if(!wanted.count(it->first)){m_physics->DestroyBody(it->second.body);it=m_contacts.erase(it);}else ++it;
 m_stats.error.clear();m_stats.collisionReady=true;m_focus=focus;
 }catch(const std::exception& e){m_stats.error=e.what();}
 m_stats.colliders=unsigned(m_contacts.size());m_stats.collisionMs=ms(start);return m_stats.collisionReady;
}
