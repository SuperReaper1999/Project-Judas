#include "ScriptBindings.h"
#include "ScriptBindingValues.h"
#include "RuntimeWorld.h"

#include "AssetDatabase.h"
#include "AuthoringJSON.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

using ScriptBindingValues::Number;
using ScriptBindingValues::ReadVec;
using ScriptBindingValues::String;

JSValue ScriptBindings::UI(JSContext* c, const std::string& op, int argc, JSValueConst* argv,
                         RuntimeWorld& world, const AssetDatabase* assets, const std::uint64_t& owner,
                         const std::uint64_t& slot) {
    auto arg=[&](int i){return i<argc?argv[i]:JS_UNDEFINED;};
    // Existing dispatch branches begin.
    if(op=="uiDiagnostics"){if(argc>1){if(!JS_IsBool(arg(1)))return JS_ThrowTypeError(c,"debug visibility requires boolean");world.UI().debugOverlayVisible=JS_ToBool(c,arg(1));}return JS_NewBool(c,world.UI().debugOverlayVisible);}
    if(op=="uiFind")return JS_NewUint32(c,world.UI().Find(String(c,arg(1))));
    if(op=="uiQuit"){world.UI().RequestQuit();return JS_UNDEFINED;}
    if(op=="uiLoad"){std::string error;auto h=world.UI().Load(String(c,arg(1)),String(c,arg(2)),owner,error,slot);if(!h)return JS_ThrowTypeError(c,"UI: %s",error.c_str());return JS_NewUint32(c,h);}
    if(op=="uiControl"){
        uint32_t h=0;JS_ToUint32(c,&h,arg(1));auto& ui=world.UI();auto* document=ui.Document(h);if(!document)return JS_ThrowReferenceError(c,"stale UI document");
        auto id=String(c,arg(2)),action=String(c,arg(3));auto* element=id.empty()?nullptr:ui.Element(h,id);if(!id.empty()&&!element)return JS_ThrowReferenceError(c,"unknown UI element");
        try{std::string error;bool write=argc>4;nlohmann::json value;if(write){auto text=String(c,arg(4));if(text.size()>4*1024*1024)throw std::runtime_error("UI payload exceeds 4 MiB");value=AuthoringJSON::parse(text);}
            auto result=[&](const std::string& text){return JS_ParseJSON(c,text.data(),text.size(),"UI observation");};
            auto require=[&](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
            if(action=="focus"&&!element){if(write)require(ui.Focus(h,value.get<std::string>()),"invalid/hidden/disabled focus target");return JS_NewString(c,ui.Focused(h).c_str());}
            if(action=="reference"&&!element){if(write){auto x=value.at("x").get<float>(),y=value.at("y").get<float>();require(std::isfinite(x)&&std::isfinite(y)&&x>=64&&y>=64&&x<=16384&&y<=16384,"referenceSize must be finite 64..16384");document->reference={x,y};ui.InvalidateLayout();}return result(nlohmann::json{{"x",document->reference.x},{"y",document->reference.y}}.dump());}
            if(action=="scale"&&!element){if(write)require(value.is_number()&&ui.SetScale(h,value.get<float>()),"UI scale must be 0.75..2");return JS_NewFloat64(c,ui.Scale(h));}
            if(action=="capture"&&!element)return result(ui.CaptureState(h));
            if(action=="accessibility"&&!element)return result(ui.Accessibility(h));
            if(action=="restore"&&!element){require(write&&ui.RestoreState(h,value.dump(),error),error.empty()?"invalid UI state":error.c_str());return JS_UNDEFINED;}
            require(element!=nullptr,"UI element required");
            if(action=="scroll"){require(element->kind==UIKind::List||element->kind==UIKind::Scroll,"scroll requires list/scroll container");if(write)require(ui.SetScroll(h,id,{value.at("x").get<float>(),value.at("y").get<float>()}),"invalid scroll offset");auto offset=ui.Scroll(h,id);return result(nlohmann::json{{"x",offset.x},{"y",offset.y}}.dump());}
            if(action=="selection"){require(element->kind==UIKind::TextInput,"selection requires text input");if(write){auto a=value.at("anchor"),b=value.at("caret");require(a.is_number_integer()&&b.is_number_integer()&&a.get<double>()>=0&&b.get<double>()>=0&&a.get<double>()<=16384&&b.get<double>()<=16384,"bounded nonnegative integer grapheme indices required");require(ui.SelectText(h,id,{a.get<size_t>(),b.get<size_t>()}),"selection outside text");}auto selected=ui.Selection(h,id);return result(nlohmann::json{{"anchor",selected.anchor},{"caret",selected.caret}}.dump());}
            if(action=="selected"){require(element->kind==UIKind::List,"selection requires list");if(write)require(value.is_number_integer()&&value.get<double>()>=-1&&value.get<double>()<10000&&ui.Select(h,id,value.get<int>()),"invalid/disabled list index");return JS_NewInt32(c,ui.Selected(h,id));}
            if(action=="items"){require(write&&value.is_array()&&value.size()<=10000,"bounded list item array required");std::vector<UIListItem> items;for(auto& item:value)items.push_back({item.at("id").get<std::string>(),item.at("text").get<std::string>(),item.value("enabled",true)});require(ui.SetItems(h,id,items,error),error.c_str());return JS_UNDEFINED;}
            if(action=="configure"){
                require(write&&value.is_object(),"UI style/control object required");auto candidate=*element;
                for(auto entry=value.begin();entry!=value.end();++entry){auto key=entry.key();auto& v=entry.value();if(key=="placeholder")candidate.placeholder=v.get<std::string>();else if(key=="accessibleLabel")candidate.accessibleLabel=v.get<std::string>();else if(key=="description")candidate.description=v.get<std::string>();else if(key=="readOnly")candidate.readOnly=v.get<bool>();else if(key=="maxBytes"){require(v.is_number_integer()&&v.get<double>()>=1&&v.get<double>()<=16384,"maxBytes must be 1..16384");candidate.maxBytes=v.get<unsigned>();}else if(key=="fontSize")candidate.fontSize=v.get<float>();else if(key=="rowHeight")candidate.rowHeight=v.get<float>();else if(key=="background"||key=="color"||key=="focusColor"){glm::vec4 color{v.at("r").get<float>(),v.at("g").get<float>(),v.at("b").get<float>(),v.at("a").get<float>()};if(key=="background")candidate.background=color;else if(key=="color")candidate.color=color;else candidate.focusColor=color;}else throw std::runtime_error("unknown UI style/control key");}
                auto proof=*document;for(auto& e:proof.elements)if(e.id==id)e=candidate;require(proof.Validate(error),error.c_str());*element=std::move(candidate);ui.InvalidateLayout();return JS_UNDEFINED;
            }
            throw std::runtime_error("unknown UI control operation");
        }catch(const std::exception& error){return JS_ThrowTypeError(c,"UI: %s",error.what());}
    }
    if(op=="uiLayout"){uint32_t h=0;JS_ToUint32(c,&h,arg(1));auto id=String(c,arg(2));auto* e=world.UI().Element(h,id);if(!e)return JS_ThrowReferenceError(c,"stale UI document/element");if(argc>3){if(!JS_IsObject(arg(3)))return JS_ThrowTypeError(c,"layout patch object required");std::map<std::string,glm::vec2> patch;JSPropertyEnum* props=nullptr;uint32_t count=0;if(JS_GetOwnPropertyNames(c,&props,&count,arg(3),JS_GPN_STRING_MASK|JS_GPN_ENUM_ONLY))return JS_EXCEPTION;bool valid=count<=6;for(uint32_t i=0;i<count;++i){auto key=JS_AtomToString(c,props[i].atom);auto name=String(c,key);JS_FreeValue(c,key);auto v=JS_GetProperty(c,arg(3),props[i].atom);glm::vec2 xy;valid&=Number(c,v,"x",xy.x)&&Number(c,v,"y",xy.y);patch[name]=xy;JS_FreeValue(c,v);JS_FreeAtom(c,props[i].atom);}js_free(c,props);std::string error;if(!valid||!world.UI().SetElementLayout(h,id,patch,error))return JS_ThrowTypeError(c,"UI layout: %s",error.c_str());e=world.UI().Element(h,id);}auto o=JS_NewObject(c);for(auto [key,v]:{std::pair<const char*,glm::vec2>{"offset",e->offset},{"size",e->size},{"anchorMin",e->anchorMin},{"anchorMax",e->anchorMax},{"relativeSize",e->relativeSize},{"align",e->align}}){auto xy=JS_NewObject(c);JS_SetPropertyStr(c,xy,"x",JS_NewFloat64(c,v.x));JS_SetPropertyStr(c,xy,"y",JS_NewFloat64(c,v.y));JS_SetPropertyStr(c,o,key,xy);}return o;}
    if(op=="uiGet"||op=="uiSet"||op=="uiUnload"){
        uint32_t h=0;JS_ToUint32(c,&h,arg(1));auto* d=world.UI().Document(h);if(!d)return JS_ThrowReferenceError(c,"stale/unloaded UI document");
        if(op=="uiUnload"){world.UI().Unload(h);return JS_TRUE;}
        auto id=String(c,arg(2)),key=String(c,arg(3));UIElement* e=id.empty()?nullptr:world.UI().Element(h,id);
        if(!id.empty()&&!e)return JS_ThrowReferenceError(c,"unknown UI element %s",id.c_str());
        bool* flag=key=="visible"?(e?&e->visible:&d->visible):key=="enabled"?(e?&e->enabled:&d->enabled):key=="modal"&&!e?&d->modal:nullptr;
        if(flag){if(op=="uiSet"){if(!JS_IsBool(arg(4)))return JS_ThrowTypeError(c,"UI flag requires boolean");*flag=JS_ToBool(c,arg(4));}return JS_NewBool(c,*flag);}
        if (e && (key == "text" || key == "texture" || key == "font" || key == "textKey")) {
            auto& value = key == "text" ? e->text : key == "texture" ? e->texture :
                          key == "font" ? e->font : e->textKey;
            if (op == "uiSet") {
                if (!JS_IsString(arg(4))) return JS_ThrowTypeError(c, "UI string required");
                auto v = String(c, arg(4));
                if (v.size() > 16384) return JS_ThrowTypeError(c, "UI text too long");
                if (key == "textKey") {
                    std::string error;
                    if (v.size() > 128 || v.find_first_of("\r\n\0", 0, 3) != std::string::npos ||
                        !ValidTextUTF8(v, error))
                        return JS_ThrowTypeError(c, "invalid localization key");
                }
                if ((key == "texture" || key == "font") && !v.empty()) {
                    auto* asset = assets ? assets->Find(v) : nullptr;
                    if (!asset || asset->missing || asset->type !=
                        (key == "font" ? AssetType::Font : AssetType::Texture))
                        return JS_ThrowTypeError(c, "invalid UI asset");
                }
                if (key == "text") {
                    std::string error;
                    // Ordinary labels retain their established length-bearing
                    // UTF-8 contract. Editable fields additionally reject NUL
                    // and line breaks, matching UIDocument validation.
                    const bool invalidInput = e->kind == UIKind::TextInput &&
                        (v.size() > e->maxBytes || v.find_first_of("\r\n\0", 0, 3) != std::string::npos);
                    if (!ValidTextUTF8(v, error) || invalidInput)
                        return JS_ThrowTypeError(c, "invalid/bounded UI text");
                }
                if (key == "textKey" && e->kind == UIKind::TextInput && !v.empty())
                    return JS_ThrowTypeError(c, "editable fields use literal text");
                value = v;
                if (key == "text") {
                    e->textKey.clear();
                    if (e->kind == UIKind::TextInput) {
                        const auto n = TextGraphemes(v).size() - 1;
                        world.UI().SelectText(h, id, {n, n});
                    }
                }
            }
            return JS_NewStringLen(c, value.data(), value.size());
        }
        if(e&&(key=="direction"||key=="textAlignment")){std::vector<std::string> choices=key=="direction"?std::vector<std::string>{"auto","ltr","rtl"}:std::vector<std::string>{"left","right","center","start","end"};int index=key=="direction"?int(e->direction):e->textLogicalAlign;if(op=="uiSet"){if(!JS_IsString(arg(4)))return JS_ThrowTypeError(c,"UI direction/alignment string required");auto v=String(c,arg(4));auto it=std::find(choices.begin(),choices.end(),v);if(it==choices.end())return JS_ThrowTypeError(c,"unsupported UI text direction/alignment");index=int(it-choices.begin());if(key=="direction")e->direction=TextDirection(index);else e->textLogicalAlign=index;}return JS_NewString(c,index<0?"":choices[index].c_str());}
        if(e&&key=="value"){if(op=="uiSet"){double v;if(JS_ToFloat64(c,&v,arg(4))||!std::isfinite(v)||v<e->minimum||v>e->maximum)return JS_ThrowTypeError(c,"UI value outside authored range");e->value=float(v);}return JS_NewFloat64(c,e->value);}
        return JS_ThrowTypeError(c,"unknown UI property");
    }

    // Existing dispatch branches end.
    return JS_ThrowTypeError(c,"unknown UI operation");
}
