// Exact event-time geometry and isolated CCD query-batch controls.
// Private access stays in this fixture; no public test API or solver tuning.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cfenv>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#include "physics/WorldIncrementalImpact.h"
#undef private


namespace {
unsigned checks=0, failures=0;
void Check(bool condition,const char* label) {
    ++checks; failures+=!condition;
    std::printf("%s %s\n",condition?"PASS":"FAIL",label);
}
void Seed(PhysicsWorld::Impl::IncrementalImpact& storage) {
    storage.Reset(2);
    auto& a=storage.bodies[0];
    auto& b=storage.bodies[1];
    a.inverseMass=1;
    a.inverseInertiaLocal=glm::mat3(1);
    a.linearVelocity={-.1f,0,0};
    Contact geometry;
    geometry.hit=true;
    geometry.normal={1,0,0};
    geometry.point={0,0,0};
    geometry.penetration=0;
    storage.rows.push_back({&a,&b,geometry,.5f,0,false});
    storage.keys.push_back({0,1,1,1,0,0});
    storage.pairs.push_back({0,1,0,0});
    storage.slots={0,1};
    storage.forcedExpansion={1};
    storage.initialized={1,1};
    storage.expanded={1,1};
    storage.gravityConsumed={1,1};
    Check(storage.Visit(0,1),"first event pair is new");
    Check(!storage.Visit(0,1),"same event pair is visited");
    storage.capture=true;
    storage.result.contacts=1;
    storage.solver.m_solver.AddContact(a,b,geometry,.5f,0);
    storage.solver.m_solver.Prepare(0);
    storage.solver.m_saved.push_back({&a,a.linearVelocity,a.angularVelocity});
    Check(storage.solver.Constraints().size()==1 &&
          storage.solver.m_solver.FrameNodeRequests()==2,
          "old transaction owns actual prepared rows and two body frames");
}
void ExerciseReset() {
    PhysicsWorld::Impl::IncrementalImpact storage;
    Seed(storage);
    const auto rowCapacity=storage.rows.capacity();
    const auto frameStorage=storage.solver.m_solver.FrameStorageBytes();
    const auto oldBodyAddress=reinterpret_cast<std::uintptr_t>(storage.bodies.data());
    storage.Reset(4096);
    Check(reinterpret_cast<std::uintptr_t>(storage.bodies.data())!=oldBodyAddress,
          "forced growth really relocates the old body backing storage");
    Check(storage.solver.Constraints().empty() && storage.solver.m_saved.empty() &&
          storage.solver.m_solver.FrameNodeRequests()==0 &&
          storage.solver.m_solver.m_frameIndex.empty(),
          "prepared body views and saved pointers are discarded on growth");
    Check(storage.rows.empty() && storage.keys.empty() && storage.pairs.empty() &&
          storage.slots.empty() && storage.forcedExpansion.empty(),
          "every logical domain/contact list is rebuilt");
    Check(storage.rows.capacity()==rowCapacity &&
          storage.solver.m_solver.FrameStorageBytes()>=frameStorage,
          "row and solver pool capacities survive reset");
    Check(!storage.capture && storage.result.contacts==0 &&
          std::all_of(storage.initialized.begin(),storage.initialized.end(),
                      [](unsigned char flag){return flag==0;}) &&
          std::all_of(storage.expanded.begin(),storage.expanded.end(),
                      [](unsigned char flag){return flag==0;}) &&
          std::all_of(storage.gravityConsumed.begin(),storage.gravityConsumed.end(),
                      [](unsigned char flag){return flag==0;}),
          "old masks, physical result and support policy do not survive");
    Check(storage.Visit(0,1),"old pair identity is new in the next transaction");
    const auto bodyCapacity=storage.bodies.capacity();
    Seed(storage);
    Check(storage.bodies.capacity()==bodyCapacity,
          "smaller scene reuses allocated body capacity without retaining membership");
    storage.Reset(0);
    Check(storage.rows.empty() && storage.bodies.empty() && storage.incoming.empty() &&
          storage.initialized.empty() && storage.solver.Constraints().empty(),
          "empty world reset has no surviving prepared physical state");
}
void ExerciseOwner() {
    PhysicsWorld first,second;
    first.Init(); second.Init();
    Check(!first.m_impl->incrementalImpact && !second.m_impl->incrementalImpact,
          "worlds do not allocate optional impact workspace at initialization");
    first.Step(1.f/60);
    Check(!first.m_impl->incrementalImpact,
          "empty free-flight step does not allocate impact workspace");
    first.m_impl->incrementalImpact.reset(new PhysicsWorld::Impl::IncrementalImpact());
    Seed(*first.m_impl->incrementalImpact);
    Check(!second.m_impl->incrementalImpact,
          "workspace ownership is isolated between worlds");
    first.Shutdown();
    Check(first.m_impl==nullptr && second.m_impl!=nullptr,
          "owner shutdown destroys its workspace without affecting another world");
    first.Init();
    Check(!first.m_impl->incrementalImpact,
          "new world lifetime starts without historical physical transaction data");
}
}
int main() {
    ExerciseReset(); ExerciseOwner();
    std::printf("reused impact workspace: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
