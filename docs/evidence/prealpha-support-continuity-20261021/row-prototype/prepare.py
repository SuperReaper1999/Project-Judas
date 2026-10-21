from pathlib import Path
import hashlib,json
root=Path('/home/conner/Documents/GitHub/Project-Judas')
out=root/'.cache/persistent-contact-20261021/row-prototype'
def replace(s,old,new):
 assert s.count(old)==1,(old,s.count(old))
 return s.replace(old,new)
source=root/'src/ContactSolver.cpp';s=source.read_text()
(out/'ContactSolver.original.cpp').write_bytes(source.read_bytes())
s=replace(s,'#include "ContactSolver.h"','#include "ContactSolver.h"\n#include "RowDependency.h"')
s=replace(s,'void ContactSolver::Clear() {','''void ContactSolver::Clear() {
    if (auto* dependencies = row_dependency::ActiveFor(this)) dependencies->Clear();''')
s=replace(s,'    m_pending.clear();\n}\n\nvoid ContactSolver::SetInelasticVelocityTarget','''    m_pending.clear();
    if (auto* dependencies = row_dependency::ActiveFor(this))
        dependencies->Prepare(m_constraints, firstRow, m_frames.size());
}

void ContactSolver::SetInelasticVelocityTarget''')
s=replace(s,'    m_constraints[row].restitutionBias = target;','''    m_constraints[row].restitutionBias = target;
    if (auto* dependencies = row_dependency::ActiveFor(this)) dependencies->Retarget(row);''')
s=replace(s,'void ContactSolver::SolveVelocities(int iterations) {','''void ContactSolver::SolveVelocities(int iterations) {
    // Non-tracked solves may change both velocities and own accumulators.
    if (auto* dependencies = row_dependency::ActiveFor(this)) dependencies->InvalidateAll();''')
s=replace(s,'                                              std::vector<RigidBody*>& activated) {\n    std::vector<unsigned char> seen','''                                              std::vector<RigidBody*>& activated) {
    auto* dependencies = row_dependency::ActiveFor(this);
    std::vector<unsigned char> seen''')
s=replace(s,'            auto& c = m_constraints[index];\n            const auto linearA','''            auto& c = m_constraints[index];
            if (dependencies && !dependencies->NeedsEvaluation(index, c)) continue;
            const auto linearA''')
s=replace(s,'            if (!SolveVelocityConstraint(c))\n                continue;','''            const bool accumulatorChanged = SolveVelocityConstraint(c);
            if (dependencies) dependencies->Evaluated(index, c, accumulatorChanged);
            if (!accumulatorChanged)
                continue;''')
(out/'ContactSolver.cpp').write_text(s)
source=root/'src/ImpactSolver.cpp';s=source.read_text()
(out/'ImpactSolver.original.cpp').write_bytes(source.read_bytes())
s=replace(s,'#include "ImpactSolver.h"','#include "ImpactSolver.h"\n#include "RowDependency.h"')
s=replace(s,'        throw std::invalid_argument("invalid growing impact interval");\n    m_saved.clear();\n    m_solver.Clear();','''        throw std::invalid_argument("invalid growing impact interval");
    m_saved.clear();
    m_solver.Clear();
    row_dependency::Graph dependencies(&m_solver);
    row_dependency::Scope dependencyScope(dependencies);''')
(out/'ImpactSolver.cpp').write_text(s)
# Original public headers preserved, not altered or needed as overlay definitions.
for name in ['ContactSolver.h','ImpactSolver.h']:
 (out/(name+'.original')).write_bytes((root/'src'/name).read_bytes())
# Existing deterministic randomized oracle, made to exercise the opt-in private graph.
source=root/'tests/TrackedContactRowsTests.cpp';s=source.read_text()
s=replace(s,'#include "../third_party/nlohmann/json.hpp"','#include "nlohmann/json.hpp"\n#include "RowDependency.h"')
s=replace(s,'    unsigned checks = 0, failures = 0;','''    unsigned checks = 0, failures = 0;
    row_dependency::SetStatsEnabled(true);
    row_dependency::ResetStats();''')
s=replace(s,'        ContactSolver full, tracked;','''        ContactSolver full, tracked;
        row_dependency::Graph dependencies(&tracked);
        row_dependency::Scope dependencyScope(dependencies);''')
s=replace(s,'                                {"tracked_visits", trackedVisits}}','''                                {"tracked_visits", trackedVisits},
                                {"actually_evaluated", row_dependency::stats.evaluated},
                                {"exact_skips", row_dependency::stats.skipped},
                                {"accumulator_only_changes", row_dependency::stats.accumulatorOnlyChanges}}''')
(out/'TrackedContactRowsDependencyTests.cpp').write_text(s)
manifest={'production_inputs':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'src/ContactSolver.cpp',root/'src/ImpactSolver.cpp',root/'src/ContactSolver.h',root/'src/ImpactSolver.h',source]},'purpose':'private exact ordered-row dependency scheduling; not built or run'}
(out/'INPUT_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Private dependency overlays prepared; no production files modified.')
