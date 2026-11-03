"""Alternate dual solvers under the same prototype pressure/support equations."""
from pathlib import Path
import shlex
import subprocess
import sys

work=Path(__file__).resolve().parent
root=work.parents[1]
build=root/".cache/2d-stage1-build"
target="judas_production_fluid_tests"
def flags(target):
    flags={}
    for line in (build/f"CMakeFiles/{target}.dir/flags.make").read_text().splitlines():
        if " = " in line:
            k,v=line.split(" = ", 1)
            flags[k]=shlex.split(v)
    return flags
def compile(path,obj,target):
    f=flags(target)
    subprocess.run(["/usr/bin/c++",*f["CXX_DEFINES"],*f["CXX_INCLUDES"],*f["CXX_FLAGS"],
                    "-iquote",str(root/"tests"),"-c",str(path),"-o",str(obj)],cwd=build,check=True)
source="#include <cstdio>\n"+(root/"src/FluidWorld.cpp").read_text()
addon=(work/"ImplicitPressure.h").read_text()
name=sys.argv[1]
if name=="cg64":
    a=addon.index("    for(;result.sweeps<maxSweeps")
    b=addon.index("    for(std::size_t i=0;i<count;++i)result.velocities",a)
    addon=addon[:a]+(work/"cg.txt").read_text()+addon[b:]
elif name=="pgs512":
    addon=addon.replace("maxSweeps=64","maxSweeps=512")
elif name.startswith(("discover","moving")):
    pass
elif name!="pgs64":
    raise ValueError(name)
step=(work/"step.txt").read_text()
if name.startswith(("discover","moving")):
    step=(work/"step-discover.txt").read_text()
if name=="discover-warm":
    addon=addon.replace("float restDensity,float step,float smoothing) {",
                        "float restDensity,float step,float smoothing,std::vector<double>& lambda) {")
    addon=addon.replace("    std::vector<double> lambda(rows.size());", """    lambda.resize(rows.size());
    for(std::size_t j=0;j<rows.size();++j) {
        if(lambda[j]==0)continue;
        for(std::size_t e=rows[j].begin;e<rows[j].end;++e)
            solution[coefficients[e].particle]-=lambda[j]*coefficients[e].gradient;
    }""")
    step=step.replace("PressureProjectionResult solution;", "PressureProjectionResult solution;\n            std::vector<double> lambda;")
    step=step.replace("substepTime,m_settings.velocitySmoothing);",
                      "substepTime,m_settings.velocitySmoothing,lambda);")
if name=="moving-prescribed":
    start=step.index("        // This stage")
    finish=step.index("            struct SupportIdentity",start)
    step=step[:start]+"""        // Cavity-free exterior fixture only. Every sampled box trajectory is
        // prescribed during this liquid substep, even if its owner is moving.
        if(iterations>0 && spheres.empty() && terrains.empty()) {
"""+step[finish:]
    before="""                        const float gap=glm::dot(starts[i]-positions[i],c.normal);
                        const float target=std::min(0.f,-gap/substepTime);"""
    after="""                        const glm::vec3 normal=c.normal;
                        const float boundary=glm::dot(c.point+m_settings.particleRadius*normal-starts[i],normal)/substepTime;
                        // Static skin overlap retains the original zero-speed target.
                        // The moving face uses its actual sampled end plane instead.
                        const float target=c.wallVelocity==glm::vec3(0) ? std::min(0.f,boundary) : boundary;"""
    assert step.count(before)==1
    step=step.replace(before,after)
source=source.replace("BodyTransform ResolvedPose",addon+"\nBodyTransform ResolvedPose",1)
anchor="        m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);\n        for (int iteration = 0;"
assert source.count(anchor)==1
source=source.replace(anchor,step+anchor,1)
source=source.replace("iteration < iterations;","iteration < iterations && !implicitResolved;",1)
source=source.replace("particle.velocity += (positions[i] - unconstrainedPositions[i] - solidCorrections[i]) / substepTime;",
                      "if(!implicitResolved)particle.velocity += reconstructedCorrection / substepTime;")
source=source.replace("if (m_settings.velocitySmoothing > 0.0f) {","if (!implicitResolved && m_settings.velocitySmoothing > 0.0f) {",1)
(src:=work/f"{name}.cpp").write_text(source)
obj=work/f"{name}.o"
compile(src,obj,"judas_engine")
exe=work/name
link=[a for a in shlex.split((build/f"CMakeFiles/{target}.dir/link.txt").read_text()) if not a.startswith("-Wl,--dependency-file=")]
link[link.index("-o")+1]=str(exe)
if "--full" not in sys.argv:
    link[link.index(next(a for a in link if a.endswith("ProductionFluidTests.cpp.o")))]=str(work/"WarmupTest.o")
link.insert(link.index("libjudas_engine.a"),str(obj))
subprocess.run(link,cwd=build,check=True)
for case in (case for case in sys.argv[2:] if case!="--full"):
    out=work/f"{name}-{case}"
    out.mkdir(exist_ok=True)
    with (out/"warmup.log").open("w") as log:
        r=subprocess.run([str(exe),"--case",case,"--strict-diagnostics","--output",str(out)],stdout=log,stderr=subprocess.STDOUT)
    print(name,case,r.returncode,flush=True)
