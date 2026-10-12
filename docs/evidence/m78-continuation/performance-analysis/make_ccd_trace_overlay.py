#!/usr/bin/env python3
"""Create an owned diagnostic-only PhysicsWorld overlay; never edit source/build."""
import argparse, hashlib, json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=ROOT/'src/PhysicsWorld.cpp')
p.add_argument('--output', type=Path, required=True, help='A new owned snapshot directory')
a = p.parse_args()
source = a.source.resolve()
output = a.output.resolve()
if output == source.parent or output.exists():
    raise SystemExit('Output must be a new owned directory; preserve prior observations')
if not output.is_relative_to(ROOT/'docs/evidence/m78-continuation') and not output.is_relative_to(ROOT/'.cache/m78-continuation'):
    raise SystemExit('Restrict overlay output to owned M78 continuation artifacts')
original = source.read_text()
text = original
anchors = []
def replace(old, new, count=1):
    global text
    found = text.count(old)
    if found != count:
        raise SystemExit(f'Expected {count} occurrences of anchor, found {found}: {old[:100]!r}')
    text = text.replace(old,new,count)
    anchors.append(old)

replace('    ContactManifold PairAt(unsigned a,unsigned b,int pa,int pb,double time,float margin) {\n', '''    // Diagnostic overlay only. Pair geometry timings accumulate by execution
    // phase, avoiding thousands of profiler events per fixed step. The existing
    // full contact evaluator and solver expressions below are unchanged.
    std::array<double,4> m78PairMilliseconds{};
    std::array<std::size_t,4> m78PairCalls{};
    unsigned m78GeometryPhase=0;
    struct M78GeometryPhase {
        unsigned* current;unsigned previous;
        M78GeometryPhase(unsigned& value,unsigned phase):current(&value),previous(value){value=phase;}
        void End(){if(current){*current=previous;current=nullptr;}}
        ~M78GeometryPhase(){End();}
    };
    struct M78PairTimer {
        std::array<double,4>& durations;std::array<std::size_t,4>& calls;
        unsigned phase;Clock::time_point start=Clock::now();
        M78PairTimer(std::array<double,4>& d,std::array<std::size_t,4>& c,unsigned p):durations(d),calls(c),phase(p){}
        ~M78PairTimer(){durations[phase]+=MillisecondsBetween(start,Clock::now());++calls[phase];}
    };
    ContactManifold PairAt(unsigned a,unsigned b,int pa,int pb,double time,float margin) {
        M78PairTimer m78PairTimer(m78PairMilliseconds,m78PairCalls,m78GeometryPhase);
''')
replace('    double ImpactTime(unsigned a,unsigned b,int pa,int pb,double start,double finish) {\n', '''    double ImpactTime(unsigned a,unsigned b,int pa,int pb,double start,double finish) {
        JUDAS_PROFILE_SCOPE("CCD impact-time query");
        M78GeometryPhase m78QueryPhase(m78GeometryPhase,1);
''')
replace('    void AdvanceImpacts(float dt) {\n        stepDuration=dt;', '''    void AdvanceImpacts(float dt) {
        m78PairMilliseconds.fill(0);m78PairCalls.fill(0);m78GeometryPhase=0;
        const auto m78CookedBefore=GetCollisionGeometryStats();
        static ProfileLabel m78IslandLabel("CCD island discovery"),m78RowsLabel("CCD event contact rows"),
            m78JointsLabel("CCD event joint preparation"),m78SolveLabel("CCD event impulse solve");
        stepDuration=dt;''')
replace('            std::vector<unsigned> island;\n', '''            ProfileScope m78IslandScope(m78IslandLabel);
            M78GeometryPhase m78IslandPhase(m78GeometryPhase,2);
            std::vector<unsigned> island;
''')
replace('            // A supported body\'s incoming impact is mechanically coupled even\n', '''            m78IslandPhase.End();m78IslandScope.End();
            // A supported body's incoming impact is mechanically coupled even
''')
replace('            std::vector<ImpactContact> rows;\n            std::vector<std::array<unsigned,4>> eventPairs;\n', '''            ProfileScope m78RowsScope(m78RowsLabel);
            M78GeometryPhase m78RowsPhase(m78GeometryPhase,3);
            std::vector<ImpactContact> rows;
            std::vector<std::array<unsigned,4>> eventPairs;
''')
replace('            if(rows.empty()) throw std::runtime_error("impact bracket lost contact geometry");\n            eventJointSolver.Prepare(JointInputs(&included),dt,false,true);\n            const auto result=impactSolver.Solve(rows,dt,capture,eventJointSolver.Empty()?nullptr:&eventJointSolver);\n', '''            if(rows.empty()) throw std::runtime_error("impact bracket lost contact geometry");
            m78RowsPhase.End();m78RowsScope.End();
            ProfileScope m78JointsScope(m78JointsLabel);
            eventJointSolver.Prepare(JointInputs(&included),dt,false,true);
            m78JointsScope.End();
            ProfileScope m78SolveScope(m78SolveLabel);
            const auto result=impactSolver.Solve(rows,dt,capture,eventJointSolver.Empty()?nullptr:&eventJointSolver);
            m78SolveScope.End();
''')
replace('    void PublishKinematics(float dt) {\n', '''    void PublishKinematics(float dt) {
''') # Assert stable next-function anchor used for placement below.
old='''        }
    }

    void PublishKinematics(float dt) {
'''
new='''        }
        JUDAS_PROFILE_COUNTER("CCD other pair geometry ms",m78PairMilliseconds[0],ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD query pair geometry ms",m78PairMilliseconds[1],ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD island pair geometry ms",m78PairMilliseconds[2],ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD contact-row pair geometry ms",m78PairMilliseconds[3],ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD other pair geometry calls",double(m78PairCalls[0]),ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD query pair geometry calls",double(m78PairCalls[1]),ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD island pair geometry calls",double(m78PairCalls[2]),ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD contact-row pair geometry calls",double(m78PairCalls[3]),ProfileCounterMode::Latest);
        const auto m78CookedAfter=GetCollisionGeometryStats();
        JUDAS_PROFILE_COUNTER("CCD cooked BVH nodes",double(m78CookedAfter.nodes-m78CookedBefore.nodes),ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD cooked triangle tests",double(m78CookedAfter.triangles-m78CookedBefore.triangles),ProfileCounterMode::Latest);
        JUDAS_PROFILE_COUNTER("CCD cooked convex SAT tests",double(m78CookedAfter.convexTests-m78CookedBefore.convexTests),ProfileCounterMode::Latest);
    }

    void PublishKinematics(float dt) {
'''
replace(old,new)
output.mkdir(parents=True)
(output/'PhysicsWorld.original.cpp').write_text(original)
(output/'PhysicsWorld.instrumented.cpp').write_text(text)
sha=lambda b:hashlib.sha256(b).hexdigest()
receipt={
    'purpose':'Diagnostic attribution only; no production edits or build execution',
    'source':str(source),
    'sourceSHA256':sha(source.read_bytes()),
    'originalSnapshotSHA256':sha((output/'PhysicsWorld.original.cpp').read_bytes()),
    'overlaySHA256':sha((output/'PhysicsWorld.instrumented.cpp').read_bytes()),
    'replacementAnchorCount':len(anchors),
    'scopes':['CCD impact-time query','CCD island discovery','CCD event contact rows','CCD event joint preparation','CCD event impulse solve'],
    'geometryCounters':'PairAt elapsed time/call counts, accumulated separately for query/island/contact rows/other; cooked BVH/triangle/SAT counter deltas',
    'limitations':[
        'Instrumented wall times include timer/profiler overhead; final acceptance timing must use uninstrumented candidate.',
        'Pair geometry includes pose sampling and prepared orientations, not just CookedContacts.',
        'Impact-time scope is inclusive of recursive interval splitting; use exclusive scope totals or geometry phase counters to avoid double counting.',
        'Existing profiler history/event caps are unchanged; inspect dropped/incomplete/truncated diagnostics before interpreting scopes.',
        'This overlay does not change geometry, inputs, cadence, iteration limits or solver expressions.',
    ],
}
(output/'OVERLAY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt,indent=2))
