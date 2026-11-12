#include "AssetDatabase.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "fluid/GameWaterRuntime.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    int checks = 0, failures = 0;
    auto check = [&](bool value, const std::string &message) {
        ++checks;
        if (!value) {
            ++failures;
            std::cerr << "FAIL " << message << '\n';
        }
    };

    const auto root = fs::temp_directory_path() /
                      ("judas-water-script-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "Assets");
    const auto path = root / "Assets/water.js";
    {
        std::ofstream file(path);
        file << R"JS(
import {world} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={};}
 start(){
  const w=this.entity.gridWater;
  if(!w?.valid||world.gridWaterRegions.length!==1||w.state.quantity!==16)throw new Error('initial state');
  let rejected=false;try{w.changeVolume({x:0,y:1,z:0},1,1);}catch(_){rejected=true;}
  if(!rejected)throw new Error('mutation outside fixedUpdate');
  const detached=w.state;detached.quantity=0;
  if(w.state.quantity!==16)throw new Error('state aliases native');
  this.state.start=true;
 }
 fixedUpdate(){
  const w=this.entity.gridWater,s=w.state;
  const added=w.changeVolume({x:0,y:1.8,z:0},.125,.8);
  const removed=w.changeVolume({x:0,y:.5,z:0},-.05,.8);
  if(added!==.125||removed!==-.05||Math.abs(w.state.quantity-s.quantity-.075)>1e-10)throw new Error('source receipts');
  for(const [point,amount,radius] of [[{x:NaN,y:0,z:0},1,1],[{x:0,y:0,z:0},Infinity,1],[{x:0,y:0,z:0},1,0]]){
   let rejected=false;try{w.changeVolume(point,amount,radius);}catch(_){rejected=true;}
   if(!rejected)throw new Error('invalid source accepted');
  }
  if(!w.impulse({x:0,y:.5,z:0},{x:1,y:0,z:0},.5))throw new Error('impulse');
  this.state.fixed=true;
 }
})JS";
    }
    AssetDatabase assets;
    assets.Scan(root.string(), (root / "Assets").string());
    AssetRecord record;
    std::string error;
    check(assets.Track(path.string(), record, error), "track ordinary script: " + error);
    ResourceManager resources(nullptr, &assets);
    Scene scene;
    auto &owner = scene.CreateObject("Scripted water");
    owner.fluidVolume = SceneFluidVolumeComponent{};
    auto &f = *owner.fluidVolume;

    f.spacing = .5f;
    f.countX = f.countY = f.countZ = 8;
    f.fillHeight = 1;
    owner.scripts.push_back({1, record.id, true, "{}"});
    ProjectSettings settings;

    settings.advancedFluidDynamics = true;
    RuntimeWorld world;
    world.ConfigureProject(settings);
    check(world.Build(scene, &resources, error), "build shared VM host: " + error);
    InputSystem input;
    world.UpdateScripts(&input, .016f);
    world.FixedScripts(&input, .016f);
    if (world.Scripts())
        for (const auto &d : world.Scripts()->Diagnostics())
            std::cerr << d.message << '\n';
    check(world.Scripts() && world.Scripts()->Diagnostics().empty(),
          "local Water binding executes without script faults");
    auto state = world.Scripts() ? world.Scripts()->Capture() : std::vector<ScriptStateRecord>{};
    check(state.size() == 1 && state[0].json.find("\"start\":true") != std::string::npos &&
              state[0].json.find("\"fixed\":true") != std::string::npos,
          "readback, execution-phase restrictions and real receipts passed "
          "inside JudasJS");
    check(world.GridWater() &&
              std::abs(world.GridWater()->Find(owner.id)->Quantity() - 16.075) < 1e-9,
          "native arrays agree with script ledger");
    world.Destroy();
    resources.Shutdown();
    fs::remove_all(root);
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
