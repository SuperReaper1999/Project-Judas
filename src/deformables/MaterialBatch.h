#pragma once
#include "Deformable.h"
#include "MaterialInterval.h"
#include "JobSystem.h"
#include "PerformanceProfiler.h"
#include <memory>
#include <utility>
#include <type_traits>
#include <vector>

namespace JudasDeformables {
// The owner temporarily transfers exclusive numerical state into batch data.
// Helpers cannot reach RuntimeWorld/PhysicsWorld and never publish results.
// All results are restored in material order after joining, including failures.
inline void IndependentMaterialPhase(const std::vector<MaterialInterval*>& intervals,
                                     MaterialInterval::Independent phase,JobSystem* jobs,unsigned lanes){
    if(intervals.empty())return;
    // Some standard libraries allocate a map sentinel when moving the instance.
    // Keep the same ordered kernels on the owner rather than risking allocation
    // during a supposedly no-throw lease transfer. Numerical scheduling is unchanged.
    constexpr bool transferSafe=std::is_nothrow_move_constructible<DeformableInstance>::value&&
                                std::is_nothrow_move_assignable<DeformableInstance>::value;
    if(!transferSafe||!jobs||lanes==1||intervals.size()<8){
        for(auto* interval:intervals)interval->RunIndependent(phase,interval->State());
        return;
    }
    struct Lease {
        MaterialInterval* interval=nullptr;
        DeformableInstance data;
        explicit Lease(MaterialInterval* value):interval(value),data(std::move(value->State())){}
        Lease(Lease&& other)noexcept:interval(std::exchange(other.interval,nullptr)),data(std::move(other.data)){}
        Lease(const Lease&)=delete;
        void Publish(){if(interval){interval->State()=std::move(data);interval=nullptr;}}
        ~Lease(){Publish();}
    };
    const auto started=PerformanceProfiler::Get().Active()?PerformanceProfiler::Now():0;
    std::vector<Lease> work;work.reserve(intervals.size());for(auto* interval:intervals)work.emplace_back(interval);
    try{
        const auto result=jobs->ParallelFor(work.size(),1,lanes,[&](size_t i){work[i].interval->RunIndependent(phase,work[i].data);},"Independent material kernels");
        for(auto& item:work)item.Publish();
        JUDAS_PROFILE_COUNTER("Material worker items",double(result.workerItems),ProfileCounterMode::Sum);
        JUDAS_PROFILE_COUNTER("Material owner items",double(result.ownerItems),ProfileCounterMode::Sum);
    }catch(...){for(auto& item:work)item.Publish();throw;}
    if(started)JUDAS_PROFILE_COUNTER("Independent material batch wall ms",double(PerformanceProfiler::Now()-started)*1e-6,ProfileCounterMode::Sum);
}
}
