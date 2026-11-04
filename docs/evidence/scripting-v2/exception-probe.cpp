#include "scripting/BindingContext.h"
#include "scripting/BindingRegistry.h"
#include <cstdio>
#include <stdexcept>
using namespace ScriptBindings;
struct Services : RuntimeServices {
 bool Json(JSValueConst,std::string&,std::string&) const override {return false;}
 JSValue RuntimeOperation(const std::string&,int,JSValueConst*) override {return JS_UNDEFINED;}
};
JSValue Throw(BindingContext&,const std::string&,int,JSValueConst*) {throw std::logic_error("state misuse");}
int main(){
 auto* rt=JS_NewRuntime();auto* js=JS_NewContext(rt);Services services;BindingStates states;
 const InputSystem* input=nullptr;bool fixed=false,ui=false,presentation=false,view=false,stopping=false,retiring=false,animation=false;
 float delta=0,alpha=1;glm::vec3 origin(0),direction(0);uint64_t owner=0,slot=0;
 BindingContext ctx{js,nullptr,nullptr,services,states,input,fixed,delta,ui,presentation,alpha,view,origin,direction,owner,slot,stopping,retiring,animation};
 BindingRegistry registry;registry.Add("Probe",{"throws"},Throw,false);
 bool escaped=false;JSValue value=JS_UNDEFINED;
 try {value=registry.Dispatch(ctx,"throws",0,nullptr);}catch(const std::exception& error){escaped=true;std::printf("raw_error=%s\n",error.what());}
 std::printf("escaped=%s js_exception=%s\n",escaped?"true":"false",JS_IsException(value)?"true":"false");
 if(JS_IsException(value)){auto e=JS_GetException(js);const char* text=JS_ToCString(js,e);std::printf("js_error=%s\n",text?text:"<unavailable>");JS_FreeCString(js,text);JS_FreeValue(js,e);}
 JS_FreeContext(js);JS_FreeRuntime(rt);
}
