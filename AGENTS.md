# Project Judas branch workflow

## Development and checkpointing

- Use `Experimental` for ordinary development. Check the current branch and worktree before making changes; preserve unrelated local work.
- After appropriate automated validation, implementation tasks may be committed and pushed to `Experimental` without waiting for human testing or another checkpoint authorization. Report the commit hash, checks and remaining limitations.
- Explicit task instructions to leave work uncommitted, avoid pushing, or perform read-only work override this default permission.
- Do not claim human validation from automated tests, synthetic input, source inspection or a successful build.

## Main release promotion

- `main` is the human-tested release branch and requires explicit operator acceptance of the exact candidate. Never promote automatically from an Experimental push or an automated pass.
- Preserve a reviewed candidate commit when promoting; do not manufacture an untested intermediate state. Fast-forward `main` from the accepted Experimental checkpoint. If main has diverged, stop and establish the actual integration state before promotion.
- Push normally. No force pushes, history rewriting or milestone tags unless specifically authorized.
- The release lineage begins at `3102bf6c6e4b5c57eb3ab66a71fa502f8fc423f6`, expressly designated by the operator. After M82 acceptance, the operator retired the duplicate `Stable` branch and made `main` the release/default branch. This workflow change does not retroactively certify outstanding milestone, Windows or performance validation.

## Repository continuity

- The two primary branches are `main` for accepted releases and `Experimental` for development. Keep development on `Experimental`; do not update `main` automatically or promote unreviewed implementation through it.
- Preserve protected historical evidence, independent research and unrelated assets. Document actual failures and limitations rather than weakening fixtures to obtain a pass.
- GitHub protects main against force pushes and deletion, including administrator actions. Human-test acceptance is an operator workflow requirement; GitHub does not independently prove that testing occurred.

## Real-time performance qualification

- Performance-relevant milestones must include measured whole-game evidence, not only functional tests or isolated subsystem timings. Reuse valid evidence for unchanged systems and run representative affected workloads plus a combined gameplay workload.
- Reference gameplay budgets are 120 FPS / 8.33 ms target, 60 FPS / 16.67 ms minimum in explicitly defined workloads, and an aggressive 5 ms combined-engine critical-path goal. These are not per-subsystem allowances or promises for arbitrary scenes.
- Record exact executable/source/input identities, hardware, resolution, rendering policy, VSync/cap settings, p50/p95/p99/max, exceptional hitch sequences, memory and any discarded simulation backlog. Separate loading, steady gameplay, exceptional impacts and editor overhead.
- CPU and GPU work overlap. Do not add their durations or nested scopes as frame time. Increased worker utilisation is useful only when whole-frame latency improves without reducing fidelity or changing authoritative ordering.
- Use the reproducible capture and relative-regression tools described in `docs/WHOLE_ENGINE_PERFORMANCE.md`. Inspect actual rendered content and simulation counts: a fast empty, stale, off-map or backgrounded scene is not qualification.
- Keep negative experiments and unresolved budget failures explicit. A passing relative p99 regression gate does not certify absolute performance, platform coverage or human acceptance. Experimental checkpoints may remain explicitly unqualified; do not present them as accepted releases.
