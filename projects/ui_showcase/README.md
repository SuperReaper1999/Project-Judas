# Field Notes — M96 UI Showcase

Open `ui_showcase.judasproj` in the Judas editor. Press Play; use the left section
buttons. All content is ordinary `.judasui`, scripts and a registered font.

- **Welcome:** menu and navigation.
- **Settings:** readable scale, focus contrast and a project audio preference.
- **Inventory:** 3000 keyed rows, search, disabled rows, selection and dynamic additions.
- **Dialogue:** scrollable conversation, choices and text reply.
- **Text entry:** Unicode/IME fields, selection/clipboard and a read-only example.
- **Local players:** two independent desks, device partition and touch regions.

Mouse/touch click; wheel or drag scrolling; arrows navigate; Enter/gamepad South
activates; Tab/gamepad right shoulder moves between controls. Text fields use
native editing shortcuts. F6/F7 or SAVE/LOAD use ordinary save/load. F1 leaves the
local desks. Player one uses arrows/Enter/mouse; player two uses W/S/Space or a
connected gamepad. On Android each viewport owns its own touch region. There is
one native text keyboard/session at a time, owned by the last focused field.

## Human checklist

1. Visit all six sections; resize the window and try the scale setting.
2. Navigate without the mouse; focus stays visible, including in scrolled content.
3. Search, select and add inventory rows; drag/wheel without accidental clicks.
4. Enter accented/RTL/emoji text; select, copy/paste, undo/redo and submit.
5. Use your installed IME: preedit remains transient until commit.
6. Scroll dialogue, choose routes and submit a reply.
7. Open local desks; confirm separate focus, selection and input ownership; F1 back.
8. Save, change values, load; confirm text/selection/scroll and settings restore.
9. Stop/restart Play and run the relocated standalone; source assets stay unchanged.
10. Android: verify keyboard show/hide, touch scroll and background/resume on hardware.

`showcase.js` owns screen policy, inventory, dialogue, preferences and save policy.
`layout.js` lists authored IDs for explicit responsive reflow at increased scale.
The engine owns actual controls, text editing, clipping, virtual row rendering,
focus and routed input. No game menu names occur in runtime C++.

M96 human validation and physical Android/IME accessibility acceptance are pending.
Native screen-reader adapters, multiline editing and complex variable-height list
cells are not implemented. See [toolkit contracts](../../docs/M96_UI_TOOLKIT.md).
