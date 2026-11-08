#include "AssetDatabase.h"
#include "InputSystem.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "scripting/BindingContext.h"
#include "scripting/BindingRegistry.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
int checks = 0, failures = 0;
void Check(bool okay, const char *message) {
    ++checks;
    failures += !okay;
    std::printf("%s %s\n", okay ? "PASS" : "FAIL", message);
}
class UnusedServices final : public ScriptBindings::RuntimeServices {
public:
    bool Json(JSValueConst, std::string &, std::string &) const override { return false; }
    JSValue RuntimeOperation(const std::string &, int, JSValueConst *) override { return JS_UNDEFINED; }
};

void ReliablePublication(RuntimeWorld &world, SceneObjectId id) {
    JudasParticles::ParticleImpact impact;
    impact.tick = 9007199254740993ull;
    impact.receiver = 9007199254740995ull;
    impact.geometryRevision = 9007199254740997ull;
    impact.position = {1000000000000.125, -1000000000000.25, 0};
    impact.localPosition = {.125, .25, .5};
    impact.normal = impact.localNormal = {0, 1, 0};
    impact.count = 17;
    impact.weight = 42.5;
    auto &pool = world.VisualEmitters().front().pool;
    Check(pool.ImpactState().Add(impact, pool.settings), "queue actual reliable observation for binding publication");
    auto *runtime = JS_NewRuntime();
    auto *js = runtime ? JS_NewContext(runtime) : nullptr;
    Check(js != nullptr, "independent binding allocation-failure control owns its VM");
    if (!js) { if (runtime) JS_FreeRuntime(runtime); return; }
    UnusedServices services;
    ScriptBindings::BindingStates states;
    const InputSystem *input = nullptr;
    bool fixed = true, ui = false, presentation = false, hasView = false;
    bool stopping = false, retiring = false, animation = false;
    float delta = .016f, alpha = 1;
    glm::vec3 origin{0}, direction{0, 0, -1};
    std::uint64_t owner = id, slot = 1;
    ScriptBindings::BindingContext context{js, &world, nullptr, services, states, input, fixed,
        delta, ui, presentation, alpha, hasView, origin, direction, owner, slot, stopping,
        retiring, animation};
    const auto &registry = ScriptBindings::BindingRegistry::Default();
    JSValue args[] = {JS_UNDEFINED, JS_NewString(js, std::to_string(id).c_str())};
    JS_SetMemoryLimit(runtime, 1);
    auto result = registry.Dispatch(context, "particleImpacts", 2, args);
    Check(JS_IsException(result) && world.PeekParticleImpacts(id) &&
              world.PeekParticleImpacts(id)->size() == 1,
          "VM allocation failure leaves reliable native observations queued");
    JS_FreeValue(js, result);
    JS_SetMemoryLimit(runtime, std::numeric_limits<std::size_t>::max());
    JS_FreeValue(js, JS_GetException(js));
    result = registry.Dispatch(context, "particleImpacts", 2, args);
    const bool published = !JS_IsException(result) && JS_IsArray(result);
    Check(published &&
              world.PeekParticleImpacts(id) && world.PeekParticleImpacts(id)->empty(),
          "successful detached array publication acknowledges native queue exactly once");
    if (!published) {
        JS_FreeValue(js, result); JS_FreeValue(js, args[1]);
        states.Clear(); JS_FreeContext(js); JS_FreeRuntime(runtime); return;
    }
    auto item = JS_GetPropertyUint32(js, result, 0);
    auto receiver = JS_GetPropertyStr(js, item, "receiver");
    const char *receiverText = JS_ToCString(js, receiver);
    auto tick = JS_GetPropertyStr(js, item, "tick");
    const char *tickText = JS_ToCString(js, tick);
    Check(receiverText && std::string(receiverText) == "9007199254740995" &&
              tickText && std::string(tickText) == "9007199254740993",
          "contact identities above JavaScript's integer range retain all bits");
    auto position = JS_GetPropertyStr(js, item, "position");
    auto x = JS_GetPropertyStr(js, position, "x");
    double actualX = 0;
    Check(JS_ToFloat64(js, &actualX, x) == 0 && actualX == impact.position.x,
          "absolute historical position remains binary64 rather than narrowed float32");
    JS_FreeCString(js, receiverText); JS_FreeCString(js, tickText);
    for (auto value : {x, position, receiver, tick, item, result, args[1]}) JS_FreeValue(js, value);
    states.Clear();
    JS_FreeContext(js); JS_FreeRuntime(runtime);
}
} // namespace

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "judas-particle-script-v2";
    fs::remove_all(root);
    fs::create_directories(root / "Assets");
    const auto path = root / "Assets/particles.js";
    {
        std::ofstream file(path);
        file << R"JS(export default class {
 constructor({entity}){this.entity=entity;this.state={};}
 start(){
  const e=this.entity;
  const assert=(v,message)=>{if(!v)throw new Error(message);};
  assert(e.setParticles({backend:'auto',collision:true,useWind:true,response:'slide',
    collisionRadius:0.125,restitution:0.75,friction:0.5,maxContacts:4,
    collisionLayers:'9223372036854775809',impacts:'reliable',impactCellSize:0.25,
    impactWeight:2.5,maxImpactCells:7,rate:0,burst:0,maxParticles:16,
    collisionRegion:true,collisionRegionOffset:{x:2,y:3,z:-4},
    collisionRegionHalfExtents:{x:7,y:11,z:13}}),'patch');
  const p=e.particles;
  assert(p.backend==='auto'&&p.response==='slide'&&p.impacts==='reliable','enums');
  assert(p.collisionLayers==='9223372036854775809'&&p.maxContacts===4&&p.maxImpactCells===7,'exact mask and budgets');
  assert(p.collision&&p.useWind&&p.collisionRadius===0.125&&p.restitution===0.75&&p.friction===0.5,'collision values');
  assert(p.collisionRegion&&p.collisionRegionOffset.z===-4&&p.collisionRegionHalfExtents.y===11,'coverage readback');
  const before=JSON.stringify(p);
  let rejected=0;
  for(const patch of [{rate:8,backend:'unknown'},{rate:8,maxContacts:2.5},
     {rate:8,maxContacts:0},{rate:8,collisionLayers:1},
     {rate:8,collisionLayers:'18446744073709551616'},
     {rate:8,collisionLayers:['unregistered']},{rate:8,impactCellSize:0},
     {rate:8,maxImpactCells:0},{rate:8,collision:1},{rate:8,unknown:true},
     {rate:8,collisionRegion:1},{rate:8,collisionRegionOffset:{x:NaN,y:0,z:0}},
     {rate:8,collisionRegionHalfExtents:{x:0,y:8,z:8}}]){
   try{e.setParticles(patch);}catch(_){rejected++;}
   assert(JSON.stringify(e.particles)===before,'invalid patch mutated emitter');
  }
  assert(rejected===13,'patch validation');
  rejected=0;
  for(const count of [-1,0.5,100001,NaN,Infinity,'2',4294967296]){
   try{e.burst(count);}catch(_){rejected++;}
  }
  assert(rejected===7&&e.burst(0)&&e.burst(2),'burst strict integer contract');
  const detached=e.particles;detached.maxContacts=1;
  assert(e.particles.maxContacts===4,'detached settings');
  assert(e.controlParticles('pause')&&e.particleStats.paused,'pause');
  assert(e.controlParticles('play')&&!e.particleStats.paused,'play');
  assert(e.controlParticles('clear')&&e.particleStats.live===0,'clear');
  assert(e.controlParticles('stop')&&e.controlParticles('restart'),'stop/restart');
  try{e.controlParticles('teleport');throw new Error('accepted invalid command');}
   catch(error){assert(error instanceof TypeError,'control validation');}
  const stats=e.particleStats;
  assert(stats.requested==='auto'&&stats.active==='cpu','headless portable fallback');
  for(const key of ['live','simulated','contacts','queryFailures','queuedImpacts',
    'deliveredImpacts','cosmeticDropped','snapshotBodies','snapshotColliders',
    'snapshotTriangles','gpuBytes','geometryUploads','stateTransferBytes',
    'simulationMs','snapshotMs'])assert(Number.isFinite(stats[key]),'missing '+key);
  assert(Array.isArray(e.takeParticleImpacts())&&e.takeParticleImpacts().length===0,'empty impact drain');
  assert(e.setParticles({collisionLayers:['Default']})&&e.particles.collisionLayers==='1','named mask');
  assert(e.setParticles({collisionLayers:null})&&e.particles.collisionLayers==='18446744073709551615','all mask');
  this.state.passed=true;this.state.rejectedPatches=10;this.state.rejectedBursts=7;
  assert(e.setParticles({backend:'gpu',collision:false,useWind:false,impacts:'off'}),'request unavailable GPU');
 }
 fixedUpdate(){
  if(this.state.requiredUnavailable)return;
  const stats=this.entity.particleStats;
  if(stats.requested!=='gpu'||stats.active!==null||stats.gpuBytes!==0||!stats.diagnostic)
   throw new Error('unavailable required GPU was reported as an active CPU backend');
  if(!this.entity.setParticles({backend:'auto',impacts:'reliable'}))throw new Error('restore reliable test settings');
  this.state.requiredUnavailable=true;
 }
})JS";
    }
    AssetDatabase assets;
    assets.Scan(root.string(), (root / "Assets").string());
    AssetRecord script;
    std::string error;
    Check(assets.Track(path.string(), script, error), "register actual JudasJS particle script");
    ResourceManager resources(nullptr, &assets);
    Scene scene;
    auto &object = scene.CreateObject("Authored emitter");
    object.particleEmitter = ParticleEmitterSettings{};
    object.scripts.push_back({1, script.id, true, "{}"});
    RuntimeWorld world;
    Check(world.Build(scene, &resources, error), "build hostless world with ordinary shared scripting runtime");
    InputSystem input;
    world.UpdateScripts(&input, .016f);
    world.UpdateVisualParticles(.016f);
    Check(world.VisualEmitters()[0].pool.Particles().empty() &&
              world.GetParticleRuntimeStats(object.id).diagnostic.find("required GPU") != std::string::npos,
          "hostless required GPU refuses the step without silently running CPU");
    world.FixedScripts(&input, .016f);
    const auto states = world.Scripts() ? world.Scripts()->Capture() : std::vector<ScriptStateRecord>{};
    if (world.Scripts())
        for (const auto &diagnostic : world.Scripts()->Diagnostics())
            std::printf("DIAGNOSTIC %s\n", diagnostic.message.c_str());
    Check(world.Scripts() && world.Scripts()->Diagnostics().empty(),
          "new bindings execute through their local module without script faults");
    Check(states.size() == 1 && states[0].json.find("\"passed\":true") != std::string::npos,
          "settings readback, transactional rejection, exact masks, controls and stats pass in VM");
    Check(states.size() == 1 && states[0].json.find("\"requiredUnavailable\":true") != std::string::npos,
          "real shared VM reports null active backend for unavailable required GPU");
    Check(world.RuntimeDefinition(object.id)->particleEmitter->collisionLayers == kAllCategories,
          "script-edited emitter definition retains exact mask for ordinary saves");
    ReliablePublication(world, object.id);
    world.Destroy();
    resources.Shutdown();
    fs::remove_all(root);
    std::printf("SUMMARY %d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
