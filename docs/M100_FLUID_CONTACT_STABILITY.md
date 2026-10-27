# Alpha advanced-fluid contact-stability restriction

## Release decision

Advanced Fluid Dynamics is **disabled in Alpha builds due to contact stability
issues**. Basic Water rendering and swimming remain available. This is a feature
restriction, not a fluid solver repair. The operator authorized this fallback if
a bounded investigation could not establish a safe targeted correction.

`ProjectFeaturePolicy.h` / `Project.cpp` derive availability from the compiled
build channel. RuntimeWorld rejects an advanced-fluid request before world
construction, including cold-world construction. ProjectExporter rejects it
before creating a staging directory. The editor explains the restriction and
cannot enable it; an existing request can be explicitly disabled and saved.
Hand-edited project flags cannot bypass the runtime/export checks. Authored fluid
components, assets and original saves are preserved, not converted or discarded.
An advanced-fluid save is not made compatible by turning its project flag off.

Experimental/local builds retain opt-in activation and original regression tests
for repair work. This is deliberately not an undocumented switch in an Alpha
package. Re-enabling Alpha requires a validated patch; no repair date is promised.

## Demonstrated defect

The operator reproduced object launches in the historical `Terrain planet` scene
(`judas_tech_demo.judasproj`, `assets/scenes/terrain.judas`) and observed that
raising the lake delayed onset. The project explicitly opts into advanced fluids.
The unchanged reviewed candidate was `0dab8e5db6e2ff1dc94283091247a097de610eb5`.

Matched full-loop controls use the real scene, GameSession and StepPlayedWorld,
1/60 s fixed timestep, no input, five rigid bodies and 125 fluid particles:

| Control | Observation |
|---|---|
| Advanced off, unchanged rigid scene | Bodies settle normally; rigid-only tests also find no initial terrain penetration. |
| Advanced on, original lake | Ship receives 85,212 N at 0.0333 s and reaches 25.42 m/s at 0.05 s. |
| Advanced on, lake +3 m | First force above the diagnostic 8,000 N reporting threshold remains at 0.0333 s, reduced to 9,823 N. |
| Advanced on, lake +10 m | Initially dry; first force above the same threshold occurs at 1.3667 s, 150,066 N. |

The reporting threshold only selects log output; it is not a solver cutoff or
physical acceptance criterion. The raised-lake timing corroborates the operator's
observation; the diagnostic does not independently timestamp first terrain impact.

Before the original force spike, particle-to-ship clearance including particle
radius is 1.7759 m, yet hydrostatic reconstruction reports 0.1671 wet fraction.
Drag is small relative to the reconstructed buoyancy load.

`EvaluateFluidHydrostatics` uses the entire body's bound radius at each local
quadrature sample. `FluidHydrostaticField::Query` extends nearby column occupancy
and material acceleration across its reconstruction neighbourhood, normalizing
only over admitted columns. This can extend water across dry shoreline gaps.
The contact-disturbed material acceleration then enters `F = -rho V (g - a)` with
false wet volume. ProductionFluidCoupling applies the force to MK2, which responds
to that load. This establishes a bad coupling path, not a general MK2 instability
or an audit of every advanced-fluid representation.

## Bounded experiment and stopping point

A scratch-only prototype replaces the body-wide donor radius with each local
quadrature footprint. No damping, clamping, solver budget or cadence was changed.
The unchanged hydrostatic fixture passes **93/93**; the prototype fails **8/93**,
including rotation covariance, sphere half-immersion and captured-pool force
covariance. Both runs used the same original 752-particle settled fixture.
The prototype settles the original scene's ship, but the raised +10 m case still
produces a 293,731 N spike and a 54.89 m/s ship at two seconds. It is rejected.
The initial missing-fixture-path experiment is retained in task scratch; it was
corrected before these matched results and is not counted as a numerical failure.

The search is also used to infer liquid around space displaced by submerged
solids. Simply shortening it loses legitimate support; removing material
acceleration would break free-fall/accelerating-liquid contracts. A proper repair
needs boundary-aware wet-support reconstruction and consistent solid coupling,
with dry-gap, submerged, contact-impact, rotation and accelerating-liquid controls.
That is broader than a safe release-blocking local edit. No solver prototype was
merged and no claim is made that all underlying numerical problems are solved.

## Evidence and qualification scope

[Raw observations and rejected prototype](evidence/m100/fluid-contact-stability/)
retain exact logs and diagnostic source. The helper scripts record the task's
existing Ubuntu build-root layout; see that directory's reproduction notes.
Protected historical evidence and independent fluid research were not modified.

The coordinated M100 runner records advanced runtime cases as **not exercised**
in Alpha, rather than counting them as numerical passes. Original tests remain
intact and eligible in Experimental. Direct liquid geometry/storage tests remain
eligible. Feature-policy and Basic Water tests exercise actual Alpha behaviour.
The earlier `0dab8e5d` sealed SDKs do **not** contain this restriction and must not
be published as the corrected candidate. New archives and exact-candidate
qualification are required before release. No Alpha publication is authorized by
this investigation, and no human acceptance of the restriction is inferred from
an automated pass.

## Focused verification

Linux Release host builds exercised both compiled channels. Alpha policy checks
cover the saved flag, rejection before allocation/publication, cold-snapshot
construction, particle emission, nonempty advanced-save rejection, inert streamed
owners, project editing and export preflight. Spoofing environment channel/enable
variables does not change the compiled Alpha policy. Experimental tests continue
to exercise actual opt-in simulation and the original confirmation workflow.

Basic Water application controls exercise rendering, swimming-on/off endpoints,
pause, no hidden advanced work and two fresh-process save/load endpoints. A real
Basic Water project was exported, moved, and launched from `/tmp` with no engine
root override; it completed 90 steps and wrote a rendered frame. This is a moved
host-native export smoke, not a fresh isolated Ubuntu-baseline SDK certification.
Python release-packaging tests include an exclusion negative control: unavailable
runtime tests are retained and labelled not-exercised, while geometry/storage and
Basic Water tests remain selected. Current documentation links are checked.

The original fluid algorithms, physical tolerances and protected fixtures remain
unchanged. No new Windows validation is claimed by this bounded follow-up. Final
source/binary hashes and checkpoint identity are recorded in the external handoff;
these retained development logs deliberately keep their original dirty-build
provenance rather than being relabelled as clean release builds.

Human checklist: restart into the corrected Alpha editor; confirm Experimental
settings cannot enable Advanced Fluids; open an old opted-in project and verify
Play reports the restriction; explicitly disable and save the request; test
Stillwater swimming and export. This is not final Alpha human acceptance.
