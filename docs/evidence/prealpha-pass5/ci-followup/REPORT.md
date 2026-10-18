# Pass 5 — actual hosted CI follow-up

## Result

[GitHub Actions run 39159743311](https://github.com/SuperReaper1999/Project-Judas/actions/runs/39159743311)
completed **successfully** at exact commit
`050016a179313c90584ce00f141cc5ca4c521808` on Experimental. All 13 job steps passed.
The previously failing tooling inventory stage passed, followed by a clean Release
build, all **38 focused CTest gates**, packaged SDK qualification and strict SDK
verification after inspection. `FINAL_RUN.json` and the original complete job log
record the actual hosted outcome; this is not inferred from local checks.

The focused CTest run took 34.54 seconds. Its artifact contains 36 structured native
executable results, each passed with exit 0, unchanged executable hashes, exact
clean source identity before/after, plus the successful gate-negative-control and
build-organization checks. `SUMMARY.json` maps the 36 native results to the exact
preserved receipts. This remains an explicit focused subset, not the full engine
production suite or certification of separately recorded known failures.

## Clean build and SDK qualification

- Build ID: `0.1.0-prealpha-Experimental-050016a-clean-bf269556e921`.
- Clean source revision: `050016a179313c90584ce00f141cc5ca4c521808`.
- Source fingerprint: `bf269556e9215a4f5a480ee6f627de39355ecf8ac77fd55e5142d5018ee7c49a`.
- Compiler/configuration: GNU 13.3.0, Release, x86-64 Linux.
- Actual qualification host: **Ubuntu 24.04.5 LTS**, glibc 2.39.
- Actual inspector policy: native loader checks enabled, required OS
  `ubuntu:24.04`, maximum required glibc `2.39`.
- All **17 packaged ELF tools** qualified with no errors and 50 resolved
  dependencies. Highest required ABI versions: GLIBC **2.38**, GLIBCXX **3.4.32**,
  CXXABI **1.3.13**.
- SDK manifest SHA256:
  `effdd8ff1ba9e337c3f21075c436ed8fd6081c47f21e41ec53137613f72ff445`.
- The actual **packaged** inspector ran; strict SDK verification subsequently ran
  from `/tmp` and reported `ok: true`, **212 files / 17 tools**.

The collected native build-info and SDK build-info contain the same identity;
the SDK serializes keys in sorted order, so these JSON objects are compared
semantically while their original bytes and individual hashes are preserved.

This qualification applies to the clean hosted Ubuntu build. It does not change
the distinct local development-host SDK's glibc 2.43 requirement, certify arbitrary
Linux distributions, or establish physical GPU/application/human acceptance.
Windows and Android are outside this hosted run.

## Artifact provenance and bounded collection

[Diagnostics artifact 12148741088](https://github.com/SuperReaper1999/Project-Judas/actions/runs/39159743311/artifacts/12148741088)
is named `prealpha-linux-050016a179313c90584ce00f141cc5ca4c521808`. The downloaded ZIP
is 17,246,837 bytes and its SHA256 exactly matches GitHub's reported digest:

`e5bfe7d8e1f1219de3e83fea6f77daa28082719798d2a138b7ba22d3768f1dbe`.

`ARTIFACT_COLLECTION.json` records all 167 archive members, their original paths,
sizes and CRCs, and explicitly maps the **82 selected receipt files (672,584
bytes)** into `artifact/`. Only build/SDK identities, qualification/dependency
metadata, manifests/checksums, focused results/logs, tooling summaries and CTest
logs were extracted. Generated content, save fixtures and the 42 MB resource test
model were excluded; the ZIP itself remains only in ignored local cache. No engine
executables, libraries or object files are added to evidence.

Actual status snapshots were read at bounded 45-second intervals. The watcher
stopped on completion. `POLL_RECEIPTS.json`, `FINAL_RECEIPTS.json`, `artifacts.json`
and the original full job log preserve command exits, source head and remote
metadata. No run was retried, cancelled, altered or skipped during this follow-up.

## Reproduction and verification

The original uploaded artifact can be obtained while GitHub retains it:

```sh
gh api repos/SuperReaper1999/Project-Judas/actions/artifacts/12148741088/zip > /tmp/judas-ci-diagnostics.zip
```

Compare the ZIP SHA256 with the exact digest above before inspecting its contents.
The committed `ARTIFACT_COLLECTION.json` gives the precise selected member mapping.

To verify the preserved hosted identities, gate outcomes and SDK qualification:

```sh
python3 docs/evidence/prealpha-pass5/ci-followup/reproduction/verify_receipts.py
```

This checks actual receipts only and does not rerun an engine test or manufacture
hosted success. `reproduction/watch_run.py` preserves the exact bounded monitoring
procedure; monitoring is already complete and need not be repeated.

## Remaining boundaries

Known curved-mesh continuation failures, dense-contact performance tails, deferred
FLU-001 and the original liquid-origin grid query limitation remain classified in
the unmodified native gate receipts. Their exclusion from the focused green gate
is explicit. This result establishes no new human validation or main promotion.
The follow-up changes evidence only; no implementation or workflow is changed.
