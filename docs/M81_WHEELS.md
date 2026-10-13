# M81 — Judas learns to roll

**Experimental candidate; human validation pending.** Development starts from
M80's accepted checkpoint `bc1fd286d75b6db843fa9942b8d21c946c2e5c2a`.
Linux is the automated acceptance platform. Windows evidence is informational
and does not imply hardware acceptance. Results and measured performance belong
to [the M81 evidence](evidence/m81/RESULTS.md).

## Ownership and representation

Judas had ordinary rigid bodies, M44 casts and M45 constraints, but no current
wheel/tyre suspension component. The historical spacecraft vehicle adapter is
not used here. M81 adds an optional Wheel component referencing an ordinary
chassis through the existing EntityId system. Dynamic authority is required
for wheel forces; ordinary static/kinematic handoff leaves the wheel safely
unavailable for physical response. A separate visual entity
owns each wheel component; there is no required axle layout, wheel count,
drivetrain, engine, gearbox or vehicle controller.

`WheelInstance` owns independent scalar axle spin, suspension travel and held
torque/brake/steering commands. `RuntimeWorld` owns the instances and their
lifetime. M44's broadphase/sphere-cast path supplies real contact geometry;
PhysicsWorld owns the resulting chassis/ground motion. Renderer consumes the
derived, interpolated wheel pose and owns no wheel simulation state. Project
JavaScript chooses inputs, cameras, load changes, selection and game rules.

The physical contact representation is a swept sphere, independent of the wheel
mesh. The wheel is not another rigid body or a parallel vehicle solver. Its
unsprung translational mass belongs in the authored chassis mass; its independent
spin inertia is explicit. Normal/tangent rows include chassis/ground effective
mass and the wheel spin degree of freedom. Axial contact torque changes wheel
spin, while the non-spinning contact torque reaches the chassis. This avoids
counting tyre torque twice. Drive and brake apply equal/opposite axle reaction
to the chassis through a generic angular impulse operation.

## Suspension and tyre model

The chassis-local mount orientation defines down **-Y**, axle **+X** and forward
**-Z**. Steering rotates about mount-local +Y. These are authored local axes,
not global-up assumptions. Chassis orientation rotates the whole assembly; the
normal Judas gravity resolver continues to act on the chassis. Contact normal,
gravity direction and suspension direction remain separate.

Positive spin is rotation about mount-local -X, yielding forward -Z rolling
on a level contact. The +X axle convention identifies its geometric line,
not the sign of the scalar rolling velocity.

A sphere cast from the mount along down over maximum travel supplies wheel
centre travel, surface witness and normal. The chassis is ignored, sensors are
excluded, and both sides' ordinary M39 layer/mask policy is respected. Required
and excluded tags use the existing classification registry. Ground point
velocity includes prescribed kinematic motion and dynamic rotation.

For projection `p = dot(surfaceNormal, -suspensionDown)`, spring compression
`x = restLength - length`, time step `h`, row inverse effective mass `K`, spring
stiffness `k`, damping `c` and normal relative velocity `v`, the compliant
unilateral normal impulse is:

```text
j_n = max(0, h * (k*x - c*v/p) / (p * (1 + K*(h*c + h*h*k)/(p*p))))
```

This backward-Euler response accounts for the resulting motion in spring and
damper response; it does not hide extra integration substeps or lower physics
cadence. Minimum travel is a unilateral bump stop expressed as an ordinary
velocity impulse. Initial-overlap casts do not provide penetration depth, so
the wheel does not fabricate a positional correction or teleport its chassis.

The longitudinal/lateral 2×2 effective-mass solve requests slip cancellation.
The impulse pair is projected onto a Coulomb disk with radius
`sqrt(wheelFriction * groundFriction) * j_n`. Saturation permits slip rather
than guaranteeing grip. Both tangent impulses affect real chassis motion,
spin and dynamic ground; a non-dynamic ground supplies velocity but takes no
dynamic response. The state reports pre-impulse slip and actual applied impulse.

Signed drive torque is in N m. Brake torque is nonnegative and bounded so that
it opposes relative axle spin without creating a reverse impulse past rest.
Authored rolling resistance adds torque
`rollingResistance * previousNormalForce * radius`. Wheel angle integrates
spin relative to chassis and is wrapped for finite presentation; spin velocity
remains the physical absolute axle value.

## Authoring

1. Create an ordinary dynamic rigid-body chassis. Its mass/inertia include the
   vehicle's carried mass and translational wheel mass.
2. Create separate wheel visual entities and add **Wheel** in the Inspector.
   Select their chassis through the normal entity reference picker.
3. Set chassis-local mount, orientation, radius, min/rest/max travel, stiffness,
   damping, spin inertia, friction and rolling resistance. No Rigidbody is
   required on the wheel entity itself.
4. Author M39 collision layer/mask and optional required/excluded tags.
5. Attach an ordinary script that supplies direct commands in `fixedUpdate`.

For a calm flat fixture, estimated per-wheel sag is its supported static load
divided by spring stiffness. This is an authoring estimate, not a car-only rule
or an imposed equal-weight drivetrain. Centre of mass, wheel arrangement,
terrain and suspension configuration determine actual load distribution.

Wheel visual owners require unit scale and cannot simultaneously own a rigid
body, CharacterMotor, ragdoll, deformable or socket that competes for root
placement. Scale the source mesh itself to the intended size. The chassis must
be an ordinary rigid body; invalid competing ownership is rejected rather
than silently resolved by update order. Static/kinematic authority may be
authored or selected normally, but wheel response requires a dynamic chassis.

Normal scene serialization, conditional content fingerprints, prefab reference
remapping and editor property/override paths carry the component. No new asset
pipeline is introduced. Saves contain a bounded `wheels` participant and retain
commands, spin, angle and travel/load state; contact body handles are rebuilt.
Streaming retains independent wheel state and pins cross-region chassis/live
ground dependencies. Whole-assembly adoption is required. Scene replacement,
owner destruction and Stop retire the component; native body-slot reuse does
not substitute for the authored chassis identity.

Strict saves reject a dangling/transient required chassis rather than dropping
its wheel state. Destroy an assembly's wheel owners alongside its chassis.

## JavaScript

See [Wheel API](judasjs/wheels.md), [copyable script](judasjs/examples/wheel.js),
[types](judas.d.ts) and [auditable inventory](judasjs/API_INVENTORY.md).

```js
const wheel = wheelEntity.wheel;
wheel.set({driveTorque: 180, brakeTorque: 0, steeringAngle: .25});
wheel.configure({friction: .8, springStiffness: 24000, damping: 2500});
const contact = wheel.state; // detached snapshot
```

Mutations are authoritative fixed-update work. Commands persist until changed;
there is no built-in input, gameplay state or camera. Configuration changes are
atomic and restricted to generic physical coefficients; geometry/references
remain authored. Presentation cameras use `presentedTransform` in
`presentationUpdate`, preserving authoritative physics/gameplay timing.

## Editable lab

Open `projects/wheel_lab/wheel_lab.judasproj`. The ordinary registered scenes
include a four-wheel chassis, a six-wheel rover, uneven surfaces/load controls,
an oblique uniform-gravity setup and a radial-gravity setup. Prefab spawning,
save/load, region residency and scene switches use public project APIs. The
project scripts are inspectable content, not engine demo modes.

Controls and the combined human checklist are recorded in the lab documentation
and [candidate evidence](evidence/m81/RESULTS.md). Human review remains pending
until the operator tests this exact candidate.

## Limitations

- Query-contact spheres have no tyre width/tread/rim geometry, deformable tyre,
  or independent unsprung translation. Chassis collision remains authoritative
  for body/obstacle impacts.
- Scalar axle spin is not a complete 3D wheel rigid body; steering/axis changes
  do not provide a full gyroscopic transport model. Two-wheel balance is not
  automatically supplied by a vehicle controller.
- Travel contact is sampled once per fixed interval, not wheel-specific CCD.
  Very thin terrain, high speed or teleporting supports can exceed that model.
- A nearly lateral contact is rejected for suspension support. Initial overlap
  reports real cast state but supplies no fabricated penetration depth.
- Coulomb grip, the spring/damper approximation and chassis mass distribution
  are game physics; this is not tyre engineering or an industrial vehicle model.
- A chassis supported only by query wheels remains awake under gravity: ordinary
  rigid-island sleeping recognizes rigid contacts/joints, not virtual wheel
  support. Quiet wheel response preserves existing sleep clocks but does not
  manufacture a rigid contact to force sleep.
- Wheel proxy contact uses authored M39 masks/tags; suppressing a rigid-body
  pair through a joint does not automatically suppress this separate query
  proxy. Author its filter to exclude that ground where required.
- Runtime filter/geometry edits are not part of the minimal wheel JS surface;
  author them in the editor. Destroyed/unloaded chassis references cannot act
  on a newly allocated body slot.

Protected history, independent fluid research and unrelated deferred physics
findings are unchanged. M81 does not imply new human acceptance or Stable
promotion.
