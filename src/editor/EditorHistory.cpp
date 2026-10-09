#include "ScriptSystem.h"
#include "PerformanceProfiler.h"
#include "Ragdoll.h"
#include "RuntimeWorld.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <set>
#include <fstream>
#include <sstream>
#include "NamedAuthoring.h"
#include <glm/gtc/matrix_transform.hpp>
#include "EditorDocument.h"

#include "SceneSerialization.h"
#include "Prefab.h"

namespace {
constexpr std::size_t kMaxHistory = 200;
bool ValidEdit(const Scene& scene,std::string& error){std::string text;SaveSceneToString(scene,text);Scene validated;return LoadSceneFromString(text,validated,error);}
std::string Source(const std::string& path){std::ifstream f(path,std::ios::binary);std::ostringstream s;s<<f.rdbuf();return s.str();}
glm::mat4 Matrix(const SceneTransform& t){return glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale);}
bool SetWorld(SceneObject& o,const SceneTransform& t,const Scene& flat,std::string& error){auto m=Matrix(t);if(o.parent){auto* parent=flat.Find(o.parent);if(!parent){error="missing transform parent";return false;}m=glm::inverse(Matrix(parent->transform))*m;}JointTransform v;if(!DecomposeRigidPose(m,v,error))return false;o.transform={v.translation,v.rotation,v.scale};return true;}
bool ComponentKey(const std::string& key,const std::string& prefix){
 if(key==prefix||key.rfind(prefix+".",0)==0)return true;
 return (prefix=="audio"&&key=="audio-emitter")||(prefix=="scripts"&&key.rfind("script.",0)==0);
}
}

void EditorDocument::NewScene() {
    ++m_generation;
    m_scene.Clear();
    m_scene.Settings().name = "Untitled scene";
    m_path.clear();m_loadedSource.clear();
    m_dirty = false;
    m_selected = kInvalidSceneObjectId;m_selection.clear();
    m_undo.clear();
    m_redo.clear();
    m_editInProgress = false;
}

bool EditorDocument::Load(const std::string& path, std::string& outError) {
    Scene loaded;
    if (!LoadSceneFromFile(path, loaded, outError)) return false;
    m_scene = std::move(loaded);++m_generation;
    m_path = path;m_loadedSource=Source(path);
    m_dirty = false;
    m_selected = kInvalidSceneObjectId;m_selection.clear();
    m_undo.clear();
    m_redo.clear();
    m_editInProgress = false;
    return true;
}

bool EditorDocument::Save(std::string& outError) {
    if (m_path.empty()) {
        outError = "the scene has no file path yet; use Save As";
        return false;
    }
    if(ExternalChanged()){outError="source changed externally; reload or save to a new path before overwriting";return false;}
    if (!SaveSceneToFile(m_scene, m_path, outError)) return false;
    m_dirty = false;m_loadedSource=Source(m_path);
    return true;
}

bool EditorDocument::SaveAs(const std::string& path, std::string& outError) {
    if (!SaveSceneToFile(m_scene, path, outError)) return false;
    m_path = path;m_loadedSource=Source(path);
    m_dirty = false;
    return true;
}

void EditorDocument::BeginEdit() {
    if (m_editInProgress) return;
    m_pendingSnapshot = m_scene;
    m_editInProgress = true;
}

void EditorDocument::CommitEdit(bool capturePrefab){CommitEditImpl(capturePrefab,false);}
bool EditorDocument::CommitCandidate(Scene&& candidate,std::string& error,bool capturePrefab,bool validated){
    // The candidate already owns its copy; transfer the live scene into history
    // rather than making a second whole-document snapshot for the same action.
    if(!m_editInProgress){m_pendingSnapshot=std::move(m_scene);m_editInProgress=true;}
    m_scene=std::move(candidate);CommitEditImpl(capturePrefab,validated);
    error=m_validationError;return error.empty();
}
void EditorDocument::CommitEditImpl(bool capturePrefab,bool alreadyValidated) {
    if (!m_editInProgress) return;
    m_editInProgress = false;
    m_validationError.clear();
    if(capturePrefab){
        // Prefab override capture edits authored data after a parsed source load.
        if(std::any_of(m_scene.Objects().begin(),m_scene.Objects().end(),[](const auto& o){return o.prefabRoot!=0;}))alreadyValidated=false;
        CapturePrefabEdits(m_pendingSnapshot,m_scene);
    }
    if(!alreadyValidated&&!ValidEdit(m_scene,m_validationError)){m_scene=m_pendingSnapshot;PruneSelection();return;}
    if (ScenesEqual(m_pendingSnapshot, m_scene)) return;
    m_undo.push_back(std::move(m_pendingSnapshot));
    if (m_undo.size() > kMaxHistory) m_undo.erase(m_undo.begin());
    m_redo.clear();
    m_dirty = true;++m_generation;
}

void EditorDocument::CancelEdit() {
    if(m_editInProgress&&!ScenesEqual(m_scene,m_pendingSnapshot))m_scene=m_pendingSnapshot;
    m_editInProgress = false;PruneSelection();
}

void EditorDocument::Undo() {
    JUDAS_PROFILE_SCOPE("Editor undo");
    if (m_undo.empty()) return;
    m_redo.push_back(m_scene);
    m_scene = std::move(m_undo.back());
    m_undo.pop_back();
    m_dirty = true;++m_generation;
    PruneSelection();
}

void EditorDocument::Redo() {
    JUDAS_PROFILE_SCOPE("Editor redo");
    if (m_redo.empty()) return;
    m_undo.push_back(m_scene);
    m_scene = std::move(m_redo.back());
    m_redo.pop_back();
    m_dirty = true;++m_generation;
    PruneSelection();
}

void EditorDocument::Select(SceneObjectId id,bool toggle){
 if(!toggle){m_selection.clear();if(id&&m_scene.Find(id))m_selection.push_back(id);m_selected=m_selection.empty()?0:id;return;}
 auto it=std::find(m_selection.begin(),m_selection.end(),id);if(it!=m_selection.end())m_selection.erase(it);else if(m_scene.Find(id))m_selection.push_back(id);m_selected=m_selection.empty()?0:m_selection.back();
}
bool EditorDocument::IsSelected(SceneObjectId id)const{return std::find(m_selection.begin(),m_selection.end(),id)!=m_selection.end();}
void EditorDocument::PruneSelection(){m_selection.erase(std::remove_if(m_selection.begin(),m_selection.end(),[&](auto id){return !m_scene.Find(id);}),m_selection.end());if(!IsSelected(m_selected))m_selected=m_selection.empty()?0:m_selection.back();}
bool EditorDocument::BatchProperties(const std::map<std::string,std::string>& properties,std::string& error){
 PruneSelection();if(m_selection.empty()){error="select objects first";return false;}
 for(auto& [key,_]:properties)if(key=="parent"||key.rfind("prefab",0)==0){error="Use the hierarchy/prefab command for "+key+"; ordinary property batches cannot replace identity links";return false;}
 auto candidate=m_scene;for(auto id:m_selection)if(!ApplyObjectProperties(*candidate.Find(id),properties,error))return false;
 return CommitCandidate(std::move(candidate),error);
}
bool EditorDocument::BatchTransform(glm::vec3 translation,glm::quat rotation,glm::vec3 scale,std::string& error,bool individual,bool world){
 PruneSelection();if(m_selection.empty()){error="select objects first";return false;}if(!std::isfinite(glm::dot(translation,translation))||!std::isfinite(glm::dot(rotation,rotation))||glm::dot(rotation,rotation)<1e-10f||!std::isfinite(glm::dot(scale,scale))||glm::any(glm::lessThanEqual(scale,glm::vec3(0)))){error="finite translation, nonzero rotation and positive scale required";return false;}
 auto roots=SelectionRoots();Scene flat;if(!FlattenHierarchy(m_scene,flat,error))return false;glm::vec3 pivot(0);for(auto id:roots)pivot+=flat.Find(id)->transform.position;pivot/=float(roots.size());auto candidate=m_scene;
 for(auto id:roots){auto* o=candidate.Find(id);auto t=world?flat.Find(id)->transform:o->transform;auto q=glm::normalize(rotation);if(!individual){if(!world){error="shared pivot requires world axes";return false;}t.position=pivot+q*((t.position-pivot)*scale)+translation;}else t.position+=translation;t.rotation=glm::normalize(q*t.rotation);t.scale*=scale;if(world){if(!SetWorld(*o,t,flat,error))return false;}else o->transform=t;}
 return CommitCandidate(std::move(candidate),error);
}
bool EditorDocument::ReparentSelection(SceneObjectId parent,std::string& error,bool preserveWorld){
 PruneSelection();Scene flat;if(!FlattenHierarchy(m_scene,flat,error))return false;auto candidate=m_scene;glm::mat4 inverse(1);if(parent){auto* p=flat.Find(parent);if(!p){error="missing parent";return false;}auto& t=p->transform;inverse=glm::inverse(glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale));}
 for(auto id:SelectionRoots()){auto* o=candidate.Find(id);if(o->prefabRoot&&o->prefabRoot!=id){error="reparent prefab roots; unpack source members first";return false;}if(!preserveWorld){o->parent=parent;continue;}auto& t=flat.Find(id)->transform;JointTransform local;if(!DecomposeRigidPose(inverse*glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale),local,error))return false;o->parent=parent;o->transform={local.translation,local.rotation,local.scale};}
 return CommitCandidate(std::move(candidate),error);
}
bool EditorDocument::GroupSelection(std::string& error){PruneSelection();if(m_selection.empty()){error="select objects before grouping";return false;}BeginEdit();auto name="Folder "+std::to_string(m_scene.NextId());for(auto id:m_selection)m_scene.Find(id)->authoringFolder=name;CommitEdit(false);return true;}
void EditorDocument::CopyComponent(const std::string& prefix){m_componentClipboard.clear();m_clipboardPrefix=prefix;auto* o=SelectedObject();if(!o)return;for(auto& [key,value]:ObjectProperties(*o))if(ComponentKey(key,prefix))m_componentClipboard[key]=value;}
bool EditorDocument::PasteComponent(std::string& error){if(m_componentClipboard.empty()){error="component clipboard is empty";return false;}auto candidate=m_scene;for(auto id:m_selection){auto* o=candidate.Find(id);if(!o)continue;auto properties=m_componentClipboard;for(const auto& [key,_]:ObjectProperties(*o))if(ComponentKey(key,m_clipboardPrefix)&&!properties.count(key))properties[key]="@remove";if(!ApplyObjectProperties(*o,properties,error))return false;}return CommitCandidate(std::move(candidate),error);}
bool EditorDocument::DuplicateSelection(std::string& error){
 PruneSelection();if(m_selection.empty())return false;std::set<SceneObjectId> members(m_selection.begin(),m_selection.end());bool changed=true;while(changed){changed=false;for(auto& o:m_scene.Objects())if(members.count(o.parent))changed|=members.insert(o.id).second;}
 auto candidate=m_scene;std::map<SceneObjectId,SceneObjectId> ids;for(auto id:members)ids[id]=candidate.CreateObject("copy").id;
 for(auto& o:m_scene.Objects())if(members.count(o.id)){auto copy=o;copy.id=ids.at(o.id);copy.name+=" copy";auto remap=[&](SceneObjectId& id){if(ids.count(id))id=ids.at(id);};remap(copy.parent);remap(copy.prefabRoot);if(copy.gravitySelection)remap(copy.gravitySelection->source);if(copy.socket)remap(copy.socket->target);if(copy.render)remap(copy.render->textureCamera);if(copy.joint){remap(copy.joint->bodyA);remap(copy.joint->bodyB);}if(copy.liquidConnection){remap(copy.liquidConnection->source);remap(copy.liquidConnection->destination);}if(copy.deformable)for(auto& a:copy.deformable->attachments)remap(a.target);for(auto& slot:copy.scripts)slot.properties=ScriptSystem::RemapPropertyEntities(slot.properties,ids);for(auto& [_,id]:copy.prefabIds)remap(id);*candidate.Find(copy.id)=std::move(copy);}
 if(!CommitCandidate(std::move(candidate),error,false))return false;
 std::vector<SceneObjectId> selection;for(auto id:m_selection)selection.push_back(ids.at(id));m_selection=selection;m_selected=m_selection.back();return true;
}

std::vector<SceneObjectId> EditorDocument::SelectionRoots()const{std::vector<SceneObjectId> roots;for(auto id:m_selection){bool inherited=false;auto* o=m_scene.Find(id);for(auto p=o?o->parent:0;p;){if(IsSelected(p)){inherited=true;break;}auto* parent=m_scene.Find(p);p=parent?parent->parent:0;}if(o&&!inherited)roots.push_back(id);}return roots;}
std::vector<SceneObjectId> EditorDocument::Search(const std::string& query)const{if(m_searchGeneration==m_generation&&m_searchQuery==query)return m_searchResults;JUDAS_PROFILE_SCOPE("Hierarchy filter");std::set<SceneObjectId> keep;for(auto& o:m_scene.Objects()){bool match=query.empty()||o.name.find(query)!=std::string::npos||o.authoringFolder.find(query)!=std::string::npos;auto properties=ObjectProperties(o);for(auto& [k,v]:properties)match|=k.find(query)!=std::string::npos||v.find(query)!=std::string::npos;if(match){keep.insert(o.id);for(auto p=o.parent;p;){keep.insert(p);auto* ancestor=m_scene.Find(p);p=ancestor?ancestor->parent:0;}}}std::vector<SceneObjectId> ids;for(auto& o:m_scene.Objects())if(keep.count(o.id))ids.push_back(o.id);m_searchGeneration=m_generation;m_searchQuery=query;m_searchResults=ids;return ids;}
void EditorDocument::SelectRange(SceneObjectId id,const std::vector<SceneObjectId>& visible,bool additive){auto a=std::find(visible.begin(),visible.end(),m_selected),b=std::find(visible.begin(),visible.end(),id);if(a==visible.end()||b==visible.end()){Select(id,additive);return;}if(!additive)m_selection.clear();if(a>b)std::swap(a,b);for(auto i=a;i<=b;++i)if(!IsSelected(*i))m_selection.push_back(*i);m_selected=id;}
bool EditorDocument::DeleteSelection(std::string& error){auto candidate=m_scene;auto roots=SelectionRoots();if(roots.empty()){error="select objects first";return false;}for(auto id:roots)candidate.DestroyObject(id);std::string text;SaveSceneToString(candidate,text);Scene validated;if(!LoadSceneFromString(text,validated,error))return false;if(!CommitCandidate(std::move(validated),error,true,true))return false;PruneSelection();return true;}
bool EditorDocument::ApplySource(const std::string& text,std::string& error){Scene candidate;if(!LoadSceneFromString(text,candidate,error))return false;if(!CommitCandidate(std::move(candidate),error,true,true))return false;PruneSelection();return true;}
bool EditorDocument::ExternalChanged()const{return !m_path.empty()&&Source(m_path)!=m_loadedSource;}

bool EditorDocument::SaveNamed(std::string& error){
    if(m_path.empty()){error="Save As first to choose the scene path";return false;}
    if(ExternalChanged()){error="External edit detected; reload or save a reviewed copy";return false;}
    std::string legacy;SaveSceneToString(m_scene,legacy);
    if(!WriteAuthoredDocument(m_path,legacy,"scene",error,true))return false;
    m_loadedSource=Source(m_path);m_dirty=false;return true;
}
bool EditorDocument::SetWorldTransforms(const std::map<SceneObjectId,SceneTransform>& targets,std::string& error){
    Scene flat;if(!FlattenHierarchy(m_scene,flat,error))return false;
    auto candidate=m_scene;
    for(auto& [id,t]:targets){auto* object=candidate.Find(id);if(!object){error="Missing selected entity";return false;}if(!SetWorld(*object,t,flat,error))return false;}
    std::string text;SaveSceneToString(candidate,text);return ApplySource(text,error);
}
bool EditorDocument::AlignSelection(unsigned axis,bool distribute,std::string& error){
    auto roots=SelectionRoots();if(axis>2||roots.size()<2){error="Select at least two independent entities and an axis";return false;}
    Scene flat;if(!FlattenHierarchy(m_scene,flat,error))return false;
    std::map<SceneObjectId,SceneTransform> transforms;
    std::stable_sort(roots.begin(),roots.end(),[&](auto a,auto b){return flat.Find(a)->transform.position[axis]<flat.Find(b)->transform.position[axis];});
    auto first=flat.Find(roots.front())->transform.position[axis],last=flat.Find(roots.back())->transform.position[axis];
    float primary=flat.Find(m_selected)->transform.position[axis];
    for(size_t i=0;i<roots.size();++i){auto t=flat.Find(roots[i])->transform;t.position[axis]=distribute?glm::mix(first,last,float(i)/float(roots.size()-1)):primary;transforms[roots[i]]=t;}
    return SetWorldTransforms(transforms,error);
}
bool EditorDocument::SnapSelection(float translation,float degrees,float scale,std::string& error){
    if(!std::isfinite(translation)||!std::isfinite(degrees)||!std::isfinite(scale)||translation<=0||degrees<=0||scale<=0){error="Positive finite snap increments required";return false;}
    Scene flat;if(!FlattenHierarchy(m_scene,flat,error))return false;std::map<SceneObjectId,SceneTransform> transforms;
    for(auto id:SelectionRoots()){auto t=flat.Find(id)->transform;auto angles=glm::degrees(glm::eulerAngles(t.rotation));for(int a=0;a<3;++a){t.position[a]=std::round(t.position[a]/translation)*translation;t.scale[a]=std::max(scale,std::round(t.scale[a]/scale)*scale);angles[a]=std::round(angles[a]/degrees)*degrees;}t.rotation=glm::quat(glm::radians(angles));transforms[id]=t;}
    return SetWorldTransforms(transforms,error);
}
bool EditorDocument::RenameSelection(const std::string& prefix,std::string& error){
    if(prefix.empty()||prefix.size()>200){error="Name prefix must contain 1–200 bytes";return false;}
    auto candidate=m_scene;unsigned n=1;for(auto& o:candidate.Objects())if(IsSelected(o.id))o.name=prefix+" "+std::to_string(n++);
    std::string text;SaveSceneToString(candidate,text);return ApplySource(text,error);
}
bool EditorDocument::SurfaceSnap(const RuntimeWorld& world,glm::vec3 direction,float distance,bool orient,std::string& error){
    if(!std::isfinite(glm::dot(direction,direction))||glm::length(direction)<1e-6f||!std::isfinite(distance)||distance<=0){error="Finite nonzero cast direction and positive distance required";return false;}
    direction=glm::normalize(direction);Scene flat;if(!FlattenHierarchy(m_scene,flat,error))return false;
    PhysicsQueryFilter filter;for(auto body:world.Physics().AliveBodies())if(IsSelected(world.EntityIdOfBody(body)))filter.ignoredBodies.push_back(body);
    std::map<SceneObjectId,SceneTransform> transforms;for(auto id:SelectionRoots()){auto t=flat.Find(id)->transform;auto hit=world.Physics().Raycast(t.position,direction,distance,filter);if(!hit.hit){error="No normal collision surface for entity #"+std::to_string(id);return false;}t.position=hit.point;if(orient)t.rotation=glm::normalize(glm::rotation(t.rotation*glm::vec3(0,1,0),hit.normal)*t.rotation);transforms[id]=t;}
    return SetWorldTransforms(transforms,error);
}
std::vector<std::string> EditorDocument::SelectionReferences()const{
    std::vector<std::string> refs;
    auto add=[&](const SceneObject& from,SceneObjectId target,const char* field){if(target&&IsSelected(target))refs.push_back("#"+std::to_string(from.id)+" "+from.name+" / "+field+" -> #"+std::to_string(target));};
    for(auto& o:m_scene.Objects()){add(o,o.parent,"parent");if(o.gravitySelection)add(o,o.gravitySelection->source,"selected gravity source");if(o.socket)add(o,o.socket->target,"socket");if(o.render)add(o,o.render->textureCamera,"render camera");if(o.joint){add(o,o.joint->bodyA,"joint A");add(o,o.joint->bodyB,"joint B");}if(o.liquidConnection){add(o,o.liquidConnection->source,"liquid source");add(o,o.liquidConnection->destination,"liquid destination");}if(o.deformable)for(auto& a:o.deformable->attachments)add(o,a.target,"attachment");for(auto& slot:o.scripts)for(auto id:ScriptSystem::PropertyEntities(slot.properties))add(o,id,"script property");}
    return refs;
}
