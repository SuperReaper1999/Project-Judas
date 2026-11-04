# Post-integration source review

This is source review, not executed acceptance. Root owns main/shell/presenter/session edits.

## Confirmed integration defects communicated to root

1. Scene asset drops inherited a `!IsWindowHovered(AnyWindow)` gate. The Scene is now an ImGui window, so the ordinary drop path became unreachable. Root is replacing it with a real Scene content drag/drop target.
2. Detached Scene had no native Window input region and the raw pan event hook admitted only main-window IDs. Consequently look/pan/capture could target the main OS window. Root is extending the edit-view region and its event routing.
3. Existing automation supplied desktop-space ImGui positions to window-local SDL event/warp APIs. Corrected in the qualification modules and TilemapEditor::Autotest using actual native owner and origin, preserving projection coordinates, thresholds and timesteps. Tool activation buttons use their own Terrain/Volume panel viewports; sculpt/picking/navigation use Scene.
4. DebugStopped shell initially reused active menu/toolbar chrome while discarding its commands. Root has explicitly disabled mutation chrome at a debug stop.

## Reviewed boundaries

- One call to EditorPlaySession::Frame per outer Play frame, regardless of view visibility. No second authoritative world is introduced by Scene/Game windows.
- Target resizing happens in Presenter::Image before the ImGui texture token is captured. FrameEditMode uses the same integer content size, avoiding stale texture IDs across ordinary resize.
- ScriptTools stopped presentation saves/restores main-context framebuffer/viewport; host Present restores its native GL context after rendering platform windows. No ordinary world step, save/scene publication or resource pump is called from stopped shell callbacks.
- App declares ScriptTools before the played session, so the callback target outlives world teardown. Normal Stop retires the world before content/tool/GL teardown.
- Project/scene replacement remains guarded by the existing unsaved document and tool-draft paths.

## Bounded GUI qualification recipe

Use a copied 3D project and a copied 2D project. Keep original data untouched.

1. Move the main editor to a nonzero desktop position. Verify centre picking and a native middle-drag pan against unchanged physical projection thresholds; authored scene bytes and undo state must remain unchanged.
2. Drag a tracked mesh and a tracked PNG from Assets onto Scene. Each must create exactly one correctly typed selected entity at the projected location; dropping over Inspector/Console creates none.
3. Detach Scene to a second native window. Repeat picking, pan, wheel zoom and focus-loss/capture-release, then redock. Closing an unrelated tool must neither close the editor nor change the scene viewport's native dimensions.
4. Detach Game, Play once and capture native body/fixed-step counters. Hiding, tabbing or resizing the Game panel must not duplicate, reset or suspend world advancement. Clicking script text fields must not move the character or fire gameplay controls. Stop must restore authored bytes exactly.
5. Trigger an actual project breakpoint with Game active. Manipulate only layout/visibility in the safe stopped shell; compare native body transforms/velocities and declared script state before/after the pause. Continue/step through ScriptTools and preserve exact source snapshots. Project mutation and world/save/streaming publication remain unavailable while stopped.
6. Start terrain, volume and tilemap painting from their actual panel buttons, then paint inside Scene after a main-window move and after detachment. The pointer event must use the panel owner's native window while all rays use unchanged desktop content bounds.
7. Save layout, restart, then simulate an absent monitor and malformed layout. Tools remain recoverable and editor preferences never enter the game export.

## Presentation limitation to state honestly

The current Scene target remains an authored preview while Play's Game target advances. If that boundary remains intentional, label it clearly rather than implying a second live simulation view. This does not justify creating another world/session.
