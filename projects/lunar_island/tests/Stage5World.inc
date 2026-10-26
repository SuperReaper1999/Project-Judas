const auto districts=Read(project.parent_path()/"Authoring/region_manifest.json")["regions"];
auto observations=std::make_shared<Json>(Json::object());
auto travel=[&](const std::string& key){const auto d=districts[key];const auto centre=V(d["position"]);
 job(5,[&,centre](EngineHost&,RuntimeWorld& w){Aim(w,centre+glm::vec3(0,1,0),centre.y,8);});
 job(1,[&,key,d,centre](EngineHost&,RuntimeWorld& w){auto* stream=w.SceneControl()->StreamingIfLoaded();bool ready=false;for(auto& row:stream->Regions())if(row.id==key){if(row.state=="failed"||row.state=="blocked")throw std::runtime_error(key+": "+row.error);ready=row.state=="active";}if(!ready||!DistrictState(w,key).contains("housing")){--index;return;}Aim(w,centre+glm::vec3(0,1,0),centre.y,8);});
};
if(mode=="worldread") {
 job(5,[&](EngineHost&,RuntimeWorld& w){std::string error;request=w.SceneControl()->Saves(*w.Resources())->Request("load","island","","{}",error);Check(request!=0,"fresh process requests ordinary island save");});
 job(1,[&,observations](EngineHost&,RuntimeWorld& w){auto* status=w.SceneControl()->Saves(*w.Resources())->Status(request);if(!status||status->state!="completed"){if(status&&status->state=="failed")throw std::runtime_error(status->error);--index;return;}if(!State(w).contains("bag")){--index;return;}auto expected=Read(out/"expected.json");*observations=expected["island"];std::cout<<"COLD ROOT expected "<<expected["state"]["bag"].dump()<<" actual "<<State(w)["bag"].dump()<<" discoveries "<<State(w)["exploration"].dump()<<std::endl;Check(State(w)["bag"]==expected["state"]["bag"],"cold island restores exact carried resources and loot");Check(State(w)["exploration"]==expected["state"]["exploration"],"cold island restores discoveries and tracked location");});
} else {
 // Get the real starter weapon from Mara, then return to ordinary world play.
 job(5,[&](EngineHost&,RuntimeWorld& w){AimEntityFixture(w,6000);press("interact");});click("primary");
 job(5,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"pistol")==1,"Mara supplies real expedition sidearm");press("pause");});
 job(5,[&](EngineHost&,RuntimeWorld&){press("inventory");});
 job(5,[&](EngineHost&,RuntimeWorld& w){auto rows=State(w)["bag"]["slots"];int row=0;for(auto& s:rows){if(s.is_null())continue;if(s["item"]=="pistol")break;++row;}auto h=w.UI().Find("lunar");w.UI().Select(h,"entries",row-1);w.UI().Focus(h,"entries");press("ui_down");});click("primary");
 job(5,[&](EngineHost&,RuntimeWorld&){press("pause");press("reload");});job(100,[](EngineHost&,RuntimeWorld&){});
}
travel("town");
if(mode=="worldread") {
 job(125,[&,observations](EngineHost& h,RuntimeWorld& w){auto state=DistrictState(w,"town");Check(state["housing"]["containers"]==(*observations)["townContainers"],"inactive town reconstructs exact depleted storage after cold load");auto ref=(*observations)["townDoor"].get<std::string>();auto id=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(ref);JointState joint;bool found=w.Physics().GetJoint(w.RuntimeJoint(id),joint);Check(found&&joint.coordinate< -1,"cold regional door remains actually open, not just a saved flag");auto board=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey((*observations)["board"].get<std::string>());Check(board&&w.RuntimeBody(board).IsValid()&&w.RuntimeJoint(board).IsValid(),"cold regional barricade retains actual body and support joint");Screenshot(h,out/"cold-town.png");});
} else {
 const auto d=districts["town"];const auto chest=d["primaryChest"].get<std::string>();const auto door=d["doors"][0]["entity"].get<std::string>();const auto floor=d["position"][1].get<float>();
 job(5,[&,chest,floor](EngineHost&,RuntimeWorld& w){auto id=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(chest);Aim(w,w.Physics().GetTransform(w.RuntimeBody(id)).position,floor,1.1,{0,0,-1});press("interact");});click("primary");
 job(5,[&](EngineHost&,RuntimeWorld& w){auto h=w.UI().Find("lunar");w.UI().Select(h,"entries2",-1);w.UI().Focus(h,"entries2");press("ui_down");});click("secondary");
 job(5,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"bandage")==1,"regional house storage transfers one actual dressing");auto h=w.UI().Find("lunar");w.UI().Select(h,"entries2",2);w.UI().Focus(h,"entries2");press("ui_down");});click("tertiary");
 job(5,[&](EngineHost&,RuntimeWorld& w){auto h=w.UI().Find("lunar");w.UI().Select(h,"entries2",2);w.UI().Focus(h,"entries2");press("ui_down");});click("tertiary");
 job(5,[&,observations](EngineHost& h,RuntimeWorld& w){Check(Count(State(w),"planks")==6&&Count(State(w),"nails")==6,"real regional storage supplies construction materials");(*observations)["townContainers"]=DistrictState(w,"town")["housing"]["containers"];Screenshot(h,out/"town-storage.png");press("pause");});
 const auto opening=d["sockets"][0];
 job(5,[&,opening,floor](EngineHost&,RuntimeWorld& w){Aim(w,V(opening["position"])+glm::vec3(1.1f,0,0),floor,2.5,{0,0,-1});press("interact");});click("primary");
 job(10,[&,observations](EngineHost&,RuntimeWorld& w){auto state=DistrictState(w,"town");std::cout<<"BOARD LEDGERS root "<<State(w)["housing"]["boards"].dump()<<" regional "<<state["housing"]["boards"].dump()<<std::endl;Check(state["housing"]["boards"].size()==1,"regional window gains a genuine constructed barricade");Check(Count(State(w),"planks")==4&&Count(State(w),"nails")==5,"regional barricade debits actual materials once");if(state["housing"]["boards"].size()){auto ref=state["housing"]["boards"].begin().value().get<std::string>();auto id=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(ref);Check(id&&w.RuntimeJoint(id).IsValid()&&w.SceneControl()->StreamingIfLoaded()->Owner(id)=="town","constructed barricade collision/joint shares its support region");(*observations)["board"]=ref;}if(w.UI().Paused())press("pause");});
 job(5,[&,door,floor](EngineHost&,RuntimeWorld& w){auto id=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(door);Aim(w,w.Physics().GetTransform(w.RuntimeBody(id)).position,floor,1.8);press("interact");});
 job(130,[&,door,observations](EngineHost&,RuntimeWorld& w){auto id=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(door);JointState joint;Check(w.Physics().GetJoint(w.RuntimeJoint(id),joint)&&joint.coordinate< -1,"regional cottage door really opens through native hinge");(*observations)["townDoor"]=door;});
}
travel("forest");
const auto salvage=*std::find_if(districts["forest"]["resources"].begin(),districts["forest"]["resources"].end(),[](const Json& n){return n["kind"]=="salvage";});
if(mode=="worldread") {
 job(20,[&,observations,salvage](EngineHost&,RuntimeWorld& w){Check(DistrictState(w,"forest")["harvest"]==(*observations)["forestHarvest"],"cold forest restores district-owned depletion ledger");Check(!w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(salvage["id"].get<std::string>()),"cold reconstructed depleted salvage has no physical body");});
} else {
 job(5,[&,salvage](EngineHost&,RuntimeWorld& w){auto id=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(salvage["id"].get<std::string>());Aim(w,w.Physics().GetTransform(w.RuntimeBody(id)).position,34,1.5);press("interact");});
 job(10,[&,observations,salvage](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"scrap")==4,"actual forest salvage gives finite gathered scrap");Check(!w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(salvage["id"].get<std::string>()),"harvest removes actual regional collision");(*observations)["forestHarvest"]=DistrictState(w,"forest")["harvest"];});
}
travel("industry");
if(mode=="worldwrite") {
 for(int shot=0;shot<4;++shot){job(35,[&](EngineHost&,RuntimeWorld& w){auto id=w.SceneControl()->StreamingIfLoaded()->Resolve("industry",45010);Aim(w,w.Physics().GetTransform(w.RuntimeBody(id)).position,33.3f,6);press("primary");});}
 job(40,[&,observations](EngineHost& h,RuntimeWorld& w){auto id=w.SceneControl()->StreamingIfLoaded()->Resolve("industry",45010);auto state=PersonState(w,id);Check(state["dead"]==true,"regional hostile is defeated by four real pistol shots");Check(State(w)["combat"].value("campKills",0)==0,"unrelated island kill does not clear Mara's homestead camp objective");(*observations)["enemy"]=state;auto corpseRef=state["corpse"].get<std::string>();auto corpse=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(corpseRef);Check(corpse&&w.Physics().IsDynamicBody(w.RuntimeBody(corpse)),"regional death leaves actual physical lootable remains");Check(w.SceneControl()->StreamingIfLoaded()->Owner(corpse)=="industry","physical remains share regional ownership");Screenshot(h,out/"works-combat.png");});
} else {
 job(40,[&,observations](EngineHost&,RuntimeWorld& w){auto id=w.SceneControl()->StreamingIfLoaded()->Resolve("industry",45010);auto state=PersonState(w,id);Check(state["dead"]==true&&state["bag"]==(*observations)["enemy"]["bag"],"cold regional actor remains dead with exact loot");auto corpse=w.SceneControl()->StreamingIfLoaded()->ResolvePersistentKey(state["corpse"].get<std::string>());Check(corpse&&w.Physics().IsDynamicBody(w.RuntimeBody(corpse)),"cold world restores actual regional corpse collision");});
}
job(5,[&](EngineHost&,RuntimeWorld& w){Aim(w,{0,15,28},14);});
job(160,[&,observations](EngineHost& h,RuntimeWorld& w){for(const auto& key:{"town","forest","industry"}){for(auto& row:w.SceneControl()->StreamingIfLoaded()->Regions())if(row.id==key)Check(row.state=="unloaded",std::string(key)+" truly suspends modified content: "+row.error);}Check(State(w).dump().size()<16000,"player save remains bounded independently of visited settlement contents");Screenshot(h,out/"world-return.png");if(mode=="worldread"){done=true;Quit();}else{stage5Expected=*observations;press("save");}});
job(30,[](EngineHost&,RuntimeWorld&){});
