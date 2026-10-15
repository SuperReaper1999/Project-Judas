from pathlib import Path
import json,subprocess,shlex,hashlib
p=Path('/tmp/m87-impact-quality');root=Path('/home/conner/Documents/GitHub/Project-Judas')
commands=json.loads((p/'build-capture-commands.json').read_text())
compile=commands[0][:];compile[-3]=str(p/'replay.cpp');compile[-1]=str(p/'replay.o')
link=commands[-1][:];link=[x.replace('live-capture.o','replay.o').replace('live-capture','replay') for x in link]
# Application/Runtime test harness objects are unnecessary for isolated solver.
link=[x for x in link if not x.startswith('CMakeFiles/judas.dir/')]
for command in [compile,link]:
 print(shlex.join(command),flush=True);subprocess.run(command,cwd=root/'build',check=True)
files=[p/'FrozenImpact.h',p/'ImpactSolver-capture.cpp',p/'ImpactSolver-capture.o',p/'replay.cpp',p/'replay.o',p/'replay',p/'live-capture',root/'build/libjudas_engine.a']
(p/'replay-fingerprints.json').write_text(json.dumps({str(f):hashlib.sha256(f.read_bytes()).hexdigest()for f in files},indent=2)+'\n')
