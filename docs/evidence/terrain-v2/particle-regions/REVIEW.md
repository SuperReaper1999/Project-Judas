# Native terrain and bounded particle collision regions

## Actual integration gap

The existing collision library already supported bounded snapshots. Ordinary
authored emitters did not expose that control: WorldParticles captured the entire
local chart from -8192 to +8192 metres. A kilometre native landscape has 1,024
colliders, exceeding the unchanged 512-body production capture limit even for a
tiny nearby effect. The prior small interaction tests did not qualify this path.

Emitters now optionally declare collisionRegion, collisionRegionOffset and
collisionRegionHalfExtents. The enclosing world AABB follows the emitter's local
box through its ordinary position, rotation and scale. Actual swept-body capture,
collision fidelity, receiver ownership and production budgets are unchanged.
This is explicit authored coverage, not guessed GPU particle positions, camera
culling, hidden clipping or physical terrain eviction. Default mode remains the
original whole-chart capture, including its explicit over-budget refusal.

Compatible batches additionally require identical actual capture domains. Different
regions cannot accidentally borrow another emitter's geometry. Idle, paused and
non-colliding effects do not build region geometry. Pending reliable delivery
prevents incompatible settings changes through the existing lifecycle rules.
CPU/GPU geometry and impact mechanisms remain the existing implementations.

All three fields are integrated with settings validation/equality, optional scene
keys, authored identity, editor controls, undo/prefabs, subsystem-owned JudasJS
setters/readback and declarations. Default canonical scenes and identities remain
unchanged. The new scene keys require this newer preview SDK; old readers reject
them explicitly rather than silently discarding them. No central scripting
dispatcher change or incompatible existing JudasJS rename is introduced.

## Correctness evidence

The real kilometre runtime control passes 35 write/restore checks and 9 checks in
a separate cold process. It first proves unchanged whole-chart capture rejects
over-budget geometry without moving droplets. Opt-in local effects then capture
9 bodies / 4,608 triangles while ALL 1,024 physical terrain chunks remain present.
An independent full-world reference with explicitly raised TEST-ONLY capacities
contains 524,288 triangles and produces the same supported contact, normal and
logical receiver. Production limits are never increased.

Two compatible actual CPU emitters share one snapshot, deliver reliable terrain
contacts and consume their droplets. Incompatible domains produce separate
snapshots; an out-of-domain path reports failure and retains its last safe
position instead of treating missing geometry as empty space. Independent eight-
corner bounds verify translated/rotated/nonuniformly scaled coverage. A normal
512m origin transaction preserves absolute coverage and impact position. Real
world capture and a new process restore runtime settings and queued reliable
terrain delivery. Pending contacts reject incompatible settings edits.

Authoring passes 51 checks, including new field identity, legacy canonical output,
scene/named codec/prefab roundtrips, undo/redo and malformed inputs. Scripting
passes 13 checks with new typed setters/readback and transactional negative cases.
All 140 existing scene fingerprint checks pass. Native terrain component35,
planetary component40, world93/300 exact seam steps, interactions85 and incremental
navigation67 pass. The JudasJS contract passes; registration11 passes with two
existing environment skips. NVIDIA modern particle-world qualification passes
146 checks, retaining original global coverage tests AND adding local GPU shared-
buffer/trajectory/owner-retirement cases. Portable GL3.3 passes 36 checks.

An ordinary project-owned script/scene consumer also passes: native fields load,
JudasJS patches/readback work, a real droplet produces one terrain contact and is
consumed, then relocated gameplay and a genuine cold restore retain exactly one
game-owned contact and the runtime settings. The same source fixture fails on
the prior 9a3e2a75 SDK with the specific unknown scene key, an expected negative
control. The current SDK-contained repetition is qualified separately; no source
tree fallback or installed RPG SDK selection is changed.

## Cost and boundaries

Ryzen 5 5600, GNU15.2 Release, one real stuck CPU particle in the 1,024-body source,
60 warm-up and 180 measured updates per scope, no compiler during timing. The
final active runtime median/p95/worst is 0.719 / 0.900 / 0.999 ms; capture accounts
for 0.718 / 0.897 / 0.997 ms. A separate full-world snapshot reference with raised
test capacities measures 0.922 / 0.943 / 1.305 ms. The earlier raw repetition is
retained too. These are capture/update costs, excluding rendering, physics steps,
terrain preparation and startup. This is NOT a before/after working-production
speedup, a GPU benchmark or game FPS claim: the former production path rejected
this world. Capture still enumerates all filtered world bodies once, so cost is
not independent of total scene size.

The benchmark explicitly verifies a new contact, a live stuck droplet and its
physical resting height; it does not measure the empty-emitter fast path. The
first observer triggered a GCC temporary-reference warning in its summary loop;
the loop was replaced with a straightforward reference-taking summary function.
The warning log is retained; the final tools build has no new compiler warnings.

Coverage must include every required path plus collision radius, including older
particles when an emitter moves. Outside coverage remains unsupported collision:
query failures are reported and last safe motion is retained for that step;
normal age/lifetime semantics continue. This is not a particle kill volume or a
promise of contacts outside the declared region. Reliable delivery/backpressure
inside supported coverage is preserved. Half extents are positive and at most
8,192 local metres. Future automatic envelopes or swept-world indexing are not
claimed. All terrain source arrays/colliders remain resident by source/region.

Reproduce with judas_terrain_v2_particle_region_tests --write FRESH, then --read
SAME in another process; --benchmark FRESH records every raw timing row. Run
judas_particle_world_tests under JUDAS_GL_PROFILE=modern and portable. The existing
tools/validation/qualify_terrain_v2.py gains --particle-region, installing an
ordinary consumer only in its fresh copied flat project; existing shipped projects
and saves are untouched. No new release, main promotion, platform certification,
Advanced Fluid unlock or research change is made.
