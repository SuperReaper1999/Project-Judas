#include "ScriptAISensing.h"
#include "AISensing.h"
#include "RuntimeWorld.h"
#include "ScriptBindingValues.h"
#include "AuthoringJSON.h"
#include <cmath>
#include <map>
#include <stdexcept>

namespace {
using J=AuthoringJSON::J;
using ScriptBindingValues::String;
void Require(bool value,const std::string& message) {if(!value)throw std::invalid_argument(message);}
J Parse(JSContext* c,JSValueConst value) {
    Require(JS_IsString(value),"AISenses requires JSON text from its public wrapper");
    const auto text=String(c,value);
    Require(text.size()<=32768,"AISenses input exceeds 32 KiB");
    return AuthoringJSON::parse(text);
}
unsigned Count(const J& value,const char* name,unsigned fallback) {
    return value.contains(name)?AuthoringJSON::typed<unsigned>(value.at(name)):fallback;
}
bool Flag(const J& value,const char* name,bool fallback=false) {
    return value.contains(name)?AuthoringJSON::typed<bool>(value.at(name)):fallback;
}
CategoryMask Categories(const J& options,const char* name,const CategoryRegistry& registry,CategoryMask fallback) {
    if(!options.contains(name))return fallback;
    const auto& list=options.at(name);
    Require(list.is_array()&&list.size()<=64,std::string(name)+" must contain at most 64 category names/IDs");
    CategoryMask mask=0;
    for(const auto& item:list) {
        int id=-1;
        if(item.is_string())id=registry.Find(item.get<std::string>());
        else if(item.is_number_integer()){const auto candidate=AuthoringJSON::typed<unsigned>(item);if(candidate<64&&registry.names.count(candidate))id=int(candidate);}
        Require(id>=0,std::string(name)+" names an unknown/retired category");
        mask|=CategoryBit(unsigned(id));
    }
    return mask;
}
glm::vec3 Vector(const J& value) {
    AuthoringJSON::keys(value,{"x","y","z"},"origin");
    Require(value.contains("x")&&value.contains("y")&&value.contains("z"),"origin requires x/y/z");
    return {AuthoringJSON::typed<float>(value.at("x")),AuthoringJSON::typed<float>(value.at("y")),AuthoringJSON::typed<float>(value.at("z"))};
}
}
struct ScriptAISensing::Impl {
    struct Entry {SceneObjectId owner;std::uint64_t slot;std::unique_ptr<AISensing> sensor;};
    JSContext* context;
    std::uint64_t next=1;
    std::map<std::string,Entry> sensors;
    explicit Impl(JSContext* c):context(c) {}
};
ScriptAISensing::ScriptAISensing(JSContext* c):m(std::make_unique<Impl>(c)) {}
ScriptAISensing::~ScriptAISensing()=default;
JSValue ScriptAISensing::Dispatch(const std::string& op,int argc,JSValueConst* argv,
                                 RuntimeWorld& world,SceneObjectId owner,std::uint64_t slot) {
    auto* c=m->context;auto arg=[&](int i){return i<argc?argv[i]:JS_UNDEFINED;};
    try {
        Require(owner&&slot,"AISenses requires an owning script instance");
        if(op!="senseDispose")Require(world.RuntimeDefinition(owner)!=nullptr,"AISenses requires a live owning script instance");
        if(op=="senseCreate") {
            const auto options=Parse(c,arg(1));
            AuthoringJSON::keys(options,{"interval","maxResults","maxRaycasts","maxCandidates"},"AISenses settings");
            unsigned owned=0;
            for(const auto& p:m->sensors)owned+=p.second.owner==owner&&p.second.slot==slot;
            Require(owned<16&&m->sensors.size()<4096,"AISenses instance limit exceeded");
            AISensingSettings settings;
            if(options.contains("interval"))settings.interval=AuthoringJSON::typed<double>(options.at("interval"));
            settings.maxResults=Count(options,"maxResults",settings.maxResults);
            settings.maxRaycasts=Count(options,"maxRaycasts",settings.maxRaycasts);
            settings.maxCandidates=Count(options,"maxCandidates",settings.maxCandidates);
            auto sensor=std::make_unique<AISensing>(settings);
            const auto id=std::to_string(m->next++);
            m->sensors.emplace(id,Impl::Entry{owner,slot,std::move(sensor)});
            return JS_NewString(c,id.c_str());
        }
        Require(JS_IsString(arg(1)),"AISenses handle required");
        const auto found=m->sensors.find(String(c,arg(1)));
        Require(found!=m->sensors.end()&&found->second.owner==owner&&found->second.slot==slot,"stale or foreign AISenses handle");
        if(op=="senseDispose") {m->sensors.erase(found);return JS_UNDEFINED;}
        if(op=="senseSample") {
            const auto options=Parse(c,arg(2));
            AuthoringJSON::keys(options,{"origin","radius","visibility","planar","includeLayers","excludeLayers",
                "requiredTags","excludedTags","includeSensors","occluderLayers"},"AISenses query");
            Require(options.contains("origin")&&options.contains("radius"),"AISenses query requires origin and radius");
            AISensingRequest request;request.origin=Vector(options.at("origin"));
            request.radius=AuthoringJSON::typed<float>(options.at("radius"));
            request.visibility=Flag(options,"visibility");request.targets.planar=Flag(options,"planar");
            request.targets.includeSensors=Flag(options,"includeSensors");
            const auto& categories=world.Categories();
            request.targets.includeLayers=Categories(options,"includeLayers",categories.collision,kAllCategories);
            request.targets.excludeLayers=Categories(options,"excludeLayers",categories.collision,0);
            request.targets.requiredTags=Categories(options,"requiredTags",categories.tags,0);
            request.targets.excludedTags=Categories(options,"excludedTags",categories.tags,0);
            request.occluderLayers=Categories(options,"occluderLayers",categories.collision,kAllCategories);
            const auto text=found->second.sensor->Sample(world,owner,request).Json();
            return JS_ParseJSON(c,text.data(),text.size(),"AISenses observation");
        }
        throw std::invalid_argument("unknown AISenses operation");
    } catch(const std::exception& error) {return JS_ThrowTypeError(c,"AISenses: %s",error.what());}
}
void ScriptAISensing::Retire(SceneObjectId owner,std::uint64_t slot) {
    for(auto i=m->sensors.begin();i!=m->sensors.end();)
        if(i->second.owner==owner&&i->second.slot==slot)i=m->sensors.erase(i);else ++i;
}
void ScriptAISensing::ShiftOrigin() {for(auto& p:m->sensors)p.second.sensor->Invalidate();}
