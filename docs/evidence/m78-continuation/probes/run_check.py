#!/usr/bin/env python3
"""One immutable continuation receipt; no fixture or acceptance edits."""
import argparse, datetime, hashlib, json, os, pathlib, subprocess, time
ROOT = pathlib.Path(__file__).resolve().parents[4]
EVIDENCE = ROOT / 'docs/evidence/m78-continuation'
p = argparse.ArgumentParser()
p.add_argument('--name', required=True)
p.add_argument('--cwd', default=str(ROOT))
p.add_argument('--timeout', type=float, default=600)
p.add_argument('command', nargs=argparse.REMAINDER)
a = p.parse_args()
command = a.command[1:] if a.command[:1] == ['--'] else a.command
if not command:
    p.error('missing command')
log, receipt = EVIDENCE / (a.name+'.log'), EVIDENCE / (a.name+'.json')
if log.exists() or receipt.exists():
    raise SystemExit('Choose a new case name; previous results remain immutable')
sha = lambda path: hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
env = os.environ.copy()
keys = dict(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy',
    LIBGL_ALWAYS_SOFTWARE='1', XDG_DATA_HOME=str(ROOT/'.cache/m78-continuation/data'),
    XDG_CONFIG_HOME=str(ROOT/'.cache/m78-continuation/config'))
env.update(keys)
for key in ('JUDAS_PROFILE', 'JUDAS_PROFILE_SPIKE_MS', 'JUDAS_PROFILE_SCRIPTS'):
    if key in env:
        keys[key] = env[key]
start = time.perf_counter()
utc = datetime.datetime.now(datetime.timezone.utc).isoformat()
with log.open('wb') as output:
    try:
        code = subprocess.run(command, cwd=a.cwd, env=env, stdout=output,
            stderr=subprocess.STDOUT, timeout=a.timeout).returncode
    except subprocess.TimeoutExpired:
        code = 'timeout'
source = {str(path.relative_to(ROOT)): sha(path) for path in [
    ROOT/'src/RigidBody.cpp', ROOT/'src/RigidBody.h', ROOT/'src/RigidMotion.h',
    ROOT/'src/PhysicsWorld.cpp', ROOT/'src/CollisionGeometry.cpp']}
record = dict(command=command, cwd=a.cwd, utc=utc, seconds=time.perf_counter()-start,
    exitCode=code, environment=keys, sourceSHA256=source,
    executableSHA256=sha(command[0]) if pathlib.Path(command[0]).is_file() else None,
    logSHA256=sha(log), head=subprocess.check_output(
        ['git','rev-parse','HEAD'], cwd=ROOT, text=True).strip())
receipt.write_text(json.dumps(record, indent=2)+'\n')
print(json.dumps(dict(case=a.name, exitCode=code, seconds=record['seconds'],
    lastLines=log.read_text(errors='replace').splitlines()[-3:])))
raise SystemExit(code if isinstance(code,int) and code>=0 else 1)
