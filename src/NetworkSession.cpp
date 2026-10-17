#include "NetworkSession.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "ResourceManager.h"
#include "SaveService.h"
#include "CharacterMotor.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <deque>
#include <cmath>
#include <random>
#include <sstream>
using Json=nlohmann::json;
namespace {
std::string Token(){std::random_device r;std::ostringstream s;for(int i=0;i<4;++i){auto n=r();s<<std::hex<<n<<'-';}return s.str();}
bool Supported(const SceneObject* d){return d&&d->id<kRuntimeEntityIdBase&&!d->parent&&!d->wheel&&!d->powertrain&&!d->ragdoll&&!d->deformable&&!d->animation&&!d->joint&&!d->socket&&!d->planet&&!d->liquidBasin&&!d->liquidContainer&&!d->fluidVolume&&(!d->body||d->body->shape!=SceneShape::Terrain);}
bool Data(const std::string& s){std::string error;return s.size()<=1024&&ScriptSystem::ValidateJson(s,error,false);}
Json V(glm::dvec3 v){return Json::array({v.x,v.y,v.z});}
bool Vec(const Json& j,glm::dvec3& v,double limit=1e15){if(!j.is_array()||j.size()!=3)return false;for(int i=0;i<3;++i){if(!j[i].is_number())return false;v[i]=j[i].get<double>();if(!std::isfinite(v[i])||std::abs(v[i])>limit)return false;}return true;}
bool Text(const Json& j,const char* key,std::string& out,size_t max){auto it=j.find(key);if(it==j.end()||!it->is_string())return false;out=it->get<std::string>();return out.size()<=max;}
bool Counter(const Json& j,const char* key,uint64_t& n){auto it=j.find(key);if(it==j.end()||!it->is_number_unsigned())return false;n=it->get<uint64_t>();return n<=9007199254740991ULL;}
}
struct NetworkSession::Impl {
 RuntimeWorld& world;NetworkTransport transport;Options options;Launch launch;
 std::string mode="offline",status="offline",error,peerId,content,session,reconnect;
 uint64_t generation=1,nextEntity=1,nextPeer=1,tick=0,inputSequence=0;double now=0,lastPump=-1,lastSnapshot=-1,lastInputSend=-1,lastReceive=0,lastHeartbeat=-1,clientHandshake=-1,simulation=0;
 bool starting=false;
 struct Identity{std::string value,error;};std::shared_ptr<Identity> identity;JobHandle identityJob;
 struct Remote{Peer peer;uint32_t socket=0;std::string secret;double absent=0,lastInput=-1;uint64_t sequence=0;std::map<std::string,float> input;glm::dvec3 interest{0};double radius=1e12;};
 std::set<uint32_t> rejectedSockets;
 std::map<std::string,Remote> peers;std::map<uint32_t,std::string> sockets;std::map<uint32_t,double> pending;
 std::map<std::string,Entry> entries;std::map<std::string,float> outgoing;
 struct Sample{SceneTransform pose;glm::dvec3 absolute{0};double received=0;};
 struct Replica{EntityPhysicalState original;glm::dvec3 capturedOrigin{0};bool visible=true;std::deque<Sample> samples;bool initialized=false,relevant=false;};std::map<EntityId,Replica> replicas;
 uint64_t snapshots=0,rejected=0;std::set<EntityId> published;
 explicit Impl(RuntimeWorld& w):world(w){}
 void Gate(bool value){if(auto c=world.SceneControl())c->networkActive=value;}
 bool ValidOptions(const Options& o,bool host,std::string& e){
  if(o.address.empty()||o.address.size()>15||o.port>65535||(!host&&!o.port)||o.maxPeers<1||o.maxPeers>7||o.snapshotHz<1||o.snapshotHz>30||o.name.size()>64||o.controls.size()>32){e="Invalid network options (IPv4, port, 1..7 peers, 1..30 Hz, 32 controls)";return false;}
  std::set<std::string> controls;for(auto& name:o.controls)if(name.empty()||name.size()>64||!controls.insert(name).second){e="Control names must be distinct nonempty strings up to 64 bytes";return false;}
  auto c=world.SceneControl();auto* r=world.Resources();if(world.legacyGameplay||!c||c->ComposedProject()||c->ProjectConfig().ModSession()||!r||!r->Assets()||!r->Jobs()){e="Networking requires a modern, single-scene, unmodded project with resources/jobs";return false;}
  if(host)for(auto& name:o.controls){auto* entry=c->ProjectConfig().Settings().input.Find(name);if(!entry||entry->vector){e="Network controls must name project scalar actions/axes";return false;}}
  if(world.Scripts()&&world.Scripts()->HasReloaded()){e="Stop and Play after script reload before opening a content-checked session";return false;}
  if(c->OperationInProgress()){e="Wait for pending scene/save operations before opening a session";return false;}return true;
 }
 void BeginIdentity(){auto c=world.SceneControl();auto project=c->ProjectConfig();auto assets=*world.Resources()->Assets();auto scenes=c->Scenes();identity=std::make_shared<Identity>();auto product=identity;
  identityJob=world.Resources()->Jobs()->Submit([product,project,assets,scenes](JobContext& job){ComputeSaveContentFingerprint(project,assets,scenes,product->value,product->error,&job);},JobPriority::Normal,"network content identity");starting=true;Gate(true);
 }
 void Neutral(Remote& r){r.input.clear();if(auto* ps=world.PlayersIfLoaded())if(auto* p=ps->Find(r.peer.player)){std::string e;p->input.SubmitLogical({},e);}}
 void RemovePlayer(Remote& r){if(!r.peer.player)return;if(world.UIIfLoaded())world.UI().RemovePlayer(r.peer.player);if(auto* ps=world.PlayersIfLoaded())ps->Remove(r.peer.player);}
 bool Send(uint32_t socket,const Json& j,bool replaceable=false){std::string e;if(!transport.Send(socket,j.dump(),replaceable,e)){++rejected;error=e;return false;}return true;}
 void Reject(uint32_t socket,const std::string& why){++rejected;error=why;rejectedSockets.insert(socket);pending.erase(socket);
  if(mode=="client"){status="disconnected";outgoing.clear();}else if(auto it=sockets.find(socket);it!=sockets.end()){auto& r=peers.at(it->second);r.peer.connected=false;r.absent=now;Neutral(r);sockets.erase(it);lastSnapshot=-1;}
  transport.Disconnect(socket,why);
 }
 bool InputValue(const Json& j,std::map<std::string,float>& out){if(!j.is_object()||j.size()>32)return false;for(auto it=j.begin();it!=j.end();++it){if(std::find(options.controls.begin(),options.controls.end(),it.key())==options.controls.end()||!it.value().is_number())return false;double v=it.value().get<double>();auto* entry=world.SceneControl()->ProjectConfig().Settings().input.Find(it.key());if(!std::isfinite(v)||v< -1||v>1||!entry||entry->vector||(!entry->axis&&v!=0&&v!=1))return false;out[it.key()]=float(v);}return true;}
 void Welcome(uint32_t socket,const Json& j){
  std::string hash,name,secret;if(!Text(j,"content",hash,64)||hash!=content||!Text(j,"name",name,64)||!Text(j,"token",secret,160)||j.value("version",0)!=1){Reject(socket,"Protocol/content mismatch or invalid hello");return;}
  Remote* remote=nullptr;for(auto& pair:peers)if(!secret.empty()&&pair.second.secret==secret&&!pair.second.peer.connected&&now-pair.second.absent<15)remote=&pair.second;
  if(!remote){if(peers.size()>=options.maxPeers){Reject(socket,"Session peer/reservation capacity reached");return;}Remote r;r.peer.id=std::to_string(nextPeer++);r.peer.name=name;r.secret=Token();std::string e;auto* player=world.Players().Create(name,world.SceneControl()->ProjectConfig().Settings().input,e);if(!player){Reject(socket,e);return;}player->input.SetLogicalMode(true);r.peer.player=player->id;remote=&peers.emplace(r.peer.id,std::move(r)).first->second;}
  remote->socket=socket;remote->peer.connected=true;remote->sequence=0;remote->lastInput=now;Neutral(*remote);sockets[socket]=remote->peer.id;pending.erase(socket);
  Send(socket,{{"t","welcome"},{"version",1},{"session",session},{"peer",remote->peer.id},{"token",remote->secret},{"controls",options.controls}});
  lastSnapshot=-1;
 }
 void HostMessage(uint32_t socket,const Json& j){
  const auto type=j.value("t",std::string());if(type=="hello"){if(sockets.count(socket)){Reject(socket,"Duplicate hello");return;}Welcome(socket,j);return;}
  auto it=sockets.find(socket);if(it==sockets.end()){Reject(socket,"Handshake required");return;}auto& r=peers.at(it->second);
  if(type=="ping")return;
  if(type=="interest"){glm::dvec3 position;double radius=j.value("radius",-1.);if(!j.contains("position")||!Vec(j["position"],position)||!std::isfinite(radius)||radius<1||radius>1e9){Reject(socket,"Invalid interest");return;}r.interest=position;r.radius=radius;lastSnapshot=-1;return;}
  if(type!="input"){Reject(socket,"Client cannot publish authoritative state");return;}
  uint64_t seq=0;std::map<std::string,float> values;if(!Counter(j,"sequence",seq)||seq<=r.sequence||!j.contains("values")||!InputValue(j["values"],values)){Reject(socket,"Invalid/stale control input");return;}
  // Flood bounds are per peer as well as transport bytes. No unbounded callback queue.
  if(auto* players=world.PlayersIfLoaded())if(auto* player=players->Find(r.peer.player)){std::string e;if(!player->input.SubmitLogical(values,e)){Reject(socket,e);return;}}
  r.sequence=seq;r.lastInput=now;r.input=std::move(values);
 }
 void ClientMessage(const Json& j){
  const auto type=j.value("t",std::string());if(type=="welcome"){
   std::string nextSession,id,token;if(status!="connecting"||j.value("version",0)!=1||!Text(j,"session",nextSession,160)||!Text(j,"peer",id,20)||!Text(j,"token",token,160)||!j.contains("controls")||!j["controls"].is_array()||j["controls"].size()>32){Reject(1,"Invalid welcome");return;}
   options.controls.clear();for(auto& x:j["controls"]){if(!x.is_string()||x.get<std::string>().size()>64){Reject(1,"Invalid control schema");return;}options.controls.push_back(x.get<std::string>());}session=nextSession;peerId=id;reconnect=token;status="connected";error.clear();lastReceive=now;return;
  }
  if(type=="ping")return;
  if(type!="snapshot"||status!="connected"){Reject(1,"Unexpected server message");return;}
  uint64_t remoteTick=0;if(!Counter(j,"tick",remoteTick)||!j.contains("entries")||!j["entries"].is_array()||j["entries"].size()>64||!j.contains("peers")||!j["peers"].is_array()||j["peers"].size()>8){Reject(1,"Invalid snapshot envelope");return;}
  if(remoteTick<=tick&&snapshots)return;
  // Validate the entire publication before mutating any world state.
  std::map<std::string,Entry> next;std::map<EntityId,EntityPhysicalState> poses;size_t newReplicas=0;
  for(auto& row:j["entries"]){Entry e;std::string entity;glm::dvec3 position,velocity,angular;
   if(!Text(row,"id",e.id,160)||!Text(row,"entity",entity,20)||!Text(row,"owner",e.owner,20)||!row.contains("state")||!Data(row["state"].dump())||entity.empty()||entity.find_first_not_of("0123456789")!=std::string::npos){Reject(1,"Invalid replicated entry");return;}
   try{e.entity=std::stoull(entity);}catch(...){Reject(1,"Invalid entity key");return;}
   if(!Supported(world.RuntimeDefinition(e.entity))||!Vec(row.at("p"),position)||!Vec(row.at("v"),velocity,1e6)||!Vec(row.at("w"),angular,1e6)||!row.contains("q")||!row["q"].is_array()||row["q"].size()!=4){Reject(1,"Unsupported replicated pose");return;}
   glm::quat q;for(int n=0;n<4;++n){if(!row["q"][n].is_number()){Reject(1,"Invalid quaternion");return;}q[n]=row["q"][n].get<float>();}const auto norm=glm::dot(q,q);if(!std::isfinite(norm)||std::abs(norm-1)>0.01f){Reject(1,"Nonunit quaternion");return;}
   if(!replicas.count(e.entity)&&replicas.size()+ ++newReplicas>64){Reject(1,"Session replica capacity reached");return;}
   if(next.count(e.id)||poses.count(e.entity)){Reject(1,"Duplicate replicated identity");return;}
   EntityPhysicalState pose;pose.position=glm::vec3(position-world.Settings().worldOrigin);pose.rotation=glm::normalize(q);pose.linearVelocity=glm::vec3(velocity);pose.angularVelocity=glm::vec3(angular);
   e.state=row["state"].dump();poses[e.entity]=pose;next[e.id]=std::move(e);
  }
  std::map<std::string,Remote> nextPeers;for(auto& row:j["peers"]){Remote r;if(!Text(row,"id",r.peer.id,20)||!Text(row,"name",r.peer.name,64)||!row.contains("connected")||!row["connected"].is_boolean()){Reject(1,"Invalid peer roster");return;}r.peer.connected=row["connected"].get<bool>();if(nextPeers.count(r.peer.id)){Reject(1,"Duplicate peer");return;}nextPeers[r.peer.id]=std::move(r);}
  for(auto& pair:replicas)pair.second.relevant=false;
  for(auto& pair:poses){auto& r=replicas[pair.first];if(!r.initialized){world.GetEntityState(pair.first,r.original);r.capturedOrigin=world.Settings().worldOrigin;r.visible=world.RenderVisible(pair.first);r.initialized=true;}
   Sample sample;sample.pose=world.RuntimeDefinition(pair.first)->transform;sample.absolute=world.Settings().worldOrigin+glm::dvec3(pair.second.position);sample.pose.position=pair.second.position;sample.pose.rotation=pair.second.rotation;sample.received=now;r.samples.push_back(sample);while(r.samples.size()>16)r.samples.pop_front();r.relevant=true;world.SetEntityState(pair.first,pair.second);world.SetRenderVisible(pair.first,true,r.visible);
  }
  for(auto& pair:replicas)if(!pair.second.relevant)world.SetRenderVisible(pair.first,true,false);
  entries=std::move(next);peers=std::move(nextPeers);tick=remoteTick;lastReceive=now;++snapshots;
 }
 void Snapshot(){
  Json roster=Json::array();roster.push_back({{"id","host"},{"name",options.name},{"connected",true}});for(auto& pair:peers)roster.push_back({{"id",pair.first},{"name",pair.second.peer.name},{"connected",pair.second.peer.connected}});
  for(auto& remote:peers){auto& peer=remote.second;if(!peer.peer.connected)continue;Json list=Json::array();for(auto& pair:entries){auto& e=pair.second;EntityPhysicalState pose;if(!world.RuntimeDefinition(e.entity)||!world.GetEntityState(e.entity,pose))continue;auto absolute=world.Settings().worldOrigin+glm::dvec3(pose.position);if(glm::length(absolute-peer.interest)>peer.radius)continue;
    list.push_back({{"id",e.id},{"entity",std::to_string(e.entity)},{"owner",e.owner},{"state",Json::parse(e.state)},{"p",V(absolute)},{"q",{pose.rotation.x,pose.rotation.y,pose.rotation.z,pose.rotation.w}},{"v",V(pose.linearVelocity)},{"w",V(pose.angularVelocity)}});
   }
   const Json message={{"t","snapshot"},{"tick",tick},{"entries",list},{"peers",roster}};if(message.dump().size()>65536){error="Snapshot exceeds 64 KiB; reduce replicated state/entities";++rejected;continue;}Send(peer.socket,message,true);
  }++snapshots;lastSnapshot=simulation;
 }
};
NetworkSession::NetworkSession(RuntimeWorld& w):p(std::make_unique<Impl>(w)){}
NetworkSession::~NetworkSession(){Close();}
bool NetworkSession::Host(const Options& o,std::string& e){if(Active()){e="Close the existing session first";return false;}if(!p->ValidOptions(o,true,e))return false;Close();p->options=o;p->mode="host";p->peerId="host";p->status="starting";p->session=Token();p->BeginIdentity();return true;}
bool NetworkSession::Join(const Options& o,std::string& e){if(Active()&&p->status!="disconnected"){e="Close the existing session first";return false;}if(!p->ValidOptions(o,false,e))return false;auto token=(p->options.address==o.address&&p->options.port==o.port)?p->reconnect:std::string();Close();p->reconnect=token;p->options=o;p->mode="client";p->status="starting";p->BeginIdentity();return true;}
void NetworkSession::Close(){
 if(p->identityJob.IsValid()){auto* r=p->world.Resources();if(r&&r->Jobs()){r->Jobs()->Cancel(p->identityJob);r->Jobs()->Forget(p->identityJob);}p->identityJob={};}p->identity.reset();
 p->transport.Stop();for(auto& pair:p->peers)p->RemovePlayer(pair.second);p->peers.clear();p->sockets.clear();p->pending.clear();
 for(auto& pair:p->replicas)if(p->world.RuntimeDefinition(pair.first)){auto original=pair.second.original;original.position=glm::vec3(pair.second.capturedOrigin+glm::dvec3(original.position)-p->world.Settings().worldOrigin);p->world.SetEntityState(pair.first,original);p->world.SetRenderVisible(pair.first,true,pair.second.visible);}
 p->replicas.clear();p->entries.clear();p->published.clear();p->outgoing.clear();p->mode="offline";p->status="offline";p->error.clear();p->peerId.clear();p->reconnect.clear();p->starting=false;p->lastPump=-1;p->tick=0;p->simulation=0;p->snapshots=0;p->inputSequence=0;p->lastSnapshot=-1;p->lastInputSend=-1;p->lastHeartbeat=-1;p->lastReceive=0;p->clientHandshake=-1;++p->generation;p->Gate(false);
}
void NetworkSession::Pump(double now){
 if(!Active()||!std::isfinite(now)||(p->lastPump>=0&&now<p->lastPump))return;
 p->now=now;p->lastPump=now;
 if(p->starting){auto* jobs=p->world.Resources()->Jobs();if(!jobs->IsFinished(p->identityJob))return;auto identity=p->identity;jobs->Forget(p->identityJob);p->identityJob={};p->identity.reset();p->starting=false;
  if(!identity->error.empty()||identity->value.empty()){p->error=identity->error.empty()?"Network content identity failed":identity->error;p->status="disconnected";return;}p->content=identity->value;
  NetworkTransport::Settings settings;settings.maxPeers=p->options.maxPeers;std::string e;bool ok=p->mode=="host"?p->transport.Listen(p->options.address,uint16_t(p->options.port),settings,e):p->transport.Connect(p->options.address,uint16_t(p->options.port),settings,e);if(!ok){p->error=e;p->status="disconnected";return;}p->status=p->mode=="host"?"listening":"connecting";
 }
 for(auto& pair:p->peers)if(auto* ps=p->world.PlayersIfLoaded())if(auto* player=ps->Find(pair.second.peer.player))player->input.BeginFrame();
 p->rejectedSockets.clear();
 // A live TCP socket is not a completed session. Check before consuming this
 // pump's messages so a late welcome cannot outrun the deadline; pings and
 // other received bytes deliberately do not renew this independent timeout.
 if(p->mode=="client"&&p->status=="connecting"&&p->clientHandshake>=0&&now-p->clientHandshake>=5)
  p->Reject(1,"Session handshake timed out");
 p->transport.Poll(now);
 for(auto& ev:p->transport.Events()){
  if(ev.type==NetworkTransport::Event::Connected){if(p->mode=="host")p->pending[ev.peer]=now;else{p->clientHandshake=now;p->Send(1,{{"t","hello"},{"version",1},{"content",p->content},{"name",p->options.name},{"token",p->reconnect}});}}
  else if(ev.type==NetworkTransport::Event::Disconnected||ev.type==NetworkTransport::Event::Error){if(p->mode=="client"){p->status="disconnected";p->error=ev.data;p->outgoing.clear();}else if(auto i=p->sockets.find(ev.peer);i!=p->sockets.end()){auto& r=p->peers.at(i->second);r.peer.connected=false;r.absent=now;p->Neutral(r);p->sockets.erase(i);p->lastSnapshot=-1;}p->pending.erase(ev.peer);}
  else if(ev.type==NetworkTransport::Event::Message){if(p->rejectedSockets.count(ev.peer))continue;try{auto j=Json::parse(ev.data,[](int depth,Json::parse_event_t,const Json&){if(depth>16)throw std::runtime_error("message nesting exceeds16");return true;});if(!j.is_object()||!j.contains("t")||!j["t"].is_string())throw std::runtime_error("message object/type required");if(p->mode=="host")p->HostMessage(ev.peer,j);else p->ClientMessage(j);}catch(const std::exception& ex){p->Reject(ev.peer,std::string("Invalid network message: ")+ex.what());}}
 }
 if(p->mode=="host")for(auto& pair:p->peers)if(!pair.second.peer.connected||now-pair.second.lastInput>.25)p->Neutral(pair.second);
 for(auto it=p->pending.begin();it!=p->pending.end();)if(now-it->second>5){auto id=it->first;it=p->pending.erase(it);p->Reject(id,"Handshake timed out");}else ++it;
 for(auto it=p->peers.begin();it!=p->peers.end();)if(p->mode=="host"&&!it->second.peer.connected&&now-it->second.absent>=15){for(auto& entry:p->entries)if(entry.second.owner==it->first)entry.second.owner.clear();p->RemovePlayer(it->second);it=p->peers.erase(it);}else ++it;
 if(p->mode=="client"&&p->status=="connected"&&now-p->lastInputSend>=1./30){p->Send(1,{{"t","input"},{"sequence",++p->inputSequence},{"values",p->outgoing}},true);p->lastInputSend=now;}
 if(now-p->lastHeartbeat>=1){if(p->mode=="host"){for(auto& pair:p->peers)if(pair.second.peer.connected)p->Send(pair.second.socket,{{"t","ping"}});}else if(p->status=="connected")p->Send(1,{{"t","ping"}});p->lastHeartbeat=now;}
}
void NetworkSession::BeforeFixed(float){/* Pump publishes levels/edges before the normal fixed-input snapshot. */}
void NetworkSession::AfterFixed(float dt){if(p->mode!="host"||p->status!="listening")return;p->simulation+=dt;++p->tick;for(auto it=p->entries.begin();it!=p->entries.end();)if(!p->world.RuntimeDefinition(it->second.entity))it=p->entries.erase(it);else ++it;if(p->lastSnapshot<0||p->simulation-p->lastSnapshot>=1./p->options.snapshotHz)p->Snapshot();}
bool NetworkSession::Active()const{return p->mode!="offline";}
bool NetworkSession::Client()const{return p->mode=="client";}
const std::string& NetworkSession::Mode()const{return p->mode;}
const std::string& NetworkSession::Status()const{return p->status;}
const std::string& NetworkSession::Error()const{return p->error;}
const std::string& NetworkSession::PeerId()const{return p->peerId;}
void NetworkSession::SetLaunch(std::string mode,std::string address,unsigned port,bool dedicated){p->launch={std::move(mode),std::move(address),port,dedicated};}
const NetworkSession::Launch& NetworkSession::LaunchOptions()const{return p->launch;}
std::vector<NetworkSession::Peer> NetworkSession::Peers()const{std::vector<Peer> out;if(p->mode=="host")out.push_back({"host",p->options.name,true,0});for(auto& pair:p->peers)out.push_back(pair.second.peer);return out;}
std::vector<NetworkSession::Entry> NetworkSession::Entries()const{std::vector<Entry> out;for(auto& pair:p->entries)out.push_back(pair.second);return out;}
bool NetworkSession::Replicate(EntityId entity,const std::string& owner,const std::string& state,std::string& id,std::string& e){
 EntityPhysicalState initial;
 if(p->mode!="host"||!p->world.GetEntityState(entity,initial)||!Supported(p->world.RuntimeDefinition(entity))||(p->entries.size()>=64||(!p->published.count(entity)&&p->published.size()>=64))||!Data(state)){e="Host replication requires an authored root, supported representation, <=64 entries and <=1024-byte JSON state";return false;}
 if(!owner.empty()&&owner!="host"&&!p->peers.count(owner)){e="Unknown owner peer";return false;}for(auto& pair:p->entries)if(pair.second.entity==entity){e="Entity already replicated";return false;}
 id=p->session+":"+std::to_string(p->nextEntity++);p->published.insert(entity);p->entries[id]={id,owner,state,entity};p->lastSnapshot=-1;return true;
}
bool NetworkSession::Forget(const std::string& id,std::string& e){if(p->mode!="host"||!p->entries.erase(id)){e="Host and current network entity required";return false;}p->lastSnapshot=-1;return true;}
bool NetworkSession::SetOwner(const std::string& id,const std::string& peer,std::string& e){auto it=p->entries.find(id);if(p->mode!="host"||it==p->entries.end()||(!peer.empty()&&peer!="host"&&!p->peers.count(peer))){e="Host, current network entity and known peer required";return false;}if(it->second.owner!=peer){it->second.owner=peer;p->lastSnapshot=-1;}return true;}
bool NetworkSession::SetState(const std::string& id,const std::string& json,std::string& e){auto it=p->entries.find(id);if(p->mode!="host"||it==p->entries.end()||!Data(json)){e="Host, current entry and <=1024-byte JSON state required";return false;}it->second.state=json;return true;}
bool NetworkSession::SendInput(const std::map<std::string,float>& value,std::string& e){Json j=value;std::map<std::string,float> out;if(p->mode!="client"||p->status!="connected"||!p->InputValue(j,out)){e="Connected client and declared finite [-1,1] scalar controls required";return false;}bool actionChanged=false;for(const auto& name:p->options.controls){const auto* entry=p->world.SceneControl()->ProjectConfig().Settings().input.Find(name);if(entry&&!entry->axis){const float before=p->outgoing.count(name)?p->outgoing.at(name):0,after=out.count(name)?out.at(name):0;actionChanged|=before!=after;}}
 if(actionChanged&&!p->Send(1,{{"t","input"},{"sequence",++p->inputSequence},{"values",out}},false)){e=p->error;return false;}
 p->outgoing=std::move(out);return true;}
std::map<std::string,float> NetworkSession::Input(const std::string& id)const{auto it=p->peers.find(id);if(p->mode!="host"||it==p->peers.end()||!it->second.peer.connected||p->now-it->second.lastInput>.25)return {};return it->second.input;}
bool NetworkSession::SetInterest(glm::dvec3 position,double radius,std::string& e){if(p->mode!="client"||p->status!="connected"||!std::isfinite(radius)||radius<1||radius>1e9||!std::isfinite(position.x)||!std::isfinite(position.y)||!std::isfinite(position.z)||glm::length(position)>1e15){e="Connected client and finite absolute position/radius1..1e9 required";return false;}return p->Send(1,{{"t","interest"},{"position",V(position)},{"radius",radius}});}
bool NetworkSession::Presentation(EntityId id,SceneTransform& pose)const{auto it=p->replicas.find(id);if(p->mode!="client"||it==p->replicas.end()||!it->second.initialized)return false;const auto& samples=it->second.samples;if(samples.empty())return false;double target=p->now-.1;const auto* a=&samples.front();const auto* b=a;for(auto& sample:samples){b=&sample;if(sample.received>=target)break;a=&sample;}float t=b->received>a->received?float(std::clamp((target-a->received)/(b->received-a->received),0.,1.)):1;pose.position=glm::vec3(glm::mix(a->absolute,b->absolute,double(t))-p->world.Settings().worldOrigin);pose.rotation=glm::normalize(glm::slerp(a->pose.rotation,b->pose.rotation,t));return true;}
NetworkSession::Statistics NetworkSession::Stats()const{return {p->transport.GetStats(),p->snapshots,p->rejected,p->mode=="client"&&p->snapshots?std::max(0.,p->now-p->lastReceive):0,p->entries.size(),p->peers.size()};}
uint64_t NetworkSession::Generation()const{return p->generation;}
uint16_t NetworkSession::Port()const{return p->transport.BoundPort();}
