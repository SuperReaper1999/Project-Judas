# Browser graphics backpressure correction — 31 October 2026

Experimental follow-up to the player stage at `357276fc`. Main and published
Alpha packages remain unchanged. This is not a new browser-support certification.

Follow-up: the previously unresolved triangle-shadow pattern has a shared-shader
correction, followed by a longer memory/close/reopen qualification. See
[the subsequent report](BROWSER_SHADOWS_AND_ENDURANCE.md). Historical results and
failures below remain unchanged.

## Failure and mechanism

Human testing reported delayed keyboard/mouse input, then severe desktop memory
pressure, especially after closing the game. Treating the earlier NVIDIA faults
as an explanation was premature. At 14:29 the kernel recorded a genuine global
out-of-memory kill of Firefox: approximately 5.9 GiB resident anonymous memory
and 13.8 GiB swapped in that process. The configured 1 GiB WASM-memory ceiling
does not bound browser graphics queues, renderer allocations or total process
memory.

The old worker presentation path waited for requestAnimationFrame, or a 250 ms
lifecycle fallback. Neither establishes completion of submitted GPU work. That
left graphics submission without a GPU-completion backpressure boundary. The
installed Firefox 159/NVIDIA path reproduced growth and a further spike when
navigating away. Its `CanvasRenderer` also invoked the OOM killer during an
isolated run. Exact internal Firefox allocation ownership has not been established;
these observations do not prove a particular driver allocation leak.

Two interventions isolated the missing boundary: a disposable per-frame
`gl.finish()` diagnostic and an asynchronous GPU-fence diagnostic both prevented
the reproduced close-time surge. The production correction uses the fence,
not blocking `gl.finish()`, browser-specific detection, reduced graphics quality,
or reduced simulation cadence.

## Correction

`src/Window.cpp`, browser-only `AwaitBrowserFrame`:

- Insert one WebGL2 fence after the submitted frame and flush its commands.
- Resume the game loop only after that fence signals, limiting submission to
  one unfinished GPU frame.
- Poll asynchronously at presentation boundaries; preserve the 250 ms fallback
  when RAF is unavailable in a hidden tab. At most one RAF and one fallback
  timer remain scheduled; each poll cancels its competing wakeup.
- Reject failed/lost GPU contexts and delete the fence on success or rejection.

The page/UI thread remains available. Physics ordering, fixed-step settings,
render resolution, shadows and game content are unchanged. Native desktop code
is outside this conditional path.

## Measurements

Same ordinary player project, same machine, installed Firefox 159.0, fresh
profiles and real WebDriver keyboard/mouse input. The baseline and both fence
runs used six approximately five-second gameplay intervals, followed by
navigating to `about:blank`. These are diagnostic memory measurements, not
isolated throughput benchmarks.

| Run | Peak MiB | Last sample before leaving | First sample after leaving | Outcome |
| --- | ---: | ---: | ---: | --- |
| Original presentation | 1999.7 | 1499.5 | 1999.7 | Proactively stopped above 1900 MiB |
| Disposable asynchronous fence | 983.7 | 950.3 | 645.7 | Completed, including 20 seconds after leaving |
| Fresh source-built correction | 1285.5 | 1283.2 | 966.4 | Completed, including 20 seconds after leaving |

The metric is the **whole isolated systemd cgroup's `memory.current`**: browser,
automation, server, shared memory and charged cache. It is not engine heap alone.
The original peak is an early-stop observation, not a bound on eventual growth.
The blocking-finish diagnostic was shorter (four intervals), peaked at 1056.1
MiB and is therefore excluded from the matched-duration table.

All reproductions had a 2500 MiB hard cgroup limit, zero swap allowance and a
runtime deadline. After the first capped OOM, the short comparisons also added
proactive termination at 1900 MiB. Functional regressions used a separate 2200
MiB proactive guard. The stock-run notification concerned `CONSTRAINT_MEMCG`,
not a second global OOM. Its separate geckodriver abort is preserved as an
automation failure and is not treated as an engine crash stack.

Hardware: Ryzen 5 5600, 8 GiB RAM, GTX 970 4 GiB, NVIDIA 580.178.04,
Linux 7.0.0-42. The source-built run's observed gameplay clock progressed from
5.15 to 31.13 seconds with changing physical positions and camera yaw. The
operator noted improved responsiveness during a diagnostic prototype; the final
source-built candidate still requires their human acceptance.

## Qualification

- Actual embedded frame-wait body: **25 checks**, including slow GPU, bounded
  callbacks, hidden-tab wake, failed/lost context and setup cleanup. The old
  source fails this regression as a negative control.
- Browser exporter/workflow Python tests: **34 passed**, including registration
  of the frame-wait test when Node is available.
- Browser surface arithmetic: **72 passed**.
- Freshly compiled player, Chromium 156 at DPR 2: **29 passed**.
- Freshly compiled player, Firefox 157 at DPR 2: **29 passed**.
- Installed Firefox 159 hardware path: input, real gameplay, navigation away,
  20-second observation and browser cleanup completed under the memory guard.
- Real normal Chromium tab: **10 passed** for actual background visibility,
  stopped game frames/fixed simulation, native Web Audio suspension/resume and
  released keys without hidden-time catch-up. Uses a freshly exported Beacon
  Grove fixture and the same rebuilt runtime.

The initial build was correctly refused because tests changed during compilation;
the final frozen-source rebuild succeeded. A separate high-DPI diagnostic harness
had a ReferenceError and was rejected, not counted as engine validation. A Python
build-organization discovery attempt selected zero tests and is not counted.
Their outputs remain available with the other failed attempts.

## Reproduction and evidence

See [the raw evidence](evidence/browser-frame-backpressure-20261031/SUMMARY.json)
and its `SHA256SUMS.txt`. It includes source/binary hashes, package receipts,
bounded reproduction scripts, original OOM lines, raw logs and browser results.
Fresh browser profiles and generated binaries are deliberately excluded.

The `diagnostics` scripts record the exact host-specific investigation. Their
cache paths and Snap geckodriver binary are prerequisites, not new engine build
dependencies. The disposable finish/fence packages intentionally changed the
generated JavaScript and refreshed only their local integrity receipt; they are
not qualified exports. The final `source-fixed-web` package was compiled through
the normal exporter with its source-matching guard intact.

Portable focused checks:

```sh
node tests/browser/frame_pacing.cjs
python3 -m unittest discover -s tests -p 'test_browser_*.py'
```

Real browser drivers remain documented in `BROWSER_PLAYER_QUALIFICATION.md`.
Run one browser workload at a time with bounded memory on this 8 GiB host.

The tested package records its parent commit and dirty source fingerprint; it is
not relabeled as a clean post-commit build. `SUMMARY.json` pins the exact changed
code and test bytes independently of that provenance.

## Remaining limits and human check

No claim is made about every browser/driver, unlimited session lengths, mobile
browsers, Safari, or automatic context restoration. Previously reported Firefox
shadow artefacts remain unresolved. Browser runtime/download size and total
browser memory remain separate optimization work; a WASM ceiling is not a
whole-browser memory budget. Advanced Fluids remain locked.

For human acceptance, play the fresh source-built lab, move/look continuously,
try third person and fullscreen, then close it. Confirm responsive input and that
desktop memory recovers without low-memory notifications. Repeat once. Do not
use the old `final-player-web` package to test this correction.
