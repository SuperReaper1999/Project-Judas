# Powertrain — shafts, torque sources and mechanical connections

[Index](../JUDASJS.md) · [Wheels](wheels.md) · [Physics/joints](physics.md) · [Entities/prefabs](entities.md) · [Saves](saves.md) · [M82 model/authoring](../M82_POWERTRAINS.md)

An optional authored Powertrain component contains a bounded rotational network.
Internal shafts have real rotational inertia. Terminals can refer to ordinary
Wheel components or physical rigid-body rotors, with explicit bearing reactions.
Gears, clutches, differentials and brakes transmit load through that network.
No vehicle layout, automatic shifting, throttle input binding or machinery
behaviour is required by the engine. Wheels still accept direct commands without
any Powertrain component.

## `entity.powertrain`

Returns a `Powertrain` handle, or `null` when no component exists. `valid` checks
owner/component lifetime. Destroying or unloading the owner invalidates retained
handles; other accesses throw `ReferenceError`. Losing an external wheel, bearing
or rotor disconnects that terminal and its incident links. It does not alias a
reused native body slot. Check both aggregate and per-shaft availability when
using a partially available network.

| Member | Meaning |
|---|---|
| `valid` | Read-only owner/component validity. |
| `enabled` | Read/write mechanical gate. Disabled networks apply no source or connection impulses. |
| `settings` | Detached canonical authored graph, including member names, indices and ordinary entity IDs. |
| `state` | Detached measured rotational and solver state. |
| `set(commands)` | Atomically update named held source, connection and brake controls. |

Mutations belong in `fixedUpdate`. Other phases, invalid names, duplicate patch
names, unknown fields, nonfinite values and out-of-range commands throw
`TypeError`; the whole request is rejected. No partial publication occurs.
Reads can be used in presentation/UI callbacks. There is no graph construction
or topology-changing JavaScript API in this milestone; author the component in
the editor/scene/prefab.

## `Powertrain.set(commands)`

Every top-level array is optional; omitted controls retain their values. Each
record identifies a member by its authored `name`, scoped to that component.

- `sources: [{name, input?, customTorque?, starter?}]`: source input, signed
  custom torque in N m, and authored starter fraction. `input` and `starter`
  ranges depend on the source model; see the M82 model reference.
- `links: [{name, enabled?, engagement?, ratio?}]`: enabled gate, clutch fraction
  in `[0,1]` and signed output/input speed ratio. `ratio: null` restores the
  authored ratio. A disabled link represents disconnection/neutral. There is no
  built-in gear-selection or clutch-pedal policy.
- `brakes: [{name, engagement?}]`: fraction in `[0,1]` of the authored torque cap.

For gear/clutch, `omegaOutput = ratio * omegaInput`; for an open differential,
`(omegaLeft + omegaRight)/2 = ratio * omegaInput`. Different output speeds are
allowed. A clutch saturates at its authored **output torque** cap and physically
slips. Gears/differentials impose ideal velocity constraints. Ratios are not RPM
assignments; engagement redistributes momentum and can dissipate energy.

```js
// Authored names belong to this project, not engine concepts.
this.entity.powertrain.set({
  sources: [{name:'engine', input:.65, starter:0}],
  links: [{name:'gearbox', ratio:1/3},
          {name:'clutch', engagement:.8}],
  brakes: [{name:'axleBrake', engagement:0}]
});
```

The copyable [logical-input example](examples/powertrain.js) executes through
the real runtime in focused checks.

## `settings`

Version 1 snapshot: `enabled`, `shafts`, `sources`, `links`, `brakes`.

- Shafts: `name`, `kind` (`internal`, `wheel`, `body`), `inertia` (kg m²),
  `initialAngularVelocity` (rad/s), `viscousDrag` (N m s/rad), `target`, `bearing`,
  `axis`. Terminal inertia comes from actual physics rather than the internal
  `inertia` field. `initialAngularVelocity` initializes internal rotors only;
  external wheel/body terminals use their actual physical initial motion.
  Entity IDs are decimal strings; `'0'` means absent.
- Sources: `name`, `shaft` index, `kind` (`combustion`, `electric`, `custom`),
  `torqueCurve` pairs `[rpm, torqueNm]`, `maxTorque` (N m), `maxPower` (W),
  `starterTorque` (N m).
- Links: `name`, `kind` (`gear`, `clutch`, `differential`), `input`, `output`,
  `outputB` indices, `ratio`, `maxTorque` (clutch output cap), optional `connection`.
- Brakes: `name`, `shaft` index, `maxTorque`.

`axis` is the canonical authored `[x,y,z]` tuple, unlike public world-vector
snapshots `{x,y,z}`. This detached data matches scene/editor serialization;
there is no alternate vector input in `set`. Internal/body axes are bearing-local
when a bearing is authored, otherwise world-space. Wheel terminals use their
own real steered axle and chassis reaction. There is no universal world-up.
Prescribed kinematic bearings use the existing current-interval motion sampler
during mechanical evaluation; reading it does not advance their pose or command.

## `state`

- `available`: at least one usable DOF; inspect each shaft for complete topology.
- `converged`, `diagnostic`, `iterations`, `residual`: bounded solver result;
  free clutch slip is distinct from an unsatisfied ideal gear constraint.
  `residual` describes the last mechanical projection; `slip` is refreshed from
  the final published physical velocities after the fixed interval.
- `kineticEnergy`, `constraintEnergyChange`: scalar-network diagnostics in joules,
  with their modelling bounds described in M82; not a whole-world energy ledger.
- `shafts`: `{name, available, angularVelocity, rpm, angle, torque}`. Relative
  angular speed is measured in rad/s; RPM is its signed conversion, not scripted
  display state. `angle` is in radians; `torque` is interval net mechanical torque.
- `sources`: `{name, torque}` in N m.
- `links`: `{name, active, connected, saturated, impulse, torque, slip}`. Angular impulse in
  N m s, torque in N m, relative velocity residual/slip in rad/s.
- `brakes`: `{name, torque}` in N m.

Commands and internal rotor momentum participate in normal save and streamed
retention. Contact/query scratch is reconstructed through the existing wheel and
physics lifecycle. Public snapshots never grant ownership of native solver state.

## Limits

This is a small scalar rotational network, not a combustion thermodynamics,
gear-tooth elasticity, battery/fuel, thermal clutch, vehicle controller or full
3D rotor gyroscope model. Physical body rotors still use ordinary M45 constraints
and PhysicsWorld. Finite-step splitting of mechanical rows, tyre contacts and
rigid integration is approximate. See M82 for measured checks and bounds.
Separate graphs execute in ascending owner entity-ID order, with one simultaneous
mechanical solve per graph. Shared bearings couple physically across graphs;
place tightly coupled shaft networks in one authored graph.

## Physical connection prerequisite

M83 adds an optional authored link record:

```json
"connection": {"owner":"102", "attachment":"right", "rootAttachment":"left"}
```

`owner` is the existing Deformable entity identity. `attachment` is its named
physical attachment group. Optional `rootAttachment` requires a second enabled,
live attachment and a continuous current material path between the groups.
For cohesive M79 cooks, every surviving patch node must belong to the same
unbroken component as the root. Removed material or committed bond breaks open
the transmission only when they invalidate that patch or its required root path;
unrelated cuts and surviving alternate paths do not. Released/disabled attachments,
disabled/failed material or unavailable targets also open it. Without a root,
the selected patch is the prerequisite.
For an unpartitioned solid there is no authored internal fracture topology;
material continues through the intact unpartitioned mesh.

There is no automatic endpoint inference: the developer explicitly authors which
material path supports which coupling. This can model wheel bearings, shaft
mounts, structural machine supports or other machinery. It is not a vehicle rule.
Scene validation rejects missing owners/groups; ordinary prefab remapping,
qualified region references, save identity and dependency retention apply.
The qualified streamed reference is `powertrain-link:<link-name>:connection`.

`state.links[i].connected` reports this physical prerequisite independently of the
held JS `enabled` command. A failed prerequisite suppresses that link's impulses;
it does not destroy shafts/wheels, discard momentum, turn off independent
sources/brakes or overwrite driver commands. Enabling the link cannot heal it.
No prerequisite means `connected: true` and unchanged M82 mechanics. The optional
record is omitted from default authored graphs, preserving their fingerprints.
Connection availability is reconstructed, not saved as a second damage truth.
Committed material changes are observed at the next mechanical interval.

See [Deformable strength and observations](deformables.md#m83-materialbody-connection-strength)
and [M83 model/authoring](../M83_MECHANICAL_DAMAGE.md).

## Material group prerequisites (M83)

A connection can opt in to `groups: true`. `attachment` and required
`rootAttachment` then identify cooked material node groups, rather than attachment
constraints. The link requires an intact physical material path between them.
Default false retains the attachment/release semantics described above.
There is no health flag or script-driven drivetrain damage calculation.
See [physical section integration](../M83_COARSE_CRASH.md).
