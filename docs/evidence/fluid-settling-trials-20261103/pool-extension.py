"""Record undisturbed warmup for 20 s, with original check at 4 s."""
import pathlib
import shlex
import subprocess

work = pathlib.Path(__file__).resolve().parent
root = work.parents[1]
build = root/".cache/2d-stage1-build"
target = build/"CMakeFiles/judas_production_fluid_tests.dir"
source = (root/"tests/ProductionFluidTests.cpp").read_text()
start=source.index("  for(int step=0;step<240;++step){StepPlayedWorld(session,window,dt);++totalSteps;}")
end=source.index("  const auto placeholder=", start)
control = """  std::ofstream rest(out/(name+"-passive-warmup.csv"));
  rest<<std::setprecision(17)<<"step,mean_speed,bulk_speed,bulk_y,kinetic_energy,above_rim,mass\\n";
  for(int step=1;step<=1200;++step) {
   StepPlayedWorld(session,window,dt);
   ++totalSteps;
   double meanSpeed=0,mass=0,energy=0;
   glm::dvec3 momentum(0);
   std::size_t aboveRim=0;
   for(const auto& p:world.Fluid().Particles()) {
    const auto v=glm::dvec3(p.velocity);
    meanSpeed+=glm::length(p.velocity);
    momentum+=double(p.mass)*v;
    mass+=p.mass;
    energy+=.5*double(p.mass)*glm::dot(v,v);
    if((glm::conjugate(q)*p.position).y>3)++aboveRim;
   }
   meanSpeed/=world.Fluid().Particles().size();
   rest<<step<<','<<meanSpeed<<','<<glm::length(momentum)/mass<<','<<
      (glm::dquat(glm::conjugate(q))*momentum).y/mass<<','<<energy<<','<<aboveRim<<','<<mass<<'\\n';
   if(step==240) DiagnosticCheck(meanSpeed<.05,name+" reference pool is quiescent before body release");
  }
  return {};
"""
(src:=work/"pool-extension.cpp").write_text(source[:start]+control+source[end:])
flags={}
for line in (target/"flags.make").read_text().splitlines():
    if " = " in line:
        k,v=line.split(" = ",1)
        flags[k]=shlex.split(v)
obj=work/"pool-extension.o"
exe=work/"pool-extension"
subprocess.run(["/usr/bin/c++",*flags["CXX_DEFINES"],*flags["CXX_INCLUDES"],*flags["CXX_FLAGS"],
                "-iquote",str(root/"tests"),"-c",str(src),"-o",str(obj)],cwd=build,check=True)
link=[arg for arg in shlex.split((target/"link.txt").read_text()) if not arg.startswith("-Wl,--dependency-file=")]
link[link.index("-o")+1]=str(exe)
link[link.index(next(arg for arg in link if arg.endswith("ProductionFluidTests.cpp.o")))]=str(obj)
subprocess.run(link,cwd=build,check=True)
for case in ("half_025","zero_gravity"):
    out=work/f"{case}-20s"
    out.mkdir(exist_ok=True)
    with (out/"result.log").open("w") as log:
        status=subprocess.run([str(exe),"--case",case,"--strict-diagnostics","--output",str(out)],stdout=log,stderr=subprocess.STDOUT).returncode
    print(f"{case} passive warmup with original 4 s diagnostic, process exit {status}",flush=True)
