"""Read-only verification of the final qualification inventory, independent of cwd."""
from pathlib import Path
import hashlib
import json
import sys

folder = Path(__file__).resolve().parent
root = folder.parents[2]
manifest = json.loads((folder / 'FINAL_FINGERPRINTS.json').read_text())
failed = []
for name, expected in manifest['files'].items():
    path = root / name
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        failed.append(name)
for name in failed:
    print('MISMATCH', name)
print(f"{len(manifest['files'])} fingerprints, {len(failed)} mismatches")
sys.exit(bool(failed))
