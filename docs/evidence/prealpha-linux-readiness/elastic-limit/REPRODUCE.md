# Open: fast elastic spinning confinement

This is a preserved numerical failure, not an accepted energy result or a default
CI pass. No force/gravity, unit mass, box half-extents `(0.08,0.2,0.05)`, static
walls at x=±0.6 with half-extents `(0.05,1,1)`, friction 0, restitution 1, one
`1/60` second step. Initial velocity `(100,0,0)`, spin `(0,0,100)`.

`probe.cpp` exercises ordinary native PhysicsWorld and reports all 16 controls.
For diagonal box inertia `I_i=(h_j²+h_k²)/3`, kinetic energy is
`E = |v|²/2 + sum(I_i * (R^-1 omega)_i²)/2`. The high-rate case yields
5077.33334 → 125043.475 (ratio 24.6277852). Free-flight controls and the 100 m/s,
0/5/20 rad/s contact controls are also retained; they do not certify every rate.
There is no physical energy source in this native fixture. Root numerical cause
is not established by these diagnostics; the path hold is not proof of its cause.

With an engine build containing `judas_mk2_diagnostics_tests`, compile `probe.cpp`
against the matching `src` headers and the same static libraries listed in
`build/CMakeFiles/judas_mk2_diagnostics_tests.dir/link.txt` (replace the test objects
with this probe object). Run and compare `native-results.log`. An independent
ideal-elastic energy check must reject the high-rate result; do not retune the
fixture, budget, or restitution to turn it green.

The `project` subdirectory is a second **ordinary JS/runtime entry point**, not
an assertion that its initialization pipeline equals the direct native fixture:

```sh
JUDAS_WORLD_STATE=none JUDAS_TEST_SCRIPT="$PWD/docs/evidence/prealpha-linux-readiness/elastic-limit/project/probe.test" \
  build/judas docs/evidence/prealpha-linux-readiness/elastic-limit/project/game.judasproj
```

The exact printed endpoint is identical on the retained binary from the accepted
0aaef603 candidate and this diagnostic candidate. Its prior binary hash/provenance
is preserved in `baseline-provenance.json`; the retained binary's own provenance
is a dirty-development build, not a newly claimed clean release. The direct-native
and ordinary-runtime results are recorded separately. No retroactive compatibility
or universal energy guarantee is inferred.
