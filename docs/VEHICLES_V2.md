# Vehicles / Wheels V2

Experimental implementation; automated and human qualification are reported separately in `VEHICLES_V2_QUALIFICATION.md`. MK2 remains the only rigid-body solver. Vehicles are ordinary editable entities, wheels and mechanical graphs; there is no required vehicle-controller component or four-wheel layout.

## Create and drive

Open a project, then use **Create → Vehicles** (also available in hierarchy creation) to add a car, truck, bus or unpowered trailer. The root is a dynamic compound chassis. Wheel children own suspension and scalar rotational state; their tread children are presentation meshes. Powered presets attach an editable `Assets/Scripts/VehicleDriver*.js` and add logical throttle, steering and brake bindings if absent. Existing bindings are preserved. Creation rejects scaled parent chains; dimensions belong in the assembly/component settings rather than inherited non-unit scale.

W/S drives forward/reverse, A/D steers, Space brakes. These defaults are project JavaScript, not engine behaviour. The exact unmodified project-starter camera is disabled when adding a powered preset; custom cameras remain yours to manage. Multiple attached drivers can compete for camera/input: disable their `driving`/`camera` properties or supply a project-owned vehicle-selection controller.

The editable example is `projects/vehicle_v2_workshop/Vehicle_V2_Workshop.judasproj`. Its selection controller demonstrates a combustion/clutch/gear/differential car, independent electric multi-axle drives and an ordinary MK2 ball-joint trailer. The generator refuses to overwrite an existing project.

## Authoritative responsibilities

| Owner | Responsibility |
| --- | --- |
| MK2 | Chassis/ground poses and momentum, collision, CCD, ordinary joints and sleep islands |
| `vehicles/WheelContact.cpp` | Finite-width support discovery and static sleeping-support cache |
| `vehicles/WheelSolver.cpp` | Accumulated suspension, bump-stop, brake and combined tyre constraints |
| `vehicles/VehicleRuntime.cpp` | Batch preparation, symmetric iterative sweeps and MK2 substep callbacks |
| `Wheel.cpp`, `WorldWheels.cpp` | Validation, durable wheel state, ownership and derived visual publication |
| Existing powertrain modules | Combustion/electric/custom sources, shafts, clutches, gears, differentials, brakes and bearing reactions |
| `authoring/VehicleAssembly.cpp` | Transactional ordinary-scene assembly generation |
| Editor vehicle creation / component Inspector | Presets, editable components, live telemetry and debug overlays |
| Game scripts | Inputs, gear-selection policy, cameras, entry/exit, fuel, damage rules and gameplay |

The current rigid-part attachment interface is retained for existing material/fracture users. Future deformation can update the physical mount/body mapping without becoming a second vehicle pose authority. Structural crash deformation is not implemented here.

## Contact and suspension

Mount position/orientation are relative to the chassis **authored pivot**, or selected rigid part. They are not centre-of-mass coordinates. Local suspension points down −Y; the axle line is X; positive scalar spin about −X rolls toward −Z. No world-up assumption is made.

The tread proxy uses three lateral tracks and five rounded circumferential probes per track. Supporting-normal filtering happens inside mesh triangle selection, so a nearer vertical wall cannot hide a valid floor in the same mesh. Discovered patch planes receive analytic circular-tread refinement; equal-height witnesses on one body form a resultant patch. Width matters for raised/narrow supports.

This is a bounded finite-width support approximation, **not full cylinder CCD or a separately colliding wheel rigid body**. Chassis CCD remains MK2's responsibility. Fast lateral wheel-only impacts, very narrow obstacles between samples and rapidly moving supports need application-specific assessment. Wheel meshes do not define tread collision.

Spring compression/rebound uses a compliant backward-Euler unilateral row. `reboundRatio` multiplies damping only while extending. Minimum travel adds a unilateral bump-stop impulse when a meaningful non-overlap gap exists. Initial-overlap casts do not invent penetration depth or teleport the chassis. Unsprung translational mass is included in chassis mass; scalar wheel inertia is configured separately.

Geometry/source preparation occurs on four native vehicle intervals per fixed step. Chassis, wheel and shaft constraints then iterate together through MK2's velocity-coupling interface. Source torque and brake capacity are integrated once per vehicle interval, not once per solver sweep. Wheels use canonical physical-layout ordering and symmetric sweeps. Sorting alone is not the order-independence test: the regression also reverses deliberately unsorted constraint sweeps.

## Tyres, drive and sleep

The two tangent rows share a Coulomb disk, with coefficient `sqrt(wheelFriction * groundFriction)`. Running response uses load-scaled `longitudinalStiffness` and `lateralStiffness`; low-speed contact uses static grip. Slip can exceed available grip. No hidden driving aid, artificial energy clamp or vehicle pose integrator is applied.

Drive/brake and tyre impulses include the scalar rotor, chassis bearing reaction and dynamic-ground reaction. Kinematic surfaces provide prescribed point velocity; static/kinematic chassis cannot be physically driven by wheel forces. Ground friction is the collider's physical material, not an automatically generated per-pixel terrain-paint material.

Wheel support participates in MK2's existing island anchoring and wake graph. Quiet supported chassis can sleep naturally. Drive effort, lost/changed support and moving ground wake them. A sleeping static-support cache checks nearby membership, body identity, pose and immutable geometry each interval; dynamic/kinematic or compound candidates use fresh tread queries. It does not pin terrain pages or keep stale contact handles in saves.

## Editor and scripting

The Wheel Inspector exposes tread dimensions, suspension travel, stiffness/damping/inertia, collision filters, friction and an expandable tyre/rebound group. Live telemetry shows support, sleep, travel, load, rpm, slip, grip and actual tyre impulses. Existing collision/contact debug overlays draw tread rings, suspension and support force arrows. Arrow length is display-scaled only.

`entity.wheel.settings` reads configuration; `entity.wheel.configure({...})` changes validated settings. `entity.wheel.set({driveTorque, brakeTorque, steeringAngle})` holds commands. Torques are N·m, angles radians and angular velocity rad/s. `entity.wheel.state` includes `contact`, `point`, `normal`, `contactEntity`, `normalForce`, `length`, `angularVelocity`, `angle`, longitudinal/lateral slip and impulse, plus `sleeping`, `slipRatio`, `slipAngle`, `gripUtilization` and `diagnostic`.

Powertrain `.settings`, `.state` and `.set({sources,links,brakes})` remain authoritative. See [powertrains](POWERTRAINS.md), [JudasJS wheels](judasjs/wheels.md) and [the editable driver](judasjs/examples/vehicle-driver.js). Pure scene authoring uses `JudasVehicles::CreateAssembly`; layouts support 1–16 axles and explicit driven-axle indices. More complex steering/gearing policies belong in project scripts.

## Persistence, platforms and limits

Modern SaveService participants preserve authored settings, commands, chassis state, wheel spin/phase and actual powertrain momentum. Geometry contacts, solver lambdas and support caches are derived and rebuilt. A true writer/reader process test compares the resumed driven trajectory, wheel phase and rotor momentum against uninterrupted execution. Ordinary entity/reference ownership supplies streaming dependencies; origin shifts preserve local physics/visual relationships.

The implementation has no new graphics-backend dependency. Qualification on Linux does not certify Windows, Android, browser hardware or cross-platform bit-identical determinism. Consult the qualification record for executed platforms.

Deliberately deferred: motorcycle balancing, full wheel gyroscopic transport, traction/stability aids, native vehicle replication, deformable tyres and detailed crash deformation. Reusable joints support trailers and articulation; heavy-machine tracks/hydraulics and aviation/boat dynamics are separate capabilities, not automatically provided by wheel presets.
