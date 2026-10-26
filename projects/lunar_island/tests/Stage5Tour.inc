// Player placement shortens travel for the content/lifecycle test. Vehicle route
// motion is verified separately. No content, inventory or NPC state is injected.
const auto districts=Read(project.parent_path()/"Authoring/region_manifest.json")["regions"];
auto observation=std::make_shared<Json>(Json::object());
for (auto it=districts.begin();it!=districts.end();++it) {
 const std::string key=it.key();const Json district=it.value();if(district.value("hidden",false))continue;const auto centre=V(district["position"]);
 job(5,[&,centre](EngineHost&,RuntimeWorld& w){Aim(w,centre+glm::vec3(0,1,0),centre.y,8);});
 job(1,[&,key,district,centre](EngineHost&,RuntimeWorld& w){
  auto* stream=w.SceneControl()->StreamingIfLoaded();bool active=false;
  for(const auto& row:stream->Regions())if(row.id==key){if(row.state=="failed"||row.state=="blocked")throw std::runtime_error(key+": "+row.error);active=row.state=="active";}
  if(!active){--index;return;}
  const auto ref=district["primaryChest"].get<std::string>();if(!DistrictState(w,key).value("housing",Json::object()).value("containers",Json::object()).contains(ref)){--index;return;}
  Check(stream->ResolvePersistentKey(ref)&&w.RuntimeBody(stream->ResolvePersistentKey(ref)).IsValid(),key+" publishes physical enterable-house cargo");
  Check(DistrictState(w,key)["housing"]["containers"][ref]["slots"].is_array(),key+" registers shared finite inventory");
  Aim(w,centre+glm::vec3(0,1,0),centre.y,8);
 });
 job(90,[&,key,district,centre,observation](EngineHost& h,RuntimeWorld& w){
  Check(State(w)["exploration"]["visited"][key]==true,key+" discovers through physical proximity");
  auto nav=w.Navigation().Sample(centre+glm::vec3(0,0,12),4);Check(bool(nav),key+" has genuinely usable native street navigation");
  if(!nav){for(auto& e:w.Navigation().Errors())std::cout<<"NAV ERROR "<<e.first<<" "<<e.second<<std::endl;}
  PhysicsQueryFilter f;f.requiredTags=1;auto hit=w.Physics().Raycast(centre+glm::vec3(0,10,12),{0,-1,0},20,f);Check(hit.hit&&std::abs(hit.point.y-centre.y)<.2f,key+" street is real physical geometry");
  auto* stream=w.SceneControl()->StreamingIfLoaded();(*observation)[key]={{"bodies",w.Physics().AliveBodyCount()},{"streamActive",stream->Stats().active},{"retainedBytes",stream->Stats().retainedBytes}};
  Screenshot(h,out/(key+".png"));
 });
}
job(5,[&](EngineHost&,RuntimeWorld& w){Aim(w,{0,15,28},14);});
job(160,[&,observation,districts](EngineHost& h,RuntimeWorld& w){
 auto* stream=w.SceneControl()->StreamingIfLoaded();for(const auto& row:stream->Regions())if(districts.contains(row.id))Check(row.state=="unloaded",row.id+" actually unloads after departure: "+row.state+" "+row.error);
 Check(stream->Stats().retainedBytes>0,"inactive districts retain real state");
 Write(out/"tour-observations.json",*observation);Screenshot(h,out/"returned-home.png");done=true;Quit();
});
