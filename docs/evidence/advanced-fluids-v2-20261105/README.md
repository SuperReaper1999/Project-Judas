# Advanced Fluids V2 admission evidence — 2026-11-05

**Result: blocked, not an integrated runtime implementation.** Read
[the consolidated report](../../ADVANCED_FLUIDS_V2.md) first. Neither the small
controls nor a process returning normally constitute fluid gameplay acceptance.

## Reproduce

From the source checkout, with Bash, a C++17 compiler and the existing GLM
headers used by `BasinFixture.h`:

```bash
bash tools/prototypes/fluid_volume_grid/qualify_moving_grid.sh /tmp/judas-retained-grid
bash docs/evidence/advanced-fluids-v2-20261105/reproduce.sh /tmp/judas-v2-admission 3
```

The first command retains all 1,030 original checks. The second builds with
`-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror`, runs 149 new checks, then
attempts the preserved basins. **Its current exit is 1:** the basin gate fails.
`/tmp/judas-v2-admission/unit` exits zero; `authored 3` exits one and leaves
failed output unpublished. Do not wrap the authored result in an expected-fail
success and call it qualified.

```bash
sha256sum -c docs/evidence/advanced-fluids-v2-20261105/SOURCE_SHA256SUMS.txt
(cd docs/evidence/advanced-fluids-v2-20261105 && sha256sum -c SHA256SUMS.txt)
```

These source hashes describe this checkpoint. Older historical manifests still
refer to their own original source commits; they have not been rewritten.

## Evidence and scope

- `qualification.json`: machine-readable disposition and measured cases.
- `provenance.json`: source parent, compiler, flags, CPU, memory and timestamps.
- `SOURCE_SHA256SUMS.txt`: exact source, fixture, driver and policy hashes.
- `unit.log`: 149/149 new controls. `unit-sanitized.log` is the same suite with
  `-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined` and
  `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`.
  This does not claim leak-detector or platform qualification.
- `retained-regressions.log.gz`: all 26 retained suites, 1,030 checks. The separate
  geometry, gas-topology and hydroacoustic logs repeat the affected 22, 17 and
  49 checks after the final helper changes.
- `project-policy.log.gz` and `project-policy-build.log`: rebuilt existing
  Experimental project policy checks, 26/26. This was not a fresh Alpha-channel
  package qualification. Alpha policy source is unchanged and hashed.
- `baseline.log`: the previous hydroacoustic adapter's two expected original
  fixture rejections. It is not a working baseline for a speedup comparison.
- `authored.log`, `authored.exit`, `resource.log`, `timings.md`: final vented
  native-timestep/shorter-interval attempts, three samples per case; all 36
  reject transactionally. The timer encloses an admission attempt, not geometry
  compilation. Peak RSS is for the whole probe, including geometry/state copies.
  No compiler jobs were launched by this task during that final timing pass;
  other desktop activity was not controlled. With three samples, p95 is worst.

The final probe uses the unchanged preserved fixture walls, body pose and
initial water parcels, reference top gas vent, native float timestep and gravity.
The body is frozen: there is no new MK2 trajectory or moving-solid result here.
A zero-gravity admission is logged separately and does not replace the gravity
case. GPU cost, live planetary gravity, saves, streaming, export and authoring
remain unqualified for the new candidate.

## Negative controls and rejected variants

`NoLocalEOS.cpp.gz` and `NoPrismAudit.cpp.gz` are disposable mutants of the
recorded source. Each removes exactly one admission audit. Their `.log`/`.exit`
files show **149 checks, one targeted failure, exit 1**. Neither variant is
linked into the runtime or used as a qualified backend.

To rerun either mutant after building the regular objects above:

```bash
s=tools/prototypes/fluid_volume_grid
out=/tmp/judas-v2-admission
for name in NoLocalEOS NoPrismAudit; do
  gzip -dc "docs/evidence/advanced-fluids-v2-20261105/$name.cpp.gz" > "$out/$name.cpp"
  c++ -std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror -I"$s" \
    "$s/CutDynamicTransportTests.cpp" "$out/$name.cpp" \
    "$out"/{CutRectangularOwners,CutRegionGeometry,CutHydroacousticGraph,CutRegionGasTopology}.o \
    -o "$out/$name"
  "$out/$name" # expected nonzero; each targeted regression detects its missing audit
 done
```

`authored-without-local-eos.log` is an additional diagnostic, not acceptance.
It uses the final vented fixture and disables only the new local EOS audit.
The native-timestep original cases still reject: level mixed-donor prism ratio
about **1.52 million**, rotated about **8,342**, and the rectangular rotated
pressure graph fails original-row balance. Thus the obstruction is not solely
the selected `1e-10` local EOS tolerance. This diagnostic also exits one.

`pre-local-audit.tar.gz` retains an earlier sealed-chart candidate, its unit
source/log and original basin logs. Its phase/transport sources were snapshotted
before adding the local audit. The probe driver inside is reconstructed for the
previous sealed boundary and timestep settings; logging differs from the raw
original log. No byte-identical old driver/binary or performance equivalence is
claimed. Its 132-check unit result is historical, not the final 149-check suite.
The archive depends on the unchanged retained `BasinFixture.h` in this checkout.

No original failure fixture was changed into an expected-pass substitute. The
new relative EOS and solid-occlusion controls were written after observing the
candidate failures; they are explicitly post hoc regression controls, not
claimed predeclared research gates.

The three `.log.gz` files preserve the exact raw stdout bytes, including trailing spaces. They are compressed rather than edited to satisfy source whitespace checks.
