import os,shlex,subprocess,sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
root=Path.cwd()
work=root/'.cache/fluid-zero-settling-kernel'
build=root/'.cache/2d-stage1-build'
old=root/'.cache/fluid-zero-g-followup-20261103'
def flags(target):
    result={}
    for line in (build/f"CMakeFiles/{target}.dir/flags.make").read_text().splitlines():
        if ' = ' in line:
            key,value=line.split(' = ',1)
            result[key]=shlex.split(value)
    return result
def compile(source,target,out):
    f=flags(target)
    subprocess.run(['/usr/bin/c++',*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-iquote',str(root/'tests'),'-c',str(work/source),'-o',str(work/out)],check=True)
def link(name, test_obj, physics_obj):
    l=shlex.split((build/'CMakeFiles/judas_production_fluid_tests.dir/link.txt').read_text())
    l=[a for a in l if not a.startswith('-Wl,--dependency-file=')]
    l[l.index('-o')+1]=str(work/name)
    l[l.index(next(a for a in l if a.endswith('ProductionFluidTests.cpp.o')))]=str(test_obj)
    l.insert(l.index('libjudas_engine.a'),str(work/physics_obj))
    subprocess.run(l,check=True,cwd=build)
source=(work/'base.cpp').read_text()
replace_gradient="""glm::vec3 Poly6Gradient(const glm::vec3& displacement, const FluidKernel& kernel) {
    const float remaining = kernel.radiusSquared - glm::dot(displacement,displacement);
    if (remaining <= 0) return glm::vec3(0);
    const float q=remaining/kernel.radiusSquared;
    return (-8.f * (3465.f/512.f/kPi) * q*q*q/(kernel.radiusSquared*kernel.radiusSquared*kernel.radius)) * displacement;
}
"""
source=source.replace('float DensityAt(',replace_gradient+'''
float Quartic(float squaredDistance, const FluidKernel& kernel) {
    if (squaredDistance >= kernel.radiusSquared) return 0.f;
    const float q=1.f-squaredDistance/kernel.radiusSquared;
    return (3465.f/512.f/kPi) * q*q*q*q / (kernel.radiusSquared*kernel.radius);
}
float DensityAt(''')
start=source.index('float DensityAt(')
end=source.index('BodyTransform ResolvedPose',start)
quartic=source[start:end].replace('DensityAt(', 'QuarticDensityAt(').replace('Poly6(', 'Quartic(')
source=source[:end]+quartic+source[end:]
source=source.replace('densities[i] = DensityAt(i, positions, m_particles, neighbors, kernel);','densities[i] = QuarticDensityAt(i, positions, m_particles, neighbors, kernel);',1)
source=source.replace('    const float restDensity = m_settings.restDensity;','''    const float restDensity = m_settings.restDensity;
    const bool normalize = std::getenv("JUDAS_FLUID_CALIBRATE") != nullptr;
    const bool exact = std::getenv("JUDAS_FLUID_EXACT_GRADIENT") != nullptr;
    const float constraintDensity = normalize ? restDensity * 1.0148015373569166f : restDensity;
    const auto gradientAt = [&](const glm::vec3& displacement) {
        return exact ? Poly6Gradient(displacement,kernel) : SpikyGradient(displacement,kernel);
    };''')
source=source.replace('densities[i] / restDensity - 1.0f','densities[i] / constraintDensity - 1.0f')
source=source.replace('SpikyGradient(positions[i] - positions[j], kernel)', 'gradientAt(positions[i] - positions[j])')
source=source.replace('m_particles[j].mass / restDensity','m_particles[j].mass / (exact ? constraintDensity : restDensity)')
(work/'trial.cpp').write_text('#include <cstdlib>\n'+source)
if '--run-only' not in sys.argv:
    compile('trial.cpp','judas_engine','trial.o')
    link('probe',build/'CMakeFiles/judas_production_fluid_tests.dir/tests/ProductionFluidTests.cpp.o','trial.o')
    compile('block-control.cpp','judas_production_fluid_tests','block.o')
    link('block',work/'block.o','trial.o')
cases=[('quartic-spiky','zero_gravity',False,False),('quartic-spiky','half_025',False,False),
 ('quartic-spiky','rotated_half',False,False),('quartic-spiky','common_freefall',False,False),
 ('quartic-exact','zero_gravity',False,True),('quartic-exact','half_025',False,True)]

def run(arg):
    name,fixture,cal,exact=arg
    name+='-'+fixture
    env=os.environ.copy()
    if cal: env['JUDAS_FLUID_CALIBRATE']='1'
    if exact: env['JUDAS_FLUID_EXACT_GRADIENT']='1'
    with (work/(name+'.log')).open('w') as out:
        r=subprocess.run([str(work/'probe'),'--case',fixture,'--strict-diagnostics','--output',str(work/name)],env=env,stdout=out,stderr=subprocess.STDOUT)
    print(name,r.returncode,flush=True)
with ThreadPoolExecutor(1) as pool:
    list(pool.map(run,cases))
