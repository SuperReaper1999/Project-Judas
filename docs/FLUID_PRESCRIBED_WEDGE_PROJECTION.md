# Prescribed particle contacts in a narrow moving wedge

The [original 40 m/s reproduction and failure](FLUID_CLEARANCE_REPAIR_AND_GRID_PROTOTYPE.txt)
are retained. Its three velocity rows are compatible. Two rows bind at the
minimum-change velocity and the floor is slack by about 17.27 m/s. Projected
Gauss–Seidel on the two active normals contracts the residual by about 0.93604
per sweep. The existing 64-sweep budget leaves about 0.05106 m/s of closing
speed. Native acceleration already declines moving prescribed normal endpoints
to avoid accepting arbitrarily large work from a nearly opposed pair.

Both the direct and native paths now attempt a small certified solve when their
normal-speed residual is still too large. For a single particle with only
frictionless prescribed contacts, minimize the squared velocity change subject
to every original row, including its original gap target. In three velocity
dimensions, a feasible minimum has a certificate with at most three independent
active normals. The solver enumerates those subsets, checks nonnegative
impulses and feasibility/complementarity at the represented float velocity,
and publishes the closest certified result.

An independent group of two to eight rows is eligible. The accepted correction
must be within the largest wall speed relative to the incoming particle, up
to float impulse roundoff. This sufficient bound avoids escalating the
prescribed-wall energy risk. Infeasible, ill-conditioned, or excessive-work
groups retain their bounded response and diagnostic. Native batches with
finite-body coupling, ordinary support rows, or unordered particle rows retain
the existing path. The normal targets, friction law, 64-sweep limit, finite
body responses and contact geometry are unchanged. The native iteration count
continues to count the point-solver sweeps. The cap flag still describes an
unresolved residual in the published response.

## Evidence

| Original unchanged wedge geometry | Before | Corrected |
| --- | ---: | ---: |
| Aligned, native and direct physical residual | 0.0510578 m/s | 0.00000190735 m/s |
| Rotated, native and direct physical residual | 0.0510654 m/s | 0 m/s |

A separate decimal calculation of the recorded two-plane intersection gives a
25.5468 m/s minimum correction against a 39.9990 m/s incoming-to-wall speed.
The test checks the independent geometry residual and velocity oracle as well
as closing momentum with the published impulses. It covers both collider
orders and orientations. The existing incompatible and narrow high-amplification
batch cases retain their cap reports and bounded response.

Focused Release checks on Linux passed 545/545 clearance assertions and 77/77
particle contact batch assertions. These runs linked the exact changed source
objects ahead of a pinned engine archive to isolate this change. Run the same
checks from a normal repository build with the `judas_fluid_contact_clearance_tests`
and `judas_particle_contact_batch_tests` targets.

This qualifies the retained prescribed-wedge reproduction. It leaves active
fluid qualification, moving curved-contact chronology, finite-rigid coupling
limits and zero-gravity startup work separate.

The normal combined Linux build reproduced 545/545 and 77/77. The strict
production run recorded 13,801 checks, zero mandatory failures, and the
same five diagnostics as the previous clearance checkpoint. Across all 11
production cases, 4,560 trajectory CSV rows match the previous checkpoint
exactly when excluding the two timing columns. Project Fluid Policy passed
26/26. Basic Water passed 45/45 plus a separate cold-process save cycle.
