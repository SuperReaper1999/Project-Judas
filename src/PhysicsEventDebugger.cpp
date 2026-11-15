#include "PhysicsEventDebugger.h"
#include "RuntimeWorld.h"
#include "NetworkSession.h"
#include "PerformanceProfiler.h"
#include "PlatformServices.h"
#include "../third_party/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <stdexcept>
using Phase=PhysicsWorld::DebugPhase;
const char* PhysicsEventDebugger::PhaseName(Phase phase){
    switch(phase){
#define PHASE(x) case Phase::x:return #x;
        PHASE(FixedBegin) PHASE(Candidates) PHASE(MaterialBegin) PHASE(MaterialEnd)
        PHASE(SearchCompleted) PHASE(PredictionDiscarded) PHASE(ToiSelected)
        PHASE(ResponsePrepared) PHASE(BeforeImpact)
        PHASE(TrajectoriesPublished) PHASE(DeformableBegin) PHASE(DeformableEnd) PHASE(FixedEnd)
#undef PHASE
    }return "Unknown";
}
PhysicsEventDebugger::PhysicsEventDebugger(std::string output):m_output(std::move(output)){}
PhysicsEventDebugger::~PhysicsEventDebugger(){Unbind();}
void PhysicsEventDebugger::Bind(RuntimeWorld& world){
    if(m_world==&world)return;
    Unbind();++m_worldEpoch;m_world=&world;m_inFixed=false;m_haveInput=false;m_trace.clear();m_omitted=0;m_lastActive=PhysicsDebugClock::now();
    world.Physics().SetDebugObserver([this](const auto& event){Observe(event);});
}
void PhysicsEventDebugger::Unbind(){if(m_world){m_world->Physics().SetDebugObserver({});m_world=nullptr;}}
void PhysicsEventDebugger::Request(Command command){
    if((command==Command::Checkpoint||command==Command::Restore)&&m_inFixed)
        throw std::logic_error("checkpoint/restore requires a completed fixed boundary");
    if(command==Command::Checkpoint){m_checkpoint=true;m_mode=Command::Pause;}
    else if(command==Command::Restore){m_restore=true;m_mode=Command::Pause;}
    else if(command!=Command::Hold)m_mode=command;
    m_clockReset=true;
}
bool PhysicsEventDebugger::BeginFixed(const InputSystem& input){
    if(!m_world)throw std::logic_error("unbound physics debugger");
    if(auto* network=m_world->NetworkIfLoaded();network&&network->Active())throw std::logic_error("physics event debugger requires an offline authority");
    m_inFixed=false;
    const bool wasManual=Manual();
    if(!m_haveInput||!wasManual){m_input=input;m_haveInput=true;}
    m_world->Physics().DebugPoint(Phase::FixedBegin);
    if(m_mode==Command::Pause||m_mode==Command::Hold||m_mode==Command::Detach)return false;
    // Arming from a paused root returns to the outer service/input boundary.
    if(wasManual&&!Manual())return false;
    if(Manual())m_input.BeginFixedStep();
    const auto begin=m_trace.back();m_trace.clear();m_trace.push_back(begin);m_omitted=0;m_inFixed=true;m_lastActive=PhysicsDebugClock::now();
    return true;
}
void PhysicsEventDebugger::EndFixed(){
    m_inFixed=false;++m_fixed;
    if(m_mode==Command::Fixed||m_mode==Command::Phase)m_mode=Command::Pause;
    m_world->Physics().DebugPoint(Phase::FixedEnd);
}
void PhysicsEventDebugger::Observe(const PhysicsWorld::DebugBoundary& point){
    const bool root=!m_inFixed;
    const bool repeatedRoot=root&&point.phase==Phase::FixedBegin&&!m_trace.empty()&&m_trace.back().phase==Phase::FixedBegin;
    if(!repeatedRoot)++m_counts[unsigned(point.phase)];
    constexpr std::size_t maximum=8192;
    if(!repeatedRoot){
        if(m_trace.size()==maximum){m_trace.pop_front();++m_omitted;}
        m_trace.push_back(point);
        if(m_trace.back().manifold.size()>256)m_trace.back().manifold.resize(256);
    }
    const bool stop=(root&&m_mode==Command::Pause&&point.phase==Phase::FixedBegin)||
        ((m_mode==Command::Event||m_mode==Command::Arm)&&point.phase==Phase::BeforeImpact)||
        (m_inFixed&&m_mode==Command::Phase&&point.phase!=Phase::SearchCompleted&&point.phase!=Phase::PredictionDiscarded);
    if(!stop)return;
    if(!repeatedRoot){
    m_snapshot.boundary=point;m_snapshot.bodies=m_world->Physics().CaptureDebugBodies(point.localTime);
    m_snapshot.materials=m_world->DebugDeformables();m_snapshot.inFlight=m_inFixed;
    m_snapshot.fixedSteps=m_fixed;m_snapshot.omittedRecords=m_omitted;
    const auto activeAt=PhysicsDebugClock::time_point(std::chrono::duration_cast<PhysicsDebugClock::base::duration>(std::chrono::nanoseconds(point.activeClockNanoseconds)));
    m_snapshot.activeMilliseconds=std::chrono::duration<double,std::milli>(activeAt-m_lastActive).count();
    m_lastActive=activeAt;
    Export();
    }
    if(!m_handler){if(m_inFixed)throw std::logic_error("in-flight breakpoint requires a handler");m_mode=Command::Pause;return;}
    const auto command=m_handler(m_snapshot);
    if((command==Command::Hold||command==Command::Pause)&&m_inFixed)throw std::logic_error("in-flight debugger must explicitly resume its stack");
    Request(command);
}
void PhysicsEventDebugger::PresentRoot(){
    if(!m_world||!m_handler||Detached())return;
    auto point=m_trace.empty()?m_snapshot.boundary:m_trace.back();point.phase=Phase::FixedEnd;point.localTime=point.macroTime=-1;point.stats=m_world->Physics().LastStepStats();
    m_snapshot.boundary=point;m_snapshot.bodies=m_world->Physics().CaptureDebugBodies();m_snapshot.materials=m_world->DebugDeformables();m_snapshot.inFlight=false;m_snapshot.fixedSteps=m_fixed;m_snapshot.omittedRecords=m_omitted;
    Export();
    const auto command=m_handler(m_snapshot);Request(command);
}

namespace {
using Json=nlohmann::json;
template<class V> Json Vector(const V& v){return Json::array({v.x,v.y,v.z});}
Json Boundary(const PhysicsWorld::DebugBoundary& p){
    Json result={{"phase",PhysicsEventDebugger::PhaseName(p.phase)},{"sequence",p.sequence},{"event",p.event},{"invalidated_predictions",p.invalidated},{"time_group",p.timeGroup},{"local_seconds",p.localTime},{"macro_seconds",p.macroTime},{"interval",p.interval},{"a",p.a},{"b",p.b},{"part_a",p.partA},{"part_b",p.partB},{"revision_a",p.revisionA},{"revision_b",p.revisionB},{"prediction_version",p.predictionVersion},{"candidates",p.candidates},{"contact_rows",p.contacts},{"participants",p.participants},{"coupled",p.coupled},{"applied",p.applied},{"queued_pairs",p.queuedPairs},{"geometry_requests",p.geometryRequests},{"geometry_hits",p.geometryHits},{"impact_queries",p.stats.impactQueries},{"sampling_tests",p.stats.impactSamplingTests},{"sampling_fallbacks",p.stats.impactSamplingFallbacks},{"responses_applied",p.stats.impactResponsesApplied},{"holds",p.stats.impactSafetyFallback},{"completed_search_ms",p.searchMs},{"completed_sampling_ms",p.samplingMs},{"completed_contact_solve_ms",p.contactSolveMs},{"manifold",Json::array()}};
    for(const auto& c:p.manifold)result["manifold"].push_back({{"point",Vector(c.point)},{"normal",Vector(c.normal)},{"penetration",c.penetration}});
    if(p.hasSearch){const auto& s=p.search;result["search"]={{"found",s.hit<s.finish},{"type_a",int(s.typeA)},{"type_b",int(s.typeB)},{"start",s.start},{"end",s.finish},{"hit",s.hit},{"milliseconds",s.milliseconds},{"queries",s.queries},{"iterations",s.iterations},{"sampling_tests",s.samplingTests},{"fallbacks",s.fallbacks},{"cache_hit",s.cacheHit},{"geometry_requests",s.geometryRequests}};}
    return result;
}
}
void PhysicsEventDebugger::Export()const{
    if(m_output.empty())return;
    Json result={{"schema",1},{"world_epoch",m_worldEpoch},{"fixed_steps_completed",m_fixed},{"in_flight",m_inFixed},{"omitted_records",m_omitted},{"boundary",Boundary(m_snapshot.boundary)},{"active_wall_ms_since_stop_excluding_inspection",m_snapshot.activeMilliseconds},{"bodies",Json::array()},{"materials",Json::array()},{"trace",Json::array()},{"timing_note","CPU work timers exclude synchronous inspector work. Raw profiler timestamps retain explicit debugger waits. Completed-category timers can be pending inside an open phase; categories overlap."}};
    for(const auto& b:m_snapshot.bodies)result["bodies"].push_back({{"body",b.body.id},{"position",Vector(b.position)},{"rotation",Json::array({b.rotation.w,b.rotation.x,b.rotation.y,b.rotation.z})},{"velocity",Vector(b.velocity)},{"enclosure_half",Vector(b.halfExtents)},{"sleeping",b.sleeping},{"dynamic",b.dynamic}});
    for(const auto& m:m_snapshot.materials){Json row={{"owner",m.owner},{"nodes",m.nodes},{"triangles",m.triangles},{"tets",m.tets},{"hinges",m.hinges},{"contacts",m.contacts},{"candidates",m.candidates},{"broken",m.broken},{"released",m.released},{"iterations",m.iterations},{"substeps",m.substeps},{"maximum_strain",m.strain},{"plastic_work",m.plasticWork},{"sleeping",m.sleeping},{"positions",Json::array()}};for(auto p:m.positions)row["positions"].push_back(Vector(p));row["positions_truncated"]=m.positions.size()!=m.nodes;result["materials"].push_back(std::move(row));}
    for(const auto& p:m_trace)result["trace"].push_back(Boundary(p));
    const auto path=std::filesystem::path(m_output);if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    const auto temporary=path.string()+".tmp";
    {std::ofstream file(temporary);file<<result.dump(2);file.close();if(!file)throw std::runtime_error("physics debugger report write failed: "+m_output);}
    ReplaceStagedFile(temporary,path);
    std::string profileError;if(!PerformanceProfiler::Get().Export(m_output+".profiler.json",profileError))throw std::runtime_error(profileError);
}

std::size_t PhysicsEventDebugger::Count(const std::string& phase)const{
    for(unsigned i=0;i<=unsigned(Phase::FixedEnd);++i)if(phase==PhaseName(Phase(i)))return m_counts[i];
    throw std::invalid_argument("unknown physics debug phase: "+phase);
}
