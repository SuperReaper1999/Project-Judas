# M90 — assembly travel candidate

**Automated Linux candidate; human acceptance pending.** Main remains the accepted
M89 baseline `7f7b617907a81d65bcbdd931f9989f01c4c72dd9`. This checkpoint belongs to
Experimental only. No tag, main promotion, Windows or Android M90 pass is claimed.

## What changed and why

The previous M85 path deliberately rejected wheels, powertrains, navigation,
articulations, deformables and effects because their world-space continuation had
no translation contract. The unchanged real mechanical/navigation fixture rejects
on the preserved baseline library. M90 extends that same single-world transaction;
it does not add separate travel physics or recreate bodies at each origin change.

Component owners now translate position-bearing history and leave local/rotational
state intact. Streaming decodes old-chart kinematic targets before validation and
retains the corrected command at private publication; deformed nodes, coarse
physical sections and world support anchors decode in the current chart. Script
camera caches use the new optional `onOriginShift` notification after publication.
The 8 km chart, safe fixed boundary and unsupported-simulation guards remain.
See [architecture and exact exclusions](../../M90_ASSEMBLY_TRAVEL.md).

## Lab / package

- Editable project: `projects/travelling_lab/travelling_lab.judasproj`.
- Earth-sized precision-safe terrain; twelve ordinary M59 service regions over
  about 11 km; real electric source, gear, clutch, differential and four wheels.
- Separate explorer motor, real cargo impulses, local-frame navigation courier,
  script camera/input, pause, save/load and reload. Watch steering is ordinary JS.
- Export: `.cache/m90/export`; moved playable copy:
  `/tmp/judas-m90-travel-20261016`. 8 dependency assets, 2 scenes, 54,708,765 bytes.
- Native editor Play/Stop leaves the authored scene identical. Export's own runtime
  launches from unrelated `/tmp`, drives through a chart transition and renders
  the included screenshot. The package harness skips most draw frames; its elapsed
  time is **not** used as gameplay FPS evidence.

## Precision and continuity

| Independent measured control | Result |
|---|---|
| Flat vehicle + motor + hinge, shifted vs unshifted, 180 continuation steps | max position 0.2744 mm; max velocity 0.001704 m/s |
| Arbitrarily rotated same physical fixture | max position 1.8271 mm; max velocity 0.001062 m/s |
| Native ragdoll/socket/skin | identities retained; actual bone endpoint and socket translate; model skin error 0 |
| Matched strained cloth continuation | node error 9.3674e-11 m; velocity error 6.4518e-10 m/s |
| Suspended/cold-restored deformed node after 1,024/-512/512 m chart change | 4.2309e-14 m in exercised binary64 fixture |
| Actual powered planetary application | >2,080 m, 4 origin transitions; wheel contacts survive; departed stations really unload |
| Normal save/load after driving and dismounting | actual absolute chassis endpoint, momentum and exact rotor/command bytes agree; supported motor resumes |

These are scoped physical checks, not bit-identical or unrestricted distant-world
simulation promises. Native body/joint handles retain identity during live changes;
streamed/cold reconstruction deliberately allocates fresh safe identities.

## Performance

Release on i7-4790 / GTX 970, native NVIDIA OpenGL 3.3, 1/60 s authoritative
steps. Application test renders every frame at 1024×768 with swap interval zero.
Raw samples: [samples.csv](linux/travel/samples.csv); four shift-step observations:
[transitions.csv](linux/travel/transitions.csv). No simultaneous CPU-intensive
Linux/Windows workload was used.

| Full application measurement | Median | p95 | Worst |
|---|---:|---:|---:|
| Fixed-step systems | 0.409 ms | 0.705 ms | 11.199 ms |
| Scene submission | 2.143 ms | 3.750 ms | 9.974 ms |
| Outer frame through presentation/draw | 3.190 ms | 6.249 ms | 17.037 ms |

Four shift-containing fixed steps ranged from the values in the raw CSV to a
maximum 6.279 ms. This includes normal fixed work, not an isolated
origin-only timer. Median uncapped frame time corresponds to roughly
313 FPS; this is one modest lab on this host, not a GPU-only cost
or multi-vehicle scaling claim. The assembly controls measured approximately
0.057/0.074 ms median fixed work (flat/rotated). One real four-wheel rover, its
independent character, nearby station/cargo/courier and bounded planet collision
are active; the editor sample reports 25 bodies and 45,600 rendered triangles.
The moved-package smoke peaked at 147,444 KiB process RSS. Configured terrain
limits stay 256 leaves / 512 resident tiles and 128 collision tiles.
There is no speedup percentage: the baseline rejects the assembly travel operation.

## Validation actually run

- **136 new focused checks pass**, plus 13 existing origin-physics checks: vehicle
  motion, arbitrary rotation, real Detour queries/corridors and negative old-chart
  sample, ragdoll/socket/IK/physical authority, particles, retained mechanical/
  kinematic/cloth/section state, full saves, local-player view and callback failure
  isolation. Audio checks actual voice cursor, one-shot ownership, backend position,
  Doppler continuation and teardown.
- CharacterMotor: 32 pass. M77 physical controller: 119 pass. Physics continuation:
  112 pass. Ordinary navigation: 77 pass. Navigation authoring-dependency negatives:
  38 pass on an owned prepared fixture.
- Wheel, powertrain, wheel lifecycle, streaming, physical animation, section material,
  gravity-selection and local-player JS checks pass apart from the two stale
  save-envelope assertions described below. Save exclusion: 77 pass; current save
  storage: 36 pass, including 19/20/21 supported and 22 rejected participants.
- API drift + live enumeration + TypeScript 5.9.3 pass: 37 exports, 422 public
  members, 286 native operations, 17 lifecycle callbacks, 45 checked JS examples,
  4 deliberate drift negatives. Origin callback executes in the actual VM.
- Native editor authoring/Play/Stop, startup/reload, ordinary standalone export and
  moved-package launch pass. Full production suite was not rerun for this focused
  integration. No fresh Windows/Android claim and no human acceptance invented.

### Unchanged failures / genuine follow-ups

Two old regression assertions fail identically on baseline and candidate:
M77 foundation expects the 18th participant to fail (33/34 pass); M82 lifecycle
expects the 20th to fail (96/97 pass). Their physical assertions pass. These tests
were not weakened or rewritten. Modern SaveStorage has a 21-participant bound
since subsequent optional systems were added; its 22-negative check passes.
Original and baseline-relinked results are preserved in [failures](failures).

Initial CLI mistakes (missing fixture arguments) and test-construction mistakes
are distinguished from engine failures in [provenance](PROVENANCE.md). The early
watch drift into a real shelter post is preserved as a content/control failure;
ordinary JS heading feedback corrected it without a native exception or teleport.

## Remaining limits

Active liquids/PBF, atmosphere and combustion still block origin changes; the
single 8 km chart, one planetary collision focus and existing articulation
suspension pins remain. Arbitrary script JSON needs explicit coordinate ownership;
suspended scripts rebuild caches from absolute data. No independent fluid research
or FLU-001 change, no protected evidence rewrite, no new gameplay C++.
Windows and Android M90 validation are unperformed and nonblocking.

## Human acceptance

Use the **single consolidated checklist and controls** in the
[lab README](../../../projects/travelling_lab/README.md#human-checklist): manual
movement/driving, at least two kilometres of watch travel, braking/dismount,
cargo/courier interaction, travelled save/load, pause/reload, editor Play/Stop and
exported standalone. The candidate is ready for that review; it is not marked
human-validated.

Exact source/content inventory is `CHANGED_FILES.txt`; source/content SHA-256
receipts are `FINAL_FINGERPRINTS.sha256`, with separate `BINARIES.json` and evidence
hashes. The user-authorized README update restores Conner / SuperReaper1999's top
copyright; the user-authored `docs/DontReadMe_Final.md` archive is included byte-for-byte.
