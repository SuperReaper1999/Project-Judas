# M89 development failures and corrected follow-ups

No human acceptance is inferred. Raw records are in `failures/`; these are
candidate-development failures, not revisions of protected historical evidence.

1. **Build:** an unnecessary GLM GTX include required experimental extensions.
   Replaced it with core GTC quaternion support; did not enable experimental GLM
   globally. First failed build retained. A later new target was requested before
   CMake regeneration; reproduction now explicitly configures before building.
2. **Composition:** the first preview used normal render-target UI UVs, so player
   views appeared upside down. Display composition now explicitly flips them;
   existing imported/camera texture defaults remain intact. Explicit display LDR
   targets also resolve modern HDR before UI sampling. Linux pixel checks cover
   two opposing camera orientations in both legacy/linear modes, resource resize,
   no simulation during render and cleanup. The first pixel fixture itself assumed
   GL bottom-up readback; CaptureFrame already returns top-down rows. The retained
   14-check/2-failure log is a fixture error, corrected without runtime changes.
   The original inverted preview image was overwritten by the corrected preview;
   no original inverted pixels are claimed to remain.
3. **UI construction:** adding per-instance fields invalidated an old aggregate
   initializer. Explicit Instance initialization retains full default viewport.
   Existing UI and Breakwater checks pass. During portrait-profile work, removing
   the authored HUD also invalidated the save probe's early-HUD assumption and
   its unchecked lookup crashed. The normal authored first-stage HUD is retained,
   portrait content swaps through public UI operations and the probe checks the
   lookup safely. Final application checks preserve actual cold-restored physical
   endpoints, not just cached score/status.
4. **Probe setup:** cached metadata made Track reject already-registered fixture
   assets, then the new probe dereferenced an absent ScriptSystem. Its private
   fixture now starts clean, uses separate example paths and checks Build failure.
   The original two failures/backtrace are retained; this was not a game crash.
5. **Device lifecycle:** native controller handles could retain previous-world
   device tokens if a new hub was published without an intervening empty poll.
   Window now recognizes hub ownership changes and republishes attached devices.
   Native virtual-controller checks include held input across world replacement,
   no fabricated button edge and no reconnection requirement. A separate mixed
   touch/gamepad unplug control already passes: legacy code clears only pad/stick
   state, so **no speculative additional repair was made**.
6. **Game policy:** autojoin originally reconsidered every unclaimed gamepad every
   frame, undoing explicit release to keyboard. It now handles connection arrivals
   once. An actual shared-lobby click test releases the pad and proves it stays
   released. Player identity/physical-body association and score survive reassignment.
7. **Multiple sources:** sequential raw writes let one assigned controller's
   release cancel another controller's held button. A touch could likewise cancel
   a held physical mouse. The unchanged focused case fails three checks before
   the repair. Retained per-device levels now sum/clamp deterministically before
   logical mapping; touch/mouse buttons combine. Final unit checks pass.
8. **Touch UI release position:** an idle assigned mouse overwrote a touch's
   release coordinate with its old position before UI dispatch. Native before/
   after captures show the owner menu staying closed/open. Touch activity and its
   release now retain pointer priority for that frame. The initial synthetic UI
   fixture used a whole-document button and was insufficient to detect this; it
   was strengthened to a small center button plus an exact pointer-position check.
   Linking the preserved pre-correction LocalMultiplayer object against that same
   final test yields **37 checks / 2 failures**; final source yields **37 / 0**.
   No physical/test displacement threshold was relaxed.
9. **Android screenshot fixtures:** a whole-hue mask merged a sphere touching its
   identically colored goal. The appearance-specific checker now selects the
   authored shaded highlights; raw failed image/results remain. The first menu
   pixel assertion expected an unfocused color, but the correctly opened Resume
   button is focus-highlighted (74,110,130). Its assertion now distinguishes that
   actual focused button from the unchanged background (20,23,28). The original
   native menu-before image and corrected focused image are both retained.

The final Android run explicitly asserts local menu opening/resume, single-owner
and simultaneous physical-bot movement, two virtual Android gamepads, rotation,
resume and absence of game script/native crash diagnostics. It is emulator
functional evidence, not phone/controller hardware acceptance. No physics solver,
cadence, collision fidelity or existing game was altered to obtain a pass.
