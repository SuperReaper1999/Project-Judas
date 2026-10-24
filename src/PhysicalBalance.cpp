#include "PhysicalBalance.h"
#include "PhysicalBalanceMath.h"
#include "SaveArchive.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include "../third_party/nlohmann/json.hpp"


using namespace PhysicalBalanceMath;
namespace {
bool Finite(glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool Unit(glm::vec3 v){return Finite(v)&&std::abs(glm::length(v)-1)<1e-4f;}
bool Quaternion(glm::quat q){return Finite({q.x,q.y,q.z})&&std::isfinite(q.w)&&std::abs(glm::length(q)-1)<1e-4f;}
}
bool ValidBalanceDefinition(const BalanceDefinition& definition,std::string& error){
    const auto& settings=definition.settings;
    auto reject=[&](const char* why){error=why;return false;};
    if(!Unit(settings.up)||!Unit(settings.forward)||std::abs(glm::dot(settings.up,settings.forward))>1e-4f)return reject("balance authored root axes must be orthonormal");
    const float values[]={settings.stiffness,settings.damping,settings.maxTorque,settings.positionResponse,settings.velocityResponse,settings.maximumLean,settings.standingHeight,settings.standingTolerance,settings.standingSpeed,settings.settleSeconds,settings.attemptSeconds,settings.referenceSeconds,settings.gravityCoherence,settings.stepTrigger,settings.stepReach,settings.stepHeight,settings.stepSeconds,settings.touchdownTolerance,settings.transferLoadFraction,settings.transferRadiusFraction,settings.transferSeconds};
    for(float value:values)if(!std::isfinite(value)||value<=0)return reject("balance gains, dimensions and deadlines must be finite and positive");
    if(settings.stiffness>10000||settings.damping>1000||settings.maxTorque>10000||settings.maximumLean>1||settings.gravityCoherence>1||settings.attemptSeconds>120||settings.maximumSteps>8||settings.stepReach>3||settings.stepHeight>2)return reject("balance configuration exceeds bounded control envelope");
    for(const auto* keys:{&settings.getUpFaceUp,&settings.getUpFaceDown})for(const auto& key:*keys)if(!std::isfinite(key.seconds)||key.seconds<=0||key.seconds>5||!(std::abs(key.hip)<=180)||!(std::abs(key.knee)<=180)||!(std::abs(key.ankle)<=180))return reject("invalid bounded get-up keys");
    if(!std::isfinite(settings.getUpUpright)||settings.getUpUpright<=0||settings.getUpUpright>=1||!std::isfinite(settings.getUpPatience)||settings.getUpPatience<0||settings.getUpPatience>30||settings.getUpAttempts<1||settings.getUpAttempts>8)return reject("invalid bounded get-up assessment");
    if(settings.transferLoadFraction>1||settings.transferRadiusFraction>1||settings.transferSeconds>10)return reject("invalid bounded physical load transfer settings");
    if(settings.positionResponse>10||settings.velocityResponse>10||settings.standingHeight>10||settings.standingTolerance>2||settings.standingSpeed>10||settings.settleSeconds>30||settings.referenceSeconds>60||settings.stepSeconds>10||settings.touchdownTolerance>2)return reject("balance dimensions, response and history exceed bounded envelope");
    if(!std::isfinite(settings.legLeanFraction)||!std::isfinite(settings.trunkLeanFraction)||settings.legLeanFraction<0||settings.legLeanFraction>1||settings.trunkLeanFraction<0||settings.trunkLeanFraction>1||!std::isfinite(settings.plantedOrientationWeight)||settings.plantedOrientationWeight<0||settings.plantedOrientationWeight>1)return reject("invalid authored hip/ankle lean fractions");
    std::set<std::string> keys;
    for(unsigned f=0;f<2;++f){const auto& chain=definition.chains[f];for(const auto* key:{&chain.upper,&chain.lower,&chain.foot})if(key->empty()||key->size()>1024||!keys.insert(*key).second)return reject("balance chains need six unique stable skeleton keys");
        const auto& foot=settings.effectors[f];
        if(!Finite(foot.contactOffset)||!Finite(foot.halfExtents)||glm::any(glm::lessThanEqual(foot.halfExtents,glm::vec3(0)))||foot.patches.empty()||foot.patches.size()>8||!std::isfinite(foot.normalAcceptance)||foot.normalAcceptance<0||foot.normalAcceptance>1)return reject("balance needs bounded explicit foot contact geometry");
        for(const auto& patch:foot.patches)if(!Unit(patch.normal)||!Finite(patch.offset)||!std::isfinite(patch.radius)||patch.radius<=0||patch.radius>2)return reject("invalid balance contact patch");
    }
    if(!settings.query.ignoredBodies.empty())return reject("authored balance filters cannot contain runtime body handles");
    error.clear();return true;
}
namespace {
using BalanceJson=nlohmann::ordered_json;
BalanceJson VectorJson(glm::vec3 v){return {{"x",v.x},{"y",v.y},{"z",v.z}};}
glm::vec3 ReadVector(const BalanceJson& j){if(j.is_array()&&j.size()==3)return {j.at(0).get<float>(),j.at(1).get<float>(),j.at(2).get<float>()};return {j.at("x").get<float>(),j.at("y").get<float>(),j.at("z").get<float>()};}
}
std::string SerializeBalanceDefinition(const BalanceDefinition& definition){
    const auto& s=definition.settings;
    BalanceJson j={{"enabled",definition.enabled},{"visualize",definition.visualize},{"up",VectorJson(s.up)},{"forward",VectorJson(s.forward)},{"chains",BalanceJson::array()}};
    j["stiffness"]=s.stiffness;
    j["damping"]=s.damping;
    j["maxTorque"]=s.maxTorque;
    j["positionResponse"]=s.positionResponse;
    j["velocityResponse"]=s.velocityResponse;
    j["maximumLean"]=s.maximumLean;
    j["legLeanFraction"]=s.legLeanFraction;
    j["trunkLeanFraction"]=s.trunkLeanFraction;
    j["plantedOrientationWeight"]=s.plantedOrientationWeight;
    j["standingHeight"]=s.standingHeight;
    j["standingTolerance"]=s.standingTolerance;
    j["standingSpeed"]=s.standingSpeed;
    j["settleSeconds"]=s.settleSeconds;
    j["attemptSeconds"]=s.attemptSeconds;
    j["referenceSeconds"]=s.referenceSeconds;
    j["gravityCoherence"]=s.gravityCoherence;
    j["stepTrigger"]=s.stepTrigger;
    j["stepReach"]=s.stepReach;
    j["stepHeight"]=s.stepHeight;
    if(!s.captureStepping)j["captureStepping"]=false;
    j["stepSeconds"]=s.stepSeconds;
    j["touchdownTolerance"]=s.touchdownTolerance;
    j["transferLoadFraction"]=s.transferLoadFraction;
    j["transferRadiusFraction"]=s.transferRadiusFraction;
    j["transferSeconds"]=s.transferSeconds;
    j["maximumSteps"]=s.maximumSteps;
    {const BalanceSettings defaults;auto keysJson=[](const std::array<BalanceGetUpKey,3>& keys){BalanceJson a=BalanceJson::array();for(const auto& k:keys)a.push_back({k.seconds,k.hip,k.knee,k.ankle});return a;};
     auto same=[](const std::array<BalanceGetUpKey,3>& a,const std::array<BalanceGetUpKey,3>& b){for(size_t i=0;i<a.size();++i)if(a[i].seconds!=b[i].seconds||a[i].hip!=b[i].hip||a[i].knee!=b[i].knee||a[i].ankle!=b[i].ankle)return false;return true;};
     if(!same(s.getUpFaceUp,defaults.getUpFaceUp))j["getUpFaceUp"]=keysJson(s.getUpFaceUp);
     if(!same(s.getUpFaceDown,defaults.getUpFaceDown))j["getUpFaceDown"]=keysJson(s.getUpFaceDown);
     if(s.getUpUpright!=defaults.getUpUpright)j["getUpUpright"]=s.getUpUpright;
     if(s.getUpPatience!=defaults.getUpPatience)j["getUpPatience"]=s.getUpPatience;
     if(s.getUpAttempts!=defaults.getUpAttempts)j["getUpAttempts"]=s.getUpAttempts;}
    j["includeLayers"]=s.query.includeLayers;j["excludeLayers"]=s.query.excludeLayers;j["requiredTags"]=s.query.requiredTags;j["excludedTags"]=s.query.excludedTags;
    for(unsigned f=0;f<2;++f){const auto& c=definition.chains[f];const auto& e=s.effectors[f];BalanceJson v={{"upper",c.upper},{"lower",c.lower},{"foot",c.foot},{"contactOffset",VectorJson(e.contactOffset)},{"halfExtents",VectorJson(e.halfExtents)},{"normalAcceptance",e.normalAcceptance},{"patches",BalanceJson::array()}};for(const auto& p:e.patches)v["patches"].push_back({{"offset",VectorJson(p.offset)},{"normal",VectorJson(p.normal)},{"radius",p.radius}});j["chains"].push_back(std::move(v));}
    return j.dump();
}
bool ParseBalanceDefinition(const std::string& text,BalanceDefinition& output,std::string& error){
    try{if(text.size()>65536)throw std::runtime_error("balance configuration exceeds 64 KiB");auto j=BalanceJson::parse(text,[](int depth,BalanceJson::parse_event_t,const BalanceJson&){if(depth>8)throw std::runtime_error("balance configuration nesting bound");return true;});if(!j.is_object()||!j.contains("chains")||!j["chains"].is_array()||j["chains"].size()!=2)throw std::runtime_error("balance requires exactly two explicit chains");BalanceDefinition d;auto& s=d.settings;d.enabled=j.value("enabled",true);d.visualize=j.value("visualize",false);if(j.contains("up"))s.up=ReadVector(j.at("up"));if(j.contains("forward"))s.forward=ReadVector(j.at("forward"));
        s.stiffness=j.value("stiffness",s.stiffness);
        s.damping=j.value("damping",s.damping);
        s.maxTorque=j.value("maxTorque",s.maxTorque);
        s.positionResponse=j.value("positionResponse",s.positionResponse);
        s.velocityResponse=j.value("velocityResponse",s.velocityResponse);
        s.maximumLean=j.value("maximumLean",s.maximumLean);
        s.legLeanFraction=j.value("legLeanFraction",s.legLeanFraction);
        s.trunkLeanFraction=j.value("trunkLeanFraction",s.trunkLeanFraction);
        s.plantedOrientationWeight=j.value("plantedOrientationWeight",s.plantedOrientationWeight);
        s.standingHeight=j.value("standingHeight",s.standingHeight);
        s.captureStepping=j.value("captureStepping",s.captureStepping);
        for(auto [name,keys]:{std::pair{"getUpFaceUp",&s.getUpFaceUp},std::pair{"getUpFaceDown",&s.getUpFaceDown}})if(j.contains(name)){const auto& a=j.at(name);if(!a.is_array()||a.size()!=keys->size())throw std::runtime_error("get-up keys require three [seconds,hip,knee,ankle] entries");for(size_t i=0;i<keys->size();++i){const auto& k=a.at(i);if(!k.is_array()||k.size()!=4)throw std::runtime_error("get-up key requires [seconds,hip,knee,ankle]");(*keys)[i]={k.at(0).get<float>(),k.at(1).get<float>(),k.at(2).get<float>(),k.at(3).get<float>()};}}
        s.getUpUpright=j.value("getUpUpright",s.getUpUpright);
        s.getUpPatience=j.value("getUpPatience",s.getUpPatience);
        s.getUpAttempts=j.value("getUpAttempts",s.getUpAttempts);
        s.standingTolerance=j.value("standingTolerance",s.standingTolerance);
        s.standingSpeed=j.value("standingSpeed",s.standingSpeed);
        s.settleSeconds=j.value("settleSeconds",s.settleSeconds);
        s.attemptSeconds=j.value("attemptSeconds",s.attemptSeconds);
        s.referenceSeconds=j.value("referenceSeconds",s.referenceSeconds);
        s.gravityCoherence=j.value("gravityCoherence",s.gravityCoherence);
        s.stepTrigger=j.value("stepTrigger",s.stepTrigger);
        s.stepReach=j.value("stepReach",s.stepReach);
        s.stepHeight=j.value("stepHeight",s.stepHeight);
        s.stepSeconds=j.value("stepSeconds",s.stepSeconds);
        s.touchdownTolerance=j.value("touchdownTolerance",s.touchdownTolerance);
        s.transferLoadFraction=j.value("transferLoadFraction",s.transferLoadFraction);
        s.transferRadiusFraction=j.value("transferRadiusFraction",s.transferRadiusFraction);
        s.transferSeconds=j.value("transferSeconds",s.transferSeconds);
        s.maximumSteps=j.value("maximumSteps",s.maximumSteps);
        s.query.includeLayers=j.value("includeLayers",s.query.includeLayers);s.query.excludeLayers=j.value("excludeLayers",s.query.excludeLayers);s.query.requiredTags=j.value("requiredTags",s.query.requiredTags);s.query.excludedTags=j.value("excludedTags",s.query.excludedTags);
        for(unsigned f=0;f<2;++f){const auto& v=j.at("chains").at(f);auto& c=d.chains[f];auto& e=s.effectors[f];c.upper=v.at("upper").get<std::string>();c.lower=v.at("lower").get<std::string>();c.foot=v.at("foot").get<std::string>();e.contactOffset=ReadVector(v.at("contactOffset"));e.halfExtents=ReadVector(v.at("halfExtents"));e.normalAcceptance=v.value("normalAcceptance",e.normalAcceptance);if(!v.contains("patches")||!v.at("patches").is_array()||v.at("patches").size()>8)throw std::runtime_error("balance contact patch count bound");for(const auto& p:v.at("patches"))e.patches.push_back({ReadVector(p.at("offset")),ReadVector(p.at("normal")),p.at("radius").get<float>()});}
        if(!ValidBalanceDefinition(d,error))return false;
        output=std::move(d);error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool PhysicalBalanceController::Configure(PhysicsWorld& world,const std::vector<BalanceBody>& bodies,const BalanceSettings& settings,std::string& error){
    auto reject=[&](const char* why){error=why;return false;};
    if(m_history.progress==BalanceProgress::Executing||m_history.progress==BalanceProgress::Succeeded)return reject("cancel physical task before replacing its configuration");
    if(bodies.size()<7||bodies.size()>32||bodies[0].parent!=-1)return reject("balance needs a parent-first dynamic articulation of 7..32 bodies");
    BalanceDefinition authored;authored.settings=settings;authored.settings.query.ignoredBodies.clear();authored.chains={BalanceChainKeys{"a","b","c"},BalanceChainKeys{"d","e","f"}};
    if(!ValidBalanceDefinition(authored,error))return false;
    for(size_t i=0;i<bodies.size();++i){const auto& b=bodies[i];
        if(!world.IsDynamicBody(b.body)||!world.IsBodyEnabled(b.body)||world.IsBodySensor(b.body)||!(world.GetMass(b.body)>0)||!Quaternion(b.restRotation))return reject("balance requires enabled nonsensor dynamic bodies and normalized rest orientations");
        for(size_t k=0;k<i;++k)if(b.body.id==bodies[k].body.id)return reject("balance mass/body mapping must be unique");
        if(i){JointState joint;if(b.parent<0||size_t(b.parent)>=i||!world.GetJoint(b.joint,joint)||joint.settings.bodyA.id!=b.body.id||joint.settings.bodyB.id!=bodies[b.parent].body.id||!joint.settings.enabled||joint.settings.motor||joint.settings.spring||(joint.settings.type!=JointType::Ball&&joint.settings.type!=JointType::Hinge)||joint.angularDrive.enabled)return reject("balance joint ownership/type/parent conflict");}
    }
    const auto& a=settings.effectors[0];const auto& b=settings.effectors[1];
    if(a.foot==b.foot||a.upper==b.upper||a.lower==b.lower)return reject("balance requires two independent support chains");
    for(const auto& foot:settings.effectors){
        if(foot.upper>=bodies.size()||foot.lower>=bodies.size()||foot.foot>=bodies.size()||bodies[foot.upper].parent!=0||bodies[foot.lower].parent!=int(foot.upper)||bodies[foot.foot].parent!=int(foot.lower)||!Finite(foot.contactOffset)||!Finite(foot.halfExtents)||glm::any(glm::lessThanEqual(foot.halfExtents,glm::vec3(0)))||foot.patches.empty()||foot.patches.size()>8||!std::isfinite(foot.normalAcceptance)||foot.normalAcceptance<0||foot.normalAcceptance>1)return reject("balance requires explicit root/upper/lower/foot chains and contact patches");
        for(const auto& patch:foot.patches)if(!Unit(patch.normal)||!Finite(patch.offset)||!std::isfinite(patch.radius)||patch.radius<=0||patch.radius>2)return reject("invalid authored balance contact patch");
        JointState hip,knee,ankle;world.GetJoint(bodies[foot.upper].joint,hip);world.GetJoint(bodies[foot.lower].joint,knee);world.GetJoint(bodies[foot.foot].joint,ankle);
        auto upper=knee.settings.anchorB-hip.settings.anchorA,lower=ankle.settings.anchorB-knee.settings.anchorA;
        if(glm::length(upper)<.01f||glm::length(lower)<.01f)return reject("balance needs nonzero authored physical limb segments");
        if(knee.settings.type==JointType::Hinge&&(glm::length(glm::cross(upper,knee.settings.frameB*glm::vec3(1,0,0)))<.001f||glm::length(glm::cross(lower,knee.settings.frameA*glm::vec3(1,0,0)))<.001f))return reject("balance hinge axes cannot be parallel to limb segments");
    }
    m_bodies=bodies;m_settings=settings;if(const char* capture=std::getenv("JUDAS_BALANCE_CAPTURE_STEPPING"))m_settings.captureStepping=std::string(capture)!="0";for(const auto& body:bodies)m_settings.query.ignoredBodies.push_back(body.body);
    m_contacts.reserve(32);m_desired.resize(bodies.size());m_history={};m_observation={};m_waitingForSample=false;m_captureStanceReady=false;error.clear();return true;
}
bool PhysicalBalanceController::Request(PhysicsWorld& world,BalanceTask task,bool steps,bool effort,std::string& error){
    if(m_bodies.empty()||(task!=BalanceTask::Stand&&task!=BalanceTask::Recover)){error="balance task needs configured articulation and stand/recover intent";return false;}
    std::vector<glm::quat> initial(m_bodies.size(),glm::quat(1,0,0,0));
    for(size_t i=0;i<m_bodies.size();++i){if(!world.IsDynamicBody(m_bodies[i].body)||!world.IsBodyEnabled(m_bodies[i].body)){error="balance mapped body no longer available";return false;}if(i){JointState state;if(!world.GetJoint(m_bodies[i].joint,state)){error="balance mapped joint no longer available";return false;}initial[i]=glm::inverse(world.GetTransform(m_bodies[i].body).rotation)*world.GetTransform(m_bodies[m_bodies[i].parent].body).rotation;}}
    m_waitingForSample=false;m_captureStanceReady=false;m_getUp=false;m_getUpAttempts=0;m_getUpReached=false;m_history={};m_history.task=task;m_history.progress=BalanceProgress::Executing;m_history.stepping=steps;m_history.effort=effort;m_history.initialTargets=std::move(initial);m_observation={};error.clear();return true;
}
void PhysicalBalanceController::Release(PhysicsWorld& world){JointAngularDrive disabled;for(size_t i=1;i<m_bodies.size();++i)world.SetJointAngularDrive(m_bodies[i].joint,disabled);}
void PhysicalBalanceController::Cancel(PhysicsWorld& world){Release(world);m_getUp=false;m_history.progress=BalanceProgress::Cancelled;m_history.task=BalanceTask::None;m_history.swing=-1;m_history.reason="cancelled";m_observation={};}
void PhysicalBalanceController::ShiftOrigin(glm::dvec3 delta){
    auto shift=[&](glm::vec3& p){p=glm::vec3(glm::dvec3(p)-delta);};
    shift(m_observation.com);
    for(auto& support:m_observation.supports)shift(support.point);
    for(auto& contact:m_contacts)shift(contact.point);
    // Planned footholds and stance/root references are support-body-local.
}
void PhysicalBalanceController::Fail(PhysicsWorld& world,const char* why){Release(world);m_history.progress=BalanceProgress::Failed;m_history.swing=-1;m_history.lastSwing=-1;m_history.frameSupport={};m_history.plannedSupport={};m_history.stanceSupport={};m_history.reason=why;}
bool PhysicalBalanceController::Observe(PhysicsWorld& world,const std::vector<glm::vec3>& gravity){
    JUDAS_PROFILE_SCOPE("Physical balance observations");
    m_observation={};if(gravity.size()!=m_bodies.size())return false;
    float mass=0;for(size_t i=0;i<m_bodies.size();++i){auto body=m_bodies[i].body;if(!world.IsDynamicBody(body)||!world.IsBodyEnabled(body)||!Finite(gravity[i]))return false;float m=world.GetMass(body);mass+=m;m_observation.com+=world.GetCenterOfMass(body)*m;m_observation.velocity+=world.GetLinearVelocity(body)*m;m_observation.gravity+=gravity[i]*m;}
    if(!(mass>0))return false;
    m_observation.com/=mass;m_observation.velocity/=mass;m_observation.gravity/=mass;
    m_observation.mass=mass;
    float magnitude=glm::length(m_observation.gravity);if(magnitude<.05f)return false;
    m_observation.up=-m_observation.gravity/magnitude;
    for(auto acceleration:gravity)if(glm::length(acceleration)<.05f||glm::dot(glm::normalize(-acceleration),m_observation.up)<m_settings.gravityCoherence)return false;
    for(unsigned i=0;i<2;++i){const auto& effector=m_settings.effectors[i];auto transform=world.GetTransform(m_bodies[effector.foot].body);auto& support=m_observation.supports[i];
        if(!world.ReadBodyContactSample(m_bodies[effector.foot].body,m_contacts))return false;
        for(const auto& contact:m_contacts){if(std::any_of(m_bodies.begin(),m_bodies.end(),[&](const auto& body){return body.body.id==contact.other.id;}))continue;
            const auto localPoint=Local(transform,contact.point),localNormal=glm::inverse(transform.rotation)*contact.normal;
            bool accepted=std::any_of(effector.patches.begin(),effector.patches.end(),[&](const auto& patch){return glm::length(localPoint-patch.offset)<=patch.radius&&glm::dot(localNormal,-patch.normal)>.65f;});
            if(contact.separation>.015f||contact.normalImpulse<=0||glm::dot(contact.normal,m_observation.up)<effector.normalAcceptance||!accepted)continue;
            // One effector straddling independently moving bodies is not a
            // coherent planted reference. Refuse rather than average identities.
            if(support.contacts&&support.body.id!=contact.other.id)return false;
            support.body=contact.other;support.point+=contact.point*contact.normalImpulse;support.normal+=contact.normal*contact.normalImpulse;support.velocity+=contact.supportVelocity*contact.normalImpulse;support.load+=contact.normalImpulse;support.centre+=contact.point;++support.contacts;
        }
        if(support.contacts)support.centre/=float(support.contacts);
        if(support.load>0){support.point/=support.load;support.velocity/=support.load;if(glm::dot(support.normal,support.normal)>1e-8f)support.normal=glm::normalize(support.normal);}
    }
    auto root=world.GetTransform(m_bodies[0].body);m_observation.upright=glm::dot(root.rotation*m_settings.up,m_observation.up);
    glm::vec3 centre(0),velocity(0);unsigned count=0;for(const auto& s:m_observation.supports)if(s.contacts){centre+=s.point;velocity+=s.velocity;++count;}
    if(count){centre/=float(count);velocity/=float(count);m_observation.height=glm::dot(root.position-centre,m_observation.up);m_observation.speed=glm::length(m_observation.velocity-velocity);}
    else m_observation.speed=glm::length(m_observation.velocity);
    m_observation.available=true;return true;
}
bool PhysicalBalanceController::PlanStep(PhysicsWorld& world,const glm::vec3& capture,const glm::quat& upright){
    JUDAS_PROFILE_SCOPE("Physical balance foothold planning");
    if(m_history.landings>=m_settings.maximumSteps)return false;
    // Treat sub-per-mille differences in summed solver impulses as a tie.
    // Near-balanced contacts vary by a few float rounding units across compiler
    // paths; those must not flip an otherwise symmetric discrete step choice.
    // Meaningfully lighter support still wins, with authored chain order for ties.
    const float load0=m_observation.supports[0].load,load1=m_observation.supports[1].load;
    const float tie=.001f*(load0+load1);
    int foot=load1+tie<load0?1:0;
    if(!m_observation.supports[1-foot].contacts)return false;
    const auto& effector=m_settings.effectors[foot];auto pose=world.GetTransform(m_bodies[effector.foot].body);
    // Capture correction translates the existing support pattern. Sending an
    // individual foot to the whole-rig COM would erase authored stance width
    // and command it through the other leg.
    auto supportCentre=(m_observation.supports[0].point+m_observation.supports[1].point)*.5f;
    auto displacement=Tangent(capture-supportCentre,m_observation.up);
    if(glm::length(displacement)>m_settings.stepReach)displacement*=m_settings.stepReach/glm::length(displacement);
    const auto origin=World(pose,effector.contactOffset)+displacement+m_observation.up*m_settings.stepHeight;
    auto hit=world.Raycast(origin,-m_observation.up,2*m_settings.stepHeight+.15f,m_settings.query);
    if(!hit.hit||glm::dot(hit.normal,m_observation.up)<effector.normalAcceptance)return false;
    // Translational conservative foot envelope; rotation remains the normal
    // body's collision/CCD responsibility. A clear ray alone is not clearance.
    auto destination=hit.point-upright*effector.contactOffset;
    auto travel=destination-pose.position;float length=glm::length(travel);
    auto overhead=pose;overhead.position+=m_observation.up*m_settings.stepHeight;
    overhead.rotation=upright*m_bodies[effector.foot].restRotation;
    if(world.BoxCast(overhead,effector.halfExtents,travel,std::max(length,.001f),m_settings.query).hit)return false;
    auto platform=world.GetTransform(hit.body);m_history.swing=foot;m_history.plannedSupport=hit.body;
    m_history.startLocal=Local(platform,pose.position);m_history.landingLocal=Local(platform,destination);
    m_history.stanceSupport=m_observation.supports[1-foot].body;
    auto stance=world.GetTransform(m_history.stanceSupport);
    m_history.stanceLocal=Local(stance,world.GetTransform(m_bodies[m_settings.effectors[1-foot].foot].body).position);
    m_history.rootStartLocal=Local(stance,world.GetTransform(m_bodies[0].body).position);
    m_history.comStartLocal=Local(stance,m_observation.com);m_history.swingTime=-1;m_history.transferTime=0;m_history.lifted=false;return true;
}
bool PhysicalBalanceController::SwingReferences(PhysicsWorld& world,const glm::quat& upright,std::vector<glm::quat>& desired){
    JUDAS_PROFILE_SCOPE("Physical balance step references");
    const int swingIndex=m_history.swing>=0?m_history.swing:m_history.lastSwing;
    if(swingIndex<0)return true;
    const bool planted=m_history.swing<0;
    if(!world.IsBodyEnabled(m_history.stanceSupport)||!world.IsBodyEnabled(m_history.plannedSupport))return false;
    auto support=world.GetTransform(m_history.plannedSupport),stance=world.GetTransform(m_history.stanceSupport);
    const auto& swing=m_settings.effectors[swingIndex];
    float t=std::clamp(m_history.swingTime/m_settings.stepSeconds,0.f,1.f),blend=t*t*(3-2*t);
    auto target=glm::mix(World(support,m_history.startLocal),World(support,m_history.landingLocal),blend)+m_observation.up*(m_settings.stepHeight*std::sin(3.1415926536f*blend));
    std::array<glm::vec3,2> footTargets;
    footTargets[swingIndex]=planted?World(support,m_history.landedLocal):target;footTargets[1-swingIndex]=World(stance,m_history.stanceLocal);
    auto rootReference=world.GetTransform(m_bodies[0].body);rootReference.rotation=upright;
    auto initialRoot=World(stance,m_history.rootStartLocal),initialCOM=World(stance,m_history.comStartLocal);
    float transfer=std::clamp(m_history.transferTime/m_settings.stepSeconds,0.f,1.f);transfer=transfer*transfer*(3-2*transfer);
    rootReference.position=initialRoot+Tangent(footTargets[1-swingIndex]-initialCOM,m_observation.up)*transfer;
    rootReference.position+=Tangent(World(support,m_history.landingLocal)-World(support,m_history.startLocal),m_observation.up)*(.5f*blend);
    if(planted){
        auto midpoint=(footTargets[0]+footTargets[1])*.5f;
        auto initialMidpoint=(World(support,m_history.startLocal)+World(stance,m_history.stanceLocal))*.5f;
        rootReference.position=midpoint+m_observation.up*(m_settings.standingHeight+glm::dot(upright*swing.contactOffset,m_observation.up))+Tangent(initialRoot-initialMidpoint,m_observation.up);
    }
    // This is an IK REFERENCE only. A crouched reference provides the real
    // chain reach needed to transfer load and move a foot. It is never assigned
    // to any physical body or owner transform, and is not achieved support.
    for(unsigned f=0;f<2;++f){const auto& e=m_settings.effectors[f];JointState hip,knee,ankle;
        if(!world.GetJoint(m_bodies[e.upper].joint,hip)||!world.GetJoint(m_bodies[e.lower].joint,knee)||!world.GetJoint(m_bodies[e.foot].joint,ankle))return false;
        auto end=footTargets[f]+upright*ankle.settings.anchorA;
        auto hipOffset=upright*hip.settings.anchorB;
        float reach=(glm::length(knee.settings.anchorB-hip.settings.anchorA)+glm::length(ankle.settings.anchorB-knee.settings.anchorA))*(.9999f-.0299f*transfer);
        float horizontal=glm::length(Tangent(end-rootReference.position-hipOffset,m_observation.up));
        if(horizontal>=reach)return false;
        float allowed=glm::dot(end-hipOffset,m_observation.up)+std::sqrt(reach*reach-horizontal*horizontal);
        float excess=glm::dot(rootReference.position,m_observation.up)-allowed;
        if(excess>0)rootReference.position-=m_observation.up*excess;
    }
    desired[0]=upright*m_bodies[0].restRotation;
    for(unsigned f=0;f<2;++f){const auto& e=m_settings.effectors[f];JointState hip,knee,ankle;
        world.GetJoint(m_bodies[e.upper].joint,hip);world.GetJoint(m_bodies[e.lower].joint,knee);world.GetJoint(m_bodies[e.foot].joint,ankle);
        auto start=World(rootReference,hip.settings.anchorB),end=footTargets[f]+upright*ankle.settings.anchorA;
        auto upperAxis=knee.settings.anchorB-hip.settings.anchorA,lowerAxis=ankle.settings.anchorB-knee.settings.anchorA;
        float a=glm::length(upperAxis),b=glm::length(lowerAxis),distance=glm::length(end-start);
        if(a<.01f||b<.01f||distance>a+b+.01f||distance<std::abs(a-b)+.001f)return false;
        distance=std::min(distance,a+b-.001f);auto direction=glm::normalize(end-start);
        auto bend=Tangent(upright*m_settings.forward,direction);if(glm::dot(bend,bend)<1e-6f)return false;bend=glm::normalize(bend);
        float along=(a*a-b*b+distance*distance)/(2*distance),height=std::sqrt(std::max(a*a-along*along,0.f));auto elbow=start+direction*along+bend*height;
        auto planeAxis=glm::normalize(glm::cross(bend,direction));
        if(knee.settings.type==JointType::Hinge){
            desired[e.upper]=LinkFrame(upperAxis,knee.settings.frameB*glm::vec3(1,0,0),elbow-start,planeAxis);
            desired[e.lower]=LinkFrame(lowerAxis,knee.settings.frameA*glm::vec3(1,0,0),end-elbow,planeAxis);
        }else{
            auto referenceUpper=upright*m_bodies[e.upper].restRotation,referenceLower=upright*m_bodies[e.lower].restRotation;
            desired[e.upper]=Align(referenceUpper*upperAxis,elbow-start,upright*m_settings.forward)*referenceUpper;
            desired[e.lower]=Align(referenceLower*lowerAxis,end-elbow,upright*m_settings.forward)*referenceLower;
        }
        desired[e.foot]=upright*m_bodies[e.foot].restRotation;
    }
    if(planted)return true;
    const auto& effector=swing;
    auto footPose=world.GetTransform(m_bodies[effector.foot].body);auto actualContact=World(footPose,effector.contactOffset);
    if(t>.1f&&!m_observation.supports[m_history.swing].contacts&&glm::dot(footPose.position-World(support,m_history.startLocal),m_observation.up)>.025f)m_history.lifted=true;
    if(t>=1&&m_history.lifted&&m_observation.supports[m_history.swing].contacts&&m_observation.supports[m_history.swing].body.id==m_history.plannedSupport.id&&glm::length(actualContact-World(support,m_history.landingLocal)-upright*effector.contactOffset)<m_settings.touchdownTolerance){m_history.lastSwing=m_history.swing;m_history.landedLocal=Local(support,footPose.position);m_history.swing=-1;++m_history.landings;return true;}
    return m_history.swingTime<m_settings.stepSeconds*3;
}
void PhysicalBalanceController::Prepare(PhysicsWorld& world,const std::vector<glm::vec3>& gravity,float dt){
    if(m_history.progress!=BalanceProgress::Executing&&m_history.progress!=BalanceProgress::Succeeded)return;
    if(!(dt>0)||dt>.1f||!std::isfinite(dt)){Fail(world,"invalid physical control interval");return;}
    if(m_waitingForSample){
        m_waitingForSample=false;m_observation={};
        // A full world snapshot can restore the actual completed publication.
        // A controller-History-only restore still waits for a real world step.
        if(!world.HasRestoredCompletedContactObservations(dt))return;
    }
    if(!Observe(world,gravity)){Fail(world,"unavailable/stale contacts, bodies or coherent nonzero gravity");return;}
    if(m_history.progress==BalanceProgress::Executing)m_history.elapsed+=dt;
    m_history.referenceTime=std::min(m_history.referenceTime+dt,m_settings.referenceSeconds);
    if(!m_history.postStepTargets.empty())m_history.postStepTime=std::min(m_history.postStepTime+dt,m_settings.stepSeconds);
    const unsigned priorLandings=m_history.landings;
    auto root=world.GetTransform(m_bodies[0].body);auto up=m_observation.up;
    if(GetUp(world,up,dt)){if(m_history.progress==BalanceProgress::Executing&&m_history.elapsed>m_settings.attemptSeconds)Fail(world,"physical standing deadline exceeded");return;}
    if(!m_history.frameReady){m_history.rootReference=Align(root.rotation*m_settings.up,up,root.rotation*m_settings.forward)*root.rotation;m_history.frameReady=true;}
    BodyHandle carrier;for(const auto& support:m_observation.supports)if(support.contacts){carrier=support.body;break;}
    if(carrier.id!=m_history.frameSupport.id||carrier.world!=m_history.frameSupport.world){m_history.frameSupport=carrier;if(carrier.IsValid())m_history.supportReference=glm::inverse(world.GetTransform(carrier).rotation)*m_history.rootReference;}
    auto frame=carrier.IsValid()?world.GetTransform(carrier).rotation*m_history.supportReference:m_history.rootReference;
    auto upright=Align(frame*m_settings.up,up,frame*m_settings.forward)*frame;
    m_history.rootReference=upright;
    glm::vec3 supportPoint(0),supportVelocity(0);unsigned count=0;
    for(unsigned f=0;f<2;++f){const auto& s=m_observation.supports[f];if(s.contacts){supportPoint+=s.point;supportVelocity+=s.velocity;++count;}}
    if(count){supportPoint/=float(count);supportVelocity/=float(count);}
    if(m_history.swing>=0&&m_observation.supports[1-m_history.swing].contacts){const auto& stance=m_observation.supports[1-m_history.swing];supportPoint=stance.point;supportVelocity=stance.velocity;}
    auto error=count?Tangent(m_observation.com-supportPoint,up):glm::vec3(0);
    auto velocity=Tangent(m_observation.velocity-supportVelocity,up);
    if(m_settings.captureStepping&&m_history.hasStood){if(!CaptureStep(world,upright,dt))return;}
    else{
        auto tilt=glm::cross(up,-(error*m_settings.positionResponse+velocity*m_settings.velocityResponse));float lean=glm::length(tilt);
        auto correction=lean>1e-5f?glm::angleAxis(std::min(lean,m_settings.maximumLean),tilt/lean):glm::quat(1,0,0,0);
        auto legCorrection=glm::slerp(glm::quat(1,0,0,0),correction,m_settings.legLeanFraction);
        auto& desired=m_desired;for(size_t i=0;i<m_bodies.size();++i)desired[i]=legCorrection*upright*m_bodies[i].restRotation;
        // Separate trunk attitude from leg lean. Making every desired body share
        // the same lean cancels the hip correction entirely and leaves a one-leg
        // stance acting as an uncontrolled rigid inverted pendulum.
        desired[0]=glm::slerp(glm::quat(1,0,0,0),correction,m_settings.trunkLeanFraction)*upright*m_bodies[0].restRotation;
        if(m_history.stepping&&m_history.hasStood&&(m_history.postStepTargets.empty()||m_history.postStepTime>=m_settings.stepSeconds)&&m_history.swing<0&&count==2&&m_observation.upright>.8f&&std::abs(m_observation.height-m_settings.standingHeight)<m_settings.standingTolerance){float height=std::max(glm::dot(m_observation.com-supportPoint,up),.1f);auto capture=m_observation.com+velocity*std::sqrt(height/glm::length(m_observation.gravity));if((glm::length(velocity)>m_settings.standingSpeed||m_observation.upright<.96f)&&glm::length(Tangent(capture-supportPoint,up))>m_settings.stepTrigger&&!PlanStep(world,capture,upright))m_history.reason="no clear reachable corrective foothold";}
        if(m_history.swing>=0){
            if(m_history.swingTime<0){m_history.transferTime+=dt;const auto& stance=m_observation.supports[1-m_history.swing];
                const auto& effector=m_settings.effectors[1-m_history.swing];float radius=0;for(const auto& patch:effector.patches)radius=std::max(radius,patch.radius);
                const float remainingMass=m_observation.mass-world.GetMass(m_bodies[m_settings.effectors[m_history.swing].foot].body);
                if(stance.contacts&&glm::length(Tangent(m_observation.com-stance.point,up))<radius*m_settings.transferRadiusFraction&&stance.load/dt>remainingMass*glm::length(m_observation.gravity)*m_settings.transferLoadFraction)m_history.swingTime=0;
                else if(m_history.transferTime>m_settings.transferSeconds){Fail(world,"insufficient physical load transfer for corrective step");return;}
            }else m_history.swingTime+=dt;
            if(!SwingReferences(world,upright,desired)){Fail(world,"corrective step lost support, reach or physical touchdown");return;}
        }
        if(m_history.swing<0&&m_history.lastSwing>=0&&!SwingReferences(world,upright,desired)){Fail(world,"changed support no longer supports planted stance reference");return;}
        if(m_history.lastSwing>=0||m_history.swing>=0)for(auto& pose:desired)pose=correction*pose;
        const float ramp=std::clamp(m_history.referenceTime/m_settings.referenceSeconds,0.f,1.f);
        const bool landed=m_history.landings!=priorLandings;
        if(landed){m_history.postStepTargets.assign(m_bodies.size(),glm::quat(1,0,0,0));m_history.postStepTime=0;}
        for(size_t i=1;i<m_bodies.size();++i){JointState state;if(!world.GetJoint(m_bodies[i].joint,state)||!state.settings.enabled||state.settings.motor||state.settings.spring){Fail(world,"joint ownership changed");return;}
            JointAngularDrive drive;drive.enabled=m_history.effort;drive.stiffness=m_settings.stiffness;drive.damping=m_settings.damping;drive.maxTorque=m_settings.maxTorque;
            drive.target=glm::inverse(desired[i])*desired[m_bodies[i].parent];
            for(unsigned f=0;f<2;++f)if(i==m_settings.effectors[f].foot&&(m_history.swing!=int(f)||m_history.swingTime<0)&&m_observation.supports[f].contacts){
                auto foot=world.GetTransform(m_bodies[i].body);
                auto orientation=glm::slerp(foot.rotation,desired[i],m_settings.plantedOrientationWeight);
                drive.target=glm::inverse(orientation)*desired[m_bodies[i].parent];
                // Compensate prescribed support rotation, not a rolling foot's
                // observed angular velocity. Cancelling the latter removes relative
                // damping precisely when a planted reference is being lost.
                drive.relativeAngularVelocity=-world.GetAngularVelocity(m_observation.supports[f].body);
            }
            drive.target=glm::slerp(m_history.initialTargets[i],drive.target,ramp);
            if(landed)m_history.postStepTargets[i]=drive.target;
            else if(m_history.swing<0&&!m_history.postStepTargets.empty()&&m_history.postStepTime<m_settings.stepSeconds){float blend=m_history.postStepTime/m_settings.stepSeconds;blend=blend*blend*(3-2*blend);drive.target=glm::slerp(m_history.postStepTargets[i],drive.target,blend);}
            if(!world.SetJointAngularDrive(m_bodies[i].joint,drive)){Fail(world,"bounded joint drive rejected");return;}
            if(glm::length(state.angularDrive.impulses)/dt>=m_settings.maxTorque*.98f)++m_observation.saturated;
        }
    }
    bool standing=count==2&&m_history.swing<0&&m_observation.upright>.96f&&std::abs(m_observation.height-m_settings.standingHeight)<m_settings.standingTolerance&&m_observation.speed<m_settings.standingSpeed;
    m_history.settled=standing?std::min(m_history.settled+dt,m_settings.settleSeconds):0;
    if(m_history.settled>=m_settings.settleSeconds){m_history.progress=BalanceProgress::Succeeded;m_history.hasStood=true;m_history.reason="measured supported physical standing";}
    // A success is a measured event, not perpetual invulnerability. Continue
    // observing/holding and withdraw the current success when disturbed.
    if(m_history.progress==BalanceProgress::Succeeded&&!standing){m_history.progress=BalanceProgress::Executing;m_history.elapsed=0;}
    if(m_history.progress==BalanceProgress::Executing&&m_history.elapsed>m_settings.attemptSeconds)Fail(world,"physical standing deadline exceeded");
}

void PhysicalBalanceController::PersistHistory(SaveArchive& a,const std::function<uint64_t(BodyHandle)>& toId,const std::function<BodyHandle(uint64_t)>& fromId){
    auto& h=m_history;
    a(h.task,h.progress,h.stepping,h.effort,h.hasStood,h.elapsed,h.settled,h.referenceTime,h.frameReady,h.rootReference,h.supportReference);
    auto targets=[&](std::vector<glm::quat>& values){uint32_t size=uint32_t(values.size());a(size);a.Require(size<=32,"saved balance reference exceeds joint bound");if(a.reading)values.resize(size);for(auto& q:values)a(q);};
    targets(h.initialTargets);targets(h.postStepTargets);
    a(h.postStepTime,h.swing,h.lastSwing,h.startLocal,h.landingLocal,h.landedLocal,h.stanceLocal,h.rootStartLocal,h.comStartLocal,h.swingTime,h.transferTime,h.lifted,h.landings,h.reason);
    auto reference=[&](BodyHandle& handle){uint64_t id=a.reading?0:toId(handle);a.Require(a.reading||!handle.IsValid()||id!=0,"balance reference lacks a durable entity identity");a(id);if(a.reading){handle=id?fromId(id):BodyHandle{};a.Require(!id||handle.IsValid(),"saved balance support is unavailable");}};
    reference(h.frameSupport);reference(h.plannedSupport);reference(h.stanceSupport);
    a.Require(int(h.task)>=0&&int(h.task)<=2&&int(h.progress)>=0&&int(h.progress)<=4&&h.elapsed>=0&&h.elapsed<=m_settings.attemptSeconds+.1f&&h.settled>=0&&h.settled<=m_settings.settleSeconds+.001f&&h.referenceTime>=0&&h.referenceTime<=m_settings.referenceSeconds+.001f&&h.postStepTime>=0&&h.postStepTime<=m_settings.stepSeconds+.001f&&h.swing>=-1&&h.swing<2&&h.lastSwing>=-1&&h.lastSwing<2&&h.transferTime>=0&&h.transferTime<=m_settings.transferSeconds+.1f&&h.swingTime>=-1&&h.swingTime<=m_settings.stepSeconds*3+.1f&&h.landings<=m_settings.maximumSteps&&h.reason.size()<=256,"invalid saved balance task clocks/state");
    a.Require(h.initialTargets.size()==m_bodies.size()&&(h.postStepTargets.empty()||h.postStepTargets.size()==m_bodies.size()),"saved balance target count differs from mapping");
    for(auto q:h.initialTargets)a.Require(Quaternion(q),"invalid saved balance reference rotation");
    for(auto q:h.postStepTargets)a.Require(Quaternion(q),"invalid saved post-step reference rotation");
    a.Require(Quaternion(h.rootReference)&&Quaternion(h.supportReference),"invalid saved balance frame");
    a.Require(h.swing<0||(h.plannedSupport.IsValid()&&h.stanceSupport.IsValid()),"saved step lacks physical support references");
    if(a.reading){m_observation={};m_waitingForSample=true;m_captureStanceReady=false;m_getUp=false;m_getUpAttempts=0;m_getUpReached=false;}
}
