# Mechanical Damage Lab

An ordinary editable Judas project for M83. Open `damage_lab.judasproj` in the
editor, or launch its exported `judas` executable. The project supplies controls,
camera, watch sequence and HUD through `Assets/scripts/controller.js`.

## What is physical

- **Orange bench:** a tetrahedral plastic arm connects a fixed bearing stand to
  an ordinary finite-mass carrier. Its wheel uses that carrier's real transform.
- **Green bench:** the same geometry and loads with stiffer material and a
  stronger mount. It is a physical comparison, not a scripted pass/fail actor.
- **Purple bench:** an M79 cohesive material path transmits load from root to
  carrier. Breaking that path disconnects the powertrain even when its tip
  attachment remains present.
- **Orange and green buggies:** four independent soft suspension arms, four
  finite carriers and four normal M81 wheels. A small M82 electric shaft graph
  drives them through bounded clutch links.

Noncollinear material attachment nodes exchange point impulses with the real
body. Bending changes bearing position/orientation and thus suspension contact,
traction direction and mechanical load. Mount overload and cohesive failure are
engine results. Links have optional physical-connection references to the
material path; gameplay never turns them off to simulate damage. Released
carriers keep their ordinary mass, collision and motion.

The wheel mesh is presentation geometry. The wheel's contact proxy and the
deformable arm's current boundary are physical geometry. The project never
writes a bent transform, wheel mount, plastic rest state or failure flag.

## Controls

| Control | Action |
|---|---|
| Tab | Select the next bench, buggy, spawned prefab or loaded annex |
| W / S | Forward / reverse source input |
| A / D | Buggy steering |
| Space | Mechanical brake |
| B | Apply/remove equal sustained bearing loads |
| I | Launch an ordinary finite-mass rigid ram at the selected bearing |
| H | Apply/remove overload; actual strength decides failure |
| V | Watch the scripted input/load tour |
| Mouse | Orbit the independent presentation camera |
| P | Spawn a complete material-supported buggy prefab |
| F3 | Load/unload the retained annex bearing region |
| 1 / 2 | Flat / genuinely oblique uniform-gravity scene |
| F5 / F8 | Save / load the complete physical state |
| R | Reload the authored intact scene |
| Escape | Pause / resume |

The watch tour changes inputs and loads only. It finishes after 36 seconds and
leaves the physically damaged result visible. Press R for a fresh authored
scene. The tour is not a navigation controller; manual steering remains useful
on the finite deck.

## Editing

Select a material arm in the hierarchy to edit its generic deformation material,
named attachments and finite force/torque strengths. Select a drive owner to
inspect its normal powertrain graph and material connection. Change compliance
or mount strength in content to compare a different physical outcome. The
green control is intentionally stronger than orange.

Each arm has a material mass owner; each carrier has a separate rigid-body mass
owner. The logical deformable entity does not also own a rigid body. Explicit
project collision layers prevent attached material from colliding against its
own pin support while retaining ordinary environment contact. This is authored
filtering, not a scene-name branch.

`Assets/prefabs/buggy.judasprefab` packages the complete referenced assembly.
`Scenes/annex.judas` and `Assets/worlds/lab.judasworld` exercise normal retained
streaming: request the annex with F3, then select it with Tab to bend, power or
strike it; release and revisit it to inspect the retained damage.
`Scenes/oblique.judas` rotates the physical geometry and the selected
uniform field together; it is not a visual-only rotation.

## Human checklist

1. Apply B, then unload. Orange should retain misalignment while green largely
   recovers; the carrier and wheel must follow the real material.
2. Power a still-connected bench or buggy. Observe actual wheel motion and RPM.
3. Apply I and H. A weak mount or purple material path should fail and stop
   transmitting drive without deleting the detached carrier.
4. Compare the stronger control under the same loads.
5. Drive the buggies and inspect suspension/traction after damage.
6. Repeat in the oblique scene.
7. Save a damaged state, reload it, and verify the damage and disconnected
   operation remain. Revisit the annex and test a spawned independent prefab.
8. Pause/reload/Stop; then repeat representative checks in exported standalone.

Automated measurements and current modelling limits are recorded in
`docs/evidence/m83` and `docs/M83_MECHANICAL_DAMAGE.md` in the source repository.
Human acceptance and native Windows validation are separate from those checks.
