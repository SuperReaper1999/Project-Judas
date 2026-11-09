#include "VehicleAssembly.h"
#include "SceneFingerprint.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <set>

namespace JudasVehicles {
bool CreateAssembly(Scene& scene,const AssemblyRecipe& recipe,AssemblyResult& result,std::string& error) {
    for(float value:{recipe.mass,recipe.track,recipe.axleSpacing,recipe.radius,recipe.width,recipe.travel,recipe.staticSag})
        if(!std::isfinite(value)||value<=0){error="Vehicle dimensions, mass, travel and sag must be finite and positive.";return false;}
    if(!recipe.axles||recipe.axles>16||recipe.staticSag>=recipe.travel||recipe.pose.scale!=glm::vec3(1)||
       (recipe.parent&&!scene.Find(recipe.parent))){error="Choose 1..16 axles, sag below travel, a unit-scale root and an existing parent.";return false;}
    std::set<unsigned> driven(recipe.drivenAxles.begin(),recipe.drivenAxles.end());
    if(!driven.empty()&&*driven.rbegin()>=recipe.axles){error="Driven axle index exceeds the authored layout.";return false;}
    Scene candidate=scene;
    AssemblyResult created;
    auto& root=candidate.CreateObject(recipe.name);
    created.root=root.id;root.parent=recipe.parent;root.transform=recipe.pose;
    root.body=SceneBodyComponent{};root.body->motion=SceneBodyMotion::Dynamic;
    root.body->mass=recipe.mass;root.body->managed=true;root.body->friction=.55f;root.body->restitution=0;
    root.body->halfExtents={recipe.track*.38f,.2f,std::max(.65f,recipe.axleSpacing*(recipe.axles-1)*.5f+.4f)};
    root.render=SceneRenderComponent{};root.render->halfExtents=root.body->halfExtents;root.render->color=recipe.color;
    const glm::vec3 cabinHalf{recipe.track*.29f,.35f,root.body->halfExtents.z*.45f};
    root.body->shape=SceneShape::Compound;
    root.body->compoundBoxes={CompoundBox{{0,0,0},root.body->halfExtents},CompoundBox{{0,.55f,0},cabinHalf}};
    auto& cabin=candidate.CreateObject("Cabin");cabin.parent=created.root;
    cabin.transform.position={0,.55f,0};cabin.render=SceneRenderComponent{};
    cabin.render->halfExtents=cabinHalf;cabin.render->color=recipe.color*.65f+glm::vec3(.1f);
    const float supportedMass=recipe.mass/(2*recipe.axles);
    const float stiffness=supportedMass*9.81f/recipe.staticSag;
    const float damping=1.8f*std::sqrt(stiffness*supportedMass);
    PowertrainSettings drive;
    for(unsigned axle=0;axle<recipe.axles;++axle)for(float side:{-1.f,1.f}) {
        auto& wheel=candidate.CreateObject("Axle "+std::to_string(axle+1)+(side<0?" left":" right"));
        wheel.parent=created.root;wheel.wheel=WheelSettings{};
        auto& settings=*wheel.wheel;settings.chassis=created.root;
        settings.mount={side*recipe.track*.5f,-.1f,recipe.axleSpacing*(float(axle)-float(recipe.axles-1)*.5f)};
        settings.radius=recipe.radius;settings.width=recipe.width;settings.restLength=recipe.travel;
        settings.minLength=recipe.travel*.2f;settings.maxLength=recipe.travel*1.75f;
        settings.springStiffness=stiffness;settings.damping=damping;
        settings.spinInertia=4*recipe.radius*recipe.radius;
        wheel.transform.position=settings.mount+glm::vec3(0,-recipe.travel,0);
        const auto wheelId=wheel.id;created.wheels.push_back(wheelId);
        if(recipe.wheelMesh.empty()) {
            wheel.render=SceneRenderComponent{};wheel.render->shape=SceneShape::Sphere;
            wheel.render->radius=recipe.radius;wheel.render->color={.07f,.08f,.09f};
        }else {
            auto& visual=candidate.CreateObject("Tread visual");visual.parent=wheelId;
            visual.render=SceneRenderComponent{};visual.render->shape=SceneShape::Mesh;
            visual.render->meshAsset=recipe.wheelMesh;visual.render->color={.07f,.08f,.09f};
            visual.transform.rotation=glm::angleAxis(glm::half_pi<float>(),glm::vec3(0,0,1));
            visual.transform.scale={2*recipe.radius,recipe.width*.5f,2*recipe.radius};
        }
        if(recipe.powered&&(driven.empty()||driven.count(axle))) {
            PowertrainShaft shaft;shaft.name="wheel_"+std::to_string(wheelId);
            shaft.kind=PowertrainShaftKind::Wheel;shaft.target=wheelId;
            PowertrainSource source;source.name="motor_"+std::to_string(wheelId);
            source.kind=PowertrainSourceKind::Electric;source.shaft=unsigned(drive.shafts.size());
            source.maxTorque=200;source.maxPower=10000;
            drive.shafts.push_back(shaft);drive.sources.push_back(source);
        }
    }
    if(recipe.powered)candidate.Find(created.root)->powertrain=std::move(drive);
    std::string fingerprint;
    if(!ComputeSceneFingerprint(candidate,fingerprint,error))return false;
    scene=std::move(candidate);result=std::move(created);error.clear();return true;
}
}
