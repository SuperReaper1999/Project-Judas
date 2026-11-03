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

test='judas_fluid_world_tests'

compile(root/'tests/FluidWorldTests.cpp',d/'FluidWorldTests.o',test)
for variant in ['baseline','joint-2','position-2','both-2']:
    obj=d/(variant+'.o')
    link=shlex.split((b/f'CMakeFiles/{test}.dir/link.txt').read_text())
    link=[str(obj) if arg.endswith('/FluidWorld.cpp.o') else str(d/'FluidWorldTests.o') if arg.endswith('/FluidWorldTests.cpp.o') else arg for arg in link if not arg.startswith('-Wl,--dependency-file=')]
    exe=d/('fluid-world-'+variant)
    link[link.index('-o')+1]=str(exe)
    run(link,cwd=b)
    with (d/('fluid-world-'+variant+'.log')).open('w') as out:
        proc=subprocess.run([str(exe)],stdout=out)
        print(variant,proc.returncode)
