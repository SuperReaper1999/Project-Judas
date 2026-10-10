# M75 development records

First incremental build stopped because the new focused test omitted `<algorithm>` for `std::all_of`. Added the test include; the first focused CPU run passed 31 checks. Initial compiler style warnings are retained in the build record and will be corrected before final candidate verification.

These are development observations, not human acceptance. M74 remains committed with human validation pending.

The analytic physics test initially used integer scalar literals with GLM float vectors; compilation rejected them. Changed the focused-test literals to float, then all 54 geometry/authoring checks passed. Subsequent real-palette checks bring this to 61.

The pipeline test reproduced a genuine pre-existing discrepancy: loading a cooked triangle-mesh instance with non-unit visual scale was accepted, despite M64 documentation reserving unit-scale instances. The surface body ignores instance scale. M75 adds one shared rigid-placement validator at scene load, world creation/prefab preflight, and transform writes. Primitive visual-only scale remains unchanged. Original failing pipeline output is retained here.

The first lab content cook rejected the incorrect `explicit` policy token (the real policy is `closure`), then a project-relative source argument was mistakenly prefixed twice by the CLI. Corrected the content policy and made the CLI cook source explicitly project-relative (absolute paths also work). These authoring failures did not publish missing geometry or replace a good package.

Application fixture corrections: the first build referred to a nonexistent
`BodyHandle.generation`; current handles use world ownership plus monotonically
allocated body IDs. The first actual region/reload run passed unload/revisit but
incorrectly expected five total bodies, omitting the motor's query volume.
The assertion now compares to the observed authored baseline and requires a
strict body-count increase after the public prefab spawn. No runtime fix was
needed for either test-fixture failure; original logs are retained.

Native viewport fixture initially called `HandleRequests` immediately after the
frame's ImGui render, causing `BeginPopupModal` to crash outside a UI frame. GDB
located that exact fixture call. The fixture now queues the rescan through the
existing next-frame deferred request path, exactly like normal editor controls.
The brush, save and cook all passed before the fixture error. The original log
and backtrace are retained; this was not a terrain renderer/physics crash.

Actual desktop Play/performance revealed a pre-existing cooked-geometry adapter
problem exposed by useful terrain size. M56 attributed ~727 ms of each catch-up
frame to eight CharacterMotor evaluations; rigid physics was ~0.37 ms/frame.
The inherited sampled capsule sweep called `SegmentGeometry` with infinite range
at every 24-substep/20-refinement overlap evaluation, scanning all 8192 faces.
That sampler only consumes `distance <= 0`, so the adapter now passes range zero:
the existing cooked BVH culls triangles outside the capsule AABB. No hit can be
lost by that exact overlap bound; convex inside handling remains unchanged.
Substep count, refinement, cadence, terrain resolution and collision response
are unchanged. Before data and original editor timeouts are preserved.

Second-project automation initially omitted its screenshot output directory. Real
brush/save/cook and Play/Stop passed, but capture/save artifacts failed to write.
The output directory was created and the same real workflow repeated successfully;
`development-second-editor-output-directory.log` retains the original fixture error.
Its subsequent export correctly rejected the newly stale navigation bake. Normal
offline rebaking restored a valid package; this was expected invalidation, not a
runtime patch or hidden stale-bake bypass.

Final bounded review added a retained-product concurrency guard (including products
not rewritten by an appearance-only cook), retired cancelled editor jobs, and made
CLI source saving work for a simple relative filename. These receive narrow shared
service reruns on Linux and MSVC, without repeating the unrelated production suites.

The first MSVC compile completed, but receipt upload failed after the temporary host transfer process ended. This was infrastructure, not a compiler/runtime failure. The established transfer route was restarted as a bounded temporary user service; the native build log and error receipt are preserved. Three new test-only MSVC warnings (float literal, unsigned-byte char literal and shadowed local name) were corrected. An incomplete local variable rename then caused a test-only compile error; it was corrected and the affected target rerun. Pre-existing MSVC warnings remain reported separately.

Native editor completed with zero failures and identical authored scene after Stop.
The PowerShell wrapper incorrectly matched the word "failures" as "FAIL" because
its regex was case-insensitive and lacked a delimiter. The retained output proves
the pass; the matcher now requires `FAIL:` and the package stage resumes without
repeating passed editor/save checks. SDK review also found the existing navigation
CLI was not shipped; it is now included with the same normal manifest/DLL policy so
Windows authors can rebake terrain-derived navigation. No new navigation API or
bake algorithm was added.

Final source review found that narrowing JSON/CLI sample counts could truncate
fractional/overflowing values, and source Open parsed one read but guarded a second
read. Counts now validate before narrowing; Open parses and guards identical bytes.
Fresh malformed-count checks prove rejection. One local rerun used an old fixture
directory and its already-registered palette caused a test failure/abort; the log is
retained, and tests now isolate each run beneath the caller-owned output directory.

Terrain creation also needed to honor non-default project asset folders. Editor
creation uses the configured root; CLI accepts an explicit product directory; forks
preserve their parent's convention. Cooking rejects products outside normal asset
discovery. Four focused custom-root checks pass; no parallel resource pipeline was
introduced.

The independent neutral neighbour was first inserted with a nested collision
property and an existing gravity object ID. The ordinary scene parser rejected
the property before any runtime publication. The fixture uses unique ID 3 and the
normal flat `body.collision-asset` field; ordinary navigation, startup and moved
runtime checks then passed. The rejected authoring output is retained.

### Final native input audit: stale guest documentation copy

The 819-input union audit initially rejected one file: the guest's pre-existing
`docs/WINDOWS.md` copy, not a compiled source, asset or packaging input. The original
failed hash audit and that copy are retained in `windows/m75-windows-final-input-failure.json`
and `windows/m75-windows-preexisting-doc.md`. The unchanged current repository document
was transferred to the guest; the same expected manifest then matched all 819 inputs.
No expected hashes were changed, no runtime source changed and no build rerun was needed.
The guest had ~49 MB free at the final audit; fixed disk/configuration was preserved.

### Human follow-up: terrain panel did not make activation discoverable

The operator opened View → Terrain sculpt / paint but could not edit. Source review
confirmed that opening the panel left the source unopened and brush disabled, with
a matching selection also required. The original editor smoke bypassed those
prerequisites by assigning `loaded`/`editing` directly. This was a usability gap
in the delivered instructions and panel, not a demonstrated brush-math failure.

The primary **Edit selected terrain** control now opens and activates in one click;
**Open source and start editing** selects a matching instance when available. Brush
readiness, selection recovery, mode/radius/strength and paint-layer choice are at
the top. Errors are shown above the source controls. No solver/cook/runtime changes.

The first new synthetic click check failed because motion/down/up were queued too
closely for ImGui's input trickling; its observed output was `FAIL: real panel click
opens selected source and activates brush`. Separating movement, press and release
across frames made the actual UI path pass; no forced editing-state assignment was
restored. The final Linux desktop check has 13 terrain assertions, zero failures,
and unchanged authored scene after Play/Stop. See `editor-ui-activation.log` and PNG.

The corresponding non-elevated native Windows UI check also passed 13 assertions,
with normal saved/cooked Play and IDENTICAL authored state after Stop. A new local
`selected` shadow warning was corrected by renaming the readiness local; only that
file was rebuilt after the successful native editor build. No behaviour change.
Original build receipt is retained under `windows/development-m75-windows-ui-build*`.
