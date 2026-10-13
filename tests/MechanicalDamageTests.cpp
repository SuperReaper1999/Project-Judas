#include "Deformable.h"
#include "SaveArchive.h"
#include "Scene.h"
#include "UniformGravity.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>

namespace {
using V=glm::dvec3;
unsigned checks=0,failures=0;
void check(bool result,const char* name){++checks;failures+=!result;std::printf("%s %s\n",result?"PASS":"FAIL",name);}
glm::dmat4 frame(const PhysicsWorld& physics,BodyHandle body){auto t=physics.GetTransform(body);return glm::translate(glm::dmat4(1),V(t.position))*glm::mat4_cast(glm::dquat(t.rotation));}
struct Patch {
    PhysicsWorld physics;
    std::shared_ptr<DeformableAsset> asset;
    DeformableSettings settings;
    DeformableInstance material;
    BodyHandle body;
    DeformableTarget target;
    Patch(double force=0,double torque=0,glm::dquat rotation={1,0,0,0},V position=V(0),V pivotOffset=V(0)){
        if(!physics.Init())throw std::runtime_error("physics initialization");
        asset=std::make_shared<DeformableAsset>(MakeDeformableSheet(3,3,.4,.4));
        // The sheet helper hangs below its top edge. Centre this deliberate
        // torque fixture about the mount COM; an offset patch has a real net
        // translational reaction when its mount spins.
        for(auto& p:asset->nodes)p.y-=.2;
        for(auto& v:asset->render.vertices)v.position.y-=.2f;
        auto& nodes=asset->groups["patch"];for(unsigned n=0;n<asset->nodes.size();++n)nodes.push_back(n);
        std::string error;if(!PrepareDeformableAsset(*asset,error))throw std::runtime_error(error);
        settings.selfContact=false;settings.collisionMask=0;settings.substeps=4;settings.iterations=6;
        settings.material.density=10;settings.material.damping=0;settings.material.airDrag=0;
        DeformableAttachment a;a.group="patch";a.kind=DeformableAttachment::Kind::Body;a.target=1;a.forceLimit=force;a.torqueLimit=torque;a.offset=pivotOffset;
        settings.attachments={a};
        if(pivotOffset!=V(0))body=physics.CreateShape(Shape::Compound({CompoundBox(glm::vec3(pivotOffset),{.2f,.2f,.05f})}),{glm::vec3(position),glm::quat(rotation)},true,2,0,0);
        else body=physics.CreateDynamicBox(glm::vec3(position),{.2f,.2f,.05f},2,0,0);
        physics.ResetBody(body,glm::vec3(position),glm::quat(rotation));physics.SetCollisionFilter(body,0,0);
        target.valid=true;target.body=body;target.previous=target.current=frame(physics,body);
        material.Initialize(asset,settings,target.current*glm::translate(glm::dmat4(1),pivotOffset),1);
    }
    void Step(){UniformGravity zero({0,0,0});material.Step(1./60,physics,zero,{target});}
    V Momentum()const {V total=V(physics.GetLinearVelocity(body))*double(physics.GetMass(body));for(unsigned n=0;n<asset->nodes.size();++n)total+=material.velocities[n]*material.NodeMass(n);return total;}
};
double maximumPositionError(const Patch& a,const Patch& b){double e=0;for(unsigned n=0;n<a.asset->nodes.size();++n)e=std::max(e,glm::length(a.material.positions[n]-b.material.positions[n]));return e;}
}

int main(){try{
    Patch calm(.01,.01);calm.Step();
    check(calm.material.error.empty()&&!calm.material.released[0]&&calm.material.attachmentState[0].peakForce<1e-8&&calm.material.attachmentState[0].peakTorque<1e-8,"finite unloaded material/body connection remains intact and calm");

    // Identical physical impact, different authored capacity. The weak and
    // strong patch solve the same entire step before topology is published.
    Patch weak(1),strong(1e9),unlimited;
    for(auto* p:{&weak,&strong,&unlimited})p->physics.SetLinearVelocity(p->body,{1,0,0});
    const V initialMomentum=weak.Momentum();weak.Step();strong.Step();unlimited.Step();
    check(weak.material.error.empty()&&weak.material.released[0]&&!strong.material.released[0],"actual impact reaction exceeds weak authored capacity while strong control survives");
    check(weak.material.attachmentState[0].peakForce>1&&weak.material.attachmentState[0].peakForce==strong.material.attachmentState[0].peakForce,"capacity does not manufacture or retune the measured physical load");
    check(maximumPositionError(weak,strong)<1e-12&&weak.physics.GetLinearVelocity(weak.body)==strong.physics.GetLinearVelocity(strong.body),"connection failure publishes after the complete physical step without teleport or extra impulse");
    check(maximumPositionError(strong,unlimited)<1e-12&&strong.physics.GetLinearVelocity(strong.body)==unlimited.physics.GetLinearVelocity(unlimited.body),"unlimited legacy and finite strong connection share identical movement mathematics");
    const double momentumError=glm::length(weak.Momentum()-initialMomentum);
    std::printf("IMPACT peak_force_N=%.9g peak_torque_Nm=%.9g momentum_error=%.9g\n",weak.material.attachmentState[0].peakForce,weak.material.attachmentState[0].peakTorque,momentumError);
    check(momentumError<2e-5,"body/material impact exchange conserves measured total linear momentum within float body precision");
    const auto detachedVelocity=weak.physics.GetLinearVelocity(weak.body);weak.physics.ApplyLinearImpulse(weak.body,{.2f,0,0});strong.physics.ApplyLinearImpulse(strong.body,{.2f,0,0});weak.Step();strong.Step();
    check(weak.material.released[0]&&weak.material.attachmentState[0].peakForce==0&&strong.material.attachmentState[0].peakForce>0,"released patch stops transmitting subsequent loads while connected control still reacts");
    check(glm::length(weak.physics.GetLinearVelocity(weak.body)-detachedVelocity)>.05f,"detached component retains finite mass and responds to ordinary later impulse");

    Patch sustained(1),sustainedStrong(1e9);
    for(auto* p:{&sustained,&sustainedStrong})p->material.Force("patch",{100,0,0});
    sustained.Step();sustainedStrong.Step();
    check(sustained.material.released[0]&&!sustainedStrong.material.released[0]&&sustained.material.attachmentState[0].peakForce>1,"ordinary sustained material force can overload a body attachment without a scripted break");

    Patch twist(0,.001),twistStrong(0,1e9);
    for(auto* p:{&twist,&twistStrong})p->physics.SetAngularVelocity(p->body,{0,0,3});
    twist.Step();twistStrong.Step();
    const auto twisting=twist.material.attachmentState[0];
    std::printf("COUPLE peak_force_N=%.9g peak_torque_Nm=%.9g\n",twisting.peakForce,twisting.peakTorque);
    check(twist.material.error.empty()&&twist.material.released[0]&&!twistStrong.material.released[0]&&twisting.peakTorque>.001,"actual rotational load fails torque capacity independently of a force-only rule");
    check(twisting.peakForce<twisting.peakTorque*.05,"symmetric force couple measures torque without inventing force from individual impulse magnitudes");

    const auto q=glm::angleAxis(.73,glm::normalize(V(1,2,3)));
    Patch rotated(1,0,q);rotated.physics.SetLinearVelocity(rotated.body,glm::vec3(q*V(1,0,0)));rotated.Step();
    check(rotated.material.error.empty()&&rotated.material.released[0]&&rotated.material.attachmentState[0].peakForce>1,"arbitrarily oriented physical impact uses the same generic finite-strength mechanism");
    Patch translated(0,.001,{1,0,0,0},{10000,-20000,30000});translated.physics.SetAngularVelocity(translated.body,{0,0,3});translated.Step();
    const double torqueTranslationError=std::abs(translated.material.attachmentState[0].peakTorque-twisting.peakTorque);
    std::printf("TRANSLATED_COUPLE torque_error_Nm=%.9g\n",torqueTranslationError);
    check(translated.material.error.empty()&&translated.material.released[0]&&torqueTranslationError<1e-5,"patch-centroid moment does not turn ordinary translation into false damage");

    // Same physical COM, inertia, shape and material patch; only the authored
    // pivot is different. Point velocity and effective mass must use COM,
    // not the entity pivot returned by GetTransform.
    const V offset(0,.4,0);
    Patch offsetMount(0,1e9,{1,0,0,0},V(0),offset),offsetControl(0,1e9,{1,0,0,0},offset);
    check(glm::length(V(offsetMount.physics.GetCenterOfMass(offsetMount.body))-offset)<1e-7&&glm::length(V(offsetMount.physics.GetTransform(offsetMount.body).position))<1e-7,"offset negative control separates actual COM from authored entity pivot");
    for(auto* p:{&offsetMount,&offsetControl})p->physics.SetAngularVelocity(p->body,{0,0,3});
    offsetMount.Step();offsetControl.Step();
    const double offsetTorqueError=std::abs(offsetMount.material.attachmentState[0].peakTorque-offsetControl.material.attachmentState[0].peakTorque);
    const double offsetVelocityError=glm::length(offsetMount.physics.GetAngularVelocity(offsetMount.body)-offsetControl.physics.GetAngularVelocity(offsetControl.body));
    std::printf("OFFSET_MOUNT torque_error_Nm=%.9g angular_velocity_error=%.9g\n",offsetTorqueError,offsetVelocityError);
    check(offsetMount.material.error.empty()&&maximumPositionError(offsetMount,offsetControl)<1e-7&&offsetTorqueError<1e-5&&offsetVelocityError<1e-6,"offset and centred mounts resolve equivalent physical point velocity, effective mass and torque reaction");

    SaveArchive saved;weak.material.Persist(saved);
    Patch cold(1);SaveArchive read(saved.bytes);cold.material.Persist(read);read.Finish();
    check(cold.material.positions==weak.material.positions&&cold.material.velocities==weak.material.velocities&&cold.material.released==weak.material.released&&cold.material.settings.attachments[0].forceLimit==1,"cold material restore preserves exact physical state, irreversible release and authored strength");
    check(cold.material.attachmentState[0].peakForce==0,"cold restore reconstructs reaction observations instead of persisting solver diagnostics");
    weak.Step();cold.Step();
    check(maximumPositionError(weak,cold)<1e-12,"restored disconnected material reaches the same subsequent physical endpoint");
    SaveArchive legacy;strong.material.Persist(legacy,true,false);Patch oldFormat(1);SaveArchive legacyRead(legacy.bytes);oldFormat.material.Persist(legacyRead,true,false);legacyRead.Finish();
    check(oldFormat.material.settings.attachments[0].forceLimit==0&&oldFormat.material.positions==strong.material.positions,"old participant layout decodes unchanged with explicitly unlimited historical strength");
    auto attachment=strong.settings.attachments[0];SaveArchive legacyAttachment;attachment.Save(legacyAttachment);attachment.forceLimit=attachment.torqueLimit=0;SaveArchive defaultAttachment;attachment.Save(defaultAttachment);
    check(legacyAttachment.bytes==defaultAttachment.bytes,"legacy attachment field bytes are independent of new optional limits");
    auto malformed=saved.bytes;
    // State.2 appends exactly one force/torque pair before its historical 3x3
    // material reference. Replace the force field with +infinity, not a
    // hand-authored weakened acceptance threshold.
    const size_t forceField=malformed.size()-9*sizeof(double)-2*sizeof(double);
    for(unsigned i=0;i<8;++i)malformed[forceField+i]=char((uint64_t(0x7ff0000000000000)>>(8*i))&255);
    bool rejected=false;try{Patch bad(1);SaveArchive input(malformed);bad.material.Persist(input);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"nonfinite saved finite strength is rejected rather than silently restoring a connection");
    rejected=false;try{Patch bad(1);SaveArchive input(saved.bytes.substr(0,saved.bytes.size()-1));bad.material.Persist(input);input.Finish();}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"truncated strength-aware physical state fails cleanly");

    SceneObject authored;authored.deformable=strong.settings;auto properties=DeformableProperties(authored);SceneObject parsed;std::string error;
    check(properties.count("deformable.attachmentLimits")&&ApplyDeformableProperties(properties,parsed,error)&&parsed.deformable->attachments[0].forceLimit==1e9,"optional generic attachment strengths round-trip through ordinary authored properties");
    authored.deformable->attachments[0].forceLimit=0;auto oldProperties=DeformableProperties(authored);
    check(!oldProperties.count("deformable.attachmentLimits")&&ApplyDeformableProperties(oldProperties,parsed,error),"default legacy authoring emits no new property or changed attachment row");
    properties["deformable.attachmentLimits"]="2 \"patch\" 1 0 \"patch\" 2 0";
    check(!ApplyDeformableProperties(properties,parsed,error),"duplicate finite patch strength is rejected");
    properties["deformable.attachmentLimits"]="1 \"missing\" 1 0";
    check(!ApplyDeformableProperties(properties,parsed,error),"unknown material patch strength cannot bind by accidental identity");
    auto invalid=strong.settings;invalid.attachments[0].forceLimit=-1;
    check(!ValidDeformableSettings(invalid,error),"negative strength is invalid");invalid.attachments[0].forceLimit=std::numeric_limits<double>::infinity();
    check(!ValidDeformableSettings(invalid,error),"nonfinite strength is invalid");invalid.attachments[0].forceLimit=1;invalid.attachments[0].kind=DeformableAttachment::Kind::World;
    check(!ValidDeformableSettings(invalid,error),"unmeasured prescribed world/bone frame cannot claim finite dynamic strength");
    Patch unsupported(1);auto staticBody=unsupported.physics.CreateStaticBox({0,0,0},{.2f,.2f,.05f},0,0);unsupported.target.body=staticBody;const auto prior=unsupported.material.positions;unsupported.Step();
    check(!unsupported.material.error.empty()&&unsupported.material.positions==prior&&!unsupported.material.released[0],"finite patch refuses a static target before mutating physical geometry");
    Patch unlimitedStatic;unlimitedStatic.target.body=unlimitedStatic.physics.CreateStaticBox({0,0,0},{.2f,.2f,.05f},0,0);unlimitedStatic.Step();
    check(unlimitedStatic.material.error.empty()&&!unlimitedStatic.material.released[0],"unlimited static compatibility root remains valid");
    Patch disabled(1);disabled.material.settings.enabled=false;disabled.physics.SetLinearVelocity(disabled.body,{1,0,0});const auto frozen=disabled.material.positions;disabled.Step();
    check(disabled.material.positions==frozen&&!disabled.material.released[0],"disabled material does not acquire load or failure from an unevaluated attachment");

    Patch failedStepWorld;auto solid=std::make_shared<DeformableAsset>(MakeDeformableBlock({1,1,1},{1,1,1}));
    auto failing=failedStepWorld.settings;failing.substeps=failing.iterations=1;failing.material.shearCompliance=failing.material.volumeCompliance=1e9;
    failing.attachments[0].group="left";failing.attachments[0].forceLimit=1e-9;
    DeformableInstance inverted;inverted.Initialize(solid,failing,glm::dmat4(1),99);
    for(auto& position:inverted.positions)position.x=-position.x;
    failedStepWorld.physics.SetLinearVelocity(failedStepWorld.body,{1,0,0});UniformGravity zero({0,0,0});
    inverted.Step(1./60,failedStepWorld.physics,zero,{failedStepWorld.target});
    std::printf("FAILED_STEP reaction_N=%.9g error=%s\n",inverted.attachmentState[0].peakForce,inverted.error.c_str());
    check(!inverted.error.empty()&&inverted.attachmentState[0].peakForce>failing.attachments[0].forceLimit&&!inverted.released[0],"failed out-of-envelope solve cannot publish a pending connection failure");

    std::printf("M83 material-attachment checks=%u failures=%u\n",checks,failures);return failures?1:0;
}catch(const std::exception& e){std::fprintf(stderr,"M83 material-attachment exception: %s\n",e.what());return 1;}}
