#include "LocalMultiplayer.h"
#include "PerformanceProfiler.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>
namespace {std::atomic<std::uint64_t> nextDevice{1},nextPlayer{1};
bool In(glm::vec4 r,float x,float y){return x>=r.x&&y>=r.y&&x<r.x+r.z&&y<r.y+r.w;}
bool Overlap(glm::vec4 a,glm::vec4 b){return std::min(a.x+a.z,b.x+b.z)>std::max(a.x,b.x)&&std::min(a.y+a.w,b.y+b.w)>std::max(a.y,b.y);}
bool Accept(const LocalPlayers::Assignment& a,const std::string& control){return a.controls.empty()||a.controls.count(control);}
}
InputDeviceHub::InputDeviceHub(){for(auto kind:{"keyboard","mouse","touch"})devices[kind]={kind,kind,std::string("SDL ")+kind,true,{},{}};changes.reserve(256);}
std::string InputDeviceHub::Connect(const std::string& kind,const std::string& name){
 unsigned live=0;for(auto& d:devices)live+=d.second.connected;if(live>=19)return {};
 // Retire disconnected device records; their tokens are never recycled.
 for(auto it=devices.begin();it!=devices.end();)if(!it->second.connected)it=devices.erase(it);else ++it;
 auto id=kind+":"+std::to_string(nextDevice++);devices[id]={id,kind,name,true,{},{}};return id;
}
const InputDeviceHub::Device* InputDeviceHub::Find(const std::string& id)const{auto it=devices.find(id);return it==devices.end()?nullptr:&it->second;}
void InputDeviceHub::Disconnect(const std::string& id){auto it=devices.find(id);if(it==devices.end())return;it->second.connected=false;it->second.values.clear();}
void InputDeviceHub::BeginFrame(){++frame;changes.clear();touchChanges.clear();overflow=false;for(auto& d:devices)for(auto control:{"mouse:dx","mouse:dy","mouse:wheelX","mouse:wheelY"}){auto it=d.second.values.find(control);if(it!=d.second.values.end())it->second=0;}}
void InputDeviceHub::Reset(){for(auto& d:devices){d.second.values.clear();d.second.pointer={};}touches.clear();changes.clear();touchChanges.clear();++resetRevision;}
void InputDeviceHub::Set(const std::string& device,const std::string& control,float value){
 auto it=devices.find(device);if(it==devices.end()||!it->second.connected||!std::isfinite(value)||overflow)return;
 auto& old=it->second.values[control];if(old==value)return;old=value;
 if(changes.size()>=4096){Reset();overflow=true;return;}changes.push_back({device,control,value});
}
void InputDeviceHub::RecordTouch(InputTouch contact,bool down){if(!contact.id||!std::isfinite(contact.x)||!std::isfinite(contact.y)||!std::isfinite(contact.pressure)||overflow)return;if(touchChanges.size()>=4096){Reset();overflow=true;return;}touchChanges.push_back({contact,down});}
void InputDeviceHub::Pointer(const std::string& device,InputPointer value){auto it=devices.find(device);if(it!=devices.end())it->second.pointer=value;}
LocalPlayers::~LocalPlayers(){if(renderer)for(auto& p:players)renderer->DestroyRenderTarget(p.second.target);}
LocalPlayers::Player* LocalPlayers::Create(const std::string& name,const InputMap& map,std::string& error){if(players.size()>=8||name.size()>128){error="At most eight players and 128-byte names";return nullptr;}Player p;p.id=nextPlayer++;p.name=name;if(!p.input.SetMap(map,error))return nullptr;auto id=p.id;return &players.emplace(id,std::move(p)).first->second;}
LocalPlayers::Player* LocalPlayers::Find(uint64_t id){auto it=players.find(id);return it==players.end()?nullptr:&it->second;}
const LocalPlayers::Player* LocalPlayers::Find(uint64_t id)const{auto it=players.find(id);return it==players.end()?nullptr:&it->second;}
bool LocalPlayers::Remove(uint64_t id){auto it=players.find(id);if(it==players.end())return false;if(renderer)renderer->DestroyRenderTarget(it->second.target);for(auto t=touchOwners.begin();t!=touchOwners.end();)if(t->second==id)t=touchOwners.erase(t);else ++t;players.erase(it);return true;}
bool LocalPlayers::ValidRect(glm::vec4 r){return std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.z)&&std::isfinite(r.w)&&r.x>=0&&r.y>=0&&r.z>0&&r.w>0&&r.x+r.z<=1.000001f&&r.y+r.w<=1.000001f;}
bool LocalPlayers::Assign(uint64_t id,const InputDeviceHub& hub,Assignment a,std::string& error){
 auto* p=Find(id);const auto* d=hub.Find(a.device);if(!p||!d||!d->connected){error="stale player or disconnected device";return false;}
 if(a.controls.size()>128||!ValidRect(a.region)){error="invalid device controls/touch region";return false;}
 for(auto& key:a.controls)if(d->kind!="keyboard"||key.rfind("key:",0)!=0||key.size()>128){error="Only keyboard controls can be partitioned";return false;}
 for(auto& pair:players)for(auto& b:pair.second.devices)if(b.device==a.device&&pair.first!=id){
  bool conflict=d->kind=="touch"?Overlap(a.region,b.region):d->kind!="keyboard"||a.controls.empty()||b.controls.empty();
  if(d->kind=="keyboard")for(auto& key:a.controls)conflict|=b.controls.count(key)>0;
  if(conflict){error="device/control region already assigned; release it first";return false;}
 }
 Unassign(id,a.device);p->devices.push_back(std::move(a));p->input.Reset();p->touchSources.clear();p->baseline=true;error.clear();return true;
}
bool LocalPlayers::Unassign(uint64_t id,const std::string& device){auto* p=Find(id);if(!p)return false;auto old=p->devices.size();p->devices.erase(std::remove_if(p->devices.begin(),p->devices.end(),[&](auto& a){return a.device==device;}),p->devices.end());if(old==p->devices.size())return false;p->input.Reset();p->touchSources.clear();p->baseline=true;for(auto t=touchOwners.begin();t!=touchOwners.end();)if(t->second==id)t=touchOwners.erase(t);else ++t;return true;}
void LocalPlayers::Route(const InputDeviceHub& hub){
 JUDAS_PROFILE_SCOPE("Local player input routing");
 if(lastFrame==hub.frame)return;
 lastFrame=hub.frame;
 if(revision!=hub.resetRevision){touchOwners.clear();for(auto& pair:players){pair.second.input.Reset();pair.second.touchSources.clear();pair.second.baseline=true;}revision=hub.resetRevision;}
 std::vector<std::pair<uint64_t,InputDeviceHub::TouchChange>> routed;
 auto capture=[&](const InputTouch& t){if(!touchOwners.count(t.id))for(auto& pair:players)for(auto& a:pair.second.devices)if(a.device=="touch"&&In(a.region,t.x,t.y)){touchOwners[t.id]=pair.first;return;}};
 for(auto& event:hub.touchChanges){if(event.down)capture(event.contact);auto owner=touchOwners.find(event.contact.id);if(owner!=touchOwners.end())routed.emplace_back(owner->second,event);if(!event.down)touchOwners.erase(event.contact.id);}
 std::set<uint64_t> live;for(auto& t:hub.touches){live.insert(t.id);if(!touchOwners.count(t.id))for(auto& pair:players)for(auto& a:pair.second.devices)if(a.device=="touch"&&In(a.region,t.x,t.y)){touchOwners[t.id]=pair.first;break;}}
 for(auto it=touchOwners.begin();it!=touchOwners.end();)if(!live.count(it->first))it=touchOwners.erase(it);else ++it;
 for(auto& pair:players){auto& p=pair.second;p.input.BeginFrame();
  std::set<std::string> connected;for(auto& a:p.devices)if(auto* device=hub.Find(a.device);device&&device->connected)connected.insert(a.device);
  if(connected!=p.connectedDevices){p.connectedDevices=std::move(connected);p.input.Reset();p.touchSources.clear();p.baseline=true;}

  auto combined=[&](const std::string& control){double sum=0;for(auto& a:p.devices)if(Accept(a,control)){auto source=p.sourceValues.find(a.device);if(source!=p.sourceValues.end()){auto value=source->second.find(control);if(value!=source->second.end())sum+=value->second;}}const bool relative=control=="mouse:dx"||control=="mouse:dy"||control=="mouse:wheelX"||control=="mouse:wheelY";return float(relative?sum:std::clamp(sum,-1.,1.));};
  if(p.baseline){p.sourceValues.clear();std::set<std::string> controls;for(auto& a:p.devices)if(auto* d=hub.Find(a.device);d&&d->connected)for(auto& v:d->values)if(Accept(a,v.first)){p.sourceValues[a.device][v.first]=v.second;controls.insert(v.first);}for(auto& control:controls)p.input.SetPhysical(control,combined(control));}
  else{
   // Relative observations begin anew even if a device has no event this frame.
   for(auto& source:p.sourceValues)for(auto control:{"mouse:dx","mouse:dy","mouse:wheelX","mouse:wheelY"}){auto it=source.second.find(control);if(it!=source.second.end())it->second=0;}
   for(auto& event:hub.Changes())for(auto& a:p.devices)if(a.device==event.device&&Accept(a,event.control)){p.sourceValues[a.device][event.control]=event.value;p.input.SetPhysical(event.control,combined(event.control));}
  }
  // Device values persist; relative deltas do not. Pointer/touch identities are
  // captured on touch-down, so crossing a viewport cannot steal another player.
  bool touchPointer=false;bool mouse=false;InputPointer mousePointer;for(auto& a:p.devices)if(a.device=="mouse")if(auto* d=hub.Find(a.device)){mouse=true;mousePointer=d->pointer;}
  for(auto& entry:routed)if(entry.first==p.id){touchPointer=true;auto& event=entry.second;auto& t=event.contact;p.input.SetTouch(0,int64_t(t.id),t.x,t.y,t.pressure,event.down);if(event.down)p.touchSources.insert(t.id);else p.touchSources.erase(t.id);p.input.SetPointer(t.x,t.y,!p.touchSources.empty());p.input.SetPhysical("mouse:Left",std::max(combined("mouse:Left"),p.touchSources.empty()?0.f:1.f));}
  for(auto it=p.touchSources.begin();it!=p.touchSources.end();)if(!touchOwners.count(*it)||touchOwners.at(*it)!=p.id){touchPointer=true;p.input.SetTouch(0,int64_t(*it),0,0,0,false);it=p.touchSources.erase(it);}else ++it;
  bool touch=false;for(auto& t:hub.touches)if(touchOwners.count(t.id)&&touchOwners.at(t.id)==p.id){p.input.SetTouch(0,int64_t(t.id),t.x,t.y,t.pressure,true);p.touchSources.insert(t.id);if(!touch){p.input.SetPointer(t.x,t.y,true);p.input.SetPhysical("mouse:Left",1);touch=true;}}
  if(!touch){auto last=p.input.Pointer();// A release still belongs to the touch pointer. An idle assigned mouse
   // must not move it to (0,0) before the UI consumes the release edge.
   p.input.SetPointer(mouse&&!touchPointer?mousePointer.x:last.x,mouse&&!touchPointer?mousePointer.y:last.y,mouse&&mousePointer.down);p.input.SetPhysical("mouse:Left",combined("mouse:Left"));}
  if(p.baseline){p.input.DiscardPending();p.baseline=false;}
 }
}
void LocalPlayers::BeginFixedStep(){for(auto& p:players)p.second.input.BeginFixedStep();}
void LocalPlayers::DiscardPending(){for(auto& p:players)p.second.input.DiscardPending();}
bool LocalPlayers::SetViewport(uint64_t id,glm::vec4 rect,std::string& error){auto* p=Find(id);if(!p||!ValidRect(rect)){error="invalid player/normalized viewport";return false;}p->viewport=rect;error.clear();return true;}
bool LocalPlayers::SetView(uint64_t id,const SceneTransform& pose,float fov,float nearPlane,float farPlane,std::string& error){auto* p=Find(id);bool finite=true;for(int i=0;i<3;++i)finite&=std::isfinite(pose.position[i]);for(int i=0;i<4;++i)finite&=std::isfinite(pose.rotation[i]);if(!p||!finite||glm::length(pose.rotation)<1e-6f||!std::isfinite(fov)||fov<1||fov>175||!std::isfinite(nearPlane)||!std::isfinite(farPlane)||nearPlane<=0||farPlane<=nearPlane){error="invalid player view/range";return false;}p->view=pose;p->view->rotation=glm::normalize(pose.rotation);p->fov=fov;p->nearPlane=nearPlane;p->farPlane=farPlane;error.clear();return true;}
bool LocalPlayers::HasViews()const{for(auto& p:players)if(p.second.view)return true;return false;}

glm::ivec2 LocalPlayers::ViewSize(glm::vec4 rect,int width,int height){const double w=std::max(1.,double(width)*rect.z),h=std::max(1.,double(height)*rect.w),scale=std::min(1.,2048./std::max(w,h));return {std::max(1,int(std::lround(w*scale))),std::max(1,int(std::lround(h*scale)))};}
