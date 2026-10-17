# Development evidence

- `build-first.log`: a live incremental build encountered an obsolete Window
  method signature after its header changed; warnings also showed missing string
  initializers and misleading indentation. Those were corrected; final completed
  build receipts supersede this partial build. It is not a physics regression.
- `api-first.log`: driver supplied a relative TypeScript package path that Node
  interpreted as a module name. Follow-up uses the existing absolute installation.
- `api-controls.log`: intentional invalid-version example was rejected statically,
  and late-initialized handles needed a guard. The example now marks only the
  intentional invalid call with ts-expect-error and checks handles before use.
  Runtime rejection probes and declaration constraints were retained.
- `ui-first.log`, `application-first.log`: initial positive controls (46 / 21),
  followed by expanded final tests; these are not replaced with fabricated failures.
- `android-build.log`: original native arm64 link failure for meshopt symbols.
  Common dependency linked below Android early return in the accepted baseline.
  Corrected dependency placement and final Android receipt are separate.
- Other build/driver logs preserve actual investigation and closure progress.

No historical evidence or protected fixtures were rewritten. There was no invented
ceremonial failure for a mechanism that already passed.
