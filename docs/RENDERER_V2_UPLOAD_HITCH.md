# Renderer V2 stream retirement correction

5 November 2026. Experimental engineering qualification; not publication or
human acceptance. Main, the RPG SDK selection and the Alpha fluid lock remain
unchanged.

## Cause and correction

The radial fluid comparison isolated a recurrent GPU interval spike in the
persistent upload path. A per-page trace then showed every late-frame spike
coinciding with ring-page retirement. Mapped copies took about 0.3–0.5 microseconds;
actual CPU fence waits were about 1–2 microseconds. Neither explained hundreds
of microseconds of extra GPU timeline duration.

The costly operation was the **GLsync retirement mechanism on this driver**, not
the fluid solver, the volume of upload data or a long CPU wait for unfinished
work. The corrected arena retains the same persistent coherent storage, page
count, allocation bounds, alignment and draw order. It retires preceding GPU
reads using private `GL_TIMESTAMP` completion queries instead of inserting a
client-visible sync fence at each turnover.

This relies on the completion semantics in the
[Khronos timer-query specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_timer_query.txt):
query availability follows realization of preceding commands. It does not use
the numeric timestamp or borrow optional profiler queries. This is specific to
the arena's CPU-upload/GPU-read ownership, not a recommendation to replace
compute/readback visibility fences everywhere. NVIDIA's internal reason for the
extra GLsync cost was not reverse-engineered.

Unavailable timestamp support/precision selects the existing exact orphan-upload
path. A genuinely pending marker is flushed and polled with bounded CPU backoff
and the existing one-second deadline. Timeout/failure cannot advance the ring or
publish a slice. Query-buffer bindings are preserved during CPU status reads.
Markers and replacement allocations are released on their existing ownership
paths; failed growth retains old storage and completion dependencies.

## Rejected experiments

Original sources and raw failures remain under `.cache/v2-upload-hitch/`:

| Experiment | Result |
|---|---|
| Noncoherent mapping with explicit range flush | Periodic spikes remain |
| One allocation with three fenced subregions | Periodic spikes remain |
| Separate modern vertex format/buffer binding | Spikes remain; not retained |
| Persistent staging plus draw-only GPU buffers | Spikes remain and typical GPU cost increases |
| Zero-time client fence check before blocking | Spikes remain; original timeout-call-count control also fails under the changed polling contract |

None is in the final implementation. One early prototype timing block overlapped
an isolated control-binary compilation; it is diagnostic, not accepted performance
evidence. Some initial image comparisons also failed despite equal physical and
uploaded geometry hashes. Their camera matrices were not captured, so their cause
is not asserted. The accepted comparisons neutralize desktop input, record exact
view/projection matrices, and require exact pixels. No pixel tolerance was relaxed.

## Matched results

Native GTX 970 4 GiB, NVIDIA 580.178.04, Ryzen 5 5600. No concurrent compilation
or benchmarks during the accepted comparisons. Before controls differ only in
the linked `StreamArena` implementation; the rest of the renderer is identical.

Original radial scene, 1280×720, 380 fluid samples, 16 bodies, unchanged fixed
1/60-second frames and 20 Hz water updates. Each process runs 360 frames, excluding
the first 60 from timing. The input-isolated ABBA block gives:

| Path | GPU median | GPU p99 | Worst | Samples over 0.5 ms |
|---|---:|---:|---:|---:|
| Old retirement, first process | 0.375 ms | 0.742 ms | 0.755 ms | 14/300 |
| Corrected, first process | 0.375 ms | 0.385 ms | 0.390 ms | 0/300 |
| Corrected, second process | 0.375 ms | 0.384 ms | 0.387 ms | 0/300 |
| Old retirement, final process | 0.375 ms | 0.740 ms | 0.751 ms | 14/300 |

After capability/failure hardening, final-source radial confirmation measured
GPU p99 **0.750 → 0.384 ms**, worst **0.767 → 0.386 ms**, and **15 → 0** samples
over 0.5 ms. These are main-camera timestamp intervals, including submission
bubbles, not independent shader cycle counts or a real-time FPS guarantee.

All accepted radial, flat-pool and HDR-pool comparisons retain identical physical
states, final uploaded water geometry, camera matrices and byte-identical PNGs.
No fluid/physics setting, particle count, shader, shadow quality or default
streaming feature was disabled to obtain the result.

Representative non-fluid checks use 180 measured frames after 60 warm-up frames,
with two views, also in ABBA order:

| Workload | CPU submission medians, old → corrected | GPU medians, old → corrected |
|---|---|---|
| 4,096 repeated objects | 2.343/2.332 → 2.362/2.357 ms | 2.639/2.454 → 2.540/2.383 ms |
| 100,000 particles | 11.056/11.049 → 10.780/10.776 ms | 11.163/11.174 → 10.898/10.877 ms |

The repeated-object CPU difference is roughly 1%, not a meaningful claimed
improvement or regression. All canonical captures match exactly; completed GPU
samples and geometry/count acceptance conditions remain required.

## Correctness and boundaries

- NVIDIA modern: **57/57** stream controls, including queued GPU draw lifetimes,
  wraps, growth, exact readback, failed allocations and repeated timeout retries.
- NVIDIA portable: **32/32**, with advanced persistent tests explicitly skipped.
- Mesa llvmpipe modern: **57/57**, correctness only, not performance evidence.
- Added controls for unsupported timestamp precision, retirement-marker creation
  failure, preservation of unrelated query-buffer bindings and marker cleanup.
  The two-upload retry and queued-payload assertions remain; the timeout mock now
  targets query availability rather than the retired sync API.
- Normal runtime/editor binaries rebuilt; the installed RPG SDK was not switched.
- Windows/Android were not run. No whole-engine performance or fluid-unlock
  certification is implied. CPU water meshing and the fluid correctness backlog
  are independent of this correction.

See [machine-readable qualification](evidence/v2-upload-hitch/qualification.json).
Complete rejected prototypes, binaries, trace and logs are retained in
`.cache/v2-upload-hitch/`. The preceding
[fluid comparison](FLUID_RENDER_V2_COMPARISON.md) remains frozen before-fix evidence.
