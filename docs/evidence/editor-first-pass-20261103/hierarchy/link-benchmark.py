import shlex
import subprocess
import sys
from pathlib import Path

build = Path(sys.argv[1]).resolve()
headers = Path(sys.argv[2]).resolve()
source = Path(sys.argv[4]).resolve() if len(sys.argv) > 4 else Path(__file__).resolve().with_name("HierarchyFrameBench.cpp")
output = Path(sys.argv[3]).resolve()
target = build / "CMakeFiles/judas_editor.dir"
flags = dict(line.split(" = ", 1) for line in (target / "flags.make").read_text().splitlines()
             if line.startswith(("CXX_DEFINES = ", "CXX_INCLUDES = ", "CXX_FLAGS = ")))
link = shlex.split((target / "link.txt").read_text())
object_file = output.with_suffix(".o")
subprocess.run([link[0], *shlex.split(flags["CXX_DEFINES"]), "-I" + str(headers),
                "-I" + str(Path.cwd() / "src/editor"), *shlex.split(flags["CXX_INCLUDES"]), *shlex.split(flags["CXX_FLAGS"]),
                "-c", str(source), "-o", str(object_file)], cwd=build, check=True)
link = [str(object_file) if arg.endswith("/EditorMain.cpp.o") else arg
        for arg in link if not arg.startswith("-Wl,--dependency-file=")]
link[link.index("-o") + 1] = str(output)
subprocess.run(link, cwd=build, check=True)
