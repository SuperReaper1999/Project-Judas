# M87 operator acceptance — 15 October 2026

Accepted implementation: `9d82e42a4783fc94607b5fe50157ee16382bb510`
(`Improve M87 contact throughput and record dense-pile limits`), including the
original M87 implementation at `cee55a898aa0e4938665903f3b207d828acdb1df`.

The operator reviewed the updated Mechanical Arcade on the Linux desktop from
`/tmp/judas-m87-watch-updated-dvcqa4b_/judas` and stated:

> yeah that passes for now push to stable and mark it as a known limit its very likely my old hardware

This is explicit human acceptance of the current candidate for release with the
known performance limit, and authorization to promote it to `main`, the stable
release branch. It is not inferred from automation. No individual checklist
outcome, sustained large-pile acceptance or additional platform result is invented.
The temporary launch copy enabled the existing watch tour at startup; engine
binaries, physics, scene population and simulation settings were unchanged.

## Retained known limits

- The 1,024-piece mixed box/sphere pile (1,152 total dynamic bodies including
  machinery) can develop a low-speed contact/continuous-impact cascade and stall
  input processing for seconds. It remains an engine scalability limit, not a
  sustained 60 Hz workload. The default room was measured separately.
- The host uses an i7-4790 and GTX 970. Hardware may affect the measured costs,
  but no equivalent cross-hardware comparison establishes old hardware as the
  cause. The measured repeated collision work remains documented; no quality,
  cadence, population or collision fidelity was reduced to obtain acceptance.
- Windows M87 validation remains unperformed and nonblocking.
- The preserved foundation participant-limit expectation failure, mesh-cost
  crossing results and unjointed contact-pile cold-continuation limitation remain
  as recorded in the [follow-up report](live-slowdown-followup/REPORT.md).

## Checkpoint and preservation

Before promotion, all 51 recorded final source/content fingerprints, 36 built
binary fingerprints and the 404 entries in the final evidence manifest were
verified. The launched runtime matches the recorded Release executable. Source,
project scripts/assets, tests and binaries remain the exact accepted candidate.

This checkpoint changes current release-status documentation and adds this
acceptance record only. Original reports, failures, raw measurements and
fingerprint manifests remain byte-for-byte unchanged. This record supersedes
their pending-human-review status for M87 Linux acceptance only; it does not
convert known failures into passes or certify unrelated historical work.

Promotion preserves the tested implementation commit and fast-forwards `main`
through this documentation-only acceptance checkpoint. Development remains on
`Experimental`. No force push, history rewrite or milestone tag is created.
