# Signals Lab — M73 candidate

Open `signals_lab.judasproj` in the ordinary editor, then Play. The fixed camera
shows two left indicator slots, one right indicator and optional spawned/annex
receivers. Counter facts live in each script's state; native signals only route.

- **B:** fixed broadcast. **T:** target left entity (both independent slots).
- **U:** toggle left pulse subscriptions. **D:** destroy right receiver.
- **P:** spawn an independent receiver prefab. **L:** load/unload streamed annex.
- **Escape:** paused UI. **Preview:** UI signal increments preview while paused and
  queues a fixed pulse; **Queue fixed:** waits for resume. Arrows/Enter or mouse.
- **F6/F7:** modern save/load `signals`. **R:** reconstruct scene.

`counter.js` changes its own real M72 material after a pulse, and publishes a UI
receipt. `lab.js` owns publishing/menu/save policy. Start/restore share registration;
restore does not reset counters. No native lab branches, behaviour-instance sharing,
video dependencies or durable notification replay. Region startup has no global
start barrier: broadcast again once its receiver is active. A receiver's earlier HUD
receipt is a game fact; removing a receiver does not erase that old display row.
New prefab labels share a HUD row but their counter instances/materials are independent.

Human checklist: broadcast then target; toggle/remove a receiver; preview while
paused then resume fixed responses; save, restart and load; annex unload/revisit;
editor Stop/Play; repeat in the moved package. Hardware/visual acceptance is operator-owned.
