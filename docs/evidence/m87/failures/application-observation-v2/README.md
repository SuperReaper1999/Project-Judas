# Second application observations — finite motor against an authored wall

50 checks, one failure. Correctly following the tour's actual selected shelf fixed the tour observation: target 1024 moved 0.119394302 m; unrelated assembly 1044 remained unmoved and asleep.

The other original hypothesis was disproved. Tracking every frame over the unchanged 40-frame resumed interval showed zero shelf excursion, not a reciprocating return to the initial point. Native motor remained enabled at 0.28 m/s; actual velocity stayed around 1e-7 m/s and native coordinate was 0.100002304 m.

The shelf extends 1.05 m along the driven axis. The rear cabinet wall's inner face is 1.15 m from centre; the front retaining rim overlaps shelf height and has an inner face approximately 1.16 m from centre. Therefore solid collision blocks the shelf around 0.10 m of travel, before the project's authored 0.225 m reversal threshold / 0.26 m joint limit. The finite motor correctly cannot drive through that collision. Exact cold-save physical endpoints, joint continuation, contacts and sleeping all passed.

This is a newly authored lab-content clearance defect, not evidence of a save-continuation regression. At the point this failure was preserved, no content or production correction had been made.
