// M73 implementation-private slot ownership and bounded transient queues.
// Included inside ScriptSystem::Impl: no second world, JS heap, or scheduler.
using SignalKey = std::pair<SceneObjectId,uint64_t>;
struct SignalSubscription {
    uint64_t token=0,generation=0; SignalKey owner; std::string name; bool ui=false;
};
struct SignalMessage {
    uint64_t sequence=0; SceneObjectId sender=0; uint64_t slot=0;
    std::string name,json; std::deque<uint64_t> recipients;
    size_t Bytes() const {return name.size()+json.size()+recipients.size()*sizeof(uint64_t)+64;}
};
struct SignalStore {
    uint64_t nextToken=1,nextSequence=1,accepted=0,delivered=0,skipped=0,rejected=0;
    size_t bytes=0,recipientCount=0,eventCount=0;
    std::map<uint64_t,SignalSubscription> subscriptions;
    std::map<std::pair<bool,std::string>,std::set<uint64_t>> channels;
    std::map<std::tuple<bool,std::string,SceneObjectId>,std::set<uint64_t>> targets;
    std::map<SignalKey,std::set<uint64_t>> owners;
    std::deque<SignalMessage> lanes[2];
};
std::unique_ptr<SignalStore> signals;
uint64_t nextGeneration=1;
inline static std::atomic<uint64_t> nextSignalWorld{1};
const uint64_t signalWorld=nextSignalWorld.fetch_add(1);
bool signalInvocation=false,signalTeardown=false;
JSValue plainObjectPrototype=JS_UNDEFINED,plainArrayPrototype=JS_UNDEFINED;
struct Invocation {
    Impl& impl; bool active,teardown;
    Invocation(Impl& s,Instance& i,bool dying=false):impl(s),active(s.signalInvocation),teardown(s.signalTeardown){
        s.currentOwner=i.entity;s.currentSlot=i.slot.id;s.signalInvocation=true;s.signalTeardown=dying;
    }
    ~Invocation(){impl.signalInvocation=active;impl.signalTeardown=teardown;}
};
void SignalCancel(uint64_t token){
    if(!signals)return;
    auto it=signals->subscriptions.find(token);if(it==signals->subscriptions.end())return;
    const auto sub=it->second;
    auto erase=[&](auto& map,const auto& key){auto found=map.find(key);if(found!=map.end()){found->second.erase(token);if(found->second.empty())map.erase(found);}};
    erase(signals->channels,std::make_pair(sub.ui,sub.name));
    erase(signals->targets,std::make_tuple(sub.ui,sub.name,sub.owner.first));
    erase(signals->owners,sub.owner);signals->subscriptions.erase(it);
    // Queued identities contain no JS references; retire reservations eagerly.
    for(auto& lane:signals->lanes)for(auto& event:lane){
        auto before=event.recipients.size();event.recipients.erase(std::remove(event.recipients.begin(),event.recipients.end(),token),event.recipients.end());
        auto removed=before-event.recipients.size();signals->bytes-=removed*sizeof(uint64_t);signals->recipientCount-=removed;signals->skipped+=removed;
    }
    for(auto& lane:signals->lanes)for(auto event=lane.begin();event!=lane.end();){
        if(event->recipients.empty()){signals->bytes-=event->Bytes();--signals->eventCount;event=lane.erase(event);}else ++event;
    }
}
void SignalRetire(const SignalKey& owner){
    if(!signals)return;
    while(true){auto it=signals->owners.find(owner);if(it==signals->owners.end())break;SignalCancel(*it->second.begin());}
}
bool SignalLive(const SignalSubscription& sub) const {
    auto it=instances.find(sub.owner);if(it==instances.end()||it->second.fault||it->second.generation!=sub.generation)return false;
    auto* d=world?world->RuntimeDefinition(sub.owner.first):nullptr;if(!d)return false;
    return std::any_of(d->scripts.begin(),d->scripts.end(),[&](const auto& slot){return slot.id==sub.owner.second&&slot.enabled&&slot.asset==it->second.slot.asset&&slot.properties==it->second.slot.properties;});
}
// Descriptor traversal, never JSON.stringify/toJSON or property getters. Proxies
// are rejected before reflection. Each delivery parses its own captured bytes.
bool SignalPayload(JSValueConst value,std::string& text){
    size_t nodes=0,bytes=0;std::set<void*> ancestors;
    std::function<bool(JSValueConst,unsigned,nlohmann::json&)> read=[&](JSValueConst v,unsigned depth,nlohmann::json& out){
        if(depth>8||++nodes>1024)return false;
        if(JS_IsNull(v)){out=nullptr;return true;}
        if(JS_IsBool(v)){out=bool(JS_ToBool(ctx,v));return true;}
        if(JS_IsNumber(v)){double n=0;if(JS_ToFloat64(ctx,&n,v)||!std::isfinite(n))return false;out=n;return true;}
        if(JS_IsString(v)){auto s=String(ctx,v);bytes+=s.size();if(bytes>16384)return false;out=s;return true;}
        if(!JS_IsObject(v)||JS_IsFunction(ctx,v)||JS_IsProxy(v))return false;
        bool array=JS_IsArray(v);auto proto=JS_GetPrototype(ctx,v);
        bool plain=(JS_IsNull(proto)&&!array)||JS_VALUE_GET_PTR(proto)==JS_VALUE_GET_PTR(array?plainArrayPrototype:plainObjectPrototype);
        JS_FreeValue(ctx,proto);if(!plain)return false;
        auto pointer=JS_VALUE_GET_PTR(v);if(!ancestors.insert(pointer).second)return false;
        JSPropertyEnum* keys=nullptr;uint32_t count=0;
        bool ok=JS_GetOwnPropertyNames(ctx,&keys,&count,v,JS_GPN_STRING_MASK|JS_GPN_SYMBOL_MASK)==0&&count<=1025;
        out=array?nlohmann::json::array():nlohmann::json::object();
        uint32_t length=0;
        if(array){auto lengthValue=JS_GetPropertyStr(ctx,v,"length");ok=ok&&JS_ToUint32(ctx,&length,lengthValue)==0&&length<=1024;JS_FreeValue(ctx,lengthValue);}
        std::map<uint32_t,nlohmann::json> entries;
        for(uint32_t k=0;ok&&k<count;++k){
            auto key=JS_AtomToValue(ctx,keys[k].atom);if(!JS_IsString(key)){JS_FreeValue(ctx,key);ok=false;break;}
            auto name=String(ctx,key);JS_FreeValue(ctx,key);bytes+=name.size();if(bytes>16384){ok=false;break;}
            JSPropertyDescriptor d{};int found=JS_GetOwnProperty(ctx,&d,v,keys[k].atom);
            ok=found==1&&JS_IsUndefined(d.getter)&&JS_IsUndefined(d.setter);
            if(array&&name=="length"){JS_FreeValue(ctx,d.value);JS_FreeValue(ctx,d.getter);JS_FreeValue(ctx,d.setter);continue;}
            nlohmann::json child;if(ok)ok=read(d.value,depth+1,child);
            JS_FreeValue(ctx,d.value);JS_FreeValue(ctx,d.getter);JS_FreeValue(ctx,d.setter);
            if(ok&&array){try{size_t end=0;auto index=std::stoul(name,&end);ok=end==name.size()&&index<length&&name==std::to_string(index);if(ok)entries[uint32_t(index)]=std::move(child);}catch(...){ok=false;}}
            else if(ok)out[name]=std::move(child);
        }
        JS_FreePropertyEnum(ctx,keys,count);ancestors.erase(pointer);
        if(array){ok=ok&&entries.size()==length;if(ok)for(auto& entry:entries)out.push_back(std::move(entry.second));}
        return ok;
    };
    nlohmann::json copy;
    try{if(!read(value,0,copy))return false;text=copy.dump();return text.size()<=16384;}catch(...){return false;}
}
JSValue SignalNative(const std::string& op,int argc,JSValueConst* argv){
    JUDAS_PROFILE_SCOPE("Signals API");
    auto arg=[&](int i){return i<argc?argv[i]:JS_UNDEFINED;};
    const SignalKey owner={currentOwner,currentSlot};
    auto found=instances.find(owner);
    bool live=signalInvocation&&found!=instances.end()&&!found->second.fault&&world&&(world->RuntimeDefinition(currentOwner)||(signalTeardown&&op=="signalUnsubscribe"));
    if(!live||(op!="signalUnsubscribe"&&(signalTeardown||stopping)))return JS_ThrowTypeError(ctx,"signals require a live behaviour invocation; teardown permits only unsubscribe");
    auto reject=[&](const char* message){if(signals)++signals->rejected;return JS_ThrowTypeError(ctx,"signals: %s",message);};
    if(op=="signalStats"){
        auto o=JS_NewObject(ctx);auto put=[&](const char* key,size_t value){JS_SetPropertyStr(ctx,o,key,JS_NewFloat64(ctx,double(value)));};
        put("subscriptions",signals?signals->subscriptions.size():0);put("queuedEvents",signals?signals->eventCount:0);put("queuedBytes",signals?signals->bytes:0);put("queuedRecipients",signals?signals->recipientCount:0);
        put("accepted",signals?signals->accepted:0);put("delivered",signals?signals->delivered:0);put("skipped",signals?signals->skipped:0);put("rejected",signals?signals->rejected:0);return o;
    }
    if(op=="signalUnsubscribe"){
        if(!JS_IsString(arg(1)))return reject("subscription token string required");
        auto token=String(ctx,arg(1));auto prefix=std::to_string(signalWorld)+":";
        if(token.rfind(prefix,0)!=0)return reject("token belongs to another world");
        uint64_t id=0;try{size_t end=0;auto tail=token.substr(prefix.size());id=std::stoull(tail,&end);if(end!=tail.size())return reject("invalid token");}catch(...){return reject("invalid token");}
        if(!signals||!signals->subscriptions.count(id))return JS_FALSE;
        if(signals->subscriptions.at(id).owner!=owner)return reject("token belongs to another script slot");
        SignalCancel(id);return JS_TRUE;
    }
    int nameIndex=op=="signalSend"?2:1;
    if(!JS_IsString(arg(nameIndex)))return reject("name must be an exact UTF-8 string");
    auto name=String(ctx,arg(nameIndex));std::string utfError;
    if(name.empty()||name.size()>128||name.find('\0')!=std::string::npos||!ValidTextUTF8(name,utfError))return reject("name must contain 1..128 UTF-8 bytes without NUL");
    int phaseIndex=op=="signalSubscribe"?2:nameIndex+2;
    auto phase=arg(phaseIndex);if(!JS_IsString(phase))return reject("phase must be fixed or ui");
    auto phaseName=String(ctx,phase);if(phaseName!="fixed"&&phaseName!="ui")return reject("phase must be fixed or ui");bool ui=phaseName=="ui";
    if(op=="signalSubscribe"){
        auto handler=JS_GetPropertyStr(ctx,found->second.value,"onSignal");bool callable=JS_IsFunction(ctx,handler);JS_FreeValue(ctx,handler);if(!callable)return reject("onSignal must be callable");
        if(!signals)signals=std::make_unique<SignalStore>();
        if(auto it=signals->owners.find(owner);it!=signals->owners.end())for(auto id:it->second){auto& sub=signals->subscriptions.at(id);if(sub.name==name&&sub.ui==ui)return JS_NewString(ctx,(std::to_string(signalWorld)+":"+std::to_string(id)).c_str());}
        auto perSlot=signals->owners.find(owner);
        if(signals->subscriptions.size()>=2048||(perSlot!=signals->owners.end()&&perSlot->second.size()>=32))return reject("subscription capacity exceeded (32/slot, 2048/world)");
        auto id=signals->nextToken++;SignalSubscription sub{id,found->second.generation,owner,name,ui};signals->subscriptions.emplace(id,std::move(sub));signals->owners[owner].insert(id);signals->channels[{ui,name}].insert(id);signals->targets[{ui,name,owner.first}].insert(id);
        return JS_NewString(ctx,(std::to_string(signalWorld)+":"+std::to_string(id)).c_str());
    }
    EntityId target=0;
    if(op=="signalSend"){
        if(!JS_IsString(arg(1)))return reject("live Entity target required");
        try{size_t end=0;auto id=String(ctx,arg(1));target=std::stoull(id,&end);if(end!=id.size())target=0;}catch(...){target=0;}
        if(!target||!world->RuntimeDefinition(target))return JS_ThrowReferenceError(ctx,"signals: stale target entity");
    }
    std::string payload;if(!SignalPayload(arg(nameIndex+1),payload))return reject("payload requires plain data <=16KiB, depth<=8, values<=1024; no accessors, proxies, cycles, wrappers or nonfinite numbers");
    if(!signals)signals=std::make_unique<SignalStore>();
    std::vector<uint64_t> recipients;
    const std::set<uint64_t>* matches=nullptr;
    if(target){auto it=signals->targets.find({ui,name,target});if(it!=signals->targets.end())matches=&it->second;}
    else {auto it=signals->channels.find({ui,name});if(it!=signals->channels.end())matches=&it->second;}
    if(matches)for(auto id:*matches)if(SignalLive(signals->subscriptions.at(id)))recipients.push_back(id);
    if(recipients.size()>256)return reject("fan-out exceeds 256 recipients");
    std::sort(recipients.begin(),recipients.end(),[&](auto a,auto b){auto& x=signals->subscriptions.at(a);auto& y=signals->subscriptions.at(b);auto& xi=instances.at(x.owner);auto& yi=instances.at(y.owner);return std::tie(x.owner.first,xi.order,x.token)<std::tie(y.owner.first,yi.order,y.token);});
    auto result=JS_NewObject(ctx);JS_SetPropertyStr(ctx,result,"recipients",JS_NewUint32(ctx,uint32_t(recipients.size())));
    if(recipients.empty()){JS_SetPropertyStr(ctx,result,"sequence",JS_NULL);return result;}
    SignalMessage event;event.sequence=signals->nextSequence;event.sender=currentOwner;event.slot=currentSlot;event.name=name;event.json=std::move(payload);event.recipients.assign(recipients.begin(),recipients.end());
    if(signals->eventCount>=256||signals->bytes+event.Bytes()>1024*1024||signals->recipientCount+recipients.size()>4096){JS_FreeValue(ctx,result);return reject("queue capacity exceeded (256 events, 1MiB, 4096 recipients)");}
    ++signals->nextSequence;++signals->accepted;signals->bytes+=event.Bytes();signals->recipientCount+=recipients.size();++signals->eventCount;
    JS_SetPropertyStr(ctx,result,"sequence",JS_NewString(ctx,std::to_string(event.sequence).c_str()));signals->lanes[ui?1:0].push_back(std::move(event));return result;
}
void SignalDrain(bool ui){
    if(!signals||stopping||signals->lanes[ui?1:0].empty())return;
    JUDAS_PROFILE_SCOPE("Signals routing and delivery");
    auto& lane=signals->lanes[ui?1:0];std::deque<SignalMessage> batch;batch.swap(lane);size_t work=0;
    while(!batch.empty()&&work<256){
        auto& event=batch.front();
        while(!event.recipients.empty()&&work<256){
            auto token=event.recipients.front();event.recipients.pop_front();++work;--signals->recipientCount;signals->bytes-=sizeof(uint64_t);
            auto sub=signals->subscriptions.find(token);
            if(sub==signals->subscriptions.end()||!SignalLive(sub->second)){++signals->skipped;continue;}
            auto& i=instances.at(sub->second.owner);Invocation invocation(*this,i);polls=0;ProfileScope assetScope(AssetLabel(i));
            auto callback="onSignal:"+event.name+":"+(ui?"ui":"fixed");
            auto fn=JS_GetPropertyStr(ctx,i.value,"onSignal");
            if(JS_IsException(fn)){Error(i,callback.c_str());JS_FreeValue(ctx,fn);continue;}
            if(!JS_IsFunction(ctx,fn)){SignalCancel(token);++signals->skipped;JS_FreeValue(ctx,fn);continue;}
            auto envelope=JS_NewObject(ctx);JS_SetPropertyStr(ctx,envelope,"name",JS_NewString(ctx,event.name.c_str()));
            JS_SetPropertyStr(ctx,envelope,"payload",JS_ParseJSON(ctx,event.json.data(),event.json.size(),"signal payload"));
            JS_SetPropertyStr(ctx,envelope,"phase",JS_NewString(ctx,ui?"ui":"fixed"));JS_SetPropertyStr(ctx,envelope,"sequence",JS_NewString(ctx,std::to_string(event.sequence).c_str()));
            JS_SetPropertyStr(ctx,envelope,"senderId",JS_NewString(ctx,std::to_string(event.sender).c_str()));JS_SetPropertyStr(ctx,envelope,"senderSlot",JS_NewString(ctx,std::to_string(event.slot).c_str()));
            auto ns=NamespaceLibrary();auto entityFn=JS_GetPropertyStr(ctx,ns,"entity");auto id=JS_NewString(ctx,std::to_string(world->RuntimeDefinition(event.sender)?event.sender:0).c_str());auto sender=JS_Call(ctx,entityFn,JS_UNDEFINED,1,&id);
            JS_FreeValue(ctx,id);JS_FreeValue(ctx,entityFn);JS_FreeValue(ctx,ns);JS_SetPropertyStr(ctx,envelope,"sender",sender);
            JSValue result;{JUDAS_PROFILE_SCOPE("Signals handler");result=JS_Call(ctx,fn,i.value,1,&envelope);}
            ++signals->delivered;
            if(JS_IsException(result))Error(i,callback.c_str());
            else if(JS_PromiseState(ctx,result)!=JS_PROMISE_NOT_A_PROMISE){JS_ThrowTypeError(ctx,"async signal callbacks unsupported");Error(i,callback.c_str());}
            JS_FreeValue(ctx,result);JS_FreeValue(ctx,envelope);JS_FreeValue(ctx,fn);
        }
        if(event.recipients.empty()){signals->bytes-=event.Bytes();--signals->eventCount;batch.pop_front();}
    }
    // Deferred accepted reservations precede emissions from this drain. Never
    // extend a frozen batch with callbacks' newly published messages.
    while(!batch.empty()){lane.push_front(std::move(batch.back()));batch.pop_back();}
    JUDAS_PROFILE_COUNTER("Signals queued recipients",double(signals->recipientCount),ProfileCounterMode::Latest);
}
