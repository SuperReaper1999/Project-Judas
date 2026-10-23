// Capture-point stepping controller, selected by BalanceSettings::captureStepping
// once the established controller has measured standing. EXPERIMENTAL: it does
// not yet pass the convergence matrix (see docs/MK2_PHYSICS_ARCHITECTURE.md);
// the established controller stays the default.
//
// - Supporting legs are struts under the body where it actually is. Hips and
//   knees cannot move the COM without ankle torque, so the COM is regulated
//   through the planted ankles by leaning the supporting attitudes, with the
//   error measured from the contact centroid (never the centre of pressure the
//   same ankle torque produces). Double support adds a small posture pull.
// - Planted ankle torque is bounded by the foot's share of body weight times
//   the sole half-extent (ZMP); more only rolls the foot onto an edge.
// - Joint targets are relative between desired attitudes, which keeps the
//   implicit joint springs stable. Measured references are used only where a
//   world anchor matters: a planted ankle aims its shank from the actual foot;
//   in single support the stance hip holds the trunk's world attitude from the
//   actual thigh and the swing hip aims the swing thigh from the actual trunk.
// - The swing chain is solved from the actual hip toward a foot path that
//   follows the capture point until mid-swing, with velocity feed-forward
//   from a one-step lookahead, and keeps descending past its schedule until
//   the foot finds support.
// - A step starts when the capture point leaves the support polygon.
#include "PhysicalBalance.h"
#include "PhysicalBalanceMath.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>
using namespace PhysicalBalanceMath;
namespace {
// Signed distance of p outside the convex hull of the points (negative inside),
// in the plane normal to up.
float OutsideHull(const std::vector<glm::vec3>& points,glm::vec3 p,glm::vec3 up){
    glm::vec3 a=std::abs(up.x)<.9f?glm::vec3(1,0,0):glm::vec3(0,1,0);a=glm::normalize(a-up*glm::dot(a,up));const auto b=glm::cross(up,a);
    std::vector<glm::vec2> q;for(const auto& v:points)q.push_back({glm::dot(v,a),glm::dot(v,b)});const glm::vec2 x(glm::dot(p,a),glm::dot(p,b));
    if(q.empty())return 1e9f;
    std::sort(q.begin(),q.end(),[](glm::vec2 l,glm::vec2 r){return l.x<r.x||(l.x==r.x&&l.y<r.y);});
    auto turn=[](glm::vec2 o,glm::vec2 l,glm::vec2 r){return (l.x-o.x)*(r.y-o.y)-(l.y-o.y)*(r.x-o.x);};
    std::vector<glm::vec2> h(2*q.size());size_t k=0; // monotone chain, counter-clockwise
    for(size_t i=0;i<q.size();++i){while(k>=2&&turn(h[k-2],h[k-1],q[i])<=0)--k;h[k++]=q[i];}
    for(size_t i=q.size()-1,t=k+1;i>0;--i){while(k>=t&&turn(h[k-2],h[k-1],q[i-1])<=0)--k;h[k++]=q[i-1];}
    h.resize(k>1?k-1:k);
    float nearest=1e9f,depth=1e9f;bool outside=h.size()<3;
    for(size_t i=0;i<h.size();++i){const auto e0=h[i],e1=h[(i+1)%h.size()],e=e1-e0;const float len2=glm::dot(e,e);
        const float t=len2>1e-12f?std::clamp(glm::dot(x-e0,e)/len2,0.f,1.f):0.f;nearest=std::min(nearest,glm::length(x-(e0+e*t)));
        if(len2>1e-12f){const float side=turn(e0,e1,x)/std::sqrt(len2);if(side<0)outside=true;depth=std::min(depth,side);}}
    return outside?nearest:-depth;
}
float Smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
glm::vec3 RotationVector(glm::quat q){if(q.w<0)q=-q;glm::vec3 v(q.x,q.y,q.z);float l=glm::length(v);return l>1e-7f?v*(2*std::atan2(l,q.w)/l):2.f*v;}
glm::vec3 ClampLength(glm::vec3 v,float limit){float l=glm::length(v);return l>limit&&l>0?v*(limit/l):v;}
// Double-support posture pull toward the support centre (metres). Both feet can
// react it; it holds position against slow drift that ankle lean alone leaves.
constexpr float kDoubleSupportPull=.05f;
// Capture steps run the authored swing this many times faster. The authored
// stepSeconds is an unhurried swing; a capture step must land before the
// capture point leaves its reach. Measured on the harness: urgency-scaled
// clocks (1-3x by capture distance) and 2x/2.5x recovered less.
constexpr float kCaptureSwingRate=3;
// Step when the capture point lies outside the support polygon by more than
// this (metres); inside it the ankles can still stop the body.
constexpr float kTriggerOutside=0;
}

bool PhysicalBalanceController::CaptureLeg(PhysicsWorld& world,unsigned chain,glm::vec3 hip,glm::vec3 ankle,const glm::quat& upright,std::vector<glm::quat>& w) const{
    const auto& e=m_settings.effectors[chain];JointState hipJoint,knee,ankleJoint;
    if(!world.GetJoint(m_bodies[e.upper].joint,hipJoint)||!world.GetJoint(m_bodies[e.lower].joint,knee)||!world.GetJoint(m_bodies[e.foot].joint,ankleJoint))return false;
    auto upperAxis=knee.settings.anchorB-hipJoint.settings.anchorA,lowerAxis=ankleJoint.settings.anchorB-knee.settings.anchorA;
    float a=glm::length(upperAxis),b=glm::length(lowerAxis);if(a<.01f||b<.01f)return false;
    float distance=std::clamp(glm::length(ankle-hip),std::abs(a-b)+.001f,a+b-.001f);
    auto direction=ankle-hip;if(glm::dot(direction,direction)<1e-10f)return false;direction=glm::normalize(direction);
    auto bend=Tangent(upright*m_settings.forward,direction);if(glm::dot(bend,bend)<1e-6f)bend=Tangent(upright*m_settings.up,direction);if(glm::dot(bend,bend)<1e-6f)return false;bend=glm::normalize(bend);
    float along=(a*a-b*b+distance*distance)/(2*distance),height=std::sqrt(std::max(a*a-along*along,0.f));auto elbow=hip+direction*along+bend*height,end=hip+direction*distance;
    if(knee.settings.type==JointType::Hinge){auto planeAxis=glm::normalize(glm::cross(bend,direction));
        w[e.upper]=LinkFrame(upperAxis,knee.settings.frameB*glm::vec3(1,0,0),elbow-hip,planeAxis);
        w[e.lower]=LinkFrame(lowerAxis,knee.settings.frameA*glm::vec3(1,0,0),end-elbow,planeAxis);
    }else{auto referenceUpper=upright*m_bodies[e.upper].restRotation,referenceLower=upright*m_bodies[e.lower].restRotation;
        w[e.upper]=Align(referenceUpper*upperAxis,elbow-hip,upright*m_settings.forward)*referenceUpper;
        w[e.lower]=Align(referenceLower*lowerAxis,end-elbow,upright*m_settings.forward)*referenceLower;}
    w[e.foot]=upright*m_bodies[e.foot].restRotation;
    return true;
}

bool PhysicalBalanceController::CaptureFoothold(PhysicsWorld& world,int foot,const BodyTransform& pose,glm::vec3 displacement,const glm::quat& upright,BodyHandle& support,glm::vec3& destination) const{
    const auto& effector=m_settings.effectors[foot];
    displacement=ClampLength(Tangent(displacement,m_observation.up),m_settings.stepReach);
    const auto origin=World(pose,effector.contactOffset)+displacement+m_observation.up*m_settings.stepHeight;
    auto hit=world.Raycast(origin,-m_observation.up,2*m_settings.stepHeight+.15f,m_settings.query);
    if(!hit.hit||glm::dot(hit.normal,m_observation.up)<effector.normalAcceptance)return false;
    // Translational conservative foot envelope, as the established planner.
    destination=hit.point-upright*effector.contactOffset;
    auto travel=destination-pose.position;float length=glm::length(travel);
    auto overhead=pose;overhead.position+=m_observation.up*m_settings.stepHeight;overhead.rotation=upright*m_bodies[effector.foot].restRotation;
    if(world.BoxCast(overhead,effector.halfExtents,travel,std::max(length,.001f),m_settings.query).hit)return false;
    support=hit.body;return true;
}

bool PhysicalBalanceController::CapturePlan(PhysicsWorld& world,glm::vec3 capture,const glm::quat& upright){
    JUDAS_PROFILE_SCOPE("Physical balance capture foothold planning");
    if(m_history.landings>=m_settings.maximumSteps)return false;
    // Same discrete choice as the established planner: lighter support, with
    // sub-per-mille load differences treated as a tie in authored order.
    // Later steps swing the trailing foot, the one further from the capture
    // point: stepping the leading foot again leaves the body behind it.
    const float load0=m_observation.supports[0].load,load1=m_observation.supports[1].load,tie=.001f*(load0+load1);
    int foot=load1+tie<load0?1:0;
    if(m_history.landings>0){const auto d0=glm::length(Tangent(capture-m_observation.supports[0].centre,m_observation.up)),d1=glm::length(Tangent(capture-m_observation.supports[1].centre,m_observation.up));foot=d1>d0?1:0;}
    const auto& stance=m_observation.supports[1-foot];if(!stance.contacts)return false;
    const auto& effector=m_settings.effectors[foot];auto pose=world.GetTransform(m_bodies[effector.foot].body);
    // Translate the support pattern so its centre lands on the capture point.
    auto centre=(m_observation.supports[0].centre+m_observation.supports[1].centre)*.5f;
    BodyHandle platformBody;glm::vec3 destination;
    if(!CaptureFoothold(world,foot,pose,capture-centre,upright,platformBody,destination))return false;
    auto platform=world.GetTransform(platformBody);
    m_history.swing=foot;m_history.plannedSupport=platformBody;m_history.startLocal=Local(platform,pose.position);m_history.landingLocal=Local(platform,destination);
    m_history.stanceSupport=stance.body;auto stanceFrame=world.GetTransform(stance.body);
    m_history.stanceLocal=Local(stanceFrame,world.GetTransform(m_bodies[m_settings.effectors[1-foot].foot].body).position);
    m_history.rootStartLocal=Local(stanceFrame,world.GetTransform(m_bodies[0].body).position);m_history.comStartLocal=Local(stanceFrame,m_observation.com);
    m_history.swingTime=-1;m_history.transferTime=0;m_history.lifted=false;
    return true;
}

bool PhysicalBalanceController::CaptureStep(PhysicsWorld& world,const glm::quat& upright,float dt){
    JUDAS_PROFILE_SCOPE("Physical balance capture stepping");
    const auto up=m_observation.up;const float gravity=glm::length(m_observation.gravity),weight=m_observation.mass*gravity;
    const auto root=world.GetTransform(m_bodies[0].body);
    auto& supports=m_observation.supports;
    unsigned count=0;glm::vec3 base(0),supportVelocity(0);
    for(const auto& s:supports)if(s.contacts){base+=s.centre;supportVelocity+=s.velocity;++count;}
    if(count){base/=float(count);supportVelocity/=float(count);}
    const auto velocity=Tangent(m_observation.velocity-supportVelocity,up);
    auto soleCorners=[&](){std::vector<glm::vec3> corners;for(unsigned f=0;f<2;++f)if(supports[f].contacts){const auto& e=m_settings.effectors[f];auto pose=world.GetTransform(m_bodies[e.foot].body);
            for(float sx:{-1.f,1.f})for(float sz:{-1.f,1.f})corners.push_back(World(pose,e.contactOffset+glm::vec3(sx*e.halfExtents.x,0,sz*e.halfExtents.z)));}return corners;};
    auto captureAbout=[&](glm::vec3 point){float h=std::max(glm::dot(m_observation.com-point,up),.1f);return m_observation.com+velocity*std::sqrt(h/gravity);};

    // Step decisions.
    if(m_history.swing<0&&count==2&&m_history.stepping&&(m_history.postStepTargets.empty()||m_history.postStepTime>=.25f*m_settings.stepSeconds)){
        auto capture=captureAbout(base);
        // Step when the capture point leaves the support polygon by more than
        // the authored trigger margin; inside it the ankles can still stop
        // the body.
        if((glm::length(velocity)>m_settings.standingSpeed||m_observation.upright<.96f)&&OutsideHull(soleCorners(),capture,up)>kTriggerOutside&&!CapturePlan(world,capture,upright))m_history.reason="no clear reachable corrective foothold";
    }
    int swing=m_history.swing;
    if(swing>=0){
        if(!world.IsBodyEnabled(m_history.stanceSupport)||!world.IsBodyEnabled(m_history.plannedSupport)){Fail(world,"corrective step lost support, reach or physical touchdown");return false;}
        const auto& stance=supports[1-swing];
        if(m_history.swingTime<0){
            m_history.transferTime+=dt;
            // Lift as soon as the stance foot has support. Waiting for the
            // load to shift lets the body run past the stance toe first; the
            // swing foot lands to catch any lean toward it as well.
            if(stance.contacts)m_history.swingTime=0;
            else if(m_history.transferTime>m_settings.transferSeconds){Fail(world,"insufficient physical load transfer for corrective step");return false;}
        }else{
            m_history.swingTime+=dt*kCaptureSwingRate;
            // Until mid-swing the foothold follows the current capture point:
            // the body keeps moving through the swing.
            if(m_history.swingTime<.5f*m_settings.stepSeconds&&stance.contacts){
                auto platform=world.GetTransform(m_history.plannedSupport);const auto& effector=m_settings.effectors[swing];
                BodyTransform start=platform;start.position=World(platform,m_history.startLocal);start.rotation=upright*m_bodies[effector.foot].restRotation;
                auto capture=captureAbout(stance.centre);BodyHandle body;glm::vec3 destination;
                if(CaptureFoothold(world,swing,start,capture-(World(start,effector.contactOffset)+stance.centre)*.5f,upright,body,destination)&&body.id==m_history.plannedSupport.id&&body.world==m_history.plannedSupport.world)m_history.landingLocal=Local(platform,destination);
            }
        }
    }

    // Desired world attitudes and posture reference.
    auto& w=m_desired;for(size_t i=0;i<m_bodies.size();++i)w[i]=upright*m_bodies[i].restRotation;
    m_captureAhead=w;
    const bool single=swing>=0&&m_history.swingTime>=0;
    // Supporting roles follow the step state, not each frame's contacts: a
    // stance foot that loses contact for a frame (a bounce) is still the
    // stance, and re-posing its chain on every flicker would fold it.
    std::array<bool,2> planted{};for(unsigned f=0;f<2;++f)planted[f]=!(single&&int(f)==swing);
    if(count){
        // Supporting legs are struts under the body where it actually is; the
        // hips and knees cannot move the COM without ankle torque, and asking
        // them to only folds the chain. The COM is regulated through the
        // planted ankles alone, by leaning the supporting attitudes toward the
        // support centre (or, while transferring and in single support, the
        // stance foot). The error is measured from the contact centroid, not
        // the centre of pressure that this same ankle torque produces.
        glm::vec3 target=base;if(single&&supports[1-swing].contacts)target=supports[1-swing].centre;
        auto error=Tangent(m_observation.com-target,up);
        auto tilt=glm::cross(up,-(error*m_settings.positionResponse+velocity*m_settings.velocityResponse));float lean=glm::length(tilt);
        auto correction=lean>1e-5f?glm::angleAxis(std::min(lean,m_settings.maximumLean),tilt/lean):glm::quat(1,0,0,0);
        // In double support both feet can react a small posture pull toward
        // the target, which holds position against slow drift; single
        // support keeps the strut under the body.
        const glm::vec3 shift=single?glm::vec3(0):ClampLength(Tangent(target-m_observation.com,up),kDoubleSupportPull);
        BodyTransform reference{root.position+shift,upright*m_bodies[0].restRotation};
        // Height above the floor the contacting supports actually stand on; a
        // bouncing foot must not raise its own floor.
        float floor=0;unsigned floors=0;for(unsigned f=0;f<2;++f)if(planted[f]&&supports[f].contacts){floor+=glm::dot(supports[f].centre,up);++floors;}
        if(floors)reference.position+=up*(floor/float(floors)+m_settings.standingHeight-glm::dot(reference.position,up));
        // Keep the reference within each supporting chain's reach.
        for(unsigned f=0;f<2;++f)if(planted[f]){const auto& e=m_settings.effectors[f];JointState hip,knee,ankle;
            if(!world.GetJoint(m_bodies[e.upper].joint,hip)||!world.GetJoint(m_bodies[e.lower].joint,knee)||!world.GetJoint(m_bodies[e.foot].joint,ankle))continue;
            auto end=World(world.GetTransform(m_bodies[e.foot].body),ankle.settings.anchorA);auto hipOffset=reference.rotation*hip.settings.anchorB;
            float reach=(glm::length(knee.settings.anchorB-hip.settings.anchorA)+glm::length(ankle.settings.anchorB-knee.settings.anchorA))*.9999f;
            float horizontal=glm::length(Tangent(end-reference.position-hipOffset,up));if(horizontal>=reach)continue;
            float excess=glm::dot(reference.position+hipOffset-end,up)-std::sqrt(reach*reach-horizontal*horizontal);if(excess>0)reference.position-=up*excess;}
        for(unsigned f=0;f<2;++f)if(planted[f]){JointState hip,ankle;world.GetJoint(m_bodies[m_settings.effectors[f].upper].joint,hip);world.GetJoint(m_bodies[m_settings.effectors[f].foot].joint,ankle);
            CaptureLeg(world,f,World(reference,hip.settings.anchorB),World(world.GetTransform(m_bodies[m_settings.effectors[f].foot].body),ankle.settings.anchorA),upright,w);}
        for(unsigned f=0;f<2;++f)if(planted[f]){const auto& e=m_settings.effectors[f];for(auto b:{e.upper,e.lower,e.foot})w[b]=correction*w[b];}
        if(!single)w[0]=correction*w[0];
    }
    if(single){
        const auto& effector=m_settings.effectors[swing];auto platform=world.GetTransform(m_history.plannedSupport);
        auto start=World(platform,m_history.startLocal),landing=World(platform,m_history.landingLocal);
        // Past the nominal swing the target keeps descending until the foot
        // finds support (bounded to the step height), so touchdown does not
        // wait on the body coming down to a reference that stopped short.
        auto footTarget=[&](float time){const float phase=time/m_settings.stepSeconds,s=Smooth(phase);
            auto p=glm::mix(start,landing,s)+up*(m_settings.stepHeight*std::sin(3.1415926536f*s));
            if(phase>1)p-=up*std::min((phase-1)*m_settings.stepHeight*2,m_settings.stepHeight);
            return p;};
        JointState hip,ankle;world.GetJoint(m_bodies[effector.upper].joint,hip);world.GetJoint(m_bodies[effector.foot].joint,ankle);
        auto hipCentre=World(root,hip.settings.anchorB),hipAhead=hipCentre+world.GetLinearVelocity(m_bodies[0].body)*dt;
        CaptureLeg(world,swing,hipCentre,footTarget(m_history.swingTime)+upright*ankle.settings.anchorA,upright,w);
        CaptureLeg(world,swing,hipAhead,footTarget(m_history.swingTime+dt)+upright*ankle.settings.anchorA,upright,m_captureAhead);
        // Touchdown: a lifted foot regaining supporting contact on the planned
        // support while descending has landed where it physically is.
        auto footPose=world.GetTransform(m_bodies[effector.foot].body);float t=m_history.swingTime/m_settings.stepSeconds;
        // Lift-off is the loss of support after the swing begins. A trailing
        // foot often starts rolled onto its toe, so a rise above its start
        // pose is not required.
        if(t>.1f&&!supports[swing].contacts)m_history.lifted=true;
        if(m_history.lifted&&supports[swing].contacts&&supports[swing].body.id==m_history.plannedSupport.id&&Smooth(t)>=.5f){
            m_history.lastSwing=swing;m_history.landedLocal=Local(platform,footPose.position);m_history.swing=-1;++m_history.landings;
            m_history.postStepTargets.assign(m_bodies.size(),glm::quat(1,0,0,0));m_history.postStepTime=0;
        }else if(m_history.swingTime>=m_settings.stepSeconds*3){Fail(world,"corrective step lost support, reach or physical touchdown");return false;}
    }

    // Drives.
    const auto& sw=single?m_settings.effectors[swing]:m_settings.effectors[0];
    auto inSwing=[&](size_t i){return single&&(i==sw.upper||i==sw.lower||i==sw.foot);};
    auto chainOf=[&](size_t i)->int{for(unsigned f=0;f<2;++f){const auto& e=m_settings.effectors[f];if(i==e.upper||i==e.lower||i==e.foot)return int(f);}return -1;};
    for(size_t i=1;i<m_bodies.size();++i){JointState state;if(!world.GetJoint(m_bodies[i].joint,state)||!state.settings.enabled||state.settings.motor||state.settings.spring){Fail(world,"joint ownership changed");return false;}
        JointAngularDrive drive;drive.enabled=m_history.effort;drive.stiffness=m_settings.stiffness;drive.damping=m_settings.damping;drive.maxTorque=m_settings.maxTorque;
        const auto parent=m_bodies[m_bodies[i].parent].body;const int chain=chainOf(i);
        auto childPose=world.GetTransform(m_bodies[i].body),parentPose=world.GetTransform(parent);
        // Relative targets between desired attitudes keep the joint springs
        // implicit and stable. Measured references are used only where the
        // world anchor matters: a planted ankle aims its shank from the actual
        // foot; in single support the stance hip holds the trunk's world
        // attitude from the actual thigh and the swing hip aims the swing
        // thigh from the actual trunk.
        drive.target=glm::inverse(w[i])*w[m_bodies[i].parent];
        if(inSwing(i)){auto rate=[&](size_t b){return RotationVector(m_captureAhead[b]*glm::inverse(w[b]))/dt;};
            const auto parentRate=i==sw.upper?world.GetAngularVelocity(parent):rate(m_bodies[i].parent);
            if(i==sw.upper)drive.target=glm::inverse(w[i])*parentPose.rotation;
            drive.relativeAngularVelocity=parentRate-rate(i);
        }else if(chain>=0&&planted[chain]){
            if(i==m_settings.effectors[chain].foot){
                drive.target=glm::inverse(childPose.rotation)*w[m_bodies[i].parent];
                drive.relativeAngularVelocity=supports[chain].contacts?-world.GetAngularVelocity(supports[chain].body):glm::vec3(0);
                // The foot's share of body weight, from the measured split
                // when the feet carry a meaningful load: one bounce frame must
                // not make the supporting ankle limp.
                const auto& e=m_settings.effectors[chain];float sole=std::min(e.halfExtents.x,e.halfExtents.z);
                float total=0;unsigned feet=0;for(unsigned f=0;f<2;++f)if(planted[f]){total+=supports[f].load;++feet;}
                float share=feet?1.f/float(feet):1.f;if(total>.2f*weight*dt)share=supports[chain].load/total;
                // The joint solver splits a ball drive's cap across its axes;
                // express the sole bound per axis.
                JointState ankleState;const float axes=world.GetJoint(m_bodies[i].joint,ankleState)&&ankleState.settings.type==JointType::Hinge?1.f:std::sqrt(3.f);
                drive.maxTorque=std::min(drive.maxTorque,std::max(weight*share*sole*axes,.01f*m_settings.maxTorque));
            }else if(single&&i==m_settings.effectors[chain].upper){drive.target=glm::inverse(childPose.rotation)*w[0];drive.relativeAngularVelocity=-world.GetAngularVelocity(m_bodies[i].body);}
        }
        if(!world.SetJointAngularDrive(m_bodies[i].joint,drive)){Fail(world,"bounded joint drive rejected");return false;}
        if(glm::length(state.angularDrive.impulses)/dt>=m_settings.maxTorque*.98f)++m_observation.saturated;
    }
    return true;
}
