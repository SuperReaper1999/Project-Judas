# Judas stabilization status

**JUDAS STATUS: READY FOR NEW FEATURE DEVELOPMENT**

FTFT1–9 are committed closures within their documented scopes. The final gate on
`1b3a134bdb3d7876d8ed8261571311ca7880e386` configured and built Release from an
absent build directory, with zero compiler warnings, then passed the complete
mandatory validation once. No engine implementation changes were needed.
See [final executed evidence](evidence/stabilization/final-gate/README.md),
[the preserved FTFT9 candidate](evidence/stabilization/ftft9/completion-candidate/RESULTS.md)
and [the concise ledger](FTFT.md). No milestone tag was created.

## Verified scope and closure commits

| Item | Guarantee within its documented scope | Closure commit |
|---|---|---|
| FTFT1 | Versioned authored-baseline identity; incompatible saves rejected before world mutation | `23f5447d37fb96ab4c42017d33b5de1fccae997b` |
| FTFT2 | Actual asynchronous IO/decode/upload coverage; CPU-ready cancellation and generation safety | `0d38ddb2babbb8bc51d9500f3fd7721f7f8a1b84` |
| FTFT3 | Scene-authored uniform acceleration preserved without angular snapping | `3404388f1af48419505c802fc11dbf8a0b577f38` |
| FTFT4 | Robust contact geometry; actual-time isolated impacts; stable inelastic coupled-event approximation | `0ebf1d4a7e93b912c8dc59cc663ee729259358a0` |
| FTFT5 | Compact local simulations at large absolute fixed origins; no live rebasing claim | `52911068afcca393e8b0836dbfbb08673a9f1091` |
| FTFT6 | Tested ordinary nonplanetary editor/project/asset/lifecycle paths | `a0ba6d1dbaf60954a08b165e09e60741469ac0f3` |
| FTFT7 | Tested orbital, spacecraft and reference-frame force/motion paths with finite-step error budgets | `6145762bbc295b586846c5dcc29aea49915f5e34` |
| FTFT8 | Finite-fuel combustion and thermal accounting with an explicit atmospheric reservoir | `d2246cd19ad70065d121ffc5d4a4445ebca5d82b` |
| FTFT9 | Approved approximate hydrostatics/drag, declared containers and custom-controller swimming | `1b3a134bdb3d7876d8ed8261571311ca7880e386` |

FTFT1's strict compatibility policy has no general save migration. The production
fluid metadata uses canonical fingerprint schema 2 and rejects old-schema saves;
the persistence format and atomic validation mechanism remain unchanged.

## Explicit approximations

- Coupled/ambiguous impacts are inelastic; isolated impacts use authored
  restitution. Low-speed capture, event caps and finite-resolution grazing CCD
  are documented real-time policies. Active compounds/supports can be costly.
- Coordinates use a fixed double absolute origin and local float simulation.
  Travelling indefinitely through an automatically rebasing region is unsupported.
- Orbital/rigid integration has finite timestep and representation error; Coarse
  celestial simulation is contact-free. No exact energy conservation is claimed.
- The atmosphere is a prescribed open reservoir. Combustion uses approximate
  material/thermal properties; smoke is visual, not evolved conservative gas CFD.
- The production fluid model uses PBF motion at an authored cadence, sampled
  hydrostatics/linear drag, declared cavities and controller swimming. Exterior
  fluid/body momentum is deliberately not conserved. Required approximate-mode
  gates pass; local particle agitation and sub-particle-scale equilibrium have
  explicit limitations. This is not a high-fidelity pressure-coupled solver.

## Final stabilization gate — 30 September 2026

The final clean Release run passed **61/61 production suites**, actual async
**12 cases / 246 assertions**, current-schema persistence validation, default-async
editor Play/Stop, standalone startup, and four 900-step classic/terrain near/far
harnesses with identical near/far physical payloads. Both shipped fluid performance
cases passed. Engine/test/script source fingerprints and protected paths were
unchanged throughout execution. The full gate took 569.738 seconds, including
217.849 seconds for the fresh build. No mandatory regression required repair.

Historical broad-run failures and rejected experiments remain unchanged; their
outputs are not relabelled. The prior 60/61 run is superseded as current acceptance
evidence by this complete 61/61 run, not erased.

Known accepted limitations:

- Six optional micro-diagnostics remain visible: individual-particle micro-rest
  and sub-particle-sized-body settling. `--strict-diagnostics` still reports
  failure; physical tolerances were not widened.
- PBF micro-agitation, filled local columns/internal-air-pocket resolution,
  sampling order, nonuniform missing-data interpolation and finite collision
  sampling remain approximate. Unresolvable hard overlap fails explicitly.

Default production particle cadence is 30 Hz, rigid physics 60 Hz. Exterior
hydrostatic support owns the body's response; exterior particle collision is
one-way. Contained particles use declared geometry and finite-mass contact
exchange. The query-only player samples actual interior fluid and does not push
particles. Neither exact exterior momentum nor complete energy conservation is
claimed. The protected high-fidelity prototypes remain future research.

## Performance and evidence scope

| Workload | Measured value | Evidence/scope |
|---|---:|---|
| 1,500 resting crates | 14.266 ms prior seven-run median; 12.059 ms final-gate reported step | Final gate reused the existing broadphase fixture once; the single step is not a new seven-run median |
| 32 active offset compounds | 6.279 ms mean-step median | FTFT4 closure; known costly case |
| Classic fluid, 125 particles | 6.961 ms particle median; 10.008 ms maximum | Final clean Release; 360 rigid / 180 particle steps |
| Terrain fluid, 125 particles | 11.790 ms particle median; 14.245 ms maximum | Same run/count |
| Classic hybrid amortized cost | 4.199 ms / 60 Hz rigid frame | Includes coupling/field/player sampling |
| Terrain hybrid amortized cost | 5.661 ms / 60 Hz rigid frame | Includes coupling/field/player sampling |
| Larger body-reference pools | Approximately 15–38 ms particle medians | Stress fixtures, not shipped-125 performance scope |
| Normal editor/runtime frame | Not benchmarked here | Fixed-step CPU timing excludes renderer/frame cost |
| Async application | 12 cases / 246 assertions pass | Actual nonblocking GL application, not a latency benchmark |

[Final evidence](evidence/stabilization/final-gate/README.md) contains the complete
61-suite results, source/binary fingerprints, raw commands/logs and performance
CSVs. The earlier candidate evidence remains intact. Compiler warnings: zero.
Offscreen runtime EGL messages are retained in logs; no human visual validation
is claimed. Prior failed candidates and raw outputs remain unchanged.

## Future capabilities

Live origin rebasing, deformable structures, high-fidelity cut-cell fluids,
animation/AI/networking and render-to-texture/portals remain roadmap features.
The P1-C/P1-C-M/P1-PF research prototypes are preserved and are not integrated
into production.

**JUDAS STATUS: READY FOR NEW FEATURE DEVELOPMENT**
