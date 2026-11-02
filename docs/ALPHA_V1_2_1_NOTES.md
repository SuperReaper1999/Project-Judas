# Judas Engine Alpha v1.2.1

Machine version **1.2.1-alpha.1**. Alpha, not Stable/GA. Candidate prepared for
operator-authorized publication after exact-source qualification; the release
manifest identifies the shipped commit and package hashes. Previous releases
remain immutable.

## Visible corrections

- Shadow comparison filtering now interpolates continuously across depth texels,
  avoiding the abrupt block-pattern transitions seen on pool walls and objects.
  Shadow resolution, receiver bias and depth-map ownership are unchanged. This
  uses 16 rather than 9 depth fetches; it is a quality correction, not a claimed
  GPU speedup. [Evidence](SHADOW_FILTER_CONTINUITY.txt).
- Optional `JUDAS_GPU_DIAGNOSTICS=1` enables bounded asynchronous graphics debug
  messages and available reset diagnostics. It is disabled by default. It does
  not establish a cause or fix for the previously observed desktop/driver reset.
  [Usage and limits](GPU_DIAGNOSTICS.txt).

## Advanced-fluid repairs included, feature still locked

**Advanced Fluid Dynamics remains disabled end-to-end in Alpha.** No project
setting, script or exported game may bypass that policy. Basic Water rendering
and swimming remain independent and available. This patch does not unlock fluids
or close the outstanding fluid qualification issue.

The included implementation repairs actual wet-support admission, finite-height
solid displacement, retained water boundaries, exact-centre initial overlaps,
rigid/fluid motion-clock conversion and contact-episode response. Optional contact
clearance no longer manufactures a simultaneous hard collision and false rigid
reaction. Real incoming impacts retain their physical response. No artificial
energy clamp, added damping or relaxed acceptance threshold was used.

The unchanged rectangular-pool recipe retained 1,440 particles, 180,000 kg and
zero escaped particles. Its published peak particle speed changed from 41.7176
to 9.6519 m/s. Linux focused contact checks passed 513 assertions. These scoped
results are not a universal stability or performance certificate.

[Contact repair and complete limitations](FLUID_CLEARANCE_REPAIR_AND_GRID_PROTOTYPE.txt),
[motion-clock evidence](FLUID_MOTION_CLOCK_REPAIR.txt),
[local wet reconstruction](FLUID_LOCAL_WET_RECONSTRUCTION.txt) and
[finite support](FLUID_FINITE_SUPPORT_REPAIR.md) preserve original failures and
reproduction details. Archived raw evidence remains unchanged.

A standalone bounded volume-grid research prototype is retained in the source
history. It is not an integrated engine backend or supported runtime option and
must not be interpreted as a replacement fluid system.

## Qualification and known limits

Release preparation reruns exact-source Linux qualification, SDK project creation,
cold saves and relocated exports, plus native hosted Windows contracts and SDK
checks. The preparation run and publication manifest provide final results;
previous local checks do not substitute for those results. Hosted Windows checks
are not fresh physical hardware/controller/audio acceptance. Android and browser
hardware are not newly qualified by this desktop patch.

Advanced-fluid production/player diagnostic warmup and settling failures remain
recorded, including additional coarse warmup warnings. A narrow prescribed fast
wedge can exhaust the existing contact solver budget with residual error. Other
legacy scenes still exhibit high particle-speed peaks; active-fluid suspension,
origin shifting and cold-save combinations retain their documented restrictions.
The rectangular pool remains expensive. No whole-engine performance improvement,
general conservative pressure backend or universally stable fluid system is claimed.

Existing Alpha platform and compatibility boundaries remain in the
[support matrix](ALPHA_SUPPORT_MATRIX.md). Native SDK consumers must rebuild
against matching headers/libraries: internal fluid contact records changed.
No new JudasJS API or save-format migration is introduced by this patch.
