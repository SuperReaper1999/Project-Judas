#pragma once
#include "RigidBody.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <stdexcept>

// Step-local authoritative motion. Observations never restart the integrator.
// The owner is the full slot/generation handle, not a pointer into body storage.
struct RigidMotionSegment {
    unsigned int owner = 0;
    double begin = 0, end = 0;
    float inverseMass = 0;
    glm::vec3 position{0}, linearVelocity{0}, angularVelocity{0};
    glm::quat orientation{1,0,0,0};
    glm::vec3 endPosition{0};
    glm::quat endOrientation{1,0,0,0};
    bool prescribed=false;
    RigidBody Evaluate(double time) const {
        if (time < begin || time > end) throw std::out_of_range("motion segment time");
        RigidBody result;
        result.inverseMass = inverseMass;
        result.position = position; result.orientation = orientation;
        result.linearVelocity = linearVelocity; result.angularVelocity = angularVelocity;
        // Keep the integrator's arithmetic for represented movement, but an
        // unchanged angular sample must preserve its anchor. Re-normalizing a
        // binary32 quaternion at zero/sub-ULP drift can alternate between two
        // values and fabricate repeated detach/impact events in a contact island.
        // This is an exact bit test, not an angular tolerance or a CCD time cap.
        if (prescribed || !result.IsStatic()) {
            const float elapsed = static_cast<float>(time - begin);
            result.position += result.linearVelocity * elapsed;
            const glm::quat omega(0.0f, angularVelocity.x, angularVelocity.y, angularVelocity.z);
            // Prescribed angular velocity is the actual constant world rate.
            // Existing dynamic integration keeps its accepted Euler expression.
            const float speed=glm::length(angularVelocity);
            const glm::quat advanced = prescribed
                ? (speed>0 && elapsed!=0 ? glm::angleAxis(speed*elapsed,angularVelocity/speed)*orientation : orientation)
                : orientation + (omega * orientation) * (0.5f * elapsed);
            const auto same = [](float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; };
            if (!same(advanced.w, orientation.w) || !same(advanced.x, orientation.x) ||
                !same(advanced.y, orientation.y) || !same(advanced.z, orientation.z))
                result.orientation = glm::normalize(advanced);
        }
        return result;
    }
};
class RigidMotion {
public:
    void Begin(unsigned int owner, const RigidBody& body, double duration) {
        segments.clear(); Append(owner, body, 0, duration);
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
    void ChangeVelocity(const RigidBody& body, double time) {
        if (segments.empty()) throw std::logic_error("motion ledger not started");
        auto& previous = segments.back();
        const double finish = previous.end;
        const auto pose = previous.Evaluate(time);
        previous.end = time; previous.endPosition = pose.position; previous.endOrientation = pose.orientation;
        RigidBody next = body; next.position = pose.position; next.orientation = pose.orientation;
        const auto owner = previous.owner;
        Append(owner, next, time, finish);
    }
    void Clear() { segments.clear(); }
    std::size_t StorageBytes() const { return segments.capacity()*sizeof(RigidMotionSegment); }
    const std::vector<RigidMotionSegment>& Segments() const { return segments; }
private:
    void Append(unsigned int owner, const RigidBody& body, double begin, double end,bool prescribed=false) {
        RigidMotionSegment s;
        s.inverseMass=body.inverseMass; s.prescribed=prescribed; s.owner=owner; s.begin=begin; s.end=end; s.position=body.position;
        s.orientation=body.orientation; s.linearVelocity=body.linearVelocity; s.angularVelocity=body.angularVelocity;
        const auto pose=s.Evaluate(end); s.endPosition=pose.position; s.endOrientation=pose.orientation;
        segments.push_back(s);
    }
    std::vector<RigidMotionSegment> segments;
};
