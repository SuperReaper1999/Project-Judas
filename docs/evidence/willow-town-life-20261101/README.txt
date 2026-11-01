Willow Town life expansion — 1 November 2026

Base fcd9b5d9d5f79b6a5179842d6c0771a35c0b2b56, Experimental.
Main remains 49c4f3d9e29debe669411f01501e0276dea23231.
No engine implementation, release identity, fluid lock or protected research changed.
Current report: projects/willow_town/Tests/LIFE_QUALIFICATION.md.

Final focused results
- raw/life-final.log: 150/150 native application checks.
- raw/life-final-cold.log: 72/72 genuine fresh-process checks.
- raw/life-absence.log: 25/25 actual UI/automatic 65-working-second dismissal.
- raw/town-qualified.log and town-cold.log: unchanged 115/115 and 56/56 regressions.
- raw/POST_SOAK_RESULTS.json / NATIVE_RESULTS.json preserve exact commands/results.
- raw/build-post-soak.log: final test compilation; no warnings suppressed.
- raw/manifest-final.log and registration-final.log: manifest/registration controls.
- Pure-model logs retain the initial grace-boundary counterexample and correction.

Actual visible observation
soak/summary.json records 1,200.142848 MONOTONIC wall seconds, 3,600.600188 game
seconds observed, 3.000143 game/wall rate. No test harness or delta override.
It records 0 paused and 0 controlled regular observations, speed3 throughout the
samples. Periodic samples do not certify every intervening instant.
soak/metrics.jsonl: 20 CPU/RSS/PSS samples; RSS 75.48–132.09 MiB.
soak/observations.jsonl: 362 actual observations, independently checked for wallet
and job-board ownership invariants. ANALYSIS.json contains measured totals.
Outcome: one old-age death (Sam), two dismissals, three hires; five healthy
survivors. 849 work tasks,357 meals,260 rests,238 participant chat rewards.
These outcomes came from the normal game rules, without injected incidents.
The economy is generous, not a claim of tuned survival balance.

soak/endpoint.save is the exact final ordinary durable save, preserved BEFORE
post-run operator interaction. resume-copy.json / RESUMED_FINAL.json document its
normal Load-key restoration, matching every final observed field and position.
The observer completed and saved, but its outer execution manager cleaned up the
leftover child (tool status143). Root reopened the exact save in a persistent
owned user service. That window is left to the operator; it is not a test fixture.
The operator subsequently reported killing Mara; this is explicitly OUTSIDE the
20-minute measured census. Do not conflate the current live state with this save.

Preserved failures and limits
- life-first.log: dismissal test clicked before the next ordinary UI publication.
  The final test waits for the actual enabled button; assertions remain intact.
- life-second.log / pre-disabled-commands-town.js: actual consumer lifecycle misuse
  commanded a body after disabling it. Cleanup now precedes disable; dead actors
  receive no further commands, including during restoration. No engine patch.
- graphics/soak-smoke: a development executable lacked engine fonts when launched
  from the isolated project. Complete packaged smoke succeeds instead; the runner
  now reports bounded startup diagnostics.
- soak/{start,midpoint,finished}.png: a startup graphics anomaly in that launch,
  with corrupt upper-left house and absent solid UI rectangles. PNG/HUD sources
  matched the qualified fixture. Later normal internal/external captures and a
  12-second wrapped probe rendered correctly (graphics/). No reliable engine
  cause or fix was established. Preserve BOTH failed and successful evidence.

Reproduce from repository root with fresh output directories
 cmake -S . -B /tmp/judas-willow-build -DCMAKE_BUILD_TYPE=Release
 cmake --build /tmp/judas-willow-build -j2 --target judas judas_editor judas_export judas_willow_town_application_tests judas_willow_town_life_application_tests
 /tmp/judas-willow-build/judas_willow_town_life_application_tests projects/willow_town/Willow_Town.judasproj /tmp/willow-life
 /tmp/judas-willow-build/judas_willow_town_life_application_tests projects/willow_town/Willow_Town.judasproj /tmp/willow-life cold
 /tmp/judas-willow-build/judas_willow_town_life_application_tests projects/willow_town/Willow_Town.judasproj /tmp/willow-absence absence
 /tmp/judas-willow-build/judas_export projects/willow_town/Willow_Town.judasproj /tmp/willow-package /tmp/judas-willow-build/judas
 python3 tools/diagnostics/watch_willow_town.py --runtime /tmp/willow-package/judas --output /tmp/willow-watch --leave-paused
The last command takes TWENTY REAL MINUTES. Use --seconds 3 only for a short adapter
smoke; it is not the qualification. Use a complete
packaged runtime with engine fonts. The caller must preserve its child lifetime
if --leave-paused is used. Current restored window is managed independently.

fixtures.tar.gz preserves isolated native source projects, expected states and
actual cold-save bytes, including original failures. No binaries/build caches.
FINGERPRINTS.json maps exact final source and tested binaries. Development
provenance is base+dirty, not falsely labelled as built from the later commit.
SHA256SUMS verifies this evidence bundle. Windows/Android/browser and full human
acceptance remain unclaimed; main promotion requires explicit operator approval.
