#include "InputSystem.h"
#include "NamedAuthoring.h"
#include "NamedInput.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <locale>
#include <set>

namespace {
bool Name(const std::string& n){return !n.empty()&&n.size()<=128&&n.find_first_of("\r\n") == std::string::npos;}
bool Binding(const InputBinding& b){
    const auto colon=b.control.find(':');const auto type=b.control.substr(0,colon);
    return colon!=std::string::npos&&colon+1<b.control.size()&&Name(b.control)&&
      (type=="key"||type=="mouse"||type=="pad"||type=="stick")&&
      std::isfinite(b.scale)&&std::isfinite(b.deadzone)&&b.deadzone>=0&&b.deadzone<1;
}
}
InputEntry* InputMap::Find(const std::string& n){for(auto& e:entries)if(e.name==n)return &e;return nullptr;}
const InputEntry* InputMap::Find(const std::string& n)const{for(const auto& e:entries)if(e.name==n)return &e;return nullptr;}
bool InputMap::Add(const std::string& n,bool axis){if(!Name(n)||Find(n))return false;entries.push_back({n,axis,{}});return true;}
bool InputMap::Rename(const std::string& from,const std::string& to){auto* e=Find(from);if(!e||!Name(to)||(to!=from&&Find(to)))return false;e->name=to;return true;}
bool InputMap::Remove(const std::string& n){auto it=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.name==n;});if(it==entries.end())return false;entries.erase(it);return true;}
bool InputMap::AddBinding(const std::string& n,InputBinding b){auto* e=Find(n);if(!e||!Binding(b))return false;e->bindings.push_back(std::move(b));return true;}
bool InputMap::ReplaceBinding(const std::string& n,std::size_t i,InputBinding b){auto* e=Find(n);if(!e||i>=e->bindings.size()||!Binding(b))return false;e->bindings[i]=std::move(b);return true;}
bool InputMap::RemoveBinding(const std::string& n,std::size_t i){auto* e=Find(n);if(!e||i>=e->bindings.size())return false;e->bindings.erase(e->bindings.begin()+i);return true;}
bool InputMap::Validate(std::string& error)const{
    std::set<std::string> names;
    for(const auto& e:entries){if(!Name(e.name)||!names.insert(e.name).second){error="invalid/duplicate input name";return false;}for(const auto& b:e.bindings)if(!Binding(b)){error="invalid input binding";return false;}}
    error.clear();return true;
}
std::string InputMap::Serialize()const{
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<"1 "<<entries.size()<<' ';
    for(const auto& e:entries){out<<std::quoted(e.name)<<' '<<e.axis<<' '<<e.bindings.size()<<' ';for(const auto& b:e.bindings)out<<std::quoted(b.control)<<' '<<b.scale<<' '<<b.deadzone<<' ';}return out.str();
}
bool InputMap::Parse(const std::string& text,InputMap& out,std::string& error){
    if(IsNamedDocument(text))return AuthoringJSON::ParseInputDocument(text,out,error);
    std::istringstream in(text);in.imbue(std::locale::classic());int version=0;std::size_t count=0;InputMap result;
    if(!(in>>version>>count)||version!=1||count>10000){error="unsupported/invalid input map";return false;}
    for(std::size_t i=0;i<count;++i){InputEntry e;int axis;std::size_t bindings;if(!(in>>std::quoted(e.name)>>axis>>bindings)||axis<0||axis>1||bindings>10000){error="malformed input entry";return false;}e.axis=axis;
        for(std::size_t b=0;b<bindings;++b){InputBinding value;if(!(in>>std::quoted(value.control)>>value.scale>>value.deadzone)){error="malformed input binding";return false;}e.bindings.push_back(value);}result.entries.push_back(e);}
    in>>std::ws;if(!in.eof()||!result.Validate(error)){if(error.empty())error="trailing input data";return false;}out=std::move(result);return true;
}
InputMap InputMap::Defaults(){
    InputMap m;
    const std::pair<const char*,const char*> actions[]={
      {"move_forward","W"},{"move_backward","S"},{"strafe_left","A"},{"strafe_right","D"},{"move_up","E"},{"move_down","Q"},
      {"pitch_up","I"},{"pitch_down","K"},{"yaw_left","J"},{"yaw_right","L"},{"roll_left","U"},{"roll_right","O"},
      {"prograde","P"},{"retrograde","M"},{"radial","N"},{"add_water","B"},{"igniter","C"},
      {"reset","R"},{"jump","Space"},{"control_toggle","F"},{"torch_toggle","T"},{"interact","G"},{"view_toggle","V"},{"throw","H"},{"sas_toggle","X"},
      {"spawn","Z"},{"destroy","Y"},{"save_state","F6"},{"delete_state","F7"},{"pause","Escape"},{"ui_up","Up"},{"ui_down","Down"},{"ui_activate","Return"},{"ui_left","Left"},{"ui_right","Right"}};
    for(auto a:actions){m.Add(a.first,false);m.AddBinding(a.first,{std::string("key:")+a.second});}
    for(auto a:{std::pair<const char*,const char*>{"move_forward","Up"},{"move_backward","Down"},{"strafe_left","Left"},{"strafe_right","Right"},{"ui_activate","Keypad Enter"}})m.AddBinding(a.first,{std::string("key:")+a.second});
    for(auto a:{std::pair<const char*,const char*>{"jump","South"},{"interact","West"},{"pause","Start"},{"ui_activate","South"},{"ui_up","DpadUp"},{"ui_down","DpadDown"},{"ui_left","DpadLeft"},{"ui_right","DpadRight"}})m.AddBinding(a.first,{std::string("pad:")+a.second});
    m.Add("ui_click",false);m.AddBinding("ui_click",{"mouse:Left"});
    auto axis=[&](const char* name,const char* negative,const char* positive,const char* stick){m.Add(name,true);m.AddBinding(name,{std::string("key:")+negative,-1});m.AddBinding(name,{std::string("key:")+positive,1});if(stick)m.AddBinding(name,{std::string("stick:")+stick,1,.15f});};
    axis("move_x","A","D","LeftX");axis("move_y","S","W",nullptr);m.AddBinding("move_y",{"stick:LeftY",-1,.15f});
    axis("move_z","Q","E",nullptr);axis("pitch","K","I",nullptr);axis("yaw","L","J",nullptr);axis("roll","U","O",nullptr);
    for(auto a:{std::pair<const char*,const char*>{"move_x","Left"},{"move_x","Right"},{"move_y","Down"},{"move_y","Up"}})m.AddBinding(a.first,{std::string("key:")+a.second,(std::string(a.second)=="Left"||std::string(a.second)=="Down")?-1.f:1.f});
    for(const char* name:{"look_x","look_y","wheel_x","wheel_y"})m.Add(name,true);
    m.AddBinding("look_x",{"mouse:dx"});m.AddBinding("look_y",{"mouse:dy"});
    m.Add("look_stick_x",true);m.Add("look_stick_y",true);m.AddBinding("look_stick_x",{"stick:RightX",1,.15f});m.AddBinding("look_stick_y",{"stick:RightY",1,.15f});
    m.AddBinding("wheel_x",{"mouse:wheelX"});m.AddBinding("wheel_y",{"mouse:wheelY"});m.Add("throttle",true);m.AddBinding("throttle",{"stick:RightTrigger",1,.05f});return m;
}
InputSystem::InputSystem():m_map(InputMap::Defaults()){Evaluate();}
bool InputSystem::SetMap(const InputMap& map,std::string& error){if(!map.Validate(error))return false;m_map=map;Reset();return true;}
float InputSystem::Deadzone(float v,float d){return std::abs(v)<=d?0:std::copysign((std::min(1.f,std::abs(v))-d)/(1-d),v);}
void InputSystem::Evaluate(){
    for(const auto& e:m_map.entries){float value=0;bool held=false;
        for(const auto& b:e.bindings){auto it=m_raw.find(b.control);float v=it==m_raw.end()?0:it->second;if(b.control.rfind("stick:",0)==0)v=Deadzone(v,b.deadzone);v*=b.scale;
            value+=v;
            held=held||v>.5f;}
        if(e.axis){const bool relative=std::any_of(e.bindings.begin(),e.bindings.end(),[](const auto& b){return b.control=="mouse:dx"||b.control=="mouse:dy"||b.control=="mouse:wheelX"||b.control=="mouse:wheelY";});m_axes[e.name]=relative?value:std::clamp(value,-1.f,1.f);}
        else {auto& s=m_actions[e.name];if(held!=s.held){s.pressed|=held;s.released|=!held;auto& p=m_pending[e.name];p.pressed|=held;p.released|=!held;}s.held=held;}
    }
}
void InputSystem::BeginFrame(){m_consumed.clear();for(auto& p:m_actions){p.second.pressed=false;p.second.released=false;}for(const char* c:{"mouse:dx","mouse:dy","mouse:wheelX","mouse:wheelY"})m_raw[c]=0;Evaluate();}
void InputSystem::SetPhysical(const std::string& c,float v){if(!std::isfinite(v))return;m_raw[c]=v;Evaluate();}
void InputSystem::AddDelta(const std::string& c,float v){if(std::isfinite(v))SetPhysical(c,m_raw[c]+v);}
void InputSystem::ClearDevice(const std::string& prefix){for(auto& p:m_raw)if(p.first.rfind(prefix,0)==0)p.second=0;Evaluate();}
void InputSystem::Reset(){m_consumed.clear();m_raw.clear();m_axes.clear();m_actions.clear();m_pending.clear();m_fixed.clear();Evaluate();DiscardPending();}
void InputSystem::DiscardPending(){m_pending.clear();m_fixed.clear();for(auto& s:m_actions){s.second.pressed=false;s.second.released=false;}}
InputActionState InputSystem::Action(const std::string& n)const{if(std::find(m_consumed.begin(),m_consumed.end(),n)!=m_consumed.end())return {};auto it=m_actions.find(n);return it==m_actions.end()?InputActionState{}:it->second;}
float InputSystem::Axis(const std::string& n)const{if(std::find(m_consumed.begin(),m_consumed.end(),n)!=m_consumed.end())return 0;auto it=m_axes.find(n);return it==m_axes.end()?0:it->second;}
void InputSystem::BeginFixedStep()const{m_fixed=m_actions;for(auto& p:m_fixed){auto it=m_pending.find(p.first);p.second.pressed=it!=m_pending.end()&&it->second.pressed;p.second.released=it!=m_pending.end()&&it->second.released;}m_pending.clear();}
InputActionState InputSystem::FixedAction(const std::string& n)const{if(std::find(m_consumed.begin(),m_consumed.end(),n)!=m_consumed.end())return {};auto it=m_fixed.find(n);return it==m_fixed.end()?InputActionState{}:it->second;}

void InputSystem::ConsumeBindings(const std::vector<std::string>& names){
    std::set<std::string> controls;for(const auto& n:names)if(auto* e=m_map.Find(n))for(const auto& b:e->bindings)controls.insert(b.control);
    for(const auto& e:m_map.entries)if(std::find(names.begin(),names.end(),e.name)!=names.end()||std::any_of(e.bindings.begin(),e.bindings.end(),[&](const auto& b){return controls.count(b.control);})){
        if(std::find(m_consumed.begin(),m_consumed.end(),e.name)==m_consumed.end())m_consumed.push_back(e.name);
        m_pending.erase(e.name);m_fixed.erase(e.name);
    }
}
