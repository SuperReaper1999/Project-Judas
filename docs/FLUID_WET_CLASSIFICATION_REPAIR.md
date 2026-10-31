# Experimental particle-fluid wet classification repair

Status: Linux automated candidate; the operator human-tested the original radial
scene on 2026-10-31 and observed no launching. Wider qualification remains pending.
See [the subsequent unlock qualification](FLUID_UNLOCK_QUALIFICATION.md) for retained
boundary failures, lifecycle restrictions and additional gameplay results. **Advanced Fluid
Dynamics remains locked in Alpha.** Main, release channels, Basic Water,
conserved-liquid implementations and independent research are unchanged.
Baseline: `cafe797a2dc661f1d1aeb38cf626e5c6184c0366`. The containing Experimental
checkpoint identifies this repair; no release or unlock is authorized here.

## Confirmed cause

The old query averaged vertical occupancy over only the water columns admitted
by a large motion search. Empty lateral space had no dry vote. Each small body
sample used the entire body's bound radius plus two particle spacings. A sample
2.77275 m sideways from its nearest contributing column could therefore receive
100% immersion: its weighted occupied numerator and admitted weight were both
0.6523212636007902. This is not a float-width or general MK2 solver defect.

In the flat reproduction the 80 kg ship acquired about 2.505875 m^3 of false
submersion: gravity supplied 24582.63 N upward and reconstructed local particle
acceleration supplied approximately (13982.11,5073.84,-94399.10) N. Initial density
relaxation alone reproduces the particle state and launch force without any
colliders. The ship then transfers impulses to the fuel props through ordinary
rigid contacts. Generic dry boxes reproduce the classifier error independently.
The original scenes and failed measurements are preserved.

## Correction

`FluidWetFootprint.h` computes a bounded lateral wet envelope from observed
column footprints at the represented particle spacing. The motion-donor radius
no longer enlarges physical wet support. Bodies, player sampling and JudasJS
`world.fluidSample` require support from one connected lateral water patch,
rather than a hull joining separate reservoirs.

Connectivity is established from observed columns before query-local solid
occlusion or vertical clipping. Occluded/dry-at-this-height columns may establish
that a pool is connected, but cannot themselves supply wet support or motion.
This retains water displaced around a genuinely immersed body. Each connected
patch's supported envelope is convex; motion interpolation and the existing
buoyancy/drag equations then run unchanged inside it.

No gravity, mass, particle spacing/count, cadence, density iteration, correction
budget, force clamp, damping, startup delay or controller tuning was changed.
No new persistent state or save format was introduced. The internal native query
has an optional connected-support argument; native code must relink against the
matching engine. JudasJS signatures are unchanged. Low-level column-interpolation
controls retain their old mean-value contract; physical callers select connected
support explicitly.

## Verification

Evidence: [machine-readable results and raw logs](evidence/fluid-wet-classification-20261031/).
Native Linux Release, GNU 15.2.0, Ryzen 5 5600; real application checks use the
NVIDIA GTX 970. This is not new Windows/Android qualification.

- **120/120 hydrostatic checks:** all original 93 remain unchanged, plus dry-gap,
  generic-body, rotation, disconnected-pool, false-swimming and positive coarse
  immersion controls. The original classifier fails 15 new checks when linked
  through a signature-only ABI adapter; its original numerical code is unchanged.
- Existing production flotation, sinking, neutral, rotated, common-freefall,
  80 kg light-body and zero-gravity cases pass their unchanged thresholds.
  Neutral drift is about 5.3 mm over four seconds. The light-body final speed is
  0.4937 m/s against its existing 0.5 m/s check: passing with a narrow margin,
  not a claim of robust settling across every configuration.
- Particle-world, boundary-contact, rigid/container coupling, swimming, project
  water and disabled-feature/persistence policy tests pass.
- Basic Water application: 48 checks pass; genuine separate-process save/load
  passes (16 checks in each child) and preserves the source save bytes.
- Original radial and flat scenes each run 240 fixed steps, plus raised radial
  lakes (+3 m and +10 m) for 360 steps, with fresh water-force-disabled controls.
  No direct body fluid loads occur in these final reproduction runs and no
  launch occurs; bodies undergo ordinary falls/contact. This does not mean
  immersed objects receive no force: the separate buoyancy controls above prove
  active support in actual pools.

Matched original-scene comparison over the first 120 fixed steps:

| Scene | Original ship peak speed | Repaired ship peak speed |
|---|---:|---:|
| Flat | 32.99 m/s | 6.70 m/s (ordinary fall) |
| Radial | 25.42 m/s | 2.10 m/s (ordinary fall) |

The repair also removes the secondary fuel-prop launches. The new raised-lake
runs are safety checks, not a timing comparison against a freshly rebuilt old
raised-lake candidate; the older M100 evidence retains its original provenance.

## Cost and limits

Matched frozen-geometry CPU sampling (300 observations per method/case, three
alternating-order pairs; assembly/query allocations included, five warmups per
process) measures a **whole body query**, not a full fluid step:

| Input | Old median / p95 / worst | New median / p95 / worst |
|---|---:|---:|
| Actual dry 125-particle snapshot | .087 / .110 / .137 ms | .134 / .170 / .271 ms |
| Captured wet 752-particle snapshot | 1.955 / 1.994 / 2.681 ms | 3.135 / 3.188 / 3.846 ms |

The spatial guard costs additional CPU work; this is a correctness improvement,
not a speedup. These short host measurements are diagnostic, not isolated-system
performance certification. Representative integrated 1152-particle light-body
work remains around 11 ms amortized coupling per rigid step in this run.

This remains a practical local hydrostatic approximation, not exact wet geometry
or a new pressure solver. Within a connected patch, concave shoreline details and
enclosed air pockets can still be filled by the convex/vertical envelope. Lateral
connectivity is inferred at particle-column resolution, not exact 3D topology.
Exterior momentum exchange, sub-particle bodies, coarse contact roughness and
other previously documented PBF limits remain. Historical FLU-001 evidence is
not deleted or declared comprehensively resolved by these selected passing cases.

## Rejected intermediate attempts

The initial outer-envelope guard passed existing checks but did not exclude
separate pools; connected support was added before checkpointing. Building that
connectivity after solid/height masking caused the light-body settling test to
fail at 0.7506 m/s. Its raw failure is retained; the final topology/support split
passes without changing the threshold. One new positive fixture also wrongly
put its sole donor inside the solid, where the existing occlusion contract removes
it. That failed fixture is retained and replaced by surrounding coarse water;
its expected force and tolerance are unchanged. Neither failure was hidden.

## Reproduce and human check

```sh
cmake --build build --target judas_fluid_hydrostatics_tests judas_production_fluid_tests -j2
./build/judas_fluid_hydrostatics_tests
./build/judas_production_fluid_tests --case light_80kg --output /tmp/judas-light-fresh-output
JUDAS_WORLD_STATE=none ./build/judas judas_tech_demo.judasproj
JUDAS_WORLD_STATE=none ./build/judas assets/scenes/terrain_flat_comparison.judas
```

Use an Experimental build for actual advanced-fluid play. Test the original and
flat scenes from a reset: no startup launch, normal falling/settling and water
still moving. Inspect a real pool for flotation and sinking as well. The original
terrain scene remains byte-identical; the flat comparison is a new scene and uses
the original root project, avoiding ambiguous automatic project discovery.
The optional Q-step viewer and deliberate suppression experiments are diagnostic
executables only; none of those force gates are linked into the game runtime.

Automated passes do not imply human acceptance, Alpha unlock or publication.
