#!/usr/bin/env python3
"""Export the ordinary game, retaining native provenance and adding player-facing help."""
import argparse
import pathlib
import shutil
import subprocess

project = pathlib.Path(__file__).resolve().parents[1]
repo = project.parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=pathlib.Path, default=repo / 'build')
p.add_argument('--output', required=True, type=pathlib.Path)
a = p.parse_args()
output = a.output.resolve()
if output.exists():
    p.error('Choose a new output directory; existing packages are never overwritten.')
subprocess.run([str(a.build.resolve() / 'judas_export'), str(project / 'lunar_island.judasproj'), str(output), str(a.build.resolve() / 'judas')], check=True)
shutil.copy2(project / 'PLAYER_GUIDE.md', output / 'PLAYER_GUIDE.md')
launcher = output / 'Run Lunar Island.sh'
launcher.write_text('#!/bin/sh\nset -eu\ncd -- "$(dirname -- "$0")"\nexec ./judas "$@"\n')
launcher.chmod(0o755)
print('Playable package:', output)
