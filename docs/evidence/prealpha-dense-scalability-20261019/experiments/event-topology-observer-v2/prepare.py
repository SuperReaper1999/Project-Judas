#!/usr/bin/env python3
"""Prepare only; compilation is separately coordinated with the host benchmark."""
from pathlib import Path
import tarfile
owned=Path(__file__).resolve().parent
base=owned.parent/'baseline-e30'
with tarfile.open(base/'source-inputs.tar.gz') as tar:
    source=tar.extractfile('src/physics/WorldImpactEvents.cpp').read().decode()
    for member in tar:
        if member.isfile() and member.name.startswith('src/') and member.name.endswith(('.h','.inl')):
            path=owned/'include'/member.name; path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(tar.extractfile(member).read())
(owned/'WorldImpactEvents.original.cpp').write_text(source)
source=source.replace('#include "ImpactCandidateCache.h"',
    '#include "ImpactCandidateCache.h"\n#include "EventTopologyObserver.h"\n#include "PairScheduleCapture.h"',1)
publication='    ObservationPublication cachePublication{contactCache, work.continuationCache, work.active};'
assert source.count(publication)==1
source=source.replace(publication,publication+'\n    event_topology::Observer eventObserver(stats.impactEvents,contactCache);',1)
assert source.count('        now = earliest;')==1
source=source.replace('        now = earliest;', '''        const double observedPreviousTime=now;
        const bool observedSupport=eventObserver.Enabled() && std::binary_search(supportPairs.begin(),supportPairs.end(),selected);
        const bool observedSpinning=eventObserver.Enabled() && std::binary_search(spinningPairs.begin(),spinningPairs.end(),selected);
        const bool observedSeparated=eventObserver.Enabled() && std::find(separatedPairs.begin(),separatedPairs.end(),selected)!=separatedPairs.end();
        now = earliest;''',1)
line='        const auto result = [&] {\n            PhysicsWorkTimer timer(work.impactSolve, work.active);'
assert source.count(line)==1
source=source.replace(line,'''        const double observedClosing=eventObserver.Enabled() ? event_topology::BeforeClosing(rows,observationKeys,selected) : 0;
        event_topology::TracePair(*this,selected,now,dt,now,0,"before_impact");
'''+line,1)
needle='''        if (!result.energyBudgetSatisfied)
            throw std::runtime_error("inelastic impact energy guard failed");'''
assert source.count(needle)==1
source=source.replace(needle,needle+'''
        if (eventObserver.Enabled()) eventObserver.Record(selected,observedPreviousTime,now,dt,
            observedSupport,observedSpinning,observedSeparated,
            StaticMeshPolyPair(selected[0],selected[1],int(selected[2]),int(selected[3])),
            island.size(),rows.size(),eventPairs,!eventJointSolver.Empty(),result,observedClosing,
            event_topology::AfterClosing(impactSolver.Constraints(),observationKeys,selected),
            std::count_if(observationKeys.begin(),observationKeys.end(),[&](const auto& key) {return event_topology::KeyOf(key)==selected;}));''',1)
schedule='''        const auto found = scheduledIndex.find(key);'''
assert source.count(schedule)==2
source=source.replace(schedule,'''        if (event_topology::SelectedTrace(key)) {
            const bool mesh=StaticMeshPolyPair(key[0],key[1],int(key[2]),int(key[3]));
            const bool support=!mesh && std::binary_search(supportPairs.begin(),supportPairs.end(),key);
            const bool spinning=!mesh && !support && std::binary_search(spinningPairs.begin(),spinningPairs.end(),key);
            event_topology::TracePair(*this,key,now,dt,time,
                spinning?SpinPenetrationTarget(key):0,"scheduled_query",!support);
        }
'''+schedule,1)
needle='''        // Solver rows and event pairs retain discovery order.'''
assert source.count(needle)==1
source=source.replace(needle,'''        event_topology::TracePair(*this,selected,now,dt,now,0,"after_trajectory_replacement");
'''+needle,1)
(owned/'WorldImpactEvents.cpp').write_text(source)
print(owned/'WorldImpactEvents.cpp')
