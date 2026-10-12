#include "RigidBody.h"
#include <algorithm>
#define main original_m77_controller_main
#include "../../../../tests/PhysicalBalanceControllerTests.cpp"
#undef main

// Unchanged actual M77 fixture/control input. All calculations below observe
// state; they do not supply new references, forces, gains or control branches.
int main() {
    Fixture f(0,glm::quat(1,0,0,0),true,false,true);
    f.Advance(120,false);
    f.controller.Request(f.world,BalanceTask::Stand,true,true,f.error);
    f.Advance(360);
    f.world.ApplyLinearImpulse(f.mapping[0].body,{0,0,29});
    std::printf("step,progress,swing,last,landings,swing_time,transfer_time,post_time,root_x,root_y,root_z,upright,speed,com_x,com_y,com_z,contact0,contact1,load0,load1,foot0_x,foot0_y,foot0_z,foot1_x,foot1_y,foot1_z,planned_error,stance_drift,joint_gap,hinge_axis_error,drive_error,max_omega,max_gyro,kinetic,potential,impacts,ccd_queries,ccd_iterations,ccd_limits,reason\n");
    for(int k=0;k<720;++k) {
        f.Advance(1);
        const auto& h=f.controller.History();const auto& o=f.controller.Observation();
        const auto root=f.world.GetTransform(f.mapping[0].body);
        const auto foot0=f.world.GetTransform(f.mapping[3].body),foot1=f.world.GetTransform(f.mapping[6].body);
        double gap=0,axisError=0,driveError=0,omegaPeak=0,gyroPeak=0,kinetic=0,potential=0;
        for(size_t i=0;i<f.mapping.size();++i) {
            const auto& body=f.mapping[i];const auto pose=f.world.GetTransform(body.body);
            const auto w=glm::dvec3(f.world.GetAngularVelocity(body.body));
            const auto v=glm::dvec3(f.world.GetLinearVelocity(body.body));
            const auto inertia=glm::dmat3(f.world.GetInertiaWorld(body.body));
            const auto mass=double(f.world.GetMass(body.body));
            kinetic+=.5*(mass*glm::dot(v,v)+glm::dot(w,inertia*w));
            potential+=mass*9.81*pose.position.y;
            omegaPeak=std::max(omegaPeak,glm::length(w));
            gyroPeak=std::max(gyroPeak,glm::length(glm::inverse(inertia)*glm::cross(w,inertia*w)));
            if(!i)continue;
            JointState state;f.world.GetJoint(body.joint,state);
            const auto parent=f.world.GetTransform(f.mapping[body.parent].body);
            gap=std::max(gap,double(glm::length(pose.position+pose.rotation*state.settings.anchorA-parent.position-parent.rotation*state.settings.anchorB)));
            if(state.settings.type==JointType::Hinge)
                axisError=std::max(axisError,double(glm::length(pose.rotation*state.settings.frameA*glm::vec3(1,0,0)-parent.rotation*state.settings.frameB*glm::vec3(1,0,0))));
            auto error=glm::normalize(parent.rotation*glm::inverse(pose.rotation*state.angularDrive.target));
            driveError=std::max(driveError,2.0*std::atan2(double(glm::length(glm::vec3(error.x,error.y,error.z))),std::abs(double(error.w))));
        }
        double planned=0,stance=0;
        const int active=h.swing>=0?h.swing:h.lastSwing;
        if(active>=0&&h.plannedSupport.IsValid()&&f.world.IsBodyEnabled(h.plannedSupport)) {
            const auto support=f.world.GetTransform(h.plannedSupport);
            const auto& effector=f.settings.effectors[active];
            const auto foot=f.world.GetTransform(f.mapping[effector.foot].body);
            const auto landing=support.position+support.rotation*h.landingLocal;
            planned=glm::length(foot.position-landing);
        }
        if(active>=0&&h.stanceSupport.IsValid()&&f.world.IsBodyEnabled(h.stanceSupport)) {
            const auto support=f.world.GetTransform(h.stanceSupport);
            const auto foot=f.world.GetTransform(f.mapping[f.settings.effectors[1-active].foot].body);
            stance=glm::length(foot.position-support.position-support.rotation*h.stanceLocal);
        }
        const auto& s=f.world.LastStepStats();
        std::printf("%d,%d,%d,%d,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%zu,%zu,%zu,%zu,%s\n",
            k,int(h.progress),h.swing,h.lastSwing,h.landings,h.swingTime,h.transferTime,h.postStepTime,
            root.position.x,root.position.y,root.position.z,o.upright,o.speed,o.com.x,o.com.y,o.com.z,
            o.supports[0].contacts,o.supports[1].contacts,o.supports[0].load,o.supports[1].load,
            foot0.position.x,foot0.position.y,foot0.position.z,foot1.position.x,foot1.position.y,foot1.position.z,
            planned,stance,gap,axisError,driveError,omegaPeak,gyroPeak,kinetic,potential,
            s.impactEvents,s.impactQueries,s.impactSearchIterations,s.impactSearchLimit,h.reason.c_str());
    }
    return 0; // Telemetry is not a new or weakened acceptance test.
}
