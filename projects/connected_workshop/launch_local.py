#!/usr/bin/env python3
"""Launch two independent ordinary runtimes; Ctrl-C closes only these children."""
import argparse
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
here = Path(__file__).resolve().parent
default_runtime = here / 'judas' if (here / 'judas').exists() else here.parents[1] / 'build/judas'
p.add_argument('--runtime', type=Path, default=default_runtime)
p.add_argument('--port', type=int, default=27999)
a = p.parse_args()
if not 1 <= a.port <= 65535:
    p.error('port must be 1..65535')
project = here / 'connected_workshop.judasproj'
if not project.is_file():
    project = here / 'game.judasproj'  # ordinary standalone export filename
if not project.is_file():
    p.error('Missing connected_workshop.judasproj or exported game.judasproj beside launcher')
children = []
try:
    children.append(subprocess.Popen([str(a.runtime.resolve()), str(project), '--host', str(a.port)]))
    time.sleep(1)
    children.append(subprocess.Popen([str(a.runtime.resolve()), str(project), '--join', f'127.0.0.1:{a.port}']))
    while any(c.poll() is None for c in children):
        time.sleep(.2)
except KeyboardInterrupt:
    pass
finally:
    for c in children:
        if c.poll() is None:
            c.terminate()
    for c in children:
        try:
            c.wait(timeout=3)
        except subprocess.TimeoutExpired:
            c.kill()
            c.wait()
