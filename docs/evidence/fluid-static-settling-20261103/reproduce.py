from pathlib import Path
import shlex, subprocess, sys
root=Path.cwd()
d=Path(__file__).resolve().parent
b=root/'.cache/2d-stage1-build'
def run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)
def flags(target):
    f={}
    for line in (b/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
        if line.startswith(("CXX_DEFINES =", "CXX_INCLUDES =", "CXX_FLAGS =")):
            k,v=line.split('=',1); f[k.strip()]=shlex.split(v)
    return [*f["CXX_DEFINES"],*f["CXX_INCLUDES"],*f["CXX_FLAGS"]]
def compile(source, obj, target, extra=[]):
    run(["/usr/bin/c++",*flags(target),*extra,"-c",str(source),"-o",str(obj)])
test='judas_production_fluid_tests'
test_obj=d/'WarmupTest.o'
if not test_obj.exists():
    source=(root/'tests/ProductionFluidTests.cpp').read_text()
    anchor='  const auto placeholder='
    assert source.count(anchor)==1
    path=d/'WarmupTest.cpp'; path.write_text(source.replace(anchor,'  return {};\n'+anchor))
    compile(path,test_obj,test,['-I'+str(root/'tests')])
variant=sys.argv[1]
source=(root/'src/FluidWorld.cpp').read_text()
if variant in ('old','old-detail'):
    source=subprocess.check_output(['git','show','941832fe:src/FluidWorld.cpp'],text=True)
if variant=='keep-static':
    source=source.replace('if(!final.active)continue;',
     'if(!final.active && contacts[i][k].wallVelocity!=glm::vec3(0))continue;')
if variant=='no-hard':
    source=source.replace('if(initialDrift) {','if(false && initialDrift) {')
if variant=='preferred-witness':
    source=source.replace('if (!row.hardEncounter || contact.hardEncounter) row = contact;',
    '''const bool hard=row.hardEncounter || contact.hardEncounter;
                row = contact;row.hardEncounter=hard;''')
if variant=='no-markers':
    source=source.replace('if(!contact.displacementWitness)continue;','')
if variant=='density-segment':
    source=source.replace('        for (int iteration = 0; iteration < iterations; ++iteration) {','''        std::vector<glm::vec3> densityStarts(count);
        for (int iteration = 0; iteration < iterations; ++iteration) {''').replace('            for (std::size_t i = 0; i < count; ++i) {\n                const glm::dvec3 before(positions[i]);','''            densityStarts=positions;
            for (std::size_t i = 0; i < count; ++i) {
                const glm::dvec3 before(positions[i]);''').replace('ApplySolidCollisions(i, starts[i], positions, preparedBoxes, preparedSpheres,\n                                     preparedTerrains','ApplySolidCollisions(i, densityStarts[i], positions, preparedBoxes, preparedSpheres,\n                                     preparedTerrains')
if variant in ('speculative','speculative-zero','speculative-gravity','bounded-face'):
    proposed="""                if(!final.active) {
                    const auto& old=contacts[i][k];
                    if(!old.box || group.yielding ||
                       old.box->start.position!=old.box->end.position ||
                       (old.box->start.rotation!=old.box->end.rotation && old.box->start.rotation!=-old.box->end.rotation)) continue;
                    bool stationary=true;
                    for(const auto& row:contacts[i])if(row.wallVelocity!=glm::vec3(0))stationary=false;
                    if(!stationary)continue;
                    const auto velocity=particle.velocity;
                    auto future=CollideBox(positions[i],positions[i]+velocity*substepTime,
                        *old.box,m_settings.particleRadius,substepTime);
                    if(!future.hit)continue;
                    const auto normal=old.box->inverseEnd*future.normal;
                    const auto local=old.box->inverseEnd*(positions[i]-old.box->end.position);
                    const float distance=std::max(0.f,glm::dot(normal,local)-
                        glm::dot(glm::abs(normal),old.box->halfExtents)-m_settings.particleRadius);
                    velocityContacts.push_back({i,old.owner,future.point,future.normal,future.wallVelocity,
                        positions[i],particle.velocity,particle.mass,alpha1,-distance/substepTime});
                    continue;
                }"""
    if variant=='speculative-zero': proposed=proposed.replace('-distance/substepTime','0')
    if variant=='speculative-gravity':
        proposed=proposed.replace('const auto velocity=particle.velocity;', 'const auto acceleration=gravity.Sample(positions[i]);\n                    const auto velocity=particle.velocity+acceleration*substepTime;')
        proposed=proposed.replace('-distance/substepTime','-distance/substepTime-glm::dot(acceleration,future.normal)*substepTime')
    if variant=='bounded-face':
        proposed=proposed.replace('if(!future.hit)continue;', 'if(!future.hit)continue;\n                    if(glm::dot(future.normal,old.normal)<.9999f)continue;')
    assert source.count("                if(!final.active)continue;")==1
    source=source.replace("                if(!final.active)continue;",proposed)
if variant in ('projected-density','projected-density-inward'):
    source=source.replace("        m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);\n        for (int iteration", """        const auto driftSolidCorrections=solidCorrections;
        m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);
        for (int iteration""")
    if variant=='projected-density':
        change='particle.velocity += (positions[i] - unconstrainedPositions[i] - driftSolidCorrections[i]) / substepTime;'
    else:
        change="""const auto raw=(positions[i]-unconstrainedPositions[i]-solidCorrections[i]);
            auto projected=raw+(solidCorrections[i]-driftSolidCorrections[i]);
            if(glm::dot(projected,projected)>glm::dot(raw,raw))projected=raw;
            particle.velocity += projected/substepTime;"""
    source=source.replace('particle.velocity += (positions[i] - unconstrainedPositions[i] - solidCorrections[i]) / substepTime;',change)
if variant in ('mobility','mobility-pressure','mobility-velocity'):
    a='            const NeighborLists& neighbors = BuildNeighbors(positions, radius, neighborWorkspace);'
    assert source.count(a)==2
    source=source.replace(a,a+"""
            std::vector<std::vector<glm::vec3>> stationaryNormals(count);
            for (std::size_t i=0;i<count;++i) {
                if(contacts[i].empty())continue;
                const auto group=ClassifyContacts(contacts[i],positions[i],m_settings.particleRadius,substepTime);
                if(group.yielding || std::any_of(contacts[i].begin(),contacts[i].end(),[](const auto& r) {
                    return r.wallVelocity!=glm::vec3(0);
                }))continue;
                for(std::size_t k=0;k<contacts[i].size();++k) {
                    const auto& row=group.rows[k];
                    if(row.active && row.preferredFeasible)
                        stationaryNormals[i].push_back(PreferredContact(contacts[i][k],row).normal);
                }
            }
            const auto mobile=[&](std::size_t i,glm::vec3 displacement) {
                const auto& normals=stationaryNormals[i];
                for(int pass=0;pass<(normals.size()>1?4:1);++pass)for(const auto& n:normals) {
                    const float vn=glm::dot(n,displacement);
                    if(vn<0)displacement-=vn*n;
                }
                return displacement;
            };""",1)
    if variant!='mobility-velocity':
        source=source.replace('gradientSum += glm::dot(gradient, gradient);','const auto projected=mobile(j,gradient);\n                    gradientSum += glm::dot(projected,projected);')
        source=source.replace('gradientSum += glm::dot(ownGradient, ownGradient);','const auto projected=mobile(i,-ownGradient);\n                gradientSum += glm::dot(projected,projected);')
    if variant!='mobility-pressure':
        source=source.replace('                const float length = glm::length(correction);','                correction=mobile(i,correction);\n                const float length = glm::length(correction);')
source='#include <cstdio>\n#include <cstring>\n'+source
anchor='        if(impulses.size()!=velocityContacts.size())'
source=source.replace(anchor,'''        static int traceStep=0;
        if(++traceStep==6)for(std::size_t k=0;k<velocityContacts.size();++k) {
            const auto& c=velocityContacts[k];const auto& v=impulses[k];
            std::fprintf(stderr,"ROW %zu %llu x=%.8g,%.8g,%.8g n=%.8g,%.8g,%.8g v=%.8g,%.8g,%.8g j=%.8g,%.8g,%.8g\\n",c.particleIndex,
                static_cast<unsigned long long>(c.owner.id),c.particlePosition.x,c.particlePosition.y,c.particlePosition.z,
                c.normal.x,c.normal.y,c.normal.z,c.particleVelocity.x,c.particleVelocity.y,c.particleVelocity.z,v.x,v.y,v.z);
        }
        const auto digest=[](const auto& project, const auto& particles) {
            uint64_t hash=14695981039346656037ull;
            for(const auto& p:particles)for(int axis=0;axis<3;++axis) {
                uint32_t bits;const auto value=project(p)[axis];std::memcpy(&bits,&value,sizeof(bits));
                hash=(hash^bits)*1099511628211ull;
            }
            return static_cast<unsigned long long>(hash);
        };
        std::fprintf(stderr,"TICK %016llx %016llx %zu\\n",digest([&](const auto& p){return positions[&p-m_particles.data()];},m_particles),
            digest([](const auto& p){return p.velocity;},m_particles),velocityContacts.size());
'''+anchor)
p=d/(variant+'.cpp'); p.write_text(source)
o=d/(variant+'.o'); compile(p,o,'judas_engine')
link=shlex.split((b/f'CMakeFiles/{test}.dir/link.txt').read_text())
link=[str(test_obj) if arg.endswith('ProductionFluidTests.cpp.o') else arg for arg in link if not arg.startswith('-Wl,--dependency-file=')]
exe=d/variant
link[link.index('-o')+1]=str(exe)
link.insert(link.index('libjudas_engine.a'),str(o))
run(link,cwd=b)
for case in sys.argv[2:] or ['half_025']:
    with (d/f'{variant}-{case}.log').open('w') as log:
        proc=subprocess.run([exe,'--case',case,'--strict-diagnostics','--output',d/f'{variant}-{case}'],stdout=log,stderr=subprocess.STDOUT)
    print(variant,case,proc.returncode,[l for l in (d/f'{variant}-{case}.log').read_text().splitlines() if 'PRECONDITION' in l],flush=True)
