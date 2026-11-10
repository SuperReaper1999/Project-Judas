# Revised Stage 1 — physical bonnet attachment failure

Experimental follow-up to the [wheel/panel proof](DEFORMABLES_V2_REVISED_STAGE1.md),
whose baseline is `c9959714eb5bd7023436cbc8918edb63a7a0123f`.
The operator accepted the improved wheels and local bending, but reported that the
bonnet never detached. This correction awaits their final visual review. Main,
the Advanced Fluids lock, RPG SDK and third-party originals remain unchanged.
Stages 2–4 have not begun.

## Cause and correction

The attachment constraints were not secretly infinite. Their rear hardware limits
were 20 kN and 8 kN m; measured 30 m/s wall-launch peaks were only about 10–14 kN
and 1.2–1.3 kN m. More importantly, the surrounding material could accumulate
permanent bending indefinitely without ever tearing its attachment to the car.
The previous proof qualified latch release, not full bonnet detachment.

The existing hardware limits are **unchanged**. The proof now also gives each
mounting-sheet patch an effective **20 J irreversible bending-work tear capacity**.
For each existing physical dihedral hinge, its accumulated plastic work is shared
among its four material nodes. A mount receives only its own nodes' share. Distant
bonnet damage cannot tear an unrelated mount, and work is not duplicated into a
new fatigue accumulator. Elastic movement, elapsed time, car speed and impact
count do not drive failure. The existing physical material provides the yielding,
hardening, irreversible work and persistence. This is an effective game-material
calibration, not a measured Challenger sheet-metal or fastener specification.

Publication occurs after a completed material step using the normal `Release`
path. There is no body creation, velocity reset, teleport or ejection impulse.
The same 25 kg simulated material becomes debris. Released targets stop resolving
their former chassis handles. Remaining targets continue ordinary physical coupling.
After complete release, the bonnet's own-car collision exclusion is removed; it
collides with its former vehicle as well as the world. Restoration derives that
collision policy from the saved release flags.

The attached/partially attached proof still excludes overlapping coarse inner-frame
proxies. Detailed engine-bay self-collision and general sheet tearing are still
outside this native Stage 1 proof; full detachment does not qualify those features.

## Qualification

- Quiet driving, braking, steering, reverse and uneven-ground course: no tear and
  zero permanent bonnet strain. Light 4/8 m/s wall cases retain all three mounts.
- The 16 m/s frontal case releases the latch while retaining both hinges.
- The 30 m/s wall and offset-pole cases reach complete physical release. The pole
  passes through a one-hinge state first. These are declared launch conditions,
  not speed-based triggers or measured first-contact speeds.
- Repeated 16 m/s launches accumulate local physical material work and can release
  the bonnet; already detached material is excluded from later vehicle launches.
- Separate process writers/readers restore exact hashes for intact, latch-only,
  one-hinge and fully detached states, then continue real simulation.
- The sustained wall/pole runs leave debris supported by ground contacts while
  the car moves approximately 46/62 m away. The new procedural checks separately
  protect local ownership, subcritical work, no release kick/mass change, remaining
  connections, independent motion, ground collision and restored collision policy.
- 174 structural checks pass (17 new), alongside the affected vehicle/material/
  powertrain regression executables. A reused powertrain-script output initially
  rejected already-existing metadata; its supported fresh output-directory run
  passes unchanged. The failed attempt is retained privately, not hidden or fixed
  by weakening asset identity checks.

Paused inspection also exposed an existing cold-load presentation defect: wheels
were published before restored structural carriers existed. `WorldPersistence::Published`
now republishes their derived poses after all participants are restored, without
running physics. Five restored crash cases reproduce the original failure and
protect visible wheels before the first simulation step.

The matched steady-state CPU controls measured one-car p50 **2.115 → 2.113 ms**
and four-car **16.220 → 16.117 ms**; panel means were **0.365 → 0.366 ms** and
**2.032 → 2.026 ms**. Untorn vehicle/wheel telemetry was identical. These small
variations are not evidence of a new optimization. The final restore publication
change only executes at restore completion, not in the simulation hot path.
Desktop GTX 970 render diagnostics (501/504 GPU samples) gave p95 **1.40/1.01 ms**;
CPU render p95 **0.914/0.929 ms**. There is no new renderer algorithm, and this
single paired run does not establish a GPU speedup. Severe structural-impact
spikes and four-car frame-budget limitations remain.

Actual matched CPU/render costs and final automated results are in the
[evidence summary](evidence/deformables-v2-bonnet-attachments/qualification.json).
CI is reported from completed runs in the final handover; pending is not passing.
No native Windows/Android execution of this local crash proof is claimed.

## Private launch and inspection

Project: `/home/conner/Projects/Judas_Deformables_V2_Crash_Proof/`.
The refreshed `Play-Revised.sh frontal --speed=30` starts stationary; Enter performs
its declared diagnostic launch. W/S drive/reverse, A/D steer, Space brake, P pause,
Q/E orbit, mouse wheel zoom, **C follows bonnet/car**, F6 save, R restart, Esc quit.
`--bonnet-view` starts the camera on the physical material. `--paused` starts an
interactive inspection paused; it is rejected for headless runs.

The private handover provides `Inspect-Bonnet-State.sh intact|latch|one-hinge|detached`
with qualified saved states, initially paused. Resume with P and inspect the actual
remaining connection or independent debris. `--save-release=101` and similar flags
are diagnostic snapshot triggers only: they observe state and cannot release a
mount. A requested state that never occurs fails the run rather than silently
pretending that it was saved.

Proof content identity is now `dihedral-panel-mount-tear-v4`. Earlier proof saves
are deliberately rejected with a configuration mismatch; their binary, source and
saves remain preserved privately. No migration or release-format change is implied.
All downloaded meshes, textures, derivatives and visual evidence remain private.
