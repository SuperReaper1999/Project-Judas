#pragma once
#include "Scene.h"
#include "EntityLifecycle.h"
#include "NetworkTransport.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
class RuntimeWorld;
// Optional owner-thread server-authoritative session. Transport identities,
// session peer identities, local input players and entity handles are distinct.
class NetworkSession {
public:
 struct Options {
  std::string address="0.0.0.0",name="Player";
  unsigned port=27999,maxPeers=4,snapshotHz=20;
  std::vector<std::string> controls;
 };
 struct Launch {std::string mode,address="127.0.0.1";unsigned port=27999;bool dedicated=false;};
 struct Peer {std::string id,name;bool connected=false;uint64_t player=0;};
 struct Entry {std::string id,owner,state="null";EntityId entity=0;};
 struct Statistics {NetworkTransport::Stats transport;uint64_t snapshots=0,rejected=0;double snapshotAge=0;size_t entities=0,peers=0;};
 explicit NetworkSession(RuntimeWorld&);
 ~NetworkSession();
 bool Host(const Options&,std::string& error);
 bool Join(const Options&,std::string& error);
 void Close();
 void Pump(double monotonicSeconds);
 void BeforeFixed(float);
 void AfterFixed(float);
 bool Active()const;
 bool Client()const;
 const std::string& Mode()const;
 const std::string& Status()const;
 const std::string& Error()const;
 const std::string& PeerId()const;
 void SetLaunch(std::string mode,std::string address,unsigned port,bool dedicated);
 const Launch& LaunchOptions()const;
 std::vector<Peer> Peers()const;
 std::vector<Entry> Entries()const;
 bool Replicate(EntityId,const std::string& owner,const std::string& state,std::string& id,std::string& error);
 bool Forget(const std::string& id,std::string& error);
 bool SetOwner(const std::string& id,const std::string& peer,std::string& error);
 bool SetState(const std::string& id,const std::string& json,std::string& error);
 bool SendInput(const std::map<std::string,float>&,std::string& error);
 std::map<std::string,float> Input(const std::string& peer)const;
 bool SetInterest(glm::dvec3 absolutePosition,double radius,std::string& error);
 bool Presentation(EntityId,SceneTransform&)const;
 Statistics Stats()const;
 uint64_t Generation()const;
 uint16_t Port()const;
private:
 struct Impl;std::unique_ptr<Impl> p;
};
