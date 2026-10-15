# Isolated persistent CCD-deadline prototype

This directory contains an optional source-level experiment. Shared `src/PhysicsWorld.cpp`, its build object and `libjudas_engine.a` were not modified by preparing or compiling this variant.

## Mechanism

Each scheduled pair retains its original query start, full fixed-step finish, target mode and two angular-speed precheck inputs. A separated centered box/sphere pair may obtain an inclusive geometric absence certificate over a prefix. Its queued time is the next representable double after that certificate's endpoint, a strict lower bound on any actual TOI returned by the original algorithm.

Only the globally earliest queued item is processed. A pending item extends its proven prefix by doubling the original-start span. If the existing conservative sweep AABBs remain disjoint, its lower bound advances; reaching the fixed-step endpoint retires a known miss. If the certificate fails, the unchanged original TOI search runs using its ORIGINAL full interval, sampling grid, target and frozen mutable angular-speed prechecks. No physics time advances merely to refine a certificate.

Initial prefix length is suggested by half the actual separated AABB axis gap divided by relative linear speed plus angular-radius speed. This is an efficiency hint only. Every accepted deadline must pass the existing outward conservative interval-motion enclosure. A failed/unsupported certificate falls back to the original search. Touching/overlapping actual AABBs run the original search immediately; penetration-target spin contacts are not hidden.

Trajectory changes invalidate and reschedule exactly the same incident pairs as before. Unchanged pairs keep their own certificate deadlines instead of being recertified through every unrelated physical event.

## Ordering and retained storage

The min-heap orders by the original `(time, primitive-pair key)` tuple. Revision numbers discard stale queue nodes. A pending deadline cannot conceal an earlier real event because its inclusive prefix excludes all actual contact. When a deadline wins, it must be refined before a later physical event can run. Simultaneous physical events retain key ordering.

Heap storage is compacted from current scheduled entries when stale/live growth exceeds three times the scheduled-pair count plus256. This bounds derived queue storage without discarding any live query. Peak-node and actual vector-capacity byte counters are recorded. The byte counter describes retained heap capacity, not the additional temporary live-entry vector during a rebuild; that temporary vector is at most one entry per scheduled pair.

Two invariants throw instead of hiding a defect: a deferred full query must not return before the current physical event time, or at/before a previously certified absence endpoint.

## Private profiler counters

- Physics impact deadline calculations
- Physics impact deadline refinements
- Physics impact deadline retirements
- Physics impact deadline full queries
- Physics impact pending deadlines
- Physics impact deadline heap rebuilds
- Physics impact deadline heap peak nodes
- Physics impact deadline heap peak bytes

These are diagnostics, with no new public binding or physical configuration.

## Build and validation state

`python3 /tmp/m87-persistent-lazy/build.py` compiles one isolated TU and links it ahead of the existing engine archive into `live`. `build.log` records exact commands. No shared archive is rewritten. The source base and variant hashes are in `provenance.json`; `persistent-lazy.diff` shows the experiment against its exact frozen base.

Compilation/linking passed without warnings. Numerical equivalence and performance remain for the parent's controlled identical-input155-step comparison. This document does not claim a runtime pass or speedup.
