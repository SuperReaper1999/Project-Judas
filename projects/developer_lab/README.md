# The Repair Bench — M94 developer-workflow lab

Open `developer_lab.judasproj` in Judas Editor. It is an ordinary project using
public JudasJS APIs. The editor tools also work on other projects; there is no
lab-specific runtime/debugger branch.

## Real edit → debug → fix → reload

Use a copy of the project if you want to retain the intentional sample defect.

1. View → JudasJS developer tools. Choose `Assets/scripts/worker.js`, line **10**.
   Toggle breakpoint and enable Attach debugger before Play.
2. Play. Inspect `phase`, `this.state` and the entity wrapper. Step over/into/out;
   continue. Disable the breakpoint for ordinary movement.
3. Click the scene viewport (or hide the tools), then press **E**. The worker calls the deliberately invalid calibration helper and
   faults. The rest of the presentation/UI stays alive. Open its runtime error.
4. Choose `Assets/scripts/calibration.js`, Edit source. On line 2 change `gain: 0`
   to `gain: 2`, then Save source and Check project scripts.
5. Uncheck Keep JSON state because the worker is faulted. Reload scripts.
6. Press E again. The amber assembly now moves through the corrected helper.
7. Reload again with Keep JSON state checked: observe phase/sample state continue,
   while constructors/start and subscription/UI rebinding run normally.
8. Introduce a syntax error in the copied helper. Save and attempt reload. Read
   the rejection; the running old code survives. Repair, save and reload again.
9. Stop/Play before testing saves against a fresh source baseline. Stop leaves the
   authored scene unchanged. **R** is a normal full scene reload; **Esc** pauses.

F5 means Continue only while the debugger is stopped; outside it, the editor's
normal F5 Play/Stop behaviour remains. The tools window releases pointer capture.
Hide it when testing a game whose mouse should be captured.

The exported game intentionally has no debugger panel and retains the demonstration
fault until the source is repaired before export. Read
[the tools guide](../../docs/M94_DEVELOPER_TOOLS.md) for exact lifecycle, snapshot,
save, source-checking and iteration limitations.
