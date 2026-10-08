# Human-reproduced passive follow-up crash

The operator reported SIGABRT for the corrected desktop package:

- PID: 116230; 2026-10-08 20:29:04 BST.
- Executable: `/tmp/judas-m70-character-lab-20261008/judas`.
- Transient user unit: `judas-m70-passive-retest-20261008.service`.
- Core: `/var/lib/systemd/coredump/core.judas.1000.b4a9588bf1e24aca8d0a7a970d7faecc.116230.1791487744000000.zst`.
- Reported executable offset: `0x4c9a96`.

The operator identified **G** and then clarified that the game locked up when the ragdoll hit the floor; it was force-closed afterward. This is not counted as acceptance; the prior timing/state-equivalence checks do not prove this failing interactive trajectory.

Core signal metadata confirms externally delivered SIGABRT (`si_code=0`, sender PID 116928), while the main thread was executing CCD. The event/motion-history growth, rather than an uncaught exception, is the investigation target.
