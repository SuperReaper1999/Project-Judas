# Pre-Alpha: elastic energy investigation and older Linux SDK

Input: Experimental `04388ffd1f9590bda0fa8114f235f5057b1c1a7c`.
Main remains `0aaef603b0347a55350a81beff896cff21d914bf`.
This is not M100, a release, platform-channel change or human acceptance.

## Disposition

- **Linux binary compatibility corrected through an optional baseline build**:
  the complete SDK builds and executes against Ubuntu 24.04's own compiler,
  glibc, libstdc++, SDL2 and Mesa. No host libraries or ELF symbol versions are
  replaced. See [workflow](LINUX_BASELINE_BUILD.md).
- **Elastic collision defect remains open**. Its numerical mechanisms and a
  moderate-rate reproduction are now established. Rejected experiments are
  preserved; no incomplete physics correction is included. The accepted solver
  source, timestep, restitution, budgets and fixtures are unchanged.

## 1. What actually creates the energy

The original no-force/no-friction box and walls are retained byte-for-byte in
[the earlier reproduction](evidence/prealpha-linux-readiness/elastic-limit/REPRODUCE.md).
The independent formula is

`E = m |v|² / 2 + omegaᵀ (R I_body Rᵀ) omega / 2`.

The original direct-native fixture still produces **5077.33334 → 125043.475**,
ratio **24.6277852**, at 100 m/s, 100 rad/s and `dt=1/60`. GCC 13/glibc 2.39 and
GCC 15/glibc 2.43 reproduce the same printed endpoint. This is not a new-libm
compatibility problem. Instrumentation uses actual physical inertia/velocities,
not the path-hold counter as an energy oracle.

### A. Temporary arrival pose and physical velocity disagree

In `src/physics/WorldMk2Solver.cpp`, `Mk2Solve` temporarily moves struck bodies
to their estimated arrival configuration, solves the impact there, restores the
start orientations, and records velocity deltas for later integration.
The angular velocity is kept when that orientation is restored. For an
anisotropic body it now belongs to a different world inertia tensor.

The first impact in the preserved failing case:

| Boundary | Actual kinetic energy |
| --- | ---: |
| Step entry | 5077.33334026 |
| Arrival pose, before impulses | 5077.33334026 |
| Arrival pose, after impulses | 5077.33352179 |
| Start pose restored with the same angular velocity | 5781.01833078 |

The impulse itself meets its local energy budget to float rounding. The pose
rewind then adds about **704 energy units** without an external force. Checking
only the impact solver's local budget cannot catch this publication error.
A momentum-preserving backward transport is also not a sufficient correction:
the rewind is temporary solver bookkeeping, not actual free rotation.

### B. Exponential orientation drift uses a Cayley midpoint predictor

`src/RigidBody.cpp::AngularDriftGenerator` solves the body-angular-momentum
implicit midpoint equation for the **Cayley** orientation update. MK2's
`RigidMotionSegment::EvaluateGeometry` instead uses an exact exponential angle
when `exactRotation` is true. These are different maps. Transporting world
angular momentum through the latter no longer preserves the former's energy
identity for a general anisotropic spin.

The isolated, contact-free box `(0.08,0.2,0.05)`, mass 1,
`omega=(100,70,30)`, `dt=1/60` gives:

| Existing drift map | Initial energy | Final energy | Ratio |
| --- | ---: | ---: | ---: |
| Cayley/midpoint | 85.06167669 | 85.06169135 | 1.00000017 |
| MK2 exponential with the same predictor | 85.06167669 | 118.16424163 | 1.38915956 |

Principal-axis and slow-rate controls explain why the existing small-step free
rotation regressions still pass. Simply reverting every rotation to Cayley
would reintroduce the known large-angle principal-axis timing error; it is not
an accepted fix.

### C. Relaxation and continuous re-timing also cross configurations

In the original trace, the first post-integration relaxation changes energy
**5721.76374965 → 7529.96241201**. Row geometry/inertia have been refreshed, but
accumulated impulses represent responses applied in the earlier configuration.
Their removal through new Jacobians is not automatically a passive operation.
This boundary is a demonstrated remaining energy source, not proof that every
relaxation is wrong.

`Mk2ResolveArrival` likewise constructs first-touch copies using endpoint
velocities, checks its local budget at the first-touch pose and writes the
result back for `retime`. Its budget does not cover the intervening inertia
change or subsequent free drift. The original continuous pass changes total
energy **7221.71186438 → 125043.406694**. The complete tail is not claimed to have
been analytically apportioned between those mechanisms.

### Actual gameplay relevance

The 64-case, two-second test holds the original geometry, 60 Hz cadence, zero
friction and no forces, varying restitution, initial speed and spin. It counts
actual events, so slower tests are not mistaken for collision-free passes.
At **5 m/s, 20 rad/s, restitution 1**, energy rises **15.5933335 → 20.5590387**,
**31.845%**, with 326 impact-work events and **zero path holds**. The safety
arrival cap is therefore not the root cause. At 5 m/s and 5 rad/s the peak gain
is about 0.180%. It can affect deliberately bouncy gameplay, not just exotic
100 m/s fixtures. Tested restitution 0, 0.2 and 0.5 cases stayed below their
initial energy; this does not establish general per-impact passivity.

The old native/JS endpoint discrepancy is explained: the native probe creates
the walls first, while the scene creates the dynamic box first. Changing only
creation order in the native probe reproduces the JS endpoint exactly
(`vx=-23.643013`, `wz=-803.034058`, energy ratio 1.03724573). There is no evidence
here of a JS setter or timestep defect. Slot-dependent manifold ordering changes
the trajectory; a special creation order is not a correction.

### Experiments rejected, and why no physics patch is shipped

1. Momentum-transport velocities across temporary poses and retiming. Original
   first-step endpoint energy remains about **8776.89**, above 5077.33. A valid
   transport primitive alone does not repair the timing/ownership model.
2. Pull the trial angular velocity back in body coordinates and reapply the
   delta at arrival. It removes the first 704-unit jump, but leaves relaxation
   and free-drift errors; endpoint energy becomes about **174519**, worse.
3. Use a self-consistent exponential midpoint in the isolated free-rotation
   experiment. It preserves energy within about 1e-7 relative at rates through
   100, but the combined contact prototype still gains energy and its nonlinear
   solve fails on the original collision case. The separate 1000-rate case also
   fails. This is research evidence, **not** an installed integrator or a pass.

A proper repair must make arrival impulses, orientation/inertia, accumulated
constraint responses and the motion ledger refer to the same physical interval.
It must also use a mutually consistent rotation predictor/update. That is a
coherent contact/integration correction, not an energy clamp, extra damping,
lower restitution or a larger iteration budget. The attempted local edits did
not meet that standard and were removed. This bounded task establishes the
mechanisms but does **not** close the physics issue or claim Alpha readiness.

[Raw sources, patches, results and reproduction instructions](evidence/prealpha-elastic-linux-baseline/README.md)
retain both successful controls and failures. Protected historical evidence and
independent fluid research were not edited.

## 2. Linux dependency chain and correction

`objdump -T` finds these six `GLIBC_2.43` imports in the Ubuntu 26.04 runtime:
`sqrtf`, `atan2f`, `asinf`, `acosf`, `remainderf`, `remainder`.
`nm -A -u libjudas_engine.a` attributes calls to ordinary engine objects,
including `NetworkSession`, `Wheel`, `Powertrain`, IK, animation and geometry.
The host `libm.so.6` exports both legacy `GLIBC_2.2.5` versions and new default
`GLIBC_2.43` versions. Normal native linking selects the latter. This is not an
unnecessary third-party package that can simply be removed.

The optional helper builds the **whole** dependency chain in an older userland.
It reuses the existing SDK inventory, pinned repository text/navigation sources,
build identity, checksums, exporter and ABI/loader inspector. Native/MSVC/Android
build paths are not altered. No glibc bundling, symbol rewriting or fake minimum
version is introduced.

The exercised backend is rootless bubblewrap with the official Ubuntu Base
24.04.3 archive and Ubuntu 24.04 package updates. GCC **13.3.0**, glibc
**2.39-0ubuntu8.9**, SDL2 **2.30.0** and Mesa llvmpipe are recorded. All **17**
executables share the same identity and resolve against that environment.
Highest required versions are **GLIBC 2.38**, **GLIBCXX 3.4.32**, **CXXABI 1.3.13**.
Ubuntu 24.04 is the qualified target; this does not claim Ubuntu 22.04 support.

Positive evidence:

- Full 17-tool Release SDK build and matching identity probes.
- Inventory/checksums, required-symbol gate and actual older-loader checks.
- Packaged editor creates an ordinary project, edits/undoes, saves and exports.
- Separate editor Play/Stop starts the project script and leaves authored scene
  identical. The unsaved-navigation smoke is deliberately separate.
- Relocated exported project executes 90 fixed steps and renders its script-driven
  scene under Ubuntu 24.04 Mesa/Xvfb, from `/tmp`, with **neither SDK nor source
  checkout mounted** in the execution namespace.
- Hearthworld actual tree felling, mining, gravity-gun use, construction, excavation
  and cold-process persistence pass: **66 write checks / 41 fresh-process restore
  checks**, using an owned project copy and saves under the older userland.
- MK2 diagnostics **106 checks** and rigid-motion regressions **10237 checks**
  pass using the older compiler/runtime. The known elastic reproduction still
  fails, with unchanged results; those passing suites do not certify it.

Negative controls:

- The prior 26.04 SDK fails the existing glibc 2.39 inspector gate for all 17 tools.
- The real Ubuntu 24.04 loader refuses its runtime with
  `libm.so.6: version GLIBC_2.43 not found`.
- Tooling tests reject unsafe mount layouts, missing prepared roots, absent
  backends and invalid job limits. Qualification refuses a non-24.04 environment.

Docker/Podman have the same isolated build interface and command-construction
checks, but are **not execution-qualified here** because neither is installed.
The workflow is reproducible from documented prerequisites; future apt updates
are not a promise of byte-identical compilers. Exact package and binary hashes
are retained for this run. Physical GPU/audio-device testing, other distributions,
Windows and Android were not performed in this task.

## Human checklist

1. Run the older-baseline SDK editor and try your ordinary create → Play → Stop
   → export workflow; launch the moved package.
2. Inspect the SDK qualification receipt and its build identity before sharing
   that specific package. A new host-native build can still require newer glibc.
3. Treat perfectly elastic, fast-spinning confinement as an **open numerical
   defect**, not a solved feature. The retained energy checker intentionally
   fails; normal-looking presentation is not energy verification.

This checkpoint is suitable for reviewing the Linux compatibility workflow and
investigation evidence. It is **not** acceptance of unrestricted elastic dynamics.
