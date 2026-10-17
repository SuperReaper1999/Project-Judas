# Preserved M95 development failures and follow-ups

These are not final passing evidence. Successful receipts are selected under
`../linux`, `../cook` and `../delivery`.

- `first-build.log`: the initial build was configured before the newly added
  editor translation unit; link missed AnimationTimeline. Reconfiguration fixed it.
- `candidate-build.log`: new test omitted `<algorithm>`; follow-up compiled it.
  New authoring/editor indentation warnings were corrected; no runtime workaround.
- `workflow-first`, `workflow-content-followup`, `startup-first`,
  `workflow-mesh-followup`: lab content initially omitted full body fields / the
  mesh render discriminator and included an unsupported UI root `name`. Corrected
  through ordinary content authoring. Original startup errors remain intact.
- `workflow-ui-followup`, `workflow-refined`: native save fixture attempted to
  capture an uninitialized script. Run its normal lifecycle first; no save guard
  was relaxed.
- `api-first.log`: driver supplied a relative Node package path. Correct absolute
  local TypeScript tooling path; no permanent runtime dependency was introduced.
- `final/`: runtime-tag test invoked without its required fresh output directory.
  Corrected test invocation, preserving the actual fixture/checks.
- `final-candidate/`, `trim-isolation/`: unchanged M74 dense trim regression failed.
  Accepted ModelCook source substitution reproduces it. Exact representable-key
  preservation corrects the mechanism; the original dense-60-Hz collapse case
  remains passing. Fixtures/tolerances unchanged; no historical evidence edited.
- `editor-wait/`: automated editor recook trigger was re-issued while its frame
  number was held waiting for publication. Guard the one-shot test action. This
  was an autotest-driver loop, not a game-runtime stall. The controlled validation
  editor was stopped; unrelated desktop instances were not stopped.
- `verified/animation-cold-write.log`: new test's content identity was not a
  required 64-character fingerprint. Corrected the test metadata; validation
  policy was retained. Separate-process continuation now passes.
- `remaining/save-write.log`: an unconditional observation record changed the
  existing save lab's exact participant count. Made the new record conditional on
  assets containing events/extracted motion. The unchanged legacy test now passes.
  Actual SaveService accounting and the composed 22-record envelope were also
  integrated, with 23-record rejection retained.
- Early delivery screenshots had unrelated autotest panels in front of the
  timeline. The capture-only hook now focuses the timeline; the readable final
  capture is `../delivery/timeline.png`.

Zero-dt CharacterMotor publication, disabled animation refusal and shared-IK
single-advance guards were added during source review and covered directly.
There is no invented failed numerical experiment for those review findings.

Source review also found model-import completion was below the Asset Browser
visibility early-return. Moved its existing generic completion step above that
return, and the timeline test now closes the browser before saving/recooking.
This was a source-observed integration dependency; no pre-fix runtime failure is
invented for it.

- `root-ik-before.log` / `root-ik-after.log`: actual pinned world-space hand tests
  expose and correct a one-step motor/IK ordering error introduced by the initial
  explicit sampling path. Same targets, movement, thresholds and solver budgets;
  actual joint endpoints, not convergence flags. Legacy error ~0.110 m and shared
  error ~0.100 m reduce to ~6e-8 m / 1.90e-5 m. Clip clock advances once.
- Draft lifetime follow-up: unsaved marker data is retained across clip/asset/
  project selection until explicit Save/Discard. The editor test deliberately
  switches clips with an unsaved marker and requires the old draft to survive.
