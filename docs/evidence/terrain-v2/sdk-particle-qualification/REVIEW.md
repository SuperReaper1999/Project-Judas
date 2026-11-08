# Terrain V2 packaged particle and persistence qualification

Engine checkpoint: `148c5fce257b02c97b33511af4f0f75ac6741772`, Experimental.
Local artifact: `.cache/terrain-v2/sdk-native-preview-148c5fce`.

All 17 native tools rebuilt, staged with tool probes and verified. The standard
packaged consumer passed its eight operations, including project creation,
editor Play/Stop, saves and relocated exports. The SDK's own copied
`tools/validation/qualify_terrain_v2.py --particle-region` also passed: native cook,
relocation, real cross-chunk excavation/traversal and a separate-process reload.
Its project-owned script received one actual reliable terrain impact, consumed
the droplet and preserved that counter and runtime collision-region settings.
The ordinary terrain checks still require eight excavated chunks, two surface
dabs, an empty edit queue, ready navigation and traversal through the hill.

The current streaming suites pass 69 ordinary and 37 planetary checks. The
5,271,385-byte runtime source survives 30 write and 18 cold-read checks. Commands,
provenance, raw logs and machine-readable results accompany this record.

This is a host-native GNU 15.2 Release build requiring GLIBC 2.43, **not** the
Ubuntu 24.04 distribution build or a published release. SourceDirty and 18
preexisting untracked build inputs are recorded honestly. Filesystem namespace
isolation was not used; relocation and SDK-only tools do not certify a clean
machine. NVIDIA desktop execution is automated engineering evidence, not human
acceptance or new Windows/Android/browser qualification. The installed RPG SDK
remains unchanged at local `9f022cf1`.

## Reproduced CI fixture failure and correction

GitHub run 42450066469 failed before C++ compilation because the older source
archive fixtures committed `fixture\n` as `packaging/sdk-inventory.json`. Since
8ca4ee17, source archives deliberately read the requested checkout's committed
inventory. The newer packaging tests passed, but did not exercise those older
invalid fixtures. Locally the old suite reproduced four JSON errors in 29 tests.

The fixtures now commit valid inventory JSON, and their existing negative-control
mock accepts the optional learning-project argument. No production parser or
archive assertions were weakened. An added malformed-inventory negative control
requires rejection and exact preservation of an existing archive. The corrected
suite passes 30 tests; coordinated packaging/workflow/registration tests pass 36
with two existing environment skips. Raw before/after and GitHub logs are kept.
Local success is not a claim that the next remote CI run has passed.

Reproduction:

```sh
python3 scripts/prealpha_packaging_checks.py
python3 -m unittest tests.test_release_packaging tests.test_ci_workflows tests.test_validation_registration
python3 tools/package_sdk.py verify .cache/terrain-v2/sdk-native-preview-148c5fce
python3 scripts/qualify_sdk.py --sdk .cache/terrain-v2/sdk-native-preview-148c5fce --work /tmp/terrain-v2-sdk-new-consumer
python3 .cache/terrain-v2/sdk-native-preview-148c5fce/tools/validation/qualify_terrain_v2.py --build .cache/terrain-v2/sdk-native-preview-148c5fce --output /tmp/terrain-v2-sdk-new-region --particle-region
```

Use fresh output directories. GPU execution uses the actual desktop session.
SDK qualification does not complete Terrain V2: per-chunk physical interest,
demand paging, temporal LOD transitions and broader materials remain open.
Main, release identity/channels and the Advanced Fluid lock remain untouched.
