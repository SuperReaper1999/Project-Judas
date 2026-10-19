#!/usr/bin/env python3
"""Isolated, physical-support-order experiment; no production source changes."""
import datetime
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile

OWNED = Path(__file__).resolve().parent
ROOT = OWNED.parents[2]
BUILD = ROOT / 'build'
FROZEN = OWNED.parent / 'baseline-e30'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

receipt = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'commands': [], 'base_manifest_sha256': sha(FROZEN / 'FROZEN.json'),
           'source_artifacts': {}, 'scope': 'BFS support-layer velocity-row ordering only'}

def run(argv, cwd=ROOT):
    receipt['commands'].append({'argv': list(map(str, argv)), 'cwd': str(cwd)})
    subprocess.run(argv, cwd=cwd, check=True)

sources = {}
with tarfile.open(FROZEN / 'source-inputs.tar.gz') as tar:
    for name in ['ContactSolver.h', 'ContactSolver.cpp', 'ImpactSolver.h', 'ImpactSolver.cpp']:
        sources[name] = tar.extractfile('src/' + name).read().decode()
        (OWNED / (name + '.original')).write_text(sources[name])

header = sources['ContactSolver.h']
assert header.count('private:\n') == 1
header = header.replace('private:\n', '''private:
    friend class ImpactSolver;
    // Experiment: visit the existing rows by a deterministic permutation.
    // The published constraint vector and every row's identity remain intact.
    void SolveVelocitiesOrdered(const std::vector<std::size_t>& order, int iterations);
''', 1)
(OWNED / 'ContactSolver.h').write_text(header)
(OWNED / 'ImpactSolver.h').write_text(sources['ImpactSolver.h'])

contact = sources['ContactSolver.cpp']
start = contact.index('void ContactSolver::SolveVelocities(int iterations) {')
end = contact.index('\nvoid ContactSolver::SolvePositions(', start)
ordered = contact[start:end]
ordered = ordered.replace('void ContactSolver::SolveVelocities(int iterations) {',
                          'void ContactSolver::SolveVelocitiesOrdered(const std::vector<std::size_t>& order, int iterations) {', 1)
assert ordered.count('for (ContactConstraint& c : m_constraints) {') == 1
ordered = ordered.replace('for (ContactConstraint& c : m_constraints) {',
                          'for (std::size_t index : order) {\n            ContactConstraint& c = m_constraints[index];', 1)
contact = contact[:end] + '\n' + ordered + contact[end:]
(OWNED / 'ContactSolver.cpp').write_text(contact)

impact = sources['ImpactSolver.cpp']
assert impact.count('    auto run = [&](bool safe) {') == 1
impact = impact.replace('    auto run = [&](bool safe) {', '''    // Physical contact graph, not world height: valid for arbitrary gravity.
    // Stable breadth-first layers propagate support outward from prescribed
    // infinite-mass boundaries. Disconnected components retain original ties.
    std::vector<std::size_t> velocityOrder;
    bool orderReady = false;
    auto prepareOrder = [&]() {
        orderReady = true;
        const std::size_t count = m_solver.m_frames.size();
        const std::size_t unknown = std::numeric_limits<std::size_t>::max();
        std::vector<std::size_t> depth(count, unknown), queue;
        queue.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (m_solver.m_frames[i].body->IsStatic()) {
                depth[i] = 0;
                queue.push_back(i);
            }
        }
        if (queue.empty()) return; // Free-flight groups retain the old order.
        const auto& rows = m_solver.m_constraints;
        std::vector<std::size_t> offsets(count + 1, 0);
        for (const auto& row : rows) {
            ++offsets[row.frameA + 1];
            ++offsets[row.frameB + 1];
        }
        for (std::size_t i = 1; i <= count; ++i) offsets[i] += offsets[i - 1];
        auto next = offsets;
        std::vector<std::size_t> neighbours(2 * rows.size());
        for (const auto& row : rows) {
            neighbours[next[row.frameA]++] = row.frameB;
            neighbours[next[row.frameB]++] = row.frameA;
        }
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
            const auto body = queue[cursor];
            for (std::size_t edge = offsets[body]; edge < offsets[body + 1]; ++edge) {
                const auto other = neighbours[edge];
                if (depth[other] == unknown) {
                    depth[other] = depth[body] + 1;
                    queue.push_back(other);
                }
            }
        }
        // Stable counting sort: same-depth contacts retain authored encounter
        // order, and rows themselves never move or acquire different indices.
        auto rank = [&](const ContactConstraint& row) {
            const auto d = std::min(depth[row.frameA], depth[row.frameB]);
            return d == unknown ? count : d;
        };
        std::vector<std::size_t> buckets(count + 2, 0);
        for (const auto& row : rows) ++buckets[rank(row) + 1];
        for (std::size_t i = 1; i < buckets.size(); ++i) buckets[i] += buckets[i - 1];
        velocityOrder.resize(rows.size());
        for (std::size_t i = 0; i < rows.size(); ++i)
            velocityOrder[buckets[rank(rows[i])]++] = i;
    };
    auto run = [&](bool safe) {''', 1)
old = '        if(!joints||joints->Empty())m_solver.SolveVelocities();'
assert impact.count(old) == 1
impact = impact.replace(old, '''        if(!joints||joints->Empty()) {
            if (result.effectiveRestitution == 0.0f) {
                if (!orderReady) prepareOrder();
                if (!velocityOrder.empty())
                    m_solver.SolveVelocitiesOrdered(velocityOrder, ContactSolver::kVelocityIterations);
                else m_solver.SolveVelocities();
            } else m_solver.SolveVelocities();
        }''', 1)
(OWNED / 'ImpactSolver.cpp').write_text(impact)

flags = {}
for line in (BUILD / 'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
objects = []
for stem in ['ContactSolver', 'ImpactSolver']:
    obj = OWNED / (stem + '.cpp.o')
    run(['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'],
         '-I' + str(OWNED), '-MMD', '-MF', str(OWNED / (stem + '.d')),
         '-c', str(OWNED / (stem + '.cpp')), '-o', str(obj)])
    objects.append(obj)
archive = OWNED / 'libjudas_engine.a'
shutil.copy2(FROZEN / 'libjudas_engine.a', archive)
run(['ar', 'r', str(archive), *map(str, objects)])
link = shlex.split((BUILD / 'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link = [x for x in link if not x.startswith('-Wl,--dependency-file=')]
binary = OWNED / 'judas_physics_stress_live_benchmark'
link[link.index('-o') + 1] = str(binary)
link = [str(archive) if x == 'libjudas_engine.a' else x for x in link]
run(link, BUILD)
for file in sorted(OWNED.iterdir()):
    if file.is_file() and file.name not in ['BUILD.json', 'build.log']:
        receipt['source_artifacts'][file.name] = {'sha256': sha(file), 'bytes': file.stat().st_size}
receipt['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
(OWNED / 'BUILD.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps({'binary': str(binary), 'sha256': sha(binary)}, indent=2))
