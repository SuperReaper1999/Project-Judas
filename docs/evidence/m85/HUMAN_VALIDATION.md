# M85 operator acceptance — 15 October 2026

Accepted candidate: `2ea9587b2fbc734cbe2ec906ba344cd6d9ae2a67`, including
M85 terrain at `b9bd655485fe07182e68b9c78bd3d4987fba2985` and Claude PR #15.

The operator ran `/tmp/judas-m85-final-package/judas` on the Linux desktop and
stated: "confirmed it works beautifully push to main with claudes portal fix thing".
This is the operator's M85 acceptance and release authorization, not an inference
from automated validation. No individual checklist outcome or separate human
playthrough of Claude's portal game is invented. PR #15's camera capabilities
have the focused automated review recorded in `docs/evidence/pr15/`.

The operator also reported reversed horizontal mouse look. It is a sign error
in the lab's project JavaScript (`yaw -= look_x` against a positive-right basis),
not an engine input defect. It remains a known content issue in this exact
accepted implementation. A separate correction may be developed on Experimental;
it is not silently included in this release or claimed human-tested.

Before recording acceptance, all 71 combined source and nine built-binary hashes
matched the PR #15 final fingerprints; the launched executable matched too.
This acceptance checkpoint changes current status/documentation only. Engine,
project scripts/assets, tests, protected history and fluid research are unchanged.
Original candidate receipts/manifests retain their original pending-review status;
this record supersedes that status for M85 Linux acceptance only.

Windows M85 validation remains unperformed and nonblocking. The documented local
chart/component limits, cold preparation hitch and LOD budget limits remain.
No unrelated historical milestone or Tuppence gameplay acceptance is asserted.
