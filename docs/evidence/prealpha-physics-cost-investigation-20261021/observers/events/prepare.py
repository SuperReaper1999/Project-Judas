from pathlib import Path
r=Path.cwd();w=Path(__file__).resolve().parent;s=(r/'src/physics/WorldImpactEvents.cpp').read_text();s='#include "EventAudit.h"\n#include "CCDAudit.h"\n'+s
def sub(a,b,n=1):
 global s
 if s.count(a)!=n:raise RuntimeError((a,s.count(a),n))
 s=s.replace(a,b)
sub('    // Keep published contact storage ordered', '    // Keep published contact storage ordered',0) if False else None
sub('auto PhysicsWorld::Impl::AdvanceImpacts(float dt, bool coupledVelocityChanged) -> void {','auto PhysicsWorld::Impl::AdvanceImpacts(float dt, bool coupledVelocityChanged) -> void {\n    event_audit::Scope totalAudit(event_audit::Advance);')
# Coarse scoped lambdas preserve exact function evaluation order and return values.
sub('    auto schedule = [&](const std::array<unsigned, 4> &key) {','    auto schedule = [&](const std::array<unsigned, 4> &key) {\n        event_audit::Scope auditSchedule(event_audit::Reschedule);')
sub('    const auto scheduleInitial = [&](const std::array<unsigned, 4>& key) {','    const auto scheduleInitial = [&](const std::array<unsigned, 4>& key) {\n        event_audit::Scope auditInitial(event_audit::InitialSchedule);\n        ccd_audit::OriginScope origin(ccd_audit::Origin::InitialSchedule);')
sub('        now = earliest;','        now = earliest;\n        event_audit::Begin(selected,now,coupledVelocityChanged);')
sub('        const bool incremental =\n            !coupledVelocityChanged &&\n            TryIncrementalImpact(selected, now, dt, jointBodies, localResponse);','''        const bool incremental = [&] {
            event_audit::Scope scope(event_audit::Trial);
            ccd_audit::OriginScope origin(ccd_audit::Origin::Domain);
            return !coupledVelocityChanged && TryIncrementalImpact(selected, now, dt, jointBodies, localResponse);
        }();''')
sub('        } else {\n            if (work.active)\n                ++work.incrementalFallbacks;','        } else {\n            event_audit::Scope legacyScope(event_audit::Legacy);\n            if (work.active)\n                ++work.incrementalFallbacks;')
sub('            PhysicsWorkTimer timer(work.impactObservations, work.active);','            event_audit::Scope observations(event_audit::Observations);\n            PhysicsWorkTimer timer(work.impactObservations, work.active);')
sub('        if (result.effectiveRestitution == 0) {\n            // An inelastic event','        if (result.effectiveRestitution == 0) {\n            event_audit::Scope supportScope(event_audit::ClassifySupport);\n            ccd_audit::OriginScope origin(ccd_audit::Origin::Support);\n            // An inelastic event')
sub('            PhysicsWorkTimer timer(work.continuationCache, work.active);','            event_audit::Scope warmScope(event_audit::WarmCache);\n            PhysicsWorkTimer timer(work.continuationCache, work.active);')
sub('                        cached.normalImpulse = 0;','                        event_audit::Discard(cached.normalImpulse,cached.tangentImpulse);\n                        cached.normalImpulse = 0;')
sub('            changed.push_back(slot);','            event_audit::Delta(oldVelocities[i].first,oldVelocities[i].second,b.rigidBody.linearVelocity,b.rigidBody.angularVelocity);\n            changed.push_back(slot);')
# Scope support invalidation only; no unbraced control-flow changes.
sub('        supportPairs.erase(\n            std::remove_if(supportPairs.begin(), supportPairs.end(),','        { event_audit::Scope supportAudit(event_audit::ValidateSupport);\n        ccd_audit::OriginScope origin(ccd_audit::Origin::Support);\n        supportPairs.erase(\n            std::remove_if(supportPairs.begin(), supportPairs.end(),')
sub('            supportPairs.end());\n        // Cached event times','            supportPairs.end());\n        }\n        // Cached event times')
# Distinguish dirty broadphase/pair work from scheduled native TOI searches via nested timer.
sub('        dirty.clear();','        { event_audit::Scope discoverAudit(event_audit::DiscoverDirty);\n        ccd_audit::OriginScope origin(ccd_audit::Origin::Reschedule);\n        dirty.clear();')
# Close block only after all dirty schedule calls; maintain variable scope for old code.
sub('        if (now >= dt)\n            break;','        }\n        event_audit::End(incremental,island.size(),rows.size());\n        if (now >= dt)\n            break;')
sub('    spinningPairs.clear();','    { event_audit::Scope initialClassify(event_audit::InitialClassification);\n    ccd_audit::OriginScope origin(ccd_audit::Origin::Support);\n    spinningPairs.clear();')
sub('    // Preserve joint encounter order','    }\n    // Preserve joint encounter order')
sub('        std::vector<unsigned> changed;\n        for (std::size_t i = 0; i < island.size(); ++i) {','        std::vector<unsigned> changed;\n        { event_audit::Scope motionScope(event_audit::PublishMotion);\n        for (std::size_t i = 0; i < island.size(); ++i) {')
sub('        // Solver rows and event pairs','        }\n        // Solver rows and event pairs')
(w/'WorldImpactEvents.cpp').write_text(s)
