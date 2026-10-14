# M83 corrective: structural crash lab

Frozen dense-node research record. The current replacement is documented in [M83 coarse physical sections](M83_COARSE_CRASH.md). Human acceptance of that replacement is pending. The previously accepted M83 checkpoint remains preserved; this extension does not claim BeamNG parity or crash-safety certification.

## Real-time representation decision — 14 October 2026

The globally implicit node/beam candidate described below is now **frozen research**, not the selected game implementation. It established useful elastic/plastic, contact and finite-body coupling results, but its damaged-car cost and remaining continuation failures are unsuitable for a real-time multi-vehicle game. Its exact95-file source/content overlay and all original results/failures are preserved in [the reassessment evidence](evidence/m83-structural-corrective/architecture-reassessment/DECISION.json).

The selected replacement is a modest assembly of physical rigid sections with generic finite elastic/plastic connections, using ordinary PhysicsWorld contacts, CCD and constraints. Local section motion must change collision, bodywork and mechanical mounts. A protected central section is a real structural part; the crush region must receive the real collision. The replacement now uses the separate current `projects/coarse_crash_lab` project. The dense representation described below is not included in the runtime build.

Independent Linux prototypes, with sleeping disabled, show the following **physics-only feasibility**:

| Prototype workload | Coasting p95 | Impact p95 | Rotated impact p95 |
| --- | ---: | ---: | ---: |
| Four five-section elastic assemblies,20bodies/16connections | 1.219ms | 1.959ms | 2.687ms |
| Four compound aggregates,one body each | 0.437ms | 1.087ms | Not measured in this matched coarse run |

The largest measured coarse-prototype step is9.300ms. These tests omit plastic damage/failure, tyres, drivetrain, scripting, rendering, activation, persistence and streaming. Their frictionless coasting keeps every assembly moving. Six-axis elasticity is implemented only in the isolated prototype; it is not a published JudasJS API. Do not interpret this comparison with the full implicit car as an equivalent game speedup. Earlier finer articulated probes also expose large drift and exceptions; those failures remain in the evidence.

The replacement budget is60Hz authoritative physics, single-car physics p95≤3ms, four active cars physics p95≤10ms, and full-frame p95≤16.67ms. Every maximum and recurring spike must be reported;25ms physics is a spike-review limit, not a promise of uninterrupted60FPS. Measure the complete lab, not only this feasibility fixture, before claiming the target.

An undamaged compound body that activates physical crush sections before impact is a viable optional optimization. First establish the already-cheap active section model. If aggregation is then necessary, activation must precede collision resolution, preserve mass/COM/inertia and linear/angular momentum, handle incoming moving bodies, and avoid double-applying impacts. It must not allocate decorative aftermath after a rigid collision has already consumed the impact. Save, streaming and lifecycle ownership are part of that contract.

The sections below preserve the full implicit research design and its existing candidate APIs for investigation; they do not certify the replacement.

## Why the earlier car was inadequate

The earlier car joined two rigid modules with a single uniformly soft tetrahedral slab. It had no independent front suspension mounts or locally bound front bodywork. Its physical cabin collider did not cover the visible cabin, while its narrow connecting slab represented neither a hollow frame nor distinct crush members. Material substeps ran after the rigid body's full step. Connected material faces were also globally excluded from self-contact. The old passing attachment/disconnection test did not constrain ordinary geometric sag: the preserved baseline moves about 56 mm relative to its cabin before impact.

This does **not** imply tetrahedral materials cannot bend or deform. Judas retains M62/M80's solid material representation for appropriate objects. The new, optional `Structure` representation addresses explicitly braced, hollow assemblies.

## Physical representation

The same Deformable asset/resource/component pipeline now accepts `JudasDeformable4` node/beam structures. Each authored member has an original length, cross-section area and region. Regions distinguish Young modulus, axial relative damping, tension/compression yield and ultimate stress, hardening and plastic shortening/extension capacities. The elastic axial stiffness is `k = E A / L0`. The implicit interval equations combine physical inertia with member stiffness and viscosity; the equivalent multiplier form uses interval-scaled compliance. Internal damping is equal/opposite relative axial damping, preserving uniform translation rather than damping world velocity.

Original node masses integrate half each incident beam's `A L0` and one-third each incident panel's area times authored thickness, then multiply by density and positive mass weights. Shortening or disconnecting members does not delete mass. Plastic work and disposed elastic fracture energy are separate observations. Constitutive state and connectivity belong to each instance, never the shared asset.

The lab uses a real rigid compound cabin with matching visible floor, sills, pillars, roof and rear body. It stops behind the crushable front. Twin front rails have cross-sectional and diagonal bracing; front panels are collision surfaces bound to local nodes. Front wheel carriers are independent finite bodies attached through noncollinear material patches. No unchanged outer collider extends over the crushable nose.

Authoring starts from [JBeam's structural principles](https://documentation.beamng.com/modding/vehicle/intro_jbeam/) and its separation of [stiffness, damping, yield and failure](https://documentation.beamng.com/modding/vehicle/sections/beams/). No BeamNG code, asset or numeric preset is used. Local visual bindings follow the principle described in [flexbodies](https://documentation.beamng.com/modding/vehicle/sections/flexbodies/); Judas uses its own explicit positive node weights and authored seams.

## Integration and ownership

Active structural instances opt into coupled rigid/material intervals using their authored substeps. JS and logical input run once at the authoritative fixed boundary. Gravity samples current physical positions. Wheels and mechanical networks resolve each interval. Structural constraints and predicted material contacts participate in PhysicsWorld's velocity rounds **before** its one authoritative rigid CCD/pose advance for that interval. External forces retain their macro-step duration; impulses are not replayed. Presentation poses, motion history and pair-event publication span the entire fixed step. Worlds with no active structural asset retain the ordinary single rigid-step path; legacy cloth and tetrahedral solids retain their existing material solve.

Each structural block solves member compatibility and finite body pins together. The general path uses a matrix-free Schur operator `J W J^T + A`, where `W` contains node inverse masses and the attached bodies' linear/angular inverse mass response, and `A` contains interval-scaled constitutive compliance. For independent finite pin patches with at most 512 reduced degrees of freedom, an equivalent primal system eliminates the pin constraints: pinned nodes follow the finite body's six linear/angular correction coordinates, and their original mass contributes to the projected mass matrix. The rebuilt primal operator contains the same nodal/body inertia and implicit member tangents; this is a change of linear algebra, not softer material or reduced contact quality. Incompatible overlapping or multi-body pins, singular body inertia and larger systems retain the general Schur path.

The reduced physical root uses the full member Hessian, including force-dependent transverse geometric stiffness. It combines interval-anchored backward-Euler nodal force balance, ordinary body force/moment balance, Fischer–Burmeister normal complementarity and Coulomb disk residuals. The current positive axial matrix is factored once per root block and reused for its right-hand sides as a preconditioner for the analytic Newton/Krylov operator; it does not replace the physical equations. A converged root skips this numerical preparation. Small systems retain their full Arnoldi basis. Larger supported systems use the explicitly bounded restarted policy and report failure rather than accepting an unresolved interval.

Static analytic box, sphere and capsule contacts evaluate the finite minimum separation over the same straight relative material path used by discovery. Its endpoint gap derivative includes the minimizing path fraction; curved normals have analytic derivatives and the isotropic tangent frame is transported consistently. Existing skin overlap uses the exact initial convex supporting plane, because a path minimum cannot repair its frozen past endpoint. Solid-intersecting or singular acquired paths retain the recovery plane. This correction preserves thin-wall crossings and finite escape; dynamic, kinematic, cooked and self contacts retain the prior sampled-plane treatment. Exact feature ties select a semismooth geometric branch rather than promising a unique derivative.

Native rigid/contact coupling rounds and numerical material-root iterations have distinct responsibilities. Within each native callback, the reduced root closes at the same fixed interval and frozen discovered rows. It replays no gravity, forces, time advance or irreversible history. Completion uses the existing physical/contact certificate; an explicit `min(4096, 2*n+8)` numerical work guard rejects unresolved work. The general Schur compatibility fallback retains its existing single-call contract. The isolated rotated/displaced steel fixture proves why one nonlinear proposal per native callback was insufficient; the current full-car evidence still needs separate validation.

Piecewise elastic/plastic/capacity events are resolved on actual geometric trial lines. An optional internally globalized, frozen-geometry material predictor retains the same return map and affine branch intercept; its proposal must still pass the actual physical residual and directional-descent checks. Materials, native coupling rounds and physics cadence have not been softened to conceal failed roots. Irreversible plastic/failure history is accepted once at interval completion.

Finite pins are dependent coordinates derived from the same candidate native body motion throughout the root, rather than separately corrected nodes. Smooth double trajectory evaluation removes intermediate pose-bin discontinuities while preserving ordinary native impulse and rigid integration ownership. Requested-versus-measured impulse/moment remainders exist only within the current interval; they are neither a second body integrator nor saved hidden continuation state. Completion checks the actual native endpoint motion and pin geometry before publishing material history. A late body impulse or shared-body change rejects stale completion, including when float world position did not change.

**Working candidate status:** the original [full-car failure](evidence/m83-structural-corrective/shared-solve-v3/car-first/low/runtime.log) and subsequent failed numerical probes remain preserved. Small material, contact, mixed-body and lifetime contracts have improved, but the full driving/crash/strength/rotation, damaged continuation, export and performance matrix is still pending. See [current status](evidence/m83-structural-corrective/CURRENT_STATUS.md). Successful startup or mathematical unit tests do not establish a convincing crash lab.

Finite attached bodies remain ordinary PhysicsWorld bodies. Material contact uses the existing geometric primitive/query pipeline, respects layers, and optionally excludes only the pinned local patch from its attached body. Relevant nonadjacent connected faces can contact during folding. Topology-local intact seam exclusions prevent initial duplicate fracture surfaces from repelling each other; caches invalidate when topology changes.

Mapped mounts physically change carrier orientation and the wheel's actual suspension/contact frame. Powertrain continuity resolves through active material patches and surviving member connectivity. Broken paths stop torque without changing the held JS command. No damage health, steering penalty, canned dent or collapse sequence owns the outcome.

## Authoring

Open `projects/structural_crash_lab/structural_crash_lab.judasproj`. The reusable car prefab and independent nonvehicle specimen use the normal asset database, scene and prefab mechanisms. `tools/CreateStructuralCrashLab.py` regenerates original content and invokes the ordinary baker. It is not a runtime simulation branch.

A `JudasStructuralSource 1` text source has SI-valued `node`, `region`, `beam`, `surface`, `group` and `end` records; see the complete original `.source` files under the project's `Assets/deformables/`. The baker takes the output before the source:

```sh
build/judas_deformable_bake structural-network OUTPUT.judasdeform INPUT.source
```

In the editor, add a Deformable, import the braced network source, inspect/edit individual region values and preview nodes/physical surfaces/groups. Material edits rebake the shared asset: all instances use the new authoring and old saved fingerprints cease to match. Author intentional panel seams and local support lists; unbraced axial bars cannot provide bending stiffness by themselves. Avoid yield strain near `yield/E = 1`, which would deliberately author an elastically rope-like region.

The Deformable inspector exposes density, contact thickness/filtering, self-contact, attached-body contact and solve quality. Solid compliance/isochoric plastic-flow controls remain for legacy solids; structural regions own this representation's constitutive settings. The ordinary Debug menu adds physical material and wheel/contact frames. Scripts may toggle the same read-only presentation through `world.debugView`; see [JudasJS entities](judasjs/entities.md) and [deformables](judasjs/deformables.md).

## Persistence and boundaries

New structural worlds use deformables save participant 5 and retained state `Judas.DeformableState.3`. The immutable cook fingerprint, node positions/velocities, plastic rest, failure/connectivity, attachment state and dissipated-work observations survive save and region retention. Older cloth/solid/fracture asset and state layouts remain readable. Structural state cannot silently use a legacy layout lacking its continuation data. Resetting irreversible structure requires normal authored scene reload.

The central cabin is intentionally rigid. There is no arbitrary mesh cutting, industrial shell FEM, fatigue, full-car homologation or BeamNG vehicle compatibility. Collision is bounded material-point/edge/face sampling with swept point casts; it is not general exact fast rotating soft-body CCD, nor an expansion of M44/M42/CharacterMotor to material surfaces. Finite partner overlap and timestep dependence must be measured rather than concealed. These limits are separate from the required demonstrated road-speed cases.

Predictive structural contacts use the same sampled geometric discovery to add condensed unilateral/friction rows to the member/body system. This contact path currently requires independent finite pin patches and at most **512 reduced degrees of freedom, including finite contact partners**. If discovered contact requires the larger/incompatible general Schur route, the instance stops with an explicit unsupported-contact error; it does not silently omit contact or soften the structure. Contact-free larger graphs can still use the general route.

Rigid CCD or penetration resolution can change the actual endpoint after the predictive structural rounds. The native `maximumPinDrift` observation compares material pins with the **actual** resolved body transform before accepting material history. It is a diagnostic, not a positional repair or proof of compatibility; final evidence must report it. Same-instance self-contact participates in the condensed system. Structure-to-structure pair projections run inside the material rounds, but they do not share one global contact operator across both instances. Mixed contacts with legacy cloth/solid instances still use their existing post-material pair projection. Neither pair path is certified as one shared implicit structural contact solve.

## Evidence and review

[Declared checks](evidence/m83-structural-corrective/DECLARED_CONTRACTS.md), original baseline views, preserved failures and numerical/application results live under `docs/evidence/m83-structural-corrective/`. Results are recorded after actual execution; this architectural note alone is not validation evidence. Windows remains informational and nonblocking.
