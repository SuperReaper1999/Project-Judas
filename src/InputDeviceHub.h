#pragma once
#include "InputSystem.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

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
