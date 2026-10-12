# Final inventory capture and verification

`capture_final_inventory.py` captures the combined candidate after its source corrections, native results and reports are complete. The generated `FINAL_FINGERPRINTS.json` records that captured state; `FINAL_VERIFICATION.json` records the subsequent verification. These receipts identify the candidate without certifying that every test passes.

The script reads Git with optional locks disabled. It never stages, commits, pushes, builds, runs physics or rewrites `.git`. Its default explicit source allowlist is the previous 22-path physics candidate; a subsequent accepted persistence correction requires its exact source/test paths through `--allow-tracked-change PATH`. New task paths outside the known prefixes require `--allow-new-path PATH`. Any additional current-source/doc exception to the old 439-file manifest requires explicit `--allow-prior-change PATH`; historical evidence/prototype paths cannot be exceptions. Configuration is stored for later verification.

When the root has finalized the candidate:

```sh
python3 docs/evidence/m78-continuation/probes/capture_final_inventory.py --capture
# Add only the root-confirmed --allow-tracked-change / --allow-new-path additions.
# Optional: --binary .cache/path/to/current/executable (repeat as needed).
python3 docs/evidence/m78-continuation/probes/capture_final_inventory.py \
  --verify --write-verification
```

Outputs are directly under `docs/evidence/m78-continuation/`:

- `CHANGED_FILES.txt`: sorted exact changed/new task files, including this generated list and the manifest.
- `FINAL_FINGERPRINTS.json`: hashes task files, explicitly selected unchanged source/test/fixture inputs and historical manifests. Current binaries are optional explicit **hash-only** entries outside repository evidence.
- `FINAL_VERIFICATION.json`: optional later verification receipt. It is excluded from the captured inventory.

Self-reference is deliberate: the text list can name itself and the manifest without recursion; the manifest hashes the text list but excludes itself and the later verification receipt. Existing artifacts are never overwritten. If capture detects a late change after creating the text list, it stops and preserves that partial list for investigation; archive it explicitly before a fresh capture.

Preservation checks verify the frozen 38,386-file baseline with named current-source exceptions, original 600-file audit, prior 439-file candidate with the approved ten current-source/doc exceptions, immutable raw/performance manifests, seven original local inputs and the unrelated `POST_M73_REPORT.txt`. Build/executable/archive outputs, caches and unknown untracked paths are explicitly excluded/reported. The prior historical manifests are never refreshed to accept a mismatch. There is no general before/after proof for every ignored user asset.

`--verify` checks exact file hashes and the current Git task inventory against the captured state; it reuses the recorded allowlist rather than silently broadening it. A new capture is refused if any final metadata already exists. Syntax/`--help` were checked during preparation only.
