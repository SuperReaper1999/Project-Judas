#include "InputDeviceHub.h"
#include <atomic>
#include <cmath>
namespace {std::atomic<std::uint64_t> nextDevice{1};}
InputDeviceHub::InputDeviceHub(){for(auto kind:{"keyboard","mouse","touch"})devices[kind]={kind,kind,std::string("SDL ")+kind,true,{},{}};changes.reserve(256);}
std::string InputDeviceHub::Connect(const std::string& kind,const std::string& name){
 unsigned live=0;for(auto& d:devices)live+=d.second.connected;if(live>=19)return {};
 // Retire disconnected device records; their tokens are never recycled.
 for(auto it=devices.begin();it!=devices.end();)if(!it->second.connected)it=devices.erase(it);else ++it;
 auto id=kind+":"+std::to_string(nextDevice++);devices[id]={id,kind,name,true,{},{}};return id;
}
const InputDeviceHub::Device* InputDeviceHub::Find(const std::string& id)const{auto it=devices.find(id);return it==devices.end()?nullptr:&it->second;}
void InputDeviceHub::Disconnect(const std::string& id){auto it=devices.find(id);if(it==devices.end())return;it->second.connected=false;it->second.values.clear();}
void InputDeviceHub::BeginFrame(){textEvents.clear();++frame;changes.clear();touchChanges.clear();overflow=false;for(auto& d:devices)for(auto control:{"mouse:dx","mouse:dy","mouse:wheelX","mouse:wheelY"}){auto it=d.second.values.find(control);if(it!=d.second.values.end())it->second=0;}}
void InputDeviceHub::Reset(){textEvents.clear();for(auto& d:devices){d.second.values.clear();d.second.pointer={};}touches.clear();changes.clear();touchChanges.clear();++resetRevision;}
void InputDeviceHub::Set(const std::string& device,const std::string& control,float value){
 auto it=devices.find(device);if(it==devices.end()||!it->second.connected||!std::isfinite(value)||overflow)return;
 auto& old=it->second.values[control];if(old==value)return;old=value;
 if(changes.size()>=4096){Reset();overflow=true;return;}changes.push_back({device,control,value});
}
void InputDeviceHub::RecordTouch(InputTouch contact,bool down){if(!contact.id||!std::isfinite(contact.x)||!std::isfinite(contact.y)||!std::isfinite(contact.pressure)||overflow)return;if(touchChanges.size()>=4096){Reset();overflow=true;return;}touchChanges.push_back({contact,down});}
void InputDeviceHub::Pointer(const std::string& device,InputPointer value){auto it=devices.find(device);if(it!=devices.end())it->second.pointer=value;}
