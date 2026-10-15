#!/usr/bin/env python3
"""Build ONE isolated PhysicsWorld TU and live observer; shared archive untouched."""
import shlex,subprocess
from pathlib import Path
ROOT=Path('/home/conner/Documents/GitHub/Project-Judas');OUT=Path(__file__).resolve().parent
flags={}
for line in (ROOT/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  key,value=line.split(' = ',1);flags[key]=shlex.split(value)
command=['/usr/bin/c++',*flags.get('CXX_DEFINES',[]),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(OUT/'PhysicsWorld.cpp'),'-o',str(OUT/'PhysicsWorld.cpp.o')]
print(shlex.join(command),flush=True);subprocess.run(command,cwd=ROOT/'build',check=True)
# Existing observer and application objects are reused. Supplying the isolated
# TU before libjudas_engine.a means its original PhysicsWorld member is not
# extracted, and no shared archive/binary/build object is replaced.
original=(Path('/tmp/m87-live-diagnostics/build.sh').read_text().splitlines())[-1]
link=shlex.split(original)
link[link.index('-o')+1]=str(OUT/'live')
link.insert(link.index('libjudas_engine.a'),str(OUT/'PhysicsWorld.cpp.o'))
print(shlex.join(link),flush=True);subprocess.run(link,cwd=ROOT/'build',check=True)
