"""Pairwise XSPH exchange in the shared tangent space of active wall rows."""
from pathlib import Path
import shlex
import subprocess

work=Path(__file__).resolve().parent
root=work.parents[1]
build=root/".cache/2d-stage1-build"
source=(root/"src/FluidWorld.cpp").read_text()
a=source.index("        // Smoothing predicts the velocity BEFORE the boundary projection.")
b=source.index("        std::vector<FluidVelocityContact> velocityContacts;",a)
c=source.index("        std::vector<glm::vec3> impulses",b)
smooth=source[a:b]
smooth=smooth.replace("            const NeighborLists& neighbors", """            // Snapshot the linear tangent spaces of closing rows. The pairwise
            // momentum exchange will be projected into their intersection.
            struct Basis { std::array<glm::dvec3,3> normals{}; unsigned size=0; };
            std::vector<Basis> boundaries(count);
            const auto insert=[](Basis& basis, glm::dvec3 normal) {
                if(basis.size==3)return;
                for(unsigned k=0;k<basis.size;++k)normal-=basis.normals[k]*glm::dot(basis.normals[k],normal);
                const double norm2=glm::dot(normal,normal);
                if(norm2>64*std::numeric_limits<float>::epsilon()*std::numeric_limits<float>::epsilon())
                    basis.normals[basis.size++]=normal/std::sqrt(norm2);
            };
            for(const auto& c:velocityContacts) {
                const double speed=glm::dot(glm::dvec3(c.normal),glm::dvec3(c.particleVelocity)-glm::dvec3(c.wallVelocity));
                if(speed<c.minimumNormalVelocity)insert(boundaries[c.particleIndex],glm::dvec3(c.normal));
            }
            const NeighborLists& neighbors""",1)
old="""                    corrections[i] += (m_particles[j].mass / pairDensity) *
                        (m_particles[j].velocity - m_particles[i].velocity) *
                        Poly6(glm::dot(separation, separation), kernel);"""
new="""                    glm::vec3 relative=m_particles[j].velocity-m_particles[i].velocity;
                    if(boundaries[i].size || boundaries[j].size) {
                        // Canonical order means each pair sees the same projection
                        // regardless of which particle's row is being accumulated.
                        const auto lo=std::min(i,j),hi=std::max(i,j);
                        Basis basis=boundaries[lo];
                        for(unsigned k=0;k<boundaries[hi].size;++k)insert(basis,boundaries[hi].normals[k]);
                        glm::dvec3 tangential(relative);
                        for(unsigned k=0;k<basis.size;++k)tangential-=basis.normals[k]*glm::dot(basis.normals[k],tangential);
                        relative=glm::vec3(tangential);
                    }
                    corrections[i] += (m_particles[j].mass / pairDensity) *
                        relative * Poly6(glm::dot(separation, separation), kernel);"""
assert smooth.count(old)==1
smooth=smooth.replace(old,new)
source=source[:a]+source[b:c]+smooth+"""        for(auto& c:velocityContacts)c.particleVelocity=m_particles[c.particleIndex].velocity;
"""+source[c:]
(src:=work/"candidate.cpp").write_text(source)
flags={}
for line in (build/"CMakeFiles/judas_engine.dir/flags.make").read_text().splitlines():
    if " = " in line:
        k,v=line.split(" = ", 1)
        flags[k]=shlex.split(v)
obj=work/"candidate.o"
subprocess.run(["/usr/bin/c++",*flags["CXX_DEFINES"],*flags["CXX_INCLUDES"],*flags["CXX_FLAGS"],
                "-c",str(src),"-o",str(obj)],cwd=build,check=True)
for target, testobj, args in (
  ("judas_production_fluid_tests",None,["--case","half_025"]),
  ("judas_player_fluid_integration_tests",root/".cache/fluid-zero-settling-kernel/clean/player.o",["--smoke","--smoke-spacing",".2"]),
):
    exe=work/target
    link=[arg for arg in shlex.split((build/f"CMakeFiles/{target}.dir/link.txt").read_text())
          if not arg.startswith("-Wl,--dependency-file=")]
    link[link.index("-o")+1]=str(exe)
    if testobj:link[link.index(next(a for a in link if a.endswith("PlayerFluidIntegrationTests.cpp.o")))]=str(testobj)
    link.insert(link.index("libjudas_engine.a"),str(obj))
    subprocess.run(link,cwd=build,check=True)
    with (work/f"{target}.log").open("w") as log:
        status=subprocess.run([str(exe),*args,"--strict-diagnostics","--output",str(work/f"{target}-out")],
                              stdout=log,stderr=subprocess.STDOUT).returncode
    print(target,status,flush=True)
