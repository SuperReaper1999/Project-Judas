#pragma once

// Isolated, opt-in observer. It performs no geometry queries and writes no
// solver/body state. Its diagnostic IO must not be used as a timing baseline.
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>

namespace event_topology {
using Key = std::array<unsigned,4>;
inline Key KeyOf(const ContactKey& key) {
    return {key.slotA,key.slotB,unsigned(key.partA),unsigned(key.partB)};
}
inline double BeforeClosing(const std::vector<ImpactContact>& rows,
                             const std::vector<ContactKey>& keys,const Key& selected) {
    double result=0;
    for (std::size_t i=0;i<rows.size();++i) if (KeyOf(keys[i])==selected) {
        const auto& c=rows[i];
        auto offset=[&](const RigidBody& b,bool a) -> glm::vec3 {
            if (!c.geometry.hasLocalAnchors)
                return glm::vec3(glm::dvec3(c.geometry.point)-glm::dvec3(b.position));
            const auto local=a?c.geometry.localAnchorA:c.geometry.localAnchorB;
            if (a?c.geometry.anchorAInWorldFrame:c.geometry.anchorBInWorldFrame)
                return glm::vec3(local);
            return glm::vec3(ContactRotation(b.orientation)*local);
        };
        const auto va=c.a->linearVelocity+glm::cross(c.a->angularVelocity,offset(*c.a,true));
        const auto vb=c.b->linearVelocity+glm::cross(c.b->angularVelocity,offset(*c.b,false));
        result=std::max(result,-double(glm::dot(va-vb,c.geometry.normal)));
    }
    return result;
}
inline double AfterClosing(const std::vector<ContactConstraint>& rows,
                            const std::vector<ContactKey>& keys,const Key& selected) {
    double result=0;
    for (std::size_t i=0;i<rows.size();++i) if (KeyOf(keys[i])==selected) {
        const auto& c=rows[i];
        const auto va=c.bodyA->linearVelocity+glm::cross(c.bodyA->angularVelocity,c.velocityOffsetA);
        const auto vb=c.bodyB->linearVelocity+glm::cross(c.bodyB->angularVelocity,c.velocityOffsetB);
        result=std::max(result,-double(glm::dot(va-vb,c.normal)));
    }
    return result;
}
struct Counters { std::uint64_t step=0,globalImpacts=0,records=0; };
inline Counters& Global() { static Counters counters; return counters; }
class Observer {
public:
    Observer(std::size_t initialImpacts,const std::vector<CachedContact>& forceCache)
        :m_path(std::getenv("JUDAS_EVENT_TOPOLOGY_OUTPUT")) {
        if (!m_path) return;
        auto& global=Global();
        m_step=++global.step;
        m_initial=initialImpacts;
        global.globalImpacts+=initialImpacts;
        for (const auto& row:forceCache) {
            auto& force=m_force[KeyOf(row.key)];
            force.first+=row.normalImpulse>0;
            force.second+=row.normalImpulse;
        }
    }
    bool Enabled() const { return m_path!=nullptr; }
    void Record(const Key& selected,double previous,double now,double dt,
                 bool support,bool spinning,bool initialSeparated,bool mesh,
                 std::size_t island,std::size_t rows,const std::vector<Key>& actualPairs,
                 bool hardJoints,const ImpactResult& result,double closingBefore,double closingAfter) {
        if (!Enabled()) return;
        auto& global=Global();
        ++global.globalImpacts;
        ++m_events;
        const auto selectedPreviously=m_selected[selected]++;
        const auto actualPreviously=m_actual[selected];
        std::size_t newActual=0;
        for (const auto& key:actualPairs) { newActual+=m_actual[key]==0; ++m_actual[key]; }
        constexpr std::uint64_t cap=4096;
        if (++global.records>cap+1) return;
        std::ofstream out(m_path,std::ios::app);
        if (!out) throw std::runtime_error("event topology observer output unavailable");
        if (global.records==cap+1) { out<<"{\"truncated_after_records\":"<<cap<<"}\n"; return; }
        const auto force=m_force.find(selected);
        out<<std::setprecision(17)<<"{\"step\":"<<m_step
           <<",\"global_impact_sequence\":"<<global.globalImpacts
           <<",\"chronological_event_in_step\":"<<m_events
           <<",\"initial_impacts_in_step\":"<<m_initial
           <<",\"selected\":["<<selected[0]<<','<<selected[1]<<','<<selected[2]<<','<<selected[3]<<']'
           <<",\"previous_event_time_s\":"<<previous<<",\"event_time_s\":"<<now
           <<",\"advance_s\":"<<now-previous<<",\"remaining_s\":"<<dt-now
           <<",\"prior_selected_count\":"<<selectedPreviously
           <<",\"prior_actual_primitive_pair_occurrences\":"<<actualPreviously
           <<",\"new_actual_primitive_pairs_in_step\":"<<newActual
           <<",\"actual_pairs\":"<<actualPairs.size()
           <<",\"selected_support\":"<<(support?"true":"false")
           <<",\"selected_spinning\":"<<(spinning?"true":"false")
           <<",\"selected_in_initial_separated_list\":"<<(initialSeparated?"true":"false")
           <<",\"selected_static_mesh\":"<<(mesh?"true":"false")
           <<",\"initial_regular_force_positive_rows\":"<<(force==m_force.end()?0:force->second.first)
           <<",\"initial_regular_force_impulse_Ns\":"<<(force==m_force.end()?0:force->second.second)
           <<",\"island_bodies\":"<<island<<",\"contact_rows\":"<<rows
           <<",\"hard_joints\":"<<(hardJoints?"true":"false")
           <<",\"effective_restitution\":"<<result.effectiveRestitution
           <<",\"selected_max_closing_before_mps\":"<<closingBefore
           <<",\"selected_max_closing_after_mps\":"<<closingAfter
           <<",\"kinetic_before_J\":"<<result.kineticBefore<<",\"kinetic_after_J\":"<<result.kineticAfter
           <<",\"boundary_work_J\":"<<result.prescribedBoundaryWork
           <<",\"energy_guard_pass\":"<<(result.energyBudgetSatisfied?"true":"false")
           <<",\"safe_retry\":"<<(result.safetyFallback?"true":"false")<<"}\n";
    }
private:
    const char* m_path;
    std::uint64_t m_step=0,m_initial=0,m_events=0;
    std::map<Key,std::size_t> m_selected,m_actual;
    std::map<Key,std::pair<std::size_t,double>> m_force;
};
} // namespace event_topology
