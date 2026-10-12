# Warm-seed-dependent predictive limit topology: diagnostic only

An independent single-hinge fixture keeps its pose, mass/inertia, pre-solve velocity, authored limits and 60 Hz timestep identical. Enabling one anchor warm seed changes the current solver's generated row count from six to five because preparation applies seeds before predicting limit crossing. A copied assemble-then-warm variant generates six rows in both cases. The original and copied-source traces and identities are adjacent.

This is not retained as a production correction: the M77 boundary has no active limit rows, and removing this preparation difference has not been established as a causal repair. ContactSolver applies its own warm impulses before joint preparation, so merely deferring joint seeds would not by itself establish cache-independent prediction for general contact scenes. No M77 threshold, physics cadence or acceptance test changed.

Build/reproduce from the repository root:

```sh
c++ -O3 -DNDEBUG -std=c++17 -I src .cache/m78-cold-boundary/root-row-topology/original.cpp .cache/m78-cold-boundary/root-row-topology/probe.cpp -o .cache/m78-cold-boundary/root-row-topology/original
c++ -O3 -DNDEBUG -std=c++17 -I src docs/evidence/m78-cold-boundary/root-row-topology/deferred.cpp docs/evidence/m78-cold-boundary/root-row-topology/probe.cpp -o .cache/m78-cold-boundary/root-row-topology/deferred
.cache/m78-cold-boundary/root-row-topology/original
.cache/m78-cold-boundary/root-row-topology/deferred
```

The original source identity is recorded in `IDENTITY.json`; its exact implementation remains in the frozen candidate and is copied only under `.cache/`. The observer exposes copied solver rows for diagnostics; it adds no public API.
