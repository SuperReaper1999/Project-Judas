#include "ScriptRuntimeInternal.h"
// Shared lifecycle/error boundary for native world events. Bindings and physical
// systems provide data; no second runtime or subsystem-specific dispatcher.
namespace {
template<class Runtime,class Builder>
void Dispatch(Runtime& runtime,SceneObjectId owner,const char* callback,Builder build){
    runtime.CheckThread();runtime.fixed=true;
    const auto* definition=runtime.world->RuntimeDefinition(owner);if(!definition)return;
    const auto slots=definition->scripts;
    for(const auto& slot:slots){
        const auto* live=runtime.world->RuntimeDefinition(owner);if(!live)return;
        if(std::none_of(live->scripts.begin(),live->scripts.end(),[&](const auto& s){return s.id==slot.id&&s.enabled;}))continue;
        auto found=runtime.instances.find({owner,slot.id});
        if(!slot.enabled||found==runtime.instances.end()||found->second.fault)continue;
        auto& instance=found->second;
        if(!instance.started){instance.started=true;runtime.Callback(instance,instance.loaded?"restore":"start");}
        if(!runtime.world->RuntimeDefinition(owner)||instance.fault)continue;
        typename Runtime::Invocation invocation(runtime,instance);ProfileScope assetScope(runtime.AssetLabel(instance));
        runtime.polls=0;runtime.currentOwner=owner;runtime.currentSlot=slot.id;
        auto function=JS_GetPropertyStr(runtime.ctx,instance.value,callback);
        if(JS_IsException(function)){runtime.Error(instance,callback);JS_FreeValue(runtime.ctx,function);continue;}
        if(JS_IsFunction(runtime.ctx,function)){
            auto event=build(runtime.ctx);
            auto result=JS_Call(runtime.ctx,function,instance.value,1,&event);
            if(JS_IsException(result))runtime.Error(instance,callback);
            else if(JS_PromiseState(runtime.ctx,result)!=JS_PROMISE_NOT_A_PROMISE){JS_ThrowTypeError(runtime.ctx,"async native world event callbacks unsupported");runtime.Error(instance,callback);}
            JS_FreeValue(runtime.ctx,result);JS_FreeValue(runtime.ctx,event);
        }
        JS_FreeValue(runtime.ctx,function);
    }
}
}
void ScriptSystem::FractureEvent(SceneObjectId owner,uint64_t revision,const std::vector<std::string>& keys,const std::vector<unsigned>& causes){
    Dispatch(*m,owner,"onFracture",[&](JSContext* context){
        auto event=JS_NewObject(context);JS_SetPropertyStr(context,event,"revision",JS_NewFloat64(context,double(revision)));
        auto interfaces=JS_NewArray(context);
        for(unsigned b=0;b<keys.size();++b){auto value=JS_NewObject(context);
            JS_SetPropertyStr(context,value,"key",JS_NewString(context,keys[b].c_str()));
            JS_SetPropertyStr(context,value,"cause",JS_NewString(context,causes[b]==1?"physical":"explicit"));
            JS_SetPropertyUint32(context,interfaces,b,value);
        }
        JS_SetPropertyStr(context,event,"interfaces",interfaces);return event;
    });
}
void ScriptSystem::AttachmentEvent(SceneObjectId owner,const std::map<std::string,std::string>& failures){
    Dispatch(*m,owner,"onAttachmentFailure",[&](JSContext* context){
        auto event=JS_NewObject(context),attachments=JS_NewArray(context);unsigned index=0;
        for(const auto& [group,cause]:failures){auto value=JS_NewObject(context);
            JS_SetPropertyStr(context,value,"group",JS_NewString(context,group.c_str()));
            JS_SetPropertyStr(context,value,"cause",JS_NewString(context,cause.c_str()));
            JS_SetPropertyUint32(context,attachments,index++,value);
        }
        JS_SetPropertyStr(context,event,"attachments",attachments);return event;
    });
}
