# Ramped lip and recessed shelf follow-up

Operator feedback: add a slight ramp at the payout edge and make the shelf smaller and farther back. This is a consumer-game content correction, not an engine physics change.

## Current authoring

- The tray terminates at a real inclined static collider, rather than continuing underneath it. The brass strip rises 6 mm over 300 mm (about 1.15 degrees). Coin mode uses the same angle over its proportionally shorter strip, rising 3.3 mm. The strip is both visible and collidable.
- Ball shelf thickness is 0.22 m instead of 0.40 m; depth is 1.10 m instead of 1.40 m. Its rest centre moves back from z=-0.9 to -1.25. A native slider carries it 10 mm clear of the tray and supplies at most 3 N through its normal motor. It can yield under load. Script policy requests a target, never teleports pieces or supplies their reaction.
- The ball machine is now a compact seven-column, eight-row cabinet: 56 initially simulated balls. Capacity is 80, with no charge when a drop is refused. The 104-ball scene is an over-capacity stress fixture, not the normal population or an interactive performance claim. This is a smaller authored game configuration, not a claim that dense-collision engine scaling was solved.
- The chute feeds the shelf's working edge. A fixed rear guide lets the shelf retract beneath pieces instead of carrying them backwards. Five appropriately spaced deflectors leave openings wider than a ball. The ramped edge, physical packing and new insertions determine payout.
- The coated track/shelf use authored friction 0.04; smooth brass balls use 0.08. These replace the previous felt-like friction 0.38, as a deliberately low-friction arcade material; high-friction exploratory trials remained costly under pressure. This is an explicit game material choice, not a solver tolerance/cadence change. Coin token friction remains 0.42.

## Investigation and preserved failures

A 24 mm rise combined with the original densely packed population produced expensive pressure/CCD work. A very short recessed stroke also failed to deliver any payout in the existing application test. The checks were allowed to fail; the payout/save-score requirement was not removed. Raw logs are retained. During compact authoring the unscaled pin radii initially left openings smaller than a ball; screenshots exposed this and the pin count/spacing was corrected. The native shelf excursion is now explicitly measured so a motor that never moves cannot pass merely through settling payouts.

An initial test build used an unavailable BodyHandle equality operator; corrected to compare both lifetime fields (id and world). This was a harness compilation error, not a runtime change. Intermediate exploratory timings and incomplete logs are diagnostic: some runs overlapped or were interrupted by their bounded timeout, and content evolved between trials. They are not a controlled engine speedup comparison. Exact final-source results are recorded separately, with all final application scenarios run sequentially on frozen content.

Prior checkpoint fingerprints are retained in PRE_LIP_SHA256.txt. The canonical fingerprint file describes the final follow-up. Original gameplay/physics regression receipts remain untouched. No physics C++, collision quality, timestep, engine solver budgets, protected history or fluid research was changed. Dense collision cost remains a limitation.

## Focused proof

The native application suite raycasts the actual ramp at two positions and checks both body lifetime identities, its rising height and sloped normal. It also checks actual slider travel, untouched-bed retention, single-insertion retention, normal repeated-drop payout, credit/population accounting, pause, native save/load, and switching modes. The original single-insertion threshold is proportional to the authored initial population: retain at least 75%; measured results report the actual count. This is distinct from the original 112-ball fixture, whose source/checkpoint and failures remain preserved.

Final results and binary fingerprints accompany this file. Human gameplay acceptance is pending. main remains at the accepted M84/Planetfall release.

## Final-source results

Sequential frozen-content native Linux checks: idle 7/0, single insertion 13/0, ball gameplay 30/0, coin gameplay 31/0. The single insertion pays out seven pieces and leaves 50 in the machine, rather than clearing the bed. Actual motor travel, ramp collision, payout, ledger, save/load and mode switching all pass. Moved exported package idle/ramp/startup proof: 7/0.

Ball autoplay: fixed p50 13.6103 ms, p95 33.9602 ms, maximum 75.1377 ms; frame-work p95 35.4047 ms. Coin autoplay: fixed p50 6.8184 ms, p95 30.0969 ms, maximum 123.7506 ms; frame-work p95 31.4583 ms. These are synthetic-frame application work measurements, not certified wall-clock FPS. Heavy pushes still hitch. The final 104-ball over-capacity stress fixture was not rerun; older exploratory stress results do not certify this final source. Dense pressure performance remains unresolved, and reducing the normal authored population does not fix that engine limit.

No full production suite or Windows checks were run for this consumer-content correction. Human visual/gameplay acceptance is pending. Existing old Tuppence saves are not migrated to the compact layout; New Machine loads the current authored configuration.
