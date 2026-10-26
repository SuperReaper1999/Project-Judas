# Ownership, readiness, presentation and saves

[First project](FIRST_PROJECT.md) · [API index](JUDASJS.md) ·
[Editable lessons](judasjs/examples/developer_workflows/README.md)

These are current developer instructions, not a milestone acceptance record.
The compact **Developer Workflows** project is included under
`docs/judasjs/examples/developer_workflows` in both source and SDK distributions.
Open its `developer_workflows.judasproj` in the editor; press Play. No external
art tool, npm package or game-specific C++ is required. Work on a copy if you want
to preserve the supplied sample. Its simple shapes are intentionally easy to inspect.

## 1. Who owns this object?

Position does not determine ownership. A root entity remains resident even after
travelling far away. A regional entity belongs to its region until explicitly
adopted; moving it across a boundary is not an ownership transfer.

During Play open **View → Diagnostics and extensions → Runtime Inspector**.
Choose a live entity, including streamed entities absent from the authored root
hierarchy. Owner, stable save reference, region status, demand count and actual
retention reasons are visible. The selected object's ordinary Inspector also has
**Ownership & readiness → Open Runtime Inspector**. Pause with Escape if the game
captures the mouse. The tool is read-only; it does not request regions or inspect
state by calling gameplay callbacks.

Use **World regions** for request/release controls. Repeated editor Request is
idempotent per row; Release removes only that editor lease, not gameplay demand. Each request retains
its own demand; release your own token. Unload can be deferred by other demand,
manual pins, hard references or physical assemblies crossing owner boundaries.
Read the actual reason rather than treating an unload request as completed work.
Stable references from `saves.reference()` survive reconstruction; a cached Entity
wrapper still needs `valid` checked. See [streaming](judasjs/streaming.md).

In the lesson, orange is a root crate and blue belongs to `yard`. R requests/releases
the yard. E pushes a crate and increments its own persistent counter. Release and
request the yard again: the same regional counter and physical state return.
Do not move all regional state into one player's ever-growing JSON blob.

## 2. Is it actually ready?

There is no universal "everything ready" Boolean. Observe the capability needed:

| Observation | What it guarantees | What it does not guarantee |
| --- | --- | --- |
| `regionStatus(...).state === 'active'` | Native required regional content published | Every script callback/visual resource/project binding completed |
| `visualReady` | Region-held resource references report Ready | A particular gameplay registration or animation choice exists |
| `entity?.valid` | Entity currently exists | A usable physics body, model, navigation path or script contract |
| `animation.info.ready` / native model resource observations | That resource is usable | Other subsystems are ready |
| Runtime Inspector "callbacks started" | Start/restore callback has completed without a recorded fault | Project-specific asynchronous work has finished |
| Project-owned registration | The exact start/restore binding was published | Unrelated resources or another instance's state |

The lesson's `registry.js` holds transient Maps. `crate.js` registers in both start
and restore and removes itself in destroy. `player.js` first resolves the regional
entity, then checks that current-world registration before using it. The map is
not saved; the crate's `state.pushes` is. Constructors do not consume restored state
before the engine installs it.

While paused, workers may reach `prepared`; native installation/unloading waits
for resume. UI/presentation on existing entities can still run.

Poll without blocking and surface `failed`/`blocked` errors. Do not spin until ready,
add arbitrary frame delays or suppress exceptions. UI and presentation callbacks
can run while gameplay is paused and can cause start/restore to run. Pausing is not
a guarantee that callback initialization stops. The Runtime Inspector distinguishes
not-instantiated, waiting-for-start/restore, callbacks-started, disabled and faulted.

## 3. Which pose should I use?

- Physical movement/queries: authoritative `entity.transform`, physics, motor,
  wheel/powertrain APIs. A physical transform assignment is an explicit teleport.
- Camera/visual follow: `presentedTransform` inside `presentationUpdate(dt, alpha)`.
- Ordinary body-free visual children: already follow the interpolated parent. Leave their authored
  local offsets alone. Do not copy the parent's presented world pose into them.
- Physical child bodies are independent bodies; hierarchy is not a constraint.
  Motors, wheels, sockets and physical animation can supply component-owned poses.

The lesson's vehicle visual has a parent and no Body. Its script never copies a
presented pose into that child. The camera reads the vehicle's presented pose.
Lunar Island's former mistake produced `P_presented * inverse(P_current) * P_presented`
when a presented child-world write was converted through the current parent and
then inherited the presented parent again. This is double interpolation, not an
insufficient physics rate. We do not emit a speculative runtime warning for every
presentation transform write: legitimate detached visual animation uses those too.

The small lesson camera intentionally omits collision-boom handling and uses flat
uniform gravity. For a fuller public-API example, source checkouts also contain
Lunar Island's `camera.js` and the wheel/powertrain labs. Those larger games are not
required SDK dependencies. No generic camera framework or extra physics system is
introduced by the lesson.

## 4. Why won't my save load?

A scene save edits authored content. An expedition save preserves a running world.
They are different operations. A project save identity selects the save directory;
a content fingerprint and optional compatibility identity/version decide whether
an existing expedition may be loaded. Save format/native participant checks remain
separate, even when those identities match.

Use the lesson's F6/F9 or your project's save UI, then inspect **Runtime Inspector →
Save compatibility**. This panel shows installed identity/content and saved
content/compatibility/version, plus the exact reported incompatibility. It does not
initialize a service, load, migrate or delete files. Editor Play uses isolated
`EditorSlots`; standalone uses `Slots`.

A mismatch refusal reports which content, compatibility/version or mod loadout
changed, before replacing the running world. Keep the original save and matching
build. Do not delete it, change hashes to lie about compatibility, or blindly turn
on migration. Matching identity metadata is not certification of native state.

For an intentional game update, author an ordered, exact-source-allowlisted policy
and request migration explicitly. See [save evolution](M98_SAVE_EVOLUTION.md) and
[source-only Save Evolution Lab](../projects/save_evolution_lab/README.md), which
contains real old/new content and failure cases. Unknown physical history cannot
be invented by a migration. Migration failure must leave the source intact.

## A short learning path

1. [First visible project](FIRST_PROJECT.md): edit → Play → change a property.
2. **Create → First/third-person player**: generated editable JS + CharacterMotor.
3. [Developer Workflows lesson](judasjs/examples/developer_workflows/README.md):
   walk, drive, request/release, inspect, push, save, quit and cold-load.
4. Add one item to your own project, then deliberately break a resource reference,
   disable its script or request an unknown region. Read the diagnostic and recover.
5. Design save migrations only once your actual game's update policy is known.

Keep gameplay readiness, inventories and control policy in project code. Use the
Runtime Inspector and existing script tools to observe engine boundaries instead
of guessing them. Current limitations and measured validation are in
`docs/evidence/prealpha-developer-workflows/REPORT.md` in source checkouts.
