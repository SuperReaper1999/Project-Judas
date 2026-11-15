#include "WorldInternal.h"
#include "PhysicsDebugClock.h"
using namespace JudasPhysicsDetail;
void PhysicsWorld::SetDebugObserver(DebugObserver observer){
    if(!m_impl||m_impl->stepping||m_impl->coupledStep||m_impl->contactPhaseContext||m_impl->debugNotifying)
        throw std::logic_error("physics debugger requires a completed fixed boundary");
    m_impl->debugObserver=std::move(observer);
}
bool PhysicsWorld::HasDebugObserver()const{return m_impl&&bool(m_impl->debugObserver);}
void PhysicsWorld::DebugPoint(DebugPhase phase){
    switch(phase){
        case DebugPhase::FixedBegin:case DebugPhase::FixedEnd:
        case DebugPhase::MaterialBegin:case DebugPhase::MaterialEnd:
        case DebugPhase::DeformableBegin:case DebugPhase::DeformableEnd:break;
        default:throw std::logic_error("collision boundaries belong to the native scheduler");
    }
    if(m_impl)m_impl->DebugPoint(phase);
}
void PhysicsWorld::Impl::DebugPoint(PhysicsWorld::DebugPhase phase,double at,unsigned a,unsigned b,
    unsigned pa,unsigned pb,std::size_t contacts,std::size_t participants,bool coupled,bool applied,std::size_t queued,const PhysicsWorld::CollisionSearchSample* search){
    if(!debugObserver)return;
    if(debugNotifying)throw std::logic_error("recursive physics debugger notification");
    const auto began=PhysicsDebugClock::base::now();
    struct Guard {bool& active;PhysicsDebugClock::base::time_point began;Guard(bool& x,PhysicsDebugClock::base::time_point t):active(x),began(t){active=true;}~Guard(){active=false;PhysicsDebugClock::Exclude(PhysicsDebugClock::base::now()-began);}} guard(debugNotifying,began);
    JUDAS_PROFILE_WAIT("Physics debugger inspection/wait");
    PhysicsWorld::DebugBoundary point;point.phase=phase;point.sequence=++debugSequence;
    point.activeClockNanoseconds=std::chrono::duration_cast<std::chrono::nanoseconds>(began.time_since_epoch()).count()-PhysicsDebugClock::Excluded();
    point.localTime=at;point.interval=contactPhaseContext?contactPhaseContext->index:0;
    point.macroTime=at<0?-1:at+(contactPhaseContext?double(contactPhaseContext->macroDeltaTime)*point.interval/contactPhaseContext->count:0);
    if(phase==PhysicsWorld::DebugPhase::BeforeImpact){++debugEvent;if(debugLastTime!=point.macroTime){++debugGroup;debugLastTime=point.macroTime;}}
    if(phase==PhysicsWorld::DebugPhase::PredictionDiscarded)++debugDiscarded;
    point.invalidated=debugDiscarded;point.event=debugEvent;point.timeGroup=debugGroup;point.predictionVersion=debugPredictionVersion;
    if(a<bodies.size()){point.a=MakeHandle(a).id;point.revisionA=bodies[a].impactTrajectoryRevision;}
    if(b<bodies.size()){point.b=MakeHandle(b).id;point.revisionB=bodies[b].impactTrajectoryRevision;}
    if(search){point.hasSearch=true;point.search=*search;}
    if(phase==PhysicsWorld::DebugPhase::ResponsePrepared||phase==PhysicsWorld::DebugPhase::BeforeImpact||phase==PhysicsWorld::DebugPhase::TrajectoriesPublished)point.manifold=debugContacts;
    point.partA=pa;point.partB=pb;point.contacts=contacts;point.participants=participants;point.coupled=coupled;point.applied=applied;
    if(phase==PhysicsWorld::DebugPhase::Candidates)point.contacts=mk2Points.size();
    point.candidates=candidatePairs.size();point.stats=stats;point.geometryRequests=work.geometryMemoRequests;point.geometryHits=work.geometryMemoReuses;point.queuedPairs=queued;
    point.searchMs=work.toi.nanoseconds*1e-6;point.samplingMs=work.sampling.nanoseconds*1e-6;point.contactSolveMs=(work.impactSolve.nanoseconds+work.arrivalSolve.nanoseconds)*1e-6;
    debugObserver(point);
}
std::vector<PhysicsWorld::DebugBody> PhysicsWorld::CaptureDebugBodies(double at,std::size_t maximum)const{
    std::vector<DebugBody> result;if(!m_impl)return result;const auto& w=*m_impl;
    result.reserve(std::min(maximum,w.aliveSlots.size()));
    for(auto slot:w.aliveSlots){const auto& b=w.bodies[slot];if(!b.enabled||b.simulationSuspended)continue;if(result.size()==maximum)break;
        auto pose=b.rigidBody;if(at>=0&&!b.motion.Segments().empty())pose=b.motion.EvaluateAfterEvents(at);
        // Deliberate diagnostic body enclosures, not replacement collision shapes.
        auto half=glm::vec3(b.boundingRadius);glm::vec3 localCenter(0);
        if(b.shape.type==ShapeType::Box)half=b.shape.halfExtents;
        else if(b.shape.type==ShapeType::Capsule)half={b.shape.radius,b.shape.halfHeight+b.shape.radius,b.shape.radius};
        else if(b.shape.type==ShapeType::CompoundBoxes&&!b.shape.boxes.empty()){
            glm::vec3 lo(std::numeric_limits<float>::max()),hi(-std::numeric_limits<float>::max());
            for(const auto& child:b.shape.boxes){
                auto h=child.type==ShapeType::Box?child.halfExtents:glm::vec3(b.boundingRadius);
                const auto r=glm::mat3_cast(child.rotation);
                h=glm::abs(r[0])*h.x+glm::abs(r[1])*h.y+glm::abs(r[2])*h.z;
                lo=glm::min(lo,child.localCenter-h);hi=glm::max(hi,child.localCenter+h);
            }
            half=(hi-lo)*.5f;localCenter=(hi+lo)*.5f;
        }
        result.push_back({w.MakeHandle(slot),pose.position+pose.orientation*localCenter,half,pose.orientation,pose.linearVelocity,b.sleeping,b.isDynamic&&!b.isKinematic});
    }return result;
}

void PhysicsWorld::Impl::DebugSearch(const PhysicsWorld::CollisionSearchSample& search){
    DebugPoint(PhysicsWorld::DebugPhase::SearchCompleted,search.hit,search.a&kSlotMask,search.b&kSlotMask,search.partA,search.partB,0,0,false,false,0,&search);
}
