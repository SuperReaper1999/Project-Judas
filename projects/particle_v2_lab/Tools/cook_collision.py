#!/usr/bin/env python3
"""Cook the two original workshop meshes from a checkout or relocated SDK."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
distribution = root.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--tool', type=Path)
args = parser.parse_args()
tool = args.tool
if tool is None:
    candidates = [distribution / 'judas_collision_cook',
                  distribution / 'judas_collision_cook.exe',
                  distribution / 'bin/judas_collision_cook',
                  distribution / 'build/judas_collision_cook']
    tool = next((path for path in candidates if path.is_file()), None)
    if tool is None:
        found = shutil.which('judas_collision_cook')
        tool = Path(found) if found else None
if tool is None or not tool.is_file():
    parser.error('Collision cooker not found; pass --tool /path/to/judas_collision_cook')
for command in json.loads((root / 'Tools/COOK_COMMANDS.json').read_text(encoding='utf-8')):
    subprocess.run([str(tool.resolve()), *command[1:]], cwd=root, check=True)
