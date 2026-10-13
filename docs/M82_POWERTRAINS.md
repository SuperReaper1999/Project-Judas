# M82 — Judas learns to roar

**Human validated by the operator on Linux on 2026-10-13.** Development starts from
the M81 accepted checkpoint `b74906350084ab20d01b09774dfc56711b4d97cb`.
Linux is the automated acceptance platform; Windows validation is informational
and does not imply native hardware acceptance. The operator authorized Stable
promotion; see [acceptance](evidence/m82/HUMAN_VALIDATION.md) and
[evidence](evidence/m82/RESULTS.md).

## Architecture and ownership

M81 already supplies wheel spin inertia, suspension, query contact, friction,
direct drive torque and braking. PhysicsWorld supplies ordinary rigid angular
inertia, impulses and M45 hinges. None of these supplied a general shaft network,
engine load response, clutch or differential. M82 adds an optional Powertrain
component that connects those existing degrees of freedom.

An assembly owns bounded arrays of named **shafts**, **sources**, **links** and
**brakes**. It may belong to any ordinary entity. Shafts can be:

- **Internal:** explicit finite scalar inertia, optional ordinary rigid bearing.
- **Wheel:** the existing M81 wheel spin degree of freedom and chassis reaction.
- **Body:** angular motion of an ordinary rigid body about an authored axis,
  optionally measured relative to another ordinary bearing body.

Internal rotors retain their momentum coordinate. External terminals never own
an extra copy of wheel/body inertia. RuntimeWorld constructs the effective
inverse-mass matrix from those actual degrees of freedom, including off-diagonal
coupling when shafts share a bearing. Mechanical impulses react on ordinary
bearings/chassis; normal PhysicsWorld still owns their transforms, contacts and
gravity. Generation-safe entity/body lookup prevents stale endpoints acting on
new allocations.

The component owns no input, driver, gears-to-select policy, camera, fuel system,
vehicle class or wheel count. A script may supply custom bounded source torque,
drive a wheel directly without a Powertrain, or mix powered and unpowered wheel
assemblies. Renderer displays physical results and owns no mechanical state.

## Source characteristics

All units are SI: shaft angular velocity in rad/s, RPM in revolutions/minute,
inertia in kg m², torque in N m and power in W.

```text
RPM = omega * 60 / (2*pi)
combustion torque = input * min(maxTorque, piecewiseLinearCurve(RPM))
electric torque = input * min(maxTorque, maxPower / abs(omega))
custom torque = customTorque
source impulse = (source torque + starter * starterTorque) * dt
```

The electric zero-speed case uses its finite torque limit. Combustion input is
nonnegative, and its authored increasing RPM curve returns zero outside the
curve range. Electric input is signed. Custom torque is signed and bounded by
the authored maximum. These are generic torque-producing characteristics, not
a fuel/combustion chemistry model or a motor electrical circuit simulation.

Optional bearing viscous drag is integrated implicitly, including shared
bearing response. An external shaft's effective inertia comes from its physical
endpoint; its generic authored inertia field is not added again. Its
`initialAngularVelocity` is also ignored: initial wheel/body motion comes from
the physical endpoint. That setting initializes only internal scalar rotors.

## Mechanical connections and solution

For `ratio = output angular speed / input angular speed`:

```text
gear / clutch: omega_out - ratio * omega_in = 0
open differential: omega_left + omega_right - 2*ratio*omega_in = 0
```

Negative ratios reverse direction. Ideal gears and differentials have no hidden
torque limiter. The differential imposes the average speed relation and applies
equal output impulses; unequal wheel loads may produce unequal output speeds.
It is an open differential, with the corresponding low-grip limitations.

A clutch supplies a bounded OUTPUT angular impulse
`abs(lambda) <= maxTorque * engagement * dt`. A mechanical brake supplies an
analogous passive impulse against its shaft's relative speed. Capacity-limited
rows retain real slip; they do not set a desired speed or overwrite velocity.
Brake impulses do not create reverse motion by overshooting rest.

The small double-precision active-set solve minimizes the convex velocity/energy
quadratic with unbounded ideal rows and bounded clutch/brake rows. Rank-redundant
consistent rows are handled without adding an arbitrary spring or numerical
diagonal. Invalid, inconsistent or nonconvergent graphs return a diagnostic and
no partial physical impulse for that projection.

Within one authoritative fixed interval, sources integrate once before wheels,
then mechanical constraints project the wheel/contact load response without
integrating the sources again. Clutch and brake capacity budgets belong to the
whole interval, so a later projection does not receive a second torque budget.
Final body/wheel state is published after normal physics; rotor presentation
angles advance once. Geometric contacts, CCD and constraints remain existing
PhysicsWorld capabilities.

## Gravity, frames and terminals

Internal/body shaft axes are bearing-local when a bearing exists, otherwise
world-space. Wheel terminals use the existing authored M81 axle convention.
The bearing body's actual orientation rotates the mechanical axis. Neither the
graph nor its solver assumes global Y, a car chassis, or a suspension direction.
Gravity remains the existing Judas field resolver acting on normal bodies.

Body terminals are most useful with an ordinary M45 hinge that constrains the
other motion axes. Powertrain supplies torque about an axis; it does not silently
invent the physical bearing or translational constraint. The lab's flywheel is
an actual dynamic body attached to its static stand by a normal hinge.

## Authoring, prefabs and lifetime

Add **Powertrain** in the editor Inspector and author the bounded graph using
the normal component controls. Names are stable local identities within one
graph; link indices reference that graph's shafts. External terminals and
bearings use the existing entity reference system.

The canonical version-1 `powertrain.graph` record is ordinary scene component
data, not a second resource pipeline. It requires 1–32 shafts, at most 32 sources,
32 brakes and 64 total constraints. Finite coefficients, increasing torque-curve
RPM, nonzero bounded ratios, unique local names and single external terminal
ownership are validated. Duplicate competing assemblies and incorrect physical
endpoint kinds are rejected.

Normal scene/prefab serialization, conditional fingerprints, reference remapping,
save participants and streaming retention carry the component. Runtime commands,
rotor momentum and angles survive supported saves; contact/query scratch and
native handles are rebuilt. Streaming pins physical endpoint dependencies and
must preserve an entire connected assembly. Owner destruction, disable, scene
replacement and Stop retire ordinary runtime state safely. Required missing or
transient endpoints cannot be silently discarded by a strict save transaction.

## JavaScript

`entity.powertrain` returns the generation-aware component handle or `null` when
the entity has no component. `settings` and `state` return detached snapshots.
`valid` checks lifetime; other access through a destroyed owner throws. Mutations
are authoritative `fixedUpdate` work, and invalid commands are atomic failures.

```js
const power = machine.powertrain;
power.set({
  sources: [{name: 'engine', input: .65, starter: 0}],
  links: [{name: 'gearbox', ratio: 1 / 3},
          {name: 'clutch', engagement: .8}],
  brakes: [{name: 'left', engagement: 0},
           {name: 'right', engagement: 0}]
});
const crank = power.state.shafts.find(s => s.name === 'crank');
const measuredRPM = crank.rpm;
```

Named partial commands leave unspecified held commands unchanged. `ratio: null`
restores the authored ratio. Sources, links and brakes are generic; no JS method
encodes accelerating, shifting a car, idling or starting a game vehicle.
The lab implements those policies in its project controller.

See [public reference](judasjs/powertrains.md), [types](judas.d.ts) and
[the API inventory](judasjs/API_INVENTORY.md). Wheels remain independently usable
through [their direct API](judasjs/wheels.md).

## Editable demonstration

Open [Powertrain Lab](../projects/powertrain_lab/powertrain_lab.judasproj):

1. Orange four-wheel buggy: combustion crank, manual ratio, bounded clutch,
   open differential and driven rear tyres.
2. Cyan six-wheel rover: independent electric motors, without a gearbox.
3. Purple stationary machine: electric source, reduction and real hinged flywheel.

Flat, oblique uniform-gravity and spherical radial-gravity scenes use the same
components. Real payloads, low tyre grip, source/brake loads and clutch/gear
controls change the mechanical outcome. HUD RPM/torque/slip values come from the
solver, and presentation cameras follow interpolated ordinary body poses.
Prefab spawn, streamed garage, save/load, pause/reload and normal export use
existing public APIs. Controls are in the [project README](../projects/powertrain_lab/README.md).

## Limitations

- Scalar shaft mechanics omit full rotor gyroscopic transport, flexible shafts,
  gear backlash, belt compliance and detailed torsional vibration.
- Sources model authored torque/power curves, not thermodynamics, fuel usage,
  battery energy, electrical circuits or thermal damage. Projects own such policy.
- Differential is ideal/open; automatic limited-slip or torque-vectoring policy
  is not built into the component.
- Ideal abrupt ratio changes may dissipate energy through the physical impulse
  projection. For smooth shifts, a project should open/re-engage its clutch.
- Mechanical projection is coupled to ordinary wheel/contact updates within the
  fixed interval; it is not an industrial multi-domain implicit simulator.
- Separate graphs execute in ascending owner entity-ID order. Shared bearings
  physically couple their motion, but only shafts within one graph participate
  in a simultaneous mechanical solve; author tightly coupled machinery together.
- Existing M81 query-contact, tyre geometry, sleep-island and high-speed limits
  remain. M82 does not redesign tyre contact or rigid-body CCD.
- Graph editing is deliberately compact rather than a visual drivetrain designer.
- Windows M82 functional/hardware validation is not inferred from Linux results.

The [Project Chrono shaft/gear/clutch tutorial](https://api.projectchrono.org/8.0.0/tutorial_demo_powertrain.html)
is a modelling reference. Judas implements its own bounded shaft solver and adds
no Chrono dependency or copied runtime implementation.

Protected history, independent fluid research, existing games and unrelated
deferred findings remain unchanged. Human acceptance comes from the operator's
subsequent desktop test, not automated delivery. Stable promotion is explicitly
authorized. Main may follow accepted Stable through an explicit operator
fast-forward request; see the [branch workflow](../AGENTS.md).
