#include "Classification.h"
#include <iomanip>
#include <sstream>
#include <set>

bool CategoryRegistry::Add(const std::string& name,unsigned& id) {
    if(name.empty()||name.find_first_of("\r\n")!=std::string::npos||Find(name)>=0||nextId>=64) return false;
    id=nextId++;names.emplace(id,name);return true;
}
bool CategoryRegistry::Rename(unsigned id,const std::string& name) {
    auto it=names.find(id);const auto other=Find(name);
    if(it==names.end()||name.empty()||name.find_first_of("\r\n")!=std::string::npos||(other>=0&&unsigned(other)!=id))return false;
    it->second=name;return true;
}
bool CategoryRegistry::Remove(unsigned id,bool preserveDefault) {
    return !(preserveDefault&&id==0)&&names.erase(id)!=0;
}
CategoryMask CategoryRegistry::ActiveMask()const {CategoryMask mask=0;for(const auto& [id,name]:names){(void)name;mask|=CategoryBit(id);}return mask;}
int CategoryRegistry::Find(const std::string& name)const {for(const auto& [id,n]:names)if(n==name)return int(id);return -1;}
bool CategoryRegistry::Validate(std::string& error)const {
    std::set<std::string> used;
    if(nextId>64){error="Registry exceeds 64 lifetime IDs";return false;}
    for(const auto& [id,name]:names)if(id>=nextId||name.empty()||!used.insert(name).second||name.find_first_of("\r\n")!=std::string::npos){error="Invalid/duplicate category name or stable ID";return false;}
    return true;
}
bool ProjectClassification::Validate(std::string& error)const {
    if(!tags.Validate(error)||!collision.Validate(error)||!render.Validate(error))return false;
    if(!collision.names.count(0)||!render.names.count(0)){error="Default collision/render ID 0 must remain registered";return false;}
    return true;
}
bool ProjectClassification::IsDefault()const {return Serialize()==ProjectClassification{}.Serialize();}
std::string ProjectClassification::Serialize()const {
    std::ostringstream s;s<<"JudasClassification 1 ";
    for(const auto* r:{&tags,&collision,&render}){
        s<<r->nextId<<' '<<r->names.size()<<' ';
        for(const auto& [id,name]:r->names)s<<id<<' '<<std::quoted(name)<<' ';
    }
    return s.str();
}
bool ProjectClassification::Parse(const std::string& text,ProjectClassification& out,std::string& error){
    std::istringstream s(text);std::string header;int version=0;ProjectClassification parsed;
    if(!(s>>header>>version)||header!="JudasClassification"||version!=1){error="Unsupported classification configuration";return false;}
    for(auto* r:{&parsed.tags,&parsed.collision,&parsed.render}){
        unsigned count=0;r->names.clear();
        if(!(s>>r->nextId>>count)||count>64){error="Invalid registry size";return false;}
        for(unsigned i=0;i<count;++i){unsigned id=0;std::string name;if(!(s>>id>>std::quoted(name))||!r->names.emplace(id,name).second){error="Invalid/duplicate registry entry";return false;}}
    }
    std::string trailing;if(s>>trailing){error="Trailing classification data";return false;}
    if(!parsed.Validate(error))return false;
    out=parsed;return true;
}

#include "Scene.h"
bool ValidateSceneClassification(const Scene& scene,const ProjectClassification& categories,std::string& error){
    if(!categories.Validate(error))return false;
    for(const auto& o:scene.Objects()){
        if(o.wheel&&(!categories.collision.names.count(o.wheel->collisionLayer)||((o.wheel->requiredTags|o.wheel->excludedTags)&~categories.tags.ActiveMask()))){error="unregistered wheel layer/tags";return false;}
        if(o.characterMotor&&(!categories.collision.names.count(o.characterMotor->collisionLayer)||((o.characterMotor->requiredTags|o.characterMotor->excludedTags)&~categories.tags.ActiveMask()))){error="unregistered motor layer/tags";return false;}
        if(o.ragdoll)for(const auto& bone:o.ragdoll->bones)if(!categories.collision.names.count(bone.collisionLayer)){error="unregistered ragdoll collision layer";return false;}
        if((o.tags&~categories.tags.ActiveMask())||!categories.render.names.count(o.renderLayer)||
           (o.body&&!categories.collision.names.count(o.body->collisionLayer))||
           (o.door&&!categories.collision.names.count(o.door->collisionLayer))||
           (o.playerStart&&!categories.collision.names.count(o.playerStart->collisionLayer))){
            error="Entity "+std::to_string(o.id)+" references an unregistered tag/layer; repair its assignment in Project settings/Inspector";return false;
        }
    }
    return true;
}
