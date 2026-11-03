"""Passively observe vertical momentum through the original .2 m player pool."""
from pathlib import Path
import shlex
import subprocess

work=Path(__file__).resolve().parent
root=work.parents[1]
build=root/".cache/2d-stage1-build"
target=build/"CMakeFiles/judas_player_fluid_integration_tests.dir"
current=root/".cache/fluid-zero-settling-kernel/clean"
def flags(directory):
    flags={}
    for line in (directory/"flags.make").read_text().splitlines():
        if " = " in line:
            k,v=line.split(" = ", 1)
            flags[k]=shlex.split(v)
    return flags
engine_flags=flags(build/"CMakeFiles/judas_engine.dir")
for name,path in (("base",root/"src/FluidWorld.cpp"),("quartic",current/"candidate.cpp")):
    source="#include <cstdio>\n"+path.read_text()
    sources=[
        ("    for (int substep = 0; substep < substeps; ++substep) {","""    const auto momentumY=[&]() {
        double sum=0;
        for(const auto& p:m_particles)sum+=double(p.mass)*double(p.velocity.y);
        return sum;
    };
    static int observedSubsteps=0;
    for (int substep = 0; substep < substeps; ++substep) {
        const double startY=momentumY();"""),
        ("        m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);\n        for (int iteration", """        const double gravityY=momentumY();
        m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);
        for (int iteration"""),
        ("        // Smoothing predicts the velocity BEFORE the boundary projection.", """        const double pressureY=momentumY();
        // Smoothing predicts the velocity BEFORE the boundary projection."""),
        ("        std::vector<FluidVelocityContact> velocityContacts;", """        const double smoothY=momentumY();
        std::vector<FluidVelocityContact> velocityContacts;"""),
        ("        m_velocityMilliseconds+=ElapsedFluidMs(phaseStart);", """        ++observedSubsteps;
        if(observedSubsteps<=363)std::fprintf(stderr,
            "BULK %d %.17g %.17g %.17g %.17g %.17g %zu\\n",
            observedSubsteps,startY,gravityY,pressureY,smoothY,momentumY(),velocityContacts.size());
        m_velocityMilliseconds+=ElapsedFluidMs(phaseStart);"""),
    ]
    for old,new in sources:
        assert source.count(old)==1
        source=source.replace(old,new)
    (src:=work/f"bulk-trace-{name}.cpp").write_text(source)
    obj=work/f"bulk-trace-{name}.o"
    subprocess.run(["/usr/bin/c++",*engine_flags["CXX_DEFINES"],*engine_flags["CXX_INCLUDES"],*engine_flags["CXX_FLAGS"],
                    "-c",str(src),"-o",str(obj)],cwd=build,check=True)
    exe=work/f"bulk-trace-{name}"
    command=[a for a in shlex.split((target/"link.txt").read_text()) if not a.startswith("-Wl,--dependency-file=")]
    command[command.index("-o")+1]=str(exe)
    command[command.index(next(a for a in command if a.endswith("PlayerFluidIntegrationTests.cpp.o")))]=str(current/"player.o")
    command.insert(command.index("libjudas_engine.a"),str(obj))
    subprocess.run(command,cwd=build,check=True)
    out=work/f"bulk-trace-{name}-out"
    with (work/f"bulk-trace-{name}.log").open("w") as log:
        result=subprocess.run([str(exe),"--smoke","--smoke-spacing",".2","--strict-diagnostics","--output",str(out)],
                              stdout=log,stderr=subprocess.STDOUT)
    print(name,result.returncode,flush=True)
