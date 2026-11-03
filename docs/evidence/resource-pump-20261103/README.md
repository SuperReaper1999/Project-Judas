# Idle resource accounting and bulk eviction

The idle pump now reads residency once. Pumps with in-flight work still
refresh before publication so measured peaks retain the existing accounting
basis. The final pass still observes renderer-side LOD changes. Eviction
keeps the existing least-recently-used order, reference counts and recent
camera-demand protection. The single-eviction path keeps its linear scan.
After the first eviction, a bulk reduction orders the remaining eligible
entries once. Ties retain the asset table's traversal order.

## Matched measurements

Linux Release, Ryzen 5 5600. Serial before/after/after/before with nine samples
per size per run. Each sample loads independently registered one-pixel TGA
textures with the real ResourceManager in headless blocking mode. It checks
400 idle pumps, then lowers residency to two textures and verifies the
eviction count, exact retained bytes and IDs. Fixture preparation and loading
are outside both timed regions. The benchmark targets resource bookkeeping
and excludes renderer or disk upload costs.

| Resident textures | Idle pump before → after, median ranges | Bulk eviction before → after, median ranges |
|---|---:|---:|
| 128 | 0.00093 → 0.00049–0.00050 ms | 0.060–0.061 → 0.0038 ms |
| 1,024 | 0.0115–0.0120 → 0.0057 ms | 5.88–6.04 → 0.055–0.061 ms |
| 4,096 | 0.109–0.111 → 0.054 ms | 213–215 → 0.366–0.369 ms |

The small idle saving and large bulk reduction apply to these workloads.
GPU release, large asset upload and first-frame performance need separate
measurement. No upload budgeting or lifetime policies changed.

Native Linux software-GL checks passed 25/25 for residency and 14/14 for
asynchronous handoff. They cover current camera demand, pinning, cancellation,
stale generations, staged publication and teardown.

## Reproduce

Build the Release `judas_asset_residency_tests` target with GNU Make for each
source version. The benchmark links the same engine archive and dependencies
as that test. Use a fresh fixture directory per process.

```bash
e=docs/evidence/resource-pump-20261103
out=$(mktemp -d)
cp "$e/"{bench.cpp,link.py} "$out/"
inc=$(find third_party -path '*/glad/gl.h' -print -quit)
c++ -O3 -DNDEBUG -std=c++17 -Isrc -I"${inc%/glad/gl.h}" \
  $(pkg-config --cflags sdl2) -c "$out/bench.cpp" -o "$out/bench.o"
python3 "$out/link.py" /absolute/path/to/build resource-bench
for n in 128 1024 4096; do "$out/resource-bench" "$out/assets-$n" "$n"; done
```

The rendered Resource Stress run exercised 24 meshes and 16 textures. It
cancelled 20 dropped requests, discarded 10 stale completions, retained
referenced textures during a budget reduction and reloaded all four evicted
meshes. The pinned working set kept actual residency above the requested
budget, as intended. See the retained console and JSON for the full run.
