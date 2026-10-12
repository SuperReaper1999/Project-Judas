#!/usr/bin/env python3
"""Scheduled read-only body/contact observations of existing final cold saves.

No compilation, controller change or new writer. Copy the exact captured inputs
before the existing observer runs, so its diagnostics cannot overwrite history.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[4]
DEST = ROOT / 'docs/evidence/m78-closure/ordinary-actual-endpoints'
OUTPUT = ROOT / '.cache/m78-closure/ordinary-actual-endpoints'
OBSERVER = ROOT / '.cache/m78-closure/mixed-final/observer'
EXPECTED_BINARY = 'c805cf1aa73ef35f918f1a86dbd17ab4223783d95364b57e669995740b874967'
EXPECTED_ARCHIVE = 'a5ec91426ce0f8476db3bbdbda8ad52f3094d38c37b64a0052c7ef4bd37c2e1f'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--execute', action='store_true')
    args = p.parse_args()
    if not args.execute:
        raise SystemExit('Preparation only; root schedules --execute after native release')
    archive = ROOT / '.cache/m78-closure/final-build/libjudas_engine.a'
    if digest(OBSERVER) != EXPECTED_BINARY or digest(archive) != EXPECTED_ARCHIVE:
        raise SystemExit('Final observer/archive identity mismatch')
    if DEST.exists() or OUTPUT.exists():
        raise SystemExit('Output exists; preserve it rather than overwrite')
    DEST.mkdir(); OUTPUT.mkdir()
    environment = dict(os.environ, SDL_VIDEODRIVER='offscreen', LIBGL_ALWAYS_SOFTWARE='1')
    results = []
    for label in ('stand', 'recovery'):
        source = ROOT / ('.cache/m78-closure/final-application-cases/balance-cold-' + label)
        owned = OUTPUT / label
        owned.mkdir()
        shutil.copytree(source / 'project', owned / 'project')
        shutil.copytree(source / 'slots', owned / 'slots')
        source_save = source / 'slots/m77.save'
        save_sha = digest(source_save)
        if digest(owned / 'slots/m77.save') != save_sha:
            raise SystemExit('Copy differs from exact final captured slot')
        command = [str(OBSERVER), str(owned), 'cold-' + label + '-read']
        start = time.monotonic()
        result = subprocess.run(command, cwd=ROOT, env=environment,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        case_dest = DEST / label
        case_dest.mkdir()
        (case_dest / 'stdout.log').write_bytes(result.stdout)
        (case_dest / 'stderr.log').write_bytes(result.stderr)
        for path in sorted(owned.iterdir()):
            if path.is_file():
                shutil.copy2(path, case_dest / path.name)
        physical = json.loads((owned / 'physical-final.json').read_text())
        actual = physical['currentPhysicalEndpoint']
        entry = dict(case=label, command=command, exitCode=result.returncode,
                     seconds=time.monotonic()-start, originalSaveSHA256=save_sha,
                     originalSaveUnchanged=digest(source_save)==save_sha,
                     copiedSaveUnchanged=digest(owned/'slots/m77.save')==save_sha,
                     observerSHA256=digest(OBSERVER), archiveSHA256=digest(archive),
                     scope='Actual current rigid bodies, gravity and completed contacts; not cached controller observation or universal trajectory parity',
                     actualEndpoint=actual,
                     passed=result.returncode==0 and actual['currentPhysicalStanding'])
        results.append(entry)
        (DEST/'RESULTS.json').write_text(json.dumps(dict(results=results),indent=2)+'\n')
        print(json.dumps(dict(case=label,exitCode=result.returncode,
                              currentPhysicalStanding=actual['currentPhysicalStanding'])),flush=True)
        if not entry['passed'] or not entry['originalSaveUnchanged'] or not entry['copiedSaveUnchanged']:
            raise SystemExit('Actual physical endpoint or preserved snapshot check failed')

if __name__ == '__main__':
    main()
