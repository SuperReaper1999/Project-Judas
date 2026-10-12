#!/usr/bin/env python3
"""Owned one-step warm-SEED discrimination; never erase geometry/contact observation eligibility."""
import pathlib,hashlib,json
r=pathlib.Path.cwd();out=r/'.cache/m78-continuation/cold-warm-seed-observer';out.mkdir(exist_ok=False)
pw=(r/'src/PhysicsWorld.cpp').read_text();js=(r/'src/JointSolver.cpp').read_text();fixture=(r/'.cache/m78-continuation/cold-state-observer/BalanceLabStateObserver.cpp').read_text()
block='''
namespace {
bool m78ObserveNextSolve=false,m78DisableNextWarm=false,m78ObserveCurrentSolve=false,m78DisableCurrentWarm=false;
unsigned m78DiagnosticResidualRecords=0;
}
void M78DiagnosticObserveNextSolve(bool disableWarm){m78ObserveNextSolve=true;m78DisableNextWarm=disableWarm;}
bool M78DiagnosticRecordJointResidual(){return m78ObserveCurrentSolve&&m78DiagnosticResidualRecords++<32;}
'''
marker='// Judas\'s own rigid-body physics'
assert pw.count(marker)==1
pw=pw.replace(marker,block+'\n'+marker).replace('#include <atomic>','#include <atomic>\n#include <iostream>')
marker='void PhysicsWorld::Step(float fixedDeltaTime) {'
pw=pw.replace(marker,marker+'''
    m78ObserveCurrentSolve=m78ObserveNextSolve;m78ObserveNextSolve=false;
    m78DisableCurrentWarm=m78DisableNextWarm;m78DisableNextWarm=false;
    m78DiagnosticResidualRecords=0;
''')
old='c.warmNormal,c.warmTangent,&oa.rotation,&ob.rotation);';assert pw.count(old)==1
pw=pw.replace(old,'m78DisableCurrentWarm?0.f:c.warmNormal,m78DisableCurrentWarm?glm::vec3(0):c.warmTangent,&oa.rotation,&ob.rotation);')
old='warmNormal,warmTangent,&orientationA.rotation,&orientationB.rotation);';assert pw.count(old)==1
pw=pw.replace(old,'m78DisableCurrentWarm?0.f:warmNormal,m78DisableCurrentWarm?glm::vec3(0):warmTangent,&orientationA.rotation,&orientationB.rotation);')
old='w.jointSolver.Prepare(w.JointInputs(),fixedDeltaTime);';assert pw.count(old)==1
pw=pw.replace(old,'w.jointSolver.Prepare(w.JointInputs(),fixedDeltaTime,!m78DisableCurrentWarm);')
old='w.jointSolver.SolveIteration();}}';assert pw.count(old)==1
pw=pw.replace(old,'''w.jointSolver.SolveIteration();if(m78ObserveCurrentSolve){float maximum=0;for(const auto& c:w.solver.Constraints()){auto va=c.bodyA->linearVelocity+glm::cross(c.bodyA->angularVelocity,c.velocityOffsetA),vb=c.bodyB->linearVelocity+glm::cross(c.bodyB->angularVelocity,c.velocityOffsetB);float residual=glm::dot(va-vb,c.normal)-c.restitutionBias;maximum=std::max(maximum,c.normalImpulse>0?std::abs(residual):std::max(-residual,0.f));}std::cout<<"WARM_SEED_CONTACT residual iteration="<<i<<" rows="<<w.solver.Constraints().size()<<" maximum_m_per_s="<<maximum<<" disabled="<<m78DisableCurrentWarm<<'\\n';}}}''')
js='#include <iostream>\nextern bool M78DiagnosticRecordJointResidual();\n'+js
old='if(r.motor)*r.motor=total;}}';assert js.count(old)==1
js=js.replace(old,'''if(r.motor)*r.motor=total;}if(M78DiagnosticRecordJointResidual()){float maximum=0,rms=0;for(const auto& r:m_rows){float velocity=glm::dot(r.linearA,r.a->linearVelocity)+glm::dot(r.linearB,r.b->linearVelocity)+glm::dot(r.angularA,r.a->angularVelocity)+glm::dot(r.angularB,r.b->angularVelocity);float correction=std::clamp(r.impulse-(velocity+r.bias+r.gamma*r.impulse)*r.mass,r.low,r.high)-r.impulse;maximum=std::max(maximum,std::abs(correction));rms+=correction*correction;}std::cout<<"WARM_SEED_JOINT projected_impulse_residual rows="<<m_rows.size()<<" maximum="<<maximum<<" rms="<<std::sqrt(rms/std::max(std::size_t(1),m_rows.size()))<<'\\n';}}''')
fixture='extern void M78DiagnosticObserveNextSolve(bool);\n'+fixture
old='diagnosticColdTrace.open(out/"diagnostic-uninterrupted-trace.jsonl");stepTrace=&diagnosticColdTrace;';assert fixture.count(old)==1
fixture=fixture.replace(old,old+'''
    const char* warmOff=std::getenv("JUDAS_DIAGNOSTIC_WARM_SEEDS_OFF");
    M78DiagnosticObserveNextSolve(warmOff&&std::string(warmOff)=="1");
''')
for name,s in [('PhysicsWorld.cpp',pw),('JointSolver.cpp',js),('BalanceLabWarmSeedObserver.cpp',fixture)]: (out/name).write_text(s)
(out/'IDENTITY.json').write_text(json.dumps({'scope':'Single initial completed solve: disable only joint/contact warm SEEDS. Geometry-cache eligibility, actual completed observations, source commands, clocks, inputs, 60Hz and original acceptance retained. Passive projected impulse/contact residual observers bounded to32records.',
 'sourceSHA256':{p:hashlib.sha256((r/p).read_bytes()).hexdigest()for p in ['src/PhysicsWorld.cpp','src/JointSolver.cpp','src/PhysicalBalance.cpp','tests/BalanceLabTests.cpp']},
 'overlaySHA256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('*.cpp')}},indent=2)+'\n')
print(out)
