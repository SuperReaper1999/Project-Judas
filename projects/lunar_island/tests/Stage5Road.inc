// An ordinary instantiated car begins on the real road. After construction no
// body pose, velocity, fuel or powertrain state is injected. Native input drives.
job(5,[&](EngineHost&,RuntimeWorld& w){
 PhysicsQueryFilter filter;filter.requiredTags=1;auto floor=w.Physics().Raycast({-500,200,180},{0,-1,0},400,filter);
 Check(floor.hit,"road fixture has actual collision");SceneTransform pose;pose.position=floor.point+glm::vec3(0,.9f,0);pose.rotation=glm::angleAxis(std::atan2(900.f,-20.f),glm::vec3(0,1,0));std::string error;
 roadVehicle=w.SpawnPrefab("14af9982203bb6c9e15378daea6df542",pose,error);Check(roadVehicle!=0,"ordinary car prefab constructs on road: "+error);
});
job(1,[&](EngineHost&,RuntimeWorld& w){if(!PersonState(w,roadVehicle).contains("fuel")){--index;return;}AimVehicleFixture(w,roadVehicle);});
job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
job(10,[&](EngineHost&,RuntimeWorld& w){Check(State(w)["transport"]["driving"]=="entity:"+std::to_string(roadVehicle),"road pilot enters through actual vehicle UI");roadActive=true;start=w.Physics().GetTransform(w.RuntimeBody(roadVehicle)).position;});
job(1,[&](EngineHost& h,RuntimeWorld& w){
 auto pos=w.Physics().GetTransform(w.RuntimeBody(roadVehicle)).position;
 if(glm::distance(glm::vec2(pos.x,pos.z),glm::vec2(-1400,200))>18){--index;return;}
 roadActive=false;Check(glm::distance(pos,start)>850,"actual native car travels over 850 metres along the island road");Check(PersonState(w,roadVehicle)["fuel"].get<double>()<28,"road trip consumes actual fuel");Check(PersonState(w,roadVehicle)["health"].get<double>()>50,"ordinary road does not destroy the car");Check(State(w)["exploration"]["visited"]["forest"]==true,"real driven traversal discovers the forest");
 Write(out/"road-endpoint.json",{{"position",J(pos)},{"state",PersonState(w,roadVehicle)},{"maxCrossTrack",roadMaxDeviation}});Screenshot(h,out/"forest-arrival.png");std::string error;PerformanceProfiler::Get().Export((out/"profile.json").string(),error);done=true;Quit();
});
