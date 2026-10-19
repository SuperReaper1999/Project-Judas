#pragma once

#include "ContactCapture.h"

namespace event_topology {
inline bool SelectedTrace(const Key& key) {
    const char* path=std::getenv("JUDAS_TOI_PAIR_TRACE_OUTPUT");
    const char* selection=std::getenv("JUDAS_TOI_PAIR_TRACE_PAIR");
    const char* first=std::getenv("JUDAS_TOI_PAIR_TRACE_FIRST_STEP");
    if (!path || !selection || !first || Global().step<std::strtoull(first,nullptr,10)) return false;
    char* end=nullptr;
    const auto a=std::strtoul(selection,&end,10);
    if (!end || *end!=',') return false;
    const auto b=std::strtoul(end+1,nullptr,10);
    return key[0]==a && key[1]==b && key[2]==0 && key[3]==0;
}
inline contact_capture::Json Segment(const RigidMotionSegment& s) {
    using contact_capture::Bits;
    return {{"owner",s.owner},{"begin",Bits(s.begin)},{"end",Bits(s.end)},
        {"inverse_mass",Bits(s.inverseMass)},{"inverse_inertia_local_columns",Bits(s.inverseInertiaLocal)},
        {"position",Bits(s.position)},{"orientation_wxyz",Bits(s.orientation)},
        {"linear_velocity",Bits(s.linearVelocity)},{"angular_velocity",Bits(s.angularVelocity)},
        {"drift_angular_velocity",Bits(s.driftAngularVelocity)},
        {"end_position",Bits(s.endPosition)},{"end_orientation_wxyz",Bits(s.endOrientation)},
        {"prescribed",s.prescribed}};
}
template<class Body>
inline contact_capture::Json TraceBody(const Body& b) {
    using contact_capture::Bits;
    auto result=contact_capture::Body(b.rigidBody);
    result["shape"]={{"type",int(b.shape.type)},{"radius",Bits(b.shape.radius)},
        {"half_height",Bits(b.shape.halfHeight)},{"half_extents",Bits(b.shape.halfExtents)},
        {"pivot_offset",Bits(b.shape.pivotOffset)},{"asset_present",bool(b.shape.asset)},
        {"terrain_present",bool(b.shape.terrain)},{"compound_children",b.shape.boxes.size()}};
    result["bounding_radius"]=Bits(b.boundingRadius);
    result["dynamic"]=b.isDynamic;result["kinematic"]=b.isKinematic;
    result["simulation_suspended"]=b.simulationSuspended;
    result["sleeping"]=b.sleeping;result["enabled"]=b.enabled;
    result["sensor"]=b.sensor;result["query_only"]=b.queryOnly;
    result["generation"]=b.generation;
    result["motion"]=contact_capture::Json::array();
    for (const auto& segment:b.motion.Segments()) result["motion"].push_back(Segment(segment));
    return result;
}
template<class World>
inline void TracePair(const World& world,const Key& key,double now,double finish,
                       double resultTime,double target,const char* stage,bool queryExecuted=true) {
    if (!SelectedTrace(key)) return;
    using contact_capture::Bits;
    using contact_capture::Json;
    Json output={{"schema","judas.pair-query-bits.v1"},{"stage",stage},
        {"step",Global().step},{"after_global_impact_sequence",Global().globalImpacts},
        {"key",key},{"query_executed",queryExecuted},{"start",Bits(now)},
        {"finish",Bits(finish)},{"step_duration",Bits(world.stepDuration)},
        {"result_time",Bits(resultTime)},{"target",Bits(target)},
        {"body_a",TraceBody(world.bodies[key[0]])},{"body_b",TraceBody(world.bodies[key[1]])}};
    std::ofstream out(std::getenv("JUDAS_TOI_PAIR_TRACE_OUTPUT"),std::ios::app);
    if (!out) throw std::runtime_error("pair schedule trace output unavailable");
    out<<output.dump()<<'\n';
}
} // namespace event_topology
