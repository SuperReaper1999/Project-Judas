# Native terrain navigation cache handoff

Base: `1b593d8f13a8a73e772ffdb014c3ddcde923421f`, Experimental.
Functional source hashes are in SOURCE-SHA256SUMS.txt. Main, fluid lock, installed
SDKs and consumer games unchanged. This is an intermediate Terrain V2 checkpoint.

The preserved base executable is
`.cache/terrain-v2/navigation-handoff-reference/before-benchmark`, SHA256
`0e3850365435376384c112a78803d00242956ac4419af1e21f8b21b6c44de965`.
Build the base source separately to reproduce when that disposable cache is absent.

Build once, `cmake --build build -j2 --target judas_terrain_v2_native_benchmark
judas_terrain_v2_navigation_tests judas_navigation_tests
judas_terrain_v2_world_tests judas_terrain_v2_preparation_tests`.
Do not build or run other qualification during timings. Each command below needs
a fresh output directory and performs five warm-file runs:

```sh
/usr/bin/time -v BASE-BENCHMARK FRESH --navigation-full
/usr/bin/time -v build/judas_terrain_v2_native_benchmark FRESH --navigation-full
/usr/bin/time -v build/judas_terrain_v2_native_benchmark FRESH --navigation-full
/usr/bin/time -v BASE-BENCHMARK FRESH --navigation-full
```

The ordering is reversed between the two groups. RESULTS.json contains raw-derived
medians, nearest-rank p95/worst (the same with ten samples) and process peak RSS.
No total-step speedup, memory saving or game FPS claim: tile construction moved
to the existing worker. Complete edit latency remains about298ms. All runs retain
1024colliders,4volumechunks,967editednavtiles and1198byte snapshots. Snapshot lengths
alone are not byte-equivalence evidence; cold tests and physical trajectory controls
separately verify state/behaviour.

Native navigation tests cover actual worker construction, private visibility,
single consumption, source mismatch, mid-cache cancellation and corrupt-layer
rejection, retaining previous live routes. Existing controls include real cave-floor
routes, physics openings, agent replan, origin, placement, whole-source deletion,
paint reuse, disable/reenable and regional cleanup. Run `--write FRESH` then
`--read SAME` in separate processes for cold persistence.

Broader checks: native world93 incl300-step exact seam trajectory, preparation25,
components35,surface59,interactions85,streaming69,raster59,saves77,exports18,
existing Spring Range navigation game26, and native editor authoring/undo/PlayStop.
No compiler warnings in either frozen build log. Tests remain unweakened.

On the actual desktop NVIDIA GPU:
`python3 tools/validation/qualify_terrain_v2.py --build build --output FRESH`.
The exporter/runtime are built from the same frozen source fingerprint. Editable
source is parked, package relocated, game run from a foreign working directory;
normal controls excavate/sculpt/paint/save, bounded cold settling resumes queued
brushes, then the player walks through and cold-restores. No teleports or private
injection. Final navready/8volumechunks/2surfacedabs/queue0/z=-10.6087.

The tunnel screenshot was inspected and shows continuous textured walls/opening.
The editor capture shows the actual landscape/nav card but the autotest's World
Building dialog obscures the viewport; it is workflow evidence, not final visual
acceptance. No human acceptance is claimed.

Explicit remaining bounds: complete-source physical residency/arrays; whole-nav
coverage rebuilds; synchronous cache reconstruction on explicit reenable; mutable
Detour heap excluded from payload admission estimates; planetary excavation and
final fresh SDK qualification remain unfinished.
