#include "ScriptAIReservations.h"
#include "AIReservations.h"
#include "RuntimeWorld.h"
#include "ScriptBindingValues.h"
#include <map>
#include <cmath>
#include <stdexcept>

struct ScriptAIReservations::Impl {
    struct Scope {std::uint64_t owner,slot,id;};
    AIReservationBoard board;
    std::map<std::string,Scope> scopes;
    std::uint64_t next=1;
};
ScriptAIReservations::ScriptAIReservations():m(std::make_unique<Impl>()){}
ScriptAIReservations::~ScriptAIReservations()=default;
JSValue ScriptAIReservations::Dispatch(JSContext* c,const std::string& op,int argc,
                                       JSValueConst* argv,RuntimeWorld& world,
                                       std::uint64_t owner,std::uint64_t slot) {
    auto arg=[&](int i){return i<argc?argv[i]:JS_UNDEFINED;};
    try {
        if(!owner||!slot)throw std::invalid_argument("reservations require an owning script instance");
        if(op=="reservationCreate") {
            if(m->scopes.size()>=4096)throw std::invalid_argument("reservation session limit reached");
            unsigned count=0;
            for(const auto& row:m->scopes)count+=row.second.owner==owner&&row.second.slot==slot;
            if(count>=16)throw std::invalid_argument("at most 16 reservation scopes per script slot");
            const auto id=m->next++;
            const auto key=std::to_string(id);
            m->scopes.emplace(key,Impl::Scope{owner,slot,id});
            return JS_NewString(c,key.c_str());
        }
        if(!JS_IsString(arg(1)))throw std::invalid_argument("reservation scope required");
        const auto found=m->scopes.find(ScriptBindingValues::String(c,arg(1)));
        if(found==m->scopes.end()||found->second.owner!=owner||found->second.slot!=slot)
            return JS_ThrowReferenceError(c,"stale or foreign AI reservation scope");
        const auto id=found->second.id;
        if(op=="reservationReleaseAll"||op=="reservationDispose") {
            m->board.ReleaseAll(id);
            if(op=="reservationDispose")m->scopes.erase(found);
            return JS_UNDEFINED;
        }
        if(!JS_IsString(arg(2)))throw std::invalid_argument("target key must be a primitive string");
        // Use explicit length so embedded NUL keys cannot alias another target.
        size_t length=0;const char* text=JS_ToCStringLen(c,&length,arg(2));
        if(!text)return JS_EXCEPTION;
        const std::string target(text,length);JS_FreeCString(c,text);
        const auto now=world.SimulationTimeSeconds();
        if(op=="reservationAvailable")return JS_NewBool(c,m->board.Available(target,id,now));
        if(op=="reservationRelease")return JS_NewBool(c,m->board.Release(target,id));
        if(op=="reservationAcquire") {
            double duration=0;
            if(!JS_IsNumber(arg(3))||JS_ToFloat64(c,&duration,arg(3))<0||!std::isfinite(duration))
                throw std::invalid_argument("finite numeric lease duration required");
            return JS_NewBool(c,m->board.Acquire(target,id,now,duration));
        }
        throw std::invalid_argument("unknown reservation operation");
    } catch(const std::exception& e) {return JS_ThrowTypeError(c,"AI reservation: %s",e.what());}
}
void ScriptAIReservations::ReleaseClaims(std::uint64_t owner,std::uint64_t slot) {
    for(const auto& entry:m->scopes)
        if(entry.second.owner==owner&&entry.second.slot==slot)m->board.ReleaseAll(entry.second.id);
}
void ScriptAIReservations::Retire(std::uint64_t owner,std::uint64_t slot) {
    for(auto it=m->scopes.begin();it!=m->scopes.end();) {
        if(it->second.owner==owner&&it->second.slot==slot){m->board.ReleaseAll(it->second.id);it=m->scopes.erase(it);}
        else ++it;
    }
}
