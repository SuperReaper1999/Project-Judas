#pragma once
#include "PhysicsHandles.h"
#include <array>
#include <cstdint>
#include <vector>
class DeformableInstance;
class PhysicsWorld;
class GravityField;
struct DeformableTarget;

// Owner-thread interval coordinator. Independent kernels accept exclusively
// owned batch data; rigid contacts, reactions and attachment publication never
// execute on workers. Standalone stepping composes these same phases.
class MaterialInterval {
public:
    enum class Independent {Internal,SelfContact,Finish};
    MaterialInterval(DeformableInstance&,double,unsigned,PhysicsWorld&,const GravityField&,
                     const std::vector<DeformableTarget>&,bool currentRigidPose);
    ~MaterialInterval();
    MaterialInterval(const MaterialInterval&)=delete;
    MaterialInterval& operator=(const MaterialInterval&)=delete;
    bool Ready()const;
    unsigned Subdivisions()const{return subdivisions;}
    DeformableInstance& State(){return state;}
    void Predict(unsigned substep);
    void RunIndependent(Independent,DeformableInstance& data);
    void RigidContacts();
    void Attach();
private:
    DeformableInstance& state;
    PhysicsWorld& physics;
    const GravityField& gravity;
    const std::vector<DeformableTarget>& targets;
    unsigned subdivisions=1;
    double h=0,fraction=0;
    bool active=false,measured=false;
    std::array<BodyHandle,64> awakened{};
    unsigned awakenedCount=0;
    std::array<uint64_t,4> phaseNanoseconds{};
};
