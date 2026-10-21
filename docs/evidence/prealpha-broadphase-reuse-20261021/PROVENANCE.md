# Broadphase candidate-reuse follow-up evidence

Baseline: accepted `80f7b2a5f96cdf1d56c5c88043c16edefe70a4c3`.
This package contains original source-only prototypes, raw records and selected
current/frozen source snapshots. Folder names never certify test status.

Normal profile-off, sampler-disabled 900/1800/live35 runs are the direct timing
reference. CPU-PC samples, existing-profiler frame capture and instrumented
candidate-demand runs are diagnostic: preserve them separately and do not use
their timing as the normal performance comparison. Sampling reports direct main
thread software CPU-PC occupancy, not hardware cycles, stacks or worker/GPU CPU.

Raw profiler inclusive scopes overlap. Worker queue waits can begin in older
frames and finish here; they are not CPU cost. Existing stage captures preserve
thread IDs, parent/depth, origin/publication frames, fixed boundaries and all
drop/incomplete diagnostics. Snapshots are read after publication, never by
prematurely ending an engine frame. Main/worker elapsed spans are not additive.

The retained production change is only guarded reuse of the already-discovered
ordinary candidate set when wake-up/integration cannot have extended coverage.
The unconditional-reuse negative control, rejected empty-manifold and prepared-
pose experiments remain exact source/evidence, not production changes or claimed
additional speedups. Check final source/test inventories for the actual candidate.

Frame intervals belong to the preceding frame's work. Normal table tools match
frame IDs to every actual fixed step, compare full common-prefix physical/work
fields, and retain strict 0–10s,10–20s,20s–stop windows. Ordinary observation
timestamps do not provide exact fixed-step boundary clipping across wall windows.
Synthetic cadence is throughput, never displayed-FPS evidence. Live endpoints
after different numbers of steps must not be called a matched physical endpoint.

The frozen core exactly matched the prior accepted final binary inventory.
Runtime and normal benchmark were subsequently relinked with clean80f build
identity; both earlier and current hashes remain recorded rather than relabelled
byte-identical. The initial private sampler builder's wrong glad include failure
is retained; independent v2/stage build outputs retain their successful receipts.

CSV/JSONL files and JSON/log records larger than64KiB are losslessly stored as
deterministic gzip(mtime0,no filename).
The manifest records original/stored size+SHA256; all other records are exact byte
copies. Binaries, objects, static/shared libraries, source archives and temporary
assets are excluded. Their byte identities remain in original run/build receipts.
Only selected relevant source files are copied; baseline FROZEN.json retains the
complete516-source input inventory. Existing historical reports/evidence are not
modified. Missing final roots remain explicitly recorded, never silently passed.
