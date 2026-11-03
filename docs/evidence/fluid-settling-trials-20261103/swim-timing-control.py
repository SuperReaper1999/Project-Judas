"""Independent gameplay control: request the jump on actual water entry."""
from pathlib import Path
import shlex
import subprocess

work=Path(__file__).resolve().parent
root=work.parents[1]
build=root/".cache/2d-stage1-build"
target=build/"CMakeFiles/judas_player_fluid_integration_tests.dir"
source=(root/"tests/PlayerFluidIntegrationTests.cpp").read_text()
source=source.replace("bool compared=false;", "bool compared=false,jumped=false;")
source=source.replace("if(swim){p.window.SetTestActionState", "const bool jumpNow=swim&&!jumped&&playerWasWet(p);if(swim){p.window.SetTestActionState")
source=source.replace("if(i==120)p.window.RequestTestJump();", "if(jumpNow){p.window.RequestTestJump();jumped=true;std::cout<<\"JUMP_ON_ENTRY \"<<r.name<<' '<<i<<'\\n';}")
source=source.replace("if(swim&&i==120)", "if(jumpNow)")
source=source.replace("if(swim){Check(entered", "if(swim){Check(jumped,r.name+\" water entry was observed and jump was requested\");Check(entered")
source=source.replace("Result Run(", "bool playerWasWet(const Played& p){return p.session.Player().GetFluidSample().immersion>.2f;}\nResult Run(")
start=source.index("if(!fallOnly)for(float spacing:")
end=source.index("std::ostringstream out;", start)
source=source[:start]+"""for(float spacing:{.2f,.25f})
 results.push_back(Run(output,"swim-through",identity,spacing,950,false,0,{0,1.5f,4},300,true));
"""+source[end:]
assert source.count("if(jumpNow)") == 2
(src:=work/"swim-timing-control.cpp").write_text(source)
flags={}
for line in (target/"flags.make").read_text().splitlines():
    if " = " in line:
        k,v=line.split(" = ", 1)
        flags[k]=shlex.split(v)
obj=work/"swim-timing-control.o"
subprocess.run(["/usr/bin/c++",*flags["CXX_DEFINES"],*flags["CXX_INCLUDES"],*flags["CXX_FLAGS"],
                "-iquote",str(root/"tests"),"-c",str(src),"-o",str(obj)],cwd=build,check=True)
for name in ("baseline","quartic"):
    exe=work/f"swim-timing-{name}"
    link=[a for a in shlex.split((target/"link.txt").read_text()) if not a.startswith("-Wl,--dependency-file=")]
    link[link.index("-o")+1]=str(exe)
    link[link.index(next(a for a in link if a.endswith("PlayerFluidIntegrationTests.cpp.o")))]=str(obj)
    if name=="quartic":
        link.insert(link.index("libjudas_engine.a"),str(root/".cache/fluid-zero-settling-kernel/clean/candidate.o"))
    subprocess.run(link,cwd=build,check=True)
    out=work/f"swim-on-entry-{name}"
    out.mkdir(exist_ok=True)
    with (out/"result.log").open("w") as log:
        result=subprocess.run([str(exe),"--output",str(out/"output")],stdout=log,stderr=subprocess.STDOUT)
    print(name,result.returncode,flush=True)
