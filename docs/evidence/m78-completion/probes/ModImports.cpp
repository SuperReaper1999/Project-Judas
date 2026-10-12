// Actual QuickJS module evaluation. The effective catalog is constructed at the
// AssetDatabase publication seam; package admission is covered separately.
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "AssetDatabase.h"
#include "ResourceManager.h"
#include "AuthoringJSON.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;using J=nlohmann::json;
J rows=J::array();unsigned failures=0;
void Check(bool ok,const std::string& name){rows.push_back({{"name",name},{"passed",ok}});failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';}
int main(int argc,char**argv){if(argc!=2||fs::exists(argv[1]))return 2;fs::path out=fs::absolute(argv[1]);fs::create_directories(out);std::string error;
 std::map<AssetId,AssetRecord> records;
 auto add=[&](const char* id,const char* rel,const char* owner,const std::string& text){auto path=out/rel;fs::create_directories(path.parent_path());std::ofstream(path)<<text;AssetRecord r;r.id=id;r.relativePath=rel;r.path=path.string();r.type=AssetType::Script;r.owner=owner;records[id]=r;};
 add("11111111111111111111111111111111","Assets/a/main.js","a","");
 add("22222222222222222222222222222222","Assets/dep/public.js","dep","export const value=7;");
 add("33333333333333333333333333333333","Assets/dep/private.js","dep","export const value=99;");
 add("44444444444444444444444444444444","Assets/a/local.js","a","export const value=3;");
 add("55555555555555555555555555555555","Assets/base.js","","export const value=11;");
 J catalog={{"exports",{{"dep",{{"public",{{"kind","script"},{"value","22222222222222222222222222222222"}}},{"private",{{"kind","script"},{"value","33333333333333333333333333333333"}}}}}}}};
 AssetDatabase db;Check(db.SetEffectiveRecords(records,catalog.dump(),{{"a",{"Assets/dep/public.js"}}},error),"effective owned module catalog published");
 struct Case{const char* name;const char* spec;int expected;const char* owner;};
 for(auto test:{Case{"same owner","./local.js",3,"a"},Case{"published dependency alias","mod:dep/public",7,"a"},Case{"declared dependency relative path","../dep/public.js",7,"a"},Case{"private alias denied","mod:dep/private",-1,"a"},Case{"private relative path denied","../dep/private.js",-1,"a"},Case{"base cannot import mod","mod:dep/public",-1,""},Case{"undeclared owner denied","mod:dep/public",-1,"outsider"},Case{"unregistered traversal denied","../../secret.js",-1,"a"}}){
  auto r=records;r.begin()->second.owner=test.owner;std::ofstream(r.begin()->second.path)<<"import {value} from '"<<test.spec<<"'; export default class {constructor(){this.state={value};}}";
  Check(db.SetEffectiveRecords(r,catalog.dump(),{{"a",{"Assets/dep/public.js"}}},error),std::string(test.name)+" fresh catalog");
  Scene scene;auto id=scene.CreateObject("caller").id;scene.Find(id)->scripts.push_back({1,r.begin()->first,true,"{}"});
  // A healthy independent script proves rejected imports do not corrupt the VM.
  add("66666666666666666666666666666666","Assets/healthy.js","","export default class {constructor(){this.state={alive:42};}}");r["66666666666666666666666666666666"]=records.at("66666666666666666666666666666666");db.SetEffectiveRecords(r,catalog.dump(),{{"a",{"Assets/dep/public.js"}}},error);
  auto other=scene.CreateObject("healthy").id;scene.Find(other)->scripts.push_back({1,"66666666666666666666666666666666",true,"{}"});ResourceManager resources(nullptr,&db);RuntimeWorld world;Check(world.Build(scene,&resources,error),std::string(test.name)+" normal world");world.UpdateScripts(nullptr,0);auto& vm=*world.Scripts();auto captured=vm.Capture();J state=J::object();bool alive=false;for(const auto& record:captured){if(record.entity==id)state=J::parse(record.json);if(record.entity==other)alive=J::parse(record.json).value("alive",0)==42;}
  if(test.expected>=0)Check(vm.Diagnostics().empty()&&state.value("value",-1)==test.expected,std::string(test.name)+" executes actual imported value");else Check(!vm.Diagnostics().empty()&&!state.contains("value"),std::string(test.name)+" fails before imported code executes");
  Check(alive,std::string(test.name)+" preserves independent script execution");for(const auto& d:vm.Diagnostics())std::cout<<"DIAGNOSTIC "<<test.name<<": "<<d.message<<'\n';world.Destroy();
 }
 std::ofstream(out/"RESULT.json")<<J{{"checks",rows},{"failures",failures},{"scope","QuickJS published dependency, private import, undeclared owner, base isolation and module fault isolation; not a security sandbox or package-store crash test"}}.dump(2)<<'\n';std::cout<<"SUMMARY "<<rows.size()<<" checks / "<<failures<<" failures\n";return failures?1:0;
}
