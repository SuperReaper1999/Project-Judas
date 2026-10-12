#!/usr/bin/env python3
"""Read-only observer of the actual completed torque-free drift segments.

Copies isolated archives; no source under src/ or original fixture is edited.
One bounded process summary and the worst 32 segments are retained. Observer
evaluation happens after the event schedule is complete, so truncated TOI
segments use their consumed duration rather than their original full horizon.
"""
import hashlib, json, pathlib, shlex, shutil, subprocess, sys

r = pathlib.Path.cwd()
b = r / '.cache/m78-followup/release'
out = r / '.cache/m78-continuation/variants'
ev = r / 'docs/evidence/m78-continuation'
h = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
flags = {}
for line in (b / 'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = shlex.split(v)

observer = r'''
namespace {
struct DiagnosticConsumedEnergy {
 struct Record {
  std::size_t step=0;unsigned owner=0;
  double begin=0,end=0,E0=0,delta=0,relative=0,castDelta=0;
  glm::vec3 omega{0},generator{0},inverseI{0};glm::quat q0{1,0,0,0};
 };
 std::size_t step=0,segments=0,splitBodies=0,eventCount=0;
 double maxAbsolute=0,maxRelative=0,sumAbsolute=0,sumSigned=0,maxCastAbsolute=0;
 std::vector<Record> worst;
 void Observe(const RigidMotionSegment& s) {
  if(s.prescribed||s.inverseMass<=0||s.end<=s.begin||s.angularVelocity==glm::vec3(0))return;
  const glm::dmat3 B(s.inverseInertiaLocal);if(!(glm::determinant(B)>0))return;
  const auto end=s.Evaluate(s.end);
  const auto R0=glm::mat3_cast(glm::normalize(glm::dquat(s.orientation)));
  const auto R1=glm::mat3_cast(glm::normalize(glm::dquat(end.orientation)));
  const auto I=glm::inverse(B);
  const auto L=R0*I*glm::transpose(R0)*glm::dvec3(s.angularVelocity);
  const double E0=.5*glm::dot(glm::dvec3(s.angularVelocity),L);
  const double E1=.5*glm::dot(L,R1*B*glm::transpose(R1)*L);
  const double castE1=.5*glm::dot(glm::dvec3(end.angularVelocity),R1*I*glm::transpose(R1)*glm::dvec3(end.angularVelocity));
  Record x;x.step=step;x.owner=s.owner;x.begin=s.begin;x.end=s.end;x.E0=E0;
  x.delta=E1-E0;x.relative=x.delta/std::max(E0,1e-30);x.castDelta=castE1-E0;
  x.omega=s.angularVelocity;x.generator=s.driftAngularVelocity;x.q0=s.orientation;
  x.inverseI={B[0][0],B[1][1],B[2][2]};
  ++segments;maxAbsolute=std::max(maxAbsolute,std::abs(x.delta));
  maxRelative=std::max(maxRelative,std::abs(x.relative));
  maxCastAbsolute=std::max(maxCastAbsolute,std::abs(x.castDelta));
  sumAbsolute+=std::abs(x.delta);sumSigned+=x.delta;
  worst.push_back(x);std::sort(worst.begin(),worst.end(),[](const Record&a,const Record&b){return std::abs(a.delta)>std::abs(b.delta);});
  if(worst.size()>32)worst.resize(32);
 }
 ~DiagnosticConsumedEnergy() {
  std::printf("CONSUMED_DRIFT_SUMMARY {\"steps\":%zu,\"segments\":%zu,\"splitBodies\":%zu,\"events\":%zu,\"maxAbsolute\":%.17g,\"maxRelative\":%.17g,\"maxCastAbsolute\":%.17g,\"sumAbsolute\":%.17g,\"sumSigned\":%.17g,\"worst32\":[",step,segments,splitBodies,eventCount,maxAbsolute,maxRelative,maxCastAbsolute,sumAbsolute,sumSigned);
  for(std::size_t i=0;i<worst.size();++i){const auto& x=worst[i];
   std::printf("%s{\"step\":%zu,\"owner\":%u,\"begin\":%.17g,\"end\":%.17g,\"E0\":%.17g,\"delta\":%.17g,\"relative\":%.17g,\"castDelta\":%.17g,\"omega\":[%.9g,%.9g,%.9g],\"q0\":[%.9g,%.9g,%.9g,%.9g],\"inverseI\":[%.9g,%.9g,%.9g],\"generator\":[%.9g,%.9g,%.9g]}",i?",":"",x.step,x.owner,x.begin,x.end,x.E0,x.delta,x.relative,x.castDelta,x.omega.x,x.omega.y,x.omega.z,x.q0.w,x.q0.x,x.q0.y,x.q0.z,x.inverseI.x,x.inverseI.y,x.inverseI.z,x.generator.x,x.generator.y,x.generator.z);
  }
  std::printf("]}\n");
 }
};
DiagnosticConsumedEnergy diagnosticConsumedEnergy;
}
'''

for arg in sys.argv[1:]:
    name, base_name = arg.split(':')
    base = out / base_name
    folder = out / name
    folder.mkdir(exist_ok=False)
    for p in base.rglob('*'):
        if p.is_file() and p.suffix in ['.h', '.hpp']:
            dest = folder / p.relative_to(base)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, dest)
    # Both composed archives share the same event/observation implementation.
    source = (out / 'right_observations_v2/PhysicsWorld.cpp').read_text()
    source = '#include <cstdio>\n' + source
    marker = '// Judas\'s own rigid-body physics'
    assert source.count(marker) == 1
    source = source.replace(marker, observer + '\n' + marker)
    marker = '    void AdvanceImpacts(float dt) {\n'
    assert source.count(marker) == 1
    source = source.replace(marker, marker + '        ++diagnosticConsumedEnergy.step;\n')
    marker = '            stats.motionSegments+=body.motion.Segments().size();'
    assert source.count(marker) == 1
    source = source.replace(marker, '            if(body.isDynamic){\n                if(body.motion.Segments().size()>1)++diagnosticConsumedEnergy.splitBodies;\n                for(const auto& s:body.motion.Segments())diagnosticConsumedEnergy.Observe(s);\n            }\n' + marker)
    marker = '    }\n\n    void PublishKinematics(float dt) {'
    assert source.count(marker) == 1
    source = source.replace(marker, '        diagnosticConsumedEnergy.eventCount+=stats.impactEvents;\n' + marker)
    source_path = folder / 'PhysicsWorld.cpp'
    source_path.write_text(source)
    obj = folder / 'PhysicsWorld.cpp.o'
    archive = folder / 'libjudas_engine.a'
    shutil.copy2(base / 'libjudas_engine.a', archive)
    command = ['c++', *flags.get('CXX_DEFINES', []), '-I'+str(folder), *flags.get('CXX_INCLUDES', []), *flags.get('CXX_FLAGS', []), '-c', str(source_path), '-o', str(obj)]
    with (ev / (name + '-compile.log')).open('xb') as f:
        result = subprocess.run(command, stdout=f, stderr=subprocess.STDOUT)
    assert result.returncode == 0
    subprocess.run(['ar', 'r', str(archive), str(obj)], check=True)
    subprocess.run(['ranlib', str(archive)], check=True)
    (ev / (name + '-consumed-observer-build.json')).write_text(json.dumps({
        'command': command, 'base': base_name,
        'baseArchiveSHA256': h(base / 'libjudas_engine.a'),
        'sourceSHA256': h(source_path), 'engineSHA256': h(archive),
        'scope': 'Read-only completed segment endpoints; aggregate and worst32 only; no physical changes'
    }, indent=2)+'\n')
