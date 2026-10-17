#pragma once
#include "InputSystem.h"
#include "Renderer.h"
#include "Scene.h"
#include <memory>
#include <optional>
#include <set>

// Optional raw device inventory. No player/entity/game policy lives here.
class InputDeviceHub {
public:
 struct Device {std::string id,kind,name;bool connected=true;std::map<std::string,float> values;InputPointer pointer;};
 struct TouchChange {InputTouch contact;bool down=false;};
 std::vector<TouchChange> touchChanges;
 std::vector<TextInputEvent> textEvents;std::shared_ptr<TextInputPlatform> textPlatform;
 void RecordTouch(InputTouch contact,bool down);
 struct Change {std::string device,control;float value=0;};
 InputDeviceHub();
 std::string Connect(const std::string& kind,const std::string& name);
 void Disconnect(const std::string& id);
 void BeginFrame();void Reset();
 void Set(const std::string& device,const std::string& control,float value);
 void Pointer(const std::string& device,InputPointer value);
 void Touches(const std::vector<InputTouch>& value){touches=value;}
 const Device* Find(const std::string& id)const;
 const std::map<std::string,Device>& Devices()const{return devices;}
 const std::vector<Change>& Changes()const{return changes;}
 std::vector<InputTouch> touches;std::uint64_t resetRevision=0,frame=0;bool overflow=false;
private:
 std::map<std::string,Device> devices;std::vector<Change> changes;
};

// Identities are world-owned, process-monotonic tokens. They neither own an
// entity nor imply attachment, camera policy, gameplay or network authority.
class LocalPlayers {
public:
 struct Assignment {std::string device;std::set<std::string> controls;glm::vec4 region{0,0,1,1};};
 struct Player {
  std::uint64_t id=0,entity=0;std::string name;InputSystem input;std::vector<Assignment> devices;
  glm::vec4 viewport{0,0,1,1};std::optional<SceneTransform> view;
  // Retained per-device levels compose before logical mapping; no source may
  // cancel a different held source by publishing its own release.
  std::map<std::string,std::map<std::string,float>> sourceValues;
  float fov=70,nearPlane=.1f,farPlane=500;CategoryMask renderMask=~CategoryMask(0);
  RenderTargetHandle target;glm::ivec2 attemptedSize{0};bool attemptedLinear=false;std::string renderError;bool baseline=true;std::set<std::uint64_t> touchSources;std::set<std::string> connectedDevices;
 };
 std::shared_ptr<InputDeviceHub> devices;
 ~LocalPlayers();
 Player* Create(const std::string& name,const InputMap& map,std::string& error);
 bool Remove(std::uint64_t id);Player* Find(std::uint64_t id);const Player* Find(std::uint64_t id)const;
 bool Assign(std::uint64_t id,const InputDeviceHub& hub,Assignment assignment,std::string& error);
 bool Unassign(std::uint64_t id,const std::string& device);
 void Route(const InputDeviceHub& hub);void BeginFixedStep();void DiscardPending();
 bool SetViewport(std::uint64_t id,glm::vec4 rect,std::string& error);
 bool SetView(std::uint64_t id,const SceneTransform& pose,float fov,float nearPlane,float farPlane,std::string& error);
 const std::map<std::uint64_t,Player>& All()const{return players;}
 std::map<std::uint64_t,Player>& All(){return players;}
 bool HasViews()const;
 void BindRenderer(Renderer& r){renderer=&r;}
 static bool ValidRect(glm::vec4 rect);
 static glm::ivec2 ViewSize(glm::vec4 rect,int width,int height);
private:
 std::map<std::uint64_t,Player> players;std::map<std::uint64_t,std::uint64_t> touchOwners;
 std::uint64_t revision=0,lastFrame=~std::uint64_t(0);Renderer* renderer=nullptr;
};
