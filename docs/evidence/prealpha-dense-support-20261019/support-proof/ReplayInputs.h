// Isolated Linux-only replay of exact recorded native query inputs.
#include <bits/stdc++.h>
#define private public
#include "WorldInternal.h"
#undef private
#define main JudasOriginalContactReplayMain
#include "../contact-system-research/Replay.cpp"
#undef main

namespace {
RigidMotionSegment MotionSegment(const Json& j) {
    RigidMotionSegment s;
    s.owner=j.at("owner");s.begin=Double(j.at("begin"));s.end=Double(j.at("end"));
    s.inverseMass=Float(j.at("inverse_mass"));
    s.inverseInertiaLocal=Matrix(j.at("inverse_inertia_local_columns"));
    s.position=Vector(j.at("position"));s.endPosition=Vector(j.at("end_position"));
    auto q=[](const Json& v) {return glm::quat(Float(v[0]),Float(v[1]),Float(v[2]),Float(v[3]));};
    s.orientation=q(j.at("orientation_wxyz"));s.endOrientation=q(j.at("end_orientation_wxyz"));
    s.linearVelocity=Vector(j.at("linear_velocity"));s.angularVelocity=Vector(j.at("angular_velocity"));
    s.driftAngularVelocity=Vector(j.at("drift_angular_velocity"));s.prescribed=j.at("prescribed");
    return s;
}
void Assign(PhysicsWorld::Impl::Body& body,const Json& j) {
    body.rigidBody=Body(j);
    const auto& shape=j.at("shape");
    if (shape.at("asset_present").get<bool>() || shape.at("terrain_present").get<bool>() ||
        shape.at("compound_children").get<std::size_t>()!=0)
        throw std::runtime_error("recorded external/compound geometry is unavailable in primitive replay");
    body.shape.type=ShapeType(shape.at("type").get<int>());
    body.shape.radius=Float(shape.at("radius"));body.shape.halfHeight=Float(shape.at("half_height"));
    body.shape.halfExtents=Vector(shape.at("half_extents"));body.shape.pivotOffset=Vector(shape.at("pivot_offset"));
    body.boundingRadius=Float(j.at("bounding_radius"));
    body.isDynamic=j.at("dynamic");body.isKinematic=j.at("kinematic");
    body.simulationSuspended=j.at("simulation_suspended");body.sleeping=j.at("sleeping");
    body.enabled=j.at("enabled");body.sensor=j.at("sensor");body.queryOnly=j.at("query_only");
    body.generation=j.at("generation");body.alive=true;
    std::vector<RigidMotionSegment> segments;
    for (const auto& s:j.at("motion")) segments.push_back(MotionSegment(s));
    body.motion.SetRecorded(std::move(segments));
}
Json Pose(const RigidBody& body) {
    return {{"position_bits",contact_capture::Bits(body.position)},
        {"orientation_bits_wxyz",contact_capture::Bits(body.orientation)},
        {"orientation_norm_squared",double(glm::dot(body.orientation,body.orientation))}};
}
Json Probe(PhysicsWorld::Impl& world,const std::array<unsigned,4>& key,double time) {
    const auto a=world.GeometryPoseAt(key[0],time),b=world.GeometryPoseAt(key[1],time);
    const auto actual=world.PairAt(key[0],key[1],int(key[2]),int(key[3]),time,0);
    const auto near=world.PairAt(key[0],key[1],int(key[2]),int(key[3]),time,.01f);
    Json out={{"time",time},{"time_bits",contact_capture::Bits(time)},
        {"a",Pose(a)},{"b",Pose(b)},
        {"actual_rows",actual.count},{"actual_uncertain",actual.uncertain},
        {"touching_boolean",world.PairTouches(key[0],key[1],int(key[2]),int(key[3]),time)},
        {"proximity_rows_1cm",near.count}};
    if (near.count) {
        double smallest=std::numeric_limits<double>::infinity();
        for (int i=0;i<near.count;++i) smallest=std::min(smallest,near.points[i].signedSeparation);
        out["minimum_signed_separation_m"]=smallest;
        out["minimum_signed_separation_bits"]=contact_capture::Bits(smallest);
        out["normal_bits"]=contact_capture::Bits(near.points[0].preciseNormal);
    }
    out["native_quaternion_evaluation"]=Json::array();
    for (auto slot:{key[0],key[1]}) {
        const auto& segments=world.bodies[slot].motion.Segments();
        auto it=std::upper_bound(segments.begin(),segments.end(),time,
            [](double t,const auto& s){return t<s.begin;});
        if (it==segments.begin()) continue;
        const auto& s=*--it;
        const float elapsed=float(time-s.begin);
        const glm::quat omega(0,s.driftAngularVelocity.x,s.driftAngularVelocity.y,s.driftAngularVelocity.z);
        const auto raw=s.orientation+(omega*s.orientation)*(.5f*elapsed);
        out["native_quaternion_evaluation"].push_back({{"slot",slot},
            {"segment_begin",s.begin},{"elapsed_float",elapsed},
            {"elapsed_bits",contact_capture::Bits(elapsed)},
            {"anchor_bits_wxyz",contact_capture::Bits(s.orientation)},
            {"raw_advanced_bits_wxyz",contact_capture::Bits(raw)},
            {"raw_norm_squared",double(glm::dot(raw,raw))}});
    }
    return out;
}
void AddFloatElapsedNeighbours(std::set<double>& times,const RigidMotion& motion,
                                double at,double lower,double upper) {
    const auto& segments=motion.Segments();
    auto it=std::upper_bound(segments.begin(),segments.end(),at,[](double t,const auto& s){return t<s.begin;});
    if (it==segments.begin()) return;
    --it;
    const float elapsed=float(at-it->begin);
    for (int offset=-2;offset<=2;++offset) {
        float shifted=elapsed;
        for (int i=0;i<std::abs(offset);++i)
            shifted=std::nextafter(shifted,offset<0?-std::numeric_limits<float>::infinity():std::numeric_limits<float>::infinity());
        const double t=it->begin+double(shifted);
        if (t>=lower && t<=upper) times.insert(t);
    }
}
}

