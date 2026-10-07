#include "StructuredAuthor.h"
#include "Scene.h"
#include "Prefab.h"
#include "SceneSerialization.h"
#include "RuntimeUI.h"
#include "InputSystem.h"
#include "quickjs.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <set>
#include <algorithm>
#include <cmath>
namespace {
struct Owned {JSContext* c;JSValue v;~Owned(){JS_FreeValue(c,v);}operator JSValue()const{return v;}};
struct Reader {
 JSRuntime* runtime=JS_NewRuntime();JSContext* c=JS_NewContext(runtime);
 ~Reader(){JS_FreeContext(c);JS_FreeRuntime(runtime);}
 std::string text(JSValueConst v){if(!JS_IsString(v))throw std::runtime_error("string required");auto s=JS_ToCString(c,v);if(!s)throw std::runtime_error("string required");std::string t=s;JS_FreeCString(c,s);return t;}
 JSValue prop(JSValueConst o,const char* key){return JS_GetPropertyStr(c,o,key);}
 template<class F>void each(JSValueConst a,F callback){if(!JS_IsArray(a))throw std::runtime_error("array required");auto len=prop(a,"length");uint32_t n=0;JS_ToUint32(c,&n,len);JS_FreeValue(c,len);if(n>4096)throw std::runtime_error("4096 item authoring limit");for(uint32_t i=0;i<n;++i){auto v=JS_GetPropertyUint32(c,a,i);try{callback(v);}catch(...){JS_FreeValue(c,v);throw;}JS_FreeValue(c,v);}}
 void string(JSValueConst o,const char* key,std::string& out){auto v=prop(o,key);if(!JS_IsUndefined(v)){if(!JS_IsString(v)){JS_FreeValue(c,v);throw std::runtime_error(std::string(key)+" requires string");}out=text(v);}JS_FreeValue(c,v);}
 void number(JSValueConst o,const char* key,float& out){auto v=prop(o,key);if(!JS_IsUndefined(v)){double x=0;if(!JS_IsNumber(v)||JS_ToFloat64(c,&x,v)||!std::isfinite(x)||std::abs(x)>1e10){JS_FreeValue(c,v);throw std::runtime_error(std::string(key)+" requires finite number");}out=float(x);}JS_FreeValue(c,v);}
 void boolean(JSValueConst o,const char* key,bool& out){auto v=prop(o,key);if(!JS_IsUndefined(v)){if(!JS_IsBool(v)){JS_FreeValue(c,v);throw std::runtime_error(std::string(key)+" requires boolean");}out=JS_ToBool(c,v);}JS_FreeValue(c,v);}
 template<int N,typename V>void vector(JSValueConst o,const char* key,V& out){Owned v{c,prop(o,key)};if(!JS_IsUndefined(v)){int i=0;each(v,[&](JSValueConst value){if(i>=N)throw std::runtime_error(std::string(key)+" wrong vector length");double x;if(!JS_IsNumber(value)||JS_ToFloat64(c,&x,value)||!std::isfinite(x)||std::abs(x)>1e10)throw std::runtime_error("finite vector required");out[i++]=float(x);});if(i!=N)throw std::runtime_error(std::string(key)+" wrong vector length");}}
 void known(JSValueConst o,std::initializer_list<const char*> keys){JSPropertyEnum* names=nullptr;uint32_t n=0;JS_GetOwnPropertyNames(c,&names,&n,o,JS_GPN_STRING_MASK);std::string unknown;for(uint32_t i=0;i<n;++i){auto v=JS_AtomToString(c,names[i].atom);auto key=text(v);JS_FreeValue(c,v);bool found=false;for(auto k:keys)found|=key==k;if(!found)unknown=key;}JS_FreePropertyEnum(c,names,n);if(!unknown.empty())throw std::runtime_error("unknown structured field "+unknown);}
};
#define STR(v,k) r.string(v,#k,e.k)
#define BOOL(v,k) r.boolean(v,#k,e.k)
#define NUM(v,k) r.number(v,#k,e.k)
#define V2(v,k) r.vector<2>(v,#k,e.k)
#define V4(v,k) r.vector<4>(v,#k,e.k)
}
bool WriteStructuredContent(const std::string& source,const std::string& output,std::string& error){
 try{std::ifstream input(source);if(!input)throw std::runtime_error("cannot read structured input");std::ostringstream buffer;buffer<<input.rdbuf();auto text=buffer.str();if(text.size()>8*1024*1024)throw std::runtime_error("8MiB structured input limit");Reader r;Owned root{r.c,JS_ParseJSON(r.c,text.data(),text.size(),source.c_str())};if(JS_IsException(root))throw std::runtime_error("invalid authoring JSON");auto kindValue=r.prop(root,"kind");auto kind=r.text(kindValue);JS_FreeValue(r.c,kindValue);
 if(kind=="ui"){
  r.known(root,{"kind","reference","visible","enabled","modal","elements"});UIDocument d;r.vector<2>(root,"reference",d.reference);r.boolean(root,"visible",d.visible);r.boolean(root,"enabled",d.enabled);r.boolean(root,"modal",d.modal);Owned elements{r.c,r.prop(root,"elements")};
  r.each(elements,[&](JSValueConst v){r.known(v,{"id","parent","kind","flow","visible","enabled","clip","wrap","fit","text","textKey","texture","font","anchorMin","anchorMax","offset","size","relativeSize","align","textAlign","margin","padding","background","color","spacing","fontSize","value","minimum","maximum","mirrorRow","direction","textLogicalAlign"});UIElement e;STR(v,id);STR(v,parent);STR(v,text);STR(v,textKey);STR(v,texture);STR(v,font);
   std::string type="panel",flow="free",direction="auto";r.string(v,"kind",type);r.string(v,"flow",flow);r.string(v,"direction",direction);const std::vector<std::string> kinds={"canvas","panel","text","image","button","slider","toggle"},flows={"free","horizontal","vertical"};auto k=std::find(kinds.begin(),kinds.end(),type),f=std::find(flows.begin(),flows.end(),flow);if(k==kinds.end()||f==flows.end())throw std::runtime_error("unknown UI kind/flow");e.kind=UIKind(k-kinds.begin());e.flow=UIFlow(f-flows.begin());if(direction=="ltr")e.direction=TextDirection::LTR;else if(direction=="rtl")e.direction=TextDirection::RTL;else if(direction!="auto")throw std::runtime_error("direction auto/ltr/rtl required");float alignment=-1;r.number(v,"textLogicalAlign",alignment);if(alignment!=std::floor(alignment)||alignment<-1||alignment>2)throw std::runtime_error("textLogicalAlign -1..2 required");e.textLogicalAlign=int(alignment);
   BOOL(v,visible);BOOL(v,enabled);BOOL(v,clip);BOOL(v,wrap);BOOL(v,fit);BOOL(v,mirrorRow);V2(v,anchorMin);V2(v,anchorMax);V2(v,offset);V2(v,size);V2(v,relativeSize);V2(v,align);V2(v,textAlign);V4(v,margin);V4(v,padding);V4(v,background);V4(v,color);NUM(v,spacing);NUM(v,fontSize);NUM(v,value);NUM(v,minimum);NUM(v,maximum);d.elements.push_back(e);
  });if(!SaveUIDocument(output,d,error)){return false;}
 }else if(kind=="input"){
  r.known(root,{"kind","entries"});InputMap map;Owned entries{r.c,r.prop(root,"entries")};r.each(entries,[&](JSValueConst v){r.known(v,{"name","axis","vector","bindings"});InputEntry e;r.string(v,"name",e.name);r.boolean(v,"axis",e.axis);r.boolean(v,"vector",e.vector);Owned bindings{r.c,r.prop(v,"bindings")};r.each(bindings,[&](JSValueConst b){r.known(b,{"control","scale","deadzone","scaleY","circular"});InputBinding x;r.string(b,"control",x.control);r.number(b,"scale",x.scale);r.number(b,"deadzone",x.deadzone);r.number(b,"scaleY",x.scaleY);r.boolean(b,"circular",x.circular);e.bindings.push_back(x);});map.entries.push_back(e);});if(!map.Validate(error)){return false;}std::ofstream out(output);out<<map.Serialize();if(!out)throw std::runtime_error("cannot write input map");
 }else if(kind=="scene"){
  r.known(root,{"kind","name","objects"});Scene scene;r.string(root,"name",scene.Settings().name);Owned objects{r.c,r.prop(root,"objects")};r.each(objects,[&](JSValueConst v){r.known(v,{"id","name","parent","components","fields"});std::string name,id,parent="0";r.string(v,"name",name);r.string(v,"id",id);r.string(v,"parent",parent);SceneObject e;if(id.empty()||parent.empty()||id.find_first_not_of("0123456789")!=std::string::npos||parent.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("id/parent require stable decimal identities");e.id=std::stoull(id);e.parent=std::stoull(parent);e.name=name;Owned components{r.c,r.prop(v,"components")};r.each(components,[&](JSValueConst item){auto name=r.text(item);
#define COMPONENT(n,t,member) if(name==n)e.member=t{};else
 COMPONENT("render",SceneRenderComponent,render) COMPONENT("body",SceneBodyComponent,body) COMPONENT("motor",CharacterMotorSettings,characterMotor) COMPONENT("gravity",SceneGravityComponent,gravity) COMPONENT("animation",SceneAnimationComponent,animation) COMPONENT("socket",SceneSocketComponent,socket) COMPONENT("joint",SceneJointComponent,joint) COMPONENT("ragdoll",RagdollDefinition,ragdoll) COMPONENT("ui",SceneUIComponent,ui) COMPONENT("particle",ParticleEmitterSettings,particleEmitter) COMPONENT("audio",SceneAudioEmitterComponent,audioEmitter)
 throw std::runtime_error("unsupported structured component "+name);
#undef COMPONENT
 });PrefabProperties fields;Owned values{r.c,r.prop(v,"fields")};if(!JS_IsUndefined(values)){JSPropertyEnum* keys=nullptr;uint32_t n=0;JS_GetOwnPropertyNames(r.c,&keys,&n,values,JS_GPN_STRING_MASK);bool invalid=false;for(uint32_t i=0;i<n;++i){auto key=JS_AtomToString(r.c,keys[i].atom),value=JS_GetProperty(r.c,values,keys[i].atom);if(JS_IsString(value))fields[r.text(key)]=r.text(value);else invalid=true;JS_FreeValue(r.c,key);JS_FreeValue(r.c,value);}JS_FreePropertyEnum(r.c,keys,n);if(invalid)throw std::runtime_error("scene fields require canonical property strings");}if(!ApplyObjectProperties(e,fields,error))throw std::runtime_error(error);if(!scene.InsertObject(e))throw std::runtime_error("duplicate/invalid object identity");});if(!ValidateHierarchy(scene,error)||!SaveSceneToFile(scene,output,error)){return false;}
 }else throw std::runtime_error("kind must be scene/ui/input");return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
