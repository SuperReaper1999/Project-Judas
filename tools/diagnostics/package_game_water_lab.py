#!/usr/bin/env python3
"""Create a local, relocatable source-preview lab, not a Judas release or SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import uuid

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--executable', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
repo = Path(__file__).resolve().parents[2]
exe = a.executable.resolve()
out = a.output.resolve()
if out.exists():
    raise SystemExit('Output already exists; choose a new directory. No files were replaced.')
identity = json.loads(subprocess.check_output([str(exe), '--build-info-json'], text=True))
out.parent.mkdir(parents=True, exist_ok=True)
stage = out.with_name(out.name + '.stage-' + uuid.uuid4().hex)
stage.mkdir()
try:
    shutil.copy2(exe, stage / 'judas_game_water_lab')
    shutil.copytree(repo / 'assets/fonts', stage / 'assets/fonts')
    for name in ('LICENSE', 'LICENSE.md', 'THIRD_PARTY_NOTICES.md', 'THIRD_PARTY_NOTICES.txt'):
        if (repo / name).is_file():
            shutil.copy2(repo / name, stage / name)
    (stage / 'play.sh').write_text('''#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd -- "$root"
export JUDAS_ENGINE_ROOT="$root"
export JUDAS_GPU_DIAGNOSTICS=${JUDAS_GPU_DIAGNOSTICS:-1}
if [[ $# == 1 ]]; then
    case "$1" in
        --version|--build-info|--build-info-json)
            exec "$root/judas_game_water_lab" "$@" ;;
    esac
fi
exec "$root/judas_game_water_lab" --save-dir "$root/Saves" "$@"
''')
    (stage / 'play.sh').chmod(0o755)
    (stage / 'README.txt').write_text('''Judas game-water source preview — not a published Alpha or SDK.

Run ./play.sh. Requires the current Linux desktop's SDL2/OpenGL runtime.
This is a relocated native-lab package, not clean-machine certification.

1 filled pool; 2 sluice; 3 trapped-air chamber; 4 authored terrain;
5 original basin; X rotated gravity; R reset; P pause.
WASD/mouse fly; Space/Ctrl vertical; B throw a real box; G gate/vent.
F5 save; F9 load; Tab cursor; F12 screenshot. Saves stay in this folder.

Advanced Fluids V2 requires explicit project opt-in in ordinary games. This
separate native preview uses the C++ grid, real Judas MK2 and Renderer V2. This package is the native diagnostic only. The normal editable project is
projects/water_v2_workshop; use judas_export for that game. See current scene,
JudasJS, shape, gravity and presentation support in the guide. Source and detailed limitations:
docs/GAME_WATER_PREVIEW.md in Project-Judas.
''')
    files = {str(f.relative_to(stage)): hashlib.sha256(f.read_bytes()).hexdigest()
             for f in sorted(stage.rglob('*')) if f.is_file()}
    manifest = {'schema': 1, 'source_preview': True, 'published_release': False,
                'source_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip(),
                'tracked_source_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=repo, text=True).strip()),
                'build_identity': identity, 'files': files}
    (stage / 'preview-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (stage / 'SHA256SUMS').write_text(''.join(
        hashlib.sha256(f.read_bytes()).hexdigest() + '  ' + str(f.relative_to(stage)) + '\n'
        for f in sorted(stage.rglob('*')) if f.is_file()))
    stage.rename(out)
except BaseException:
    if stage.exists():
        shutil.rmtree(stage)
    raise
print(out)
