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
compile(d/'column.cpp',d/'column.o',test)
for variant in ['trace','xsphtrace','discover']:
    obj=d/(variant+'.o')
    link=shlex.split((b/f'CMakeFiles/{test}.dir/link.txt').read_text())
    link=[str(d/'column.o') if arg.endswith('FluidWorldTests.cpp.o') else str(obj) if arg.endswith('/FluidWorld.cpp.o') and variant!='baseline' else arg for arg in link if not arg.startswith('-Wl,--dependency-file=')]
    exe=d/('column-'+variant)
    link[link.index('-o')+1]=str(exe)
    run(link,cwd=b)
    with (d/('column-'+variant+'.csv')).open('w') as out:
        run([str(exe)],stdout=out)
