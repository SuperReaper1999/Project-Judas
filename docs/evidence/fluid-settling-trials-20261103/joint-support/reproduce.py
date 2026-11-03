from pathlib import Path
import shlex, subprocess, sys, os
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
test_obj=d/('WarmupTest.o' if '--warmup' in sys.argv else 'ProductionFluidTests.o')
if not test_obj.exists():
    source=(root/'tests/ProductionFluidTests.cpp').read_text()
    anchor='  const auto placeholder='
    assert source.count(anchor)==1
    path=d/'WarmupTest.cpp'; path.write_text(source.replace(anchor,'  return {};\n'+anchor))
    compile(path if '--warmup' in sys.argv else root/'tests/ProductionFluidTests.cpp',test_obj,test,['-I'+str(root/'tests')])
variant=sys.argv[1]
source=(root/'src/FluidWorld.cpp').read_text()
if variant == 'exact' or variant.startswith(('vel','gs','split','joint','position','both')):
    a=source.index('glm::vec3 SpikyGradient')
    end=source.index('\nfloat DensityAt',a)
    source=source[:a]+(source[a:end] if variant.startswith(('velkeep','gskeep','jointkeep')) else '')+"""glm::vec3 DensityGradient(const glm::vec3& displacement, const FluidKernel& kernel) {
    const float squaredDistance=glm::dot(displacement,displacement);
    if (squaredDistance >= kernel.radiusSquared) return glm::vec3(0.0f);
    const float remaining=kernel.radiusSquared-squaredDistance;
    return (-6.0f*kernel.poly6*remaining*remaining)*displacement;
}
"""+source[end:]
    if not variant.startswith(('velkeep','gskeep','jointkeep')):
        source=source.replace('SpikyGradient','DensityGradient')
    source=source.replace('gradientSum += glm::dot(gradient, gradient);','gradientSum += (m_particles[i].mass/m_particles[j].mass)*glm::dot(gradient, gradient);')
if variant.startswith('split'):
    source=source.replace('particle.velocity += (positions[i] - unconstrainedPositions[i] - solidCorrections[i]) / substepTime;',
                          'particle.velocity += 0.0f*reconstructedCorrection;')
if variant.startswith(('vel','gs','split')):
    count=int(variant.split('-')[1])
    anchor='        // Smoothing predicts the velocity BEFORE the boundary projection.'
    source=source.replace(anchor,"""        // Correct only an approaching density violation, using the same density
        // differential as the geometry. No effect on rigid translation or
        // expansion, and each pair correction conserves momentum.
        const auto& ratesNeighbors=BuildNeighbors(positions,radius,neighborWorkspace);
        for(std::size_t i=0;i<count;++i)
            densities[i]=DensityAt(i,positions,m_particles,ratesNeighbors,kernel);
        for(int pass=0;pass<""" +str(count)+ """;++pass) {
            for(std::size_t i=0;i<count;++i) {
                glm::vec3 ownGradient(0.0f);
                float gradientSum=0.0f,rate=0.0f;
                for(std::size_t j:ratesNeighbors[i]) {
                    const auto gradient=(m_particles[j].mass/restDensity)*
                        DensityGradient(positions[i]-positions[j],kernel);
                    ownGradient+=gradient;
                    gradientSum+=(m_particles[i].mass/m_particles[j].mass)*glm::dot(gradient,gradient);
                    rate+=glm::dot(gradient,m_particles[i].velocity-m_particles[j].velocity);
                }
                const float margin=std::max(1.0f-densities[i]/restDensity,0.0f);
                const float approaching=std::max(rate-margin/substepTime,0.0f);
                lambdas[i]=-approaching/(gradientSum+glm::dot(ownGradient,ownGradient)+1e-5f);
            }
            for(std::size_t i=0;i<count;++i) {
                corrections[i]=glm::vec3(0.0f);
                for(std::size_t j:ratesNeighbors[i])
                    corrections[i]+=(lambdas[i]+lambdas[j])*(m_particles[j].mass/restDensity)*
                        DensityGradient(positions[i]-positions[j],kernel);
            }
            for(std::size_t i=0;i<count;++i)m_particles[i].velocity+=corrections[i];
        }
""" +anchor)
if variant.startswith(('gs','split')):
    a=source.index('                lambdas[i]=-approaching/')
    z=source.index('        // Smoothing predicts',a)
    if variant.startswith('split'):
        source=source.replace('const float margin=std::max(1.0f-densities[i]/restDensity,0.0f);',
                              'const float margin=1.0f-densities[i]/restDensity;')
        a=source.index('                lambdas[i]=-approaching/')
        z=source.index('        // Smoothing predicts',a)
    source=source[:a]+'''                const float lambda=-approaching/(gradientSum+glm::dot(ownGradient,ownGradient)+1e-5f);
                m_particles[i].velocity+=lambda*ownGradient;
                for(std::size_t j:ratesNeighbors[i]) {
                    const auto gradient=(m_particles[i].mass/restDensity)*
                        DensityGradient(positions[i]-positions[j],kernel);
                    m_particles[j].velocity-=lambda*gradient;
                }
            }
        }
'''+source[z:]
if variant.startswith(('joint','both')):
    count=int(variant.split('-')[1])
    anchor='        std::vector<glm::vec3> impulses(velocityContacts.size(),glm::vec3(0));'
    assert source.count(anchor)==1
    code=r'''
        // Trial: same nonnegative pressure and contact multipliers can add
        // and release velocity as the other rows change. Freeze endpoint
        // geometry, keep existing finite-feature admission, and avoid trying
        // to approximate unsupported moving-body mobility.
        const bool prescribed=std::all_of(velocityContacts.begin(),velocityContacts.end(),
            [](const auto& contact){return contact.wallVelocity==glm::vec3(0);});
        if(prescribed) {
            const auto& rateNeighbors=BuildNeighbors(positions,radius,neighborWorkspace);
            std::vector<float> inverseDiagonals(count), maximumRates(count), pressure(count,0), wallPressure(velocityContacts.size(),0);
            std::vector<glm::vec3> ownGradients(count), weightedGradients;
            weightedGradients.reserve(rateNeighbors.indices.size());
            for(std::size_t i=0;i<count;++i) {
                glm::vec3 own(0.0f);
                float diagonal=0;
                for(std::size_t j:rateNeighbors[i]) {
                    const auto gradient=(m_particles[j].mass/restDensity)*
                        DensityGradient(positions[i]-positions[j],kernel);
                    weightedGradients.push_back(gradient);
                    own+=gradient;
                    diagonal+=(m_particles[i].mass/m_particles[j].mass)*glm::dot(gradient,gradient);
                }
                ownGradients[i]=own;
                diagonal+=glm::dot(own,own);
                inverseDiagonals[i]=diagonal>0?1/diagonal:0;
                maximumRates[i]=std::max(1.0f-DensityAt(i,positions,m_particles,rateNeighbors,kernel)/restDensity,0.0f)/substepTime;
            }
            for(int pass=0;pass<PASSCOUNT;++pass) {
                for(std::size_t r=0;r<velocityContacts.size();++r) {
                    const auto& c=velocityContacts[r];auto& v=m_particles[c.particleIndex].velocity;
                    const float next=std::max(0.f,wallPressure[r]+c.minimumNormalVelocity-glm::dot(v,c.normal));
                    v+=(next-wallPressure[r])*c.normal;
                    wallPressure[r]=next;
                }
                for(std::size_t i=0;i<count;++i) {
                    auto& p=m_particles[i];
                    float rate=0;
                    std::size_t edge=rateNeighbors.offsets[i];
                    for(std::size_t j:rateNeighbors[i])rate+=glm::dot(weightedGradients[edge++],p.velocity-m_particles[j].velocity);
                    const float next=std::max(0.0f,pressure[i]+(rate-maximumRates[i])*inverseDiagonals[i]);
                    const float change=next-pressure[i];
                    if(change==0)continue;
                    pressure[i]=next;
                    p.velocity-=change*ownGradients[i];
                    edge=rateNeighbors.offsets[i];
                    for(std::size_t j:rateNeighbors[i])
                        m_particles[j].velocity+=(change*p.mass/m_particles[j].mass)*weightedGradients[edge++];
                }
            }
            if(contactImpulses)for(std::size_t r=0;r<velocityContacts.size();++r) {
                const auto& c=velocityContacts[r];
                if(c.owner.IsValid() && wallPressure[r]>0)
                    contactImpulses->push_back({c.owner,c.point,(-c.particleMass*wallPressure[r])*c.normal,c.particleIndex});
            }
        }
'''
    if variant.startswith('jointmob'):
        code=code.replace('            for(int pass=0;',r"""
            std::vector<std::vector<std::size_t>> supportIndices(count);
            for(std::size_t r=0;r<velocityContacts.size();++r)
                supportIndices[velocityContacts[r].particleIndex].push_back(r);
            const auto admit=[&](std::size_t r) {
                const auto& c=velocityContacts[r];
                const float vn=glm::dot(m_particles[c.particleIndex].velocity,c.normal);
                const float next=std::max(0.f,wallPressure[r]+c.minimumNormalVelocity-vn);
                m_particles[c.particleIndex].velocity+=(next-wallPressure[r])*c.normal;
                wallPressure[r]=next;
            };
            const auto mobility=[&](std::size_t k,glm::vec3 direction) {
                for(std::size_t r:supportIndices[k]) {
                    const auto& c=velocityContacts[r];
                    const float vn=glm::dot(m_particles[k].velocity,c.normal);
                    const float delta=glm::dot(direction,c.normal);
                    if(delta<0 && vn<=c.minimumNormalVelocity+std::numeric_limits<float>::epsilon())
                        direction-=delta*c.normal;
                }
                return direction;
            };
            for(int pass=0;""")
        a=code.index('                for(std::size_t r=0;')
        z=code.index('                for(std::size_t i=0;',a)
        code=code[:a]+'''                for(std::size_t r=0;r<velocityContacts.size();++r)admit(r);
'''+code[z:]
        code=code.replace('                    const float next=std::max(0.0f,pressure[i]+(rate-maximumRates[i])*inverseDiagonals[i]);',r"""
                    const float residual=rate-maximumRates[i];
                    float inverse=inverseDiagonals[i];
                    if(residual>0) {
                        const auto& own=ownGradients[i];
                        float diagonal=-glm::dot(own,mobility(i,-own));
                        edge=rateNeighbors.offsets[i];
                        for(std::size_t j:rateNeighbors[i]) {
                            const auto& gradient=weightedGradients[edge++];
                            diagonal+=(p.mass/m_particles[j].mass)*glm::dot(gradient,mobility(j,gradient));
                        }
                        if(diagonal>0)inverse=1.f/diagonal;
                    }
                    const float next=std::max(0.0f,pressure[i]+residual*inverse);""")
        code=code.replace('                    for(std::size_t j:rateNeighbors[i])\n                        m_particles[j].velocity+=(change*p.mass/m_particles[j].mass)*weightedGradients[edge++];',
'''                    for(auto r:supportIndices[i])admit(r);
                    for(std::size_t j:rateNeighbors[i]) {
                        m_particles[j].velocity+=(change*p.mass/m_particles[j].mass)*weightedGradients[edge++];
                        for(auto r:supportIndices[j])admit(r);
                    }''')
    source=source.replace(anchor,code.replace('PASSCOUNT',str(count))+anchor)
if variant.startswith(('position','both')):
    passes=int(variant.split('-')[1])
    begin=source.index('            for (std::size_t i = 0; i < count; ++i) {\n                densities[i] = DensityAt')
    end=source.index('            m_densityMilliseconds',begin)
    source=source[:begin]+r"""
            // Frozen tangent system for this nonlinear density iteration.
            // Density multipliers can release, and shares the mobility of
            // finite preferred supports at this geometry.
            std::vector<glm::vec3> ownGradients(count), weightedGradients;
            std::vector<glm::vec3> boundaryCorrections(count);
            weightedGradients.reserve(neighbors.indices.size());
            std::vector<float> inverseDiagonals(count), required(count), multiplier(count);
            struct PositionSupport {std::size_t index;glm::vec3 normal;float multiplier=0;};
            std::vector<PositionSupport> supports;
            std::fill(corrections.begin(),corrections.end(),glm::vec3(0));
            for(std::size_t i=0;i<count;++i) {
                glm::vec3 own(0);
                float diagonal=0;
                for(std::size_t j:neighbors[i]) {
                    const auto gradient=(m_particles[j].mass/restDensity)*
                        DensityGradient(positions[i]-positions[j],kernel);
                    weightedGradients.push_back(gradient);
                    own+=gradient;
                    diagonal+=(m_particles[i].mass/m_particles[j].mass)*glm::dot(gradient,gradient);
                }
                ownGradients[i]=own;
                diagonal+=glm::dot(own,own);
                inverseDiagonals[i]=diagonal>0?1/diagonal:0;
                required[i]=1.0f-DensityAt(i,positions,m_particles,neighbors,kernel)/restDensity;
                if(contacts[i].empty())continue;
                const auto group=ClassifyContacts(contacts[i],positions[i],m_settings.particleRadius,substepTime);
                if(group.yielding)continue;
                for(std::size_t r=0;r<contacts[i].size();++r) {
                    if(!group.rows[r].preferredFeasible)continue;
                    const auto& c=PreferredContact(contacts[i][r],group.rows[r]);
                    if(c.wallVelocity==glm::vec3(0))supports.push_back({i,c.normal});
                }
            }
            for(int pass=0;pass<PASSCOUNT;++pass) {
                for(std::size_t i=0;i<count;++i) {
                    float rate=0;
                    std::size_t edge=neighbors.offsets[i];
                    for(std::size_t j:neighbors[i])rate+=glm::dot(weightedGradients[edge++],corrections[i]-corrections[j]);
                    const float next=std::max(0.f,multiplier[i]+(rate-required[i])*inverseDiagonals[i]);
                    const float change=next-multiplier[i];
                    if(change==0)continue;
                    multiplier[i]=next;
                    corrections[i]-=change*ownGradients[i];
                    edge=neighbors.offsets[i];
                    for(std::size_t j:neighbors[i])
                        corrections[j]+=(change*m_particles[i].mass/m_particles[j].mass)*weightedGradients[edge++];
                }
                for(auto& row:supports) {
                    const float next=std::max(0.f,row.multiplier-glm::dot(corrections[row.index],row.normal));
                    const auto change=(next-row.multiplier)*row.normal;
                    corrections[row.index]+=change;
                    boundaryCorrections[row.index]+=change;
                    row.multiplier=next;
                }
            }
            // Preserve a single common step length when the authored limiter
            // is needed, keeping pair momentum and homogeneous wall mobility.
            float fraction=1;
            for(const auto& correction:corrections) {
                const float length=glm::length(correction);
                if(length>m_settings.maxDensityCorrection && length>kEpsilon)
                    fraction=std::min(fraction,m_settings.maxDensityCorrection/length);
            }
            for(std::size_t i=0;i<count;++i) {
                const glm::dvec3 before(positions[i]);
                positions[i]+=fraction*corrections[i];
                const auto solid=fraction*boundaryCorrections[i];
                solidCorrections[i]+=solid;
                representedPressureCorrections[i]+=glm::dvec3(positions[i])-before-glm::dvec3(solid);
            }
""".replace("PASSCOUNT",str(passes))+source[end:]
p=d/(variant+'.cpp');
if not p.exists() or p.read_text()!=source:p.write_text(source)
o=d/(variant+'.o'); compile(p,o,'judas_engine') if not o.exists() or o.stat().st_mtime < p.stat().st_mtime else None
link=shlex.split((b/f'CMakeFiles/{test}.dir/link.txt').read_text())
link=[str(test_obj) if arg.endswith('ProductionFluidTests.cpp.o') else arg for arg in link if not arg.startswith('-Wl,--dependency-file=')]
exe=d/variant
link[link.index('-o')+1]=str(exe)
link.insert(link.index('libjudas_engine.a'),str(o))
run(link,cwd=b)
for case in [arg for arg in sys.argv[2:] if arg!='--warmup'] or ['half_025']:
    with (d/(f'{variant}-{case}'+('-warmup.log' if '--warmup' in sys.argv else '-full.log'))).open('w') as log:
        proc=subprocess.run([exe,'--case',case,'--strict-diagnostics','--output',d/(f'{variant}-{case}'+('-warmup' if '--warmup' in sys.argv else '-full'))],stdout=log,stderr=subprocess.STDOUT)
    print(variant,case,proc.returncode,[l for l in (d/(f'{variant}-{case}'+('-warmup.log' if '--warmup' in sys.argv else '-full.log'))).read_text().splitlines() if 'PRECONDITION' in l],flush=True)
