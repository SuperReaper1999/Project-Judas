#pragma once
#include "RigidBody.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <stdexcept>

// Geometry observations intentionally omit transported physical angular
// velocity. Collision sampling needs the exact represented pose and linear
// drift; only solver/event publication needs the inertia-dependent transport.
struct RigidMotionGeometry {
    glm::vec3 position{0},linearVelocity{0};
    glm::quat orientation{1,0,0,0};
};

// Step-local authoritative motion. Observations never restart the integrator.
// The owner is the full slot/generation handle, not a pointer into body storage.
struct RigidMotionSegment {
    unsigned int owner = 0;
    double begin = 0, end = 0;
    float inverseMass = 0;
    glm::mat3 inverseInertiaLocal{0};
    glm::vec3 position{0}, linearVelocity{0}, angularVelocity{0},driftAngularVelocity{0};
    glm::quat orientation{1,0,0,0};
    glm::vec3 endPosition{0};
    glm::quat endOrientation{1,0,0,0};
    bool prescribed=false;
    // Rotation leaves and solver intervals need not restart unchanged linear
    // flight. This anchor belongs to a velocity episode, in the ledger clock.
    glm::vec3 linearProjection{0}, angularProjection{0};
    bool hasPhysicalLinearVelocity = false;
    glm::vec3 physicalLinearVelocity{0};
    bool anchoredTranslation = false;
    glm::vec3 translationOrigin{0};
    double translationTime = 0;
    // This leaf is an analytic principal-axis rotation. General dynamic
    // leaves use the Cayley map matching their implicit midpoint predictor.
    bool exactRotation=false;
    float planarInverseIz=0; glm::vec3 planeNormal{0};
    RigidMotionGeometry EvaluateGeometry(double time) const {
        if (time < begin || time > end) throw std::out_of_range("motion segment time");
        RigidMotionGeometry result;
        result.position = position; result.orientation = orientation;
        result.linearVelocity = linearVelocity;
        // Keep the integrator's arithmetic for represented movement, but an
        // unchanged angular sample must preserve its anchor. Re-normalizing a
        // binary32 quaternion at zero/sub-ULP drift can alternate between two
        // values and fabricate repeated detach/impact events in a contact island.
        // This is an exact bit test, not an angular tolerance or a CCD time cap.
        if (prescribed || !(inverseMass <= 0.0f)) {
            const float elapsed = static_cast<float>(time - begin);
            result.position = anchoredTranslation
                ? translationOrigin + result.linearVelocity * float(time - translationTime)
                : position + result.linearVelocity * elapsed;
            const glm::quat omega(0.0f, driftAngularVelocity.x, driftAngularVelocity.y, driftAngularVelocity.z);
            // Prescribed angular velocity is the actual constant world rate.
            // Dynamic drift uses the midpoint angular predictor fixed at the
            // segment anchor; observations never re-evaluate the predictor.
            const float speed=glm::length(driftAngularVelocity);
            const glm::quat advanced = prescribed || exactRotation
                ? (speed>0 && elapsed!=0 ? glm::angleAxis(speed*elapsed,driftAngularVelocity/speed)*orientation : orientation)
                : orientation + (omega * orientation) * (0.5f * elapsed);
            const auto same = [](float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; };
            if (!same(advanced.w, orientation.w) || !same(advanced.x, orientation.x) ||
                !same(advanced.y, orientation.y) || !same(advanced.z, orientation.z))
                result.orientation = glm::normalize(advanced);
        }
        return result;
    }
    RigidBody Evaluate(double time) const {
        const auto geometry=EvaluateGeometry(time);
        RigidBody result;
        result.planarInverseIz=planarInverseIz;result.planeNormal=planeNormal;
        result.inverseMass=inverseMass;result.inverseInertiaLocal=inverseInertiaLocal;
        result.position=geometry.position;result.orientation=geometry.orientation;
        result.linearVelocity=hasPhysicalLinearVelocity ? physicalLinearVelocity : geometry.linearVelocity;
        result.angularVelocity=angularVelocity;
        if(!prescribed&&!result.IsStatic()&&!result.IsPlanar())
            result.angularVelocity=TransportAngularVelocity(inverseInertiaLocal,orientation,result.orientation,angularVelocity);
        return result;
    }

};
// Append an anchored, error-controlled torque-free trajectory. Every dynamic
// leaf uses its matching Cayley midpoint; principal-axis drift remains analytic.
void AppendDynamicRigidMotion(std::vector<RigidMotionSegment>& segments,
    unsigned owner, const RigidBody& body, double begin, double end);

class RigidMotion {
public:
    void Begin(unsigned int owner, const RigidBody& body, double duration, bool exactRotation=false) {
        segments.clear(); Append(owner, body, 0, duration, false, exactRotation);
    }
    void BeginPrescribed(unsigned int owner,const RigidBody& body,double duration,double activeDuration) {
        segments.clear();
        Append(owner,body,0,activeDuration,true);
        if(activeDuration<duration){auto rest=segments.back().Evaluate(activeDuration);
            rest.linearVelocity=rest.angularVelocity=glm::vec3(0);
            Append(owner,rest,activeDuration,duration,true);}
    }
    RigidBody Evaluate(double time) const {
        // ChangeVelocity only splits the last segment, so end times stay
        // ordered. Find the earliest segment whose end includes the query:
        // at a shared boundary this preserves the original ledger's choice
        // of the pre-change velocity, including zero-length segments.
        const auto segment = std::lower_bound(segments.begin(), segments.end(), time,
            [](const RigidMotionSegment& value, double query) { return value.end < query; });
        if (segment != segments.end() && time >= segment->begin && time <= segment->end)
            return segment->Evaluate(time);
        throw std::out_of_range("motion ledger time");
    }
    // Physical solver state is the right limit after impulses at a shared time.
    RigidBody EvaluateAfterEvents(double time) const {
        auto it=std::upper_bound(segments.begin(),segments.end(),time,
            [](double query,const RigidMotionSegment& s){return query<s.begin;});
        if(it==segments.begin())throw std::out_of_range("motion ledger time");
        --it;
        if(time>=it->begin && time<=it->end)return it->Evaluate(time);
        throw std::out_of_range("motion ledger time");
    }
    // The same right-limit segment selection as EvaluateAfterEvents, without
    // computing a physical angular velocity that geometry callers never read.
    RigidMotionGeometry EvaluateGeometryAfterEvents(double time) const {
        auto it=std::upper_bound(segments.begin(),segments.end(),time,
            [](double query,const RigidMotionSegment& segment){return query<segment.begin;});
        if(it==segments.begin())throw std::out_of_range("motion ledger time");
        --it;
        if(time>=it->begin&&time<=it->end)return it->EvaluateGeometry(time);
        throw std::out_of_range("motion ledger time");
    }
    void ChangeVelocity(const RigidBody& body, double time) {
        if (segments.empty()) throw std::logic_error("motion ledger not started");
        auto& previous = segments.back();
        const double finish = previous.end;
        const auto pose = previous.EvaluateGeometry(time);
        previous.end = time; previous.endPosition = pose.position; previous.endOrientation = pose.orientation;
        RigidBody next = body; next.position = pose.position; next.orientation = pose.orientation;
        const auto owner = previous.owner;
        Append(owner, next, time, finish, false, previous.exactRotation);
    }
    void Clear() { segments.clear(); }
    void ShiftOrigin(glm::dvec3 delta) { for(auto& s:segments){s.position=glm::vec3(glm::dvec3(s.position)-delta);s.endPosition=glm::vec3(glm::dvec3(s.endPosition)-delta);if(s.anchoredTranslation)s.translationOrigin=glm::vec3(glm::dvec3(s.translationOrigin)-delta);} }
    // Concatenate already resolved coupled intervals without restarting their
    // angular integrators. The same generation owner must span every segment.
    void SetRecorded(std::vector<RigidMotionSegment> values) { segments=std::move(values); }
    std::size_t StorageBytes() const { return segments.capacity()*sizeof(RigidMotionSegment); }
    const std::vector<RigidMotionSegment>& Segments() const { return segments; }
private:
    void Append(unsigned int owner, const RigidBody& body, double begin, double end,bool prescribed=false,bool exactRotation=false) {
        if (!prescribed && exactRotation && !body.IsStatic()) {
            AppendDynamicRigidMotion(segments, owner, body, begin, end);
            return;
        }
        RigidMotionSegment s;
        s.planarInverseIz=body.planarInverseIz;s.planeNormal=body.planeNormal;
        s.inverseMass=body.inverseMass;s.inverseInertiaLocal=body.inverseInertiaLocal; s.prescribed=prescribed; s.exactRotation=exactRotation; s.owner=owner; s.begin=begin; s.end=end; s.position=body.position;
        s.orientation=body.orientation; s.linearVelocity=body.linearVelocity; s.angularVelocity=body.angularVelocity;
        s.driftAngularVelocity=prescribed||body.IsPlanar()?body.angularVelocity:AngularDriftGenerator(body.inverseInertiaLocal,body.orientation,body.angularVelocity,static_cast<float>(end-begin));
        const auto pose=s.EvaluateGeometry(end); s.endPosition=pose.position; s.endOrientation=pose.orientation;
        segments.push_back(s);
    }
    std::vector<RigidMotionSegment> segments;
};
