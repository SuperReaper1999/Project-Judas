import shlex,subprocess,sys
from pathlib import Path
root=Path.cwd()
work=root/'.cache/fluid-zero-settling-kernel/clean'
build=root/'.cache/2d-stage1-build'
def flags(target):
    result={}
    for line in (build/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key,value=line.split(' = ',1)
            result[key]=shlex.split(value)
    return result
def compile(source,target,out):
    f=flags(target)
    subprocess.run(['/usr/bin/c++',*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-iquote',str(root/'tests'),'-c',str(source),'-o',str(out)],check=True)
def link(target,executable,test_obj=None,physics_obj=None):
    l=shlex.split((build/f'CMakeFiles/{target}.dir/link.txt').read_text())
    l=[a for a in l if not a.startswith('-Wl,--dependency-file=')]
    l[l.index('-o')+1]=str(work/executable)
    if test_obj:
        l[l.index(next(a for a in l if a.startswith('CMakeFiles/'+target+'.dir/tests/') and a.endswith('.o')))]=str(test_obj)
    if 'libjudas_engine.a' in l:
        index=l.index('libjudas_engine.a')
        l[index]=str(work/'engine.a')
        if physics_obj:
            l.insert(index,str(physics_obj))
    elif physics_obj:
        l[l.index(next(a for a in l if a.endswith('FluidWorld.cpp.o')))]=str(physics_obj)
    subprocess.run(l,check=True,cwd=build)
if __name__=='__main__':
    source=(work.parent/'base.cpp').read_text()
    source=source.replace('float DensityAt(','''// The normalized compact quartic has much smaller quadrature bias than
// Poly6 at the standard seeded particle spacing. Retain the existing Spiky
// projection direction and keep the smoothing weights independent.
float PressureDensityWeight(float squaredDistance, const FluidKernel& kernel) {
    if (squaredDistance >= kernel.radiusSquared) return 0.0f;
    const float q = 1.0f - squaredDistance / kernel.radiusSquared;
    return (3465.0f / 512.0f / kPi) * q * q * q * q / (kernel.radiusSquared * kernel.radius);
}

template<class Weight>
float DensityAt(''')
    source=source.replace('const NeighborLists& neighbors, const FluidKernel& kernel) {\n    float density = particles[i].mass * Poly6(0.0f, kernel);','const NeighborLists& neighbors, const FluidKernel& kernel, Weight weight) {\n    float density = particles[i].mass * weight(0.0f, kernel);')
    source=source.replace('density += particles[j].mass * Poly6(', 'density += particles[j].mass * weight(')
    old='DensityAt(i, positions, m_particles, neighbors, kernel)'
    assert source.count(old)==2
    source=source.replace(old,'DensityAt(i, positions, m_particles, neighbors, kernel, PressureDensityWeight)',1)
    source=source.replace(old,'DensityAt(i, positions, m_particles, neighbors, kernel, Poly6)',1)
    source=source.replace('DensityAt(i, positions, m_particles, finalNeighbors, kernel)','DensityAt(i, positions, m_particles, finalNeighbors, kernel, PressureDensityWeight)')
    (work/'candidate.cpp').write_text(source)
    compile(work/'candidate.cpp','judas_engine',work/'candidate.o')
    link('judas_production_fluid_tests','production',physics_obj=work/'candidate.o')
    if 'player' in sys.argv:
        target='judas_player_fluid_integration_tests'
        compile(root/'tests/PlayerFluidIntegrationTests.cpp',target,work/'player.o')
        link(target,'player',work/'player.o',work/'candidate.o')
