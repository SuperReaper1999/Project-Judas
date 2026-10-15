# Raw evidence index

- [CASE_SUMMARY.csv](CASE_SUMMARY.csv): all retained live-loop cases, profiler mode, step counts, median/p95/max/sum, event counts and physical-digest comparison.
- [CASE_SUMMARY.json](CASE_SUMMARY.json): same values plus parsed run settings.
- `live-cases/<case>/`: deterministic gzip of original steps/frames/begins CSV; original run log unmodified.
- [PROFILE_INDEX.json](PROFILE_INDEX.json): selected M56 captures and their collection diagnostics.
- `profiles/original-human-overflow.json.gz`: original incomplete profiler capture; 115452 dropped events, 50504 truncated records.
- `profiles/dense-fixed-tight-query.json.gz`: bounded later zero-overflow capture; separate from profiler-off timings.
- Other selected profiles preserve the genuine lazy/cache experiments and ordinary 128-body watch run.
- `../experiments/impact-quality/`: frozen single-island raw input, exact endpoint proof, residual CSV and isolated solver timings.
- [Copied artifact manifest](../provenance/COPIED_ARTIFACT_MANIFEST.json): original/decompressed/stored SHA256 identities and original source locations. Gzip headers use mtime 0 and no filename.

Inclusive CCD geometry timers overlap query/discovery timers. Do not sum overlapping categories. Short 155-step cases cover about 2.58 simulated seconds; they are not sustained 60 Hz certification. Compare profiler-off cases separately from profile-on/unknown early captures.
