#!/usr/bin/env python3
"""Build the bounded, off-line replay against the exact original engine."""
from pathlib import Path
import hashlib
import json
import shlex
import subprocess
import sys
OWNED = Path(__file__).resolve().parent
ROOT = OWNED.parents[2]
BUILD = ROOT/'build'
BASE = OWNED.parent/'baseline-e30/libjudas_engine.a'
flags = {}
for line in (BUILD/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1); flags[key] = shlex.split(value)
stem = sys.argv[1] if len(sys.argv)>1 else 'Replay'
if stem not in ['Replay','SchurReplay','AndersonReplay']: raise SystemExit('unexpected replay stem')
obj = OWNED/(stem+'.cpp.o')
compile = ['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'],
           '-I'+str(ROOT), '-I'+str(OWNED), '-c', str(OWNED/(stem+'.cpp')), '-o', str(obj)]
subprocess.run(compile, cwd=ROOT, check=True)
link = shlex.split((BUILD/'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text())
link = [x for x in link if not x.startswith('-Wl,--dependency-file=') and not x.endswith('tests/ValidationMain.cpp.o')]
link = [str(obj) if x.endswith('tests/ImpactSolverTests.cpp.o') else x for x in link]
link = [str(BASE) if x == 'libjudas_engine.a' else x for x in link]
binary = OWNED/({'Replay':'replay','SchurReplay':'schur-replay','AndersonReplay':'anderson-replay'}[stem])
link[link.index('-o')+1] = str(binary)
subprocess.run(link, cwd=BUILD, check=True)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
(OWNED/(stem.upper()+'_BUILD.json')).write_text(json.dumps({'compile': compile, 'link': link,
    'baseline_archive_sha256': sha(BASE), 'binary_sha256': sha(binary),
    'source_sha256': sha(OWNED/(stem+'.cpp')), 'Replay.cpp_sha256': sha(OWNED/'Replay.cpp'),
    'ContactCapture.h_sha256': sha(OWNED/'ContactCapture.h'),
    'NormalSchur.h_sha256': sha(OWNED/'NormalSchur.h')}, indent=2)+'\n')
print(binary)
