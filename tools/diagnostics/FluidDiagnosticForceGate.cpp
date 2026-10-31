#include "FluidDiagnosticForceGate.h"
#include "ProductionFluidCoupling.h"
#include "PlayerController.h"
#include "RuntimeWorld.h"

namespace FluidLaunchDiagnostic {
bool suppress=false;
Counts counts;
namespace { bool preparingFluid=false; }
}
// GNU ld wrappers intercept only this diagnostic executable's references.
// There is no copied/edited production solver and no runtime engine switch.
extern void RealPrepare(ProductionFluidCoupling*,RuntimeWorld&,float)
    asm("__real__ZN23ProductionFluidCoupling16PrepareRigidStepER12RuntimeWorldf");
extern void WrapPrepare(ProductionFluidCoupling* coupling,RuntimeWorld& world,float dt)
    asm("__wrap__ZN23ProductionFluidCoupling16PrepareRigidStepER12RuntimeWorldf");
void WrapPrepare(ProductionFluidCoupling* coupling,RuntimeWorld& world,float dt) {
    struct Scope {
        bool previous=FluidLaunchDiagnostic::preparingFluid;
        Scope(){FluidLaunchDiagnostic::preparingFluid=true;}
        ~Scope(){FluidLaunchDiagnostic::preparingFluid=previous;}
    } scope;
    RealPrepare(coupling,world,dt);
}
extern void RealForce(PhysicsWorld*,BodyHandle,const glm::vec3&)
    asm("__real__ZN12PhysicsWorld10ApplyForceE10BodyHandleRKN3glm3vecILi3EfLNS1_9qualifierE0EEE");
extern void WrapForce(PhysicsWorld* physics,BodyHandle body,const glm::vec3& force)
    asm("__wrap__ZN12PhysicsWorld10ApplyForceE10BodyHandleRKN3glm3vecILi3EfLNS1_9qualifierE0EEE");
void WrapForce(PhysicsWorld* physics,BodyHandle body,const glm::vec3& force) {
    if(FluidLaunchDiagnostic::suppress&&FluidLaunchDiagnostic::preparingFluid) {
        ++FluidLaunchDiagnostic::counts.forceCalls;return;
    }
    RealForce(physics,body,force);
}
extern void RealTorque(PhysicsWorld*,BodyHandle,const glm::vec3&,bool)
    asm("__real__ZN12PhysicsWorld11ApplyTorqueE10BodyHandleRKN3glm3vecILi3EfLNS1_9qualifierE0EEEb");
extern void WrapTorque(PhysicsWorld* physics,BodyHandle body,const glm::vec3& torque,bool wake)
    asm("__wrap__ZN12PhysicsWorld11ApplyTorqueE10BodyHandleRKN3glm3vecILi3EfLNS1_9qualifierE0EEEb");
void WrapTorque(PhysicsWorld* physics,BodyHandle body,const glm::vec3& torque,bool wake) {
    if(FluidLaunchDiagnostic::suppress&&FluidLaunchDiagnostic::preparingFluid) {
        ++FluidLaunchDiagnostic::counts.torqueCalls;return;
    }
    RealTorque(physics,body,torque,wake);
}
extern void RealSample(PlayerController*,const PlayerFluidSample&)
    asm("__real__ZN16PlayerController14SetFluidSampleERK17PlayerFluidSample");
extern void WrapSample(PlayerController* player,const PlayerFluidSample& sample)
    asm("__wrap__ZN16PlayerController14SetFluidSampleERK17PlayerFluidSample");
void WrapSample(PlayerController* player,const PlayerFluidSample& sample) {
    if(FluidLaunchDiagnostic::suppress) {
        ++FluidLaunchDiagnostic::counts.swimSamples;RealSample(player,{});return;
    }
    RealSample(player,sample);
}
using Particles=std::vector<PhysicsWorld::ContactParticle>;
using Rows=std::vector<PhysicsWorld::ParticleBoundaryContact>;
using Impulses=std::vector<glm::vec3>;
extern void RealContacts(PhysicsWorld*,Particles&,const Rows&,Impulses&,glm::vec3*,float)
    asm("__real__ZN12PhysicsWorld21SolveParticleContactsERSt6vectorINS_15ContactParticleESaIS1_EERKS0_INS_23ParticleBoundaryContactESaIS5_EERS0_IN3glm3vecILi3EfLNSA_9qualifierE0EEESaISD_EEPSD_f");
extern void WrapContacts(PhysicsWorld*,Particles&,const Rows&,Impulses&,glm::vec3*,float)
    asm("__wrap__ZN12PhysicsWorld21SolveParticleContactsERSt6vectorINS_15ContactParticleESaIS1_EERKS0_INS_23ParticleBoundaryContactESaIS5_EERS0_IN3glm3vecILi3EfLNSA_9qualifierE0EEESaISD_EEPSD_f");
void WrapContacts(PhysicsWorld* physics,Particles& particles,const Rows& rows,Impulses& impulses,glm::vec3* support,float dt) {
    if(!FluidLaunchDiagnostic::suppress) {RealContacts(physics,particles,rows,impulses,support,dt);return;}
    Rows prescribed=rows;
    for(auto& row:prescribed)if(row.twoWay){row.twoWay=false;++FluidLaunchDiagnostic::counts.reactionRows;}
    RealContacts(physics,particles,prescribed,impulses,support,dt);
}
