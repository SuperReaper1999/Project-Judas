# Two pre-existing issues found while testing continuous impacts

> **Status:** Issue 2 is fixed, and issue 1 is fixed for planar slopes (sheets and slabs). Fast
> boxes still cross the curved troughs, for separate reasons. See
> `docs/evidence/impact-robustness/RESULTS.md`.

Both were found while building a tunnelling check for the stall-triggered fallback in this PR.
Both reproduce on unmodified `3bdf09e9` (Linux, Release). They are reported here and **not
changed by this PR**. The probes below are self-contained `PhysicsWorld` snippets.
`Sheet(60,60,false)` is the flat 3 m × 3 m sheet cooked by `tests/ImpactMeshCostTests.cpp`
(7,200 triangles, one-sided, normal +y).

## 1. Fast thin bodies cross sloped triangle meshes

A 4 × 2 × 6 cm box (0.2 kg) falls straight down onto the sheet, which is tilted about x.
Depth is measured in the sheet's frame, only while the box is over the sheet.

```cpp
PhysicsWorld w;w.Init();const glm::quat tilt=glm::angleAxis(glm::radians(deg),glm::vec3(1,0,0));
w.CreateShape(Shape::Cooked(Sheet(60,60,false),"s"),{{0,0,0},tilt},false,1,.6f,0);
auto h=w.CreateDynamicBox(tilt*glm::vec3(0,1.2f,0),{.02f,.01f,.03f},.2f,.4f,0);
w.SetLinearVelocity(h,{0,-speed,0});
for(int i=0;i<30;++i){w.ApplyLinearAcceleration(h,{0,-9.81f,0},1.f/60.f);w.Step(1.f/60.f);}
```

Deepest point below the surface, in metres. The step at which the box first went more than
5 cm under is in brackets:

| Tilt | Mesh sheet, 10 m/s | Mesh sheet, 20 m/s | Mesh sheet, 40 m/s | Box primitive slab, 40 m/s |
|---|---|---|---|---|
| 0° | 0 | 0 | 0 | 0 |
| 5° | 0.15 (7) | 3.85 (3) | 3.75 (1) | 0.087 (1) |
| 15° | 1.55 (8) | 1.79 (3) | 1.67 (1) | 0.052 (1) |
| 30° | 0.01 | 0.46 (4) | 0.61 (2) | 0.287 (2) |
| 45° | 0.09 (10) | 0.01 | 0.09 (2) | 0.086 (2) |

The box primitive slab is 1 m thick and the same size as the sheet.

- In the trough of the same test, a box at 40 m/s produces one impact event in step 1. Its
  post-step velocity, (0, −27.2, 9.1) m/s, still points into the surface along the local
  normal: about −15 m/s, against −31 m/s before the impact. It ends the step 0.23 m beneath
  the surface and keeps going.
- So the impact is detected, but on a non-horizontal surface it does not remove the
  approaching normal velocity.
- Flat (0°) cases are always correct. The box-primitive slab is not crossed only because it is
  thick, yet the box still sinks up to 29 cm into it.

`judas_impact_mesh_cost_tests` reports this as `fastBoxesThrough`: 24 such boxes per sheet,
from steep to grazing. At `3bdf09e9` the counts are:

| Sheet | Boxes crossing (of 24) |
|---|---|
| flat 6×6 | 0 |
| flat 60×60 | 3 |
| each trough | 24 |

## 2. `PhysicsWorld::Step` throws `angular midpoint did not converge`

`AngularDriftGenerator` (`src/RigidBody.cpp`) runs at most 12 Newton iterations on the
implicit-midpoint body rotation and then throws by design ("there is no unconserved predictor
fallback"). The same 4 × 2 × 6 cm, 0.2 kg box above is driven to 500–1,000 rad/s by impacts at
20–40 m/s: impact spin-up is normal for a small, light box hit off-centre. At those rates the
solve fails, and the exception leaves `Step`. In a running game that is a crash.

It reproduced with the box falling at 20 m/s with a 30 m/s sideways component, 15 rad/s of
initial spin, onto a horizontal box-primitive floor. It also reproduced in several of the
sloped-mesh probes above, with no initial spin at all.

Possible directions, both the engine owner's call:
- Split an over-large rotation (large `|ω|·Δt`) into sub-intervals that each converge. Each
  sub-interval keeps the same conservation properties.
- Give the solver a solvable bound on the spin.

At minimum, the failure should not escape `Step` as an exception.
