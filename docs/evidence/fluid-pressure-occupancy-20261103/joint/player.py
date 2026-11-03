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

test='judas_player_fluid_integration_tests'
test_obj=d/'PlayerFluidIntegrationTests.o'
if not test_obj.exists():compile(root/'tests/PlayerFluidIntegrationTests.cpp',test_obj,test,['-I'+str(root/'tests')])
variant=sys.argv[1]
link=shlex.split((b/f'CMakeFiles/{test}.dir/link.txt').read_text())
link=[str(test_obj) if arg.endswith('PlayerFluidIntegrationTests.cpp.o') else arg for arg in link if not arg.startswith('-Wl,--dependency-file=')]
exe=d/('player-'+variant)
link[link.index('-o')+1]=str(exe)
link.insert(link.index('libjudas_engine.a'),str(d/(variant+'.o')))
run(link,cwd=b)
output=d/('player-'+variant+'-0.20')
with (d/('player-'+variant+'.log')).open('w') as log:
    proc=subprocess.run([exe,'--smoke','--smoke-spacing','.2','--strict-diagnostics','--output',output],stdout=log,stderr=subprocess.STDOUT)
    print(variant,proc.returncode,[l for l in (d/('player-'+variant+'.log')).read_text().splitlines() if 'SUMMARY' in l or 'FAIL' in l])
