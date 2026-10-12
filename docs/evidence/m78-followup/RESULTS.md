# M78 corrective follow-up — final bounded results

Frozen HEAD: `9e77b2226edcf217ab789fe4f0fea42a3415a72d`. One **uncommitted, not fully passing** candidate. No all-184-suite claim. The original M78 audit and all original failing observations remain frozen. [Findings](FINDINGS.json), [M77 requirements](M77_REQUIREMENTS.md), [review packages](HANDOFF.md).

## Matched production observations

| Case | Frozen baseline | Final combined candidate | Boundary |
|---|---:|---:|---|
| Four 13-body rigs, 8192 triangles, 900 steps at 60 Hz: median/p95/max | 170.128834 / 258.706529 / 338.867168 ms | 1.040693 / 18.875046 / 33.793832 ms | p95 still exceeds declared 16.67 ms; 39/52 sleep |
| One rig, same mesh/cadence: median/p95/max | See `concave-before.log` | 0.331528 / 12.233121 / 25.841217 ms | 13/13 sleep; first-120-step max 11.301396 ms |
| Four box-floor controls: median/p95/max | See original matched rows | 0.636415 / 0.855523 / 17.960147 ms | 52/52 sleep; no mesh simplification |
| 6400 inert objects, prefab resolve, three repetitions | ~1844–1854 ms | 62.309 / 54.0621 / 52.4134 ms | Same authored ordering; no steady-script speed claim |
| 6400 total build | ~2020–2032 ms | 258.024 / 243.725 / 233.397 ms | Flatten remains 149–163 ms; not optimized |
| Rotated half-density fluid fixture terminal speed | 0.558701396 m/s (FAIL) | 0.558701396 m/s (FAIL) | Original <0.5 at four seconds unchanged; operator deferred contract review |

Torque-free actual `PhysicsWorld::Step`, triaxial body at 60/120/240/480 Hz: peak relative world-L vector errors **7.95e-7 / 9.63e-7 / 1.51e-6 / 1.33e-6**; energy errors **1.0576e-4 / 2.6096e-5 / 6.3154e-6 / 3.1191e-6**. Separate sphere, rotated, torque and hit-point impulse controls pass. Original constant-world-angular-velocity spacecraft assertions were physically obsolete for anisotropic torque-free rotation; tests now independently check conserved world L. Spacecraft runtime/control code is unchanged. Original failed assertions remain recorded.

Seven-body paired-effort/no-contact test at 240 steps: angular residual **0.00037369522 kg m²/s** with effort off, **0.000292517565** with effort on; linear residual below **5.24e-6 kg m/s**. This is approximate constrained integration, not exact multibody conservation. Final shared-source reproducers link the same clean engine archive (`55f181d2858c39bff02610283dc4392a63ac07962b437de83fd00894b6e30a32`).

## Fresh affected validation

Linux clean affected Release build passed, **two pre-existing RuntimeWorld warnings**. Native Windows affected Release build passed, **102 warnings**; warning logs preserved. Incremental builds were limited to the later portable test paths, spacecraft contract assertions, navigation negative proof and two affected fluid consumers. No production fluid source was edited.

Linux **and native Windows** passes: rigid motion **10,217**, collision geometry **153**, joints **29**, prefab **37**, streaming **82**, navigation dependency/transaction **38**, CharacterMotor **32**, physical-animation **64**, balance foundation **34**; scalability and both spacecraft current-contract executables also pass. Native initial `/tmp` fixture failure and missing-source-fixture abort are retained; portable temp paths and exact original fixtures corrected the test environment, not physics.

Additional Linux passes: rigid contacts; casts **26**; fluid boundary **24**; hydrostatics **93**; cavity impact **7 cases**; fluid/rigid coupling; existing terrain pipeline **15**; independent owned-project terrain fork. Real Balance Lab application: variants **17**, recovery **16**, lifecycle **26**, regions **24**, cold recovery write/read **21/13**. Editor duplicate/undo and Edit/Play/Stop left the authored scene **IDENTICAL**, resources had no failed uploads. Offscreen SDL has no relative mouse capture: this is not an interactive-input acceptance claim.

Linux shipping mod workflow: both saved scene bakes rebuilt; deliberately unrepaired second scene rejected by normal validation; copied package launched from `/tmp` through registered scenes with actual edited collision, Detour and rendering/reload: **20 checks passed**, repeated with the portable package's own data store. Published kit and original inputs remain unchanged.

## Failed and unverified gates

- **REG-001:** 119 controller checks / two failures on both Linux and native Windows; normal Balance Lab step-trace 15 checks / one failure. Original 29 N s case lands then loses support during the second step and falls. No gain/rig/budget/threshold retuning. [Bounded proposal](REG-001.md).
- **PHY-002:** mesh four-rig p95 gate still fails. Median/max improved; some CCD spikes and one active rig remain.
- **FLU-001:** unchanged rotated-half suite 731 checks / one settling failure. Conditional continuum oracle does not guarantee the original terminal deadline; full coupled-model correctness is not thereby established. [Operator-deferred contract](FLU-001.md).
- Native Windows exact thirteen-body/8192-triangle performance, fresh editor/moved-package application and physical GPU/input remain unverified. Native numerical/workflow tests do not replace these.
- M67's deferred review, M74, M76, M77 and Wayfarer boarding/flight human acceptance remain pending. Other missing original briefs remain audit coverage gaps. M75 acceptance stays historical fact.

VM resources were not enlarged. Final native source check verifies 21 affected compiled-source paths against current host source; current free C: space is **69,431,296 bytes**. No more Windows build/package operations are assumed safe at this headroom. Final libvirt observation: four vCPUs, 6 GiB RAM, **40 GiB** disk capacity; this is the observed existing disk, not an invented expansion. [Cleanup ledger](VM_CLEANUP.json) preserves the old test archive and identifies only owned redundant/compiler/configuration scratch removals. Original toolchains, user sources, trust stores, saves and working review package were preserved.

## Evidence identity

Per-case JSON records exact commands, cwd/environment, exit status, time and executable hashes; final probe-build receipts include source/engine archive identity. Raw failures and rejected numerical/certificate attempts remain. `FINAL_FINGERPRINTS.json`, `BINARY_INPUT_FINGERPRINTS.json`, `PRESERVATION_FINAL.json` and `CHANGED_FILES.txt` identify the handoff. Hashes establish byte identity, not scientific correctness or human acceptance.
