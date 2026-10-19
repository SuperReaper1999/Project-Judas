# Dense-contact support-certificate follow-up

This is a new evidence package following safe Experimental checkpoint `8f6bd18b876158c131bee61fa0dc71625d912c30`. It preserves successful and rejected investigations without modifying the earlier dense-scalability evidence. It is neither human acceptance nor a claim that the dense lab now sustains 24–30 FPS. Final production qualification and checkpoint fingerprints are supplied separately by the integrating task.

## What changed

`SupportContinuationSafe` bounds a **lower-clearance proxy** using conservative rotational extents over a time interval. The old internal recursion treated the proxy's nonpositive upper endpoint as proof that further subdivision could not help. That implication is invalid: smaller time intervals tighten the rotational extents and can establish clearance.

The correction removes only that premature internal rejection. It retains the original 5 mm allowance, depth-three bound, native motion and contact geometry, endpoint precheck, unsupported-path fallbacks, solver iterations, friction, restitution and 60 Hz cadence. See [support-proof/FINDING.md](support-proof/FINDING.md), the exact original/corrected source, and recorded control results.

- All 78 exact captured stages previously stopped at one root node. The actual original method and diagnostic branch observer agreed on every rejection. The corrected actual method certifies all 78 within the unchanged partition budget.
- The independent permanent fixture passes 11 checks on the corrected engine. The exact same fixture genuinely fails four checks on the original implementation.
- Negative controls reject both a closing pair with 16.67 mm endpoint penetration and a rotating thin box with 9.90 mm interior penetration despite a clear endpoint. Endpoint sampling is not substituted for the interval certificate.
- The separate 257-sample native-manifold diagnosis is explicitly sampling evidence, not continuous proof.

## Actual matched ordinary workload

Both ordinary runs use the identical authored room, 270 fixed steps (4.5 simulated seconds), original bodies and physical settings. Profiling is disabled in both. These are one measured pair, not a statistical confidence interval or sustained frame-rate guarantee.

| Fixed-step milliseconds | Safe 8f baseline | Support correction |
| --- | ---: | ---: |
| Median | 23.004 | 22.484 |
| Mean | 63.033 | 46.271 |
| p95 | 213.605 | 158.841 |
| Worst | 942.047 | 262.562 |

The corrected certificate deliberately changes event scheduling and hence the physical trajectory; exact trajectory parity with the old incomplete certificate is not claimed. No solver quality, object count or contact fidelity was reduced.

The temporal observer runs after the measured physics span. Every ordinary corrected-run physical/work row exactly matches its corresponding observer-run row. All 1,024 tagged pieces, their 2,048 kg mass, finite states and finite-floor coverage remain present through all 271 observations (initial state plus 270 steps). The corrected run's endpoint depth is 7.187 mm, with no uncertain or unsupported endpoint pairs.

The full transient observations are mixed: maximum downward floor excursion improves from 63.58 to 45.07 mm, while the maximum sampled interbody penetration worsens from 57.80 to 61.96 mm. These sampled maxima are diagnostics, not newly invented acceptance thresholds. Pair geometry is sampled at step zero and every tenth step. No blanket accuracy improvement or whole-world energy-conservation claim is made. Observer wall time/FPS is not performance evidence.

## Index

| Directory/file | Contents |
| --- | --- |
| `MEASUREMENT_INDEX.json` | Raw-result index, exact physical-row comparisons, numerical observations and explicit dispositions. |
| `runs/baseline-safe8f-270`, `runs/support-candidate-270` | Ordinary raw steps, frames, endpoint quality, actual commands/exits and timing receipts. |
| `runs/temporal-baseline-270`, `runs/temporal-support-270` | All-step mass/energy/floor observations and periodic actual pair geometry. Baseline and candidate observer timing is not compared. |
| `support-proof/` | Exact original/corrected source, 78-stage results, independent controls, original genuine failure, first failed wrapper compilation and build provenance. |
| `traces/` | Gzip-preserved exact native input bits for pair 595/851 and captured impact event 3470, plus their read-only capture source/receipts. |
| `temporal-oracle/` | Exact unchanged Float-layout observer source, meaningful oracle controls, original compilation follow-up and support-driver dependency receipts. No coherent-double headers are used. |
| `provenance/` | Immutable source-input archives, frozen binary/archive hashes, source identities, and independent ordered-member single-TU freeze verification. Binary payloads are intentionally omitted. |
| `unadopted/coherent-geometry-A2/` | Rejected double-geometry source archive, complete ABI/member receipts, actual 270-step regression, support-bound failure/correction and independent public-motion-history incompatibility. |
| `unadopted/cached-discovery/` | Exact-parity one-TU experiment, original controls and detecting mutants, actual 270-step result; not adopted. |
| `unadopted/impact-memo/` | Event-local body lookup/lazy rotation experiment, complete bit-parity replay, original regressions, invalid-input/joint/lazy controls and detecting mutant; not adopted. |
| `unadopted/pair-cache/` | Standalone broadphase cache source and adversarial controls only; never integrated into world physics. |
| `preliminary-validation/` | First integration build/preflight receipts, explicitly classified as preliminary rather than final qualification. |
| `SOURCE_RECEIPTS.json`, `INVENTORY.json` | Exact source-copy provenance and package file hashes. Later root-owned final receipts are outside this initial inventory. |

## Rejected/unadopted alternatives

- **Coherent double geometry A2:** actual mean fixed-step cost regressed from 63.033 to 96.684 ms; p95 from 213.605 to 416.014 ms. Search iterations grew despite fewer events and queries. The same-source public-history control also found 79/192 post-impact rotation samples and 49/192 position samples no longer exactly reconstructible from the public Float motion records. No full integration suite was run for this rejected candidate. Its earlier support-proof double-COM bug and corrected controls remain distinct from the unchanged safe baseline.
- **Cached remaining-bound discovery guard:** exact physical/work/endpoints, but a 1.6% local mean change with a worse worst step does not establish a useful end-to-end gain. It remains unadopted.
- **Impact membership/rotation memo:** all body, prepared/solved row and result bits match the captured event over 32 repetitions. The original 46-case/267-check suite and 22 additional checks pass. An eager unused-orientation mutant genuinely fails. The local mean improves only 3.7%, with worse p95/maximum. This does not justify added runtime complexity or a whole-world speedup claim; it remains unadopted.
- **Persistent broadphase pair cache:** 24,025 standalone checks pass; missing-bound and missing-eligibility mutants produce 8,834 and 3,217 detecting failures. No engine integration or end-to-end speed measurement occurred. The original combined shell call did not preserve each process's separate exit code; the command receipt states this explicitly.

## Reproduction and provenance boundaries

Use the final integrating Experimental checkpoint and its normal configured Release build. The current permanent reproduction is `judas_support_continuation_tests`; original frozen source and genuine original nonzero output are preserved here rather than changing the regression into an expected-failure mode. The raw run receipts record exact arguments, scene hash, original source identity, executable/archive hashes and real exits. The committed `scripts/physics_dense_measure.py` provides the unchanged explicit `--profile 0` exact-step runner for ordinary measurements.

Historical `.cache` paths in copied receipts are intentionally not rewritten: they identify what actually ran. Source archives preserve the exact investigation input bytes, while external dependency/toolchain paths are identified rather than bundled. The read-only original research and protected historical evidence are not inputs to these edits.

The preliminary validation includes 18 wrappers that refused stale build-source identity before executing their tests, after tracked documentation changed during a build. Those unavailable cases are neither physics failures nor passes. Thirteen direct/negative/tooling invocations completed; root refreshes identity and reruns the 18 previously unavailable wrappers separately. Consult the later final receipts for actual final-source qualification.
