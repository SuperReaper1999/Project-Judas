# M35 — Project input actions, axes and rebinding

**Status through M69:** M35 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Candidate; human keyboard/mouse/controller validation is pending.

## Boundary and semantics

Window owns SDL events and one active SDL game controller. InputSystem owns
portable raw control values, project mappings and logical state. Gameplay queries
named actions/axes, not SDL codes. Existing Window Action/Consume wrappers remain
compatibility adapters for current consumers and historical harnesses; their real
path resolves named inputs. Historical scripted injection remains labelled tooling.

`Action(name)` is non-consuming: held, pressed, released. Multiple bindings OR
into one action. Edges accumulate through event processing, so even down/up within
one poll is visible. BeginFrame clears frame edges and mouse delta/wheel; held
inputs persist. `BeginFixedStep` snapshots pending edges once for all consumers.
Zero-step frames retain pending edges; subsequent catch-up steps do not repeat
those edges. FixedAction queries do not consume. Existing player jump latch remains
in use. Pause/editor ownership discards pending gameplay requests; gameplay state,
not InputSystem, decides whether an action is allowed.

Axes sum scaled sources. Persistent axes are bounded [-1,1]; mouse-delta/wheel axes
retain relative quantities and reset each poll. Opposed keys cancel. Stick axes use
independent axial deadzones: zero inside d, signed (abs(value)-d)/(1-d) outside.
This is an axial linear model, not a radial/response-curve system. SDL signed sticks
normalize negative values by 32768 and positive by 32767; triggers normalize [0,1].
Source scales and deadzones are binding data. Player/ship translation preserves
analog magnitude, restricting diagonals to magnitude 1. Stick look is a rate
(240 equivalent mouse pixels/second before existing mouse sensitivity); mouse look
is an unscaled relative delta. Secondary cameras do not tick input separately.

## Project data and rebinding

`ProjectSettings::input` is an InputMap. Project format 1 gains an optional quoted
`input-map` field with its own version 1 payload: entry count, quoted name,
action/axis flag, binding count, then quoted control/scale/deadzone triplets.
Old projects without the field get explicit defaults; Save writes the full map.
New projects and both shipped projects store real bindings. Unsupported input-map
versions, duplicate names and malformed numeric fields fail before changing Project.
No scene/save fingerprint schema changes: these bindings are project configuration,
not authored scene physics or runtime deltas. Scene changes keep the project map.

InputMap supports Find/Add/Rename/Remove and AddBinding/ReplaceBinding/RemoveBinding.
Apply a modified map with InputSystem::SetMap; state is reset rather than carrying
stale events across a rebind. Names are case-sensitive. Renaming an entry changes
its public API name; game code must query that new name. Bindings use readable tokens:

- `key:W`, `key:Space`, `key:Escape`, `key:Keypad Enter`: SDL display key names converted only by Window.
- `mouse:Left/Middle/Right/X1/X2`, `mouse:dx/dy/wheelX/wheelY`.
- `pad:South/East/West/North`, `LeftShoulder/RightShoulder`, `LeftStick/RightStick`,
  `DpadUp/DpadDown/DpadLeft/DpadRight`, `Back/Guide/Start` (all prefixed `pad:`).
- `stick:LeftX/LeftY/RightX/RightY/LeftTrigger/RightTrigger`.

Unknown physical tokens produce no input; use the documented spelling. No key
capture wizard or device-profile system is supplied. Keyboard names are physical
SDL scancode names, not text typing or locale-dependent characters.

## Editor and existing controls

Project settings → Input actions and axes: create, rename/delete, edit/add/remove
bindings, edit scale and stick deadzone. Save project persists edits; Play and
standalone startup load the same map. Runtime SetMap handles immediate rebindings.
Editor project edits take effect on the next Play entry. Project authoring is not
scene undo history. Editor-only Ctrl+S/Z/Y, gizmos and viewport shortcuts remain
editor shortcuts, not game bindings. Edit mode does not run gameplay.

Default keys remain W/S/A/D and arrows for movement, Q/E vertical, I/K/J/L/U/O
ship rotation, Space jump, G interact/pickup/drop, H throw, F pilot, V view, T torch,
X SAS, R reset, B water, C heater, P/M/N orbit thrust, Z/Y lifecycle, F6/F7 persistence,
Escape pause, arrows/Enter menu navigation. Left stick moves, right stick looks,
South jumps/activates, West interacts, Start pauses, D-pad navigates. Mouse capture
continues to belong to gameplay/editor state; uncaptured mouse motion does not look.

The first available SDL-compatible controller is active; it is retained until
removed, then the first remaining one is selected. Disconnect clears pad/stick
state. Focus loss clears inputs; editor keyboard capture suppresses gamepad input
as well. No multiplayer assignment, haptics, Steam Input, touch, replay or macros.

## Human input checklist

1. Existing keyboard movement, jump, interaction and ship controls still work.
2. Mouse look and Escape/cursor handling still work.
3. Replace jump's Space binding, Save project, restart Play: new key works and Space does not.
4. If hardware is available: controller movement/look/action work and stick center is quiet.

Automated tests exercise actual SDL event/controller adapters, including a tiny
SDL virtual controller. This is not human hardware acceptance. See evidence under
`docs/evidence/m35/` for executed counts, measurements and exact source fingerprints.
