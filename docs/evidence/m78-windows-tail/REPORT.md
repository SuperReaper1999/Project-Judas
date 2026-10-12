# Windows VM tail follow-up

Two additional executions of the exact original native benchmark completed before the operator clarified that the VM is for functional validation, and Linux host measurements govern performance acceptance. Further timing repeats, phase instrumentation and performance changes were stopped. The third prepared repeat was never launched.

The source, 900 fixed steps at 60 Hz, four 13-body articulations, 8,192-triangle floor, contacts and CCD policy are unchanged. Both repeats preserve identical aggregate contact/CCD/sleep counters to the original native run. Mesh-four p95 was 11.6577 / 13.1317 ms; maximum was 72.2031 / 76.7884 ms, both at step 89. The original maximum was 56.376 ms at step 85. Step 85 repeated at 60.2216 / 59.2907 ms. The native measurements are descriptive, not a Windows hardware or performance certification.

Existing `solver_ms` includes continuous-impact scheduling/search and event solves, so it does not isolate linear constraint solving. No per-step thread CPU, clock-quantization calibration or CCD-subphase instrumentation was run after the operator's clarification. These measurements alone cannot separate computational cost from VM scheduling.

See `ORIGINAL_REPEATS.json`, the two original stdout/CSV/profile sets and exact input identities. The final production archive and original concave executable were reused without recompilation. Old evidence/fingerprints were not overwritten.

Disk was initially nearly full (11.5–12.9 MB free). Only independently host-archived and freshly byte-count/SHA-verified owned generated runtime/diagnostic artifacts were removed. Final production library, original concave executable, moved package, sources and raw results were preserved. Disk recovery receipts document the exact paths and hashes.
