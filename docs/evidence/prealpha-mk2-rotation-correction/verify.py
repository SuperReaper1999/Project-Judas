"""Verify the exact intended checkpoint files; read-only, independent of cwd."""
from pathlib import Path
import hashlib
import json
import sys

folder = Path(__file__).resolve().parent
root = folder.parents[2]
manifest = json.loads((folder / 'FINAL_FINGERPRINTS.json').read_text())
failures = []
for name, expected in manifest['files'].items():
    path = root / name
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        failures.append(name)
for name in failures:
    print('MISMATCH', name)
print(f"{len(manifest['files'])} fingerprints, {len(failures)} mismatches")
sys.exit(bool(failures))
