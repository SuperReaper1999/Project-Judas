#include "PerformanceProfiler.h"
#include "PhysicsWorld.h"
#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "SaveArchive.h"
#include "PhysicsCastGeometry.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <tuple>
#include <utility>
#include <vector>

#include "Broadphase.h"
#include "CollisionShapes.h"
#include "ContactSolver.h"
#include "Contacts.h"
#include "Narrowphase.h"
#include "RadialTerrain.h"
#include "RigidBody.h"
#include "RigidMotion.h"
#include "ImpactSolver.h"
#include "JointSolver.h"
#include <atomic>

// Judas's own rigid-body physics — no middleware. Collision detection,
// contact generation, contact resolution, and integration are all owned
// here, built from RigidBody (state/integration), Contacts (narrowphase),
// and ContactSolver (impulse resolution). See docs/ARCHITECTURE.md,
// "Physics ownership" for the migration this replaces (Jolt Physics,
// used through the first Milestone 7-Final attempt) and why: no subsystem here — broadphase,
// narrowphase, contact resolution, the player's sweep query — assumes a
// world-space up axis; every one of them is expressed purely in terms of
// the shapes' and bodies' own positions/orientations. Judas already owned
// gravity, reference frames, and world coordinates before this milestone;
// this closes the remaining gap (collision/contact/rigid-body solving)
// that used to belong to Jolt.
namespace {

using Clock = std::chrono::steady_clock;
double MillisecondsBetween(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

bool FiniteVector(glm::vec3 v) {return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool UsableVelocity(glm::vec3 v) {return FiniteVector(v)&&std::isfinite(glm::length(v));}
bool UsableKinematicPose(const BodyTransform& pose) {
    const double norm=glm::dot(glm::dquat(pose.rotation),glm::dquat(pose.rotation));
    return FiniteVector(pose.position)&&std::isfinite(norm)&&norm>1e-12&&norm<1e12;
}
bool KinematicShape(const Shape& shape) {
    if(shape.type==ShapeType::Box||shape.type==ShapeType::Sphere||shape.type==ShapeType::ConvexHull)return true;
    if(shape.type!=ShapeType::CompoundBoxes||shape.boxes.empty())return false;
    return std::all_of(shape.boxes.begin(),shape.boxes.end(),[](const auto& c){
        return c.type==ShapeType::Box||c.type==ShapeType::Sphere||c.type==ShapeType::ConvexHull;});
}

// Milestone 32: how far a proxy's fat bound extends past its tight bound.
// A body that moves less than this within its fat box costs the tree
// nothing; one that escapes is reinserted. Static bodies use the same
// margin (only kinematically-driven static bodies such as doors move).
constexpr float kBroadphaseMargin = 0.1f;
// Tolerance added to the player sweep's query box.
constexpr float kSweepQueryEpsilon = 1.0e-3f;
// Warm starting: a new contact inherits last step's converged impulses from
// the same body pair/primitive pair when its anchor (in body A's frame) is
// within this distance of a cached one and the normals agree.
constexpr float kWarmStartAnchorDistance = 0.05f;
constexpr float kWarmStartNormalDot = 0.9f;

struct ContactKey {
    unsigned int slotA = 0, slotB = 0;
    unsigned int generationA = 0, generationB = 0;
    int partA = 0, partB = 0;
    bool operator<(const ContactKey& o) const {
        return std::tie(slotA, slotB, partA, partB) < std::tie(o.slotA, o.slotB, o.partA, o.partB);
    }
    bool SameBodies(const ContactKey& o) const {
        return slotA == o.slotA && slotB == o.slotB && partA == o.partA && partB == o.partB &&
               generationA == o.generationA && generationB == o.generationB;
    }
};
struct CachedContact {
    ContactKey key;
    glm::dvec3 localAnchorA{0.0};
    glm::vec3 normal{0.0f};
    float normalImpulse = 0.0f;
    glm::vec3 tangentImpulse{0.0f};
};

std::vector<BodyBox> BoxesAt(const Shape& shape, const glm::vec3& position,
                              const glm::quat& orientation) {
    std::vector<BodyBox> boxes;
    if (shape.type == ShapeType::Box) {
        boxes.push_back({position, orientation, shape.halfExtents});
    } else if (shape.type == ShapeType::CompoundBoxes) {
        boxes.reserve(shape.boxes.size());
        for (const CompoundBox& child : shape.boxes) {
            if(child.type!=ShapeType::Box)continue;
            boxes.push_back({position + orientation * (child.localCenter-shape.pivotOffset),
                             glm::normalize(orientation*child.rotation), child.halfExtents});
        }
    }
    return boxes;
}

glm::mat3 CompoundInverseInertia(float mass, const std::vector<CompoundBox>& boxes) {
    float totalVolume = 0.0f;
    for (const CompoundBox& box : boxes) {
        totalVolume += 8.0f * box.halfExtents.x * box.halfExtents.y * box.halfExtents.z;
    }
    if (mass <= 0.0f || totalVolume <= 0.0f) return glm::mat3(0.0f);

    // Uniform density, with the parallel-axis theorem for each child. The
    // resulting full tensor includes products of inertia when the geometry
    // is asymmetric; RigidBody rotates it to world space as usual.
    glm::mat3 inertia(0.0f);
    for (const CompoundBox& box : boxes) {
        const float childMass = mass *
            (8.0f * box.halfExtents.x * box.halfExtents.y * box.halfExtents.z / totalVolume);
        const glm::vec3 h = box.halfExtents;
        glm::mat3 tensor(0);tensor[0][0]=childMass*(h.y*h.y+h.z*h.z)/3;
        tensor[1][1]=childMass*(h.x*h.x+h.z*h.z)/3;tensor[2][2]=childMass*(h.x*h.x+h.y*h.y)/3;
        const glm::mat3 rotation(ContactRotation(box.rotation));inertia+=rotation*tensor*glm::transpose(rotation);
        const glm::vec3& r = box.localCenter;
        inertia += childMass * (glm::dot(r, r) * glm::mat3(1.0f) - glm::outerProduct(r, r));
    }
    return glm::determinant(inertia) > 1.0e-12f ? glm::inverse(inertia) : glm::mat3(0.0f);
}

}  // namespace

// Distance (and separating normal/owning body index) from the player's
// capsule, placed at a given segment, to the closest world body — used by
// SweepPlayerShape's substep march. `bodyIndex == -1` means no body was
// found closer than `distance`'s initial +infinity (never happens once any
// world geometry exists, but keeps the result total).
struct ClosestBodyResult {
    float distance = std::numeric_limits<float>::max();
    glm::vec3 normal{0.0f};
    int bodyIndex = -1;
    glm::vec3 point{0};
};

struct PhysicsWorld::Impl {
    const uint64_t worldToken=[](){static std::atomic<uint64_t> next{1};return next.fetch_add(1);}();
    struct PoseBound {
        glm::vec3 position{0};
        glm::quat orientation{1,0,0,0};
        Aabb bound;
        bool valid = false;
    };
    struct Body {
        RigidBody rigidBody;
        // Previous fixed-step pose for player movement sweeps against
        // dynamic targets; initialized together with the current pose and
        // refreshed immediately before every physics integration.
        glm::vec3 previousPosition{0.0f};
        RigidMotion motion;
        glm::quat previousOrientation{1.0f, 0.0f, 0.0f, 0.0f};
        Shape shape;
        // Shape is immutable for this slot generation. AddBody replaces the
        // entire cache on slot reuse, including all compound child dimensions.
        float boundingRadius = 0;
        std::optional<ContactPreparedOrientation> orientationCache;
        std::optional<PreparedShapeBounds> boundPreparation;
        bool boundPreparationValid = false;
        PoseBound currentBound, previousBound;
        unsigned collisionLayer=0;
        CategoryMask collisionMask=kAllCategories, tags=0;
        std::string physicalMaterial;
        float friction = 0.5f;
        float restitution = 0.0f;
        bool sensor = false, enabled = true, queryOnly=false, queriesEnabled=true;
        bool isDynamic = false, isKinematic=false;
        KinematicMotionState kinematic;
        double kinematicActiveDuration=0;
        bool HasMotion() const {return isDynamic||isKinematic;}
        bool sleeping=false,deferredAcceleration=false;
        float quietSeconds=0;
        glm::vec3 lastAcceleration{0};
        bool alive = false;
        // Milestone 29: a slot is reused after DestroyBody; the generation
        // in the handle's upper bits makes a handle from a previous
        // occupant of the same slot invalid instead of aliasing the new one.
        unsigned int generation = 0;
        // Milestone 32: this body's leaf in the broadphase tree.
        int proxy = DynamicAabbTree::kNull;
    };

    static constexpr unsigned int kSlotBits = 20;
    static constexpr unsigned int kSlotMask = (1u << kSlotBits) - 1u;

    std::vector<Body> bodies;
    // Slots of destroyed bodies awaiting reuse, and the sorted slots of
    // every live body — the loops below iterate this so a world whose
    // entities were unloaded pays for the bodies that exist, not for the
    // slots they once occupied.
    std::vector<unsigned int> freeSlots;
    std::vector<unsigned int> aliveSlots;
    std::vector<PhysicsWorld::DebugContact> lastStepContacts;
    using TouchKey=std::pair<unsigned,unsigned>;
    std::map<TouchKey,PhysicsWorld::TouchEvent> previousTouches,stepTouches,preStepQueryTouches;
    bool queryTouchesPending=false,collectingPreStepQueries=false;
    std::vector<PhysicsWorld::TouchEvent> touchEvents;
    void Observe(unsigned a,unsigned b,const Contact& c,float impulse=0,bool preStep=false) {
        auto ha=MakeHandle(a),hb=MakeHandle(b);
        bool flip=ha.id>hb.id;if(flip)std::swap(ha,hb);
        auto key=std::make_pair(ha.id,hb.id);
        auto& e=(preStep?preStepQueryTouches:stepTouches)[key];
        e.a=ha;e.b=hb;e.sensor=bodies[a].sensor||bodies[b].sensor;
        e.point=c.point;e.normal=flip?-c.normal:c.normal;
        const auto va=bodies[a].rigidBody.linearVelocity+glm::cross(bodies[a].rigidBody.angularVelocity,c.point-bodies[a].rigidBody.position);
        const auto vb=bodies[b].rigidBody.linearVelocity+glm::cross(bodies[b].rigidBody.angularVelocity,c.point-bodies[b].rigidBody.position);
        e.relativeVelocity=flip?va-vb:vb-va;e.normalImpulse+=impulse;
    }
    void FinishTouches() {
        touchEvents.clear();
        // Sensor overlap is discrete endpoint geometry, never speculative skin
        // or TOI response. Reuse the same tree, layer policy and narrowphase.
        if(std::any_of(aliveSlots.begin(),aliveSlots.end(),[&](unsigned i){return (bodies[i].sensor||bodies[i].isKinematic)&&bodies[i].enabled;})) {
            GenerateCandidatePairs();
            for(auto [a,b]:candidatePairs) if(bodies[a].sensor||bodies[b].sensor||
                ((bodies[a].isKinematic||bodies[b].isKinematic)&&bodies[a].rigidBody.IsStatic()&&bodies[b].rigidBody.IsStatic())) {
                const auto& oa=Orientation(bodies[a]);const auto& ob=Orientation(bodies[b]);
                for(int pa=0;pa<PrimitiveCount(bodies[a].shape);++pa)
                    for(int pb=0;pb<PrimitiveCount(bodies[b].shape);++pb) {
                        auto m=ComputeContacts(PrimitiveAt(bodies[a].shape,bodies[a].rigidBody,pa,&oa),PrimitiveAt(bodies[b].shape,bodies[b].rigidBody,pb,&ob),0,&oa,&ob);
                        if(m.count)Observe(a,b,m.points[0]);
                    }
            }
        }
        for(auto& [key,e]:stepTouches) {
            const auto old=previousTouches.find(key);
            if(old!=previousTouches.end()&&old->second.sensor!=e.sensor){auto exit=old->second;exit.phase=PhysicsWorld::TouchPhase::Exit;touchEvents.push_back(exit);}
            e.phase=old!=previousTouches.end()&&old->second.sensor==e.sensor?PhysicsWorld::TouchPhase::Stay:PhysicsWorld::TouchPhase::Enter;touchEvents.push_back(e);
        }
        for(auto& [key,e]:previousTouches)if(!stepTouches.count(key)){auto exit=e;exit.phase=PhysicsWorld::TouchPhase::Exit;touchEvents.push_back(exit);}
        std::stable_sort(touchEvents.begin(),touchEvents.end(),[](const auto& a,const auto& b){return std::tie(a.a.id,a.b.id)<std::tie(b.a.id,b.b.id);});
        previousTouches=stepTouches;
    }

    // Milestone 32: broadphase, the per-step candidate list, the solver
    // (reused so its storage is not reallocated every step) and statistics.
    DynamicAabbTree tree;
    std::vector<std::pair<unsigned int, unsigned int>> candidatePairs;
    mutable std::vector<unsigned int> queryScratch;
    struct JointRecord {JointHandle handle;JointState state;std::array<float,12> warm{};float previousCoordinate=0;};
    std::vector<JointRecord> joints;
    std::set<std::pair<unsigned,unsigned>> suppressedPairs;
    JointSolver jointSolver,eventJointSolver;
    RigidBody worldAnchor;
    bool JointActive(const JointRecord& joint) const {
        const auto* a=Get(joint.state.settings.bodyA);const auto* b=Get(joint.state.settings.bodyB);
        return joint.state.settings.enabled&&a&&a->enabled&&(!joint.state.settings.bodyB.IsValid()||(b&&b->enabled));
    }
    std::vector<JointInput> JointInputs(const std::vector<unsigned char>* included=nullptr) {
        std::vector<JointInput> result;
        for(auto& joint:joints){joint.state.active=JointActive(joint);if(!joint.state.active){joint.warm.fill(0);continue;}
            auto* a=Get(joint.state.settings.bodyA);auto* b=Get(joint.state.settings.bodyB);
            if((a->sleeping||a->rigidBody.IsStatic())&&(!b||b->sleeping||b->rigidBody.IsStatic()))continue;
            if(included&&!(*included)[joint.state.settings.bodyA.id&kSlotMask]&&
               (!b||!(*included)[joint.state.settings.bodyB.id&kSlotMask]))continue;
            result.push_back({&a->rigidBody,b?&b->rigidBody:&worldAnchor,&joint.state,&joint.warm,a->shape.pivotOffset,b?b->shape.pivotOffset:glm::vec3(0)});
        }
        return result;
    }
    bool sleepingEnabled=true;
    void Wake(unsigned slot){
        if(slot>=bodies.size()||!bodies[slot].alive)return;
        std::set<unsigned> visited;std::vector<unsigned> queue{slot};
        for(size_t i=0;i<queue.size();++i){auto n=queue[i];if(!visited.insert(n).second)continue;
            auto& b=bodies[n];b.sleeping=false;b.quietSeconds=0;
            if(n!=slot&&b.rigidBody.IsStatic())continue;
            for(const auto& j:joints)if(j.state.settings.enabled){const auto a=j.state.settings.bodyA,c=j.state.settings.bodyB;if(!Get(a))continue;if((a.id&kSlotMask)==n&&Get(c))queue.push_back(c.id&kSlotMask);else if(Get(c)&&(c.id&kSlotMask)==n)queue.push_back(a.id&kSlotMask);}
            for(const auto& [_,e]:previousTouches)if(!e.sensor&&Get(e.a)&&Get(e.b)){
                auto a=e.a.id&kSlotMask,c=e.b.id&kSlotMask;if(a==n&&!bodies[c].rigidBody.IsStatic())queue.push_back(c);if(c==n&&!bodies[a].rigidBody.IsStatic())queue.push_back(a);
            }
            // Speculative resting rows may have a positive gap and therefore
            // no touch callback. They still connect the sleeping contact island.
            for(const auto& c:contactCache)if(c.normalImpulse>0){
                const auto a=c.key.slotA,b=c.key.slotB;
                if(a>=bodies.size()||b>=bodies.size()||!bodies[a].alive||!bodies[b].alive||bodies[a].generation!=c.key.generationA||bodies[b].generation!=c.key.generationB)continue;
                if(a==n&&!bodies[b].rigidBody.IsStatic())queue.push_back(b);
                if(b==n&&!bodies[a].rigidBody.IsStatic())queue.push_back(a);
            }
        }
    }
    uint64_t sleepTraceStep=0;
    void Settle(float dt){
        ++sleepTraceStep;
        if(!sleepingEnabled){for(auto i:aliveSlots)if(bodies[i].isDynamic)++stats.awakeBodies;return;}
        std::vector<unsigned> parent(bodies.size());for(unsigned i=0;i<parent.size();++i)parent[i]=i;
        auto root=[&](unsigned i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
        auto unite=[&](unsigned a,unsigned b){if(bodies[a].isDynamic&&bodies[b].isDynamic)parent[root(b)]=root(a);};
        // Positive solver impulses also represent speculative resting support.
        // Touch callbacks alone deliberately omit positive-gap contact rows.
        auto validSupport=[&](const CachedContact& c){return c.normalImpulse>0&&c.key.slotA<bodies.size()&&c.key.slotB<bodies.size()&&bodies[c.key.slotA].alive&&bodies[c.key.slotB].alive&&bodies[c.key.slotA].generation==c.key.generationA&&bodies[c.key.slotB].generation==c.key.generationB;};
        for(const auto& c:contactCache)if(validSupport(c))unite(c.key.slotA,c.key.slotB);
        for(const auto& [_,e]:stepTouches)if(!e.sensor&&Get(e.a)&&Get(e.b))unite(e.a.id&kSlotMask,e.b.id&kSlotMask);
        for(const auto& j:joints)if(JointActive(j)&&Get(j.state.settings.bodyB))unite(j.state.settings.bodyA.id&kSlotMask,j.state.settings.bodyB.id&kSlotMask);
        std::map<unsigned,std::vector<unsigned>> islands;for(auto i:aliveSlots)if(bodies[i].isDynamic&&bodies[i].enabled&&!bodies[i].sensor)islands[root(i)].push_back(i);
        for(auto& [_,slots]:islands){bool quiet=true,anchored=false;float seconds=1000;
            for(const auto& c:contactCache)if(validSupport(c)){auto a=c.key.slotA,b=c.key.slotB;anchored|=(root(a)==root(slots.front())&&bodies[b].rigidBody.IsStatic())||(root(b)==root(slots.front())&&bodies[a].rigidBody.IsStatic());}
            for(const auto& [_,e]:stepTouches)if(!e.sensor&&Get(e.a)&&Get(e.b)){auto a=e.a.id&kSlotMask,b=e.b.id&kSlotMask;anchored|=(root(a)==root(slots.front())&&bodies[b].rigidBody.IsStatic())||(root(b)==root(slots.front())&&bodies[a].rigidBody.IsStatic());}
            for(const auto& j:joints)if(JointActive(j)){auto a=j.state.settings.bodyA.id&kSlotMask;auto* b=Get(j.state.settings.bodyB);anchored|=root(a)==root(slots.front())&&(!b||b->rigidBody.IsStatic());}
            for(auto i:slots){auto& b=bodies[i];quiet&=glm::length(b.rigidBody.linearVelocity)<.03f&&glm::length(b.rigidBody.angularVelocity)<.03f&&glm::length(b.rigidBody.position-b.previousPosition)<.002f&&glm::abs(glm::dot(b.rigidBody.orientation,b.previousOrientation))>.999999f;seconds=std::min(seconds,b.quietSeconds);}
            for(const auto& j:joints)if(JointActive(j)&&root(j.state.settings.bodyA.id&kSlotMask)==root(slots.front())){
                const auto& s=j.state.settings;auto* a=Get(s.bodyA);auto* b=Get(s.bodyB);auto pa=a->rigidBody.position+a->rigidBody.orientation*(s.anchorA-a->shape.pivotOffset);auto pb=b?b->rigidBody.position+b->rigidBody.orientation*(s.anchorB-b->shape.pivotOffset):s.anchorB;
                auto error=pb-pa;if(s.type==JointType::Slider){auto axis=a->rigidBody.orientation*s.frameA*glm::vec3(1,0,0);error-=axis*glm::dot(error,axis);}quiet&=glm::length(error)<.02f;quiet&=!s.motor||s.speed==0;
                auto qa=glm::normalize(a->rigidBody.orientation*s.frameA),qb=b?glm::normalize(b->rigidBody.orientation*s.frameB):glm::normalize(s.frameB);
                if(s.type==JointType::Fixed||s.type==JointType::Slider)quiet&=2*std::acos(std::clamp(glm::abs(glm::dot(qa,qb)),0.f,1.f))<.01f;
                if(s.type==JointType::Hinge)quiet&=glm::length(qa*glm::vec3(1,0,0)-qb*glm::vec3(1,0,0))<.01f;
                if(s.spring)quiet&=std::abs(j.state.coordinate-j.previousCoordinate)<.002f; // gravity can balance a spring away from its rest coordinate
                if(s.limits)quiet&=j.state.coordinate>=s.lower-.01f&&j.state.coordinate<=s.upper+.01f;
            }
            for(const auto& c:lastStepContacts)if(c.penetration>.02f){ // only invalidate the contacting island
                for(auto i:slots)if(glm::length(c.point-bodies[i].rigidBody.position)<bodies[i].boundingRadius+.1f)quiet=false;
            }
            if(std::getenv("JUDAS_SLEEP_TRACE")&&sleepTraceStep%60==0){
                float linear=0,angular=0,travel=0,anchorError=0,angularError=0,penetration=0;bool drive=false,limit=true;
                for(auto i:slots){const auto& b=bodies[i];linear=std::max(linear,glm::length(b.rigidBody.linearVelocity));angular=std::max(angular,glm::length(b.rigidBody.angularVelocity));travel=std::max(travel,glm::length(b.rigidBody.position-b.previousPosition));}
                for(const auto& j:joints)if(JointActive(j)&&root(j.state.settings.bodyA.id&kSlotMask)==root(slots.front())){const auto& x=j.state.settings;auto* a=Get(x.bodyA);auto* b=Get(x.bodyB);auto pa=a->rigidBody.position+a->rigidBody.orientation*(x.anchorA-a->shape.pivotOffset),pb=b?b->rigidBody.position+b->rigidBody.orientation*(x.anchorB-b->shape.pivotOffset):x.anchorB;anchorError=std::max(anchorError,glm::length(pa-pb));drive|=x.motor&&x.speed!=0;limit&=!x.limits||(j.state.coordinate>=x.lower-.01f&&j.state.coordinate<=x.upper+.01f);auto qa=glm::normalize(a->rigidBody.orientation*x.frameA),qb=b?glm::normalize(b->rigidBody.orientation*x.frameB):glm::normalize(x.frameB);if(x.type==JointType::Hinge)angularError=std::max(angularError,glm::length(qa*glm::vec3(1,0,0)-qb*glm::vec3(1,0,0)));}
                for(const auto& c:lastStepContacts)for(auto i:slots)if(glm::length(c.point-bodies[i].rigidBody.position)<bodies[i].boundingRadius+.1f)penetration=std::max(penetration,c.penetration);
                std::fprintf(stderr,"SLEEP step=%llu root=%u members=%zu v=%g w=%g travel=%g anchor=%g axis=%g penetration=%g drive=%d limits=%d quiet=%d time=%g supported=%d\n",(unsigned long long)sleepTraceStep,slots.front(),slots.size(),linear,angular,travel,anchorError,angularError,penetration,drive,limit,quiet,seconds,anchored);
            }
            seconds=quiet?seconds+dt:0;
            const bool asleep=quiet&&seconds>=.75f&&(anchored||std::all_of(slots.begin(),slots.end(),[&](unsigned i){return glm::length(bodies[i].lastAcceleration)<1e-6f&&glm::length(bodies[i].rigidBody.linearVelocity)<1e-6f&&glm::length(bodies[i].rigidBody.angularVelocity)<1e-6f;}));
            for(auto i:slots){auto& b=bodies[i];b.quietSeconds=seconds;b.sleeping=asleep;if(asleep){b.rigidBody.linearVelocity={0,0,0};b.rigidBody.angularVelocity={0,0,0};}}
            stats.sleepingIslands+=asleep;
        }
        for(auto& j:joints)j.previousCoordinate=j.state.coordinate;
        for(auto i:aliveSlots)if(bodies[i].isDynamic){stats.sleepingBodies+=bodies[i].sleeping;stats.awakeBodies+=!bodies[i].sleeping;}
    }
    ContactSolver solver;
    ContactSolver particleSolver; // independent storage; never replays the rigid warm cache
    PhysicsWorld::ParticleContactStats particleContactStats;
    ImpactSolver impactSolver;
    struct StartContact {
        unsigned a,b;
        std::size_t constraint;
        float friction, warmNormal;
        glm::vec3 warmTangent;
        bool newImpact;
    };
    std::vector<StartContact> startContacts;
    std::vector<std::array<unsigned,4>> separatedPairs;
    std::vector<std::array<unsigned,4>> supportPairs;
    struct ScheduledImpact { std::array<unsigned,4> key; double time; };
    std::vector<ScheduledImpact> scheduledImpacts;
    std::vector<unsigned> impactCounts;
    std::vector<unsigned char> impactCapped;
    double stepDuration = 0;

    // A derived CCD sample, never an integration anchor. Many incident pairs
    // ask for the same body's pose at the same event time. Keep its exact
    // represented pose/orientation preparation once until that trajectory
    // changes. Time and generation are part of the key; every step and every
    // Begin/ChangeVelocity invalidates samples explicitly.
    struct ImpactPoseSample {
        RigidBody pose;
        std::optional<ContactPreparedOrientation> orientation;
        double time=std::numeric_limits<double>::quiet_NaN();
        unsigned generation=0;
        Aabb remainingBound;
        double boundStart=std::numeric_limits<double>::quiet_NaN(),boundFinish=0;
        unsigned boundGeneration=0;
    };
    std::vector<ImpactPoseSample> impactPoseSamples;
    std::size_t impactPoseEvaluations=0,impactPoseReuses=0,impactOrientationBuilds=0,impactTruthRequests=0,impactTruthReuses=0,impactBoundsRejected=0;
    void InvalidateImpactPose(unsigned slot){
        if(slot<impactPoseSamples.size()){
            impactPoseSamples[slot].time=std::numeric_limits<double>::quiet_NaN();
            impactPoseSamples[slot].boundStart=std::numeric_limits<double>::quiet_NaN();
        }
    }

    RigidBody PoseAt(unsigned slot, double time) const {
        const Body& b=bodies[slot];
        RigidBody pose=b.rigidBody;
        if (b.HasMotion() && !b.motion.Segments().empty()) {
            const auto sampled=b.motion.Evaluate(time);
            pose.position=sampled.position; pose.orientation=sampled.orientation;
            if(b.isKinematic){pose.linearVelocity=sampled.linearVelocity;pose.angularVelocity=sampled.angularVelocity;}
        }
        return pose;
    }
    static double NormalSpeed(const Contact& c,const RigidBody& a,const RigidBody& b) {
        const glm::dvec3 ra=c.hasLocalAnchors ? (c.anchorAInWorldFrame ? c.localAnchorA : ContactRotation(a.orientation)*c.localAnchorA) :
            glm::dvec3(c.point)-glm::dvec3(a.position);
        const glm::dvec3 rb=c.hasLocalAnchors ? (c.anchorBInWorldFrame ? c.localAnchorB : ContactRotation(b.orientation)*c.localAnchorB) :
            glm::dvec3(c.point)-glm::dvec3(b.position);
        return glm::dot(glm::dvec3(c.normal),glm::dvec3(a.linearVelocity-b.linearVelocity)+
            glm::cross(glm::dvec3(a.angularVelocity),ra)-glm::cross(glm::dvec3(b.angularVelocity),rb));
    }
    const ImpactPoseSample& ImpactPoseAt(unsigned slot,double time){
        EnsureImpactPoseStorage();
        auto& sample=impactPoseSamples[slot];
        if(sample.time==time&&sample.generation==bodies[slot].generation){++impactPoseReuses;return sample;}
        sample.pose=PoseAt(slot,time);sample.time=time;sample.generation=bodies[slot].generation;++impactPoseEvaluations;
        if(!sample.orientation||!sample.orientation->Matches(sample.pose.orientation)){
            sample.orientation.emplace(sample.pose.orientation);++impactOrientationBuilds;
        }
        return sample;
    }
    const Aabb& RemainingImpactBound(unsigned slot,double start,double finish){
        EnsureImpactPoseStorage();
        auto& sample=impactPoseSamples[slot];const auto& body=bodies[slot];
        if(sample.boundStart==start&&sample.boundFinish==finish&&sample.boundGeneration==body.generation)return sample.remainingBound;
        if(!body.HasMotion())sample.remainingBound=CurrentBound(bodies[slot]);
        else {
            // The remaining motion is one anchored segment. Each represented
            // center component stays between its sampled endpoints. An outward
            // enclosing sphere at both endpoints therefore bounds every shape
            // orientation, including a full turn with equal endpoint rotations.
            // Only centers are needed: do not evict the prepared start pose or
            // prepare an unused endpoint orientation for this sphere enclosure.
            const auto from=PoseAt(slot,start).position,to=PoseAt(slot,finish).position;
            const auto sphere=Shape::Sphere(body.boundingRadius);const glm::quat identity(1,0,0,0);
            sample.remainingBound=ShapeAabb(sphere,from,identity).Union(ShapeAabb(sphere,to,identity));
        }
        sample.boundStart=start;sample.boundFinish=finish;sample.boundGeneration=body.generation;return sample.remainingBound;
    }
    ContactManifold PairAt(unsigned a,unsigned b,int pa,int pb,double time,float margin) {
        const auto& aa=ImpactPoseAt(a,time);const auto& bb=ImpactPoseAt(b,time);
        return ComputeContacts(PrimitiveAt(bodies[a].shape,aa.pose,pa,&*aa.orientation),
            PrimitiveAt(bodies[b].shape,bb.pose,pb,&*bb.orientation),margin,&*aa.orientation,&*bb.orientation);
    }
    // Lower bound on a fixed separating-plane gap along the anchored drift.
    // For q(t)=normalize(q0+t*Omega*q0/2), instantaneous angular speed is
    // |omega|/(1+(|omega|t/2)^2). Every rotated vertex has |r''|<=|omega|^2|r_perp|.
    // Evaluate ALL support vertices, including those not currently extremal.
    double PlaneDrift(unsigned a,unsigned b,int pa,int pb,double time,const glm::dvec3& n) {
        struct Vertex {glm::dvec3 r,velocity;double curvature;};
        struct Support {std::array<Vertex,128> vertices;int count=0;double radius=0;};
        auto support=[&](unsigned slot,int part) {
            Support result;const auto& body=bodies[slot];const auto& sample=ImpactPoseAt(slot,time);const auto& pose=sample.pose;
            const auto child=PrimitiveAt(body.shape,pose,part,&*sample.orientation);
            if(child.shape.type!=ShapeType::Box && child.shape.type!=ShapeType::Sphere&&child.shape.type!=ShapeType::ConvexHull) return result;
            const auto& rotation=sample.orientation->rotation;
            const glm::dvec3 omega(body.HasMotion()?pose.angularVelocity:glm::vec3(0));
            const double w2=glm::dot(omega,omega);
            const double elapsed=body.isDynamic && !body.motion.Segments().empty() ? time-body.motion.Segments().back().begin : 0;
            const glm::dvec3 instantaneous=body.isKinematic?omega:omega/(1+.25*w2*elapsed*elapsed);
            auto vertex=[&](glm::dvec3 local) {
                const glm::dvec3 r=rotation*local;
                const glm::dvec3 perpendicular=w2>0 ? r-omega*(glm::dot(omega,r)/w2) : glm::dvec3(0);
                result.vertices[result.count++]={r,glm::dvec3(body.HasMotion()?pose.linearVelocity:glm::vec3(0))+glm::cross(instantaneous,r),
                                                  w2*glm::length(perpendicular)};
            };
            if(child.shape.type==ShapeType::Sphere) {vertex(glm::dvec3(child.parentLocalCenter));result.radius=child.shape.radius;}
            else if(child.shape.type==ShapeType::ConvexHull){const auto r=ContactRotation(child.childRotation);for(auto p:child.shape.asset->vertices)vertex(glm::dvec3(child.parentLocalCenter)+r*p);}
            else {const auto childRotation=ContactRotation(child.childRotation);for(int k=0;k<8;++k) vertex(glm::dvec3(child.parentLocalCenter)+childRotation*glm::dvec3(child.shape.halfExtents)*
                glm::dvec3(k&1?1:-1,k&2?1:-1,k&4?1:-1));}
            return result;
        };
        const auto sa=support(a,pa),sb=support(b,pb);
        if(!sa.count || !sb.count) return 0;
        const glm::dvec3 delta=glm::dvec3(ImpactPoseAt(a,time).pose.position)-glm::dvec3(ImpactPoseAt(b,time).pose.position);
        double advance=std::numeric_limits<double>::infinity();
        for(int i=0;i<sa.count;++i) for(int j=0;j<sb.count;++j) {
            const auto& va=sa.vertices[i];const auto& vb=sb.vertices[j];
            const double gap=glm::dot(n,delta+va.r-vb.r)-sa.radius-sb.radius;
            if(!(gap>0)) return 0; // This sampled normal cannot certify a separating plane.
            const double rate=glm::dot(n,va.velocity-vb.velocity),curvature=va.curvature+vb.curvature;
            double root=std::numeric_limits<double>::infinity();
            if(curvature>0) {
                const double discriminant=std::sqrt(rate*rate+2*curvature*gap);
                root=rate<0 ? 2*gap/(discriminant-rate) : (rate+discriminant)/curvature;
            } else if(rate<0) root=-gap/rate;
            advance=std::min(advance,root);
        }
        return advance;
    }

    // Conservative advancement along a separating plane. Translation projected
    // onto that plane plus |omega|*radius bounds its possible closing speed.
    // Anchored quaternion interpolation turns no faster than |omega|.
    double ImpactTime(unsigned a,unsigned b,int pa,int pb,double start,double finish) {
        // A target can finish inside the step. Never certify a plane across
        // its velocity discontinuity; search the two real ledger intervals.
        for(auto slot:{a,b})if(bodies[slot].isKinematic)for(const auto& s:bodies[slot].motion.Segments())
            if(s.end>start&&s.end<finish){auto first=ImpactTime(a,b,pa,pb,start,s.end);
                return first<=s.end?first:ImpactTime(a,b,pa,pb,s.end,finish);}
        ++stats.impactQueries;
        const auto& ba=bodies[a]; const auto& bb=bodies[b];
        const auto posa=PoseAt(a,start),posb=PoseAt(b,start);
        const glm::dvec3 relative=glm::dvec3(ba.HasMotion()?posa.linearVelocity:glm::vec3(0))-glm::dvec3(bb.HasMotion()?posb.linearVelocity:glm::vec3(0));
        const double angular=(ba.HasMotion()?glm::length(glm::dvec3(posa.angularVelocity))*ba.boundingRadius:0)+
                             (bb.HasMotion()?glm::length(glm::dvec3(posb.angularVelocity))*bb.boundingRadius:0);
        const double speed=glm::length(relative)+angular;
        if (!(speed>0)) return finish+1;
        const auto simpleRadius=[](const Body& body){
            return (body.shape.type==ShapeType::Box||body.shape.type==ShapeType::Sphere)&&body.shape.pivotOffset==glm::vec3(0)&&std::isfinite(body.boundingRadius)&&body.boundingRadius>0;
        };
        // Restrict this rejection to the existing outward-proven primitive
        // radius/bounds. Compound, cooked and terrain shapes retain the full
        // old query until their radius enclosure is independently established.
        if(simpleRadius(ba)&&simpleRadius(bb)){
            const auto& boundA=RemainingImpactBound(a,start,finish);const auto& boundB=RemainingImpactBound(b,start,finish);
            const auto finite=[](const Aabb& bound){for(int k=0;k<3;++k)if(!std::isfinite(bound.min[k])||!std::isfinite(bound.max[k]))return false;return true;};
            // An arithmetic-range failure is not a geometric rejection. Keep
            // the established query path whenever the enclosure is uncertain.
            if(finite(boundA)&&finite(boundB)&&!boundA.Overlaps(boundB)){++impactBoundsRejected;return finish+1;}
        }
        // Bisection still performs all 32 represented-time bracket updates.
        // Binary32 pose integration can map several of those times to exactly
        // the same pair of poses. Contact truth for that identical geometry is
        // reusable; no margin contact, tolerance or earlier stopping is used.
        struct ActualTruth {std::array<float,14> pose{};bool valid=false,touching=false;};
        std::array<ActualTruth,3> actualTruth{};std::size_t nextTruth=0;
        auto touchingAt=[&](double at){
            ++impactTruthRequests;
            const auto& aa=ImpactPoseAt(a,at).pose;const auto& bb=ImpactPoseAt(b,at).pose;
            const std::array<float,14> represented={aa.position.x,aa.position.y,aa.position.z,aa.orientation.w,aa.orientation.x,aa.orientation.y,aa.orientation.z,
                bb.position.x,bb.position.y,bb.position.z,bb.orientation.w,bb.orientation.x,bb.orientation.y,bb.orientation.z};
            for(const auto& old:actualTruth)if(old.valid&&std::memcmp(old.pose.data(),represented.data(),sizeof(represented))==0){++impactTruthReuses;return old.touching;}
            const bool touching=PairAt(a,b,pa,pb,at,0).count>0;
            actualTruth[nextTruth]={represented,true,touching};nextTruth=(nextTruth+1)%actualTruth.size();return touching;
        };
        double time=start, previous=start;
        bool uncertifiedAdvance=false;
        for (int iteration=0;iteration<64;++iteration) {
            ++stats.impactSearchIterations;stats.impactPeakIterations=std::max(stats.impactPeakIterations,std::size_t(iteration+1));
            const float reach=std::nextafter(static_cast<float>(speed*(finish-time)),
                                             std::numeric_limits<float>::infinity());
            // Margin manifolds contain proximity features, not necessarily the
            // zero-margin contact set. Contact truth must use the latter.
            if(touchingAt(time)) {
                if(time==start) return finish+1;
                double lo=previous,hi=time;
                for(int k=0;k<32;++k) {
                    const double mid=(lo+hi)*.5;
                    if(touchingAt(mid)) hi=mid;else lo=mid;
                }
                return hi;
            }
            // The exact endpoint has already been checked. There is no future
            // interval left from which a proximity reach could reveal a hit.
            if(time==finish)return finish+1;
            const auto manifold=PairAt(a,b,pa,pb,time,reach);
            if (!manifold.count) return finish+1;
            const Contact* nearest=&manifold.points[0];
            for (int k=1;k<manifold.count;++k)
                if (manifold.points[k].signedSeparation<nearest->signedSeparation) nearest=&manifold.points[k];
            const double gap=nearest->hasLocalAnchors ? nearest->signedSeparation : -double(nearest->penetration);
            if(gap<=0) {
                // Some specialized geometry (notably terrain) excludes exact
                // equality at zero margin while a proximity query includes it.
                // The latter is not a certified separating plane or an impact.
                // Let bounded actual-contact sampling establish contact truth.
                ++stats.impactUncertifiedAdvances;
                uncertifiedAdvance=true;
                break;
            }
            const double closing=-glm::dot(relative,nearest->hasLocalAnchors ? nearest->preciseNormal : glm::dvec3(nearest->normal))+angular;
            if (!(closing>0)) return finish+1;
            double increment=gap/closing;
            if(angular>0) increment=std::max(increment,PlaneDrift(a,b,pa,pb,time,nearest->hasLocalAnchors ? nearest->preciseNormal : glm::dvec3(nearest->normal)));
            // Progress in represented time only. A large coordinate orthogonal
            // to motion must not enlarge the step and skip a thin obstacle.
            increment=std::max(increment,std::nextafter(time,finish)-time);
            previous=time;
            time=std::min(finish,time+increment);
            if (time<=previous) return finish+1;
        }
        if(!uncertifiedAdvance) ++stats.impactSearchLimit;
        ++stats.impactSamplingFallbacks;
        // Rotating grazing features can keep a fixed separating-plane bound
        // arbitrarily short. Finish this PAIR query with a bounded temporal
        // sampling approximation, never by applying an impulse across a gap.
        // Intersections shorter than the reported sampling interval can be
        // missed; this is deliberately not a complete continuous-CCD proof.
        auto firstContact=[&](double lo,double hi) {
            for(int k=0;k<32;++k) {
                const double mid=(lo+hi)*.5;
                if(touchingAt(mid)) hi=mid;else lo=mid;
            }
            return hi;
        };
        ++stats.impactSamplingTests;
        if(touchingAt(time)) return firstContact(previous,time);
        const double remaining=finish-time;
        if(!(remaining>0)) return finish+1;
        auto primitiveFeature=[&](unsigned slot,int part) {
            const Shape& shape=bodies[slot].shape;
            auto primitive=PrimitiveAt(shape,bodies[slot].rigidBody,part);
            if(primitive.shape.type==ShapeType::Sphere) return double(primitive.shape.radius);
            if(primitive.shape.type==ShapeType::ConvexHull){auto extent=primitive.shape.asset->maximum-primitive.shape.asset->minimum;return std::min({extent.x,extent.y,extent.z})*.5;}
            const glm::vec3 half=shape.type==ShapeType::CompoundBoxes
                ? shape.boxes[std::size_t(part)].halfExtents : shape.halfExtents;
            if(shape.type==ShapeType::Box || shape.type==ShapeType::CompoundBoxes)
                return double(std::min({half.x,half.y,half.z}));
            // Terrain uses the other, finite rigid primitive's feature size.
            return std::numeric_limits<double>::infinity();
        };
        const double feature=std::min(primitiveFeature(a,pa),primitiveFeature(b,pb));
        if(!(feature>0)) throw std::runtime_error("invalid primitive feature in impact sampling");
        const double angularSpeed=(ba.HasMotion()?glm::length(glm::dvec3(posa.angularVelocity)):0)+
                                  (bb.HasMotion()?glm::length(glm::dvec3(posb.angularVelocity)):0);
        const double requested=std::max({16.0,std::ceil(16.0*speed*remaining/feature),
                                              std::ceil(256.0*angularSpeed*remaining)});
        constexpr unsigned kMaxSamples=65536;
        if(requested>double(kMaxSamples)) ++stats.impactSamplingResolutionCaps;
        const unsigned samples=static_cast<unsigned>(std::min(double(kMaxSamples),requested));
        const double interval=remaining/double(samples);
        stats.impactSamplingMaxInterval=std::max(stats.impactSamplingMaxInterval,interval);
        double lo=time;
        for(unsigned sample=1;sample<=samples;++sample) {
            const double hi=sample==samples ? finish : time+remaining*(double(sample)/double(samples));
            ++stats.impactSamplingTests;
            if(touchingAt(hi)) return firstContact(lo,hi);
            lo=hi;
        }
        return finish+1;
    }

    void ResolveInitialImpacts(float dt) {
        if (std::none_of(startContacts.begin(),startContacts.end(),[](const auto& c){return c.newImpact;})) return;
        std::vector<Contact> geometry;
        geometry.reserve(solver.Constraints().size());
        for(const auto& c:solver.Constraints()) {
            Contact g;g.hit=true;g.point=c.point;g.normal=c.normal;g.preciseNormal=c.preciseNormal;
            g.penetration=c.penetration;g.signedSeparation=c.signedSeparation;
            g.localAnchorA=c.localAnchorA;g.localAnchorB=c.localAnchorB;g.hasLocalAnchors=c.hasLocalAnchors;
            g.anchorAInWorldFrame=c.anchorAInWorldFrame;g.anchorBInWorldFrame=c.anchorBInWorldFrame;
            geometry.push_back(g);
        }
        std::vector<unsigned> parent(bodies.size());
        for (unsigned i=0;i<parent.size();++i) parent[i]=i;
        auto root=[&](unsigned i){while(parent[i]!=i) {parent[i]=parent[parent[i]];i=parent[i];}return i;};
        for (const auto& c:startContacts) if (!bodies[c.a].rigidBody.IsStatic() && !bodies[c.b].rigidBody.IsStatic())
            parent[root(c.b)]=root(c.a);
        for(const auto& joint:joints)if(JointActive(joint)&&joint.state.settings.bodyB.IsValid()){
            const auto a=joint.state.settings.bodyA.id&kSlotMask,b=joint.state.settings.bodyB.id&kSlotMask;
            if(!bodies[a].rigidBody.IsStatic()&&!bodies[b].rigidBody.IsStatic())parent[root(b)]=root(a);
        }
        std::vector<std::pair<unsigned,std::size_t>> order;
        for (std::size_t i=0;i<startContacts.size();++i) {
            const auto& c=startContacts[i];
            order.emplace_back(root(bodies[c.a].rigidBody.IsStatic()?c.b:c.a),i);
        }
        std::sort(order.begin(),order.end());
        for (std::size_t begin=0;begin<order.size();) {
            std::size_t end=begin+1; bool impact=startContacts[order[begin].second].newImpact;
            while(end<order.size() && order[end].first==order[begin].first) {
                impact=impact||startContacts[order[end].second].newImpact;++end;
            }
            if (impact) {
                std::vector<ImpactContact> rows;
                for (auto k=begin;k<end;++k) {
                    auto& c=startContacts[order[k].second];
                    const bool persistent=c.warmNormal>0;
                    c.warmNormal=0;c.warmTangent=glm::vec3(0);
                    if (geometry[c.constraint].signedSeparation>0) continue;
                    auto& a=bodies[c.a];auto& b=bodies[c.b];
                    rows.push_back({&a.rigidBody,&b.rigidBody,geometry[c.constraint],c.friction,
                                    std::max(a.restitution,b.restitution),persistent});
                }
                std::vector<unsigned char> included(bodies.size(),0);
                for(unsigned slot:aliveSlots)if(root(slot)==order[begin].first)included[slot]=1;
                eventJointSolver.Prepare(JointInputs(&included),dt,false,true);
                const auto result=impactSolver.Solve(rows,dt,false,eventJointSolver.Empty()?nullptr:&eventJointSolver);
                if(!result.energyBudgetSatisfied) throw std::runtime_error("inelastic impact energy guard failed");
                if(result.effectiveRestitution==0) for(auto k=begin;k<end;++k) startContacts[order[k].second].newImpact=false;
                ++stats.impactEvents; stats.impactSafetyFallback+=result.safetyFallback;
            }
            begin=end;
        }
        // Only the rare time-zero impact path needs rebuilt warm-start inputs.
        // Persistent contact solver arithmetic and APIs remain unchanged.
        solver.Clear();
        for(const auto& c:startContacts) {
            auto& a=bodies[c.a];auto& b=bodies[c.b];
            const auto& oa=Orientation(a);const auto& ob=Orientation(b);
            solver.AddContact(a.rigidBody,b.rigidBody,geometry[c.constraint],c.friction,0,
                c.warmNormal,c.warmTangent,&oa.rotation,&ob.rotation);
        }
    }

    void AdvanceImpacts(float dt) {
        stepDuration=dt;
        impactPoseEvaluations=impactPoseReuses=impactOrientationBuilds=impactTruthRequests=impactTruthReuses=impactBoundsRejected=0;
        for(auto slot:aliveSlots)InvalidateImpactPose(slot);
        impactCounts.assign(bodies.size(),0); impactCapped.assign(bodies.size(),0);
        // A sleeping body still needs its stationary pre-impact trajectory.
        // An actual TOI may wake it and append a new segment during this step.
        for (const auto slot:aliveSlots) if (bodies[slot].isDynamic){
            bodies[slot].motion.Begin(MakeHandle(slot).id,bodies[slot].rigidBody,dt);InvalidateImpactPose(slot);
        }
        // Initial touching impacts can launch a previously stationary body.
        // Refresh those trajectories before searching for a separated event.
        if(stats.impactEvents) {
            for(auto slot:aliveSlots) if(bodies[slot].isDynamic) CoverStepReach(bodies[slot],dt);
            GenerateCandidatePairs(); separatedPairs.clear();
            for(const auto& pair:candidatePairs)
                for(int pa=0;pa<PrimitiveCount(bodies[pair.first].shape);++pa)
                    for(int pb=0;pb<PrimitiveCount(bodies[pair.second].shape);++pb)
                        separatedPairs.push_back({pair.first,pair.second,unsigned(pa),unsigned(pb)});
        }
        double now=0;
        scheduledImpacts.clear();
        std::map<std::array<unsigned,4>,std::size_t> scheduledIndex;
        auto schedule=[&](const std::array<unsigned,4>& key) {
            if(!CanRespond(key[0],key[1]))return;
            const double time=std::binary_search(supportPairs.begin(),supportPairs.end(),key) ? double(dt)+1 :
                ImpactTime(key[0],key[1],int(key[2]),int(key[3]),now,dt);
            const auto found=scheduledIndex.find(key);
            if(found!=scheduledIndex.end()) scheduledImpacts[found->second].time=time;
            else {scheduledIndex.emplace(key,scheduledImpacts.size());scheduledImpacts.push_back({key,time});}
        };
        for(const auto& key:separatedPairs) schedule(key);
        for (;;) {
            double earliest=double(dt)+1; unsigned seedA=0,seedB=0;
            std::array<unsigned,4> selected{};
            for (const auto& event:scheduledImpacts) {
                if(event.time<earliest || (event.time==earliest && event.key<selected)) {
                    earliest=event.time;selected=event.key;seedA=event.key[0];seedB=event.key[1];
                }
            }
            if (earliest>dt) break;
            now=earliest;
            // Infinite-mass boundaries do not join the velocity-changing
            // island, but solver lever arms and observed point velocities
            // must use their actual event-time COM/orientation.
            for(auto slot:aliveSlots)if(bodies[slot].isKinematic){auto& b=bodies[slot];auto p=PoseAt(slot,now);
                b.rigidBody.position=p.position;b.rigidBody.orientation=p.orientation;
                b.rigidBody.linearVelocity=p.linearVelocity;b.rigidBody.angularVelocity=p.angularVelocity;}
            std::vector<unsigned> island;
            std::vector<unsigned char> included(bodies.size(),0);
            auto include=[&](unsigned slot){if(!included[slot] && !bodies[slot].rigidBody.IsStatic()) {
                if(bodies[slot].sleeping)Wake(slot);
                included[slot]=1;island.push_back(slot);}};
            include(seedA);include(seedB);
            std::vector<std::pair<unsigned,unsigned>> touching;
            for (std::size_t next=0;next<island.size();++next) {
                const unsigned a=island[next]; const auto pose=PoseAt(a,now);
                for(const auto& joint:joints)if(JointActive(joint)&&joint.state.settings.bodyB.IsValid()){
                    const auto x=joint.state.settings.bodyA.id&kSlotMask,y=joint.state.settings.bodyB.id&kSlotMask;
                    if(x==a)include(y);
                    if(y==a)include(x);
                }
                const auto candidates=QuerySlots(ShapeAabb(bodies[a].shape,pose.position,pose.orientation));
                for (unsigned b:candidates) {
                    if (a==b || !CanRespond(a,b)) continue;
                    const auto pair=std::minmax(a,b);
                    if (std::find(touching.begin(),touching.end(),std::pair<unsigned,unsigned>(pair))!=touching.end()) continue;
                    bool hit=false;
                    for(int pa=0;pa<PrimitiveCount(bodies[a].shape);++pa)
                        for(int pb=0;pb<PrimitiveCount(bodies[b].shape);++pb)
                            hit=hit||PairAt(a,b,pa,pb,now,0).count>0;
                    if(hit) {touching.emplace_back(pair);include(b);}
                }
            }
            // A supported body's incoming impact is mechanically coupled even
            // when a cached support point currently has a small positive gap.
            // This only chooses restitution zero: separated support geometry
            // is never inserted into the actual-contact impulse rows below.
            const bool supportedIsland=std::any_of(supportPairs.begin(),supportPairs.end(),
                [&](const auto& pair){return included[pair[0]] || included[pair[1]];});
            bool capture=supportedIsland;
            std::vector<std::pair<glm::vec3,glm::vec3>> oldVelocities;
            oldVelocities.reserve(island.size());
            for(unsigned slot:island) {
                auto& body=bodies[slot];const auto pose=PoseAt(slot,now);
                oldVelocities.emplace_back(body.rigidBody.linearVelocity,body.rigidBody.angularVelocity);
                body.rigidBody.position=pose.position;body.rigidBody.orientation=pose.orientation;
                // Gravity was deferred while stationary. Integrate only the
                // remaining interval; never drift the frozen pre-impact pose.
                if(body.deferredAcceleration){
                    body.rigidBody.linearVelocity+=body.lastAcceleration*static_cast<float>(dt-now);
                    body.deferredAcceleration=false;
                }
                if (++impactCounts[slot]>16) {
                    if(!impactCapped[slot]) {impactCapped[slot]=1;++stats.impactEventCapFallback;}
                    capture=true;
                }
            }
            std::vector<ImpactContact> rows;
            std::vector<std::array<unsigned,4>> eventPairs;
            for (const auto& pair:touching) {
                auto& a=bodies[pair.first];auto& b=bodies[pair.second];
                for(int pa=0;pa<PrimitiveCount(a.shape);++pa)
                    for(int pb=0;pb<PrimitiveCount(b.shape);++pb) {
                        const auto m=PairAt(pair.first,pair.second,pa,pb,now,0);
                        if(m.count) eventPairs.push_back({pair.first,pair.second,unsigned(pa),unsigned(pb)});
                        for(int k=0;k<m.count;++k) {
                            Observe(pair.first,pair.second,m.points[k]);
                            rows.push_back({&a.rigidBody,&b.rigidBody,m.points[k],
                                std::sqrt(std::max(0.f,a.friction)*std::max(0.f,b.friction)),
                                std::max(a.restitution,b.restitution),false});
                            lastStepContacts.push_back({m.points[k].point,m.points[k].normal,m.points[k].penetration});
                        }
                    }
            }
            if(rows.empty()) throw std::runtime_error("impact bracket lost contact geometry");
            eventJointSolver.Prepare(JointInputs(&included),dt,false,true);
            const auto result=impactSolver.Solve(rows,dt,capture,eventJointSolver.Empty()?nullptr:&eventJointSolver);
            if(!result.energyBudgetSatisfied) throw std::runtime_error("inelastic impact energy guard failed");
            if(result.effectiveRestitution==0) {
                supportPairs.insert(supportPairs.end(),eventPairs.begin(),eventPairs.end());
                std::sort(supportPairs.begin(),supportPairs.end());supportPairs.erase(std::unique(supportPairs.begin(),supportPairs.end()),supportPairs.end());
            }
            ++stats.impactEvents;stats.impactSafetyFallback+=result.safetyFallback;
            contactCache.erase(std::remove_if(contactCache.begin(),contactCache.end(),
                [&](const CachedContact& c){return included[c.key.slotA] || included[c.key.slotB];}),contactCache.end());
            std::vector<unsigned> changed;
            for(std::size_t i=0;i<island.size();++i) {
                const auto slot=island[i];auto& b=bodies[slot];
                if(SamePosition(oldVelocities[i].first,b.rigidBody.linearVelocity) &&
                   SamePosition(oldVelocities[i].second,b.rigidBody.angularVelocity)) continue;
                changed.push_back(slot);
                b.motion.ChangeVelocity(b.rigidBody,now);
                InvalidateImpactPose(slot);
                CoverStepReach(b,static_cast<float>(dt-now));
                supportPairs.erase(std::remove_if(supportPairs.begin(),supportPairs.end(),[&](const auto& pair) {
                    return (pair[0]==slot || pair[1]==slot) &&
                        std::find(eventPairs.begin(),eventPairs.end(),pair)==eventPairs.end();
                }),supportPairs.end());
            }
            // Cached event times depend only on the two anchored trajectories.
            // Recompute incident queries after an actual velocity change; retain
            // every unaffected event (including disjoint simultaneous events).
            std::vector<std::array<unsigned,4>> dirty;
            dirty.push_back(selected); // retire the event even if its impulse is zero
            for(const auto& event:scheduledImpacts)
                if(std::find(changed.begin(),changed.end(),event.key[0])!=changed.end() ||
                   std::find(changed.begin(),changed.end(),event.key[1])!=changed.end()) dirty.push_back(event.key);
            for(unsigned slot:changed) {
                const auto candidates=QuerySlots(tree.FatAabb(bodies[slot].proxy));
                for(unsigned other:candidates) if(other!=slot && CanRespond(slot,other)) {
                    const auto a=std::min(slot,other),b=std::max(slot,other);
                    for(int pa=0;pa<PrimitiveCount(bodies[a].shape);++pa)
                        for(int pb=0;pb<PrimitiveCount(bodies[b].shape);++pb)
                            dirty.push_back({a,b,unsigned(pa),unsigned(pb)});
                }
            }
            std::sort(dirty.begin(),dirty.end());dirty.erase(std::unique(dirty.begin(),dirty.end()),dirty.end());
            for(const auto& key:dirty) schedule(key);
            if(now>=dt) break;
        }
        for(const auto slot:aliveSlots) if(bodies[slot].HasMotion()&&!bodies[slot].sleeping) {
            auto& body=bodies[slot];const auto pose=body.motion.Evaluate(dt);
            stats.motionSegments+=body.motion.Segments().size();stats.motionStorageBytes+=body.motion.StorageBytes();
            body.rigidBody.position=pose.position;body.rigidBody.orientation=pose.orientation;
            if(body.isKinematic){body.rigidBody.linearVelocity=pose.linearVelocity;body.rigidBody.angularVelocity=pose.angularVelocity;
                if(body.kinematic.control==KinematicControl::Target){body.kinematic.remainingSeconds=std::max(0.f,body.kinematic.remainingSeconds-float(body.kinematicActiveDuration));
                    if(body.kinematic.remainingSeconds==0)body.kinematic.control=KinematicControl::Stopped;}}
        }
    }

    void PublishKinematics(float dt) {
        for(auto slot:aliveSlots){auto& body=bodies[slot];if(!body.isKinematic)continue;
            auto& rigid=body.rigidBody;auto& command=body.kinematic;
            body.previousPosition=rigid.position;body.previousOrientation=rigid.orientation;
            CurrentBound(body);body.previousBound=body.currentBound;
            rigid.linearVelocity=rigid.angularVelocity=glm::vec3(0);
            body.kinematicActiveDuration=0;
            if(body.enabled&&command.control==KinematicControl::Velocity){
                rigid.linearVelocity=command.linearVelocity;rigid.angularVelocity=command.angularVelocity;
                body.kinematicActiveDuration=dt;
            }else if(body.enabled&&command.control==KinematicControl::Target){
                if(command.targetNextStep){command.remainingSeconds=dt;command.targetNextStep=false;}
                if(command.remainingSeconds>0){
                    const auto targetCenter=command.target.position+command.target.rotation*body.shape.pivotOffset;
                    rigid.linearVelocity=(targetCenter-rigid.position)/command.remainingSeconds;
                    // World-relative shortest arc. Equivalent quaternion signs
                    // are the same target; full spins use velocity control.
                    glm::quat delta=command.target.rotation*glm::conjugate(rigid.orientation);
                    if(delta.w<0)delta=-delta;
                    const double sine=glm::length(glm::dvec3(delta.x,delta.y,delta.z));
                    if(sine>0&&!SameRotation(command.target.rotation,rigid.orientation)){const double angle=2*std::atan2(sine,double(delta.w));
                        rigid.angularVelocity=glm::vec3(glm::dvec3(delta.x,delta.y,delta.z)*(angle/(sine*command.remainingSeconds)));}
                    body.kinematicActiveDuration=std::min(double(dt),double(command.remainingSeconds));
                }
            }
            rigid.ClearAccumulators();body.deferredAcceleration=false;
            body.motion.BeginPrescribed(MakeHandle(slot).id,rigid,dt,body.kinematicActiveDuration);
            InvalidateImpactPose(slot);
        }
    }

    // Milestone 32 warm-start state: last step's converged impulses, sorted
    // by key, and the keys/anchors of this step's constraints in solver order.
    std::vector<CachedContact> contactCache;
    std::vector<CachedContact> pendingCache;
    std::vector<char> cacheUsed;
    PhysicsWorld::StepStats stats;
    std::size_t reinsertionsSinceStep = 0;
    struct CacheCounters {
        std::size_t orientationHits=0, orientationRebuilds=0;
        std::size_t boundHits=0, boundRebuilds=0, shapeRebuilds=0, allocations=0, allocatedBytes=0;
    } cacheCounters;

    static bool SameFloat(float a, float b) {
        return std::memcmp(&a,&b,sizeof(float))==0;
    }
    static bool SamePosition(const glm::vec3& a,const glm::vec3& b) {
        return SameFloat(a.x,b.x) && SameFloat(a.y,b.y) && SameFloat(a.z,b.z);
    }
    static bool SameRotation(const glm::quat& a,const glm::quat& b) {
        return SameFloat(a.w,b.w) && SameFloat(a.x,b.x) && SameFloat(a.y,b.y) && SameFloat(a.z,b.z);
    }
    static bool Matches(const PoseBound& bound,const glm::vec3& p,const glm::quat& q) {
        return bound.valid && SamePosition(bound.position,p) && SameRotation(bound.orientation,q);
    }
    const ContactPreparedOrientation& Orientation(Body& body) {
        if (body.orientationCache && body.orientationCache->Matches(body.rigidBody.orientation)) {
            ++cacheCounters.orientationHits;
        } else {
            body.orientationCache.emplace(body.rigidBody.orientation);
            body.boundPreparationValid = false;
            ++cacheCounters.orientationRebuilds;
        }
        return *body.orientationCache;
    }
    const Aabb& CurrentBound(Body& body) {
        const auto& p=body.rigidBody.position;
        const auto& q=body.rigidBody.orientation;
        if (Matches(body.currentBound,p,q)) {
            ++cacheCounters.boundHits;
            return body.currentBound.bound;
        }
        const auto& orientation=Orientation(body);
        if (!body.boundPreparationValid) {
            if (!body.boundPreparation) body.boundPreparation.emplace();
            const auto capacity=body.boundPreparation->children.capacity();
            PrepareShapeBounds(body.shape,orientation,*body.boundPreparation);
            if (body.boundPreparation->children.capacity()!=capacity) {
                ++cacheCounters.allocations;
                cacheCounters.allocatedBytes+=body.boundPreparation->children.capacity()*sizeof(PreparedBoxBound);
            }
            body.boundPreparationValid=true;
        }
        body.currentBound={p,q,ShapeAabb(*body.boundPreparation,p),true};
        ++cacheCounters.boundRebuilds;
        return body.currentBound.bound;
    }
    const Aabb& PreviousBound(Body& body) {
        const auto& p=body.previousPosition;
        const auto& q=body.previousOrientation;
        if (Matches(body.previousBound,p,q)) {
            ++cacheCounters.boundHits;
        } else if (Matches(body.currentBound,p,q)) {
            body.previousBound=body.currentBound;
            ++cacheCounters.boundHits;
        } else {
            // Normally captured before integration. This independent fallback
            // preserves the old pose if a caller introduces another transition.
            body.previousBound={p,q,ShapeAabb(body.shape,p,q),true};
            ++cacheCounters.boundRebuilds;
        }
        return body.previousBound.bound;
    }
    std::size_t CacheBytes() const {
        std::size_t bytes=bodies.capacity()*(sizeof(float)+sizeof(std::optional<ContactPreparedOrientation>)+
            sizeof(std::optional<PreparedShapeBounds>)+sizeof(bool)+2*sizeof(PoseBound));
        for (const Body& body:bodies) if (body.boundPreparation)
            bytes+=body.boundPreparation->children.capacity()*sizeof(PreparedBoxBound);
        return bytes+impactPoseSamples.capacity()*sizeof(ImpactPoseSample)+solver.FrameStorageBytes();
    }
    void EnsureImpactPoseStorage(){
        if(impactPoseSamples.size()>=bodies.size())return;
        const auto capacity=impactPoseSamples.capacity();impactPoseSamples.resize(bodies.size());
        if(impactPoseSamples.capacity()!=capacity){++cacheCounters.allocations;cacheCounters.allocatedBytes+=impactPoseSamples.capacity()*sizeof(ImpactPoseSample);}
    }

    // The tight broadphase bound a body must stay inside. For a dynamic
    // body it covers both the previous and the current fixed-step pose —
    // player sweeps interpolate between the two — and, if the body turned
    // during the step, the orientation-independent bounding sphere at both
    // positions (an interpolated orientation can poke outside both
    // endpoint boxes; it can never leave the swept sphere).
    Aabb TightBound(Body& body) {
        const Aabb current = CurrentBound(body);
        if (!body.HasMotion()) { body.previousBound=body.currentBound; return current; }
        Aabb bound = current.Union(PreviousBound(body));
        const float turn = std::abs(glm::dot(body.previousOrientation, body.rigidBody.orientation));
        if (turn < 1.0f - 1.0e-7f) {
            const float r = body.boundingRadius;
            bound = bound.Union(Aabb{body.previousPosition - glm::vec3(r), body.previousPosition + glm::vec3(r)});
            bound = bound.Union(Aabb{body.rigidBody.position - glm::vec3(r), body.rigidBody.position + glm::vec3(r)});
        }
        if (body.motion.Segments().size()>1) for(const auto& segment:body.motion.Segments()) {
            const float r=body.boundingRadius;
            bound=bound.Union(Aabb{glm::min(segment.position,segment.endPosition)-glm::vec3(r),
                                  glm::max(segment.position,segment.endPosition)+glm::vec3(r)});
        }
        return bound;
    }

    // Milestone 32: how far this body's surface can move within the current
    // step at its post-force velocities — the most any of its contacts can
    // close before the next detection.
    float StepReach(const Body& body, float fixedDeltaTime) const {
        return (glm::length(body.rigidBody.linearVelocity) +
                glm::length(body.rigidBody.angularVelocity) * body.boundingRadius) *
               fixedDeltaTime;
    }

    // Before candidate generation: a dynamic proxy's fat bound covers the
    // body grown by its reach, so every pair that can come into contact
    // within this step is a candidate (speculative contacts).
    void CoverStepReach(Body& body, float fixedDeltaTime) {
        const Aabb reach = TightBound(body).Expanded(StepReach(body, fixedDeltaTime));
        if (tree.FatAabb(body.proxy).Contains(reach)) return;
        tree.MoveProxy(body.proxy, reach.Expanded(kBroadphaseMargin));
        ++reinsertionsSinceStep;
    }

    void RefreshProxy(Body& body) {
        const Aabb tight = TightBound(body);
        if (tree.FatAabb(body.proxy).Contains(tight)) return;
        tree.MoveProxy(body.proxy, tight.Expanded(kBroadphaseMargin));
        ++reinsertionsSinceStep;
    }

    // Live slots whose fat bounds overlap `box`, ascending.
    const std::vector<unsigned int>& QuerySlots(const Aabb& box) const {
        queryScratch.clear();
        tree.Query(box, [&](int proxy) { queryScratch.push_back(tree.UserData(proxy)); });
        std::sort(queryScratch.begin(), queryScratch.end());
        return queryScratch;
    }

    // Candidate pairs (lo, hi) for every pair with at least one movable
    // body, in ascending lexicographic slot order — the same order the
    // pre-M32 all-pairs loop visited them in, restricted to candidates.
    bool CanCollide(unsigned a,unsigned b) const {
        auto ha=MakeHandle(a).id,hb=MakeHandle(b).id;
        if(!suppressedPairs.empty()&&suppressedPairs.count(std::minmax(ha,hb)))return false;
        return bodies[a].enabled&&bodies[b].enabled&&CollisionPermitted(bodies[a].collisionLayer,bodies[a].collisionMask,bodies[b].collisionLayer,bodies[b].collisionMask);
    }
    bool CanRespond(unsigned a,unsigned b) const {return CanCollide(a,b)&&!bodies[a].sensor&&!bodies[b].sensor&&!bodies[a].queryOnly&&!bodies[b].queryOnly&&!( (bodies[a].sleeping||bodies[a].rigidBody.IsStatic())&&(bodies[b].sleeping||bodies[b].rigidBody.IsStatic()) );}
    bool MatchesQuery(unsigned slot,const PhysicsQueryFilter& filter) const {
        const auto& b=bodies[slot]; if(!b.enabled||!b.queriesEnabled||(b.sensor&&!filter.includeSensors))return false; const auto bit=CategoryBit(b.collisionLayer);
        if(!(filter.includeLayers&bit)||(filter.excludeLayers&bit)||
           (b.tags&filter.requiredTags)!=filter.requiredTags||(b.tags&filter.excludedTags))return false;
        for(const auto handle:filter.ignoredBodies)if(Get(handle)&&handle.id==MakeHandle(slot).id)return false;
        return true;
    }
    void GenerateCandidatePairs() {
        candidatePairs.clear();
        for (const unsigned int slot : aliveSlots) {
            const Body& a = bodies[slot];
            if (!a.enabled || (!a.HasMotion()&&!a.sensor&&!a.queryOnly)) continue;
            tree.Query(tree.FatAabb(a.proxy), [&](int proxy) {
                const unsigned int other = tree.UserData(proxy);
                if (other == slot) return;
                const Body& b = bodies[other];
                // A pair of two movable bodies is found by both queries;
                // keep it from the lower slot's query only.
                if ((b.HasMotion()||b.sensor||b.queryOnly) && other < slot) return;
                if(!CanCollide(slot,other)){++stats.layerRejectedPairs;return;}
                candidatePairs.emplace_back(std::min(slot, other), std::max(slot, other));
            });
        }
        std::sort(candidatePairs.begin(), candidatePairs.end());
    }

    BodyHandle MakeHandle(unsigned int slot) const {
        BodyHandle handle;
        handle.id = slot | (bodies[slot].generation << kSlotBits);handle.world=worldToken;
        return handle;
    }

    unsigned playerCollisionLayer=0;
    CategoryMask playerCollisionMask=kAllCategories;
    bool hasPlayerShape = false;
    Shape playerShape;

    // A member of Impl (rather than a free function taking a body list)
    // specifically so it can name `Body` without exposing this private
    // nested type outside PhysicsWorld.cpp.
    // Milestone 32: evaluates only `candidates` (the broadphase's answer for
    // the whole sweep). Any body not among them has a positive distance, so
    // it could never be the reported closest body of a hit.
    ClosestBodyResult ClosestBodyToCapsule(const std::vector<unsigned int>& candidates,
                                            const glm::vec3& segA, const glm::vec3& segB,
                                            float capsuleRadius,
                                            float bodyMotionAlpha,
                                            bool interpolateDynamicBodyMotion) const {
        ClosestBodyResult result;
        for (const unsigned int i : candidates) {
            const Body& body = bodies[i];
            if(!body.enabled||body.sensor||body.queryOnly)continue;
            const bool interpolateBody = interpolateDynamicBodyMotion && body.HasMotion();
            const bool ledger=interpolateBody && bodyMotionAlpha<1.0f && !body.motion.Segments().empty();
            const auto observed=ledger ? body.motion.Evaluate(std::clamp(double(bodyMotionAlpha),0.0,1.0)*stepDuration) : body.rigidBody;
            const glm::vec3 bodyPosition = ledger ? observed.position : interpolateBody
                ? glm::mix(body.previousPosition, body.rigidBody.position, bodyMotionAlpha)
                : body.rigidBody.position;
            const glm::quat bodyOrientation = ledger ? observed.orientation : interpolateBody
                ? glm::normalize(glm::slerp(body.previousOrientation, body.rigidBody.orientation,
                                            bodyMotionAlpha))
                : body.rigidBody.orientation;
            RigidBody sampledBody = body.rigidBody;
            sampledBody.position = bodyPosition;
            sampledBody.orientation = bodyOrientation;
            if (body.shape.type == ShapeType::Terrain) {
                if (!body.shape.terrain) continue;
                const float segmentLength = glm::length(segB - segA);
                const float minimumEndRadius = std::min(glm::length(segA - bodyPosition),
                                                        glm::length(segB - bodyPosition));
                if (minimumEndRadius - segmentLength >
                    body.shape.terrain->BoundRadius() + capsuleRadius) continue;
                // Sampling the whole capsule core, rather than just its
                // lower endpoint, also handles arbitrary capsule attitude
                // and terrain slopes without a universal up axis.
                constexpr int kSegmentSamples = 9;
                for (int sampleIndex = 0; sampleIndex < kSegmentSamples; ++sampleIndex) {
                    const float t = static_cast<float>(sampleIndex) /
                                    static_cast<float>(kSegmentSamples - 1);
                    const TerrainSample sample = SampleTerrainAtWorld(
                        *body.shape.terrain, sampledBody, glm::mix(segA, segB, t));
                    const float distance = sample.signedDistance - capsuleRadius;
                    if (distance < result.distance) {
                        result.distance = distance;
                        result.normal = sample.outwardNormal;
                        result.point=glm::mix(segA,segB,t)-sample.outwardNormal*sample.signedDistance;
                        result.bodyIndex = static_cast<int>(i);
                    }
                }
                continue;
            }
            for (int part = 0; part < PrimitiveCount(body.shape); ++part) {
                const PrimitivePose primitive = PrimitiveAt(body.shape, sampledBody, part);
                CapsuleDistance capsuleDistance;
                if (primitive.shape.type == ShapeType::Sphere) {
                    capsuleDistance = CapsuleDistanceToSphere(segA, segB, capsuleRadius,
                                                               primitive.body.position,
                                                               primitive.shape.radius);
                } else if (primitive.shape.type == ShapeType::Box) {
                    capsuleDistance = CapsuleDistanceToBox(segA, segB, capsuleRadius,
                                                            primitive.body.position,
                                                            primitive.body.orientation,
                                                            primitive.shape.halfExtents);
                } else if(primitive.shape.asset) {
                    const auto d=SegmentGeometry(glm::dvec3(segA),glm::dvec3(segB),capsuleRadius,primitive);
                    if(!d.valid)continue;
                    capsuleDistance={float(d.gap),glm::vec3(d.normal),glm::vec3(d.point)};
                } else {continue;}
                if (capsuleDistance.distance < result.distance) {
                    result.distance = capsuleDistance.distance;
                    result.normal = capsuleDistance.normal;
                    result.point=capsuleDistance.otherPoint;
                    result.bodyIndex = static_cast<int>(i);
                }
            }
        }
        return result;
    }

    Body* Get(BodyHandle handle) {
        if (!handle.IsValid()||(handle.world&&handle.world!=worldToken)) return nullptr;
        const unsigned int slot = handle.id & kSlotMask;
        if (slot >= bodies.size() || !bodies[slot].alive ||
            bodies[slot].generation != (handle.id >> kSlotBits)) {
            return nullptr;
        }
        return &bodies[slot];
    }
    const Body* Get(BodyHandle handle) const {
        if (!handle.IsValid()||(handle.world&&handle.world!=worldToken)) return nullptr;
        const unsigned int slot = handle.id & kSlotMask;
        if (slot >= bodies.size() || !bodies[slot].alive ||
            bodies[slot].generation != (handle.id >> kSlotBits)) {
            return nullptr;
        }
        return &bodies[slot];
    }

    BodyHandle AddBody(const Shape& shape, const glm::vec3& position, const glm::quat& rotation,
                        bool isDynamic, float mass, float friction, float restitution) {
        Body body;
        body.shape = shape;
        body.boundingRadius = ShapeBoundingRadius(shape);
        ++cacheCounters.shapeRebuilds;
        body.friction = friction;
        body.restitution = restitution;
        body.isDynamic = isDynamic;
        body.alive = true;
        body.rigidBody.position = position;
        body.rigidBody.orientation = rotation;
        body.previousPosition = position;
        body.previousOrientation = rotation;
        if (isDynamic) {
            body.rigidBody.inverseMass = mass > 0.0f ? 1.0f / mass : 0.0f;
            if (shape.type == ShapeType::Sphere) {
                body.rigidBody.inverseInertiaLocal = SolidSphereInverseInertia(mass, shape.radius);
            } else if (shape.type == ShapeType::Box) {
                body.rigidBody.inverseInertiaLocal = SolidBoxInverseInertia(mass, shape.halfExtents);
            } else if (shape.type == ShapeType::CompoundBoxes) {
                body.rigidBody.inverseInertiaLocal = CompoundInverseInertia(mass, shape.boxes);
            }
        }
        // Static bodies keep inverseMass=0 / zero inverse inertia (RigidBody's own defaults),
        // which is exactly what "never moved by force or impulse" means throughout this engine.
        unsigned int slot;
        if (!freeSlots.empty()) {
            slot = freeSlots.back();
            freeSlots.pop_back();
            body.generation = (bodies[slot].generation + 1u) & ((1u << (32u - kSlotBits)) - 1u);
            bodies[slot] = body;
        } else {
            slot = static_cast<unsigned int>(bodies.size());
            bodies.push_back(body);
        }
        aliveSlots.insert(std::lower_bound(aliveSlots.begin(), aliveSlots.end(), slot), slot);
        bodies[slot].proxy = tree.CreateProxy(TightBound(bodies[slot]).Expanded(kBroadphaseMargin), slot);
        return MakeHandle(slot);
    }

    void Remove(BodyHandle handle) {
        Body* body = Get(handle);
        if (!body) return;
        const unsigned int slot = handle.id & kSlotMask;
        body->alive = false;
        tree.DestroyProxy(body->proxy);
        body->proxy = DynamicAabbTree::kNull;
        freeSlots.push_back(slot);
        const auto it = std::lower_bound(aliveSlots.begin(), aliveSlots.end(), slot);
        if (it != aliveSlots.end() && *it == slot) aliveSlots.erase(it);
    }
};

bool PhysicsWorld::Init() {
    m_impl = new Impl();
    return true;
}

PhysicsWorld::~PhysicsWorld() { Shutdown(); }

void PhysicsWorld::Shutdown() {
    delete m_impl;
    m_impl = nullptr;
}

BodyHandle PhysicsWorld::CreateShape(const Shape& input,const BodyTransform& pose,bool dynamic,float mass,float friction,float restitution) {
    Shape shape=input;
    auto positive=[](glm::vec3 h){return std::isfinite(h.x)&&std::isfinite(h.y)&&std::isfinite(h.z)&&h.x>0&&h.y>0&&h.z>0;};
    auto valid=[&](const Shape& s){if(s.type==ShapeType::Box&&!positive(s.halfExtents))throw std::invalid_argument("box dimensions must be finite positive");if(s.type==ShapeType::Sphere&&(!std::isfinite(s.radius)||s.radius<=0))throw std::invalid_argument("sphere radius must be finite positive");if((s.type==ShapeType::ConvexHull||s.type==ShapeType::TriangleMesh)&&(!s.asset||s.asset->convex!=(s.type==ShapeType::ConvexHull)))throw std::invalid_argument("cooked shape kind/asset mismatch");};valid(shape);
    if(!std::isfinite(pose.position.x)||!std::isfinite(pose.position.y)||!std::isfinite(pose.position.z)||!std::isfinite(friction)||friction<0||!std::isfinite(restitution)||restitution<0||restitution>1)throw std::invalid_argument("invalid body pose/material");
    if(shape.type==ShapeType::TriangleMesh&&dynamic)throw std::invalid_argument("concave triangle collision is static-only; use convex/compound");
    if(shape.type==ShapeType::TriangleMesh&&!shape.asset)throw std::invalid_argument("missing cooked mesh");
    if(shape.type==ShapeType::CompoundBoxes){if(shape.boxes.empty()||shape.boxes.size()>64)throw std::invalid_argument("compound limit: 1..64 children");std::set<uint32_t> keys;for(auto& c:shape.boxes){if(!c.key)c.key=uint32_t(&c-shape.boxes.data()+1);if(!keys.insert(c.key).second)throw std::invalid_argument("duplicate compound child key");if(c.type!=ShapeType::Box&&c.type!=ShapeType::Sphere&&c.type!=ShapeType::ConvexHull)throw std::invalid_argument("unsupported compound child shape");ContactRotation(c.rotation);if(!std::isfinite(c.localCenter.x)||!std::isfinite(c.localCenter.y)||!std::isfinite(c.localCenter.z))throw std::invalid_argument("nonfinite child position");valid(c.type==ShapeType::Box?Shape::Box(c.halfExtents):c.type==ShapeType::Sphere?Shape::Sphere(c.radius):Shape::Cooked(c.asset,c.assetId));}}
    ContactRotation(pose.rotation);
    CollisionMassProperties properties;
    if(dynamic){if(!(mass>0)||!std::isfinite(mass))throw std::invalid_argument("dynamic mass must be positive");properties=ShapeMassProperties(shape);shape.pivotOffset=glm::vec3(properties.center);}
    const auto center=pose.position+pose.rotation*shape.pivotOffset;
    auto handle=m_impl->AddBody(shape,center,pose.rotation,dynamic,mass,friction,restitution);
    if(dynamic)m_impl->Get(handle)->rigidBody.inverseInertiaLocal=glm::mat3(glm::inverse(properties.inertia*(double(mass)/properties.volume)));
    return handle;
}

BodyHandle PhysicsWorld::CreateStaticBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                          float friction, float restitution) {
    return CreateStaticBox(position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), halfExtents, friction,
                            restitution);
}

BodyHandle PhysicsWorld::CreateStaticBox(const glm::vec3& position, const glm::quat& rotation,
                                          const glm::vec3& halfExtents, float friction,
                                          float restitution) {
    return m_impl->AddBody(Shape::Box(halfExtents), position, rotation, false, 0.0f, friction,
                            restitution);
}

BodyHandle PhysicsWorld::CreateStaticSphere(const glm::vec3& position, float radius,
                                             float friction, float restitution) {
    return m_impl->AddBody(Shape::Sphere(radius), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                            false, 0.0f, friction, restitution);
}

BodyHandle PhysicsWorld::CreateStaticTerrain(const glm::vec3& position,
                                              const glm::quat& rotation,
                                              std::shared_ptr<const RadialTerrain> terrain,
                                              float friction, float restitution) {
    if (!terrain) return BodyHandle{};
    return m_impl->AddBody(Shape::Terrain(std::move(terrain)), position, rotation,
                           false, 0.0f, friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicBox(const glm::vec3& position, const glm::vec3& halfExtents,
                                           float mass, float friction, float restitution) {
    return m_impl->AddBody(Shape::Box(halfExtents), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                            true, mass, friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicSphere(const glm::vec3& position, float radius, float mass,
                                              float friction, float restitution) {
    return m_impl->AddBody(Shape::Sphere(radius), position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), true,
                            mass, friction, restitution);
}

BodyHandle PhysicsWorld::CreateDynamicCompoundBoxes(const glm::vec3& position,
                                                     const std::vector<CompoundBox>& boxes,
                                                     float mass, float friction, float restitution) {
    if (boxes.empty() || mass <= 0.0f) return BodyHandle{};
    for (const CompoundBox& box : boxes) {
        if (box.halfExtents.x <= 0.0f || box.halfExtents.y <= 0.0f || box.halfExtents.z <= 0.0f) {
            return BodyHandle{};
        }
    }
    return m_impl->AddBody(Shape::Compound(boxes), position,
                            glm::quat(1.0f, 0.0f, 0.0f, 0.0f), true,
                            mass, friction, restitution);
}

void PhysicsWorld::DestroyBody(BodyHandle handle) {
    Wake(handle);
    if(m_impl){for(auto it=m_impl->suppressedPairs.begin();it!=m_impl->suppressedPairs.end();){if(it->first==handle.id||it->second==handle.id)it=m_impl->suppressedPairs.erase(it);else ++it;}}
    if(m_impl){auto& joints=m_impl->joints;joints.erase(std::remove_if(joints.begin(),joints.end(),[&](const auto& j){return j.state.settings.bodyA.id==handle.id||j.state.settings.bodyB.id==handle.id;}),joints.end());}
    m_impl->Remove(handle);
}

std::size_t PhysicsWorld::AliveBodyCount() const {
    return m_impl->aliveSlots.size();
}

const std::vector<PhysicsWorld::DebugContact>& PhysicsWorld::LastStepContacts() const {
    return m_impl->lastStepContacts;
}

std::size_t PhysicsWorld::DynamicBodyCount() const {
    std::size_t count = 0;
    for (const unsigned int slot : m_impl->aliveSlots) {
        if (m_impl->bodies[slot].isDynamic) ++count;
    }
    return count;
}

void PhysicsWorld::ApplyLinearAcceleration(BodyHandle handle, const glm::vec3& acceleration,
                                            float fixedDeltaTime) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || body->rigidBody.IsStatic()) return;
    if(body->sleeping&&glm::length(acceleration-body->lastAcceleration)>1e-5f)m_impl->Wake(handle.id&Impl::kSlotMask);
    body->lastAcceleration=acceleration;
    if(body->sleeping){body->deferredAcceleration=true;return;}
    body->deferredAcceleration=false;body->rigidBody.linearVelocity += acceleration * fixedDeltaTime;
}

void PhysicsWorld::ApplyForce(BodyHandle handle, const glm::vec3& force) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || body->rigidBody.IsStatic()) return;
    if(glm::length(force)>1e-8f)m_impl->Wake(handle.id&Impl::kSlotMask);
    body->rigidBody.ApplyForce(force);
}

void PhysicsWorld::ApplyTorque(BodyHandle handle, const glm::vec3& torque,bool wake) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || body->rigidBody.IsStatic()) return;
    // Unchanged pose drives retain the normal quiet-time clock at equilibrium.
    // Existing callers keep their normal explicit wake behaviour.
    if(wake&&glm::length(torque)>1e-8f)m_impl->Wake(handle.id&Impl::kSlotMask);
    body->rigidBody.ApplyTorque(torque);
}

bool PhysicsWorld::IsDynamicBody(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    return body && body->isDynamic;
}

bool PhysicsWorld::IsKinematicBody(BodyHandle handle) const {
    const auto* body=m_impl->Get(handle);return body&&body->isKinematic;
}
BodyMotionType PhysicsWorld::GetMotionType(BodyHandle handle) const {
    const auto* body=m_impl->Get(handle);
    return body&&body->isKinematic?BodyMotionType::Kinematic:body&&body->isDynamic?BodyMotionType::Dynamic:BodyMotionType::Static;
}
bool PhysicsWorld::SetMotionType(BodyHandle handle,BodyMotionType mode,float mass,bool preserveVelocity) {
    auto* body=m_impl->Get(handle);
    if(!body||body->queryOnly||(mode!=BodyMotionType::Static&&mode!=BodyMotionType::Dynamic&&mode!=BodyMotionType::Kinematic))return false;
    if(GetMotionType(handle)==mode)return true;
    if(mode!=BodyMotionType::Static&&(!KinematicShape(body->shape)||!std::isfinite(mass)||mass<=0))return false;
    // A joint's anchors are COM-local. Changing mass authority/COM while it
    // participates would silently reinterpret the connection; reject atomically.
    for(const auto& j:m_impl->joints)if(j.state.settings.bodyA.id==handle.id||j.state.settings.bodyB.id==handle.id)return false;
    Shape shape=body->shape;glm::mat3 inverseInertia(0);float inverseMass=0;
    try{
        if(mode!=BodyMotionType::Static){const auto properties=ShapeMassProperties(shape);
            shape.pivotOffset=glm::vec3(properties.center);
            if(mode==BodyMotionType::Dynamic){inverseMass=1/mass;
                inverseInertia=glm::mat3(glm::inverse(properties.inertia*(double(mass)/properties.volume)));}
        }
    }catch(const std::exception&){return false;}
    if(!std::isfinite(inverseMass)||!FiniteVector(shape.pivotOffset))return false;
    for(int column=0;column<3;++column)if(!FiniteVector(inverseInertia[column]))return false;
    const auto pivot=GetTransform(handle);const auto oldLinear=body->rigidBody.linearVelocity;
    const auto oldAngular=body->rigidBody.angularVelocity;
    if(!FiniteVector(pivot.position+pivot.rotation*shape.pivotOffset)||
       (preserveVelocity&&(!UsableVelocity(oldLinear)||!UsableVelocity(oldAngular))))return false;
    m_impl->Wake(handle.id&Impl::kSlotMask);
    for(auto i:m_impl->QuerySlots(m_impl->CurrentBound(*body)))if(m_impl->bodies[i].sleeping)m_impl->Wake(i);
    body->shape=std::move(shape);body->boundingRadius=ShapeBoundingRadius(body->shape);
    body->isDynamic=mode==BodyMotionType::Dynamic;body->isKinematic=mode==BodyMotionType::Kinematic;
    body->rigidBody.inverseMass=inverseMass;body->rigidBody.inverseInertiaLocal=inverseInertia;
    body->rigidBody.position=pivot.position+pivot.rotation*body->shape.pivotOffset;
    body->rigidBody.linearVelocity=preserveVelocity&&mode!=BodyMotionType::Static?oldLinear:glm::vec3(0);
    body->rigidBody.angularVelocity=preserveVelocity&&mode!=BodyMotionType::Static?oldAngular:glm::vec3(0);
    body->rigidBody.ClearAccumulators();body->lastAcceleration=glm::vec3(0);body->deferredAcceleration=false;
    body->kinematic={};body->kinematicActiveDuration=0;
    if(body->isKinematic&&preserveVelocity){body->kinematic.control=KinematicControl::Velocity;
        body->kinematic.linearVelocity=oldLinear;body->kinematic.angularVelocity=oldAngular;}
    body->motion.Clear();body->previousPosition=body->rigidBody.position;body->previousOrientation=body->rigidBody.orientation;
    body->currentBound.valid=body->previousBound.valid=false;body->boundPreparationValid=false;
    // Disposable solver history belongs to the old authority, never the handle
    // itself. Contact-event identity remains continuous across a safe handoff.
    const auto slot=handle.id&Impl::kSlotMask;
    auto uses=[&](const auto& entry){return entry.key.slotA==slot||entry.key.slotB==slot;};
    auto& cache=m_impl->contactCache;cache.erase(std::remove_if(cache.begin(),cache.end(),uses),cache.end());
    m_impl->InvalidateImpactPose(slot);m_impl->RefreshProxy(*body);
    return true;
}
bool PhysicsWorld::MoveKinematic(BodyHandle handle,const BodyTransform& target,float seconds) {
    auto* body=m_impl->Get(handle);
    if(!body||!body->enabled||!body->isKinematic||!KinematicShape(body->shape)||!UsableKinematicPose(target)||
       !std::isfinite(seconds)||seconds<0||seconds>60)return false;
    auto normalized=target;
    const auto anchor=body->rigidBody.orientation;
    normalized.rotation=Impl::SameRotation(target.rotation,anchor)||Impl::SameRotation(-target.rotation,anchor)?anchor:glm::normalize(target.rotation);
    const auto center=normalized.position+normalized.rotation*body->shape.pivotOffset;
    if(!FiniteVector(center)||!FiniteVector(center-body->rigidBody.position)||
       !std::isfinite(glm::length(center-body->rigidBody.position))||
       (seconds>0&&(!UsableVelocity((center-body->rigidBody.position)/seconds)||!std::isfinite(glm::pi<float>()/seconds))))return false;
    KinematicMotionState command;command.control=KinematicControl::Target;command.target=normalized;
    command.remainingSeconds=seconds;command.targetNextStep=seconds==0;
    body->kinematic=command;return true;
}
bool PhysicsWorld::SetKinematicVelocity(BodyHandle handle,const glm::vec3& linear,const glm::vec3& angular) {
    auto* body=m_impl->Get(handle);
    if(!body||!body->enabled||!body->isKinematic||!KinematicShape(body->shape)||!UsableVelocity(linear)||!UsableVelocity(angular))return false;
    KinematicMotionState command;command.control=KinematicControl::Velocity;
    command.linearVelocity=linear;command.angularVelocity=angular;body->kinematic=command;return true;
}
bool PhysicsWorld::StopKinematic(BodyHandle handle) {
    auto* body=m_impl->Get(handle);if(!body||!body->isKinematic)return false;
    body->kinematic={};return true;
}
bool PhysicsWorld::GetKinematicMotion(BodyHandle handle,KinematicMotionState& state) const {
    const auto* body=m_impl->Get(handle);if(!body||!body->isKinematic)return false;
    state=body->kinematic;return true;
}
glm::vec3 PhysicsWorld::GetPointVelocity(BodyHandle handle,const glm::vec3& point) const {
    const auto* body=m_impl->Get(handle);if(!body||!body->enabled||!FiniteVector(point))return glm::vec3(0);
    return body->rigidBody.linearVelocity+glm::cross(body->rigidBody.angularVelocity,point-body->rigidBody.position);
}
glm::vec3 PhysicsWorld::GetPreviousPointVelocity(BodyHandle handle,const glm::vec3& point) const {
    const auto* body=m_impl->Get(handle);if(!body||!body->enabled||!FiniteVector(point))return glm::vec3(0);
    return body->rigidBody.linearVelocity+glm::cross(body->rigidBody.angularVelocity,point-(body->HasMotion()?body->previousPosition:body->rigidBody.position));
}

float PhysicsWorld::GetMass(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    return body && body->isDynamic && body->rigidBody.inverseMass > 0.0f
               ? 1.0f / body->rigidBody.inverseMass
               : 0.0f;
}

void PhysicsWorld::ApplyLinearImpulse(BodyHandle handle, const glm::vec3& impulse) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || !body->isDynamic) return;
    if(glm::length(impulse)>1e-8f)m_impl->Wake(handle.id&Impl::kSlotMask);
    body->rigidBody.ApplyLinearImpulse(impulse);
}

void PhysicsWorld::ApplyImpulseAtPoint(BodyHandle handle, const glm::vec3& impulse,
                                       const glm::vec3& worldPoint) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body || !body->isDynamic) return;
    if(glm::length(impulse)>1e-8f)m_impl->Wake(handle.id&Impl::kSlotMask);
    body->rigidBody.ApplyImpulseAtPoint(impulse, worldPoint);
}

namespace {
// Algebraic preconditioner for the auxiliary normal constraints only. The
// existing solver still owns their target, nonnegative impulse and friction.
// A = J M^-1 J^T is applied from prepared lever arms/inertias, without a
// dense contact matrix or any body-mass classification.
bool AccelerateParticleNormals(const std::vector<ContactConstraint>& rows,
                               PhysicsWorld::ParticleContactStats& stats,
                               std::vector<float>& normal,
                               std::vector<glm::vec3>& tangent) {
    struct Frame { const RigidBody* body=nullptr; glm::dmat3 inverseInertia{0}; };
    struct Row { std::size_t original,a,b; glm::dvec3 n,angularA,angularB; double diagonal; };
    struct Wrench { glm::dvec3 linear{0},angular{0}; };
    std::size_t frameCount=0;
    for(const auto& c:rows) frameCount=std::max(frameCount,std::max(c.frameA,c.frameB)+1);
    std::vector<Frame> frames(frameCount);
    std::vector<Row> active;
    std::vector<double> residual;
    normal.reserve(rows.size());tangent.reserve(rows.size());
    for(std::size_t i=0;i<rows.size();++i) {
        const auto& c=rows[i];normal.push_back(c.normalImpulse);tangent.push_back(c.tangentImpulse);
        frames[c.frameA]={c.bodyA,glm::dmat3(c.inverseInertiaA)};
        frames[c.frameB]={c.bodyB,glm::dmat3(c.inverseInertiaB)};
        const glm::dvec3 n(c.normal),ra(c.velocityOffsetA),rb(c.velocityOffsetB);
        const glm::dvec3 angularA=glm::cross(ra,n),angularB=glm::cross(rb,n);
        const double speed=glm::dot(n,glm::dvec3(c.bodyA->linearVelocity)-glm::dvec3(c.bodyB->linearVelocity))+
            glm::dot(angularA,glm::dvec3(c.bodyA->angularVelocity))-
            glm::dot(angularB,glm::dvec3(c.bodyB->angularVelocity));
        const double r=double(c.restitutionBias)-speed;
        if (!(c.normalImpulse>0 || r>0)) continue;
        // Optional PCG is restricted to homogeneous prescribed normal motion.
        // A moving infinite-mass boundary can supply unbounded constraint work
        // when nearly opposing coarse particle rows form a narrow wedge. The
        // ordinary bounded PGS still processes every such row unchanged.
        const double prescribedA=glm::dot(n,glm::dvec3(c.bodyA->linearVelocity))+
            glm::dot(angularA,glm::dvec3(c.bodyA->angularVelocity));
        const double prescribedB=glm::dot(n,glm::dvec3(c.bodyB->linearVelocity))+
            glm::dot(angularB,glm::dvec3(c.bodyB->angularVelocity));
        if((c.bodyA->IsStatic() && prescribedA!=0) ||
           (c.bodyB->IsStatic() && prescribedB!=0)) {
            ++stats.normalAccelerationBoundaryDeclines;return false;
        }
        const double diagonal=(double(c.bodyA->inverseMass)+c.bodyB->inverseMass)*glm::dot(n,n)+
            glm::dot(angularA,frames[c.frameA].inverseInertia*angularA)+
            glm::dot(angularB,frames[c.frameB].inverseInertia*angularB);
        if (!(diagonal>0) || !std::isfinite(diagonal)) {
            ++stats.normalAccelerationBreakdowns;return false;
        }
        active.push_back({i,c.frameA,c.frameB,n,angularA,angularB,diagonal});residual.push_back(r);
    }
    if(active.empty()) return false;
    std::vector<Wrench> velocity(frames.size());
    const auto multiply=[&](const std::vector<double>& x,std::vector<double>& output) {
        std::fill(velocity.begin(),velocity.end(),Wrench{});
        for(std::size_t i=0;i<active.size();++i) {
            const auto& c=active[i];
            velocity[c.a].linear+=c.n*x[i];velocity[c.a].angular+=c.angularA*x[i];
            velocity[c.b].linear-=c.n*x[i];velocity[c.b].angular-=c.angularB*x[i];
        }
        for(std::size_t i=0;i<frames.size();++i) if(frames[i].body) {
            velocity[i].linear*=double(frames[i].body->inverseMass);
            velocity[i].angular=frames[i].inverseInertia*velocity[i].angular;
        }
        for(std::size_t i=0;i<active.size();++i) {
            const auto& c=active[i];
            output[i]=glm::dot(c.n,velocity[c.a].linear-velocity[c.b].linear)+
                glm::dot(c.angularA,velocity[c.a].angular)-glm::dot(c.angularB,velocity[c.b].angular);
        }
        ++stats.normalMatrixProducts;
    };
    const auto dot=[](const std::vector<double>& a,const std::vector<double>& b) {
        double value=0;for(std::size_t i=0;i<a.size();++i)value+=a[i]*b[i];return value;
    };
    const auto initialResidual=residual;
    std::vector<double> correction(active.size(),0),z(active.size()),direction(active.size()),product(active.size());
    for(std::size_t i=0;i<active.size();++i) z[i]=residual[i]/active[i].diagonal;
    direction=z;double rz=dot(residual,z);
    const double tolerance=1.0e-10*stats.speedScale;
    // Exact CG terminates in at most the active operator's rank. Redundant
    // manifold rows make it positive semidefinite; zero/nonfinite curvature
    // stops this optional acceleration, never inserts artificial stiffness.
    for(std::size_t iteration=0;iteration<active.size();++iteration) {
        double largest=0;for(double r:residual)largest=std::max(largest,std::abs(r));
        if(largest<=tolerance)break;
        multiply(direction,product);const double curvature=dot(direction,product);
        if(!(curvature>0) || !std::isfinite(curvature) || !std::isfinite(rz)) {
            ++stats.normalAccelerationBreakdowns;return false;
        }
        const double step=rz/curvature;
        for(std::size_t i=0;i<active.size();++i) {correction[i]+=step*direction[i];residual[i]-=step*product[i];}
        for(std::size_t i=0;i<active.size();++i)z[i]=residual[i]/active[i].diagonal;
        const double next=dot(residual,z);
        if(rz==0)break;
        for(std::size_t i=0;i<active.size();++i)direction[i]=z[i]+(next/rz)*direction[i];
        rz=next;
    }
    // A line search preserves the same accumulated-normal-impulse cone.
    // Also require strict descent of the frozen-tangent normal quadratic.
    double fraction=1;
    for(std::size_t i=0;i<active.size();++i) {
        if(!std::isfinite(correction[i])) {++stats.normalAccelerationBreakdowns;return false;}
        if(correction[i]<0) {
            const auto row=active[i].original;
            // Tangential impulse is frozen during the normal acceleration.
            // Its already-satisfied Coulomb load must remain available.
            const double minimum=rows[row].friction>0 ?
                glm::length(glm::dvec3(tangent[row]))/rows[row].friction : 0;
            fraction=std::min(fraction,std::max(0.0,double(normal[row])-minimum)/-correction[i]);
        }
    }
    if(!(fraction>0))return false;
    multiply(correction,product);
    const double decrease=fraction*dot(initialResidual,correction)-.5*fraction*fraction*dot(correction,product);
    if(!(decrease>0) || !std::isfinite(decrease))return false;
    bool changed=false;
    for(std::size_t i=0;i<active.size();++i) {
        const auto row=active[i].original;
        // Nonnegative accumulated impulse is the existing contact law, not
        // a velocity repair. The line search reaches its boundary exactly.
        const float value=static_cast<float>(std::max(0.0,double(normal[row])+fraction*correction[i]));
        if(!std::isfinite(value)) {++stats.normalAccelerationBreakdowns;return false;}
        changed=changed || value!=normal[row];normal[row]=value;
    }
    return changed;
}
} // namespace

void PhysicsWorld::SolveParticleContacts(std::vector<ContactParticle>& particles,
                                         const std::vector<ParticleBoundaryContact>& contacts,
                                         std::vector<glm::vec3>& particleImpulses,
                                         glm::vec3* staticSupportImpulse,
                                         float remainingRigidDt) {
    for(const auto& contact:contacts)if(contact.twoWay)Wake(contact.body);
    m_impl->particleContactStats={};
    particleImpulses.assign(contacts.size(), glm::vec3(0));
    if (staticSupportImpulse) *staticSupportImpulse = glm::vec3(0);
    if (!std::isfinite(remainingRigidDt) || remainingRigidDt<0)
        throw std::invalid_argument("invalid remaining rigid contact interval");
    if (contacts.empty()) return;
    Impl& w = *m_impl;
    const auto finite = [](const glm::vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    std::set<unsigned> islandSlots;
    std::vector<unsigned> pending;
    for (const auto& c : contacts) {
        if (c.particleIndex >= particles.size() || !finite(c.point) || !finite(c.normal) ||
            !finite(c.wallVelocity) || std::abs(glm::dot(c.normal,c.normal)-1.0f)>1.0e-3f)
            throw std::invalid_argument("invalid particle boundary contact");
        const auto& particle = particles[c.particleIndex];
        if (!(particle.mass>0) || !std::isfinite(particle.mass) || !std::isfinite(1.0f/particle.mass) ||
            !finite(particle.position) || !finite(particle.velocity))
            throw std::invalid_argument("invalid finite particle state");
        if (c.twoWay) {
            auto* body = w.Get(c.body);
            if (!body) throw std::invalid_argument("stale particle boundary owner");
            const unsigned slot = c.body.id & Impl::kSlotMask;
            if (islandSlots.insert(slot).second && !body->rigidBody.IsStatic()) pending.push_back(slot);
        }
    }
    float maximumParticleSpeed=0, maximumBodySpeed=0;
    for (const auto& p:particles) maximumParticleSpeed=std::max(maximumParticleSpeed,glm::length(p.velocity));
    const auto surfaceSpeed=[](const Impl::Body& body) {
        return glm::length(body.rigidBody.linearVelocity)+
            glm::length(body.rigidBody.angularVelocity)*body.boundingRadius;
    };
    if (remainingRigidDt>0) for (const auto slot:w.aliveSlots)
        maximumBodySpeed=std::max(maximumBodySpeed,surfaceSpeed(w.bodies[slot]));
    struct RigidRow { unsigned a,b; Contact contact; };
    std::vector<RigidRow> rigidRows;
    std::set<std::pair<unsigned,unsigned>> inspected;
    std::vector<char> supportCacheUsed(w.contactCache.size(),0);
    // Expand only through dynamic bodies. A static floor is an external
    // boundary, not a connection to every otherwise unrelated body on it.
    for (std::size_t cursor=0; cursor<pending.size(); ++cursor) {
        const unsigned slot = pending[cursor];
        const float queryMargin=(surfaceSpeed(w.bodies[slot])+maximumBodySpeed+maximumParticleSpeed)*remainingRigidDt;
        const auto neighbors = w.QuerySlots(w.CurrentBound(w.bodies[slot]).Expanded(queryMargin));
        for (const unsigned other : neighbors) {
            if (other==slot || !w.CanRespond(slot,other)) continue;
            const auto pair = std::minmax(slot,other);
            if (!inspected.emplace(pair.first,pair.second).second) continue;
            auto& a = w.bodies[pair.first]; auto& b = w.bodies[pair.second];
            const auto& oa = w.Orientation(a); const auto& ob = w.Orientation(b);
            bool touching = false;
            const float margin=(surfaceSpeed(a)+surfaceSpeed(b)+maximumParticleSpeed)*remainingRigidDt;
            for (int pa=0; pa<PrimitiveCount(a.shape); ++pa) {
                const auto childA = PrimitiveAt(a.shape,a.rigidBody,pa,&oa);
                for (int pb=0; pb<PrimitiveCount(b.shape); ++pb) {
                    const auto childB = PrimitiveAt(b.shape,b.rigidBody,pb,&ob);
                    const auto manifold = ComputeContacts(childA,childB,margin,&oa,&ob);
                    for (int k=0; k<manifold.count; ++k) {
                        const auto& c = manifold.points[k];
                        if (!c.hit) continue;
                        const double separation = c.hasLocalAnchors ? c.signedSeparation : -double(c.penetration);
                        if (separation>0) {
                            // Reuse only the ordinary solver's existing support
                            // identity/anchor/normal contract. New separated pairs
                            // still belong to normal impact timing, not this batch.
                            if (!(remainingRigidDt>0)) continue;
                            const ContactKey key{pair.first,pair.second,a.generation,b.generation,pa,pb};
                            const auto range=std::equal_range(w.contactCache.begin(),w.contactCache.end(),
                                CachedContact{key,{},{},0,{}},[](const auto& x,const auto& y){return x.key<y.key;});
                            const glm::dvec3 anchor=c.hasLocalAnchors ? c.localAnchorA :
                                glm::transpose(oa.rotation)*(glm::dvec3(c.point)-glm::dvec3(a.rigidBody.position));
                            std::ptrdiff_t best=-1;
                            double bestDistance=kWarmStartAnchorDistance;
                            for (auto old=range.first;old!=range.second;++old) {
                                const auto oldIndex=old-w.contactCache.begin();
                                if (supportCacheUsed[std::size_t(oldIndex)] || !old->key.SameBodies(key) ||
                                    glm::dot(old->normal,c.normal)<kWarmStartNormalDot) continue;
                                const double distance=glm::distance(old->localAnchorA,anchor);
                                if (distance<bestDistance) {best=oldIndex;bestDistance=distance;}
                            }
                            if (best<0) continue;
                            supportCacheUsed[std::size_t(best)]=1;
                            ++w.particleContactStats.persistentGapRows;
                        }
                        touching = true;
                        rigidRows.push_back({pair.first,pair.second,c});
                    }
                }
            }
            if (touching && islandSlots.insert(other).second && !w.bodies[other].rigidBody.IsStatic())
                pending.push_back(other);
        }
    }
    std::stable_sort(rigidRows.begin(),rigidRows.end(),[](const auto& a,const auto& b) {
        return std::tie(a.a,a.b) < std::tie(b.a,b.b);
    });
    // Finish allocation before any solver stores pointers into these arrays.
    std::vector<RigidBody> particleBodies(particles.size());
    for (std::size_t i=0; i<particles.size(); ++i) {
        particleBodies[i].position=particles[i].position;
        particleBodies[i].linearVelocity=particles[i].velocity;
        particleBodies[i].inverseMass=particles[i].mass>0 ? 1.0f/particles[i].mass : 0;
    }
    std::vector<RigidBody> rigidBodies;
    rigidBodies.reserve(islandSlots.size()+contacts.size());
    std::map<unsigned,std::size_t> index;
    for (const unsigned slot : islandSlots) {
        index.emplace(slot,rigidBodies.size()); rigidBodies.push_back(w.bodies[slot].rigidBody);
    }
    std::vector<std::size_t> boundaryIndex;
    boundaryIndex.reserve(contacts.size());
    for (const auto& c : contacts) {
        if (c.twoWay) boundaryIndex.push_back(index.at(c.body.id & Impl::kSlotMask));
        else {
            boundaryIndex.push_back(rigidBodies.size());
            RigidBody wall; wall.position=c.point; wall.linearVelocity=c.wallVelocity;
            rigidBodies.push_back(wall);
        }
    }
    auto& solver = w.particleSolver;
    solver.Clear();
    struct ClearOnExit { ContactSolver& solver; ~ClearOnExit() { solver.Clear(); } } clear{solver};
    struct BatchRow { RigidBody* a; RigidBody* b; Contact geometry; float friction; };
    std::vector<BatchRow> batchRows;batchRows.reserve(rigidRows.size()+contacts.size());
    for (const auto& row : rigidRows) {
        const auto& a = w.bodies[row.a]; const auto& b = w.bodies[row.b];
        const float friction=std::sqrt(std::max(a.friction,0.0f)*std::max(b.friction,0.0f));
        batchRows.push_back({&rigidBodies[index.at(row.a)],&rigidBodies[index.at(row.b)],row.contact,friction});
    }
    const std::size_t particleRowStart=batchRows.size();
    w.particleContactStats.rigidContactRows=particleRowStart;
    for (std::size_t i=0; i<contacts.size(); ++i) {
        const auto& c = contacts[i];
        Contact geometry; geometry.hit=true; geometry.point=c.point; geometry.normal=c.normal;
        geometry.preciseNormal=glm::dvec3(c.normal); geometry.signedSeparation=0;
        geometry.hasLocalAnchors=true;
        const auto& particle=particleBodies[c.particleIndex];const auto& body=rigidBodies[boundaryIndex[i]];
        geometry.localAnchorA=glm::dvec3(c.point)-glm::dvec3(particle.position);
        geometry.localAnchorB=glm::transpose(ContactRotation(body.orientation))*(glm::dvec3(c.point)-glm::dvec3(body.position));
        batchRows.push_back({&particleBodies[c.particleIndex],&rigidBodies[boundaryIndex[i]],geometry,0});
    }
    struct OriginalVelocity { RigidBody* body; glm::vec3 linear,angular; };
    std::vector<OriginalVelocity> original;original.reserve(particleBodies.size()+rigidBodies.size());
    for(auto& body:particleBodies)original.push_back({&body,body.linearVelocity,body.angularVelocity});
    for(auto& body:rigidBodies)original.push_back({&body,body.linearVelocity,body.angularVelocity});
    const auto prepare=[&](const std::vector<float>* normal=nullptr,const std::vector<glm::vec3>* tangent=nullptr) {
        solver.Clear();
        for(const auto& v:original) {v.body->linearVelocity=v.linear;v.body->angularVelocity=v.angular;}
        for(std::size_t i=0;i<batchRows.size();++i) {
            const auto& row=batchRows[i];
            solver.AddContact(*row.a,*row.b,row.geometry,row.friction,0,
                normal?(*normal)[i]:0,tangent?(*tangent)[i]:glm::vec3(0));
        }
        solver.Prepare(remainingRigidDt);
    };
    prepare();
    const auto& solved=solver.Constraints();
    const auto pointVelocity=[](const RigidBody& b,const glm::vec3& r) {
        return b.linearVelocity+glm::cross(b.angularVelocity,r);
    };
    auto& convergence=w.particleContactStats;
    for (const auto& c:solved) {
        convergence.speedScale=std::max(convergence.speedScale,
            glm::length(pointVelocity(*c.bodyA,c.velocityOffsetA)));
        convergence.speedScale=std::max(convergence.speedScale,
            glm::length(pointVelocity(*c.bodyB,c.velocityOffsetB)));
    }
    // Fluid contacts include finite particles and their actual rigid support
    // island. Refine this auxiliary solve to a declared velocity residual;
    // ordinary rigid step iteration counts and warm starts remain unchanged.
    do {
        const int count=std::min(10,64-convergence.iterations);
        solver.SolveVelocities(count);
        convergence.iterations+=count;
        const auto closingResidual=[&]() {
            float largest=0;
            for (const auto& c:solved) {
                const float normalSpeed=glm::dot(pointVelocity(*c.bodyA,c.velocityOffsetA)-
                    pointVelocity(*c.bodyB,c.velocityOffsetB),c.normal);
                if(!std::isfinite(normalSpeed)) return std::numeric_limits<float>::infinity();
                largest=std::max(largest,c.restitutionBias-normalSpeed);
            }
            return largest;
        };
        convergence.maximumClosingSpeed=closingResidual();
        if(convergence.maximumClosingSpeed>1.0e-5f*convergence.speedScale && convergence.iterations<64) {
            std::vector<float> normal;std::vector<glm::vec3> tangent;
            if(AccelerateParticleNormals(solved,convergence,normal,tangent)) {
                // This is an optional linear-solver proposal. Its double-
                // precision model is not permission to commit a worse actual
                // float constraint state, particularly for inconsistent PSD
                // rows generated by intersecting prescribed boundaries.
                const auto previousRows=solved;
                std::vector<OriginalVelocity> previous;previous.reserve(original.size());
                for(const auto& v:original)
                    previous.push_back({v.body,v.body->linearVelocity,v.body->angularVelocity});
                const float baseline=convergence.maximumClosingSpeed;
                prepare(&normal,&tangent);
                const float candidate=closingResidual();
                bool admissible=std::isfinite(candidate);
                const float rounding=32*std::numeric_limits<float>::epsilon();
                for(const auto& v:original)
                    admissible=admissible && finite(v.body->linearVelocity) && finite(v.body->angularVelocity);
                for(const auto& c:solved) {
                    const float limit=c.friction*c.normalImpulse;
                    admissible=admissible && std::isfinite(c.normalImpulse) && c.normalImpulse>=0 &&
                        finite(c.tangentImpulse) && glm::length(c.tangentImpulse)<=
                            limit+rounding*std::max(1.0f,limit);
                }
                admissible=admissible && candidate<=baseline+rounding*std::max(convergence.speedScale,baseline);
                if(admissible) {
                    convergence.maximumClosingSpeed=candidate;
                    ++convergence.normalAccelerations;
                } else {
                    // Roll back bit-for-bit, including the impulses. Replaying
                    // them would repeat float rounding and tangent projection.
                    // Only this private auxiliary solver's freshly prepared
                    // rows are restored; ordinary rigid cache/state is absent.
                    auto& writable=const_cast<std::vector<ContactConstraint>&>(solver.Constraints());
                    std::copy(previousRows.begin(),previousRows.end(),writable.begin());
                    for(const auto& v:previous) {
                        v.body->linearVelocity=v.linear;v.body->angularVelocity=v.angular;
                    }
                    convergence.maximumClosingSpeed=baseline;
                    ++convergence.normalAccelerationRejections;
                }
            }
        }
    } while (convergence.maximumClosingSpeed>1.0e-5f*convergence.speedScale && convergence.iterations<64);
    convergence.iterationCapReached=convergence.maximumClosingSpeed>1.0e-5f*convergence.speedScale;
    for (std::size_t i=0; i<contacts.size(); ++i) {
        const auto& c=solved[particleRowStart+i];
        particleImpulses[i]=c.normal*c.normalImpulse+c.tangentImpulse;
    }
    if (staticSupportImpulse) for (std::size_t i=0; i<particleRowStart; ++i) {
        const auto& c=solved[i];const glm::vec3 impulse=c.normal*c.normalImpulse+c.tangentImpulse;
        if (c.bodyA->IsStatic()) *staticSupportImpulse-=impulse;
        if (c.bodyB->IsStatic()) *staticSupportImpulse+=impulse;
    }
    for (std::size_t i=0; i<particles.size(); ++i) particles[i].velocity=particleBodies[i].linearVelocity;
    for (const auto& [slot,copy] : index) if (!w.bodies[slot].rigidBody.IsStatic()) {
        w.bodies[slot].rigidBody.linearVelocity=rigidBodies[copy].linearVelocity;
        w.bodies[slot].rigidBody.angularVelocity=rigidBodies[copy].angularVelocity;
    }
}

PhysicsWorld::ParticleContactStats PhysicsWorld::GetParticleContactStats() const {
    return m_impl ? m_impl->particleContactStats : ParticleContactStats{};
}

glm::vec3 PhysicsWorld::GetLinearVelocity(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    return body ? body->rigidBody.linearVelocity : glm::vec3(0.0f);
}

void PhysicsWorld::SetLinearVelocity(BodyHandle handle, const glm::vec3& velocity) {
    Impl::Body* body = m_impl->Get(handle);
    if (body) {if(velocity!=body->rigidBody.linearVelocity)m_impl->Wake(handle.id&Impl::kSlotMask);body->rigidBody.linearVelocity = velocity;}
}

glm::vec3 PhysicsWorld::GetAngularVelocity(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    return body ? body->rigidBody.angularVelocity : glm::vec3(0.0f);
}

glm::mat3 PhysicsWorld::GetInertiaWorld(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body || !body->isDynamic) return glm::mat3(0.0f);
    return glm::inverse(body->rigidBody.InverseInertiaWorld());
}

float PhysicsWorld::GetBodySupportDistance(BodyHandle handle, const glm::vec3& worldDirection) const {
    const Impl::Body* body = m_impl->Get(handle);
    const float directionLength = glm::length(worldDirection);
    if (!body || directionLength < 1.0e-6f) return 0.0f;

    const glm::vec3 direction = worldDirection / directionLength;
    float maximum = 0.0f;
    for (int part = 0; part < PrimitiveCount(body->shape); ++part) {
        const PrimitivePose primitive = PrimitiveAt(body->shape, body->rigidBody, part);
        float extent = 0.0f;
        if (primitive.shape.type == ShapeType::Sphere) {
            extent = primitive.shape.radius;
        } else if (primitive.shape.type == ShapeType::Box) {
            const glm::vec3 localDirection =
                glm::conjugate(glm::normalize(primitive.body.orientation)) * direction;
            extent = glm::dot(glm::abs(localDirection), primitive.shape.halfExtents);
        } else if (primitive.shape.asset) {
            // Directional support uses the physical vertices about the body's COM.
            // Child offsets and rotations have already been composed in PrimitiveAt.
            const auto r = PrimitiveRotation(primitive);
            const auto c = PrimitiveCenter(primitive);
            for (const auto& vertex : primitive.shape.asset->vertices)
                maximum = std::max(maximum, static_cast<float>(glm::dot(
                    c + r * vertex - glm::dvec3(body->rigidBody.position), glm::dvec3(direction))));
            continue;
        } else if (primitive.shape.type == ShapeType::Terrain && primitive.shape.terrain) {
            // Exact directional support of an arbitrary radial height
            // function would require global optimization. Its immutable
            // radius bound is conservative for this separation query.
            extent = primitive.shape.terrain->BoundRadius();
        }
        maximum = std::max(maximum,
                            glm::dot(primitive.body.position - body->rigidBody.position,
                                     direction) + extent);
    }
    return maximum;
}

float PhysicsWorld::GetPlayerShapeMaxSupportDistance() const {
    if (!m_impl->hasPlayerShape) return 0.0f;
    return m_impl->playerShape.radius + m_impl->playerShape.halfHeight;
}

void PhysicsWorld::SetAngularVelocity(BodyHandle handle, const glm::vec3& angularVelocity) {
    Impl::Body* body = m_impl->Get(handle);
    if (body) {if(angularVelocity!=body->rigidBody.angularVelocity)m_impl->Wake(handle.id&Impl::kSlotMask);body->rigidBody.angularVelocity = angularVelocity;}
}

void PhysicsWorld::Step(float fixedDeltaTime) {
    JUDAS_PROFILE_SCOPE("Rigid physics");
    Impl& w = *m_impl;
    const Clock::time_point stepStart = Clock::now();
    w.stats = StepStats{};
    if(w.collectingPreStepQueries){
        w.stepTouches=std::move(w.preStepQueryTouches);w.preStepQueryTouches.clear();w.collectingPreStepQueries=false;
        // A safe authority handoff may disable an observer after its motor move.
        for(auto it=w.stepTouches.begin();it!=w.stepTouches.end();){auto* a=w.Get(it->second.a);auto* b=w.Get(it->second.b);
            if(!a||!b||!a->enabled||!b->enabled)it=w.stepTouches.erase(it);else ++it;}
    }else w.stepTouches.clear();
    const auto cachesBefore = w.cacheCounters;
    const ContactGeometryDiagnostics geometryBefore = GetContactGeometryDiagnostics();

    w.PublishKinematics(fixedDeltaTime);

    // Awaken connected state before velocity integration for predicted impacts.
    for(auto i:w.aliveSlots)if(w.bodies[i].HasMotion()&&!w.bodies[i].sleeping)w.CoverStepReach(w.bodies[i],fixedDeltaTime);
    w.GenerateCandidatePairs();
    for(auto [a,b]:w.candidatePairs){auto& aa=w.bodies[a];auto& bb=w.bodies[b];if(aa.sensor||bb.sensor||aa.queryOnly||bb.queryOnly)continue;
        auto moving=[&](const auto& body){return body.isKinematic
            ? body.rigidBody.linearVelocity!=glm::vec3(0)||body.rigidBody.angularVelocity!=glm::vec3(0)
            : !body.sleeping&&!body.rigidBody.IsStatic()&&(glm::length(body.rigidBody.linearVelocity-body.lastAcceleration*fixedDeltaTime)>.03f||glm::length(body.rigidBody.angularVelocity)>.03f);};
        if(aa.sleeping&&moving(bb))w.Wake(a);
        if(bb.sleeping&&moving(aa))w.Wake(b);
    }
    // 1) Velocity from this step's force/torque accumulators (M12). Gravity
    // is already in linearVelocity via the caller's ApplyLinearAcceleration.
    // The start-of-step pose is kept for player sweeps and presentation.
    std::size_t movable = 0;
    for (const unsigned int slot : w.aliveSlots) {
        Impl::Body& body = w.bodies[slot];
        if (!body.rigidBody.IsStatic()) ++movable;
        if (!body.isDynamic) continue;
        body.motion.Clear();
        body.previousPosition = body.rigidBody.position;
        body.previousOrientation = body.rigidBody.orientation;
        // Capture the old endpoint before integration. CurrentBound itself is
        // keyed by represented pose, so a later correction cannot reuse it.
        w.CurrentBound(body);
        body.previousBound = body.currentBound;
        if(body.sleeping)continue;
        if(body.deferredAcceleration){body.rigidBody.linearVelocity+=body.lastAcceleration*fixedDeltaTime;body.deferredAcceleration=false;}
        IntegrateRigidBodyVelocity(body.rigidBody, fixedDeltaTime);
        w.CoverStepReach(body, fixedDeltaTime);
    }
    w.stats.bodies = w.aliveSlots.size();
    w.stats.dynamicBodies = movable;
    w.stats.possiblePairs = movable * (movable - (movable > 0 ? 1 : 0)) / 2 +
                            movable * (w.aliveSlots.size() - movable);

    // 2) Broadphase: every fat bound contains its body's current pose
    // (refreshed at the end of the previous Step and by ResetBody), so every
    // touching pair is among the candidates. Static-static pairs never are.
    static ProfileLabel broadLabel("Physics broadphase"), narrowLabel("Physics contacts"), solverLabel("Physics contact and joint solver"), proxiesLabel("Physics proxy refresh");
    ProfileScope broadScope(broadLabel);
    const Clock::time_point broadphaseStart = Clock::now();
    w.GenerateCandidatePairs();
    w.stats.candidatePairs = w.candidatePairs.size();
    broadScope.End(); ProfileScope narrowScope(narrowLabel);
    const Clock::time_point narrowphaseStart = Clock::now();

    // 3) Narrowphase at the current poses — the sole authority on contact.
    w.solver.Clear();
    w.lastStepContacts.clear();
    w.pendingCache.clear();
    w.startContacts.clear();
    w.separatedPairs.clear();
    w.cacheUsed.assign(w.contactCache.size(), 0);
    for (const auto& [slotA, slotB] : w.candidatePairs) {
        Impl::Body& a = w.bodies[slotA];
        Impl::Body& b = w.bodies[slotB];
        if(a.sensor||b.sensor||a.queryOnly||b.queryOnly)continue;
        if((a.sleeping||a.rigidBody.IsStatic())&&(b.sleeping||b.rigidBody.IsStatic())){
            const auto ha=w.MakeHandle(slotA).id,hb=w.MakeHandle(slotB).id;auto key=std::minmax(ha,hb);auto old=w.previousTouches.find(key);
            if(old!=w.previousTouches.end()){auto observation=old->second;observation.normalImpulse=0;observation.impulseAvailable=false;observation.relativeVelocity={0,0,0};w.stepTouches[key]=observation;}
            continue;
        }
        const float friction = std::sqrt(std::max(a.friction, 0.0f) * std::max(b.friction, 0.0f));
        // Speculative margin: the distance this pair's surfaces can close
        // within the step at their post-force velocities (relative linear
        // motion plus each body's rotation at its bounding radius). Any
        // contact within it is generated now, before it can penetrate; at
        // rest on a support that is g*dt^2 (2.7 mm at 60 Hz, 9.81 m/s^2).
        const float margin =
            (glm::length(a.rigidBody.linearVelocity - b.rigidBody.linearVelocity) +
             glm::length(a.rigidBody.angularVelocity) * a.boundingRadius +
             glm::length(b.rigidBody.angularVelocity) * b.boundingRadius) *
            fixedDeltaTime;
        int pairPoints = 0;
        const auto& orientationA = w.Orientation(a);
        const auto& orientationB = w.Orientation(b);
        const glm::dmat3 inverseA = glm::transpose(orientationA.rotation);
        for (int partA = 0; partA < PrimitiveCount(a.shape); ++partA) {
            const PrimitivePose childA = PrimitiveAt(a.shape, a.rigidBody, partA, &orientationA);
            for (int partB = 0; partB < PrimitiveCount(b.shape); ++partB) {
                const PrimitivePose childB = PrimitiveAt(b.shape, b.rigidBody, partB, &orientationB);
                const ContactManifold manifold =
                    ComputeContacts(childA, childB, margin, &orientationA, &orientationB);
                const ContactKey key{slotA, slotB, a.generation, b.generation, partA, partB};
                const auto range = std::equal_range(
                    w.contactCache.begin(), w.contactCache.end(), CachedContact{key, {}, {}, 0.0f, {}},
                    [](const CachedContact& x, const CachedContact& y) { return x.key < y.key; });
                bool touchingManifold=false;
                for (int p=0;p<manifold.count;++p) {
                    const auto& c=manifold.points[p];
                    touchingManifold=touchingManifold || (c.hasLocalAnchors ? c.signedSeparation<=0 : c.penetration>=0);
                }
                if(!touchingManifold && manifold.count)
                    touchingManifold=ComputeContacts(childA,childB,0,&orientationA,&orientationB).count>0;
                for (int p = 0; p < manifold.count; ++p) {
                    Contact contact = manifold.points[p];
                    if (!contact.hit) continue;
                    if((contact.hasLocalAnchors?contact.signedSeparation<=0:contact.penetration>=0)||touchingManifold) w.Observe(slotA,slotB,contact);
                    // Warm start from the nearest unused cached point of the
                    // same bodies/primitives (same generations: a reused slot
                    // never inherits a previous occupant's impulses).
                    const glm::dvec3 anchor = contact.hasLocalAnchors ? contact.localAnchorA :
                        inverseA * (glm::dvec3(contact.point) - glm::dvec3(a.rigidBody.position));
                    float warmNormal = 0.0f;
                    glm::vec3 warmTangent(0.0f);
                    std::ptrdiff_t best = -1;
                    double bestDistance = kWarmStartAnchorDistance;
                    for (auto it = range.first; it != range.second; ++it) {
                        const std::ptrdiff_t index = it - w.contactCache.begin();
                        if (w.cacheUsed[static_cast<std::size_t>(index)] || !it->key.SameBodies(key) ||
                            glm::dot(it->normal, contact.normal) < kWarmStartNormalDot) continue;
                        const double distance = glm::distance(it->localAnchorA, anchor);
                        if (distance < bestDistance) {
                            bestDistance = distance;
                            best = index;
                        }
                    }
                    if (best >= 0) {
                        w.cacheUsed[static_cast<std::size_t>(best)] = 1;
                        warmNormal = w.contactCache[static_cast<std::size_t>(best)].normalImpulse;
                        warmTangent = w.contactCache[static_cast<std::size_t>(best)].tangentImpulse;
                    }
                    // A separated, newly encountered pair is handled at TOI.
                    // Existing cached support retains its gap-closing constraint.
                    const double gap=contact.hasLocalAnchors ? contact.signedSeparation : -double(contact.penetration);
                    if (gap>0 && best<0 && !touchingManifold) {
                        const std::array<unsigned,4> pair{slotA,slotB,unsigned(partA),unsigned(partB)};
                        if(w.separatedPairs.empty() || w.separatedPairs.back()!=pair) w.separatedPairs.push_back(pair);
                        continue;
                    }
                    const auto constraint=w.solver.Constraints().size();
                    w.solver.AddContact(a.rigidBody,b.rigidBody,contact,friction,0,
                        warmNormal,warmTangent,&orientationA.rotation,&orientationB.rotation);
                    if(w.solver.Constraints().size()>constraint) {
                        w.pendingCache.push_back({key,anchor,contact.normal,0,glm::vec3(0)});
                        w.startContacts.push_back({slotA,slotB,constraint,friction,warmNormal,warmTangent,
                            best<0 && gap<=0 && Impl::NormalSpeed(contact,a.rigidBody,b.rigidBody)<-0.5});
                    }
                    w.lastStepContacts.push_back({contact.point, contact.normal, contact.penetration});
                    ++pairPoints;
                }
            }
        }
        if (pairPoints > 0) ++w.stats.collidingPairs;
    }
    w.stats.contactPoints = w.lastStepContacts.size();
    narrowScope.End(); ProfileScope solverScope(solverLabel);
    const Clock::time_point solverStart = Clock::now();

    // 4) Accumulated-impulse velocity solve (src/ContactSolver.h), then
    // positions from the solved velocities, then direct penetration removal.
    // A newly gravity-driven neighbour can reach an asleep body without any
    // prior velocity. Actual start contacts, not the early prediction alone,
    // must wake participating state before the velocity solver changes it.
    for(const auto& c:w.startContacts)for(auto slot:{c.a,c.b})
        if(w.bodies[slot].sleeping)w.Wake(slot);
    for(auto slot:w.aliveSlots){auto& body=w.bodies[slot];
        if(body.isDynamic&&!body.sleeping&&body.deferredAcceleration){
            body.rigidBody.linearVelocity+=body.lastAcceleration*fixedDeltaTime;
            body.deferredAcceleration=false;
        }
    }
    static ProfileLabel initialImpactsLabel("Physics initial impacts"), velocityLabel("Physics velocity constraints"), continuousImpactsLabel("Physics continuous impacts"), positionsLabel("Physics position constraints"), settleLabel("Physics sleeping assessment");
    ProfileScope initialImpactsScope(initialImpactsLabel);
    w.ResolveInitialImpacts(fixedDeltaTime);
    initialImpactsScope.End();
    ProfileScope velocityScope(velocityLabel);
    w.supportPairs.clear();
    for(const auto& c:w.startContacts) if(!c.newImpact) {
        const auto& key=w.pendingCache[c.constraint].key;
        w.supportPairs.push_back({c.a,c.b,unsigned(key.partA),unsigned(key.partB)});
    }
    std::sort(w.supportPairs.begin(),w.supportPairs.end());
    w.supportPairs.erase(std::unique(w.supportPairs.begin(),w.supportPairs.end()),w.supportPairs.end());
    w.solver.Prepare(fixedDeltaTime);
    if(w.joints.empty())w.solver.SolveVelocities();
    else {w.jointSolver.Prepare(w.JointInputs(),fixedDeltaTime);for(int i=0;i<ContactSolver::kVelocityIterations;++i){w.solver.SolveVelocities(1);w.jointSolver.SolveIteration();}}
    // Remember this step's converged impulses for the next step's warm start.
    const std::vector<ContactConstraint>& solved = w.solver.Constraints();
    for (std::size_t i = 0; i < solved.size() && i < w.pendingCache.size(); ++i) {
        const auto& key=w.pendingCache[i].key;
        const unsigned ha=w.MakeHandle(key.slotA).id,hb=w.MakeHandle(key.slotB).id;
        const auto pair=std::minmax(ha,hb);
        if(auto it=w.stepTouches.find({pair.first,pair.second});it!=w.stepTouches.end()){it->second.normalImpulse+=solved[i].normalImpulse;it->second.impulseAvailable=true;}
        w.pendingCache[i].normalImpulse = solved[i].normalImpulse;
        w.pendingCache[i].tangentImpulse = solved[i].tangentImpulse;
    }
    for(const auto& cached:w.contactCache)if(cached.key.slotA<w.bodies.size()&&cached.key.slotB<w.bodies.size()&&w.bodies[cached.key.slotA].alive&&w.bodies[cached.key.slotB].alive&&w.bodies[cached.key.slotA].generation==cached.key.generationA&&w.bodies[cached.key.slotB].generation==cached.key.generationB&&(w.bodies[cached.key.slotA].sleeping||w.bodies[cached.key.slotA].rigidBody.IsStatic())&&(w.bodies[cached.key.slotB].sleeping||w.bodies[cached.key.slotB].rigidBody.IsStatic()))w.pendingCache.push_back(cached);
    std::stable_sort(w.pendingCache.begin(), w.pendingCache.end(),
                     [](const CachedContact& x, const CachedContact& y) { return x.key < y.key; });
    std::swap(w.contactCache, w.pendingCache);
    velocityScope.End();
    ProfileScope continuousImpactsScope(continuousImpactsLabel);
    w.AdvanceImpacts(fixedDeltaTime);
    continuousImpactsScope.End();
    ProfileScope positionsScope(positionsLabel);
    for (const unsigned int slot:w.aliveSlots) {
        auto& body=w.bodies[slot];
        if(body.HasMotion()) w.solver.UpdatePreparedRotation(body.rigidBody,w.Orientation(body).rotation);
    }
    w.solver.SolvePositions();
    positionsScope.End();
    ProfileScope settleScope(settleLabel);
    w.Settle(fixedDeltaTime);
    settleScope.End();
    solverScope.End(); ProfileScope proxyScope(proxiesLabel);
    const Clock::time_point solverEnd = Clock::now();

    // 5) Keep every dynamic proxy's fat bound around its previous-to-current
    // motion for the player's sweeps and the next step's candidates.
    for (const unsigned int slot : w.aliveSlots) {
        Impl::Body& body = w.bodies[slot];
        if (body.HasMotion()) w.RefreshProxy(body);
    }
    w.queryTouchesPending=std::any_of(w.aliveSlots.begin(),w.aliveSlots.end(),[&](unsigned i){return w.bodies[i].queryOnly;});
    if(!w.queryTouchesPending)w.FinishTouches();
    w.stats.proxyReinsertions = w.reinsertionsSinceStep;
    w.reinsertionsSinceStep = 0;
    w.stats.treeHeight = w.tree.Height();
    w.stats.orientationCacheHits = w.cacheCounters.orientationHits-cachesBefore.orientationHits;
    w.stats.orientationCacheRebuilds = w.cacheCounters.orientationRebuilds-cachesBefore.orientationRebuilds;
    w.stats.boundCacheHits = w.cacheCounters.boundHits-cachesBefore.boundHits;
    w.stats.boundCacheRebuilds = w.cacheCounters.boundRebuilds-cachesBefore.boundRebuilds;
    w.stats.shapeCacheRebuilds = w.cacheCounters.shapeRebuilds-cachesBefore.shapeRebuilds;
    w.stats.solverFrameBuilds = w.solver.FrameBuilds();
    w.stats.geometryCacheAllocations = w.cacheCounters.allocations-cachesBefore.allocations+w.solver.FrameAllocations();
    w.stats.geometryCacheAllocatedBytes = w.cacheCounters.allocatedBytes-cachesBefore.allocatedBytes+w.solver.FrameAllocatedBytes();
    w.stats.solverFrameNodeRequests = w.solver.FrameNodeRequests();
    w.stats.geometryCacheBytes = w.CacheBytes();
    const Clock::time_point stepEnd = Clock::now();
    w.stats.broadphaseMilliseconds = MillisecondsBetween(broadphaseStart, narrowphaseStart) +
                                     MillisecondsBetween(solverEnd, stepEnd);
    w.stats.narrowphaseMilliseconds = MillisecondsBetween(narrowphaseStart, solverStart);
    JUDAS_PROFILE_COUNTER("Sleeping physics bodies",double(w.stats.sleepingBodies),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Awake physics bodies",double(w.stats.awakeBodies),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics bodies last step",double(w.stats.bodies),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics candidate pairs last step",double(w.stats.candidatePairs),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics contacts last step",double(w.stats.contactPoints),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics joints",double(w.joints.size()),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact events",double(w.stats.impactEvents),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact queries",double(w.stats.impactQueries),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact search iterations",double(w.stats.impactSearchIterations),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact sampling fallbacks",double(w.stats.impactSamplingFallbacks),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact sampling tests",double(w.stats.impactSamplingTests),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact search limits",double(w.stats.impactSearchLimit),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact sampling resolution caps",double(w.stats.impactSamplingResolutionCaps),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact pose evaluations",double(w.impactPoseEvaluations),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact pose reuses",double(w.impactPoseReuses),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact orientation preparations",double(w.impactOrientationBuilds),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact actual-contact requests",double(w.impactTruthRequests),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact actual-contact reuses",double(w.impactTruthReuses),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics impact remaining-bound rejections",double(w.impactBoundsRejected),ProfileCounterMode::Latest);
    w.stats.solverMilliseconds = MillisecondsBetween(solverStart, solverEnd);
    w.stats.totalMilliseconds = MillisecondsBetween(stepStart, stepEnd);
    const ContactGeometryDiagnostics geometryAfter = GetContactGeometryDiagnostics();
    w.stats.geometryPredicates = geometryAfter.predicates - geometryBefore.predicates;
    w.stats.geometryExactFallbacks = geometryAfter.exactFallbacks - geometryBefore.exactFallbacks;
    w.stats.geometryUnresolved = geometryAfter.unresolved - geometryBefore.unresolved;
    w.stats.geometryNumericGapFallbacks = geometryAfter.numericGapFallbacks - geometryBefore.numericGapFallbacks;
}

const PhysicsWorld::StepStats& PhysicsWorld::LastStepStats() const { return m_impl->stats; }

void PhysicsWorld::QueryBodiesInAabbInto(const glm::vec3& min,const glm::vec3& max,std::vector<BodyHandle>& output,const PhysicsQueryFilter& filter)const{output.clear();for(auto slot:m_impl->QuerySlots(Aabb{glm::min(min,max),glm::max(min,max)}))if(m_impl->MatchesQuery(slot,filter))output.push_back(m_impl->MakeHandle(slot));}

std::vector<BodyHandle> PhysicsWorld::QueryBodiesInAabb(const glm::vec3& min, const glm::vec3& max, const PhysicsQueryFilter& filter) const {
    std::vector<BodyHandle> result;
    for (const unsigned int slot : m_impl->QuerySlots(Aabb{glm::min(min, max), glm::max(min, max)})) {
        if(m_impl->MatchesQuery(slot,filter))result.push_back(m_impl->MakeHandle(slot));
    }
    return result;
}

bool PhysicsWorld::GetBodyBroadphaseBounds(BodyHandle handle, glm::vec3& outMin, glm::vec3& outMax) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return false;
    const Aabb& fat = m_impl->tree.FatAabb(body->proxy);
    outMin = fat.min;
    outMax = fat.max;
    return true;
}

bool PhysicsWorld::GetBodyGeometryCacheState(BodyHandle handle, GeometryCacheState& state) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return false;
    const auto& current = body->currentBound;
    const auto& previous = body->previousBound;
    state.currentPose = {current.position,current.orientation};
    state.previousPose = {previous.position,previous.orientation};
    state.currentMin = current.bound.min; state.currentMax = current.bound.max;
    state.previousMin = previous.bound.min; state.previousMax = previous.bound.max;
    state.currentValid = current.valid; state.previousValid = previous.valid;
    state.boundingRadius = body->boundingRadius;
    return true;
}

std::vector<PhysicsWorld::CollidingPair> PhysicsWorld::FindCollidingPairs() const {
    Impl& w = *m_impl;
    // Same candidate generation and narrowphase as Step, without solving.
    w.GenerateCandidatePairs();
    std::vector<CollidingPair> result;
    for (const auto& [slotA, slotB] : w.candidatePairs) {
        Impl::Body& a = w.bodies[slotA];
        Impl::Body& b = w.bodies[slotB];
        const auto& orientationA = w.Orientation(a);
        const auto& orientationB = w.Orientation(b);
        int points = 0;
        for (int partA = 0; partA < PrimitiveCount(a.shape); ++partA) {
            const PrimitivePose childA = PrimitiveAt(a.shape, a.rigidBody, partA, &orientationA);
            for (int partB = 0; partB < PrimitiveCount(b.shape); ++partB) {
                const PrimitivePose childB = PrimitiveAt(b.shape, b.rigidBody, partB, &orientationB);
                const ContactManifold manifold =
                    ComputeContacts(childA, childB, 0.0f, &orientationA, &orientationB);
                for (int p = 0; p < manifold.count; ++p) {
                    if (manifold.points[p].hit) ++points;
                }
            }
        }
        if (points > 0) result.push_back({w.MakeHandle(slotA), w.MakeHandle(slotB), points});
    }
    return result;
}

std::vector<BodyHandle> PhysicsWorld::AliveBodies() const {
    std::vector<BodyHandle> result;
    result.reserve(m_impl->aliveSlots.size());
    for (const unsigned int slot : m_impl->aliveSlots) result.push_back(m_impl->MakeHandle(slot));
    return result;
}

bool PhysicsWorld::GetBodyShape(BodyHandle handle, Shape& outShape, BodyTransform& outPose) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return false;
    outShape = body->shape;
    outPose.position = body->rigidBody.position;
    outPose.rotation = body->rigidBody.orientation;
    return true;
}

BodyTransform PhysicsWorld::GetTransform(BodyHandle handle) const {
    BodyTransform result;
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return result;
    result.position = body->rigidBody.position-body->rigidBody.orientation*body->shape.pivotOffset;
    result.rotation = body->rigidBody.orientation;
    return result;
}

BodyTransform PhysicsWorld::GetPreviousTransform(BodyHandle handle) const {
    BodyTransform result;
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return result;
    if (body->HasMotion()) {
        result.position = body->previousPosition-body->previousOrientation*body->shape.pivotOffset;
        result.rotation = body->previousOrientation;
    } else {
        result.position = body->rigidBody.position-body->rigidBody.orientation*body->shape.pivotOffset;
        result.rotation = body->rigidBody.orientation;
    }
    return result;
}

std::vector<PhysicsWorld::BodyMotionSegment> PhysicsWorld::GetBodyMotionSegments(BodyHandle handle) const {
    std::vector<BodyMotionSegment> result;
    const Impl::Body* body = m_impl->Get(handle);
    if (!body || !body->HasMotion()) return result;
    result.reserve(body->motion.Segments().size());
    for (const auto& source : body->motion.Segments()) {
        if (source.owner != handle.id) return {};
        result.push_back({source.begin, source.end,
            {source.position-source.orientation*body->shape.pivotOffset, source.orientation}, {source.endPosition-source.endOrientation*body->shape.pivotOffset, source.endOrientation},
            source.linearVelocity, source.angularVelocity, source.inverseMass > 0||source.prescribed,body->shape.pivotOffset,source.prescribed});
    }
    if (!result.empty()) result.back().endPose = {body->rigidBody.position-body->rigidBody.orientation*body->shape.pivotOffset, body->rigidBody.orientation};
    return result;
}

BodyTransform PhysicsWorld::EvaluateBodyMotionSegment(const BodyMotionSegment& segment, double time) {
    if (!std::isfinite(time) || time < segment.begin || time > segment.end)
        throw std::out_of_range("body motion segment time");
    RigidMotionSegment source;
    source.begin=segment.begin;source.end=segment.end;source.position=segment.start.position+segment.start.rotation*segment.pivotOffset;
    source.orientation=segment.start.rotation;source.linearVelocity=segment.linearVelocity;source.angularVelocity=segment.angularVelocity;
    source.inverseMass=segment.movable&&!segment.prescribed?1.f:0.f;source.prescribed=segment.prescribed;
    const auto body=source.Evaluate(time);
    return {body.position-body.orientation*segment.pivotOffset, body.orientation};
}

std::vector<BodyBox> PhysicsWorld::GetBodyBoxes(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return {};
    return BoxesAt(body->shape, body->rigidBody.position, body->rigidBody.orientation);
}

std::vector<BodyBox> PhysicsWorld::GetPreviousBodyBoxes(BodyHandle handle) const {
    const Impl::Body* body = m_impl->Get(handle);
    if (!body) return {};
    return BoxesAt(body->shape,
                   body->HasMotion() ? body->previousPosition : body->rigidBody.position,
                   body->HasMotion() ? body->previousOrientation : body->rigidBody.orientation);
}

void PhysicsWorld::ResetBody(BodyHandle handle, const glm::vec3& position,
                              const glm::quat& rotation) {
    Impl::Body* body = m_impl->Get(handle);
    if (!body) return;
    if(body->rigidBody.position!=position+rotation*body->shape.pivotOffset||body->rigidBody.orientation!=rotation){
        m_impl->Wake(handle.id&Impl::kSlotMask);
        const auto bounds=m_impl->CurrentBound(*body);auto slots=m_impl->QuerySlots(bounds);for(auto i:slots)if(m_impl->bodies[i].sleeping)m_impl->Wake(i);
    }
    for(auto& j:m_impl->joints)if(j.state.settings.bodyA.id==handle.id||j.state.settings.bodyB.id==handle.id)j.warm.fill(0);
    body->rigidBody.position = position+rotation*body->shape.pivotOffset;
    body->rigidBody.orientation = rotation;
    body->previousPosition = body->rigidBody.position;
    body->motion.Clear();
    body->kinematic={};body->kinematicActiveDuration=0;
    body->previousOrientation = rotation;
    body->rigidBody.linearVelocity = glm::vec3(0.0f);
    body->rigidBody.angularVelocity = glm::vec3(0.0f);
    body->rigidBody.ClearAccumulators();
    m_impl->RefreshProxy(*body);
    if(!body->isDynamic&&!body->queryOnly){auto slots=m_impl->QuerySlots(m_impl->CurrentBound(*body));for(auto i:slots)if(m_impl->bodies[i].sleeping)m_impl->Wake(i);}
}

bool PhysicsWorld::CreatePlayerShape(float radius, float halfHeight) {
    m_impl->playerShape = Shape::Capsule(radius, halfHeight);
    m_impl->hasPlayerShape = true;
    return true;
}

void PhysicsWorld::DestroyPlayerShape() { m_impl->hasPlayerShape = false; }

ShapeSweepHit PhysicsWorld::SweepPlayerShape(const glm::vec3& fromCenter, const glm::quat& rotation,
                                              const glm::vec3& displacement,
                                              bool interpolateDynamicBodyMotion,
                                              float bodyMotionStart,
                                              float bodyMotionEnd, const PhysicsQueryFilter* filter) const {
    ShapeSweepHit result;
    if (!m_impl->hasPlayerShape) return result;

    return SweepCapsuleMotion(m_impl->playerShape.radius,m_impl->playerShape.halfHeight,
        fromCenter,rotation,displacement,interpolateDynamicBodyMotion,bodyMotionStart,
        bodyMotionEnd,filter,m_impl->playerCollisionLayer,m_impl->playerCollisionMask,filter==nullptr);
}
ShapeSweepHit PhysicsWorld::SweepCapsuleMotion(float radius,float halfHeight,
    const glm::vec3& fromCenter,const glm::quat& rotation,const glm::vec3& displacement,
    bool interpolateDynamicBodyMotion,float bodyMotionStart,float bodyMotionEnd,
    const PhysicsQueryFilter* filter,unsigned collisionLayer,CategoryMask collisionMask,bool bilateralFilter) const {
    ShapeSweepHit result;
    if(!(radius>0)||halfHeight<0)return result;
    const float displacementLength = glm::length(displacement);
    if (displacementLength < 1.0e-6f && !interpolateDynamicBodyMotion) return result;

    const glm::vec3 localSegA(0.0f, -halfHeight, 0.0f);
    const glm::vec3 localSegB(0.0f, halfHeight, 0.0f);
    const float capsuleRadius = radius;

    auto worldSegmentAt = [&](const glm::vec3& center) {
        return std::make_pair(center + rotation * localSegA, center + rotation * localSegB);
    };
    // Milestone 32: one broadphase query for the whole swept capsule; every
    // evaluation below then tests only those bodies. Dynamic bodies' fat
    // bounds already cover their previous-to-current motion.
    Aabb sweptBound;
    {
        const auto [a0, b0] = worldSegmentAt(fromCenter);
        const auto [a1, b1] = worldSegmentAt(fromCenter + displacement);
        sweptBound.min = glm::min(glm::min(a0, b0), glm::min(a1, b1));
        sweptBound.max = glm::max(glm::max(a0, b0), glm::max(a1, b1));
        sweptBound = sweptBound.Expanded(capsuleRadius + kSweepQueryEpsilon);
    }
    std::vector<unsigned int> candidates = m_impl->QuerySlots(sweptBound);
    candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](unsigned slot){
        if(filter && !m_impl->MatchesQuery(slot,*filter))return true;
        if(!m_impl->bodies[slot].enabled||(m_impl->bodies[slot].sensor&&(!filter||bilateralFilter)))return true;
        const auto& b=m_impl->bodies[slot];
        return bilateralFilter && !CollisionPermitted(collisionLayer,collisionMask,b.collisionLayer,b.collisionMask);
    }),candidates.end());
    auto evaluateAt = [&](float t) {
        const glm::vec3 center = fromCenter + displacement * t;
        const auto [segA, segB] = worldSegmentAt(center);
        const float bodyMotionAlpha = glm::mix(bodyMotionStart, bodyMotionEnd, t);
        return m_impl->ClosestBodyToCapsule(candidates, segA, segB, capsuleRadius, bodyMotionAlpha,
                                            interpolateDynamicBodyMotion);
    };

    // Already touching/overlapping at the very start of the sweep — report
    // an immediate zero-distance hit rather than marching forward, mirroring
    // the "already touching" case a grounded move-and-slide step produces
    // routinely (see docs/ARCHITECTURE.md, "Contact normal correctness and
    // the 'already touching' case," law #13 — this normal is likewise
    // unconditional, never derived from travel direction).
    const ClosestBodyResult startResult = evaluateAt(0.0f);
    if (startResult.bodyIndex >= 0 && startResult.distance <= 0.0f) {
        result.hit = true;
        result.distance = 0.0f;
        result.penetration = -startResult.distance;
        result.normal = startResult.normal;
        result.point=startResult.point;
        result.hitBody = m_impl->MakeHandle(static_cast<unsigned int>(startResult.bodyIndex));
        return result;
    }

    // March forward in substeps looking for the first t where the capsule
    // starts overlapping something, then refine that bracket by bisection.
    // Per-step displacements in this engine are always small (a fraction
    // of a meter — see docs/ARCHITECTURE.md's move-and-slide/ground-probe
    // distances), so a modest fixed substep count plus a short bisection
    // pass gives ample precision without needing closed-form continuous
    // collision detection for every shape pair.
    constexpr int kSubsteps = 24;
    constexpr int kBisectionIterations = 20;
    std::vector<float> divisions{0,1};
    if(interpolateDynamicBodyMotion && bodyMotionEnd>bodyMotionStart && m_impl->stepDuration>0)
        for(auto slot:candidates) for(const auto& segment:m_impl->bodies[slot].motion.Segments()) {
            const float t=(float(segment.end/m_impl->stepDuration)-bodyMotionStart)/(bodyMotionEnd-bodyMotionStart);
            if(t>0 && t<1) divisions.push_back(t);
        }
    std::sort(divisions.begin(),divisions.end());
    divisions.erase(std::unique(divisions.begin(),divisions.end()),divisions.end());
    float previousT = 0.0f;
    for(std::size_t interval=1;interval<divisions.size();++interval)
    for (int step = 1; step <= kSubsteps; ++step) {
        const float t=glm::mix(divisions[interval-1],divisions[interval],float(step)/kSubsteps);
        const ClosestBodyResult stepResult = evaluateAt(t);
        if (stepResult.bodyIndex >= 0 && stepResult.distance <= 0.0f) {
            float lo = previousT;
            float hi = t;
            ClosestBodyResult refined = stepResult;
            for (int iteration = 0; iteration < kBisectionIterations; ++iteration) {
                const float mid = (lo + hi) * 0.5f;
                const ClosestBodyResult midResult = evaluateAt(mid);
                if (midResult.bodyIndex >= 0 && midResult.distance <= 0.0f) {
                    hi = mid;
                    refined = midResult;
                } else {
                    lo = mid;
                }
            }
            result.hit = true;
            result.distance = lo * displacementLength;
            result.normal = refined.normal;
            result.point=refined.point;
            result.hitBody = m_impl->MakeHandle(static_cast<unsigned int>(refined.bodyIndex));
            return result;
        }
        previousT = t;
    }

    return result;  // no hit across the entire displacement
}

bool PhysicsWorld::SetCollisionFilter(BodyHandle handle,unsigned layer,CategoryMask mask){
    auto* b=m_impl->Get(handle);if(!b||layer>=64)return false;
    if(b->collisionLayer==layer&&b->collisionMask==mask)return true;
    m_impl->Wake(handle.id&Impl::kSlotMask);
    b->collisionLayer=layer;b->collisionMask=mask;
    const unsigned slot=handle.id&Impl::kSlotMask;
    auto remove=[&](auto& cache){cache.erase(std::remove_if(cache.begin(),cache.end(),[&](const auto& c){return c.key.slotA==slot||c.key.slotB==slot;}),cache.end());};
    remove(m_impl->contactCache);remove(m_impl->pendingCache);return true;
}
bool PhysicsWorld::GetCollisionFilter(BodyHandle handle,unsigned& layer,CategoryMask& mask)const{
    const auto* b=m_impl->Get(handle);if(!b)return false;layer=b->collisionLayer;mask=b->collisionMask;return true;
}
bool PhysicsWorld::SetBodyTags(BodyHandle handle,CategoryMask tags){auto* b=m_impl->Get(handle);if(!b)return false;b->tags=tags;return true;}
void PhysicsWorld::SetPlayerCollisionFilter(unsigned layer,CategoryMask mask){if(layer<64){m_impl->playerCollisionLayer=layer;m_impl->playerCollisionMask=mask;}}

const std::vector<PhysicsWorld::TouchEvent>& PhysicsWorld::LastStepTouchEvents() const {return m_impl->touchEvents;}
bool PhysicsWorld::SetBodySensor(BodyHandle h,bool value){auto* b=m_impl->Get(h);if(!b|| (value&&b->shape.type==ShapeType::TriangleMesh))return false;if(b->sensor!=value)m_impl->Wake(h.id&Impl::kSlotMask);b->sensor=value;return true;}
bool PhysicsWorld::IsBodySensor(BodyHandle h) const {auto* b=m_impl->Get(h);return b&&b->sensor;}
bool PhysicsWorld::SetBodyQueriesEnabled(BodyHandle h,bool enabled){auto* body=m_impl->Get(h);if(!body)return false;body->queriesEnabled=enabled;return true;}

bool PhysicsWorld::SetBodyEnabled(BodyHandle h,bool value){auto* b=m_impl->Get(h);if(!b)return false;if(b->enabled==value)return true;m_impl->Wake(h.id&Impl::kSlotMask);b->enabled=value;
 if(!value&&b->isKinematic){b->kinematic={};b->kinematicActiveDuration=0;b->motion.Clear();b->rigidBody.linearVelocity=b->rigidBody.angularVelocity=glm::vec3(0);b->previousPosition=b->rigidBody.position;b->previousOrientation=b->rigidBody.orientation;}
 for(auto& j:m_impl->joints)if(j.state.settings.bodyA.id==h.id||j.state.settings.bodyB.id==h.id)j.warm.fill(0);
 return true;}
bool PhysicsWorld::IsBodyEnabled(BodyHandle h) const {auto* b=m_impl->Get(h);return b&&b->enabled;}

void PhysicsWorld::ClearTouchHistory(){m_impl->previousTouches.clear();m_impl->stepTouches.clear();m_impl->preStepQueryTouches.clear();m_impl->collectingPreStepQueries=false;m_impl->queryTouchesPending=false;m_impl->touchEvents.clear();}

PhysicsCastHit PhysicsWorld::Cast(const Shape& shape,const BodyTransform& pose,const glm::vec3& direction,float maximum,
    const PhysicsQueryFilter& filter,PhysicsCastStats* stats) const {
    JUDAS_PROFILE_SCOPE("Physics shape query");
    if(stats)*stats={};
    auto finite=[](glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    double q2=glm::dot(glm::dvec4(pose.rotation.x,pose.rotation.y,pose.rotation.z,pose.rotation.w),
                        glm::dvec4(pose.rotation.x,pose.rotation.y,pose.rotation.z,pose.rotation.w));
    double d2=glm::dot(glm::dvec3(direction),glm::dvec3(direction));
    if(!finite(pose.position)||!finite(direction)||!std::isfinite(maximum)||maximum<0||d2<=0||!std::isfinite(q2)||q2<=0)
        throw std::invalid_argument("cast requires finite pose, nonzero orientation/direction and nonnegative distance");
    if(!finite(shape.halfExtents)||!std::isfinite(shape.radius)||!std::isfinite(shape.halfHeight)||shape.radius<0||shape.halfHeight<0||
       (shape.type==ShapeType::Box&&(shape.halfExtents.x<=0||shape.halfExtents.y<=0||shape.halfExtents.z<=0)))
        throw std::invalid_argument("invalid cast dimensions");
    PhysicsCastHit result;if(!m_impl)return result;
    const glm::vec3 unit=glm::vec3(glm::dvec3(direction)/std::sqrt(d2));
    auto bound=ShapeAabb(shape,pose.position,pose.rotation);
    auto end=ShapeAabb(shape,pose.position+unit*maximum,pose.rotation);
    Aabb swept{glm::min(bound.min,end.min),glm::max(bound.max,end.max)};
    auto candidates=m_impl->QuerySlots(swept.Expanded(kSweepQueryEpsilon));
    if(stats)stats->broadphaseCandidates=static_cast<unsigned>(candidates.size());
    double nearest=maximum;
    for(auto slot:candidates){if(!m_impl->MatchesQuery(slot,filter))continue;if(stats)++stats->filteredCandidates;
        const auto& body=m_impl->bodies[slot];
        for(int part=0;part<PrimitiveCount(body.shape);++part){if(stats)++stats->primitivesTested;
            const auto primitive=PrimitiveAt(body.shape,body.rigidBody,part);
            const auto hit=CastAgainstPrimitive(shape,pose,unit,nearest,primitive);
            const auto handle=m_impl->MakeHandle(slot);
            if(hit.hit&&(!result.hit||hit.distance<nearest||(hit.distance==nearest&&handle.id<result.body.id))){
                nearest=hit.distance;result.hit=true;result.initialOverlap=hit.initialOverlap;result.body=handle;
                result.point=hit.point;result.normal=hit.normal;result.distance=static_cast<float>(hit.distance);
                result.fraction=maximum>0?result.distance/maximum:0;result.primitiveIndex=part;result.shape=primitive.shape.type;result.childKey=primitive.childKey;result.feature=hit.feature;
            }
        }
    }
    return result;
}
PhysicsClosestPoint PhysicsWorld::ClosestPoint(const glm::vec3& point,float maximum,const PhysicsQueryFilter& filter) const {
    for(int k=0;k<3;++k)if(!std::isfinite(point[k]))throw std::invalid_argument("closestPoint requires finite point");
    if(!std::isfinite(maximum)||maximum<0)throw std::invalid_argument("closestPoint requires finite nonnegative maximum radius");
    PhysicsClosestPoint out;if(!m_impl)return out;double nearest=maximum;
    for(auto slot:m_impl->QuerySlots({point-glm::vec3(maximum),point+glm::vec3(maximum)})){
        if(!m_impl->MatchesQuery(slot,filter))continue;
        const auto& body=m_impl->bodies[slot];
        for(int part=0;part<PrimitiveCount(body.shape);++part){auto primitive=PrimitiveAt(body.shape,body.rigidBody,part);auto result=PointGeometry(glm::dvec3(point),primitive,nearest);if(!result.valid)continue;
            auto handle=m_impl->MakeHandle(slot);if(!out.hit||result.gap<nearest||(result.gap==nearest&&std::tie(handle.id,part)<std::tie(out.body.id,out.primitiveIndex))){nearest=result.gap;out.hit=true;out.body=handle;out.point=glm::vec3(result.point);out.normal=glm::vec3(result.normal);out.distance=nearest;out.contains=result.contains;out.normalUnique=result.normalUnique;out.containmentKnown=primitive.shape.type!=ShapeType::TriangleMesh;out.primitiveIndex=part;out.childKey=primitive.childKey;out.feature=result.feature;out.shape=primitive.shape.type;}
        }
    }return out;
}
PhysicsCastHit PhysicsWorld::Raycast(const glm::vec3& o,const glm::vec3& d,float m,const PhysicsQueryFilter& f,PhysicsCastStats* s)const{return Cast(Shape::Sphere(0),{o,{1,0,0,0}},d,m,f,s);}
PhysicsCastHit PhysicsWorld::SphereCast(const glm::vec3& o,float r,const glm::vec3& d,float m,const PhysicsQueryFilter& f,PhysicsCastStats* s)const{return Cast(Shape::Sphere(r),{o,{1,0,0,0}},d,m,f,s);}
PhysicsCastHit PhysicsWorld::CapsuleCast(const BodyTransform& p,float r,float h,const glm::vec3& d,float m,const PhysicsQueryFilter& f,PhysicsCastStats* s)const{return Cast(Shape::Capsule(r,h),p,d,m,f,s);}
PhysicsCastHit PhysicsWorld::BoxCast(const BodyTransform& p,const glm::vec3& h,const glm::vec3& d,float m,const PhysicsQueryFilter& f,PhysicsCastStats* s)const{return Cast(Shape::Box(h),p,d,m,f,s);}

JointHandle PhysicsWorld::CreateJoint(const JointSettings& settings) {
    if(!m_impl||!ValidJointSettings(settings))return {};
    auto* a=m_impl->Get(settings.bodyA);auto* b=m_impl->Get(settings.bodyB);
    if(!a||(settings.bodyB.IsValid()&&!b)||(b&&a==b)||(a->rigidBody.IsStatic()&&(!b||b->rigidBody.IsStatic())))return {};
    static std::atomic<std::uint64_t> next{1};JointHandle handle{next.fetch_add(1)};
    Wake(settings.bodyA);Wake(settings.bodyB);
    m_impl->joints.push_back({handle,{settings,false,0,0},{}});return handle;
}
bool PhysicsWorld::DestroyJoint(JointHandle handle){JointState old;if(GetJoint(handle,old)){Wake(old.settings.bodyA);Wake(old.settings.bodyB);}if(!m_impl)return false;auto& joints=m_impl->joints;auto it=std::find_if(joints.begin(),joints.end(),[&](const auto& j){return j.handle.id==handle.id;});if(it==joints.end())return false;joints.erase(it);return true;}
bool PhysicsWorld::GetJoint(JointHandle handle,JointState& state)const{if(!m_impl)return false;for(auto& joint:m_impl->joints)if(joint.handle.id==handle.id){if(!m_impl->Get(joint.state.settings.bodyA)||(joint.state.settings.bodyB.IsValid()&&!m_impl->Get(joint.state.settings.bodyB)))return false;state=joint.state;state.active=m_impl->JointActive(joint);return true;}return false;}
bool PhysicsWorld::SetJoint(JointHandle handle,const JointSettings& settings){JointState previous;if(!GetJoint(handle,previous)||!ValidJointSettings(settings)||settings.bodyA.id!=previous.settings.bodyA.id||settings.bodyB.id!=previous.settings.bodyB.id)return false;for(auto& joint:m_impl->joints)if(joint.handle.id==handle.id){if(JointSettingsEqual(joint.state.settings,settings))return true;m_impl->Wake(settings.bodyA.id&Impl::kSlotMask);if(m_impl->Get(settings.bodyB))m_impl->Wake(settings.bodyB.id&Impl::kSlotMask);joint.state.settings=settings;joint.warm.fill(0);return true;}return false;}

bool PhysicsWorld::SetPairCollisionEnabled(BodyHandle a,BodyHandle b,bool enabled){
    if(!m_impl||a.id==b.id||!m_impl->Get(a)||!m_impl->Get(b))return false;
    auto pair=std::minmax(a.id,b.id);if((m_impl->suppressedPairs.count(pair)==0)==enabled)return true;Wake(a);Wake(b);if(enabled)m_impl->suppressedPairs.erase(pair);else m_impl->suppressedPairs.insert(pair);return true;
}

void PhysicsWorld::PersistTouches(SaveArchive& a,const std::function<uint64_t(BodyHandle)>& identity,const std::function<BodyHandle(uint64_t)>& resolve,const std::function<bool(BodyHandle)>& include){
 std::vector<TouchEvent> live;if(!a.reading)for(const auto& [_,event]:m_impl->previousTouches)if(m_impl->Get(event.a)&&m_impl->Get(event.b)&&(!include||(include(event.a)&&include(event.b))))live.push_back(event);
 uint32_t count=uint32_t(live.size());a(count);a.Require(count<=8192,"saved contact-pair limit");if(a.reading)ClearTouchHistory();
 for(uint32_t i=0;i<count;++i){uint64_t first=0,second=0;TouchEvent event;if(!a.reading){event=live.at(i);first=identity(event.a);second=identity(event.b);}
  a.Require(a.reading||(first&&second),"saved contact has unowned required body");
  a(first,second,event.sensor,event.point,event.normal,event.relativeVelocity,event.impulseAvailable,event.normalImpulse);
  if(a.reading){event.a=resolve(first);event.b=resolve(second);a.Require(event.a.IsValid()&&event.b.IsValid()&&event.a.id!=event.b.id,"saved contact body unavailable");if(event.a.id>event.b.id){std::swap(event.a,event.b);event.normal=-event.normal;event.relativeVelocity=-event.relativeVelocity;}event.phase=TouchPhase::Stay;a.Require(m_impl->previousTouches.emplace(std::make_pair(event.a.id,event.b.id),event).second,"duplicate saved contact pair");}
 }
}

void PhysicsWorld::PersistBodyForces(SaveArchive& archive,BodyHandle handle){
 auto* body=m_impl->Get(handle);archive.Require(body!=nullptr,"saved force body unavailable");archive(body->rigidBody.forceAccumulator,body->rigidBody.torqueAccumulator);
}

void PhysicsWorld::PersistKinematic(SaveArchive& archive,BodyHandle handle) {
 auto* body=m_impl->Get(handle);archive.Require(body&&body->isKinematic,"saved kinematic body unavailable");
 auto state=body->kinematic;auto linear=body->rigidBody.linearVelocity,angular=body->rigidBody.angularVelocity;
 archive(state.control,state.target.position,state.target.rotation,state.linearVelocity,state.angularVelocity,
         state.remainingSeconds,state.targetNextStep,linear,angular);
 archive.Require(state.control==KinematicControl::Stopped||state.control==KinematicControl::Target||state.control==KinematicControl::Velocity,"invalid kinematic control");
 archive.Require(UsableKinematicPose(state.target)&&state.remainingSeconds>=0&&state.remainingSeconds<=60,
                 "invalid kinematic target state");
 archive.Require(state.control==KinematicControl::Target||(!state.targetNextStep&&state.remainingSeconds==0),"inconsistent kinematic interval");
 archive.Require(UsableVelocity(state.linearVelocity)&&UsableVelocity(state.angularVelocity)&&UsableVelocity(linear)&&UsableVelocity(angular),"unusable saved kinematic velocity");
 if(state.control==KinematicControl::Target){const auto delta=state.target.position+state.target.rotation*body->shape.pivotOffset-body->rigidBody.position;
     archive.Require(UsableVelocity(delta)&&(state.targetNextStep||state.remainingSeconds==0||
         (UsableVelocity(delta/state.remainingSeconds)&&std::isfinite(glm::pi<float>()/state.remainingSeconds))),"unusable saved kinematic target trajectory");}
 if(archive.reading){body->kinematic=state;body->rigidBody.linearVelocity=linear;body->rigidBody.angularVelocity=angular;
     body->kinematicActiveDuration=0;body->motion.Clear();body->previousPosition=body->rigidBody.position;body->previousOrientation=body->rigidBody.orientation;
     m_impl->InvalidateImpactPose(handle.id&Impl::kSlotMask);m_impl->RefreshProxy(*body);}
}

bool PhysicsWorld::SetMassDistribution(BodyHandle h,float mass,const glm::mat3& inertia){auto* b=m_impl->Get(h);if(!b||!b->isDynamic||!std::isfinite(mass)||mass<=0)return false;for(int i=0;i<3;++i)for(int j=0;j<3;++j)if(!std::isfinite(inertia[i][j])||std::abs(inertia[i][j]-inertia[j][i])>1e-5f)return false;if(inertia[0][0]<=0||inertia[0][0]*inertia[1][1]-inertia[0][1]*inertia[0][1]<=0||glm::determinant(inertia)<=0)return false;Wake(h);b->rigidBody.inverseMass=1/mass;b->rigidBody.inverseInertiaLocal=glm::inverse(inertia);return true;}
void PhysicsWorld::PersistJointSolverState(SaveArchive& a,JointHandle h){for(auto& j:m_impl->joints)if(j.handle.id==h.id){for(auto& x:j.warm){a(x);a.Require(std::isfinite(x),"nonfinite fracture joint history");}a(j.state.reactionImpulse,j.state.reactionAngularImpulse);for(auto v:{j.state.reactionImpulse,j.state.reactionAngularImpulse})for(int k=0;k<3;++k)a.Require(std::isfinite(v[k]),"nonfinite fracture reaction history");return;}throw std::runtime_error("stale fracture solver history");}

BodyHandle PhysicsWorld::CreateQueryCapsule(float radius,float halfHeight,const BodyTransform& pose){
 auto h=m_impl->AddBody(Shape::Capsule(radius,halfHeight),pose.position,pose.rotation,false,0,0,0);m_impl->Get(h)->queryOnly=true;return h;
}
void PhysicsWorld::BeginPreStepQueryContacts(){m_impl->preStepQueryTouches.clear();m_impl->collectingPreStepQueries=true;}
void PhysicsWorld::ObserveQueryContact(BodyHandle h,const ShapeSweepHit& hit,const glm::vec3& velocity){
 auto* a=m_impl->Get(h);auto* b=m_impl->Get(hit.hitBody);if(!a||!b||!a->enabled||!a->queryOnly||!m_impl->CanCollide(h.id&Impl::kSlotMask,hit.hitBody.id&Impl::kSlotMask))return;
 a->rigidBody.linearVelocity=velocity;Contact c;c.point=hit.point;c.normal=hit.normal;
 m_impl->Observe(h.id&Impl::kSlotMask,hit.hitBody.id&Impl::kSlotMask,c,0,m_impl->collectingPreStepQueries);
}
void PhysicsWorld::FinishQueryTouches(){if(!m_impl->collectingPreStepQueries&&m_impl->queryTouchesPending){m_impl->FinishTouches();m_impl->queryTouchesPending=false;}}

bool PhysicsWorld::IsSleeping(BodyHandle h)const{auto* b=m_impl?m_impl->Get(h):nullptr;return b&&b->sleeping;}
void PhysicsWorld::Wake(BodyHandle h){if(m_impl&&m_impl->Get(h))m_impl->Wake(h.id&Impl::kSlotMask);}
void PhysicsWorld::SetSleepingEnabled(bool enabled){m_impl->sleepingEnabled=enabled;if(!enabled)for(auto i:m_impl->aliveSlots)m_impl->Wake(i);}
void PhysicsWorld::PersistSleep(SaveArchive& a,BodyHandle h){auto* b=m_impl->Get(h);a.Require(b,"missing sleep body");a(b->sleeping,b->quietSeconds,b->lastAcceleration,b->deferredAcceleration);a.Require(std::isfinite(b->quietSeconds)&&b->quietSeconds>=0&&std::isfinite(b->lastAcceleration.x)&&std::isfinite(b->lastAcceleration.y)&&std::isfinite(b->lastAcceleration.z),"invalid settling state");}

bool PhysicsWorld::GetPhysicalMaterial(BodyHandle h,std::string& id,float& friction,float& restitution)const{auto* b=m_impl?m_impl->Get(h):nullptr;if(!b)return false;id=b->physicalMaterial;friction=b->friction;restitution=b->restitution;return true;}
bool PhysicsWorld::SetPhysicalMaterial(BodyHandle h,const std::string& id,float friction,float restitution){auto* b=m_impl?m_impl->Get(h):nullptr;if(!b||!std::isfinite(friction)||friction<0||!std::isfinite(restitution)||restitution<0||restitution>1)return false;if(b->physicalMaterial==id&&b->friction==friction&&b->restitution==restitution)return true;Wake(h);b->physicalMaterial=id;b->friction=friction;b->restitution=restitution;const auto slot=h.id&Impl::kSlotMask;auto remove=[&](auto& cache){cache.erase(std::remove_if(cache.begin(),cache.end(),[&](const auto& c){return c.key.slotA==slot||c.key.slotB==slot;}),cache.end());};remove(m_impl->contactCache);remove(m_impl->pendingCache);return true;}
