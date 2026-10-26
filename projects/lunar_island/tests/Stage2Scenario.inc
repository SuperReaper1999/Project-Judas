// Included within the native write scenario. All game actions use real input/UI.
// Player positioning is a fixture; no resources, objects or integrity are granted.
  auto aimEntity=[&](EntityId id,float height=0){job(6,[&,id,height](EngineHost&,RuntimeWorld& w){AimEntityFixture(w,id,height);});};
  auto selectOther=[&](int row){job(3,[](EngineHost&,RuntimeWorld& w){auto h=w.UI().Find("lunar");w.UI().Select(h,"entries2",-1);w.UI().Focus(h,"entries2");});for(int i=0;i<=row;++i)job(3,[&](EngineHost&,RuntimeWorld&){press("ui_down");});};
  // Open a real hinged door, then loot/move the packing crate in the entrance.
  aimEntity(3061);job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});
  job(90,[&](EngineHost&,RuntimeWorld& w){JointState state;bool hinge=w.Physics().GetJoint(w.RuntimeJoint(3061),state);std::cout<<"DOOR physical "<<hinge<<" coordinate "<<state.coordinate<<" ledger "<<State(w)["housing"]["doors"].dump()<<std::endl;Check(hinge&&state.coordinate<-.15f,"E drives actual hinge motion rather than only a door flag");});
  aimEntity(3120);job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
  for(int i=0;i<2;++i){selectOther(0);click("tertiary");}
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"planks")==2&&Count(State(w),"nails")==2,"physical entrance crate yields its authored loot exactly once");press("pause");});
  aimEntity(3120);job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("secondary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){AimLive(h,w,{-16,14,-4},14,2);});
  job(6,[&](EngineHost&,RuntimeWorld&){press("build_rotate");});
  job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(60,[&](EngineHost&,RuntimeWorld& w){auto p=w.Physics().GetTransform(w.RuntimeBody(3120));Check(glm::distance(p.position,glm::vec3(-18.5f,14.595f,-6.5f))>2,"furniture placement actually moves the existing collision body");Check(w.RuntimeJoint(3120).IsValid(),"moved furniture receives a real physical support connection");Check(std::abs(p.rotation.y)>.6f,"R rotates the actual moved furniture");});
  job(180,[&](EngineHost&,RuntimeWorld& w){JointState state;Check(w.Physics().GetJoint(w.RuntimeJoint(3061),state)&&state.coordinate< -1,"door opens after the obstructing crate is cleared");auto t=w.RuntimeDefinition(100)->transform;t.position={-18.5f,14.25f,-3};t.rotation={1,0,0,0};w.SetRuntimeTransform(100,t);w.RuntimeCharacter(100)->velocity={};StateFixture(w,[](Json& s){s["yaw"]=0;s["pitch"]=0;s["third"]=false;});held={"move_y"};});
  job(90,[&](EngineHost& h,RuntimeWorld& w){held.clear();Check(w.RuntimeDefinition(100)->transform.position.z< -6.4f,"actual CharacterMotor walks through the opened doorway into the cottage");Screenshot(h,out/"entered-cottage.png");});
  // A non-empty chest cannot be dismantled. Take its four stacks through native UI.
  aimEntity(3101);job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("drop");
  job(4,[](EngineHost&,RuntimeWorld& w){Check(w.RuntimeBody(3101).IsValid(),"dismantling non-empty storage refuses without losing its body");});click("primary");
  for(int i=0;i<4;++i){selectOther(0);click("tertiary");}
  job(5,[&](EngineHost& h,RuntimeWorld& w){Check(Count(State(w),"planks")==10&&Count(State(w),"scrap")==6,"native storage transfers main loot to the shared player inventory");Screenshot(h,out/"storage.png");press("pause");});
  // Transferring equipped gear must revoke its gameplay use until taken back.
  job(4,[&](EngineHost&,RuntimeWorld&){press("inventory");});click("tab_pack");
  job(4,[&](EngineHost&,RuntimeWorld& w){int row=0;auto state=State(w);for(auto& slot:state["bag"]["slots"]){if(slot.is_null())continue;if(slot["item"]=="stone_pickaxe")break;++row;}auto h=w.UI().Find("lunar");w.UI().Select(h,"entries",row-1);w.UI().Focus(h,"entries");press("ui_down");});click("primary");job(4,[&](EngineHost&,RuntimeWorld&){press("pause");});
  aimEntity(3101);job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
  job(4,[&](EngineHost&,RuntimeWorld& w){int row=0;auto state=State(w);for(auto& slot:state["bag"]["slots"]){if(slot.is_null())continue;if(slot["item"]=="stone_pickaxe")break;++row;}auto h=w.UI().Find("lunar");w.UI().Select(h,"entries",row-1);w.UI().Focus(h,"entries");press("ui_down");});click("primary");
  job(4,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"stone_pickaxe")==0&&State(w)["equipped"]=="hands","storing an equipped tool removes its gameplay equipment state");});selectOther(0);click("secondary");job(4,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"stone_pickaxe")==1,"taking the stored tool restores the exact item");press("pause");});
  // Resting/claiming and native light state are functional, not decoration flags.
  aimEntity(3100);job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(State(w)["housing"]["home"].is_string(),"bed claims a persistent home reference");Check(State(w)["vitals"]["hunger"].get<float>()<71,"rest consumes actual survival supplies");press("pause");});
  aimEntity(3108);job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
  job(5,[&](EngineHost&,RuntimeWorld& w){bool off=false;for(auto& l:w.StaticLights())if(l.id==3108)off=!l.light.enabled;Check(off,"lamp interaction changes actual native illumination state");press("pause");});
  aimEntity(3102);job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");select(7);click("primary");click("primary");
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(Count(State(w),"metal_sheet")==2&&Count(State(w),"scrap")==0,"workbench consumes six scrap to make two metal sheets");press("pause");});
  // Secure a real open window, damage/repair it, reinforce and repair the metal.
  job(6,[&](EngineHost&,RuntimeWorld& w){Aim(w,{-20.4f,15.53f,-5.5f},14);});job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
  job(5,[&](EngineHost&,RuntimeWorld& w){metalBoard=FindNamed(w,"Barricade / wood");Check(metalBoard!=0&&w.RuntimeJoint(metalBoard).IsValid(),"barricade is an actual rigid body fixed to its opening");Check(Count(State(w),"planks")==8,"barricading consumes the authored plank cost");});
  job(6,[&](EngineHost&,RuntimeWorld& w){if(metalBoard)Aim(w,w.Physics().GetTransform(w.RuntimeBody(metalBoard)).position,14);press("hotbar_2");});
  job(35,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(6,[&](EngineHost&,RuntimeWorld& w){Check(BarricadeHealth(State(w))==40,"axe hit damages actual barricade's project integrity");press("interact");});click("primary");
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(BarricadeHealth(State(w))==60&&Count(State(w),"planks")==7,"repair restores integrity and debits a plank");});click("secondary");
  job(5,[&](EngineHost&,RuntimeWorld& w){metalBoard=FindNamed(w,"Barricade / metal");Check(metalBoard!=0&&Count(State(w),"metal_sheet")==1,"reinforcement replaces the physical board with a metal barrier");Check(BarricadeHealth(State(w))==140,"reinforcement raises actual game strength to 140");press("pause");});
  job(6,[&](EngineHost&,RuntimeWorld& w){if(metalBoard)Aim(w,w.Physics().GetTransform(w.RuntimeBody(metalBoard)).position,14);press("hotbar_3");});job(35,[&](EngineHost&,RuntimeWorld&){press("primary");});job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(BarricadeHealth(State(w))==140&&Count(State(w),"metal_sheet")==0,"reinforced repair consumes the last metal sheet");press("pause");});
  // Independent second opening: three axe hits remove actual collision, no refund.
  job(6,[&](EngineHost&,RuntimeWorld& w){Aim(w,{-13.6f,15.53f,-5.5f},14);});job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
  job(5,[&](EngineHost&,RuntimeWorld& w){brokenBoard=FindNamed(w,"Barricade / wood");if(brokenBoard)Aim(w,w.Physics().GetTransform(w.RuntimeBody(brokenBoard)).position,14);press("hotbar_2");});
  for(int i=0;i<3;++i)job(35,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(5,[&](EngineHost&,RuntimeWorld& w){Check(brokenBoard&&!w.RuntimeBody(brokenBoard).IsValid(),"destroyed barricade removes actual blocking collision");Check(metalBoard&&w.RuntimeBody(metalBoard).IsValid(),"other reinforced opening remains independent");});
  // Additional actual gathering funds the roof/door, without inventory grants.
  for(auto& r:resources)if(r["id"]=="208"||r["id"]=="210"||r["id"]=="212"||r["id"]=="214"||r["id"]=="390")gather(r);
  // Build a load-bearing pair through the actual preview, then remove its support.
  job(5,[&](EngineHost&,RuntimeWorld&){press("build_menu");});select(0);click("primary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){buildPoint=ClearBuildPoint(w);AimLive(h,w,buildPoint,buildPoint.y,.25f);});job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(6,[&](EngineHost&,RuntimeWorld& w){Check(State(w)["housing"]["stats"]["placed"]==0&&Count(State(w),"wood")==20,"self-overlapping placement refuses without spending resources");press("pause");});
  job(6,[&](EngineHost&,RuntimeWorld& w){Check(!w.UI().Paused()&&State(w)["housing"]["stats"]["placed"]==0,"Escape cancels a preview without pausing or spawning a piece");press("build_menu");});select(0);click("primary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){buildPoint=ClearBuildPoint(w);std::cout<<"CLEAR BUILD "<<J(buildPoint).dump()<<std::endl;AimLive(h,w,buildPoint,buildPoint.y,3.5f);});job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(10,[&](EngineHost&,RuntimeWorld& w){builtFoundation=FindNamed(w,"Stone foundation");std::cout<<"BUILD FEEDBACK "<<w.UI().Element(w.UI().Find("lunar"),"notice")->text<<" ledger "<<State(w)["housing"]["stats"].dump()<<std::endl;Check(builtFoundation&&w.RuntimeJoint(builtFoundation).IsValid(),"construction makes a real anchored foundation");Check(Count(State(w),"wood")==16&&Count(State(w),"stone")==3,"foundation cost debits once after successful physical placement");press("build_menu");});select(2);click("primary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){if(builtFoundation){auto p=w.Physics().GetTransform(w.RuntimeBody(builtFoundation)).position;AimLive(h,w,p+glm::vec3(0,.2f,0),14,3.5f);}});
  job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(10,[&](EngineHost&,RuntimeWorld& w){builtWall=FindNamed(w,"Timber wall");Check(builtWall&&w.RuntimeJoint(builtWall).IsValid(),"wall snaps and connects physically to the placed foundation");if(builtWall)wallBefore=w.Physics().GetTransform(w.RuntimeBody(builtWall)).position;press("build_menu");});select(5);click("primary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){if(builtWall)AimLive(h,w,w.Physics().GetTransform(w.RuntimeBody(builtWall)).position,buildPoint.y,3.5f);});job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(10,[&](EngineHost&,RuntimeWorld& w){builtRoof=FindNamed(w,"Roof panel");Check(builtRoof&&w.RuntimeJoint(builtRoof).IsValid(),"roof fits from a wall face and creates an actual attachment");if(builtFoundation)AimEntityFixture(w,builtFoundation,.1f);});
  // Aim at the foundation's exposed corner rather than through its wall.
  job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("drop");
  job(120,[&](EngineHost& h,RuntimeWorld& w){Check(builtFoundation&&!w.RuntimeBody(builtFoundation).IsValid(),"dismantling removes the foundation's real collision");Check(builtWall&&!w.RuntimeJoint(builtWall).IsValid(),"support loss releases the actual dependent joint");Check(builtWall&&w.Physics().GetTransform(w.RuntimeBody(builtWall)).position.y<wallBefore.y-.15f,"unsupported wall falls through actual gravity");Screenshot(h,out/"support-loss.png");});
  // Construct persistent storage and deposit real stone into its new container.
  job(5,[&](EngineHost&,RuntimeWorld&){press("build_menu");});select(8);click("primary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){AimLive(h,w,{8,14,38},14,3);});job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(10,[&](EngineHost&,RuntimeWorld& w){builtChest=FindNamed(w,"Storage chest");Check(builtChest&&w.RuntimeJoint(builtChest).IsValid(),"constructed storage uses ordinary native prefab and support joint");if(builtChest)Aim(w,w.Physics().GetTransform(w.RuntimeBody(builtChest)).position,14);});
  job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});click("primary");
  // Stage 1 gathering places stone in slot 4; select by current row name instead.
  job(5,[&](EngineHost&,RuntimeWorld& w){auto state=State(w);int row=0;for(auto& slot:state["bag"]["slots"]){if(slot.is_null())continue;if(slot["item"]=="stone")break;++row;}auto handle=w.UI().Find("lunar");w.UI().Select(handle,"entries",row-1);w.UI().Focus(handle,"entries");press("ui_down");});click("primary");
  job(5,[&](EngineHost& h,RuntimeWorld& w){Check(StoredCount(State(w),"stone")==1,"real stone transfers into constructed persistent storage");Screenshot(h,out/"constructed-storage.png");press("pause");});

  // A newly spawned door prefab must also have a functioning child-body hinge.
  job(5,[&](EngineHost&,RuntimeWorld&){press("build_menu");});select(4);click("primary");
  job(6,[&](EngineHost& h,RuntimeWorld& w){AimLive(h,w,{14,14,38},14,3);});job(6,[&](EngineHost&,RuntimeWorld&){press("primary");});
  job(10,[&](EngineHost&,RuntimeWorld& w){builtDoor=FindNamed(w,"Physical hinged leaf");Check(builtDoor&&w.RuntimeJoint(builtDoor).IsValid(),"newly constructed door instantiates its physical leaf and hinge");if(builtDoor)AimEntityFixture(w,builtDoor);});
  job(5,[&](EngineHost&,RuntimeWorld&){press("interact");});
  job(120,[&](EngineHost& h,RuntimeWorld& w){JointState state;Check(builtDoor&&w.Physics().GetJoint(w.RuntimeJoint(builtDoor),state)&&state.coordinate< -1,"constructed door opens through actual physics");Screenshot(h,out/"built-door.png");});
