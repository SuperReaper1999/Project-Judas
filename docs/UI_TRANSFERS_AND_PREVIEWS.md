# Item transfers and live 3D previews

Experimental additions after Alpha 1.2.1. Published SDKs are unchanged. The editable
example is [Satchel Menu Workshop](../projects/menu_workshop/README.md).

## Author a transfer

Use the UI asset inspector's collapsed **Drag and drop** section, or
`element.configure({dragType:'item',dropType:'item'})`. Empty categories opt out;
equal, case-sensitive categories allow a drop request. Each category is at most
64 UTF-8 bytes, without NUL or line breaks. Document names must be unique,
nonempty UTF-8 identifiers up to 128 bytes. Image, Button, Panel and Text can be
sources; editable controls and virtual-list rows are deliberately excluded.
Existing UI files retain their old format when no categories are used. Categories
are persisted in JudasUI version 4, which older SDKs cannot read.

Pointer movement beyond six scaled pixels starts a drag. Release over a compatible
visible target queues `drop`, then `dragEnd`; other releases queue `dragCancel`.
A short click remains a click. Focused Activate picks up or places an item; Back
cancels. `document.beginDrag(id)`, `cancelDrag()` and readonly `drag` expose the
same mechanism to scripts. Keyboard/gamepad use existing project UI bindings.
Touch uses the existing primary-pointer path; physical touch/controller validation
of this particular menu is pending.

Transfer events contain source/target document and element names, category and
framebuffer-pixel position. `dragOver` reports target changes rather than every
motion sample. Cross-document pointer transfers work between nonmodal documents;
modal and player ownership still constrain input. Source disappearance, owner
removal, input reset, player departure, modal takeover, unloading and successful
state restoration cancel transient transfers. Failed restoration leaves state intact.

The engine does **not** move items or serialize a payload. In `onUI`, check the
source document, owned slot names, current stack, capacity and equipment rules,
then commit your game-data change atomically and redraw. Rejecting a request must
leave the original item intact. The example implements these rules in project JS.
Do not trust a category alone as permission to transfer another script's items.

## Author a 3D preview

Add the **Render camera** component to an ordinary entity, then create render-only display objects on a dedicated
render layer. Exclude that layer from gameplay cameras and include it in the
preview camera. For example, this project uses a 512×640 target and separate
mannequin/equipment meshes. These proxies have no physical bodies. Bind its Image:

```js
this.document.get('preview').setPreviewCamera(this.previewCamera);
// On closing the menu:
this.previewCamera.setCameraEnabled(false);
// Clear a binding and return to the authored image:
this.document.get('preview').setPreviewCamera(null);
```

Only a live render-camera Entity in the same world is accepted; an invalid binding
throws without replacing the current one. The UI borrows the current GPU target
at draw time, preserves its orientation and clips it like an ordinary Image.
There is no CPU readback or second render target allocation. Disabled/unpublished
cameras show the image background rather than a stale frame.

The camera/world owns GPU resources. Hiding an Image alone does not stop its
camera: explicitly disable it when unused. A disabled camera retains one reusable
target until world teardown. Stop/world destruction releases it. Bindings and
in-progress drags are transient, omitted from UI snapshots; reacquire in script
`restore`. Normal item/equipment data remains in project save participants.

This reuses normal scene lighting and weather. It does not add an independent
studio environment, separate scene, animated wardrobe system or paused skeletal
animation clock. The sample rotates render-only geometry through `uiUpdate` while
a modal menu pauses physical gameplay; it does not rotate a physical player.

## Working example

The menu has sixteen bag slots, nearby-container transfers, three equipment slots,
stack merging/splitting, capacity checks, consumables, a character tab, text-size
settings and genuine Save/Load. Rules and item definitions live in
`Assets/menu.js` and `items.js`. Extend those definitions and replace the
preview meshes to build your own game; no inventory policy was added to C++.

Use Escape to resume/reopen, drag slots to transfer/equip, or focus a slot and use
Activate. Left/right rotates the preview. Buttons inspect, use/equip, split, move
to the chest, save and load. Text-size preferences preserve authored relative font
sizes. This is a desktop/landscape example, not a finished portrait/mobile layout.

The original art generator and native project builder are under
`tools/demos/menu_workshop/` and `tools/demos/MenuWorkshop.cpp`. Pillow is an
art-authoring dependency only; the game loads ordinary PNG/OBJ assets. There are
no downloaded third-party assets. Project source remains readable and editable.

## Verification boundaries

Native transfer tests cover pointer/keyboard, clipping, categories, lifecycle,
player isolation, cross-document transfer and multiple scales/resolutions. The
menu integration test runs independent writer, reader and actual application
Save/Load processes; it checks item accounting, restored physical endpoints,
preview pixels/orientation, disabled-camera behavior and resource destruction.
The final evidence/report records actual results and renderer details. Windows,
Android and browser qualification of these additions is pending; existing platform
support is not a claim that these new paths have been tested there.
