# Reconciliation source boundary — 2026-11-17

The primary Experimental candidate starts at ca0d437135ac5f5dd7adf9314b56526099d1c313
with committed production MK2. The original dirty contact-material and reduced-hull
prototype was preserved separately before any reconciliation commit.

- Preservation branch: `codex/preserved-contact-material-20261117`.
- Preservation commit: `2e6079950625fedb59b343979fc47f29de15395b`.
- Worktree: `source-worktrees/contact-material-preserved`, excluded locally from
  primary status and outside `.cache` / `build`.
- All 17 selected source files matched SHA-256 after preservation. The exact paths,
  hashes and restored/relocated classification are in
  `evidence/source-selection-preservation.json`.
- Independent physics-core branch remains `codex/physics-core-v2` at
  `d12490a9f1fa1b4a48672a7ab858d1b2f3347141`.

The primary source no longer selects the dirty prototype hooks or opt-in proof
targets. Preservation does not certify those implementations or merge them into
accepted functionality. No selected external SDK or release branch was changed.
Old Stage A binaries and timings used the dirty prototype; they are historical
audit evidence, not candidate qualification. Current builds reuse the primary
build directory and must report their new source identity.
