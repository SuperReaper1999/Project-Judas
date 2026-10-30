#!/usr/bin/env python3
"""Verify Stage 0 source/evidence bytes; do not compile or run simulations."""
import hashlib
import json
from pathlib import Path


def main():
    folder=Path(__file__).resolve().parent
    root=folder.parents[2]
    fingerprints=json.loads((folder/"FINAL_FINGERPRINTS.json").read_text(encoding="utf-8"))
    for relative,expected in fingerprints.items():
        path=root/relative
        if hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
            raise SystemExit("Fingerprint differs: "+relative)
    base=json.loads((folder/"BASE_SOURCES.json").read_text(encoding="utf-8"))
    for relative,expected in base.items():
        if hashlib.sha256((root/relative).read_bytes()).hexdigest()!=expected:
            raise SystemExit("Production baseline differs: "+relative)
    print(f"Verified {len(fingerprints)} source/evidence files and {len(base)} unchanged production dependencies; no execution.")


if __name__=="__main__":
    main()
