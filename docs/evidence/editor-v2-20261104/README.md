# Editor V2 retained qualification material

See [report and reproduction](../../EDITOR_V2_QUALIFICATION.md) and
[architecture/usage](../../EDITOR_V2.md). These are automated observations on
Linux, with final human acceptance pending. Source begins at Scripting V2
45fe558388710d46d6f346b6e19ca2be9e76d0e6. The preserved baseline executable
fingerprint is in baseline/PROVENANCE.json; large binaries/disposable projects
remain in .cache/editor-v2, not this evidence directory.

Mechanical formatting is a subsequent, separate checkpoint. MECHANICAL_FORMAT.json
records 69 files with identical C++ token sequences before/after clang-format19.1.7,
with include sorting and string/comment rewriting disabled.

## Successful controls

- viewport-focus-final.log and viewport-final-native.log: actual rendered pixels,
  secondary camera cadence, independent local-player composition, detached SDL
  contexts and text/drag focus controls (60 and 75 checks).
- window-final.log/window-native.log: 23 window/input/IME controls.
- console-final.log: real failed source and actual suspended QuickJS instruction,
  source selection and unchanged physical/script/file state (16 checks).
- lifecycle-final.log: separate invalid-project application lifetimes, native/UI
  teardown and no preference writes (22 checks).
- focused/, script-direct/, legacy-final/: bounded existing regression results.
- sdk-workflow/: actual external SDK authoring, errors, Play/Stop, tool and moved
  standalone runs. sdk-cold/: distinct processes restore script JSON and an
  authored static collision body's moved endpoint. No old SDK is assumed current.
- extension-proof/: frozen editor source hashes before/after registering Console.
  Only its catalogue and build registration were required; the frame did not gain
  a Console branch. Later independent focus/tab corrections are called out rather
  than falsely claimed byte-identical to the earlier freeze.
- profile/: raw gzip-compressed profiler JSON, exact run commands, peak RSS and
  diagnostic summaries. Layout/pixel-area differences prohibit speedup claims.
- screenshots/: agent-reviewed Dark/Light/scaled scenes, not human acceptance.

## Preserved failures and invalid attempts

- window-negative/negative.log: original main-window event handling fails 4/12
  native controls (foreign close/resize/key/focus). Fixed 23-control runs retain
  primary-window and synthetic-id-zero positive controls.
- viewport-final.log: six new nested-target/observer controls initially failed.
  The corrected composition opens the final image after auxiliary work; readback
  preserves existing pixels instead of clearing them. Known-camera image and
  cadence controls remain, now with genuine split-screen coverage.
- pointer.log: SDL offscreen cannot resize the native drawable; 14/51 failures are
  environmental. pointer-native.log passes the original 51 checks on X11.
- focused/lifecycle.log: first fixture called ImGui serialization with no context
  and crashed before entering the application. Fixture initialization was fixed;
  no product change was made to hide that invalid precondition.
- viewport-focus.log: the added text-ownership negative fixture had not yet
  established an active edit. Real click/release activation fixed its setup;
  active-text/slider refusal assertions remain in the passing test.
- extension-before.log: intentional missing-registration negative; the same five
  architecture checks pass after the module-local addition. Source guards are
  supplementary to the runtime Console test.
- journeys/navigation/: original failures and bounded input traces distinguish
  synthetic SDL cursor state, a hidden Scene tab, missing right/middle focus and
  the gesture release boundary. sync.log passes original motion tolerances,
  repeated presses, focus loss and Play/Stop controls.
- journeys/picking/editor.log: wrong project (developer_lab has no 9001/9002
  fixture IDs or 14 bodies). This is not evidence of broken picking and is not
  counted as coverage; legacy-final geometry-picking suite passes unchanged.
- extraction/BODY_PARITY.json: first owner extraction matched 18/21 normalized
  bodies. The three exceptions are explained in DECOMPOSITION.md, including the
  caught local/member gizmo name collision, corrected before qualification.

No original production threshold or physical fixture was weakened. Final-source
build identity checks may deliberately reject stale intermediate binaries; the
final clean-checkpoint gate and fingerprints are recorded in
.cache/editor-v2/FINAL_FINGERPRINTS.json and .cache/editor-v2/final-gates.

Some verbatim logs are gzip-compressed to preserve terminal whitespace.

SHA256SUMS.txt inventories retained files except itself. Evidence is not modified
by ordinary builds or the test executable; future runs use new output paths.
