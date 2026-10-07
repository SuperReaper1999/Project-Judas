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
      std::isfinite(b.scale)&&std::isfinite(b.scaleY)&&std::isfinite(b.deadzone)&&b.deadzone>=0&&b.deadzone<1;
}
bool Paired(const InputBinding& b){return b.control=="stick:Left"||b.control=="stick:Right";}
bool BindingFor(const InputEntry& e,const InputBinding& b){
    return Binding(b)&&(e.vector?(!e.axis&&Paired(b)):
        (!Paired(b)&&b.scaleY==1&&!b.circular));
}
int StickIndex(const std::string& side){return side=="left"?0:side=="right"?1:-1;}
std::string StickControl(int index,const char* axis){return std::string("stick:")+(index==0?"Left":"Right")+axis;}
}
InputEntry* InputMap::Find(const std::string& n){for(auto& e:entries)if(e.name==n)return &e;return nullptr;}
const InputEntry* InputMap::Find(const std::string& n)const{for(const auto& e:entries)if(e.name==n)return &e;return nullptr;}
bool InputMap::Add(const std::string& n,bool axis){if(!Name(n)||Find(n))return false;entries.push_back({n,axis,{}});return true;}
bool InputMap::AddVector(const std::string& n){if(!Add(n,false))return false;entries.back().vector=true;return true;}
bool InputMap::Rename(const std::string& from,const std::string& to){auto* e=Find(from);if(!e||!Name(to)||(to!=from&&Find(to)))return false;e->name=to;return true;}
bool InputMap::Remove(const std::string& n){auto it=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.name==n;});if(it==entries.end())return false;entries.erase(it);return true;}
bool InputMap::AddBinding(const std::string& n,InputBinding b){auto* e=Find(n);if(!e||!BindingFor(*e,b))return false;e->bindings.push_back(std::move(b));return true;}
bool InputMap::ReplaceBinding(const std::string& n,std::size_t i,InputBinding b){auto* e=Find(n);if(!e||i>=e->bindings.size()||!BindingFor(*e,b))return false;e->bindings[i]=std::move(b);return true;}
bool InputMap::RemoveBinding(const std::string& n,std::size_t i){auto* e=Find(n);if(!e||i>=e->bindings.size())return false;e->bindings.erase(e->bindings.begin()+i);return true;}
bool InputMap::Validate(std::string& error)const{
    std::set<std::string> names;
    for(const auto& e:entries){if(!Name(e.name)||!names.insert(e.name).second){error="invalid/duplicate input name";return false;}if(e.axis&&e.vector){error="input cannot be both scalar axis and paired vector: "+e.name;return false;}for(const auto& b:e.bindings)if(!BindingFor(e,b)){error="invalid input binding: "+e.name+" (paired vectors require stick:Left/Right; scalar bindings retain their original settings)";return false;}}
    error.clear();return true;
}
std::string InputMap::Serialize()const{
    // Existing maps keep their exact canonical v1 representation/save identity.
    const bool paired=std::any_of(entries.begin(),entries.end(),[](const auto& e){return e.vector;});
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<(paired?2:1)<<' '<<entries.size()<<' ';
    for(const auto& e:entries){out<<std::quoted(e.name)<<' '<<(e.vector?2:int(e.axis))<<' '<<e.bindings.size()<<' ';for(const auto& b:e.bindings){out<<std::quoted(b.control)<<' '<<b.scale<<' '<<b.deadzone<<' ';if(paired)out<<b.scaleY<<' '<<b.circular<<' ';}}return out.str();
}
bool InputMap::Parse(const std::string& text,InputMap& out,std::string& error){
    if(IsNamedDocument(text))return AuthoringJSON::ParseInputDocument(text,out,error);
    std::istringstream in(text);in.imbue(std::locale::classic());int version=0;std::size_t count=0;InputMap result;
    if(!(in>>version>>count)||(version!=1&&version!=2)||count>10000){error="unsupported/invalid input map";return false;}
    for(std::size_t i=0;i<count;++i){InputEntry e;int kind;std::size_t bindings;if(!(in>>std::quoted(e.name)>>kind>>bindings)||kind<0||kind>(version==1?1:2)||bindings>10000){error="malformed input entry";return false;}e.axis=kind==1;e.vector=kind==2;
        for(std::size_t b=0;b<bindings;++b){InputBinding value;if(!(in>>std::quoted(value.control)>>value.scale>>value.deadzone)){error="malformed input binding";return false;}if(version==2){int circular;if(!(in>>value.scaleY>>circular)||circular<0||circular>1){error="malformed paired input binding";return false;}value.circular=circular!=0;}e.bindings.push_back(value);}result.entries.push_back(e);}
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
InputVector InputSystem::CircularDeadzone(InputVector v,float d){
    const float magnitude=std::hypot(v.x,v.y);
    if(magnitude<=d)return {};
    const float response=std::min(1.f,(magnitude-d)/(1-d))/magnitude;
    return {v.x*response,v.y*response};
}
void InputSystem::Evaluate(){
    for(const auto& e:m_map.entries){float value=0;bool held=false;
        if(e.vector){
            double sumX=0,sumY=0;
            for(const auto& b:e.bindings){const int index=b.control=="stick:Left"?0:1;
                InputVector v{m_raw[StickControl(index,"X")],m_raw[StickControl(index,"Y")]};
                if(b.circular)v=CircularDeadzone(v,b.deadzone);
                else v={Deadzone(v.x,b.deadzone),Deadzone(v.y,b.deadzone)};
                sumX+=double(v.x)*b.scale;sumY+=double(v.y)*b.scaleY;
            }
            m_vectors[e.name]={float(std::clamp(sumX,-1.0,1.0)),float(std::clamp(sumY,-1.0,1.0))};
            continue;
        }
        for(const auto& b:e.bindings){auto it=m_raw.find(b.control);float v=it==m_raw.end()?0:it->second;if(b.control.rfind("stick:",0)==0)v=Deadzone(v,b.deadzone);v*=b.scale;
            value+=v;
            held=held||v>.5f;}
        if(e.axis){const bool relative=std::any_of(e.bindings.begin(),e.bindings.end(),[](const auto& b){return b.control=="mouse:dx"||b.control=="mouse:dy"||b.control=="mouse:wheelX"||b.control=="mouse:wheelY";});m_axes[e.name]=relative?value:std::clamp(value,-1.f,1.f);}
        else {auto& s=m_actions[e.name];if(held!=s.held){s.pressed|=held;s.released|=!held;auto& p=m_pending[e.name];p.pressed|=held;p.released|=!held;}s.held=held;}
    }
}
void InputSystem::BeginFrame(){m_consumed.clear();for(int i=0;i<2;++i)m_stickFrameStart[i]={m_raw[StickControl(i,"X")],m_raw[StickControl(i,"Y")]};for(auto& p:m_actions){p.second.pressed=false;p.second.released=false;}for(const char* c:{"mouse:dx","mouse:dy","mouse:wheelX","mouse:wheelY"})m_raw[c]=0;Evaluate();}
void InputSystem::SetPhysical(const std::string& c,float v){
    if(!std::isfinite(v))return;
    for(int i=0;i<2;++i)if(c==StickControl(i,"X")||c==StickControl(i,"Y")){
        SetStick(i==0?"left":"right",c==StickControl(i,"X")?v:m_raw[StickControl(i,"X")],c==StickControl(i,"Y")?v:m_raw[StickControl(i,"Y")]);return;
    }
    m_raw[c]=v;Evaluate();
}
void InputSystem::SetStick(const std::string& side,float x,float y,bool baseline){
    const int i=StickIndex(side);if(i<0||!std::isfinite(x)||!std::isfinite(y))return;
    x=std::clamp(x,-1.f,1.f);y=std::clamp(y,-1.f,1.f);
    const bool changed=x!=m_raw[StickControl(i,"X")]||y!=m_raw[StickControl(i,"Y")];
    m_raw[StickControl(i,"X")]=x;m_raw[StickControl(i,"Y")]=y;
    if(baseline||!m_stickReady[i]){m_stickFrameStart[i]={x,y};m_stickReady[i]=true;}
    else if(changed){
        auto& history=m_stickHistory[i];
        history.push_back({++m_stickSequence,std::chrono::duration<double>(std::chrono::steady_clock::now()-m_inputClock).count(),x,y});
        if(history.size()>128){m_stickDroppedThrough[i]=history.front().sequence;history.pop_front();}
    }
    Evaluate();
}
void InputSystem::AddDelta(const std::string& c,float v){if(std::isfinite(v))SetPhysical(c,m_raw[c]+v);}
void InputSystem::ClearDevice(const std::string& prefix){for(auto& p:m_raw)if(p.first.rfind(prefix,0)==0)p.second=0;if(prefix.rfind("stick:",0)==0)DiscardStickHistory();Evaluate();}
void InputSystem::Reset(){m_consumed.clear();m_raw.clear();m_axes.clear();m_vectors.clear();m_actions.clear();m_pending.clear();m_fixed.clear();Evaluate();DiscardPending();}
void InputSystem::DiscardPending(){m_pending.clear();m_fixed.clear();for(auto& s:m_actions){s.second.pressed=false;s.second.released=false;}DiscardStickHistory();}
void InputSystem::DiscardStickHistory(){
    m_stickResetSequence=++m_stickSequence;
    for(int i=0;i<2;++i){m_stickHistory[i].clear();m_stickDroppedThrough[i]=0;m_stickReady[i]=false;m_stickFrameStart[i]={m_raw[StickControl(i,"X")],m_raw[StickControl(i,"Y")]};}
}
bool InputSystem::StickConsumed(const std::string& side)const{
    const int i=StickIndex(side);if(i<0)return false;
    const auto prefix=StickControl(i,"");
    for(const auto& name:m_consumed)if(const auto* entry=m_map.Find(name))for(const auto& binding:entry->bindings)if(binding.control==prefix||binding.control==prefix+"X"||binding.control==prefix+"Y")return true;
    return false;
}
InputVector InputSystem::Stick(const std::string& side)const{
    const int i=StickIndex(side);if(i<0||StickConsumed(side))return {};
    const auto x=m_raw.find(StickControl(i,"X")),y=m_raw.find(StickControl(i,"Y"));
    return {x==m_raw.end()?0:x->second,y==m_raw.end()?0:y->second};
}
InputVector InputSystem::StickDelta(const std::string& side)const{
    const int i=StickIndex(side);if(i<0||StickConsumed(side))return {};
    const auto v=Stick(side);return {v.x-m_stickFrameStart[i].x,v.y-m_stickFrameStart[i].y};
}
InputVector InputSystem::Vector(const std::string& name)const{
    if(std::find(m_consumed.begin(),m_consumed.end(),name)!=m_consumed.end())return {};
    const auto it=m_vectors.find(name);return it==m_vectors.end()?InputVector{}:it->second;
}
InputStickSnapshot InputSystem::StickSamples(const std::string& side,std::uint64_t after)const{
    InputStickSnapshot result;result.sequence=m_stickSequence;result.reset=after<m_stickResetSequence;
    const int i=StickIndex(side);if(i<0)return result;
    result.overflow=after<m_stickDroppedThrough[i];
    if(!StickConsumed(side))for(const auto& sample:m_stickHistory[i])if(sample.sequence>after)result.samples.push_back(sample);
    return result;
}
InputActionState InputSystem::Action(const std::string& n)const{if(std::find(m_consumed.begin(),m_consumed.end(),n)!=m_consumed.end())return {};auto it=m_actions.find(n);return it==m_actions.end()?InputActionState{}:it->second;}
float InputSystem::Axis(const std::string& n)const{if(std::find(m_consumed.begin(),m_consumed.end(),n)!=m_consumed.end())return 0;auto it=m_axes.find(n);return it==m_axes.end()?0:it->second;}
void InputSystem::BeginFixedStep()const{m_fixed=m_actions;for(auto& p:m_fixed){auto it=m_pending.find(p.first);p.second.pressed=it!=m_pending.end()&&it->second.pressed;p.second.released=it!=m_pending.end()&&it->second.released;}m_pending.clear();}
InputActionState InputSystem::FixedAction(const std::string& n)const{if(std::find(m_consumed.begin(),m_consumed.end(),n)!=m_consumed.end())return {};auto it=m_fixed.find(n);return it==m_fixed.end()?InputActionState{}:it->second;}

void InputSystem::ConsumeBindings(const std::vector<std::string>& names){
    std::set<std::string> controls;auto add=[&](const InputBinding& b){controls.insert(b.control);if(Paired(b)){controls.insert(b.control+"X");controls.insert(b.control+"Y");}};
    for(const auto& n:names)if(auto* e=m_map.Find(n))for(const auto& b:e->bindings)add(b);
    auto shared=[&](const InputBinding& b){return controls.count(b.control)||(Paired(b)&&(controls.count(b.control+"X")||controls.count(b.control+"Y")));};
    for(const auto& e:m_map.entries)if(std::find(names.begin(),names.end(),e.name)!=names.end()||std::any_of(e.bindings.begin(),e.bindings.end(),shared)){
        if(std::find(m_consumed.begin(),m_consumed.end(),e.name)==m_consumed.end())m_consumed.push_back(e.name);
        m_pending.erase(e.name);m_fixed.erase(e.name);
    }
    if(std::any_of(controls.begin(),controls.end(),[](const auto& c){return c=="stick:Left"||c=="stick:LeftX"||c=="stick:LeftY"||c=="stick:Right"||c=="stick:RightX"||c=="stick:RightY";}))DiscardStickHistory();
}
