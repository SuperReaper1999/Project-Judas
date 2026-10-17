# Preserved development failures and follow-ups

These are actual implementation/test-development receipts, not fabricated failures
and not regressions in protected historical experiments.

- `first-build.log`: new editor file used the wrong GLAD header spelling. Corrected
  to Judas's existing `glad/gl.h` backend.
- `candidate-build.log`: new fixture used a nonexistent ResourceManager default
  constructor/Init and lacked `<algorithm>`. Corrected to the existing constructor.
- `final-build.log`: standalone benchmark inherited GNU C99 although timespec_get
  needs C11. Target now explicitly requests C11.
- `focused-first.log`: fixture subscribed without the required `onSignal` callback.
  Corrected the fixture; the actual API validation was retained.
- `focused-followup.log`: a **real reload preflight defect**. JS_ResolveModule
  loaded paths but did not link named exports. A missing export got past preflight,
  faulted the replacement and lost the original running instance. JS_PrepareModule
  now performs resolve/create/link without evaluating project bodies. Original
  negative fixture and independent positive controls remain in the test.
- `link-followup-focused.log`: functional assertions passed but an early timing
  harness emitted without draining signals, eventually timing a faulted instance.
  Those timings are rejected. Final timings drain ordinary signal work and assert
  that the benchmark remains healthy. No queue limit was raised.
- `consumer-followup.log`: empty native UI fixture was invalid (no root canvas);
  fixed the fixture. Separately, a **real pending-save/reload interaction** restarted
  the streamed game's HUD while its save-list request was active. The generic
  reload boundary now rejects active save/scene operations before teardown.
  The test independently queues a real list request, proves rejection/state
  preservation, waits for real completion and proves successful reload afterward.
- `fixture-repeat-tools.log`: the test tried to Track its already-tracked fixture
  on a repeated run. Its exclusively owned temporary fixture is now rebuilt cleanly.
- `save-invocation.log`: the runner omitted the existing save test's required
  project/write/read arguments. No save assertion ran. Correct write/read invocations
  subsequently passed; both receipts are retained.
- `api-first.log`: inventory identity metadata was updated before the verifier's
  matching schema identity. Corrected metadata together; no public symbol/type or
  negative drift assertion was removed.
- `validation-build.log`: retained the incorrect cookbook target spelling in an
  early build-driver invocation, and the corrected temporary unused-variable
  warning. Final build/test commands use `judasjs_examples_tests`.

`../RESULTS.json`, final logs and reproduction instructions identify the successful
follow-ups. Later narrow source-view/breakpoint changes received narrow reruns.
