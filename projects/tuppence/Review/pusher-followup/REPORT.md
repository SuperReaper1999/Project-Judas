# Human follow-up: shelf clears the starting bed

The operator reported all balls rolling straight into the end zone on the first insertion. The original automated checks accepted any physical payout and conservation, so did not reject a machine that paid out its entire starting population. That was inadequate gameplay validation.

Reproduction on ad54739f: ten seconds without input retains 112/112 balls. One inserted ball starts the shelf, and at frame 850 all 112 starting balls have paid out (one inserted ball remains). Thus the immediate sweep, rather than spontaneous startup drift, was established as the trigger.

Original shelf travel was 1.30 m, with peak speed 1.0725 m/s. Ball diameter is 0.42 m. This was several ball diameters of sweep, followed by free rolling on the horizontal tray. Corrected content uses 0.24 m travel and peak speed 0.096 m/s; its rest centre is moved from z=-1.5 to -0.9 so the short sweep still reaches the rear row. Motion remains an ordinary kinematic contact-driven shelf. No velocity damping/overrides, freezing, invisible retaining wall or physics solver changes.

Identical single-drop check now retains all 113 real balls, with zero early collections. New regression requires at least 84 balls still present after one insertion; the exact observed count is recorded. Existing full gameplay still proves actual payouts, credits, pause, save/load and mode switching: 28 checks pass. Single-drop test: 12 checks pass. Updated performance: normal autoplay fixed p95 5.2832 ms, maximum 23.0273 ms. Dense-bed stress and final untouched-bed receipts accompany this report.

Coin scene geometry and its native slider configuration are byte-for-byte unchanged. The generator explicitly retains its original shelf transform independent of the new ball layout. Existing physics regression evidence remains applicable; this is a game/content and application-test correction only. Original failure evidence and the previous checkpoint fingerprints are preserved. Canonical FINAL_SHA256 describes the follow-up source; PRE_FOLLOWUP_SHA256 describes ad54739f.

Reproduce with the ordinary application test, using mode `idle` or `single` instead of `play`. No human acceptance of this correction is claimed. main remains at its accepted release; the follow-up is Experimental.

Retained 208-ball stress: 17/0 checks, fixed p95 31.5426 ms, max 84.6340 ms. This remains a stress fixture, not a smooth-frame-rate claim.
