#!/usr/bin/env python3
"""Read-only byte, compression and physical-observation verification for this receipt."""
from pathlib import Path
import argparse
import csv
import gzip
import hashlib
import json

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--evidence", type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument("--root", type=Path, help="also verify the exact adopted production sources")
args = parser.parse_args()
evidence = args.evidence.resolve()

def sha(data):
    return hashlib.sha256(data).hexdigest()

def load(relative):
    return json.loads((evidence / relative).read_text())

manifest = load("MANIFEST.json")
for item in manifest["files"]:
    data = (evidence / item["path"]).read_bytes()
    if len(data) != item["bytes"] or sha(data) != item["sha256"]:
        raise SystemExit("Evidence bytes disagree: " + item["path"])
for item in load("COMPRESSED_INPUTS.json")["files"]:
    data = gzip.decompress((evidence / item["path"]).read_bytes())
    if len(data) != item["uncompressedBytes"] or sha(data) != item["uncompressedSha256"]:
        raise SystemExit("Lossless-compression proof disagrees: " + item["path"])

fields = load("observations/ANALYSIS.json")["physicalFieldsCompared"]
with (evidence / "runs/baseline/dense270/steps.csv").open() as stream:
    baseline = list(csv.DictReader(stream))
quality = load("runs/baseline/dense270/quality.json")
verified = []
for run in sorted((evidence / "runs").iterdir()):
    with (run / "dense270/steps.csv").open() as stream:
        observed = list(csv.DictReader(stream))
    if len(observed) != 270 or len(baseline) != 270:
        raise SystemExit("Incomplete physical observation: " + run.name)
    for old, new in zip(baseline, observed):
        for field in fields:
            if old[field] != new[field]:
                raise SystemExit("Physical mismatch: %s step %s field %s" %
                                 (run.name, old["step"], field))
    current_quality = json.loads((run / "dense270/quality.json").read_text())
    differences = {key for key in quality.keys() | current_quality.keys()
                   if quality.get(key) != current_quality.get(key)}
    if differences - {"observation_ms"}:
        raise SystemExit("Endpoint geometry mismatch: " + run.name)
    verified.append(run.name)

source_verified = False
if args.root:
    for relative, expected in load("SOURCE_FINGERPRINTS.json")["files"].items():
        if sha((args.root / relative).read_bytes()) != expected:
            raise SystemExit("Adopted source changed: " + relative)
    source_verified = True
print(json.dumps({"evidenceFilesVerified": len(manifest["files"]),
                  "physicalRunsVerified": verified, "stepsPerRun": 270,
                  "physicalFieldCount": len(fields),
                  "endpointTimingOnlyExcluded": "observation_ms",
                  "adoptedSourceFingerprintsVerified": source_verified}, indent=2))
