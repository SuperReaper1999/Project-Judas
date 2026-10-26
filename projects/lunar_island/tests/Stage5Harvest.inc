const auto forest=Read(project.parent_path()/"Authoring/region_manifest.json")["regions"]["forest"];
const auto treeNode=*std::find_if(forest["resources"].begin(),forest["resources"].end(),[](const Json& n){return n["kind"]=="tree";});
const auto oreNode=*std::find_if(forest["resources"].begin(),forest["resources"].end(),[](const Json& n){return n["kind"]=="ore";});
auto observations=std::make_shared<Json>(Json::object());
auto visit=[&](){auto centre=V(forest["position"]);job(5,[&,centre](EngineHost&,RuntimeWorld& w){Aim(w,centre+glm::vec3(0,1,0),centre.y);});job(1,[&,centre](EngineHost&,RuntimeWorld& w){if(!DistrictState(w,"forest").contains("harvest")){--index;return;}Aim(w,centre+glm::vec3(0,1,0),centre.y);});};
auto depart=[&](){job(5,[&](EngineHost&,RuntimeWorld& w){Aim(w,{0,15,28},14);});job(1,[&](EngineHost&,RuntimeWorld& w){bool unloaded=false;for(auto& row:w.SceneControl()->StreamingIfLoaded()->Regions())if(row.id=="forest"){if(row.state=="failed"||row.state=="blocked")throw std::runtime_error(row.error);unloaded=row.state=="unloaded";}if(!unloaded){--index;return;}Check(true,"felled-tree region genuinely unloads");});};
if(mode=="harvestread") {
 job(5,[&](EngineHost&,RuntimeWorld& w){std::string error;request=w.SceneControl()->Saves(*w.Resources())->Request("load","island","","{}",error);Check(request!=0,"cold harvest requests ordinary save load");});
 job(1,[&,observations](EngineHost&,RuntimeWorld& w){auto* s=w.SceneControl()->Saves(*w.Resources())->Status(request);if(!s||s->state!="completed"||!State(w).contains("bag")){if(s&&s->state=="failed")throw std::runtime_error(s->error);--index;return;}auto expected=Read(out/"expected.json");*observations=expected["regionalHarvest"];Check(State(w)["bag"]==expected["state"]["bag"],"cold regional mining retains exact ore and tool inventory");});
} else {
 int branches=0,stones=0,fibres=0;for(auto& r:resources)if((r["kind"]=="branch"&&branches++<4)||(r["kind"]=="loose_stone"&&stones++<3)||(r["kind"]=="fiber"&&fibres++<2))gather(r);
 job(5,[&](EngineHost&,RuntimeWorld&){press("inventory");});click("tab_craft");select(0);click("primary");select(1);click("primary");job(5,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"stone_axe")==1&&Count(State(w),"stone_pickaxe")==1,"regional harvesting uses genuinely crafted tools");press("pause");});
}
visit();
if(mode=="harvestwrite") {
 job(5,[&,treeNode](EngineHost&,RuntimeWorld& w){auto p=V(treeNode["position"]);float height=treeNode["height"];Aim(w,p-glm::vec3(0,height/2-1.2f,0),p.y-height/2,2);press("hotbar_2");});
 for(int i=0;i<3;++i)job(45,[&](EngineHost&,RuntimeWorld&){press("primary");});
 job(600,[&,treeNode,observations](EngineHost& h,RuntimeWorld& w){auto id=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(treeNode["id"].get<std::string>());auto body=w.RuntimeBody(id);Check(w.Physics().IsDynamicBody(body),"regional axe releases actual tree into rigid-body physics");auto t=w.Physics().GetTransform(body);Check(glm::dot(t.rotation*glm::vec3(0,1,0),glm::vec3(0,1,0))<.95f,"regional tree genuinely falls under impulse/gravity");(*observations)["position"]=J(t.position);(*observations)["rotation"]={t.rotation.w,t.rotation.x,t.rotation.y,t.rotation.z};Screenshot(h,out/"regional-felled-tree.png");});
 job(5,[&,oreNode](EngineHost&,RuntimeWorld& w){auto p=V(oreNode["position"]);Aim(w,p,p.y-.6f,2);press("hotbar_3");});
 for(int i=0;i<8;++i)job(50,[&](EngineHost&,RuntimeWorld&){press("primary");});
 job(5,[&,oreNode,observations](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"iron_ore")==8,"eight real regional pickaxe hits yield eight ore");Check(!w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(oreNode["id"].get<std::string>()),"depleted regional ore loses actual collision");(*observations)["harvest"]=DistrictState(w,"forest")["harvest"];});
 depart();visit();
}
job(20,[&,treeNode,oreNode,observations](EngineHost& h,RuntimeWorld& w){auto* stream=w.SceneControl()->StreamingIfLoaded();auto id=stream->ResolvePersistentKey(treeNode["id"].get<std::string>());auto body=w.RuntimeBody(id);Check(body.IsValid()&&w.Physics().IsDynamicBody(body),"reconstructed regional trunk remains physically dynamic");auto t=w.Physics().GetTransform(body);Check(glm::distance(t.position,V((*observations)["position"]))<.03f,"reconstructed regional tree preserves actual settled endpoint");auto q=(*observations)["rotation"];glm::quat expected{q[0].get<float>(),q[1].get<float>(),q[2].get<float>(),q[3].get<float>()};Check(std::abs(glm::dot(expected,t.rotation))>1-1e-4f,"reconstructed regional tree preserves actual fallen orientation");Check(DistrictState(w,"forest")["harvest"]==(*observations)["harvest"],"district-owned cuts and mining depletion survive reconstruction");Check(!stream->ResolvePersistentKey(oreNode["id"].get<std::string>()),"depleted regional ore cannot respawn on return");Screenshot(h,out/"regional-restored-harvest.png");});
if(mode=="harvestwrite") {
 depart();job(5,[&,observations](EngineHost&,RuntimeWorld&){stage5Expected=*observations;press("save");});job(30,[](EngineHost&,RuntimeWorld&){});
} else {job(5,[&](EngineHost&,RuntimeWorld&){done=true;Quit();});}
