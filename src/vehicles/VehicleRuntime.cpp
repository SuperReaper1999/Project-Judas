#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"

#include <algorithm>
#include <tuple>

void RuntimeWorld::PrepareWheelInterval(float dt) {
    JUDAS_PROFILE_SCOPE("Vehicle contact preparation");
    m_vehicleWheels.clear();
    for(auto id:m_wheelOwners)if(RuntimeWheel(id))m_vehicleWheels.push_back(id);
    JUDAS_PROFILE_COUNTER("Vehicle contact intervals",1,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Wheel instances",double(m_vehicleWheels.size()),ProfileCounterMode::Latest);
    // A physical layout, not component creation order, supplies the initial
    // sweep order. Iterative forward/reverse sweeps then converge the coupled
    // response; ordering is not a substitute for convergence.
    std::sort(m_vehicleWheels.begin(),m_vehicleWheels.end(),[&](auto a,auto b) {
        const auto& x=m_wheels.at(a).settings;const auto& y=m_wheels.at(b).settings;
        return std::tie(x.chassis,x.part,x.mount.x,x.mount.y,x.mount.z,x.radius,x.width)<
               std::tie(y.chassis,y.part,y.mount.x,y.mount.y,y.mount.z,y.radius,y.width);
    });
    for(auto id:m_vehicleWheels) {
        auto& wheel=m_wheels.at(id);
        wheel.PrepareInterval(m_physics,RuntimeBody(WheelChassisEntity(wheel.settings)),dt);
    }
}

void RuntimeWorld::SolveWheelInterval() {
    JUDAS_PROFILE_SCOPE("Vehicle suspension and tyres");
    for(auto id:m_vehicleWheels)m_wheels.at(id).SolveIteration(m_physics);
    for(auto it=m_vehicleWheels.rbegin();it!=m_vehicleWheels.rend();++it)
        m_wheels.at(*it).SolveIteration(m_physics);
}

void RuntimeWorld::FinishWheelInterval() {
    std::vector<PhysicsWorld::ConstraintSupport> support;
    support.reserve(m_vehicleWheels.size());
    for(auto id:m_vehicleWheels) {
        auto& wheel=m_wheels.at(id);
        wheel.FinishInterval(m_physics);
        if(wheel.HasSleepSupport())support.push_back({
            RuntimeBody(WheelChassisEntity(wheel.settings)),wheel.state.body,
            wheel.QuietSupport(m_physics)});
    }
    m_physics.SetConstraintSupports(std::move(support));
}

void RuntimeWorld::BeginVehicleStep(float dt) {
    m_vehicleStep=dt;m_vehicleClock=0;m_vehicleNext=0;
    m_vehicleInterval=0;m_vehicleIntervalOpen=false;
    for(auto id:m_wheelOwners)if(auto* wheel=RuntimeWheel(id))wheel->previous=wheel->state;
}

void RuntimeWorld::SolveVehicleSubstep(const PhysicsWorld::VelocityCoupling::Substep& context) {
    if(context.iteration>=4)return;
    if(!m_vehicleIntervalOpen) {
        // Vehicle time belongs to the fixed step and native chronology, not
        // to an unrelated finite material's extra structural subdivisions.
        m_vehicleInterval=m_vehicleStep/float(context.count);
        m_vehicleNext=m_vehicleClock+m_vehicleInterval;
        UpdatePowertrains(m_vehicleInterval);
        PrepareWheelInterval(m_vehicleInterval);
        m_vehicleIntervalOpen=true;
    }
    SolveWheelInterval();
    ResolvePowertrainLoads(m_vehicleInterval);
}

void RuntimeWorld::CompleteVehicleSubstep(const PhysicsWorld::VelocityCoupling::Substep& context) {
    m_vehicleClock+=context.deltaTime;
    if(m_vehicleIntervalOpen&&m_vehicleClock+1e-6*m_vehicleStep>=m_vehicleNext) {
        FinishWheelInterval();m_vehicleIntervalOpen=false;
    }
}
