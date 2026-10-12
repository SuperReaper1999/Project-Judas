#include "Project.h"
#include "AssetDatabase.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "RuntimeUI.h"
#include "InputSystem.h"
#include "ScriptSystem.h"
#include "AuthoringJSON.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;using J=nlohmann::json;
J rows=J::array();int failures=0;
void Check(bool ok,const std::string& name){rows.push_back({{"name",name},{"passed",ok}});failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';}
void Need(bool ok,const std::string& name,const std::string& error={}){Check(ok,name);if(!ok)throw std::runtime_error(name+":"+error);}
J State(RuntimeWorld& w,EntityId id){for(const auto& s:w.Scripts()->Capture())if(s.entity==id)return J::parse(s.json);return J::object();}
void Click(RuntimeWorld& w,InputSystem& input,glm::vec2 p){input.Reset();input.SetPhysical("mouse:Left",1);w.UI().Input(input,p,true,640,360);input.BeginFrame();input.SetPhysical("mouse:Left",0);w.UI().Input(input,p,true,640,360);w.DispatchUIEvents(&input,.016f);}
int main(int argc,char** argv){fs::path out;try{
 Need(argc==2,"fresh output argument");out=fs::absolute(argv[1]);Need(!fs::exists(out),"output not reused");fs::create_directories(out);
 Project project;std::string error;Need(Project::CreateNew((out/"project").string(),"UI lifetime audit",project,error),"ordinary project",error);
 UIDocument d;d.reference={640,360};d.modal=false;UIElement root;root.kind=UIKind::Canvas;root.id="canvas";d.elements.push_back(root);
 UIElement button;button.id="button";button.kind=UIKind::Button;button.parent="canvas";button.offset={30,40};button.size={100,40};button.text="AUTHORED";d.elements.push_back(button);
 auto uiPath=fs::path(project.AssetsDir())/"audit.judasui";Need(SaveUIDocument(uiPath.string(),d,error),"ordinary UI asset",error);std::string authored=SerializeUIDocument(d);
 AssetDatabase db;db.Scan(project.RootDir(),project.AssetsDir());AssetRecord ui;Need(db.Track(uiPath.string(),ui,error),"registered UI",error);
 auto ownerPath=fs::path(project.AssetsDir())/"owner.js";
 std::ofstream(ownerPath)<<"import {ui} from 'judas'; export const properties={name:{type:'string',default:''},asset:{type:'string',default:''}}; export default class {constructor({properties}){this.p=properties;this.state={events:[]};} start(){this.doc=ui.load(this.p.asset,this.p.name);this.button=this.doc.get('button');this.button.text=this.p.name;} onUI(e){if(e.document===this.p.name)this.state.events.push({type:e.type,element:e.element});}}";
 AssetRecord owner;Need(db.Track(ownerPath.string(),owner,error),"registered owner script",error);
 auto observerPath=fs::path(project.AssetsDir())/"observer.js";
 std::ofstream(observerPath)<<R"JS(import {ui} from 'judas'; export default class {constructor(){this.state={};} uiUpdate(){
 if(!this.a){this.a=ui.get('one');this.b=ui.get('two');if(!this.a||!this.b)return;this.e=this.a.get('button');this.f=this.b.get('button');
 const throws=(fn,type)=>{try{fn();return false;}catch(e){return e.name===type;}};
 this.state.missing=throws(()=>this.a.get('absent'),'ReferenceError');
 const original=JSON.stringify(this.e.layout);const copy=this.e.layout;copy.offset.x=999;this.state.detached=JSON.stringify(this.e.layout)===original;
 this.state.badKey=throws(()=>this.e.setLayout({offset:{x:200,y:40},bogus:{x:0,y:0}}),'TypeError')&&JSON.stringify(this.e.layout)===original;
 this.state.badSize=throws(()=>this.e.setLayout({offset:{x:200,y:40},size:{x:-1,y:40}}),'TypeError')&&JSON.stringify(this.e.layout)===original;
 this.state.nonfinite=throws(()=>this.e.setLayout({offset:{x:NaN,y:1}}),'TypeError')&&JSON.stringify(this.e.layout)===original;
 this.e.setLayout({offset:{x:200,y:40},size:{x:120,y:50}});this.state.changed=this.e.layout.offset.x===200;this.b.hide();
 this.state.badValue=throws(()=>{this.e.value=Infinity;},'TypeError');this.state.badFlag=throws(()=>{this.e.enabled=1;},'TypeError');
 this.a.modal=true;this.state.modal=true;
 }else{let count=0;for(const fn of [()=>this.a.visible,()=>this.e.text,()=>this.e.layout,()=>this.e.setLayout({}),()=>this.a.unload()]){try{fn();}catch(e){if(e.name==='ReferenceError')count++;}}this.state.stale=count;this.state.other=this.b.get('button').text;}
}})JS";
 AssetRecord observer;Need(db.Track(observerPath.string(),observer,error),"registered observer",error);
 ResourceManager resources(nullptr,&db);resources.SetBlockingMode(true);Scene scene;auto& one=scene.CreateObject("owner one");auto id1=one.id;one.scripts.push_back({1,owner.id,true,J{{"name","one"},{"asset",ui.id}}.dump()});
 auto& two=scene.CreateObject("owner two");auto id2=two.id;two.scripts.push_back({1,owner.id,true,J{{"name","two"},{"asset",ui.id}}.dump()});auto& obs=scene.CreateObject("observer");auto oid=obs.id;obs.scripts.push_back({1,observer.id,true,"{}"});
 RuntimeWorld world;Need(world.Build(scene,&resources,error,&project.Settings().classification),"world builds",error);InputSystem input;world.UpdateUIScripts(&input,0);
 Need(world.Scripts()->Diagnostics().empty(),"actual public UI scripts have no unhandled exception");auto state=State(world,oid);for(auto key:{"missing","detached","badKey","badSize","nonfinite","changed","badValue","badFlag"})Check(state.value(key,false),std::string("public JS ")+key);
 auto h1=world.UI().Find("one"),h2=world.UI().Find("two");Need(h1&&h2&&h1!=h2,"independent documents sharing immutable asset");Check(world.UI().Paused(),"public modal true owns pause");
 world.UI().Layout(640,360);auto r=world.UI().LayoutOf(h1,"button")->rect;Check(r.position==glm::vec2(200,40)&&r.size==glm::vec2(120,50),"independent computed hit geometry observes public patch");
 Click(world,input,{60,60});auto before=State(world,id1)["events"].size();Check(before==0,"old button rectangle cannot activate after layout move");
 Click(world,input,{250,60});auto events=State(world,id1)["events"];Check(events.size()>0&&events.back()["type"]=="click","new button rectangle activates actual JS callback");Check(State(world,id2)["events"].empty(),"hidden independent document receives no targeted click");
 world.UI().Element(h1,"button")->enabled=false;auto count=State(world,id1)["events"].size();Click(world,input,{250,60});Check(State(world,id1)["events"].size()==count,"disabled control does not click");
 Need(world.DestroyEntity(id1,&error),"normal owner destruction",error);Check(!world.UI().Document(h1)&&world.UI().Document(h2),"owner destroy unloads only owned UI");Check(!world.UI().Paused(),"modal owner removal releases pause");world.UpdateUIScripts(&input,0);state=State(world,oid);Check(state.value("stale",0)==5,"retained public handles throw after owner destruction");Check(state.value("other",std::string{})=="two","other instance retains state after owner destruction");
 auto slots=world.ScriptSlots();for(auto& object:slots)if(object.id==id2)for(auto& slot:object.scripts)slot.enabled=false;world.Scripts()->Synchronize(slots);Check(!world.UI().Document(h2),"normal slot synchronization disables only that slot-owned UI");
 std::string file;{std::ifstream f(uiPath);file.assign(std::istreambuf_iterator<char>(f),{});}UIDocument decoded;Need(LoadUIDocument(uiPath.string(),decoded,error),"authored UI reload",error);Check(SerializeUIDocument(decoded)==authored,"runtime UI changes do not rewrite asset");
 world.EndScripts();Check(!world.UIIfLoaded(),"Stop releases UI and VM");Need(world.Build(scene,&resources,error,&project.Settings().classification),"fresh world rebuild",error);world.UpdateUIScripts(&input,0);Check(world.Scripts()->Diagnostics().empty()&&world.UI().Find("one")&&world.UI().Find("two"),"new world has fresh scripts/documents without stale owner state");world.Destroy();resources.Shutdown();
 }catch(const std::exception& e){Check(false,std::string("fixture failure: ")+e.what());}
 if(!out.empty()&&fs::exists(out)){std::ofstream(out/"RESULT.json")<<J{{"checks",rows},{"failures",failures},{"scope","Public QuickJS UI mutation/lifetime + native input/layout effects; no renderer pixels or all scene-transition paths"}}.dump(2)<<'\n';}
 std::cout<<"SUMMARY "<<rows.size()<<" checks "<<failures<<" failures\n";return failures?1:0;}
