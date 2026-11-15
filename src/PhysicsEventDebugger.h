#pragma once
#include "PhysicsWorld.h"
#include "InputSystem.h"
#include "PhysicsDebugClock.h"
#include <deque>
#include <array>
#include <string>
class RuntimeWorld;
// Diagnostic controller, not a solver. All callbacks run synchronously at
// joined owner boundaries. Re-entry/mutation is forbidden while in flight.
class PhysicsEventDebugger {
public:
    enum class Command { Hold, Fixed, Event, Phase, Arm, Realtime, Pause, Checkpoint, Restore, Detach };
    struct Material {
        std::uint64_t owner=0;std::size_t nodes=0,triangles=0,tets=0,hinges=0,contacts=0,candidates=0,broken=0,released=0;
        unsigned iterations=0,substeps=0;double strain=0,plasticWork=0;bool sleeping=false;
        std::vector<glm::dvec3> positions;
    };
    struct Snapshot {
        PhysicsWorld::DebugBoundary boundary;
        std::vector<PhysicsWorld::DebugBody> bodies;
        std::vector<Material> materials;
        std::size_t fixedSteps=0,omittedRecords=0;
        double activeMilliseconds=0;
        bool inFlight=false;
    };
    using Handler=std::function<Command(const Snapshot&)>;
    explicit PhysicsEventDebugger(std::string output={});
    ~PhysicsEventDebugger();
    void Bind(RuntimeWorld&);
    void Unbind();
    void SetHandler(Handler handler){m_handler=std::move(handler);}
    void Request(Command);
    bool Detached()const{return m_mode==Command::Detach;}
    bool Manual()const{return m_mode!=Command::Arm&&m_mode!=Command::Realtime&&m_mode!=Command::Detach;}
    bool BeginFixed(const InputSystem&);
    void EndFixed();
    void PresentRoot();
    const InputSystem& FixedInput()const{return m_input;}
    bool ConsumeClockReset(){bool value=m_clockReset;m_clockReset=false;return value;}
    bool ConsumeCheckpoint(){bool value=m_checkpoint;m_checkpoint=false;return value;}
    bool ConsumeRestore(){bool value=m_restore;m_restore=false;return value;}
    const Snapshot& Current()const{return m_snapshot;}
    Command Mode()const{return m_mode;}
    void Export()const;
    std::size_t Count(const std::string& phase)const;
    static const char* PhaseName(PhysicsWorld::DebugPhase);
private:
    RuntimeWorld* m_world=nullptr;Handler m_handler;Command m_mode=Command::Pause;
    InputSystem m_input;bool m_haveInput=false,m_inFixed=false,m_clockReset=false,m_checkpoint=false,m_restore=false;
    std::array<std::size_t,64> m_counts{};
    std::uint64_t m_worldEpoch=0;
    std::size_t m_fixed=0,m_omitted=0;std::string m_output;
    std::deque<PhysicsWorld::DebugBoundary> m_trace;Snapshot m_snapshot;
    PhysicsDebugClock::time_point m_lastActive{};
    void Observe(const PhysicsWorld::DebugBoundary&);
};
