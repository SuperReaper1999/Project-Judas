# M92 baseline provenance

Baseline: `61a9df6acaa102c0a9fcf1006866a01fc3b33a89`, the exact M91 candidate
operator-tested and explicitly promoted to main immediately before M92.
That promotion did not alter the preserved M91 raw evidence or its older pending
review statement. Main is not changed by this M92 development batch.

Initial native render samples precede the final observer's process-CPU additions.
They remain diagnostic receipts. The final paired comparisons use identical
observer source compiled against frozen baseline headers/archive versus current
headers/archive. Frozen headers are recovered from the baseline Git object;
current layout-changing headers are never used to allocate baseline EngineHost.

A separate Afterglow Judas process (PID 128249) was observed using about four CPU
cores. The operator initially requested preserving it, then explicitly authorized
closing it before final comparisons. Only that exact executable/PID received a
close signal; it exited. Final measurements record process/GPU observations before
and after each run, alternate baseline/candidate order, and run after compilation.
Ordinary desktop applications remain. GPU timestamps include command-stream gaps
and are not pure GPU utilization. Process CPU time supplements wall submission time.
