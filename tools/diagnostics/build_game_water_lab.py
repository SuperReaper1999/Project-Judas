#!/usr/bin/env python3
"""Build the opt-in game-water lab against an existing native Judas engine."""
import argparse
from pathlib import Path
import shlex
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-dir',type=Path,default=Path('build'))
p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--sanitize',action='store_true')
p.add_argument('--unit',action='store_true')
a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build_dir.resolve();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
flags={}
for line in (build/'CMakeFiles/judas.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v=line.split(' = ',1);flags[k]=shlex.split(v)
objects=[]
sources=list(sorted((root/'src/fluid').glob('GameWater*.cpp')))
if a.unit: sources += [root/'tests/GameWaterTests.cpp']
else: sources += [root/'tools/diagnostics/GameWaterLabScene.cpp',root/'tools/diagnostics/GameWaterLab.cpp']
sanitizer=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if a.sanitize else []
for source in sources:
    obj=out/(source.stem+'.o')
    subprocess.run(['c++','-std=c++17','-O1' if a.sanitize else '-O3','-g','-Wall','-Wextra','-Werror',*sanitizer,*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],'-c',str(source),'-o',str(obj)],check=True)
    objects.append(str(obj))
link_file=build/('CMakeFiles/judas.dir/link.txt' if a.unit else 'CMakeFiles/judas_game_water_lab.dir/link.txt')
if not link_file.exists(): raise SystemExit('Configure JUDAS_BUILD_GAME_WATER_LAB=ON first; use the matching CMake dependency graph.')
link=shlex.split(link_file.read_text())
exe=out/('judas_game_water_tests' if a.unit else 'judas_game_water_lab')
subprocess.run([link[0],*sanitizer,*objects,'-o',str(exe),*link[link.index('-o')+2:]],cwd=build,check=True)
print(exe)
