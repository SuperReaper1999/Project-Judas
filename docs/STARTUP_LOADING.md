# Game startup presentation

Normal standalone launches show the Judas vortex animation while the initial
world is prepared. The scene is not presented with placeholder geometry and
physics/gameplay updates do not run behind the loading screen. This applies to
new exports too; there is no reference to the original animation directory.

`StartupScreen` owns presentation and window events on the graphics thread.
`StartupMovie` indexes a bounded, independently compressed frame stream and
uses the existing image decoder, retaining one decoded frame and one texture.
The supplied eight-second, 30 fps clip is cooked to 512 square pixels (about
5.4 MiB on disk); FFmpeg is an authoring dependency only. Playback loops only
while needed and does not impose an eight-second startup delay. Missing or
corrupt splash content falls back to an ordinary text loading screen.

`RuntimeWorld::PrepareStartup` prepares structural instances and animation at
zero elapsed time and warms the ordinary renderer to discover its actual
resource dependencies. Once authored dependencies are available, scripts run
`start` (or restored lifecycle) exactly once so their initial content and camera
can join preparation. Newly attached scripts/resources require another pass.
Two quiescent passes establish readiness before the first gameplay frame.
The game frame clock begins after preparation, avoiding loading-time catch-up.
Asset failure reports an error and exits rather than waiting indefinitely.
Window close remains available during loading.

Start callbacks should declare initial content; content intentionally created
later by update callbacks or runtime streaming follows the normal asynchronous
policy. This is an initial-launch gate, not a synchronous loading policy for
all subsequent scene transitions. The deterministic `JUDAS_TEST_SCRIPT`
harness retains its existing blocking resource mode and skips splash playback.
Editor Play remains an editor workflow; this display belongs to game launches.

## Re-cooking the engine animation

```
python3 tools/cook_startup_movie.py /path/to/judas_vortex.mp4 assets/branding/judas-startup.jsplash
```

The cooker preserves the source, emits the bounded JSM1 JPEG-frame container
and records its SHA-256 in the adjacent JSON. The container limits are 32 MiB,
600 frames, 60 fps, 1024 square pixels maximum and 2 MiB per encoded frame.
No downloaded vehicle content or external project files belong in this asset.

## Qualification

- `judas_startup_movie_tests`: every supplied frame, looping, corrupt/truncated
  payloads and bounds; no graphics driver needed.
- `judas_async_application_tests`: real asynchronous worker/GPU ownership,
  stalled startup with responsive SDL input, no physics during loading, complete
  initial mesh residency before gameplay and no loading-time catch-up; retains
  cancellation, recovery, stale generation, budget and shutdown coverage.
- `judas_script_tests`: lifecycle-only startup is idempotent and does not invoke
  gameplay updates.
- `judas_export_tests`: package-relative animation included in exported games.
- Whole-engine benchmark observers record loading-frame count and preparation
  display duration separately in `startup.json`; sufficiently long preparation
  also captures `startup.png`. Gameplay metrics exclude startup.

Native Linux NVIDIA qualification is recorded in
`.cache/startup-splash/QUALIFICATION.json`. Other platform runtime validation
must be reported separately; common packaging alone is not platform proof.
