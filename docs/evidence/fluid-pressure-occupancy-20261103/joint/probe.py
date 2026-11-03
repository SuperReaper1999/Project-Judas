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

if variant in ('implicit','owned','normalized','discover'):
    addon=(d/'ImplicitPressure.h').read_text(); source='#include <cstdio>\n'+source
    source=source.replace('BodyTransform ResolvedPose',addon+'\nBodyTransform ResolvedPose',1)
    position_anchor='        m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);\n        for (int iteration = 0;'
    assert source.count(position_anchor)==1
    source=source.replace(position_anchor,(d/('step-sampled.txt' if variant=='implicit' else 'step-discover.txt' if variant=='discover' else 'step.txt')).read_text()+position_anchor,1)
    if variant == 'normalized':
        source=source.replace('const float restDensity = m_settings.restDensity;','const float restDensity = m_settings.restDensity*1.0148015373569166f;')
    source=source.replace('iteration < iterations;','iteration < iterations && !implicitResolved;',1)
    source=source.replace('particle.velocity += (positions[i] - unconstrainedPositions[i] - solidCorrections[i]) / substepTime;',
                          'if(!implicitResolved)particle.velocity += reconstructedCorrection / substepTime;')
    source=source.replace('if (m_settings.velocitySmoothing > 0.0f) {','if (!implicitResolved && m_settings.velocitySmoothing > 0.0f) {',1)
elif variant.startswith(('xsph','trace')):
    a=source.index('        // Smoothing predicts the velocity BEFORE the boundary projection.')
    mid=source.index('        std::vector<FluidVelocityContact> velocityContacts;',a)
    c=source.index('        std::vector<glm::vec3> impulses',mid)
    smoothing=source[a:mid]
    source='#include <cstdio>\n'+source[:a]+source[mid:c]+smoothing+source[c:]
    anchor='        if (m_settings.velocitySmoothing > 0.0f) {'
    if variant.startswith('xsph'):
        source=source.replace(anchor,anchor+r"""
            // Scratch predictor. No rigid callback is run, no tentative contact
            // impulse is published, and the final solve still owns all impulse.
            std::vector<FluidParticle> boundaryFeasible;
            if(!velocityContacts.empty()) {
                const auto stationary=[](const auto& solid) {
                    return solid.resolvedMotion.empty() &&
                        solid.previousPose.position==solid.currentPose.position &&
                        (solid.previousPose.rotation==solid.currentPose.rotation ||
                         solid.previousPose.rotation==-solid.currentPose.rotation);
                };
                const bool prescribed=!velocityResponse || (
                    std::all_of(boxes.begin(),boxes.end(),stationary) &&
                    std::all_of(spheres.begin(),spheres.end(),stationary) &&
                    std::all_of(terrains.begin(),terrains.end(),stationary));
                if(prescribed) {
                    boundaryFeasible=m_particles;
                    std::vector<glm::vec3> ignored(velocityContacts.size());
                    std::size_t caps=0;float residual=0;
                    ResolvePrescribedContacts(boundaryFeasible,velocityContacts,ignored,caps,residual);
                    if(caps)boundaryFeasible.clear();
                }
            }
            const auto& velocitySource=boundaryFeasible.empty()?m_particles:boundaryFeasible;""")
        source=source.replace('(m_particles[j].velocity - m_particles[i].velocity) *',
            '(velocitySource[j].velocity - velocitySource[i].velocity) *')
        anchor='        std::vector<glm::vec3> impulses(velocityContacts.size(),glm::vec3(0));'
        source=source.replace(anchor,'''        for(auto& contact:velocityContacts)contact.particleVelocity=m_particles[contact.particleIndex].velocity;
'''+anchor)
    if 'trace' in variant:
        anchors=['        if (m_settings.velocitySmoothing > 0.0f) {','        std::vector<glm::vec3> impulses(velocityContacts.size(),glm::vec3(0));',
                 '        if(impulses.size()!=velocityContacts.size())']
        for label,anchor in zip(['before-smooth','after-smooth','after-contact'],anchors):
            source=source.replace(anchor,r'''        if(count==2)std::fprintf(stderr,"COLUMN PHASE LABEL %.9g %.9g\n",m_particles[0].velocity.y,m_particles[1].velocity.y);
'''.replace('LABEL',label)+anchor)
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
