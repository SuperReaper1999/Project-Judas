# Character Lab — M70 candidate

Ordinary project: `character_lab.judasproj`. Open in the editor or run with the
normal runtime. Three registered scenes share immutable imported models while
each instance owns its target/pose/physical state.

## Controls

- **F1:** wall palms/soles; **F2:** translated/tilted board; **F3:** partial response.
- **T:** change spacing/tilt and board step variant. **I:** impossible wall contact.
- **C:** Idle/Wave crossfade. **L:** existing masked Carry layer. **H:** existing
  HeadHidden clip layer (no new joint-subtree visibility API).
- **WASD / Space:** script-driven motor motion/launch in the physical scene.
  **Mouse:** inspect from the project camera.
- **J:** ordinary impulse at the mapped arm body; **K:** zero/restore drive effort.
- **G:** full active articulation with explicit motor handoff; **P:** passive.
  **N:** request animation return with an explicitly chosen, collision-validated
  capsule placement and visual blend. Refusal is displayed; no automatic get-up.
- **Z:** ordinary prefab instance in the physical scene. **F6/F7:** modern slot save/load. **R:** reload.
  **Escape:** authored pause menu, pointer release/resume.

Wall/board target frames are project calculations using explicit imported mappings
and ordinary transforms. The shared native solver changes skeletal locals only.
Each target's distance/angular residual is shown separately. Physical mode uses
the same bodies/joints/query/contact world; visual result comes from the resolved
physical pose, not a scripted impact clip.
The board collider is an ordinary prescribed static body. Its separate body-free
visual clone interpolates the known fixed transforms at the bones' presentation
alpha; collision detection, targets and physics use only the actual collider.
Animation return deliberately places the motor above the floor and lets normal
gravity settle it. A blocked placement/path is refused and displayed, rather
than teleporting through geometry or performing an automatic get-up.

## Author / reproduce

`python3 scripts/create_m70_lab.py --project projects/character_lab --overwrite`
uses shipped `judas_scene_author` templates, named patches, import recipes and
skeleton fitting. **Overwrite replaces this lab's authored edits intentionally**;
use a fresh destination to inspect reproduction without changing the reviewed copy.
The existing M67 skeleton picker/normal inspector edits the same component data.
No protected evidence or consumer workspace is read as a runtime content path.

Source rigs each have 65 joints including unmapped helpers/fingers, six parts,
two differently ordered skin palettes and four clips. The alternate is renamed
and differently proportioned, with an explicit `Authoring/joint-names.json` map.
The lab does not retarget between rigs. `Sources` contains original glTF and the
import service's own project-owned source copies/recipes, not restricted art.
The physical subset maps the neck, leaving the independently hidden visual head
unmapped; hiding it never requires a collider with a singular scale.

Original source art, scripts and layout are CC0-1.0; see `Assets/LICENSE.txt` and
`Sources/LICENSE.txt`. DejaVu keeps its font license under `Assets/fonts/`.
No rights are inferred for Claude's/M66's original Skate character.

## Review boundary

This is an uncommitted candidate. The operator accepted the other lab behaviours;
human re-test of the passive-mode performance correction remains pending.
Numerical, physical, lifecycle, packaging and M56 measurements are recorded in
[`docs/evidence/m70`](../../docs/evidence/m70). The earlier one/ten-instance passive
benchmark precedes the actual two-rig human failure and correction; see the
[preceding passive follow-up](../../docs/evidence/m70/development/passive-follow-up/REPORT.md).
Those CPU measurements do not certify desktop FPS or final acceptance.
Windows portability is retained;
Linux automated results cannot certify Windows or replace interactive review.

## Current consumer and landing-freeze follow-up

The subsequent human G/floor freeze rejected the preceding passive performance
handback. The [consumer and numerical landing follow-up](../../docs/evidence/m70/development/passive-crash-follow-up/REPORT.md) records
the exact core reproduction, rotation-sampling correction, explicit owner-contact
subscription, API conventions and affected results. Windows M70 moved-package
validation remains outstanding. The corrected landing candidate requires human
re-test; prior successful measurements retain their preceding-candidate scope.

The corrected package is `/tmp/judas-m70-character-lab-corrected-20261008` on the
Linux development host. **G** in the initial wall scene tests the reported floor
landing; **F3**, then **P**, tests passive motion. The exact frozen-state replay
finishes in about 30 ms rather than remaining stuck beyond 35 seconds. All four
600-step public-control runs finish with finite poses and no script diagnostics.
Passive fixed-step median/p95/max is 1.317/8.643/34.539 ms; after two seconds it
settles to 1.245/3.682/5.213 ms. Full active rigs fighting dense wall contacts can
still spike to 65.979 ms (69.272 ms after passive-to-active); comfortable frame
rates in those cases are not claimed. No cadence or contact-quality reduction
was used. Human landing re-test and Windows moved-package validation are pending.

Mapped-body contacts can be delivered to an owning script with the generic
default-false `entity.ragdoll.receiveContactEvents` option. The inspector and
authored `ragdoll.receive-contact-events` property use the same subscription.
Internal same-articulation contacts are not forwarded; external unscripted bones
and ground work through ordinary collision callbacks with `selfBody`/`selfJoint`.
