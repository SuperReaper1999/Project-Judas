#include "ScriptSystem.h"
#include "Prefab.h"
#include "SceneSerialization.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>

namespace {
void StripLink(SceneObject& o) {
    o.prefabAsset.clear(); o.prefabRoot=0; o.prefabSource=0;
    o.prefabIds.clear(); o.prefabOverrides.clear();
}
std::vector<std::string> Lines(const std::string& s) {
    std::vector<std::string> lines; std::istringstream in(s); std::string line;
    while(std::getline(in,line)) lines.push_back(line);
    return lines;
}
SceneTransform Compose(const SceneTransform& p,const SceneTransform& c) {
    SceneTransform t;
    const auto rotation=glm::normalize(p.rotation);
    t.position=p.position+rotation*(p.scale*c.position);
    t.rotation=glm::normalize(rotation*glm::normalize(c.rotation)); t.scale=p.scale*c.scale;
    return t;
}
}

std::string EncodePrefabOverrides(const PrefabProperties& p) {
    std::ostringstream out; out<<p.size();
    for(const auto& pair:p) out<<' '<<std::quoted(pair.first)<<' '<<std::quoted(pair.second);
    return out.str();
}
bool DecodePrefabOverrides(const std::string& text,PrefabProperties& p,std::string& error) {
    std::istringstream in(text); size_t count=0; PrefabProperties result;
    if(!(in>>count)||count>10000){error="invalid prefab property count";return false;}
    for(size_t i=0;i<count;++i){std::string key,value;
        if(!(in>>std::quoted(key)>>std::quoted(value))||key.empty()||key.find_first_of(" \t\r\n")!=std::string::npos||
           value.find_first_of("\r\n")!=std::string::npos||!result.emplace(key,value).second){error="invalid/duplicate prefab property";return false;}
    }
    in>>std::ws; if(!in.eof()){error="trailing prefab property data";return false;}
    p=std::move(result);return true;
}
PrefabProperties ObjectProperties(const SceneObject& object) {
    auto o=object;StripLink(o);o.parent=0;
    std::string text;WriteSceneObjectBlock(o,text); PrefabProperties p;
    std::ostringstream name;name<<std::quoted(o.name);p["_name"]=name.str();
    std::map<std::string,unsigned> repeated;
    for(const auto& line:Lines(text)){
        std::istringstream in(line);std::string key;in>>key;
        if(key.empty()||key=="object"||key=="end")continue;
        std::string value;std::getline(in,value);auto first=value.find_first_not_of(" \t");
        if(key=="body.compound-box"||key=="body.fluid-cavity")key+="."+std::to_string(repeated[key]++);
        p[key]=first==std::string::npos?"":value.substr(first);
    }
    return p;
}
bool ApplyObjectProperties(SceneObject& o,const PrefabProperties& properties,std::string& error) {
    auto p=ObjectProperties(o);
    for(const auto& pair:properties){
        // A removed component removes its complete serialized block. Added
        // components supply their normal header and fields through the same map.
        if(pair.second=="@remove"){
            for(auto it=p.begin();it!=p.end();){
                if(it->first==pair.first||it->first.rfind(pair.first+".",0)==0)it=p.erase(it);else++it;
            }
        }else p[pair.first]=pair.second;
    }
    std::string text="object "+std::to_string(o.id)+" "+p["_name"]+"\n";
    for(const auto& pair:p)if(pair.first!="_name"){auto key=pair.first;for(const char* repeated:{"body.compound-box.","body.fluid-cavity."})if(key.rfind(repeated,0)==0)key=std::string(repeated).substr(0,std::string(repeated).size()-1);text+="  "+key+" "+pair.second+"\n";}
    text+="end\n";auto lines=Lines(text);size_t index=0;SceneObject parsed;
    if(!ParseSceneObjectBlock(lines,index,parsed,error))return false;
    parsed.parent=o.parent;parsed.prefabAsset=o.prefabAsset;parsed.prefabRoot=o.prefabRoot;
    parsed.prefabSource=o.prefabSource;parsed.prefabIds=o.prefabIds;parsed.prefabOverrides=o.prefabOverrides;
    o=std::move(parsed);return true;
}
bool ValidateHierarchy(const Scene& scene,std::string& error) {
    std::set<SceneObjectId> done,visiting;
    std::function<bool(const SceneObject&)> visit=[&](const SceneObject& o){
        if(done.count(o.id))return true;
        if(!visiting.insert(o.id).second){error="cyclic authored hierarchy";return false;}
        if(o.socket){const auto& k=*o.socket;const auto* target=scene.Find(k.target);
            if(!target||!target->animation||k.joint.empty()||o.body||o.characterMotor||o.ragdoll||o.deformable){error="invalid visual socket owner/target";return false;}
            if(!visit(*target))return false;
        }
        if(o.parent){const auto* p=scene.Find(o.parent);if(!p){error="missing parent "+std::to_string(o.parent);return false;}if(!visit(*p))return false;}
        visiting.erase(o.id);done.insert(o.id);return true;
    };
    for(const auto& o:scene.Objects()){
        if(!visit(o))return false;
        for(const auto& pair:o.prefabIds)if(!pair.first||!pair.second||pair.second>=scene.NextId()){
            error="prefab mapped identities must precede scene NextId";return false;
        }
    }
    return true;
}
bool FlattenHierarchy(const Scene& scene,Scene& flattened,std::string& error) {
    if(!ValidateHierarchy(scene,error))return false;
    flattened=scene;std::map<SceneObjectId,SceneTransform> transforms;
    std::function<SceneTransform(const SceneObject&)> world=[&](const SceneObject& o){
        auto it=transforms.find(o.id);if(it!=transforms.end())return it->second;
        return transforms[o.id]=o.parent?Compose(world(*scene.Find(o.parent)),o.transform):o.transform;
    };
    for(auto& o:flattened.Objects())o.transform=world(*scene.Find(o.id));
    return true;
}
bool ValidatePrefab(const Scene& prefab,std::string& error) {
    if(prefab.Objects().empty()||!ValidateHierarchy(prefab,error)){if(error.empty())error="empty prefab";return false;}
    for(const auto& o:prefab.Objects())if(o.gravitySelection&&o.gravitySelection->mode==GravitySelection::Mode::Field){
        auto* source=prefab.Find(o.gravitySelection->source);if(!source||!source->gravity){error="prefab gravity selection source must be an internal gravity field";return false;}}
    for(const auto& o:prefab.Objects())if(o.joint){auto a=prefab.Find(o.joint->bodyA),b=prefab.Find(o.joint->bodyB);if(!a||!a->body||(o.joint->bodyB&&(!b||!b->body))){error="prefab joint references missing body";return false;}}
    for(const auto& o:prefab.Objects())if(o.liquidConnection){auto a=prefab.Find(o.liquidConnection->source),b=prefab.Find(o.liquidConnection->destination);if(!a||!b||!a->liquidBasin||!b->liquidBasin){error="prefab liquid connection references missing basin";return false;}}
    int roots=0;for(const auto& o:prefab.Objects()){
        roots+=!o.parent;
        if(o.prefabRoot||!o.prefabAsset.empty()){error="nested prefab references are unsupported in M36";return false;}
    }
    if(roots!=1){error="prefab requires exactly one hierarchy root";return false;}
    return true;
}
bool LoadPrefab(const AssetDatabase& assets,const AssetId& asset,Scene& prefab,std::string& error) {
    const auto* r=assets.Find(asset);
    if(!r||r->missing||r->type!=AssetType::Prefab){error="missing or wrong-type prefab asset: "+asset;return false;}
    Scene temp;if(!LoadSceneFromFile(r->path,temp,error)||!ValidatePrefab(temp,error))return false;
    prefab=std::move(temp);return true;
}
bool CreatePrefab(const Scene& scene,SceneObjectId root,Scene& prefab,std::string& error) {
    if(!scene.Find(root)){error="no selected prefab root";return false;}
    if(!ValidateHierarchy(scene,error))return false;
    Scene result;result.Settings().name=scene.Find(root)->name;
    for(const auto& o:scene.Objects()){
        auto id=o.id;while(id&&id!=root){const auto* p=scene.Find(id);if(!p){error="invalid hierarchy";return false;}id=p->parent;}
        if(id!=root)continue;
        auto copy=o;StripLink(copy);
        if(copy.id==root){copy.parent=0;copy.transform=SceneTransform{};}
        result.InsertObject(copy);
    }
    if(!ValidatePrefab(result,error))return false;
    prefab=std::move(result);return true;
}
bool InstantiatePrefab(Scene& scene,const Scene& prefab,const AssetId& asset,const SceneTransform& placement,
                       SceneObjectId& root,std::string& error) {
    if(!IsValidAssetId(asset)||!ValidatePrefab(prefab,error)){if(error.empty())error="invalid prefab asset identity";return false;}
    Scene result=scene;std::map<SceneObjectId,SceneObjectId> ids;
    SceneObjectId sourceRoot=0;
    for(const auto& o:prefab.Objects()){ids[o.id]=result.CreateObject(o.name).id;if(!o.parent)sourceRoot=o.id;}
    root=ids.at(sourceRoot);
    for(const auto& o:prefab.Objects()){
        auto copy=o;for(auto& slot:copy.scripts)slot.properties=ScriptSystem::RemapPropertyEntities(slot.properties,ids);copy.id=ids.at(o.id);copy.parent=o.parent?ids.at(o.parent):0;
        copy.prefabRoot=root;copy.prefabSource=o.id;
        if(copy.render&&copy.render->textureCamera)copy.render->textureCamera=ids.at(copy.render->textureCamera);
        if(copy.deformable)for(auto& a:copy.deformable->attachments)if(a.target){auto it=ids.find(a.target);if(it==ids.end()){error="prefab attachment target outside source";return false;}a.target=it->second;}
        if(copy.liquidConnection){copy.liquidConnection->source=ids.at(copy.liquidConnection->source);copy.liquidConnection->destination=ids.at(copy.liquidConnection->destination);}
        if(copy.gravitySelection&&copy.gravitySelection->source)copy.gravitySelection->source=ids.at(copy.gravitySelection->source);
        if(copy.socket)copy.socket->target=ids.at(copy.socket->target);
        if(copy.joint){copy.joint->bodyA=ids.at(copy.joint->bodyA);if(copy.joint->bodyB)copy.joint->bodyB=ids.at(copy.joint->bodyB);}
        if(o.id==sourceRoot){copy.prefabAsset=asset;copy.prefabIds=ids;copy.transform=placement;}
        *result.Find(copy.id)=copy;
    }
    scene=std::move(result);return true;
}
bool ResolvePrefabs(const Scene& scene,const AssetDatabase* assets,Scene& resolved,std::string& error) {
    Scene result=scene;std::map<AssetId,Scene> sources;
    for(const auto& root:scene.Objects())if(!root.prefabAsset.empty()){
        if(!assets){error="prefab resolution requires a project asset database";return false;}
        if(root.prefabRoot!=root.id||!root.prefabSource){error="invalid prefab root provenance";return false;}
        if(!sources.count(root.prefabAsset)){Scene source;if(!LoadPrefab(*assets,root.prefabAsset,source,error))return false;sources[root.prefabAsset]=std::move(source);}
        const Scene& source=sources.at(root.prefabAsset);
        const auto* sourceRoot=source.Find(root.prefabSource);
        if(!sourceRoot||sourceRoot->parent){error="prefab source root identity changed";return false;}
        auto mapping=root.prefabIds;std::set<SceneObjectId> destinations;
        for(const auto& pair:mapping)if(!pair.first||!pair.second||!destinations.insert(pair.second).second){error="invalid duplicate prefab identity mapping";return false;}
        for(const auto& o:source.Objects())if(!mapping.count(o.id)){
            auto& placeholder=result.CreateObject(o.name);placeholder.prefabRoot=root.id;
            mapping[o.id]=placeholder.id;
        }
        if(mapping[root.prefabSource]!=root.id){error="prefab root mapping mismatch";return false;}
        for(const auto& old:scene.Objects())if(old.prefabRoot==root.id) {
            if(!mapping.count(old.prefabSource)||mapping.at(old.prefabSource)!=old.id){error="prefab child mapping mismatch";return false;}
            auto& objects=result.Objects();objects.erase(std::remove_if(objects.begin(),objects.end(),[&](const auto& o){return o.id==old.id;}),objects.end());
        }
        for(const auto& o:source.Objects()){
            auto copy=o;
            const auto* old=scene.Find(mapping.at(o.id));
            if(old){copy.prefabOverrides=old->prefabOverrides;if(!ApplyObjectProperties(copy,copy.prefabOverrides,error))return false;}
            for(auto& slot:copy.scripts)slot.properties=ScriptSystem::RemapPropertyEntities(slot.properties,mapping);
            copy.id=mapping.at(o.id);copy.parent=o.parent?mapping.at(o.parent):root.parent;
            copy.prefabRoot=root.id;copy.prefabSource=o.id;
            if(copy.render&&copy.render->textureCamera){auto it=mapping.find(copy.render->textureCamera);if(it==mapping.end()){error="prefab camera reference outside source";return false;}copy.render->textureCamera=it->second;}
            if(copy.deformable)for(auto& a:copy.deformable->attachments)if(a.target){auto it=mapping.find(a.target);if(it==mapping.end()){error="prefab attachment target outside source";return false;}a.target=it->second;}
            if(copy.liquidConnection){auto a=mapping.find(copy.liquidConnection->source),b=mapping.find(copy.liquidConnection->destination);if(a==mapping.end()||b==mapping.end()){error="prefab liquid connection outside source";return false;}copy.liquidConnection->source=a->second;copy.liquidConnection->destination=b->second;}
            if(copy.gravitySelection&&copy.gravitySelection->source){auto target=mapping.find(copy.gravitySelection->source);if(target==mapping.end()){error="gravity source outside prefab";return false;}copy.gravitySelection->source=target->second;}
            if(copy.socket){auto target=mapping.find(copy.socket->target);if(target==mapping.end()){error="socket target outside prefab";return false;}copy.socket->target=target->second;}
            if(copy.joint){auto a=mapping.find(copy.joint->bodyA),b=mapping.find(copy.joint->bodyB);if(a==mapping.end()||(copy.joint->bodyB&&b==mapping.end())){error="prefab joint reference outside source";return false;}copy.joint->bodyA=a->second;if(copy.joint->bodyB)copy.joint->bodyB=b->second;}
            if(o.id==root.prefabSource){copy.prefabAsset=root.prefabAsset;copy.prefabIds=mapping;copy.transform=root.transform;}
            // Keep original scene order and gravity precedence. New children append.
            auto* existing=result.Find(copy.id);
            if(existing){if(!existing->prefabRoot){error="prefab mapping collides with unrelated object";return false;}*existing=copy;}
            else result.InsertObject(copy);
        }
    }
    // Restore source-independent scene ordering after resolution.
    std::stable_sort(result.Objects().begin(),result.Objects().end(),[&](const auto& a,const auto& b){
        auto rank=[&](SceneObjectId id){for(size_t i=0;i<scene.Objects().size();++i)if(scene.Objects()[i].id==id)return i;return scene.Objects().size();};return rank(a.id)<rank(b.id);
    });
    for(const auto& o:result.Objects())if(o.prefabRoot){const auto* root=result.Find(o.prefabRoot);if(!root||root->prefabAsset.empty()){error="orphaned prefab child";return false;}}
    if(!ValidateHierarchy(result,error))return false;
    resolved=std::move(result);return true;
}
void CapturePrefabEdits(const Scene& before,Scene& after) {
    for(auto& o:after.Objects())if(o.prefabRoot){
        const auto* previous=before.Find(o.id);if(!previous)continue;
        auto normalized=[&](const SceneObject& value){auto copy=value;
            if(copy.render&&copy.render->textureCamera){const auto* root=after.Find(o.prefabRoot);
                if(root)for(const auto& pair:root->prefabIds)if(pair.second==copy.render->textureCamera)copy.render->textureCamera=pair.first;}
            if(copy.joint){const auto* root=after.Find(o.prefabRoot);if(root){const auto a=copy.joint->bodyA,b=copy.joint->bodyB;for(const auto& pair:root->prefabIds){if(pair.second==a)copy.joint->bodyA=pair.first;if(pair.second==b)copy.joint->bodyB=pair.first;}}}
            if(copy.gravitySelection&&copy.gravitySelection->source){const auto* root=after.Find(o.prefabRoot);if(root)for(auto pair:root->prefabIds)if(copy.gravitySelection->source==pair.second){copy.gravitySelection->source=pair.first;break;}}
            if(copy.socket){const auto* root=after.Find(o.prefabRoot);if(root)for(auto pair:root->prefabIds)if(copy.socket->target==pair.second){copy.socket->target=pair.first;break;}}
            if(copy.deformable){const auto* root=after.Find(o.prefabRoot);if(root)for(auto& a:copy.deformable->attachments)for(auto pair:root->prefabIds)if(a.target==pair.second){a.target=pair.first;break;}}
            if(copy.liquidConnection){const auto* root=after.Find(o.prefabRoot);if(root){auto a=copy.liquidConnection->source,b=copy.liquidConnection->destination;for(const auto& pair:root->prefabIds){if(pair.second==a)copy.liquidConnection->source=pair.first;if(pair.second==b)copy.liquidConnection->destination=pair.first;}}}
            std::map<SceneObjectId,SceneObjectId> reverse;const auto* root=after.Find(o.prefabRoot);if(root)for(auto [source,id]:root->prefabIds)reverse[id]=source;for(auto& slot:copy.scripts)slot.properties=ScriptSystem::RemapPropertyEntities(slot.properties,reverse);
            return ObjectProperties(copy);};
        auto a=normalized(*previous),b=normalized(o);
        if(o.prefabRoot==o.id)for(const char* key:{"position","rotation","scale"}){a.erase(key);b.erase(key);}
        for(const auto& pair:b)if(!a.count(pair.first)||a.at(pair.first)!=pair.second)o.prefabOverrides[pair.first]=pair.second;
        for(const auto& pair:a)if(!b.count(pair.first))o.prefabOverrides[pair.first]="@remove";
    }
}
bool RevertPrefabProperty(Scene& scene,SceneObjectId id,const std::string& key,const AssetDatabase& assets,std::string& error) {
    Scene temp=scene;auto* o=temp.Find(id);if(!o||!o->prefabRoot){error="not a prefab instance";return false;}
    o->prefabOverrides.erase(key);
    if(key.find('.')==std::string::npos)for(auto it=o->prefabOverrides.begin();it!=o->prefabOverrides.end();){
        if(it->first.rfind(key+".",0)==0)it=o->prefabOverrides.erase(it);else++it;
    }
    Scene resolved;if(!ResolvePrefabs(temp,&assets,resolved,error))return false;
    scene=std::move(resolved);return true;
}
bool ApplyPrefabSource(Scene& scene,SceneObjectId rootId,const AssetDatabase& assets,std::string& error) {
    const auto* root=scene.Find(rootId);if(!root||root->prefabAsset.empty()){error="select the instance root";return false;}
    Scene source;if(!CreatePrefab(scene,rootId,source,error))return false;
    auto mapping=root->prefabIds;std::map<SceneObjectId,SceneObjectId> reverse;
    for(const auto& pair:mapping)reverse[pair.second]=pair.first;
    for(auto& o:source.Objects()){
        if(!reverse.count(o.id)){error="unmapped instance child";return false;}
        o.id=reverse.at(o.id);if(o.parent)o.parent=reverse.at(o.parent);
        if(o.gravitySelection&&o.gravitySelection->source)o.gravitySelection->source=reverse.at(o.gravitySelection->source);
        if(o.socket)o.socket->target=reverse.at(o.socket->target);
        for(auto& slot:o.scripts)slot.properties=ScriptSystem::RemapPropertyEntities(slot.properties,reverse);
        if(o.render&&o.render->textureCamera)o.render->textureCamera=reverse.at(o.render->textureCamera);
    }
    source.SetNextId(1);
    const auto* record=assets.Find(root->prefabAsset);
    if(!record||record->missing){error="missing prefab source";return false;}
    if(!ValidatePrefab(source,error)||!SaveSceneToFile(source,record->path,error))return false;
    Scene temp=scene;for(auto& o:temp.Objects())if(o.prefabRoot==rootId)o.prefabOverrides.clear();
    Scene resolved;if(!ResolvePrefabs(temp,&assets,resolved,error))return false;
    scene=std::move(resolved);return true;
}
